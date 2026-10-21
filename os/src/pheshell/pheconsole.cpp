// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheconsole.cpp -- OURS: the console eshell talks to inside pheshell: a CColorConsoleBase whose screen is a phettyd tty.
// Output: the console calls become ANSI/VT bytes (CUP, EL, ED, SGR, cursor show/hide) in the tty's scrollback; the cursor is
// tracked here (ewsrv's text-console rules: LF = CR+LF, CR to column 0, wrap at the right edge, scroll at the bottom), and
// every text run is preceded by an absolute CUP when the terminal's cursor is elsewhere, so a consumer never has to guess.
// Input: Read() = a pending EPtConRead on the tty's key queue.
// (the build defines Console=PheConsole for every TU of pheshell: the real <e32cons.h> then declares class PheConsole with
// the IMPORT_C static NewL, and this file DEFINES it -- the one place eshell's Console::NewL call lands)
#include <e32cons.h>
#include <e32keys.h>
#include <e32debug.h>
#include "../phetty/phetty.h"

class CPheConsole : public CColorConsoleBase
	{
public:
	~CPheConsole();
	TInt Create(const TDesC& aTitle, TSize aSize);
	void Read(TRequestStatus& aStatus);
	void ReadCancel();
	void Write(const TDesC& aDes);
	TPoint CursorPos() const;
	void SetCursorPosAbs(const TPoint& aPoint);
	void SetCursorPosRel(const TPoint& aPoint);
	void SetCursorHeight(TInt aPercentage);
	void SetTitle(const TDesC& aTitle);
	void ClearScreen();
	void ClearToEndOfLine();
	TSize ScreenSize() const;
	TKeyCode KeyCode() const;
	TUint KeyModifiers() const;
	void SetTextAttribute(TTextAttribute aAttribute);
private:
	void Out(const char* aText);
	void OutNum(TInt aNum);
	void Flush();
	void Sync();                     // CUP to (iX,iY) unless the terminal is already there
	void Cup(TInt aX, TInt aY);
	void Resized(TInt aPacked);
	void LineFeed();
private:
	RPheTty iTty;
	TInt iId;
	TInt iCols, iRows;
	TInt iX, iY;                     // the console's cursor
	TInt iEX, iEY;                   // where the terminal's cursor is after the bytes emitted so far (-1 = unknown)
	TBuf8<1536> iOut;
	TPheKeyBuf iKey;
	};

CPheConsole::~CPheConsole()
	{
	iTty.Close();
	}

void CPheConsole::Resized(TInt aPacked)
	{
	if (aPacked <= 0)
		return;
	iCols = (aPacked >> 16) & 0xffff;
	iRows = aPacked & 0xffff;
	if (iX >= iCols) iX = iCols - 1;
	if (iY >= iRows) iY = iRows - 1;
	}

static TInt gPheTtyId;                       // this shell's tty (pheshell is one console per process)

TInt CPheConsole::Create(const TDesC& aTitle, TSize)
	{
	TInt id = 0;
	TInt r = User::GetTIntParameter(KPheShellSlot, id);
	if (r != KErrNone)
		return r;
	iId = id;
	gPheTtyId = id;                          // (a parameter slot reads once: PheTtyAdoptChild needs it later)
	r = iTty.ConnectNoStart();
	if (r != KErrNone)
		return r;
	TInt packed = iTty.ConSize(iId);
	if (packed < 0)
		return packed;
	Resized(packed);
	iX = iY = 0;
	iEX = iEY = -1;
	if (aTitle.Length())
		iTty.ConTitle(iId, aTitle);
	ClearScreen();
	return KErrNone;
	}

void CPheConsole::Flush()
	{
	if (iOut.Length())
		{
		Resized(iTty.ConWrite(iId, iOut));
		iOut.Zero();
		}
	}

void CPheConsole::Out(const char* aText)
	{
	TInt n = User::StringLength((const TUint8*)aText);
	if (iOut.Length() + n > iOut.MaxLength())
		Flush();
	iOut.Append((const TUint8*)aText, n);
	}

void CPheConsole::OutNum(TInt aNum)
	{
	if (iOut.Length() + 12 > iOut.MaxLength())
		Flush();
	iOut.AppendNum(aNum);
	}

void CPheConsole::Cup(TInt aX, TInt aY)
	{
	Out("\x1b[");
	OutNum(aY + 1);
	Out(";");
	OutNum(aX + 1);
	Out("H");
	iEX = aX;
	iEY = aY;
	}

void CPheConsole::Sync()
	{
	if (iEX != iX || iEY != iY)
		Cup(iX, iY);
	}

void CPheConsole::LineFeed()
	{
	if (iY + 1 >= iRows)
		{
		Cup(iX, iRows - 1);
		Out("\n");                    // the terminal scrolls
		}
	else
		iY++;
	}

