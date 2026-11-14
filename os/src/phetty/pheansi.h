/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// pheansi.h -- OURS: a minimal ANSI/VT screen model (header-only): feed it a tty's byte stream, read back the cell grid.
// Handles what pheconsole.cpp emits and what a shell commonly needs: printable UTF-8, CR LF BS TAB, CSI A B C D H f J K m,
// ?25h/l. Wrap is deferred as in xterm (a character at the last column sets a pending wrap).
#ifndef PHEANSI_H
#define PHEANSI_H

#include <e32std.h>
#include <e32base.h>

const TUint8 KPheAttrInverse = 1;
const TUint8 KPheAttrBold = 2;

class CPheScreen : public CBase
	{
public:
	static CPheScreen* NewL(TInt aCols, TInt aRows)
		{
		CPheScreen* s = new (ELeave) CPheScreen;
		CleanupStack::PushL(s);
		s->ConstructL(aCols, aRows);
		CleanupStack::Pop(s);
		return s;
		}
	~CPheScreen() { delete[] iCh; delete[] iAt; }
	void ConstructL(TInt aCols, TInt aRows)
		{
		iCols = aCols;
		iRows = aRows;
		iCh = new (ELeave) TUint16[aCols * aRows];
		iAt = new (ELeave) TUint8[aCols * aRows];
		iCursorOn = ETrue;
		Clear();
		}
	void Clear()
		{
		for (TInt i = 0; i < iCols * iRows; i++) { iCh[i] = ' '; iAt[i] = 0; }
		iX = iY = 0;
		iPending = EFalse;
		}
	TInt Cols() const { return iCols; }
	TInt Rows() const { return iRows; }
	TUint16 Ch(TInt aX, TInt aY) const { return iCh[aY * iCols + aX]; }
	TUint8 Attr(TInt aX, TInt aY) const { return iAt[aY * iCols + aX]; }
	TInt CurX() const { return iX; }
	TInt CurY() const { return iY; }
	TBool CursorOn() const { return iCursorOn; }

	void Feed(const TUint8* aData, TInt aLen)
		{
		for (TInt i = 0; i < aLen; i++)
			Byte(aData[i]);
		}

private:
	CPheScreen() {}
	void Put(TUint aCh)
		{
		if (iPending)
			{ iPending = EFalse; iX = 0; Lf(); }
		iCh[iY * iCols + iX] = (TUint16)aCh;
		iAt[iY * iCols + iX] = iAttr;
		if (iX + 1 >= iCols)
			iPending = ETrue;
		else
			iX++;
		}
	void Lf()
		{
		if (iY + 1 >= iRows)
			{
			for (TInt r = 1; r < iRows; r++)
				for (TInt c = 0; c < iCols; c++)
					{ iCh[(r - 1) * iCols + c] = iCh[r * iCols + c]; iAt[(r - 1) * iCols + c] = iAt[r * iCols + c]; }
			for (TInt c = 0; c < iCols; c++)
				{ iCh[(iRows - 1) * iCols + c] = ' '; iAt[(iRows - 1) * iCols + c] = 0; }
			}
		else
			iY++;
		}
	void Erase(TInt aFrom, TInt aTo)       // linear cell range [aFrom, aTo)
		{
		for (TInt i = aFrom; i < aTo; i++)
			if (i >= 0 && i < iCols * iRows) { iCh[i] = ' '; iAt[i] = 0; }
		}
	TInt Param(TInt aN, TInt aDef) const
		{
		return (aN < iNP && iPar[aN] >= 0) ? iPar[aN] : aDef;
		}
	void Csi(TUint8 aFinal)
		{
		iPending = (aFinal == 'H' || aFinal == 'f' || aFinal == 'A' || aFinal == 'B' || aFinal == 'C' || aFinal == 'D') ? EFalse : iPending;
		switch (aFinal)
			{
		case 'H': case 'f':
			iY = Clampi(Param(0, 1) - 1, 0, iRows - 1);
			iX = Clampi(Param(1, 1) - 1, 0, iCols - 1);
			break;
		case 'A': iY = Clampi(iY - Param(0, 1), 0, iRows - 1); break;
		case 'B': iY = Clampi(iY + Param(0, 1), 0, iRows - 1); break;
		case 'C': iX = Clampi(iX + Param(0, 1), 0, iCols - 1); break;
		case 'D': iX = Clampi(iX - Param(0, 1), 0, iCols - 1); break;
		case 'J':
			{
			TInt m = Param(0, 0), at = iY * iCols + iX;
			if (m == 0) Erase(at, iCols * iRows);
			else if (m == 1) Erase(0, at + 1);
			else { Erase(0, iCols * iRows); }
			break;
			}
		case 'K':
			{
			TInt m = Param(0, 0), row = iY * iCols;
			if (m == 0) Erase(row + iX, row + iCols);
			else if (m == 1) Erase(row, row + iX + 1);
			else Erase(row, row + iCols);
			break;
			}
		case 'm':
			for (TInt k = 0; k < (iNP ? iNP : 1); k++)
				{
				TInt v = Param(k, 0);
				if (v == 0) iAttr = 0;
				else if (v == 1) iAttr |= KPheAttrBold;
				else if (v == 7) iAttr |= KPheAttrInverse;
				else if (v == 22) iAttr &= ~KPheAttrBold;
				else if (v == 27) iAttr &= ~KPheAttrInverse;
				}
			break;
		case 'h': case 'l':
			if (iPriv && Param(0, 0) == 25)
				iCursorOn = (aFinal == 'h');
			break;
		default:
			break;
			}
		}
	static TInt Clampi(TInt v, TInt lo, TInt hi) { return v < lo ? lo : (v > hi ? hi : v); }
	void Byte(TUint8 c)
		{
		switch (iState)
			{
		case 1:                                   // after ESC
			if (c == '[') { iState = 2; iNP = 0; iPriv = EFalse; iPar[0] = -1; iHave = EFalse; }
			else iState = 0;
			return;
		case 2:                                   // CSI parameters
			if (c >= '0' && c <= '9')
				{
				if (iNP == 0) { iNP = 1; iPar[0] = 0; }
				if (iPar[iNP - 1] < 0)
					iPar[iNP - 1] = 0;           // -1 = parameter present but empty so far
				iPar[iNP - 1] = iPar[iNP - 1] * 10 + (c - '0');
				}
			else if (c == ';')
				{
				if (iNP == 0) { iNP = 1; iPar[0] = -1; }
				if (iNP < 6) { iPar[iNP] = -1; iNP++; }
				}
			else if (c == '?')
				iPriv = ETrue;
			else if (c >= 0x40 && c <= 0x7e)
				{
				iState = 0;
				Csi(c);
				}
			return;
		case 3:                                   // UTF-8 continuation
			iU = (iU << 6) | (c & 0x3f);
			if (--iUNeed == 0) { iState = 0; Put(iU); }
			return;
		default:
			break;
			}
		if (c == 0x1b) { iState = 1; return; }
		if (c == '\r') { iX = 0; iPending = EFalse; return; }
		if (c == '\n') { Lf(); return; }
		if (c == 0x08) { if (iX > 0) iX--; iPending = EFalse; return; }
		if (c == 0x09) { iX = Clampi((iX + 8) & ~7, 0, iCols - 1); return; }
		if (c < 0x20) return;
		if (c >= 0xe0) { iState = 3; iU = c & 0x0f; iUNeed = 2; return; }
		if (c >= 0xc0) { iState = 3; iU = c & 0x1f; iUNeed = 1; return; }
		if (c >= 0x80) return;
		Put(c);
		}
private:
	TInt iCols = 0, iRows = 0;
	TUint16* iCh = NULL;
	TUint8* iAt = NULL;
	TInt iX = 0, iY = 0;
	TBool iPending = EFalse;
	TBool iCursorOn = ETrue;
	TUint8 iAttr = 0;
	TInt iState = 0;
	TInt iPar[6] = {0, 0, 0, 0, 0, 0};
	TInt iNP = 0;
	TBool iPriv = EFalse;
	TBool iHave = EFalse;
	TUint iU = 0;
	TInt iUNeed = 0;
	};

#endif
