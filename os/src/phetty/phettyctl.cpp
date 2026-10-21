// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phettyctl.cpp -- OURS: headless phettyd test client. Starts the daemon if needed, waits for tty 1's pheshell, opens tty 2,
// types into both and prints each tty's screen (via the ANSI model) to the debug port, then checks the ttys kept separate state.
//   phettyctl            the self-test (prints [phettyctl] PASS / FAIL)
//   phettyctl list       lists ttys
#include <e32debug.h>
#include <e32keys.h>
#include "phetty.h"
#include "pheansi.h"

struct TView
	{
	TInt iId;
	CPheScreen* iScr;
	TUint32 iSeq;
	};

static void Pump(RPheTty& aTty, TView& aV)
	{
	TBuf8<2048> b;
	for (;;)
		{
		TRequestStatus st;
		aTty.Read(aV.iId, b, aV.iSeq, EFalse, st);
		User::WaitForRequest(st);
		if (st.Int() <= 4)
			return;
		TUint32 first = b[0] | (b[1] << 8) | (b[2] << 16) | ((TUint32)b[3] << 24);
		if (first != aV.iSeq)
			aV.iScr->Clear();            // the scrollback lost bytes we had not read: restart the screen from what is left
		aV.iScr->Feed(b.Ptr() + 4, st.Int() - 4);
		aV.iSeq = first + (st.Int() - 4);
		}
	}

