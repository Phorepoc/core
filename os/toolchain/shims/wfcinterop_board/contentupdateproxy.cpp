// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description:
// contentupdateproxy.cpp
// Proxy object for receiving SUS surface content update notifications and rerouting them to nativestream 
//


#include <e32def.h>
#include <graphics/suerror.h>
#include "streammap.h"
#include "surfacestream.h"
#include "contentupdateproxy.h"
#include <e32atomics.h>

// PH (composition, real damage step 1): the 2010 CSurfaceStream drops the update region. Keep it here per surface: the
// bounding box of every region since the compositor last took it (no region = the whole surface). The compositor
// (libwfc phgpucompose) takes it per element with PhStreamTakeDamage. A spin lock: both sides run in WSERV's process.
namespace {
const TInt KPhDamageSlots = 32;
struct TPhDamage { TSurfaceId iId; TInt iX0, iY0, iX1, iY1; TBool iFull; TBool iUsed; };
TPhDamage gPhDamage[KPhDamageSlots];
volatile TUint32 gPhDamageLock;
void PhDamageLock() { while (__e32_atomic_swp_acq32(&gPhDamageLock, 1)) {} }
void PhDamageUnlock() { __e32_atomic_store_rel32(&gPhDamageLock, 0); }
TPhDamage* PhDamageSlot(const TSurfaceId& aId, TBool aCreate)
	{
	TPhDamage* freeSlot = NULL;
	for (TInt i = 0; i < KPhDamageSlots; i++)
		{
		if (gPhDamage[i].iUsed && gPhDamage[i].iId == aId)
			return &gPhDamage[i];
		if (!gPhDamage[i].iUsed && !freeSlot)
			freeSlot = &gPhDamage[i];
		}
	if (!aCreate)
		return NULL;
	if (!freeSlot)
		freeSlot = &gPhDamage[(aId.iInternal[0] ^ aId.iInternal[1]) % KPhDamageSlots];	// full: reuse one (its owner gets a full frame)
	freeSlot->iId = aId; freeSlot->iUsed = ETrue; freeSlot->iFull = EFalse;
	freeSlot->iX0 = freeSlot->iY0 = KMaxTInt; freeSlot->iX1 = freeSlot->iY1 = KMinTInt;
	return freeSlot;
	}
void PhDamageAdd(const TSurfaceId& aId, const TRegion* aRegion)
	{
	PhDamageLock();
	TPhDamage* d = PhDamageSlot(aId, ETrue);
	static TInt logged;
	if (logged < 6)	// (round 42o) what the window server posts: the surface and its region's box, the first few
		{
		++logged;
		const TRect b = aRegion && !aRegion->CheckError() ? aRegion->BoundingRect() : TRect();
		RDebug::Printf("PH: [dmg] post %08x %08x: region %s %d,%d-%d,%d", aId.iInternal[0], aId.iInternal[1],
			!aRegion ? "NULL" : aRegion->CheckError() ? "error" : aRegion->IsEmpty() ? "empty" : "box",
			b.iTl.iX, b.iTl.iY, b.iBr.iX, b.iBr.iY);
		}
	if (!aRegion || aRegion->CheckError() || aRegion->IsEmpty())
		d->iFull = ETrue;
	else
		{
		const TRect r = aRegion->BoundingRect();
		d->iX0 = Min(d->iX0, r.iTl.iX); d->iY0 = Min(d->iY0, r.iTl.iY);
		d->iX1 = Max(d->iX1, r.iBr.iX); d->iY1 = Max(d->iY1, r.iBr.iY);
		}
	PhDamageUnlock();
	}
}

