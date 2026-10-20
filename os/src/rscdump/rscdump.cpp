// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// rscdump.cpp -- OURS (X4.79): print the readable strings of every resource in a resource file, decoded by the
// platform's own reader (bafl RResourceFile: plain, dictionary- and run-length-compressed files alike). For ground
// truth from a firmware we cannot decompress on the host -- e.g. a phone's compiled SSM command lists (which
// programs the real phone starts at boot), copied onto C: of a scratch disk:
//
//     rscdump c:\data\noncriticalcmdlist.rsc [more files ...]
//
// Output on the serial log: "[rscdump] <file> #<id> <len> B: <string> | <string> ..." -- 8-bit and 16-bit runs of
// printable characters (>= 4), which is where a command list's executable names and arguments live.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <barsc2.h>

static void Strings(const TDesC8& aData, TDes8& aOut)
	{
	// 16-bit runs (resource LTEXT/BUF are UTF-16 in a Unicode build), then 8-bit runs
	for (TInt odd=0; odd<2; ++odd)
		{
		TBuf8<256> run;
		for (TInt i=odd; i+1<aData.Length(); i+=2)
			{
			TUint c = aData[i] | aData[i+1] << 8;
			if (c >= 0x20 && c < 0x7F && run.Length() < run.MaxLength())
				run.Append((TChar)c);
			else
				{
				if (run.Length() >= 4 && aOut.Length() + run.Length() + 3 < aOut.MaxLength())
					{ aOut.Append(run); aOut.Append(_L8(" | ")); }
				run.Zero();
				}
			}
		if (run.Length() >= 4 && aOut.Length() + run.Length() + 3 < aOut.MaxLength())
			{ aOut.Append(run); aOut.Append(_L8(" | ")); }
		}
	}

static void DumpL(RFs& aFs, const TDesC& aFile)
	{
	TBuf8<256> name8;
	name8.Copy(aFile.Left(250));
	CResourceFile* rf = CResourceFile::NewLC(aFs, aFile, 0, 0);
	TInt found = 0;
	// resource ids: the file's offset (upper 20 bits) + an index 1..n; walk the indices until a gap of 32
	const TInt base = rf->Offset();
	for (TInt i=1, misses=0; misses<32 && i<4096; ++i)
		{
		TBool owns = EFalse;
		TRAP_IGNORE(owns = rf->OwnsResourceIdL(base | i));
		if (!owns)
			{ ++misses; continue; }
		misses = 0;
		HBufC8* data = NULL;
		TRAPD(err, data = rf->AllocReadL(base | i));
		if (err)
			{
			RDebug::Printf("[rscdump] %S #%d: read error %d", &name8, i, err);
			continue;
			}
		TBuf8<400> s;
		Strings(*data, s);
		RDebug::Printf("[rscdump] %S #%d %d B: %S", &name8, i, data->Length(), &s);
		delete data;
		++found;
		}
	RDebug::Printf("[rscdump] %S: %d resources", &name8, found);
	CleanupStack::PopAndDestroy(rf);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RFs fs;
	TInt r = fs.Connect();
	TBuf<512> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	for (TPtrC f = lex.NextToken(); r == KErrNone && f.Length(); f.Set(lex.NextToken()))
		{
		TRAPD(err, DumpL(fs, f));
		if (err)
			{
			TBuf8<256> n8;
			n8.Copy(f.Left(250));
			RDebug::Printf("[rscdump] %S: error %d", &n8, err);
			}
		}
	fs.Close();
	delete cleanup;
	return r;
	}
