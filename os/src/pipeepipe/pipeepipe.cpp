// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pipeepipe -- pipe(), close the read end, write: expect -1 / EPIPE (was errno 142, "unknown error"), SIGPIPE delivered.
#include <e32debug.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

static volatile int gSig = 0;
static void OnPipe(int) { gSig++; }

int main()
	{
	signal(SIGPIPE, OnPipe);
	int fd[2];
	if (pipe(fd) != 0)
		{
		RDebug::Printf("[pipeepipe] pipe() failed errno=%d", errno);
		return 1;
		}
	int n = write(fd[1], "ok", 2);
	RDebug::Printf("[pipeepipe] write with a reader: %d", n);
	close(fd[0]);
	errno = 0;
	n = write(fd[1], "x", 1);
	int e = errno;
	RDebug::Printf("[pipeepipe] write without a reader: ret=%d errno=%d (EPIPE=%d) sigpipe=%d -> %s", n, e, (int)EPIPE, (int)gSig,
		(n == -1 && e == EPIPE) ? "PASS" : "FAIL");
	close(fd[1]);
	return 0;
	}
