// OURS (PinePhone, 2026-10-28): Symbian's stub vibra plug-in (sf.adapt.stubs hwrmstubplugins/src/VibraPlugin.cpp,
// which acknowledged every command and moved nothing) driving the motor through the board's power HAL function
// 0x5056 (a1 = on/off; adaptation-a64 a64_glue.cpp A64Vibra, PD2). Start = on until HWRM's own duration timer sends
// Stop; Pulse = on for TVibraData::iDuration ms. Silent (KErrNotSupported, ignored) on boards without a motor.
/*
* Copyright (c) 2002-2005 Nokia Corporation and/or its subsidiary(-ies). 
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the Eclipse Public License v2.0, available at
* https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
* section 7 of the Eclipse Public License v1.0).
* This Source Code may also be made available under the following Secondary
* Licenses when the conditions for such availability set forth in the Eclipse
* Public License, v. 2.0 are satisfied: GNU General Public License, version 2
* or later.
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
* The Phorepoc Authors
*
* Description:  Hardware Resource Manager stub plugins vibra plugin 
*                implementation.
*
*/


#include "VibraPlugin.h"
#include "PluginTimer.h"
#include <hwrmvibracommands.h>
#include <e32svr.h>
#include <u32std.h>
#include "Trace.h" 

const TInt KA64PowerHalVibra = 0x5056;
const TInt KPulseEndCmdId = -1;		// an internal timer: the end of a pulse (no response is sent for it)

static void Motor(TBool aOn)
    {
    UserSvr::HalFunction(EHalGroupPower, KA64PowerHalVibra, (TAny*)(TInt)aOn, 0);
    }

CVibraPlugin* CVibraPlugin::NewL()
    {
    CVibraPlugin* self = new(ELeave) CVibraPlugin();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
    }

CVibraPlugin::~CVibraPlugin()
    {
    Motor(EFalse);
    delete iPulseEnd;
    iTimers.ResetAndDestroy();
    }

CVibraPlugin::CVibraPlugin()
    {
    }

void CVibraPlugin::ConstructL()
    {
    }

void CVibraPlugin::ProcessCommandL( const TInt aCommandId,
                                      const TUint8 aTransId,
                                      TDesC8& aData )
    {
    COMPONENT_TRACE((_L("HWRM VibraPlugin: Processing command: 0x%x, TransId: 0x%x"), aCommandId, aTransId));

    // any command ends a pulse still running
    delete iPulseEnd;
    iPulseEnd = NULL;

    TInt retval(KErrNone);
    switch ( aCommandId )
        {
        case HWRMVibraCommand::EStartVibraCmdId:
        case HWRMVibraCommand::EStartVibraWithDefaultSettingsCmdId:
            Motor(ETrue);
            break;
        case HWRMVibraCommand::EPulseVibraCmdId:
            {
            TUint ms = 50;
            if ( aData.Size() == sizeof(HWRMVibraCommand::TVibraData) )
                {
                HWRMVibraCommand::TPulseVibraCmdDataPackage pckg;
                pckg.Copy(aData);
                ms = pckg().iDuration;
                }
            Motor(ETrue);
            iPulseEnd = CPluginTimer::NewL(ms * 1000, iResponseCallback, KPulseEndCmdId, aTransId, KErrNone, this);
            }
            break;
        case HWRMVibraCommand::EStopVibraCmdId:
            Motor(EFalse);
            break;
        default:
            retval = KErrNotSupported;
            break;
        }

    // the response, as the stub sent it
    CPluginTimer* timer = CPluginTimer::NewL(500, iResponseCallback, aCommandId, aTransId, retval, this);
    CleanupStack::PushL(timer); 
    iTimers.AppendL(timer); 
    CleanupStack::Pop(timer);
    }

void CVibraPlugin::CancelCommandL( const TUint8 aTransId, 
                                     const TInt /*aCommandId*/)
    {
    for( TInt i = 0; i < iTimers.Count(); i++ )
        {
        if ( iTimers[i]->TransId() == aTransId )
            {
            delete iTimers[i];
            iTimers.Remove(i);
            break;
            }
        }
    }

void CVibraPlugin::GenericTimerFired(MHWRMPluginCallback* aService,
                  TInt aCommandId,
                 const TUint8 aTransId,
                 TInt aRetVal)
    {
    __ASSERT_ALWAYS(aService != NULL, User::Invariant() );

    if ( aCommandId == KPulseEndCmdId )
        {
        Motor(EFalse);
        delete iPulseEnd;    // (from its own RunL, which touches nothing after this call -- as the stub deletes iTimers)
        iPulseEnd = NULL;
        return;
        }
    else
        {
        HWRMVibraCommand::TErrorCodeResponsePackage retvalPackage(aRetVal);
        TRAPD(err, aService->ProcessResponseL(aCommandId, aTransId, retvalPackage));    
        if ( err != KErrNone )
            {
            COMPONENT_TRACE((_L("HWRM VibraPlugin: Error in ProcessResponseL: %d"), err));
            }
        }

    // delete obsolete timers
    for( TInt i = (iTimers.Count()-1); i > -1 ; i-- )
        {
        if ( !iTimers[i]->IsActive() )
            {
            delete iTimers[i];
            iTimers.Remove(i);    
            }
        }
    }
