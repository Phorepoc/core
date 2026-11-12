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
// shape. Which of Normal/Alternate that is follows from the display's own shape (HAL EDisplayXPixels/YPixels): on a portrait
// display (the phones, the PC image's 360x640) the normal mode is portrait (wsini mode 1, hardware states 0 and 1).
#ifndef PHEORIENT_H
#define PHEORIENT_H

#include <e32base.h>
#include <e32property.h>
#include <hal.h>
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

// make the preferred orientation match aWanted (1/2; 0 changes nothing). KErrNone, KErrNotReady while AknCapServer has not
// defined the property yet, or the error of the call
inline TInt PheApplyOrientation(TInt aWanted)
	{
	if (aWanted != EPheOrientPortrait && aWanted != EPheOrientLandscape)
		return KErrNone;
	TInt pref = KPhePreferredNormal;
	TInt r = RProperty::Get(KPheUikPSCategory, KPheUikPreferredOrientation, pref);
	if (r != KErrNone)
		return r == KErrNotFound ? KErrNotReady : r;
	TInt w = 0, h = 0;
	HAL::Get(HALData::EDisplayXPixels, w);
	HAL::Get(HALData::EDisplayYPixels, h);
	const TBool displayPortrait = w <= h;
	const TBool wantAlternate = (aWanted == EPheOrientLandscape) == displayPortrait;
	const TBool isAlternate = pref == KPhePreferredAlternate;
	if (wantAlternate == isAlternate)
		return KErrNone;
	RAknUiServer ui;
	r = ui.Connect();
	if (r == KErrNone)
		{
		r = ui.RotateScreen();
		ui.Close();
		}
	return r;
	}

#endif