// the damage of a surface since the last call, then cleared: 0 = none, 1 = aBox[4] (x0 y0 x1 y1, end exclusive),
// 2 = the whole surface (no region given, or a surface never seen: assume all of it)
extern "C" EXPORT_C int PhStreamTakeDamage(const TSurfaceId* aId, int* aBox)
	{
	if (!aId)
		return 2;
	PhDamageLock();
	TPhDamage* d = PhDamageSlot(*aId, EFalse);
	int ret = 3;	// (round 42o) 3 = a surface never posted (told apart from 2 = posted without a region)
	if (!d)
		{
		static TInt logged;
		if (logged < 4)
			{
			++logged;
			RDebug::Printf("PH: [dmg] take %08x %08x: never posted", aId->iInternal[0], aId->iInternal[1]);
			}
		}
	if (d)
		{
		if (d->iFull)
			ret = 2;
		else if (d->iX0 >= d->iX1 || d->iY0 >= d->iY1)
			ret = 0;
		else
			{
			aBox[0] = d->iX0; aBox[1] = d->iY0; aBox[2] = d->iX1; aBox[3] = d->iY1;
			ret = 1;
			}
		d->iFull = EFalse;
		d->iX0 = d->iY0 = KMaxTInt; d->iX1 = d->iY1 = KMinTInt;
		}
	PhDamageUnlock();
	return ret;
	}

CExtensionContainer* CContentUpdateProxy::NewL(TInt aScreenNum, COpenWfcStreamMap*	aSingleton,TInt aInternalVersion,TInt aPriority)
	{
	CContentUpdateProxy* pInstance = new(ELeave) CContentUpdateProxy();
	CleanupStack::PushL(pInstance);
	pInstance->ConstructL(aScreenNum,aSingleton,aInternalVersion,aPriority);
	CleanupStack::Pop(pInstance);
	return pInstance;
	}
void CContentUpdateProxy::ConstructL(TInt aScreenNum, COpenWfcStreamMap*	aSingleton,TInt aInternalVersion,TInt aPriority)
	{
	iScreenNum=aScreenNum;
	iSingleton=aSingleton;
	iInternalVersion=aInternalVersion;
	iPriority=aPriority;
	}
CContentUpdateProxy::CContentUpdateProxy():
iHeap(User::Heap())
	{
	}
CContentUpdateProxy::~CContentUpdateProxy()
	{
	
	}

TInt CContentUpdateProxy::Extension_(TUint aExtensionId, TAny*& aRetPtr, TAny* a1)
	{
	switch (aExtensionId)
		{
		case MCompositionSurfaceUpdate::ETypeId:
			{
			aRetPtr=static_cast<MCompositionSurfaceUpdate*>(this);
			return KErrNone;
			}
		default:
			{
			return CExtensionContainer::Extension_(aExtensionId,aRetPtr,a1);
			}
		}
	}

TInt CContentUpdateProxy::ApiVersion()
	{
	return KContentUpdateProxyVersion;
	}

TVersion CContentUpdateProxy::InternalVersion()
	{
	return TVersion(iInternalVersion>>24,(iInternalVersion>>16)&0xff,iInternalVersion&0xffff);
	}

