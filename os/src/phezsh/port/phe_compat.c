/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * phe_compat.c -- what zsh calls that the P.I.P.S. libc does not export. Each entry answers like a POSIX system
 * without the feature would (ENOSYS / no job control), never pretends to succeed at something it did not do.
 */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

/* fork() is not part of the P.I.P.S. libc (no address-space clone in EKA2): every caller in zsh is routed to the
 * spawn plan first (phe_spawn.c); a call that still reaches here fails like a fork that ran out of resources. */
pid_t phe_fork(void)
{
	errno = ENOSYS;
	return -1;
}

#include "termios.h"
#include <string.h>
#include <sys/stat.h>

/* No terminal device yet (interactive rung). tcgetattr answers a phetty console (a character device) with the
 * settings phetty actually has -- a cooked line with echo -- and anything else with "not a terminal". zsh reads the
 * tty settings after every foreground job (gettyinfo); an error there is a zerr, which aborts the rest of the
 * command line ("zsh -fc 'exit 3'; print rc=$?" lost its print). tcsetattr stays a no-op error zsh ignores. */
int tcgetattr(int fd, struct termios *t)
{
	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISCHR(st.st_mode)) {
		errno = ENOTTY;
		return -1;
	}
	memset(t, 0, sizeof *t);
	t->c_iflag = ICRNL;
	t->c_oflag = OPOST | ONLCR;
	t->c_cflag = CS8;
	t->c_lflag = ISIG | ICANON | ECHO | ECHOE;
	t->c_cc[VERASE] = 8;
	t->c_cc[VINTR] = 3;
	t->c_ispeed = t->c_ospeed = B38400;
	return 0;
}

int tcsetattr(int fd, int act, const struct termios *t)
{
	(void)fd; (void)act; (void)t;
	errno = ENOTTY;
	return -1;
}

speed_t cfgetospeed(const struct termios *t) { return t->c_ospeed; }
speed_t cfgetispeed(const struct termios *t) { return t->c_ispeed; }

int tcflush(int fd, int q)
{
	(void)fd; (void)q;
	errno = ENOTTY;
	return -1;
}

/* no termcap/terminfo database (the interactive rung brings one): zsh sees every terminal type as unknown */
int tgetent(char *buf, const char *name) { (void)buf; (void)name; return 0; }
int tgetflag(char *id) { (void)id; return 0; }
int tgetnum(char *id) { (void)id; return -1; }
char *tgetstr(char *id, char **area) { (void)id; (void)area; return 0; }
char *tgoto(const char *cap, int col, int row) { (void)cap; (void)col; (void)row; return 0; }
int tputs(const char *str, int affcnt, int (*putc_)(int))
{
	(void)affcnt;
	while (str && *str)
		putc_(*str++);
	return 0;
}

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

char *cuserid(char *s) { (void)s; return 0; }

int isblank(int c) { return c == ' ' || c == '\t'; }

/* no per-process resource limits in EKA2: report every limit as unlimited, refuse to set one */
int getrlimit(int res, struct rlimit *rl)
{
	(void)res;
	rl->rlim_cur = RLIM_INFINITY;
	rl->rlim_max = RLIM_INFINITY;
	return 0;
}

int setrlimit(int res, const struct rlimit *rl)
{
	(void)res;
	if (rl->rlim_cur == RLIM_INFINITY && rl->rlim_max == RLIM_INFINITY)
		return 0;
	errno = EPERM;
	return -1;
}

/* the template's XXXXXX become a name no file has yet (the caller creates it; zsh opens with O_EXCL) */
char *mktemp(char *tmpl)
{
	static const char set[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	size_t n = strlen(tmpl), x = 0;
	int tries;
	while (x < n && tmpl[n - 1 - x] == 'X')
		x++;
	for (tries = 0; tries < 100; tries++) {
		struct stat sb;
		size_t i;
		for (i = n - x; i < n; i++)
			tmpl[i] = set[(unsigned)rand() % (sizeof(set) - 1)];
		if (stat(tmpl, &sb) < 0 && errno == ENOENT)
			return tmpl;
	}
	*tmpl = 0;
	return tmpl;
}

/* The libc has no sigsuspend. What zsh waits for in it is a child changing state (SIGCHLD): sleep until a child has
 * exited (waitpid WNOWAIT: the child stays for zsh's own waitpid), then run zsh's SIGCHLD handler as the signal would,
 * and return like an interrupted wait. Without children, or with SIGCHLD blocked by the mask, return at once; zsh's
 * wait loop then looks at its job table again. */
#ifndef WNOWAIT
#define WNOWAIT 8
#endif
void zhandler(int sig);

int sigsuspend(const sigset_t *mask)
{
	if (!(mask && sigismember(mask, SIGCHLD) == 1) && waitpid(-1, 0, WNOWAIT) > 0)
		zhandler(SIGCHLD);
	errno = EINTR;
	return -1;
}

/* process times: the libc has no per-process CPU accounting; the elapsed clock only */
clock_t times(struct tms *t)
{
	struct timeval tv;
	if (t)
		memset(t, 0, sizeof(*t));
	gettimeofday(&tv, 0);
	return (clock_t)(tv.tv_sec * 100 + tv.tv_usec / 10000);
}

char *ttyname(int fd)
{
	(void)fd;
	errno = ENOTTY;
	return 0;
}
