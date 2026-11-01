// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phewhere.cpp -- OURS: which DLL holds a code address, and the nearest export below it. RAM-loaded code runs at the
// same address in every process (the code is shared), so a crash pc from the kernel's exception log can be placed
// by loading the candidate DLLs here and walking their export tables:
//   phewhere <hex address> <dll> [<dll> ...]
// prints, per DLL, the address range its exports span and, for the one containing the address, the nearest export
// ordinal below it and the offset (ordinal -> name via the DLL's .def). Output: RDebug + C:\data\phewhere.txt.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

static RFile gOut;
_LIT8(KContains, " <-- CONTAINS");

static void Log(TRefByValue<const TDesC8> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf8<256> line;
	line.AppendFormatList(aFmt, list);
	RDebug::Printf("[phewhere] %S", &line);
	line.Append('\n');
	gOut.Write(line);
	}

static void MainL()
	{
	TBuf<256> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TPtrC tok = lex.NextToken();
	TLex hex(tok.Left(2) == _L("0x") ? tok.Mid(2) : tok);
	TUint32 addr = 0;
	if (hex.Val(addr, EHex) != KErrNone)
		{
		Log(_L8("usage: phewhere <hex address> <dll> [<dll> ...]"));
		return;
		}
	Log(_L8("address 0x%08x"), addr);
	while (!lex.Eos())
		{
		TPtrC name = lex.NextToken();
		if (!name.Length())
			break;
		TBuf8<64> name8;
		name8.Copy(name.Left(64));
		RLibrary lib;
		TInt r = lib.Load(name);
		if (r != KErrNone)
			{
			Log(_L8("%S: load %d"), &name8, r);
			continue;
			}
		TUint32 lo = 0xffffffffu, hi = 0, best = 0;
		TInt bestOrd = 0, count = 0;
		for (TInt ord = 1; ord < 20000; ++ord)
			{
			const TUint32 f = (TUint32)lib.Lookup(ord);
			if (!f)
				break;
			++count;
			if (f < lo) lo = f;
			if (f > hi) hi = f;
			if (f <= addr && f > best)
				{
				best = f;
				bestOrd = ord;
				}
			}
		TInt code = 0, data = 0;
		lib.GetRamSizes(code, data);
		// the exports lie inside the code; the code starts at most a little below the lowest one
		const TBool inside = count && addr + 0x10000 >= lo && addr < lo + (TUint32)code;
		Log(_L8("%S: %d exports 0x%08x..0x%08x, code %d bytes%S"), &name8, count, lo, hi, code,
			inside ? &KContains() : &KNullDesC8);
		if (inside && bestOrd)
			Log(_L8("%S: nearest export below = ordinal %d at 0x%08x, +0x%x"), &name8, bestOrd, best, addr - best);
		lib.Close();
		}
	}

TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		fs.MkDirAll(_L("C:\\data\\"));
		if (gOut.Replace(fs, _L("C:\\data\\phewhere.txt"), EFileWrite) == KErrNone)
			{
			TRAP_IGNORE(MainL());
			gOut.Close();
			}
		fs.Close();
		}
	delete tc;
	return 0;
	}
