/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheshellapps.h -- OURS: shell-tagged applications. The same function can exist twice (S^3's Messages and an Etesian or Orbit
// Messages); only the active shell's version is to be visible and the default handler. A package says which apps belong to which
// shell with plain text files next to the shell's description file (pheshells.h), in <drive>:\resource\phe\shells\, named after
// the shell id; '#' starts a comment:
//   <id>.apps     one application UID (hex) per line: apps that belong to shell <id>, shown only while <id> is the active shell
//   <id>.hides    UIDs hidden while <id> is the active shell (the stock apps it replaces)
//   <id>.assoc    "<data type> <uid hex>" per line: the handler of a MIME type / URL scheme (x-epoc-url/tel ...) while <id> is active
// PheApplyShellAppsL(fs, activeId) is called by pheshellsel after the shell is chosen, at every start:
//   * visibility: the UIDs of every other shell's .apps and of the active shell's .hides are put on the Uikon hidden-applications
//     list (P&S KPSUidUikon / KUikAppHiddenList = 0x10, pairs of 16-bit halves; the list the stock task switcher honours), and
//     the active shell's .apps UIDs are taken off it; entries other writers put there stay;
//   * handlers: for every type of every .assoc file the mapping is deleted from apparc, then the active shell's are inserted with
//     high priority (apparc keeps mappings across boots, so the previous shell's are removed whenever a different shell starts).
// Launchers that read the files themselves (Etesian Home, Orbit) need nothing else. Removing a package removes its files, so its
// entries vanish at the next start. Nothing here knows a shell by name.
#ifndef PHESHELLAPPS_H
#define PHESHELLAPPS_H

#include <e32base.h>
#include <e32debug.h>
#include <e32property.h>
#include <f32file.h>
#include <apgcli.h>
#include <apmstd.h>
#include "pheshells.h"

const TUid KPheUikonCategory = { 0x101F8773 };
const TUint32 KPheUikAppHiddenList = 0x10;
const TInt KPheMaxHiddenApps = 25;		// the property holds 25 UIDs (50 characters)
const TInt KPheMaxManagedApps = 64;

// one tag file's lines (8-bit text) -> aOut: the first hex word of each line
inline void PheTagUidsL(const TDesC8& aText, RArray<TUint32>& aOut)
	{
	TPtrC8 rest(aText);
	while (rest.Length())
		{
		const TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
		TLex8 lx(line);
		lx.SkipSpace();
		if (lx.Eos() || lx.Peek() == '#')
			continue;
		TUint32 u = 0;
		if (lx.Val(u, EHex) == KErrNone && u != 0)
			aOut.AppendL(u);
		}
	}

// the text of a file; a new heap buffer
inline HBufC8* PheReadFileL(RFs& aFs, const TDesC& aPath)
	{
	RFile f;
	User::LeaveIfError(f.Open(aFs, aPath, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	f.Size(size);
	HBufC8* text = HBufC8::NewLC(size);
	TPtr8 tp = text->Des();
	User::LeaveIfError(f.Read(tp));
	CleanupStack::Pop(text);
	CleanupStack::PopAndDestroy(&f);
	return text;
	}

// every <id><aExt> file of every drive's shells folder: aCallback(aId, text) for each
class MPheTagFile
	{
public:
	virtual void TagFileL(const TDesC& aShellId, const TDesC8& aText) = 0;
	};

inline void PheForEachTagFileL(RFs& aFs, const TDesC& aExt, MPheTagFile& aSink)
	{
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
		pattern.Append(_L("*"));
		pattern.Append(aExt);
		CDir* list = NULL;
		if (aFs.GetDir(pattern, KEntryAttNormal, ESortByName, list) != KErrNone)
			continue;
		CleanupStack::PushL(list);
		for (TInt i = 0; i < list->Count(); i++)
			{
			TFileName path(dir);
			path.Append((*list)[i].iName);
			HBufC8* text = NULL;
			TRAPD(e, text = PheReadFileL(aFs, path));
			if (e != KErrNone)
				continue;
			CleanupStack::PushL(text);
			TParsePtrC pp((*list)[i].iName);
			aSink.TagFileL(pp.Name(), *text);
			CleanupStack::PopAndDestroy(text);
			}
		CleanupStack::PopAndDestroy(list);
		}
	}

class TPheHiddenCollector : public MPheTagFile
	{
public:
	TPheHiddenCollector(const TDesC& aActive, RArray<TUint32>& aManaged, RArray<TUint32>& aHide)
		: iActive(aActive), iManaged(aManaged), iHide(aHide) {}
	void TagFileL(const TDesC& aShellId, const TDesC8& aText)
		{
		RArray<TUint32> u;
		CleanupClosePushL(u);
		PheTagUidsL(aText, u);
		const TBool active = aShellId.CompareF(iActive) == 0;
		for (TInt i = 0; i < u.Count(); i++)
			{
			iManaged.AppendL(u[i]);
			if (iHideMode ? active : !active)
				iHide.AppendL(u[i]);
			}
		CleanupStack::PopAndDestroy(&u);
		}
	TBool iHideMode = EFalse;		// .hides files: hide while active; .apps files: hide while NOT active
private:
	const TDesC& iActive;
	RArray<TUint32>& iManaged;
	RArray<TUint32>& iHide;
	};

class TPheAssocApplier : public MPheTagFile
	{
public:
	TPheAssocApplier(RApaLsSession& aLs, const TDesC& aActive, TBool aInsert) : iLs(aLs), iActive(aActive), iInsert(aInsert) {}
	void TagFileL(const TDesC& aShellId, const TDesC8& aText)
		{
		const TBool active = aShellId.CompareF(iActive) == 0;
		TPtrC8 rest(aText);
		while (rest.Length())
			{
			const TInt nl = rest.Locate('\n');
			TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
			rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
			TLex8 lx(line);
			lx.SkipSpace();
			if (lx.Eos() || lx.Peek() == '#')
				continue;
			lx.Mark();
			lx.SkipCharacters();
			TBuf8<KMaxDataTypeLength> type(lx.MarkedToken());
			lx.SkipSpace();
			TUint32 uid = 0;
			if (type.Length() == 0 || lx.Val(uid, EHex) != KErrNone || uid == 0)
				continue;
			TDataType dt(type);
			if (!iInsert)
				{
				iLs.DeleteDataMapping(dt);		// whichever shell set it last
				}
			else if (active)
				{
				const TInt r = iLs.InsertDataMapping(dt, KDataTypePriorityHigh, TUid::Uid(uid));
				RDebug::Printf("PH: [shellapps] %S handler %08x: %d", &aShellId, uid, r);
				}
			}
		}
private:
	RApaLsSession& iLs;
	const TDesC& iActive;
	TBool iInsert;
	};

inline void PheApplyShellAppsL(RFs& aFs, const TDesC& aActiveId)
	{
	// visibility
	RArray<TUint32> managed, hide;
	CleanupClosePushL(managed);
	CleanupClosePushL(hide);
	TPheHiddenCollector apps(aActiveId, managed, hide);
	PheForEachTagFileL(aFs, _L(".apps"), apps);
	TPheHiddenCollector hides(aActiveId, managed, hide);
	hides.iHideMode = ETrue;
	PheForEachTagFileL(aFs, _L(".hides"), hides);
	if (managed.Count() == 0)
		{
		RDebug::Print(_L("PH: [shellapps] no tag files for %S"), &aActiveId);
		}
	else
		{
		TBuf<2 * KPheMaxHiddenApps> cur;
		if (RProperty::Get(KPheUikonCategory, KPheUikAppHiddenList, cur) != KErrNone)
			cur.Zero();
		TBuf<2 * KPheMaxHiddenApps> next;
		// what other writers put there stays; what the tag files manage is recomputed
		for (TInt i = 0; i + 1 < cur.Length(); i += 2)
			{
			const TUint32 u = ((TUint32)cur[i] << 16) | cur[i + 1];
			if (u != 0 && managed.Find(u) == KErrNotFound && next.Length() + 2 <= next.MaxLength())
				{
				next.Append((TText)(u >> 16));
				next.Append((TText)(u & 0xFFFF));
				}
			}
		TInt hidden = 0;
		for (TInt i = 0; i < hide.Count(); i++)
			if (next.Length() + 2 <= next.MaxLength())
				{
				next.Append((TText)(hide[i] >> 16));
				next.Append((TText)(hide[i] & 0xFFFF));
				hidden++;
				}
		TInt r = RProperty::Set(KPheUikonCategory, KPheUikAppHiddenList, next);
		if (r == KErrNotFound)
			{
			RProperty::Define(KPheUikonCategory, KPheUikAppHiddenList, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass),
				TSecurityPolicy(ECapabilityWriteDeviceData), 2 * KPheMaxHiddenApps * 2);
			r = RProperty::Set(KPheUikonCategory, KPheUikAppHiddenList, next);
			}
		RDebug::Print(_L("PH: [shellapps] shell %S: %d tagged apps, %d hidden: %d"), &aActiveId, managed.Count(), hidden, r);
		}
	CleanupStack::PopAndDestroy(2, &managed);
	// handlers
	RApaLsSession ls;
	if (ls.Connect() == KErrNone)
		{
		CleanupClosePushL(ls);
		TPheAssocApplier remove(ls, aActiveId, EFalse);
		PheForEachTagFileL(aFs, _L(".assoc"), remove);
		TPheAssocApplier insert(ls, aActiveId, ETrue);
		PheForEachTagFileL(aFs, _L(".assoc"), insert);
		CleanupStack::PopAndDestroy(&ls);
		}
	}

#endif
