/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheshelltheme.h -- OURS: a shell may bring its own theme (skin). While that shell is chosen its theme is the active one; the theme that
// was active before is kept in the developer-menu repository and put back when the shell is left: by the user choosing another shell
// (the shell's own chooser calls these) or by pheshellsel's guard giving the shell up and falling back to S^3.
//   key 15 (KPheDevKeySavedThemeId):  the saved theme's package id as text (empty = nothing saved)
//   key 16 (KPheDevKeySavedThemeLoc): its location (EAknsSrvPhone / EAknsSrvMMC)
// Activating a theme is what the Themes application does: the skin server's definition sets (RAknsSrvSession::SetAllDefinitionSets), then the
// Personalisation repository's active-skin keys. Links aknskinsrv.lib and centralrepository.lib.
#ifndef PHESHELLTHEME_H
#define PHESHELLTHEME_H

#include <e32base.h>
#include <e32debug.h>
#include <centralrepository.h>
#include <aknssrvclient.h>
#include <aknspkgid.h>
#include <aknssrvskininformationpkg.h>
#include <aknskinsinternalcrkeys.h>
#include "phedevmenu.h"

const TUint32 KPheDevKeySavedThemeId = 15;
const TUint32 KPheDevKeySavedThemeLoc = 16;

// make aPid the active theme
inline TInt PheActivateThemeL(const TAknsPkgID& aPid, TInt aLocation)
	{
	RAknsSrvSession srv;
	CleanupClosePushL(srv);
	User::LeaveIfError(srv.Connect());
	TInt r = srv.SetAllDefinitionSets(aPid);
	if (r == KErrNone)
		{
		TAknsPkgIDBuf buf;
		aPid.CopyToDes(buf);
		CRepository* skins = CRepository::NewLC(KCRUidPersonalisation);
		skins->Set(KPslnActiveSkinUid, buf);
		skins->Set(KPslnActiveSkinLocation, aLocation);
		CleanupStack::PopAndDestroy(skins);
		}
	CleanupStack::PopAndDestroy(&srv);
	return r;
	}

// the installed theme named aName: its package id and location
inline TInt PheFindThemeL(const TDesC& aName, TAknsPkgID& aPid, TInt& aLocation)
	{
	RAknsSrvSession srv;
	CleanupClosePushL(srv);
	User::LeaveIfError(srv.Connect());
	CArrayPtr<CAknsSrvSkinInformationPkg>* skins = srv.EnumerateSkinPackagesL(EAknsSrvAll);
	TInt r = KErrNotFound;
	for (TInt i = 0; i < skins->Count(); i++)
		if (skins->At(i)->Name().CompareF(aName) == 0)
			{
			aPid = skins->At(i)->PID();
			TBuf<1> d;
			d.CopyUC(skins->At(i)->IniFileDirectory().Left(1));
			aLocation = d.CompareF(_L("e")) == 0 ? EAknsSrvMMC : EAknsSrvPhone;
			r = KErrNone;
			break;
			}
	skins->ResetAndDestroy();
	delete skins;
	CleanupStack::PopAndDestroy(&srv);
	return r;
	}

// the shell is chosen: keep the active theme once (not when the shell's theme is already active, not over one already saved) and activate the shell's
inline void PheThemeEnterShellL(const TDesC& aThemeName)
	{
	CRepository* dev = CRepository::NewLC(KPheDevMenuRepoUid);
	TAknsPkgID mine;
	TInt mineLoc = 0;
	User::LeaveIfError(PheFindThemeL(aThemeName, mine, mineLoc));
	TBuf<64> saved;
	const TInt have = dev->Get(KPheDevKeySavedThemeId, saved);
	TAknsPkgIDBuf cur;
	TInt curLoc = 0;
	CRepository* skins = CRepository::NewLC(KCRUidPersonalisation);
	skins->Get(KPslnActiveSkinUid, cur);
	skins->Get(KPslnActiveSkinLocation, curLoc);
	CleanupStack::PopAndDestroy(skins);
	TAknsPkgIDBuf mineBuf;
	mine.CopyToDes(mineBuf);
	if ((have != KErrNone || saved.Length() == 0) && cur.CompareF(mineBuf) != 0)
		{
		TInt r = dev->Set(KPheDevKeySavedThemeId, cur);
		if (r == KErrNotFound)
			dev->Create(KPheDevKeySavedThemeId, cur);
		r = dev->Set(KPheDevKeySavedThemeLoc, curLoc);
		if (r == KErrNotFound)
			dev->Create(KPheDevKeySavedThemeLoc, curLoc);
		RDebug::Print(_L("PH: [shelltheme] theme %S saved"), &cur);
		}
	const TInt r = PheActivateThemeL(mine, mineLoc);
	RDebug::Printf("PH: [shelltheme] %d: shell theme active", r);
	CleanupStack::PopAndDestroy(dev);
	}

// the shell is left (chosen away from, or given up by the guard): the saved theme comes back. Nothing saved = nothing to do; if the skin
// server is not reachable yet the saved theme stays saved for the next try.
inline void PheThemeLeaveShellL()
	{
	CRepository* dev = CRepository::NewLC(KPheDevMenuRepoUid);
	TBuf<64> saved;
	if (dev->Get(KPheDevKeySavedThemeId, saved) == KErrNone && saved.Length())
		{
		TAknsPkgID pid;
		pid.SetFromDesL(saved);
		TInt loc = 0;
		dev->Get(KPheDevKeySavedThemeLoc, loc);
		const TInt r = PheActivateThemeL(pid, loc);
		RDebug::Print(_L("PH: [shelltheme] theme %S restored: %d"), &saved, r);
		if (r == KErrNone)
			dev->Set(KPheDevKeySavedThemeId, KNullDesC);
		}
	else
		RDebug::Printf("PH: [shelltheme] no saved theme to restore");
	CleanupStack::PopAndDestroy(dev);
	}

#endif
