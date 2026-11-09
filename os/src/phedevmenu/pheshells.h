/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheshells.h -- OURS: the UI shells a device can start. Shared by pheshellsel (boot), the developer menu, the Settings
// plug-in (gsphshell) and SysAp's power menu.
//
// A shell is either built into the image (Symbian^3 always; Symbian^4 when the image carries it, as before) or installed by a
// package that drops a description file into  <drive>:\resource\phe\shells\<name>.txt  (the Catalogue's Orbit package does).
// The file is plain text, one "key=value" per line, '#' starts a comment, "!:" in a path stands for the drive the file is on:
//   id=orbit                                      short name; the CenRep value (key 10) that selects it
//   name=Orbit                                    what the menus show
//   start=!:\resource\phe\shells\orbit.start      start list: the format of s4start.txt (an exe per line; "wait <exe>",
//                                                 "waitdir <dir> <ms>" ...); optional
//   launcher=!:\sys\bin\hsapplicationlauncher.exe the shell's launcher: started last; it rendezvouses when the shell is up,
//                                                 and the shell lives as long as it does
//   icon=!:\resource\phe\shells\orbit.mif         optional
// No field has a length limit of ours: a shell is a heap object with a heap buffer per field.
// The selection is CenRep key 10 of the developer-menu repository (the id; CenRep's own text limit is 1024 characters); key 1
// (0 = S^3, 1 = S^4) is kept in step for the readers that only know the two built-in shells.
#ifndef PHESHELLS_H
#define PHESHELLS_H

#include <e32base.h>
#include <f32file.h>
#include "phedevmenu.h"

// key 10: the selected shell's id (text). Absent or empty = key 1 decides
const TUint32 KPheDevKeyShellId = 10;
// key 11: starts of a non-S^3 shell that were not yet confirmed good (the shell's launcher up for KPheShellGoodSeconds).
// pheshellsel counts it up before each start; two in a row and the shell is given up: back to S^3, with the note in key 12
const TUint32 KPheDevKeyShellTries = 11;
// key 12: the caption of a shell that was given up (text); shown once, then cleared
const TUint32 KPheDevKeyShellNote = 12;
const TInt KPheShellMaxTries = 2;
const TInt KPheShellGoodSeconds = 150;
const TInt KPheCenRepTextMax = 1024;		// CenRep's own limit for a string value (centralrepository.h KMaxUnicodeStringLength)

_LIT(KPheShellIdS3, "symbian3");
_LIT(KPheShellIdS4, "symbian4");
_LIT(KPheShellsDir, "\\resource\\phe\\shells\\");

// one shell; every text is its own heap buffer (NULL = empty)
class CPheShell : public CBase
	{
public:
	~CPheShell() { delete iId; delete iName; delete iStart; delete iLauncher; delete iIcon; }
	TPtrC Id() const { return iId ? TPtrC(*iId) : TPtrC(); }
	TPtrC Name() const { return iName ? TPtrC(*iName) : TPtrC(); }
	TPtrC Start() const { return iStart ? TPtrC(*iStart) : TPtrC(); }
	TPtrC Launcher() const { return iLauncher ? TPtrC(*iLauncher) : TPtrC(); }
	TPtrC Icon() const { return iIcon ? TPtrC(*iIcon) : TPtrC(); }
	static void SetL(HBufC*& aField, const TDesC& aText)
		{
		HBufC* n = aText.AllocL();
		delete aField;
		aField = n;
		}
	TBool IsS3() const { return Id().CompareF(KPheShellIdS3) == 0; }
	// a built-in shell
	static CPheShell* NewBuiltinLC(const TDesC& aId, const TDesC& aName, const TDesC& aStart, const TDesC& aLauncher)
		{
		CPheShell* s = new (ELeave) CPheShell;
		CleanupStack::PushL(s);
		SetL(s->iId, aId);
		SetL(s->iName, aName);
		if (aStart.Length()) SetL(s->iStart, aStart);
		if (aLauncher.Length()) SetL(s->iLauncher, aLauncher);
		s->iBuiltin = ETrue;
		return s;
		}
	HBufC* iId;
	HBufC* iName;
	HBufC* iStart;		// start list (NULL = none)
	HBufC* iLauncher;
	HBufC* iIcon;
	TBool iBuiltin;
	TUint32 iIdleUid;		// the UID of the shell's idle application (0 = none): published as the active idle UID once the launcher is up
	};

typedef RPointerArray<CPheShell> RPheShells;

inline void PheShellsClose(RPheShells& aShells)
	{
	aShells.ResetAndDestroy();
	aShells.Close();
	}

