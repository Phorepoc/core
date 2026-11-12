/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// pheorient.h -- OURS: the screen orientation the user wants when no sensor turns the screen. Shared by the Control Panel
// plug-in (gsphorient) and the boot (pheshellsel).
//
// Nothing new is invented for the turning itself: S60's own mechanism is used (classicui, 2010 source):
//  - Avkon's layout config pairs each hardware state (wsini S60_HWSTATE_*) with a screen mode and an ALTERNATE screen mode;
//  - the preferred orientation (P&S KPSUidUikon / KUikPreferredOrientation: Normal | Alternate, defined by AknCapServer,
//    CEikSgcServer::ConstructL) picks which of the two an application that asks for no orientation gets
//    (CAknSgcClient::DoCalculateScreenMode, aknsgcc.cpp);
//  - RAknUiServer::RotateScreen (EAknSRotateScreen -> CEikSgcServer::RotateScreenL) toggles that preference and makes the
//    cap server re-apply the screen mode: the same call a "rotate" key uses. An application that asks for an orientation
//    (SetOrientationL) still gets it.
// What is ours: the stored choice (the developer-menu repository, key 13) and applying it at boot and on change.
// 0 = Automatic: the sensor (or, without one, the hardware state) decides -- nothing is changed;
// 1 = Portrait, 2 = Landscape: the preference is set so that the screen mode of an application that asks for nothing has that
// shape (which of Normal/Alternate that is: PheApplyOrientation).
#ifndef PHEORIENT_H
#define PHEORIENT_H

#include <e32base.h>
#include <e32property.h>
#include <w32std.h>
#include <centralrepository.h>
#include <akncapserverclient.h>
#include "phedevmenu.h"

// key 13: the orientation choice, 0 Automatic | 1 Portrait | 2 Landscape (absent = 0)
const TUint32 KPheDevKeyOrientation = 13;
enum TPheOrientation { EPheOrientAuto = 0, EPheOrientPortrait = 1, EPheOrientLandscape = 2 };

// UikonInternalPSKeys.h (classicui uikon_status_api): the category and key, and its two values
const TUid KPheUikPSCategory = { 0x101F8773 };		// KPSUidUikon
const TUint32 KPheUikPreferredOrientation = 0x00000009;
const TInt KPhePreferredNormal = 0, KPhePreferredAlternate = 1;

inline TInt PheStoredOrientation()
	{
	TInt v = EPheOrientAuto;
	CRepository* repo = NULL;
	TRAPD(err, repo = CRepository::NewL(KPheDevMenuRepoUid));
	if (err == KErrNone)
		{
		if (repo->Get(KPheDevKeyOrientation, v) != KErrNone)
			v = EPheOrientAuto;
		delete repo;
		}
	return v < EPheOrientAuto || v > EPheOrientLandscape ? EPheOrientAuto : v;
	}

// is the window server's current screen mode landscape (wider than tall)? KErrNone or the error
inline TInt PheCurrentModeLandscape(RWsSession& aWs, TBool& aLandscape)
	{
	CWsScreenDevice* dev = new CWsScreenDevice(aWs);
	if (!dev)
		return KErrNoMemory;
	TInt r = dev->Construct();
	if (r == KErrNone)
		{
		TPixelsTwipsAndRotation m;
		dev->GetScreenModeSizeAndRotation(dev->CurrentScreenMode(), m);
		aLandscape = m.iPixelSize.iWidth > m.iPixelSize.iHeight;
		}
	delete dev;
	return r;
	}

// make the preferred orientation give aWanted (1/2; 0 changes nothing) to an application that asks for none. Which of
// Normal/Alternate that is depends on the hardware state's modes (the PC's normal mode is landscape, a phone's portrait), so
// the current screen mode is looked at: the wrong shape -> RotateScreen. If the shape does not change (the application on top
// fixed its orientation), the preference is toggled back. KErrNone, KErrNotReady while AknCapServer has not defined the
// property yet, KErrLocked when the top application holds the other orientation, or the error of a call
inline TInt PheApplyOrientation(TInt aWanted)
	{
	if (aWanted != EPheOrientPortrait && aWanted != EPheOrientLandscape)
		return KErrNone;
	TInt pref = 0;
	TInt r = RProperty::Get(KPheUikPSCategory, KPheUikPreferredOrientation, pref);
	if (r != KErrNone)
		return r == KErrNotFound ? KErrNotReady : r;
	RWsSession ws;
	r = ws.Connect();
	if (r != KErrNone)
		return r;
	TBool land = EFalse;
	r = PheCurrentModeLandscape(ws, land);
	const TBool wantLand = aWanted == EPheOrientLandscape;
	if (r == KErrNone && land != wantLand)
		{
		RAknUiServer ui;
		r = ui.Connect();
		if (r == KErrNone)
			{
			r = ui.RotateScreen();
			TBool now = land;
			if (r == KErrNone && PheCurrentModeLandscape(ws, now) == KErrNone && now != wantLand)
				{
				ui.RotateScreen();			// the top application holds its orientation: leave the preference as it was
				r = KErrLocked;
				}
			ui.Close();
			}
		}
	ws.Close();
	return r;
	}

#endif
