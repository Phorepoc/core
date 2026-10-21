// OURS, DEBUG ONLY (telephony T1, 2026-10-16): CWatcherLog -> the serial log ("[wlog] ...") instead of flogger (not in the ROM), always on. Wire it in with
// drop=["watcherlog.cpp"], extra=["toolchain/shims/watcher_dbg/watcherlog.cpp"] on the "watcher" component (emitter/components.py), rebuild watcher + the ROM.
// Copyright (c) 2000-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description:
//

#ifdef _DEBUG
#undef _MSG_NO_LOGGING
#endif


#include <e32cons.h>
#include <flogger.h>
#include <e32svr.h>
#include <watcher.h>

#if 1
_LIT(KWatcherLogDir, "watcher");
_LIT(KWatcherLogDirFull, "c:\\logs\\watcher");
_LIT(KWatcherLogDirConsole, "c:\\logs\\watcher\\cons");
_LIT(KWatcherLogFile, "watcher.txt");
_LIT(KWatcherConsoleTitle, "Messaging Watchers");
#endif

CWatcherLog* CWatcherLog::NewL(RFs& aFs)
	{
	CWatcherLog* self = new (ELeave) CWatcherLog(aFs);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

CWatcherLog::CWatcherLog(RFs& aFs)
: CBase(), iFs(aFs)
	{
	}

CWatcherLog::~CWatcherLog()
	{
	delete iConsole;
	}

void CWatcherLog::ConstructL()
	{
#if 1
	if (IsConsole())
		iConsole = Console::NewL(KWatcherConsoleTitle, TSize(KConsFullScreen, KConsFullScreen));
#endif
	}

#if 1
EXPORT_C void CWatcherLog::Printf(TRefByValue<const TDesC> aFmt,...)
#else
EXPORT_C void CWatcherLog::Printf(TRefByValue<const TDesC> /*aFmt*/,...)
#endif
	{
#if 1
	VA_LIST list;
	VA_START(list, aFmt);

	// Print to log file
	TBuf<KWatcherLogBuffer> buf;
	buf.FormatList(aFmt, list);

	// Write to log file
	// DEBUG shim: serial log instead of flogger
	RDebug::Print(_L("[wlog] %S"), &buf);

	// Write to console
	if (iConsole)
		{
		buf.Append('\n');
		iConsole->Printf(KWatcherStringFormat, &buf);
		}
#endif
	}

#if 1
EXPORT_C void CWatcherLog::Printf(TRefByValue<const TDesC8> aFmt,...)
#else
EXPORT_C void CWatcherLog::Printf(TRefByValue<const TDesC8> /*aFmt*/,...)
#endif
	{
#if 1
	VA_LIST list;
	VA_START(list, aFmt);
	
	// Print to log file
	TBuf8<KWatcherLogBuffer> buf;
	buf.FormatList(aFmt, list);
	
	// Write to log file
	RDebug::Printf("[wlog] %S", &buf);
	
	// Write to console
	if (iConsole)
		{
		buf.Append('\n');
		iConsole->Printf(KWatcherStringFormat, &buf);
		}
#endif
	}

EXPORT_C TBool CWatcherLog::IsLogging() const
	{
#if 1
	return ETrue; TUint att;
	return (iFs.Att(KWatcherLogDirFull, att) == KErrNone);
#else
	return EFalse;
#endif
	}


TBool CWatcherLog::IsConsole() const
	{
#if 1
	return EFalse; TUint att;
	return (iFs.Att(KWatcherLogDirConsole, att) == KErrNone);
#else
	return EFalse;
#endif
	}
