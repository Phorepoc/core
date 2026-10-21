// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phetelaudio.cpp -- OURS (telephony T1): the telephony audio routing server host, with a policy that accepts every route.
//
// S60's call UI (the Phone engine's CPEAudioData, the CS call provider, CTSY) asks CTelephonyAudioRouting for the
// handset / loudspeaker route. The 2010 release has the whole client/server machinery (TelephonyAudioRoutingServer.dll,
// the sessions, CTelephonyAudioRoutingManager) but NOT the policy: that was the licensee's (a device adaptation owns
// the DSP route). This exe is the smallest policy there can be: it starts the server (CTelephonyAudioRoutingManager::NewL
// does) and says "ok, route changed" to every request. Without a server the Phone engine's construction fails
// (EPEMessagePEConstructionFailed: CTelephonyAudioRoutingProxy::ConstructL KErrNotFound) and no call ever shows.
// Stage T7 replaces the policy body with the real codec route (AIF2 / the 'MODM' path, telephony-plan.md section 6).
#include <e32base.h>
#include <e32debug.h>
#include <TelephonyAudioRouting.h>
#include <TelephonyAudioRoutingManager.h>
#include <MTelephonyAudioRoutingPolicyObserver.h>

class CAcceptPolicy : public CBase, public MTelephonyAudioRoutingPolicyObserver
	{
public:
	// every route is available and every request succeeds at once
	virtual void OutputChangeRequested(CTelephonyAudioRoutingManager& aManager, CTelephonyAudioRouting::TAudioOutput aOutput)
		{
		RDebug::Printf("[phetelaudio] route -> %d", (TInt)aOutput);
		aManager.OutputChanged(aOutput);
		aManager.OutputChangeRequestCompleted(aOutput, KErrNone);
		}
	};

static void RunL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);

	CAcceptPolicy* policy = new (ELeave) CAcceptPolicy;
	CleanupStack::PushL(policy);

	RArray<CTelephonyAudioRouting::TAudioOutput> outputs;
	CleanupClosePushL(outputs);
	User::LeaveIfError(outputs.Append(CTelephonyAudioRouting::EHandset));
	User::LeaveIfError(outputs.Append(CTelephonyAudioRouting::ELoudspeaker));

	CTelephonyAudioRoutingManager* manager = CTelephonyAudioRoutingManager::NewL(*policy, outputs.Array());
	CleanupStack::PushL(manager);
	manager->OutputChanged(CTelephonyAudioRouting::EHandset);

	RDebug::Printf("[phetelaudio] routing server up (handset + loudspeaker)");
	RProcess::Rendezvous(KErrNone);
	CActiveScheduler::Start();

	CleanupStack::PopAndDestroy(manager);
	CleanupStack::PopAndDestroy(&outputs);
	CleanupStack::PopAndDestroy(policy);
	CleanupStack::PopAndDestroy(sched);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, RunL());
	if (err != KErrNone)
		RDebug::Printf("[phetelaudio] FAILED: %d", err);
	delete cleanup;
	return err;
	}
