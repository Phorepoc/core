/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * phe_spawn.c -- process creation for zsh without fork().
 *
 * phe_spawn: start a program as a new process the way a forked-and-exec'd child would see it:
 *   - its arguments arrive as they were given: the libc joins argv[1..] with spaces into one command line and the
 *     child's startup splits it again by the Win32 rules (space separates, "..." groups, backslashes before a quote
 *     escape), so each argument is quoted for that split here ("zsh -fc 'exit 3'" arrived as -fc, exit, 3);
 *   - its fds 0-2 are what the shell has there: the libc hands over fds 3 and up only (a child's 0-2 are its own
 *     stdio), so a pipe or a file on 0-2 goes over as a copy at 10+ that a dup2 action puts back on 0-2. A console
 *     fd stays the child's own stdio (a dup of it, closed again, would close the shell's stdio);
 *   - in/out (> 0) are given fds that become the child's 0/1 instead;
 *   - zsh walks $path and asks for "<dir>/<name>"; the loader knows no such folders (every program is found by name
 *     in sys/bin), so a failed "<dir>/<name>" is tried once more as "<name>";
 *   - the child joins this process's tty (phestdio routes its stdio there).
 *
 * execve: EKA2 cannot replace a process image in place, so "exec" = start the program as a new process with the same
 * fds, wait for it and leave with its status. The pid changes; a parent waiting for this process still sees the
 * program's exit status.
 */
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

void phe_tty_adopt(int pid);				/* phe_tty.cpp */
void phe_debug(const char *fmt, int a, int b);		/* phe_debug.cpp */

/* one argument quoted for the Win32 split of the child's startup (libc_init/ucrt0.cpp); malloc'd */
char *phe_quote_arg(const char *a)
{
	size_t n = strlen(a), i, o = 0;
	char *q = malloc(2 * n + 3);
	int plain = n > 0;

	if (!q)
		return 0;
	for (i = 0; i < n && plain; i++)
		if (a[i] == ' ' || a[i] == '\t' || a[i] == '"')
			plain = 0;
	if (plain) {
		memcpy(q, a, n + 1);
		return q;
	}
	q[o++] = '"';
	for (i = 0; i < n; i++) {
		size_t bs = 0;
		while (i < n && a[i] == '\\') {
			bs++;
			i++;
		}
		if (i == n) {			/* backslashes before the closing quote: double them */
			while (bs--) {
				q[o++] = '\\';
				q[o++] = '\\';
			}
			break;
		}
		if (a[i] == '"') {		/* 2N+1 backslashes + quote = N backslashes + a literal quote */
			while (bs--) {
				q[o++] = '\\';
				q[o++] = '\\';
			}
			q[o++] = '\\';
			q[o++] = '"';
		} else {			/* backslashes not before a quote are literal */
			while (bs--)
				q[o++] = '\\';
			q[o++] = a[i];
		}
	}
	q[o++] = '"';
	q[o] = 0;
	return q;
}

static int spawn_quoted(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa, char *const argv[],
			char *const envp[])
{
	int n = 0, i, r;
	char **qv;

	while (argv[n])
		n++;
	qv = malloc((n + 1) * sizeof *qv);
	if (!qv)
		return ENOMEM;
	qv[0] = argv[0];			/* argv[0] is not on the command line (the child gets its file name) */
	for (i = 1; i < n; i++)
		if (!(qv[i] = phe_quote_arg(argv[i]))) {
			while (--i > 0)
				free(qv[i]);
			free(qv);
			return ENOMEM;
		}
	qv[n] = 0;
	r = posix_spawn(pid, path, fa, 0, qv, envp);
	for (i = 1; i < n; i++)
		free(qv[i]);
	free(qv);
	return r;
}

int phe_spawn(pid_t *pid, const char *path, char *const argv[], char *const envp[], posix_spawn_file_actions_t *fa,
	      int in, int out)
{
	posix_spawn_file_actions_t own;
	int tmp[3] = { -1, -1, -1 }, i, r;

	if (!fa) {
		posix_spawn_file_actions_init(&own);
		fa = &own;
	}
	for (i = 0; i < 3; i++) {
		int src = (i == 0 && in > 0) ? in : (i == 1 && out > 0) ? out : -1;
		if (src < 0) {
			struct stat st;
			int fr = fstat(i, &st);
			if (fr == 0 && (S_ISFIFO(st.st_mode) || S_ISREG(st.st_mode)))
				src = tmp[i] = fcntl(i, F_DUPFD, 10);
		}
		phe_debug("PH: [zsh] spawn: fd %d <- %d", i, src);
		if (src >= 0)
			posix_spawn_file_actions_adddup2(fa, src, i);
	}
	r = spawn_quoted(pid, path, fa, argv, envp);
	if (r && strchr(path, '/'))
		r = spawn_quoted(pid, strrchr(path, '/') + 1, fa, argv, envp);
	for (i = 0; i < 3; i++)
		if (tmp[i] >= 0)
			close(tmp[i]);
	if (fa == &own)
		posix_spawn_file_actions_destroy(&own);
	if (r)
		return r;
	phe_debug("PH: [zsh] spawned pid %d", (int)*pid, 0);
	phe_tty_adopt(*pid);
	return 0;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
	pid_t pid;
	int st, r;

	r = phe_spawn(&pid, path, argv, envp, 0, 0, 0);
	if (r) {
		errno = r;
		return -1;
	}
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			_exit(127);
	if (WIFEXITED(st))
		_exit(WEXITSTATUS(st));
	_exit(128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0));
}
