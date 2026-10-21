/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// ph_audio_props.h -- OURS (audio V0, 3.3): the Publish&Subscribe contract between the
// volume-key service (src/phvolume, the writer) and the userland mixer (phmixer.cpp inside mdasoundadapter.dll, the
// reader/applier). Plain constants, no includes beyond e32std.h, so both sides take it unmodified.
#ifndef PH_AUDIO_PROPS_H
#define PH_AUDIO_PROPS_H

#include <e32std.h>

// category = the volume service's own SID (src/phvolume.mmp UID3 0xE70157A1): only a process whose SID IS the category
// may define a property in it (was 0xE70157A2 -> RProperty::Define = KErrPermissionDenied, phvolume left -46, boot 451)
const TUid KPhAudioPropertyCategory = { (TInt32)0xE70157A1 };
// master volume STEP, EInt, 0..KPhVolMaxStep (0 = mute). Defined and written by phvolume.exe; read by the mixer.
const TUint KPhAudioMasterVolume = 1;

const TInt KPhVolMaxStep = 15;
const TInt KPhVolDefaultStep = 15;		// = today's behaviour (hardware volume at its maximum)

// step -> soundsc volume code (255 = full, 0.5 dB per code; 3 dB per step, step 1 = -42 dB, 0 = mute)
inline TInt PhVolStepToCode(TInt aStep)
	{
	if (aStep <= 0)
		return 0;
	if (aStep >= KPhVolMaxStep)
		return 255;
	return 255 - (KPhVolMaxStep - aStep) * 6;
	}

#endif