static void Dump(const TView& aV, const char* aTag)
	{
	CPheScreen& s = *aV.iScr;
	RDebug::Printf("[phettyctl] ---- tty %d (%s) %dx%d cursor %d,%d", aV.iId, aTag, s.Cols(), s.Rows(), s.CurX(), s.CurY());
	for (TInt y = 0; y < s.Rows(); y++)
		{
		char line[260];
		TInt n = 0;
		for (TInt x = 0; x < s.Cols() && n < 250; x++)
			{
			TUint16 c = s.Ch(x, y);
			line[n++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
			}
		while (n > 0 && line[n - 1] == ' ')
			n--;
		line[n] = 0;
		if (n)
			RDebug::Printf("[phettyctl] t%d|%s", aV.iId, line);
		}
	}

static TBool Contains(const TView& aV, const TDesC8& aNeedle)
	{
	CPheScreen& s = *aV.iScr;
	for (TInt y = 0; y < s.Rows(); y++)
		{
		TBuf8<256> row;
		for (TInt x = 0; x < s.Cols() && x < 256; x++)
			row.Append((TUint8)(s.Ch(x, y) < 0x80 ? s.Ch(x, y) : '?'));
		if (row.FindF(aNeedle) != KErrNotFound)
			return ETrue;
		}
	return EFalse;
	}

// the raw stream, escaped (ESC = ^, CR = |, LF = _), 100 bytes per line -- for debugging the screen model
static void Raw(RPheTty& aTty, TInt aId)
	{
	TUint32 seq = 0;
	TBuf8<2048> b;
	TBuf8<120> line;
	for (;;)
		{
		TRequestStatus st;
		aTty.Read(aId, b, seq, EFalse, st);
		User::WaitForRequest(st);
		if (st.Int() <= 4)
			break;
		seq = (b[0] | (b[1] << 8) | (b[2] << 16) | ((TUint32)b[3] << 24)) + (st.Int() - 4);
		for (TInt i = 4; i < st.Int(); i++)
			{
			TUint8 c = b[i];
			line.Append(c == 0x1b ? '^' : c == 0x0d ? '|' : c == 0x0a ? '_' : (c < 0x20 ? '.' : c));
			if (line.Length() >= 100)
				{
				line.Append(0);
				RDebug::Printf("[phettyctl] raw%d: %s", aId, line.Ptr());
				line.Zero();
				}
			}
		}
	if (line.Length())
		{
		line.Append(0);
		RDebug::Printf("[phettyctl] raw%d: %s", aId, line.Ptr());
		}
	}

static void Wait(TInt aMs) { User::After(aMs * 1000); }

static void Type(RPheTty& aTty, TInt aId, const char* aText)
	{
	TPtrC8 p((const TUint8*)aText, User::StringLength((const TUint8*)aText));
	aTty.Bytes(aId, p);
	}

static void Run()
	{
	RPheTty tty;
	TInt r = tty.Connect();
	RDebug::Printf("[phettyctl] connect -> %d", r);
	if (r != KErrNone)
		{
		RDebug::Printf("[phettyctl] FAIL (connect)");
		return;
		}
	TBuf<64> cmd;
	User::CommandLine(cmd);
	TUint8 ids[KPheTtyMax];
	TInt n = tty.List(ids, KPheTtyMax);
	RDebug::Printf("[phettyctl] %d tty(s)", n);
	if (cmd.FindF(_L("list")) >= 0)
		{
		for (TInt i = 0; i < n; i++)
			{
			TPheTtyInfo in;
			tty.Info(ids[i], in);
			RDebug::Printf("[phettyctl] tty %d %dx%d shell %d seq %u..%u", in.iId, in.iCols, in.iRows, in.iShell, in.iStartSeq, in.iEndSeq);
			}
		tty.Close();
		return;
		}
	// wait for tty 1's shell to put its banner/prompt up
	TInt first = n ? ids[0] : tty.New();
	TPheTtyInfo in;
	for (TInt i = 0; i < 120; i++)
		{
		tty.Info(first, in);
		if (in.iEndSeq > 20)
			break;
		Wait(500);
		}
	RDebug::Printf("[phettyctl] tty %d first output after wait: seq %u shell state %d", first, in.iEndSeq, in.iShell);
	TInt second = tty.New();
	RDebug::Printf("[phettyctl] New() -> %d", second);
	for (TInt i = 0; i < 120 && second > 0; i++)
		{
		tty.Info(second, in);
		if (in.iEndSeq > 20)
			break;
		Wait(500);
		}
	TView v1 = {first, CPheScreen::NewL(KPheTtyDefCols, KPheTtyDefRows), 0};
	TView v2 = {second, second > 0 ? CPheScreen::NewL(KPheTtyDefCols, KPheTtyDefRows) : NULL, 0};
	Pump(tty, v1);
	Dump(v1, "boot");
	if (second > 0)
		{
		Pump(tty, v2);
		Dump(v2, "boot");
		}
	Type(tty, first, "cd \\sys\r");
	if (second > 0)
		Type(tty, second, "cd z:\\resource\r");
	Wait(3000);
	Type(tty, first, "echo hello-tty-one\r");
	if (second > 0)
		Type(tty, second, "dir\r");
	Wait(4000);
	Pump(tty, v1);
	Dump(v1, "after typing");
	if (second > 0)
		{
		Pump(tty, v2);
		Dump(v2, "after typing");
		if (!(Contains(v2, _L8("resource")) && !Contains(v2, _L8("hello-tty-one"))))
			Raw(tty, second);
		}
	TBool ok = Contains(v1, _L8("hello-tty-one")) && Contains(v1, _L8("sys")) && second > 0 && Contains(v2, _L8("resource"))
	           && !Contains(v2, _L8("hello-tty-one"));
	if (second > 0)
		tty.Kill(second);
	n = tty.List(ids, KPheTtyMax);
	RDebug::Printf("[phettyctl] after Kill: %d tty(s)", n);
	RDebug::Printf(ok ? "[phettyctl] PASS" : "[phettyctl] FAIL");
	delete v1.iScr;
	delete v2.iScr;
	tty.Close();
	}

TInt E32Main()
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TRAPD(r, Run());
	if (r != KErrNone)
		RDebug::Printf("[phettyctl] FAIL leave %d", r);
	delete c;
	return r;
	}
