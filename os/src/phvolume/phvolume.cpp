// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phvolume.cpp -- OURS (audio V0, 3.3): the volume-key service.
//
// The KeyPublisherPlugin (a WSERV animation DLL, loaded by SysAp) CONSUMES the hardware volume keys and publishes them
// to Publish&Subscribe (category KPSUidMediaKeysEventNotifier, key KMediaKeysVolumeKeyEvent): up/down x clicked,
// pressed (auto-repeat while held, 600 ms then every 500 ms) and released. The RemCon chain that would hand them to
// an app (remconserver + MMKeyBearer + a TSP, step V1) is not in the ROM, so no app ever sees them -- games included.
// This daemon makes the keys DO something regardless: it steps a master volume (0..15, 3 dB per step, 0 = mute) and
// publishes it as KPhAudioMasterVolume (ph_audio_props.h); the userland mixer (phmixer.cpp, inside mdasoundadapter in
// the audio server) applies it as the HARDWARE volume of the real SoundSc channel, so it scales every stream at once.
//
//   up/down CLICKED and PRESSED (repeat) step the volume; RELEASED is ignored. The level is kept in the property only
//   (it resets to maximum on the next boot, as the sound driver does today).
#include <e32base.h>
#include <e32property.h>
#include <e32debug.h>
#include <f32file.h>
#include "ph_audio_props.h"

// MediaKeysInternalPSKeys.h / MMKeyBearerScanCodes.h (mw/appsupport), restated so this exe needs no MW headers
const TUid KPSUidMediaKeysEventNotifier = { 0x10207263 };
const TUint32 KMediaKeysVolumeKeyEvent = 0x00000001;
const TInt KPSVolumeDownReleased = -3;
const TInt KPSVolumeDownPressed = -2;
const TInt KPSVolumeDownClicked = -1;
const TInt KPSVolumeUpClicked = 1;
const TInt KPSVolumeUpPressed = 2;
const TInt KPSVolumeUpReleased = 3;

_LIT_SECURITY_POLICY_PASS(KReadAnyone);
_LIT_SECURITY_POLICY_C1(KWriteDeviceData, ECapabilityWriteDeviceData);

class CVolumeWatcher : public CActive
	{
public:
	static CVolumeWatcher* NewLC()
		{
		CVolumeWatcher* w = new (ELeave) CVolumeWatcher;
		CleanupStack::PushL(w);
		w->ConstructL();
		return w;
		}
	~CVolumeWatcher()	{ Cancel(); iKeys.Close(); iMaster.Close(); }
private:
	CVolumeWatcher() : CActive(CActive::EPriorityStandard), iStep(KPhVolDefaultStep)
		{
		CActiveScheduler::Add(this);
		}
	void ConstructL()
		{
		// the published master volume (the mixer reads it); another instance may have defined it already
		TInt r = RProperty::Define(KPhAudioPropertyCategory, KPhAudioMasterVolume, RProperty::EInt, KReadAnyone, KWriteDeviceData);
		// Define needs SID == category (this exe's own SID) or, for a low category, WriteDeviceData. A -46 here once killed the
		// service on one x86 ROM (not reproducible since): log what the kernel saw and carry on -- the mixer reads the key if it
		// exists, and an owner-less key only means volume stays at the sound driver's default.
		RDebug::Printf("[phvolume] sid %08x, Define -> %d", (TUint)RProcess().SecureId().iId, r);
		if (r != KErrNone && r != KErrAlreadyExists && r != KErrPermissionDenied)
			User::Leave(r);
		User::LeaveIfError(iMaster.Attach(KPhAudioPropertyCategory, KPhAudioMasterVolume));
		// a property defined just now reads 0 (= mute, boot 452): start it at the default; an existing one keeps its level
		if (r == KErrNone || iMaster.Get(iStep) != KErrNone)
			iStep = LoadStep();
		TInt sr = iMaster.Set(iStep);
		if (sr != KErrNone)
			RDebug::Printf("[phvolume] master volume Set -> %d", sr);
		// the key events (defined by KeyPublisherPlugin when SysAp loads it; Subscribe waits if that is not yet)
		User::LeaveIfError(iKeys.Attach(KPSUidMediaKeysEventNotifier, KMediaKeysVolumeKeyEvent));
		Listen();
		RDebug::Printf("[phvolume] up, step %d", iStep);
		}
	// the last step lives in this exe's private directory on C:, so the volume survives a reboot as on a phone; the default
	// (quiet) applies only when nothing valid is saved yet
	static TBool StepFile(RFs& aFs, TFileName& aName)
		{
		if (aFs.Connect() != KErrNone)
			return EFalse;
		aFs.CreatePrivatePath(EDriveC);
		aFs.PrivatePath(aName);
		aName.Insert(0, _L("C:"));
		aName.Append(_L("volume.dat"));
		return ETrue;
		}
	static TInt LoadStep()
		{
		TInt step = KPhVolDefaultStep;
		RFs fs;
		TFileName name;
		if (StepFile(fs, name))
			{
			RFile f;
			if (f.Open(fs, name, EFileRead) == KErrNone)
				{
				TBuf8<8> b;
				if (f.Read(b) == KErrNone && b.Length() >= 1 && b[0] <= KPhVolMaxStep)
					step = b[0];
				f.Close();
				}
			fs.Close();
			}
		RDebug::Printf("[phvolume] restored step %d", step);
		return step;
		}
	void SaveStep()
		{
		RFs fs;
		TFileName name;
		if (!StepFile(fs, name))
			return;
		RFile f;
		if (f.Replace(fs, name, EFileWrite) == KErrNone)
			{
			TBuf8<1> b;
			b.Append((TUint8)iStep);
			f.Write(b);
			f.Close();
			}
		fs.Close();
		}
	void Listen()
		{
		iKeys.Subscribe(iStatus);
		SetActive();
		}
	void RunL()
		{
		TInt err = iStatus.Int();
		if (err != KErrNone)
			{
			RDebug::Printf("[phvolume] subscribe error %d: retry in 1 s", err);
			User::After(1000000);
			Listen();
			return;
			}
		Listen();							// re-subscribe BEFORE reading so no event is lost
		TInt ev = 0;
		if (iKeys.Get(ev) != KErrNone)
			return;
		TInt step = iStep;
		if (ev == KPSVolumeUpClicked || ev == KPSVolumeUpPressed)
			step = iStep < KPhVolMaxStep ? iStep + 1 : KPhVolMaxStep;
		else if (ev == KPSVolumeDownClicked || ev == KPSVolumeDownPressed)
			step = iStep > 0 ? iStep - 1 : 0;
		if (step != iStep)
			{
			iStep = step;
			iMaster.Set(iStep);
			SaveStep();
			RDebug::Printf("[phvolume] event %d -> step %d (code %d)", ev, iStep, PhVolStepToCode(iStep));
			}
		}
	void DoCancel()
		{
		iKeys.Cancel();
		}
private:
	RProperty iKeys;
	RProperty iMaster;
	TInt iStep;
	};

static void RunL()
	{
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	CVolumeWatcher::NewLC();
	RProcess::Rendezvous(KErrNone);
	CActiveScheduler::Start();
	CleanupStack::PopAndDestroy(2, s);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (c)
		{
		TRAP(r, RunL());
		if (r != KErrNone)
			RDebug::Printf("[phvolume] left %d", r);
		delete c;
		}
	return r;
	}
