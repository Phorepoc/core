// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// alfdecoderserverclient_null.cpp — OURS (not Symbian source): a null ALF effect observer with the
// REAL alfdecoderserverclientu.def ordinal table, built from the real mmp (nosrc).
//
// RCL_3 eikcoctl (AknClearer.cpp) registers the screen clearer as ALF's "distraction window".
// No ALF server here -> the observer is an empty object. Everything else stays a loud unimpl stub.
#include <alf/alfcompositionutility.h>

CAlfEffectObserver::CAlfEffectObserver()
	{
	}

EXPORT_C CAlfEffectObserver* CAlfEffectObserver::NewL()
	{
	return new (ELeave) CAlfEffectObserver;
	}

EXPORT_C CAlfEffectObserver::~CAlfEffectObserver()
	{
	}

EXPORT_C TInt CAlfEffectObserver::ActiveEffectsCount()
	{
	return 0;
	}

EXPORT_C void CAlfEffectObserver::SubscribeCallbackL(MAlfEffectObserver*, TInt, TInt)
	{
	}

EXPORT_C void CAlfEffectObserver::SetDistractionWindow(const RWindowTreeNode&, TInt)
	{
	}
