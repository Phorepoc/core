/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheshells.h -- OURS: the UI shells a device can start. Shared by pheshellsel (boot), the developer menu, the Settings
// plug-in (gsphshell) and SysAp's power menu.
//
// A shell is either built into the image (Symbian^3 always; Symbian^4 when the image carries it, as before) or installed by a
// package that drops a description file into  <drive>:\resource\phe\shells\<name>.txt  (the Catalogue's Symbian^4 package does).
// The file is plain text, one "key=value" per line, '#' starts a comment, "!:" in a path stands for the drive the file is on:
//   id=symbian4                                   short name, [a-z0-9]; the CenRep value (key 10) that selects it
//   name=Symbian^4                                what the menus show
//   start=!:\resource\phe\shells\symbian4.start   start list: the format of s4start.txt (an exe per line; "wait <exe>",
//                                                 "waitdir <dir> <ms>" ...); optional
//   launcher=hsapplicationlauncher.exe            the shell's launcher: started last; it rendezvouses when the shell is up,
//                                                 and the shell lives as long as it does
//   icon=!:\resource\phe\shells\symbian4.mif      optional
// The selection is CenRep key 10 of the developer-menu repository (the id); key 1 (0 = S^3, 1 = S^4) is kept in step for the
// readers that only know the two built-in shells.
#ifndef PHESHELLS_H
#define PHESHELLS_H

#include <e32std.h>
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

_LIT(KPheShellIdS3, "symbian3");
_LIT(KPheShellIdS4, "symbian4");
_LIT(KPheShellsDir, "\resource\phe\shells\\");

class TPheShell
	{
public:
	TBuf<24> iId;
	TBuf<40> iName;
	TBuf<KMaxFileName> iStart;		// start list ("" = none)
	TBuf<KMaxFileName> iLauncher;
	TBuf<KMaxFileName> iIcon;
	TBool iBuiltin;
	};

// "key=value" lines of a description file
inline void PheShellParse(const TDesC8& aText, TChar aDrive, TPheShell& aShell)
	{
	TPtrC8 rest(aText);
	while (rest.Length())
		{
		const TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
		TBuf<KMaxFileName + 24> l;
		l.Copy(line.Left(Min(line.Length(), (TInt)l.MaxLength())));
		l.Trim();
		const TInt eq = l.Locate('=');
		if (eq <= 0 || l[0] == '#')
			continue;
		TBuf<16> key(l.Left(Min(eq, 16)));
		key.Trim();
		TBuf<KMaxFileName> val(l.Mid(eq + 1).Left(KMaxFileName));
		val.Trim();
		if (val.Length() >= 2 && val[0] == '!' && val[1] == ':')
			{
			val[0] = (TText)aDrive;
			}
		if (key.CompareF(_L("id")) == 0) aShell.iId.Copy(val.Left(aShell.iId.MaxLength()));
		else if (key.CompareF(_L("name")) == 0) aShell.iName.Copy(val.Left(aShell.iName.MaxLength()));
		else if (key.CompareF(_L("start")) == 0) aShell.iStart.Copy(val);
		else if (key.CompareF(_L("launcher")) == 0) aShell.iLauncher.Copy(val);
		else if (key.CompareF(_L("icon")) == 0) aShell.iIcon.Copy(val);
		}
	}

// the shells there are: S^3 first, then S^4 when the image carries it, then the installed ones (a package's own "symbian4"
// is dropped when the image has the built-in one). aFs: a connected session.
inline TInt PheListShells(RFs& aFs, RArray<TPheShell>& aShells)
	{
	TPheShell s3;
	s3.iId.Copy(KPheShellIdS3);
	s3.iName.Copy(_L("Symbian^3"));
	s3.iBuiltin = ETrue;
	aShells.Append(s3);
	if (PheShellsInRom() & KPheShellsS4)
		{
		TPheShell s4;
		s4.iId.Copy(KPheShellIdS4);
		s4.iName.Copy(_L("Symbian^4"));
		s4.iStart.Copy(_L("z:\resource\phe\s4start.txt"));
		s4.iLauncher.Copy(_L("z:\sys\bin\hsapplicationlauncher.exe"));
		s4.iBuiltin = ETrue;
		aShells.Append(s4);
		}
	TDriveList drives;
	if (aFs.DriveList(drives) != KErrNone)
		return KErrNone;
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
		TFindFile ff(aFs);
		CDir* list = NULL;
		if (aFs.GetDir(pattern, KEntryAttNormal, ESortByName, list) != KErrNone)
			continue;
		for (TInt i = 0; list && i < list->Count(); i++)
			{
			TFileName path(dir);
			path.Append((*list)[i].iName);
			RFile f;
			if (f.Open(aFs, path, EFileRead | EFileShareReadersOnly) != KErrNone)
				continue;
			TBuf8<1024> buf;
			f.Read(buf);
			f.Close();
			TPheShell s;
			s.iBuiltin = EFalse;
			PheShellParse(buf, letter, s);
			if (!s.iId.Length() || !s.iLauncher.Length())
				continue;
			if (!s.iName.Length())
				s.iName.Copy(s.iId);
			TBool dup = EFalse;
			for (TInt k = 0; k < aShells.Count(); k++)
				if (aShells[k].iId.CompareF(s.iId) == 0)
					dup = ETrue;
			if (!dup)
				aShells.Append(s);
			}
		delete list;
		}
	return KErrNone;
	}

// the selected shell's id from a repository (key 10, else key 1)
template <class TRepo>
inline void PheSelectedShellId(TRepo* aRepo, TDes& aId)
	{
	aId.Zero();
	if (aRepo)
		{
		TBuf<24> id;
		if (aRepo->Get(KPheDevKeyShellId, id) == KErrNone && id.Length())
			{
			aId.Copy(id);
			return;
			}
		TInt v = 0;
		aRepo->Get(KPheDevKeyShell, v);
		aId.Copy(v == KPheShellS4 ? KPheShellIdS4 : KPheShellIdS3);
		return;
		}
	aId.Copy(KPheShellIdS3);
	}

template <class TRepo>
inline void PheStoreShellId(TRepo* aRepo, const TDesC& aId)
	{
	aRepo->Set(KPheDevKeyShellId, aId);
	aRepo->Set(KPheDevKeyShell, aId.CompareF(KPheShellIdS4) == 0 ? KPheShellS4 : KPheShellS3);
	}

#endif
