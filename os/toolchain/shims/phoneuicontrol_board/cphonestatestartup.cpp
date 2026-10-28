/*
* Copyright (c) 2005-2010 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description: Implementation of CPhoneStateStartup class.
*
*/


// INCLUDES
#include "cphonestatestartup.h"
#include "phonestatedefinitions.h"
#include "mphonestatemachine.h"
#include "tphonecmdparamKeycapture.h"
#include "tphonecmdparamboolean.h"
#include "phonelogger.h"
#include "phonerssbase.h"
#include "phoneviewcommanddefinitions.h"
#include "mphonesecuritymodeobserver.h"

// ================= MEMBER FUNCTIONS =======================

// C++ default constructor can NOT contain any code, that
// might leave.
//
EXPORT_C CPhoneStateStartup::CPhoneStateStartup( 
    MPhoneStateMachine* aStateMachine, 
    MPhoneViewCommandHandle* aViewCommandHandle,
    MPhoneCustomization* aPhoneCustomization
     ) : 
    CPhoneState( aStateMachine, aViewCommandHandle, aPhoneCustomization ),
    iPEReady( EFalse ),
    iPhoneReady( EFalse )
    {
    }

// -----------------------------------------------------------
// CPhoneStateStartup::~CPhoneStateStartup()
// Destructor
// (other items were commented in a header).
// -----------------------------------------------------------
//
EXPORT_C CPhoneStateStartup::~CPhoneStateStartup()
    {
    delete iCreateNote;
    }

// -----------------------------------------------------------
// CPhoneStateStartup::ConstructL()
// Constructor
// (other items were commented in a header).
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::ConstructL()
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::ConstructL() ");
    CPhoneState::BaseConstructL();
    
    // Set send key activation off.
    TPhoneCmdParamBoolean boolean;
    boolean.SetBoolean( EFalse );
    iViewCommandHandle->ExecuteCommandL( EPhoneViewSetSendKeyDialerActivationFlag, 
            &boolean );

    // CAPTURE KEY EVENTS PERMANENTLY
        
    // Capture the up and down events for the No key
    TPhoneCmdParamKeyCapture noKeyCaptureParam;
    noKeyCaptureParam.SetKey( EStdKeyNo );
    noKeyCaptureParam.SetKeyCode( EKeyNo );
    noKeyCaptureParam.SetCaptureType( EPhoneKeyAllEvents );
    iViewCommandHandle->ExecuteCommandL( EPhoneViewStartCapturingKey, 
        &noKeyCaptureParam );
          
    // Capture the up and down events for the EStdKeyEnd key
    TPhoneCmdParamKeyCapture endKeyCaptureParam;
    endKeyCaptureParam.SetKey( EStdKeyEnd );
    endKeyCaptureParam.SetKeyCode( EKeyEnd );
    endKeyCaptureParam.SetCaptureType( EPhoneKeyAllEvents );
    iViewCommandHandle->ExecuteCommandL( EPhoneViewStartCapturingKey, 
        &endKeyCaptureParam );
    }

// -----------------------------------------------------------
// CPhoneStateStartup::NewL()
// Constructor
// (other items were commented in a header).
// -----------------------------------------------------------
//
EXPORT_C CPhoneStateStartup* CPhoneStateStartup::NewL( 
    MPhoneStateMachine* aStateMachine, 
    MPhoneViewCommandHandle* aViewCommandHandle,
    MPhoneCustomization* aPhoneCustomization )
    {
    CPhoneStateStartup* self = new (ELeave) CPhoneStateStartup( 
        aStateMachine, aViewCommandHandle, aPhoneCustomization );
    
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    
    return self;
    }

// -----------------------------------------------------------
// CPhoneStateStartup::HandlePhoneEngineMessageL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandlePhoneEngineMessageL( 
    const TInt aMessage, 
    TInt aCallId )
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandlePhoneEngineMessageL() ");
    switch ( aMessage )
        {
        case MEngineMonitor::EPEMessagePEConstructionReady:
            HandlePEConstructionReadyL( aCallId );
            break;
            
        default:
            CPhoneState::HandlePhoneEngineMessageL( aMessage, aCallId );
            break;
        }
    }

// -----------------------------------------------------------
// CPhoneStateStartup::HandlePEConstructionReadyL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandlePEConstructionReadyL( TInt /*aCallId*/ )
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandlePEConstructionReadyL() ");
    // Indicate that phone engine construction is ready
    iPEReady = ETrue;

    // Only go to the idle state if phone is also ready
    if ( iPhoneReady )
        {
        InitializationReadyL();
        }
    }