// "key=value" lines of a description file (8-bit text) into a new shell; aDrive replaces a leading "!:"
inline CPheShell* PheShellParseLC(const TDesC8& aText, TChar aDrive)
	{
	CPheShell* s = new (ELeave) CPheShell;
	CleanupStack::PushL(s);
	TPtrC8 rest(aText);
	while (rest.Length())
		{
		const TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
		HBufC* l = HBufC::NewLC(line.Length());
		l->Des().Copy(line);
		TPtr lp = l->Des();
		lp.Trim();
		const TInt eq = lp.Locate('=');
		if (eq > 0 && lp[0] != '#')
			{
			TPtrC key = lp.Left(eq);
			TPtrC val = lp.Mid(eq + 1);
			HBufC* k = key.AllocLC();
			k->Des().Trim();
			HBufC* v = val.AllocLC();
			TPtr vp = v->Des();
			vp.Trim();
			if (vp.Length() >= 2 && vp[0] == '!' && vp[1] == ':')
				vp[0] = (TText)aDrive;
			if (k->CompareF(_L("id")) == 0) CPheShell::SetL(s->iId, *v);
			else if (k->CompareF(_L("name")) == 0) CPheShell::SetL(s->iName, *v);
			else if (k->CompareF(_L("start")) == 0) CPheShell::SetL(s->iStart, *v);
			else if (k->CompareF(_L("launcher")) == 0) CPheShell::SetL(s->iLauncher, *v);
			else if (k->CompareF(_L("icon")) == 0) CPheShell::SetL(s->iIcon, *v);
			else if (k->CompareF(_L("idleuid")) == 0) { TLex lx(*v); TUint32 u = 0; if (lx.Val(u, EHex) == KErrNone) s->iIdleUid = u; }
			CleanupStack::PopAndDestroy(2, k);		// v, k
			}
		CleanupStack::PopAndDestroy(l);
		}
	return s;
	}

// the shells there are: S^3 first, then S^4 when the image carries it, then the installed ones (a package's own id is dropped
// when one with it is already listed). aFs: a connected session. Leaves on out of memory.
inline void PheListShellsL(RFs& aFs, RPheShells& aShells)
	{
	CPheShell* s3 = CPheShell::NewBuiltinLC(KPheShellIdS3, _L("Symbian^3"), KNullDesC, KNullDesC);
	aShells.AppendL(s3);
	CleanupStack::Pop(s3);
	if (PheShellsInRom() & KPheShellsS4)
		{
		CPheShell* s4 = CPheShell::NewBuiltinLC(KPheShellIdS4, _L("Symbian^4"), _L("z:\\resource\\phe\\s4start.txt"),
			_L("z:\\sys\\bin\\hsapplicationlauncher.exe"));
		aShells.AppendL(s4);
		CleanupStack::Pop(s4);
		}
	TDriveList drives;
	if (aFs.DriveList(drives) != KErrNone)
		return;
	for (TInt d = EDriveC; d <= EDriveY; d++)
		{
		if (!drives[d])
			continue;
		TChar letter;
		aFs.DriveToChar(d, letter);
		TFileName dir;
		dir.Append(letter);
		dir.Append(':');
		dir.Append(KPheShellsDir);
		TFileName pattern(dir);
		pattern.Append(_L("*.txt"));
		CDir* list = NULL;
		if (aFs.GetDir(pattern, KEntryAttNormal, ESortByName, list) != KErrNone)
			continue;
		CleanupStack::PushL(list);
		for (TInt i = 0; i < list->Count(); i++)
			{
			TFileName path(dir);
			path.Append((*list)[i].iName);
			RFile f;
			if (f.Open(aFs, path, EFileRead | EFileShareReadersOnly) != KErrNone)
				continue;
			CleanupClosePushL(f);
			TInt size = 0;
			f.Size(size);
			HBufC8* text = HBufC8::NewLC(size);
			TPtr8 tp = text->Des();
			f.Read(tp);
			CPheShell* s = PheShellParseLC(*text, letter);
			CleanupStack::Pop(s);
			CleanupStack::PopAndDestroy(2, &f);		// text, f
			CleanupStack::PushL(s);
			TBool ok = s->Id().Length() && s->Launcher().Length();
			for (TInt k = 0; ok && k < aShells.Count(); k++)
				if (aShells[k]->Id().CompareF(s->Id()) == 0)
					ok = EFalse;
			if (ok)
				{
				if (!s->Name().Length())
					CPheShell::SetL(s->iName, s->Id());
				aShells.AppendL(s);
				CleanupStack::Pop(s);
				}
			else
				CleanupStack::PopAndDestroy(s);
			}
		CleanupStack::PopAndDestroy(list);
		}
	}

// the selected shell's id from a repository (key 10, else key 1); a new heap text
template <class TRepo>
inline HBufC* PheSelectedShellIdL(TRepo* aRepo)
	{
	if (aRepo)
		{
		RBuf id;
		id.CreateL(KPheCenRepTextMax);
		id.CleanupClosePushL();
		if (aRepo->Get(KPheDevKeyShellId, id) == KErrNone && id.Length())
			{
			HBufC* r = id.AllocL();
			CleanupStack::PopAndDestroy(&id);
			return r;
			}
		CleanupStack::PopAndDestroy(&id);
		TInt v = 0;
		aRepo->Get(KPheDevKeyShell, v);
		return (v == KPheShellS4 ? TPtrC(KPheShellIdS4) : TPtrC(KPheShellIdS3)).AllocL();
		}
	return TPtrC(KPheShellIdS3).AllocL();
	}

template <class TRepo>
inline void PheStoreShellId(TRepo* aRepo, const TDesC& aId)
	{
	aRepo->Set(KPheDevKeyShellId, aId);
	aRepo->Set(KPheDevKeyShell, aId.CompareF(KPheShellIdS4) == 0 ? KPheShellS4 : KPheShellS3);
	}

#endif