void CContentUpdateProxy::ContentUpdated      
	(	const TSurfaceId&  aSurface,     		TInt		aBuffer,
		const TRegion*     aRegion,
		TRequestStatus*    aStatusConsumed,
		TRequestStatus*    aStatusDisplayed,	TUint32*	aTimeStamp,
		TRequestStatus*    aStatusDispXTimes,	TInt*		aDisplayedXTimes)
	{
    NFLOG(("### CContentUpdateProxy srf(0x%x:0x%x:0x%x:0x%x),"
            "\n\t\t buff(%d),\n\t\t rg(0x%x),\n\t\t rsConsumed(0x%x),\n\t\t rsDsp(0x%x),\n\t\t ts(0x%x),"
            "\n\t\t rsDXT(0x%x),\n\t\t dXT(%d)", 
            aSurface.iInternal[0], aSurface.iInternal[1], aSurface.iInternal[2], aSurface.iInternal[3],
            aBuffer, aRegion, aStatusConsumed, aStatusDisplayed, aTimeStamp, aStatusDispXTimes, aDisplayedXTimes ? *aDisplayedXTimes: 0));
    
    COpenWfcStreamMap* stream = NULL;
    PhDamageAdd(aSurface, aRegion);	// PH: real damage step 1
    TRAPD(err,stream = &COpenWfcStreamMap::InstanceL());
    if (err != KErrNone)
        {
        NFLOG(("### ERROR - EARLY EXIT * CContentUpdateProxy::ContentUpdated()"));
        // cancel notifications
        CompleteStatus(
                aStatusConsumed,
                aStatusDisplayed,
                aStatusDispXTimes,
                err);
        return;
        }
    RHeap* oldHeap = NULL;
	if (&iHeap != &(User::Heap()))
	    {
	    oldHeap = User::SwitchHeap(stream->GetMainHeap());
	    }
	CSurfaceStream* ns = NULL;
	ns = stream->Find(aSurface);

	if (ns)
		{
		RegisterNotifications(*ns,
                              aBuffer,
                              aStatusDisplayed, aTimeStamp,
                              aStatusDispXTimes, aDisplayedXTimes,
                              aStatusConsumed, aRegion);
		
		ns->ReleaseReference();
		}
	else
	    {
	    // PHOREPOC trace (2026-10-15, DSA under NGA: boot 492 EndUpdate -> KErrSurfaceNotRegistered)
	    RDebug::Printf("PH: wfci update for an unregistered surface %08x %08x %08x %08x",
	            (TUint)aSurface.iInternal[0], (TUint)aSurface.iInternal[1], (TUint)aSurface.iInternal[2], (TUint)aSurface.iInternal[3]);
	    // cancel notifications
	    CompleteStatus(aStatusConsumed,
	            aStatusDisplayed,
	            aStatusDispXTimes,
	            KErrSurfaceNotRegistered);
	    }
	if (oldHeap)
	    {
	    (void)User::SwitchHeap(oldHeap);
	    }
    NFLOG(("### EXIT * CContentUpdateProxy::ContentUpdated()"));
	}

void CContentUpdateProxy::CompleteStatus(
        TRequestStatus*    aStatusConsumed,
        TRequestStatus*    aStatusDisplayed,
        TRequestStatus*    aStatusDispXTimes,
        TInt aReason)
    {
    if (aStatusConsumed)
        {
        User::RequestComplete(aStatusConsumed, aReason);
        }
    if (aStatusDisplayed)
        {
        User::RequestComplete(aStatusDisplayed, aReason);
        }
    if (aStatusDispXTimes)
        {
        User::RequestComplete(aStatusDispXTimes, aReason);
        }
    }

void CContentUpdateProxy::RegisterNotifications(CSurfaceStream& aNs,
                                                 TInt            aBuffer,
                                                 TRequestStatus* aStatusDisplayed, TUint32* aTimeStamp,
                                                 TRequestStatus* aStatusDispXTimes, TInt* aDisplayedXTimes,
                                                 TRequestStatus* aStatusConsumed, const TRegion* aRegion)
    {
    if (KErrNone != aNs.CheckBufferNumber(aBuffer, aStatusDisplayed, aStatusDispXTimes, aStatusConsumed))
        {
        return;
        }
    
    TInt updateFlag = 0;
    if (aStatusConsumed)
        {
        *aStatusConsumed = KRequestPending;
        updateFlag |= ESOWF_EventAvailable;
        }
    if (aStatusDisplayed)
        {
        *aStatusDisplayed = KRequestPending;
        updateFlag |= ESOWF_EventDisplayed;
        }
    if (aStatusDispXTimes)
        {
        *aStatusDispXTimes = KRequestPending;
        updateFlag |= ESOWF_EventDisplayedX;
        }
    
    aNs.SetNewNotifications(aBuffer, 
                            aStatusDisplayed, 
                            aTimeStamp, 
                            aStatusDispXTimes, 
                            aDisplayedXTimes, 
                            aStatusConsumed, 
                            aRegion,
                            iScreenNum);
    }

