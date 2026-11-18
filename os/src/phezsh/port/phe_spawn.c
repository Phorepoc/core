/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * phe_spawn.c -- process creation for zsh without fork().
 *
 * execve: EKA2 cannot replace a process image in place, so "exec" = start the program as a new process with the same
 * fds (0-2 through the libc's spawn), wait for it and leave with its status. The pid changes; a parent waiting for this
 * process still sees the program's exit status.
 */
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void phe_tty_adopt(int pid);	/* phe_tty.cpp */

int execve(const char *path, char *const argv[], char *const envp[])
{
	pid_t pid;
	int st, r;
	r = posix_spawn(&pid, path, 0, 0, argv, envp);
	if (r && strchr(path, '/')) {
		/* zsh walks $path and tries "<dir>/<name>"; the loader knows no such folders, it finds every program by
		 * name in sys/bin. So a failed "<dir>/<name>" is tried once more as "<name>". */
		r = posix_spawn(&pid, strrchr(path, '/') + 1, 0, 0, argv, envp);
	}
	if (r) {
		errno = r;
		return -1;
	}
	phe_tty_adopt(pid);
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			_exit(127);
	if (WIFEXITED(st))
		_exit(WEXITSTATUS(st));
	_exit(128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0));
}
