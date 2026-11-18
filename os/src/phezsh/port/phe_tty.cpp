// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// phe_tty.cpp -- a process zsh starts gets zsh's tty. phestdio routes a program's stdio to a tty only when that tty
// adopted it (phettyd EPtAdopt); pheshell adopts what it starts, and so does zsh here: once posix_spawn returns, the
// child is adopted by the tty zsh itself belongs to (if any). phestdio asks again on each request of a process
// without a tty, so output written in the moment before the adoption is the only part that misses the tty.
#include <e32std.h>
#include <e32debug.h>
#include "../../phetty/phetty.h"

extern "C" void phe_tty_adopt(int aPid)
	{
	RPheTty tty;
	if (tty.ConnectNoStart() != KErrNone)
		return;
	TInt id = tty.TtyOf((TUint)RProcess().Id());
	TInt r = id >= 0 ? tty.Adopt(id, (TUint)aPid) : id;
	RDebug::Printf("PH: [zsh] adopt pid %d on tty %d: %d", aPid, id, r);
	tty.Close();
	}

// zsh's waitforpid (jobs.c): does this child still run, or has it exited without being reaped yet (one more
// signal_suspend then reaps it)? kill(pid, 0) cannot tell: it succeeds while the process object exists.
#include <sys/types.h>
#include <sys/wait.h>
#ifndef WNOWAIT
#define WNOWAIT 8
#endif
extern "C" int phe_child_pending(pid_t aPid)
	{
	RProcess p;
	if (p.Open(TProcessId((TUint)aPid)) != KErrNone)
		return 0;
	TBool running = p.ExitType() == EExitPending;
	p.Close();
	if (running)
		return 1;
	int st;
	return waitpid(aPid, &st, WNOHANG | WNOWAIT) == aPid;
	}