void CPheConsole::Write(const TDesC& aDes)
	{
	for (TInt i = 0; i < aDes.Length(); i++)
		{
		TUint c = aDes[i];
		if (c == 0x0a)
			{ iX = 0; LineFeed(); continue; }      // ewsrv's new-line mode: LF also returns the carriage
		if (c == 0x0d)
			{ iX = 0; continue; }
		if (c == 0x08)
			{ if (iX > 0) iX--; continue; }
		if (c == 0x09)
			{ iX = (iX + 8) & ~7; if (iX >= iCols) iX = iCols - 1; continue; }
		if (c < 0x20 || c == 0x7f)
			continue;
		if (iX >= iCols)
			{ iX = 0; LineFeed(); }
		Sync();
		if (iOut.Length() + 4 > iOut.MaxLength())
			Flush();
		if (c < 0x80)
			iOut.Append((TUint8)c);
		else if (c < 0x800)
			{ iOut.Append((TUint8)(0xc0 | (c >> 6))); iOut.Append((TUint8)(0x80 | (c & 0x3f))); }
		else
			{ iOut.Append((TUint8)(0xe0 | (c >> 12))); iOut.Append((TUint8)(0x80 | ((c >> 6) & 0x3f))); iOut.Append((TUint8)(0x80 | (c & 0x3f))); }
		iX++;
		iEX = (iX >= iCols) ? iCols - 1 : iX;   // a terminal at the last column holds a pending wrap, not a new column
		if (iX >= iCols)
			iEX = -1;                           // never trust that state: the next run re-positions
		}
	Flush();
	}

void CPheConsole::Read(TRequestStatus& aStatus)
	{
	iTty.ConRead(iId, iKey, aStatus);
	}

void CPheConsole::ReadCancel()
	{
	iTty.ConReadCancel();
	}

TPoint CPheConsole::CursorPos() const { return TPoint(iX, iY); }

void CPheConsole::SetCursorPosAbs(const TPoint& aPoint)
	{
	iX = aPoint.iX < 0 ? 0 : (aPoint.iX >= iCols ? iCols - 1 : aPoint.iX);
	iY = aPoint.iY < 0 ? 0 : (aPoint.iY >= iRows ? iRows - 1 : aPoint.iY);
	}

void CPheConsole::SetCursorPosRel(const TPoint& aPoint)
	{
	SetCursorPosAbs(TPoint(iX + aPoint.iX, iY + aPoint.iY));
	}

void CPheConsole::SetCursorHeight(TInt aPercentage)
	{
	Out(aPercentage <= 0 ? "\x1b[?25l" : "\x1b[?25h");
	Flush();
	}

void CPheConsole::SetTitle(const TDesC& aTitle)
	{
	iTty.ConTitle(iId, aTitle);
	}

void CPheConsole::ClearScreen()
	{
	Out("\x1b[2J\x1b[H");
	iX = iY = 0;
	iEX = iEY = 0;
	Flush();
	}

void CPheConsole::ClearToEndOfLine()
	{
	Sync();
	Out("\x1b[K");
	Flush();
	}

TSize CPheConsole::ScreenSize() const { return TSize(iCols, iRows); }

TKeyCode CPheConsole::KeyCode() const { return (TKeyCode)iKey().iCode; }

TUint CPheConsole::KeyModifiers() const { return iKey().iMods; }

void CPheConsole::SetTextAttribute(TTextAttribute aAttribute)
	{
	switch (aAttribute)
		{
	case ETextAttributeBold: Out("\x1b[0;1m"); break;
	case ETextAttributeInverse: Out("\x1b[0;7m"); break;
	case ETextAttributeHighlight: Out("\x1b[0;1;7m"); break;
	default: Out("\x1b[0m"); break;
		}
	Flush();
	}

CConsoleBase* PheConsole::NewL(const TDesC& aTitle, TSize aSize)
	{
	CPheConsole* c = new (ELeave) CPheConsole;
	TInt r = c->Create(aTitle, aSize);
	if (r != KErrNone)
		{
		delete c;
		User::Leave(r);
		}
	return c;
	}

// Called by the launcher (ts_edshl_ph.cpp) between RProcess::Create and Resume: the program is this shell's, so its
// Open C stdio (our stdioserver) goes to this tty.
void PheTtyAdoptChild(RProcess& aChild)
	{
	TInt id = gPheTtyId;
	if (id <= 0)
		return;
	RPheTty tty;
	if (tty.ConnectNoStart() != KErrNone)
		return;
	TInt r = tty.Adopt(id, (TUint)aChild.Id());
	if (r != KErrNone)
		RDebug::Printf("PH: [pheshell] adopt %u on tty %d: %d", (TUint)aChild.Id(), id, r);
	tty.Close();
	}
