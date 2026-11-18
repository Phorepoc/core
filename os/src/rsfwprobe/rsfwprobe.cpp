// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// rsfwprobe.cpp -- OURS: a diagnosis probe for the Remote Storage Framework start (not Symbian source; remove with the RSFW trace).
//
// Starts the two RSFW processes the way RSFW itself does (RProcess::Create by name, as RRsfwSession::CreateServerProcess and the SSM
// list do), logs every step to the debug port with the loader's answer, waits for each to end (bounded) and reports its exit
// type, category and reason, then reports what the file server says about drive J:. Started by SSM (non-critical list).
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

static const TInt KStepTimeout = 8 * 1000000;

static void Log(const char* aWhat, TInt aValue)
	{
	RDebug::Printf("[rsfwprobe] %s: %d", aWhat, aValue);
	}

// create aName, resume it, wait up to KStepTimeout for it to end, and say how it ended
static void RunProcess(const TDesC& aName, const char* aTag)
	{
	RProcess p;
	TInt r = p.Create(aName, KNullDesC);
	RDebug::Printf("[rsfwprobe] %s: RProcess::Create = %d", aTag, r);
	if (r != KErrNone)
		{
		return;
		}
	TRequestStatus st;
	p.Logon(st);
	p.Resume();
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, KStepTimeout);
	User::WaitForRequest(st, ts);
	if (st == KRequestPending)
		{
		RDebug::Printf("[rsfwprobe] %s: still running after %d s (exit type %d)", aTag, KStepTimeout / 1000000, (TInt)p.ExitType());
		p.LogonCancel(st);
		User::WaitForRequest(st);
		t.Cancel();
		User::WaitForRequest(ts);
		}
	else
		{
		t.Cancel();
		User::WaitForRequest(ts);
		TBuf8<64> cat;
		cat.Copy(p.ExitCategory());
		RDebug::Printf("[rsfwprobe] %s: ended, exit type %d, reason %d, category %S", aTag, (TInt)p.ExitType(), p.ExitReason(), &cat);
		}
	t.Close();
	p.Close();
	}

// is a server of that name registered?
static void FindServer(const TDesC& aName, const char* aTag)
	{
	TFindServer f(aName);
	TFullName n;
	TInt r = f.Next(n);
	RDebug::Printf("[rsfwprobe] server %s: %d", aTag, r);
	}

TInt E32Main()
	{
	RDebug::Printf("[rsfwprobe] start");
	FindServer(_L("remotefe"), "remotefe (before)");
	RunProcess(_L("remotefe"), "remotefe");
	FindServer(_L("remotefe"), "remotefe (after)");
	FindServer(_L("RemoteFE"), "RemoteFE (after)");
	RunProcess(_L("rsfwbootmounter"), "rsfwbootmounter");

	RFs fs;
	TInt r = fs.Connect();
	Log("RFs::Connect", r);
	if (r == KErrNone)
		{
		TDriveList list;
		r = fs.DriveList(list, KDriveAttAll);
		Log("DriveList", r);
		const TInt jDrive = EDriveJ;
		RDebug::Printf("[rsfwprobe] drive J attributes 0x%x", (TInt)list[jDrive]);
		TFileName fsName;
		r = fs.FileSystemName(fsName, jDrive);
		TBuf8<128> fsName8;
		fsName8.Copy(fsName);
		RDebug::Printf("[rsfwprobe] FileSystemName(J) = %d, name %S", r, &fsName8);
		fs.Close();
		}
	RDebug::Printf("[rsfwprobe] done");
	return KErrNone;
	}