// -----------------------------------------------------------
// CPhoneStateStartup::HandleKeyMessageL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandleKeyMessageL(
    TPhoneKeyEventMessages /*aMessage*/,
    TKeyCode aCode )
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandleKeyMessageL( ) ");

    // PHOREPOC delta (2026-10-15, INTERIM until the phone engine starts -- telephony-plan.md T1, a modem behind ETel):
    // the stock code showed its DEBUG note "qtn_debug_pe_failed_at_startup" for EVERY key while the phone engine is not
    // constructed -- on a device without a modem that is forever, so the red key only ever showed that note. Instead the
    // red key does what CPhoneStateIdle::HandleEndKeyPressL does without a call: the apps the red key closes are closed
    // and the home screen comes to the front (not while the keys or the device are locked). Other keys: nothing.
    if ( iPhoneReady && !iPEReady && aCode == EKeyNo && !IsAutoLockOn() && !IsKeyLockOn() )
        {
        iViewCommandHandle->ExecuteCommandL( EPhoneViewCancelAllNotications );
        iViewCommandHandle->ExecuteCommandL( EPhoneViewExitApplications );
        iViewCommandHandle->ExecuteCommandL( EPhoneViewBringIdleToForeground );
        }
    }

// -----------------------------------------------------------
// CPhoneStateStartup::HandleKeyEventL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandleKeyEventL(
        const TKeyEvent& /*aKeyEvent*/, TEventCode /*aEventCode*/ )
    {
    // Empty implementation
    }

// CPhoneStateStartup::HandleCreateNumberEntryL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandleCreateNumberEntryL( 
        const TKeyEvent& /* aKeyEvent */ ,
        TEventCode /* aEventCode */ )
    {
    // Empty implementation
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandleCreateNumberEntryL() ");   
    }

// -----------------------------------------------------------
// CPhoneStateStartup::HandleDtmfKeyToneL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandleDtmfKeyToneL( 
        const TKeyEvent& /* aKeyEvent */,
        TEventCode /* aEventCode */ )
    {
    // Empty implementation
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandleDtmfKeyToneL() ");
    }
    
// -----------------------------------------------------------
// CPhoneStateStartup::HandlePhoneStartupL
// -----------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandlePhoneStartupL()
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandlePhoneStartupL() ");
    // Indicate that the phone is ready
    iPhoneReady = ETrue;

    // Sim security status is available at this phase.
    iStateMachine->SecurityMode()->Initialize();
    if ( !IsSimOk() )
        {
        iCreateNote = CIdle::NewL( CActive::EPriorityHigh );
        CreateAndShowNoteAfterIdle(); 
        }
    
    // Only go to idle state when Phone engine is also ready
    if ( iPEReady )
        {
        InitializationReadyL();
        }
    }

// ---------------------------------------------------------
// CPhoneStateStartup::HandleIdleForegroundEventL
// Phone should show security note asap. 
// Active idle might be in front quite early if f.e. rejected SIM 
// ---------------------------------------------------------
//
EXPORT_C void CPhoneStateStartup::HandleIdleForegroundEventL()
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::HandleIdleForegroundEventL( ) ");
    // Security mode check. 
    if ( !iStateMachine->SecurityMode()->IsSecurityMode() && !IsSimOk() )
        {
        iCreateNote = CIdle::NewL( CActive::EPriorityHigh );
        CreateAndShowNoteAfterIdle(); 
        }
    }

// -----------------------------------------------------------------------------
// CPhoneStateStartup::CreateAndShowNoteAfterIdle
//
// Callback function.
// -----------------------------------------------------------------------------
//
void CPhoneStateStartup::CreateAndShowNoteAfterIdle()
    {
 __LOGMETHODSTARTEND( EPhoneControl, "CPhoneStateStartup::CreateAndShowNoteAfterIdle ");
        
    if ( !iCreateNote->IsActive() )
        {
        iCreateNote->Start( 
            TCallBack( DoShowNoteL, this ) );
        }
    }

// -----------------------------------------------------------------------------
// CPhoneStateStartup::DoShowNoteL
//
// Callback function.
// -----------------------------------------------------------------------------
//
TInt CPhoneStateStartup::DoShowNoteL( TAny* aAny )
    {        
    __LOGMETHODSTARTEND( EPhoneControl, "CPhoneStateStartup::DoShowNoteL ");
    static_cast< CPhoneStateStartup* >( aAny )->
        StartShowSecurityNoteL();

    return KErrNone;
    }

// -----------------------------------------------------------------------------
// CPhoneStateStartup::InitializationReadyL
//
// -----------------------------------------------------------------------------
//
void CPhoneStateStartup::InitializationReadyL()
    {
    __LOGMETHODSTARTEND(EPhoneControl, "CPhoneStateStartup::InitializationReady() ");
    // Go to idle state
    SetDefaultFlagsL();
    iCbaManager->UpdateCbaL( EPhoneEmptyCBA );
    iViewCommandHandle->ExecuteCommand( EPhoneViewPrepareIcons );
    iViewCommandHandle->ExecuteCommand( EPhoneViewLoadPlugins );
    iStateMachine->ChangeState( EPhoneStateIdle );   
    }


// End of File
