/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// phetty.h -- OURS (phe prefix): the developer tty daemon.
// One server process (phettyd) owns up to KPheTtyMax ttys. Each tty = a scrollback of ANSI bytes (what the shell's console
// wrote), a key queue (what the user typed) and a shell process (pheshell = ESHELL with its console redirected to the tty).
// Observers (the on-screen pheterm, a PhoneLab bridge, phettyctl) read the byte stream by sequence number and inject keys.
#ifndef PHETTY_H
#define PHETTY_H

#include <e32std.h>
#include <e32base.h>

_LIT(KPheTtyServerName, "!phettyd");
_LIT(KPheTtyServerExe, "phettyd.exe");
_LIT(KPheShellExe, "pheshell.exe");
const TUint32 KPheTtyUid = 0xE7015901;
const TUint32 KPheTtyCenRepUid = 0xE7015904;       // key 1 = enabled (PHETTY=on|off), key 2 = ttys started at boot
const TInt KPheTtyMax = 8;
const TInt KPheTtyRing = 32768;                    // scrollback bytes kept per tty
const TInt KPheTtyDefCols = 80;
const TInt KPheTtyDefRows = 24;
const TInt KPheShellSlot = 3;                      // process parameter slot carrying the tty id to pheshell

enum TPheTtyOp
	{
	EPtNew,          // a0 cols, a1 rows (0 = default) -> tty id
	EPtList,         // a0 des8 (write): one byte per live tty id -> count
	EPtInfo,         // a0 id, a1 TPckg<TPheTtyInfo> (write)
	EPtResize,       // a0 id, a1 cols, a2 rows
	EPtKill,         // a0 id (kills the shell and drops the tty)
	EPtKey,          // a0 id, a1 TKeyCode, a2 modifiers
	EPtBytes,        // a0 id, a1 des8 (read): raw terminal bytes (ASCII + CSI arrows) -> keys
	EPtRead,         // a0 id, a1 des8 (write) = [u32 seq of first byte][bytes], a2 from seq, a3 1 = wait (async) -> length
	EPtReadCancel,   // cancels this session's pending EPtRead
	EPtConWrite,     // (console side) a0 id, a1 des8 (read) ANSI bytes -> cols<<16 | rows
	EPtConRead,      // (console side, async) a0 id, a1 TPckg<TPheKey> (write)
	EPtConReadCancel,
	EPtConSize,      // a0 id -> cols<<16 | rows
	EPtConTitle,     // a0 id, a1 des16 (read)
	EPtShellState,   // a0 id -> 0 = dead, 1 = running, 2 = running and restarted at least once
	EPtAdopt,        // a0 id, a1 process id: a program that tty's shell started (its stdio goes to the tty)
	EPtTtyOf         // a0 process id -> the tty that adopted it, or KErrNotFound (the stdio server asks)
	};

struct TPheTtyInfo
	{
	TInt iId;
	TInt iCols;
	TInt iRows;
	TInt iShell;           // see EPtShellState
	TUint32 iEndSeq;       // sequence number of the next byte to be written
	TUint32 iStartSeq;     // oldest byte still in the scrollback
	TBuf<32> iTitle;
	};
typedef TPckgBuf<TPheTtyInfo> TPheTtyInfoBuf;

struct TPheKey
	{
	TUint iCode;
	TUint iMods;
	};
typedef TPckgBuf<TPheKey> TPheKeyBuf;

// Client handle (pheclient.cpp, compiled into every user of the daemon).
class RPheTty : public RSessionBase
	{
public:
	TInt Connect();                       // connects; starts phettyd first if it is not running (EFalse start: aStart)
	TInt ConnectNoStart();
	TInt New(TInt aCols = 0, TInt aRows = 0);
	TInt List(TUint8* aIds, TInt aMax);
	TInt Info(TInt aId, TPheTtyInfo& aInfo);
	TInt Resize(TInt aId, TInt aCols, TInt aRows);
	TInt Kill(TInt aId);
	TInt Key(TInt aId, TUint aCode, TUint aMods = 0);
	TInt Bytes(TInt aId, const TDesC8& aBytes);
	// aBuf: 4 bytes of first-byte sequence, then the data. Completes with the length (>= 4) or an error.
	void Read(TInt aId, TDes8& aBuf, TUint32 aFrom, TBool aWait, TRequestStatus& aStatus);
	void ReadCancel();
	TInt ConWrite(TInt aId, const TDesC8& aBytes);
	void ConRead(TInt aId, TPheKeyBuf& aKey, TRequestStatus& aStatus);
	void ConReadCancel();
	TInt ConSize(TInt aId);
	TInt ConTitle(TInt aId, const TDesC& aTitle);
	TInt ShellState(TInt aId);
	TInt Adopt(TInt aId, TUint aPid);
	TInt TtyOf(TUint aPid);
	static TInt StartServer();
	};

#endif
