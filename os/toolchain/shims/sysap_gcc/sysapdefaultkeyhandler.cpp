/*
* Copyright (c) 2006-2010 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Default key event handling of System Application
*
*/


#include <e32property.h>
#include <eikon.hrh>
#include <coemain.h>
#include <aknkeylock.h>
#include <AknTaskList.h>
#include <apgtask.h>
#include <apgcli.h>
#include <apgwgnam.h>
#include <apacmdln.h>
#include <AiwPoCParameters.h>
#include <AiwServiceHandler.h>
#include <featmgr.h>
#include <coreapplicationuisdomainpskeys.h>
#include <PSVariables.h>
#include <startupdomainpskeys.h>
#include <hwrmdomainpskeys.h>
#include <u32hal.h>
#include <SysAp.rsg>
#include <centralrepository.h> //for CRepository
#include <settingsinternalcrkeys.h>
#include <keylockpolicyapi.h>
#include <ctsydomainpskeys.h>
#include <hwrmdomainpskeys.h>
#include "sysapdefaultkeyhandler.h"
#include "sysapcallback.h"
#include "SysAp.hrh"
#include "AknSgcc.h"


const TInt KModifierMask( 0 );

// ======== MEMBER FUNCTIONS ========

// ---------------------------------------------------------------------------
// CSysApDefaultKeyHandler::CSysApDefaultKeyHandler
// ---------------------------------------------------------------------------
//
CSysApDefaultKeyHandler::CSysApDefaultKeyHandler( MSysapCallback& aCallback ) :
                                                     iCallback(aCallback),
                                                     iCameraSupported(EFalse),
                                                     iCoverDisplaySupported(EFalse),
                                                     iCameraLongPress(EFalse),
                                                     iKeylock(NULL),
                                                     iCapturedEKeyCamera(0),
                                                     iCapturedEKeyTwistOpen(0),
                                                     iCapturedEKeyTwistClose(0),
                                                     iCapturedEKeyGripOpen(0),
                                                     iCapturedEKeyGripClose(0),
                                                     iCapturedEKeyPoC(0),
                                                     iAknUiServerConnected(EFalse)
    {
    }

// ---------------------------------------------------------------------------
// CSysApDefaultKeyHandler::ConstructL
// ---------------------------------------------------------------------------
//
void CSysApDefaultKeyHandler::ConstructL()
    {
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::ConstructL()") ) );
    
    RWindowGroup groupWin = CCoeEnv::Static()->RootWin();
    User::LeaveIfError ( iCapturedEKeyCamera = groupWin.CaptureKey( EKeyCamera, KModifierMask, KModifierMask ) );
    User::LeaveIfError ( iCapturedEKeyTwistOpen = groupWin.CaptureKey( EKeyTwistOpen, KModifierMask, KModifierMask ) );
    User::LeaveIfError ( iCapturedEKeyTwistClose = groupWin.CaptureKey( EKeyTwistClose, KModifierMask, KModifierMask ) );   
    User::LeaveIfError ( iCapturedEKeyGripOpen = groupWin.CaptureKey( EKeyGripOpen, KModifierMask, KModifierMask ) ); 
    User::LeaveIfError ( iCapturedEKeyGripClose = groupWin.CaptureKey( EKeyGripClose, KModifierMask, KModifierMask ) ); 
    User::LeaveIfError ( iCapturedEKeyPoC = groupWin.CaptureKey( EKeyPoC, KModifierMask, KModifierMask ) ); 
    
    // flip key events don't need to be captured, because in current setup they are not real events but simulated by SysAp
    // according to notifications received from P&S key that stores flip information
    
    FeatureManager::InitializeLibL();
    iCameraSupported = FeatureManager::FeatureSupported( KFeatureIdCamera );
    iCoverDisplaySupported = FeatureManager::FeatureSupported( KFeatureIdCoverDisplay );
    iCameraLongPress = FeatureManager::FeatureSupported(KFeatureIdFfCameraLongKeyPress);
    FeatureManager::UnInitializeLib();
    
    TKeyLockBuf lockBuf;
    iCallback.ExecQueryL( MSysapCallback::EGetKeylock, lockBuf );  
    iKeylock = lockBuf();
    
    iKeypadWasLocked = iKeylock->IsKeyLockEnabled();
    
    __ASSERT_DEBUG( iKeylock, User::Panic( _L("CSysApDefaultKeyHandler::ConstructL: iKeylock not initialized"), KErrBadHandle ) );
    
    if ( iCoverDisplaySupported ) // if flip status is monitored, set the initial flip status now
        {
        TInt flipValue;
        TInt err = RProperty::Get( KPSUidHWRM, KHWRMFlipStatus, flipValue );

        TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::ConstructL: flipValue=%d, err=%d"), flipValue, err ) );
        
        if ( err == KErrNone )
            {
            switch ( flipValue )
                {
                case EPSHWRMFlipOpen:
                    SetDisplayState( ETrue );
                    break;
                    
                case EPSHWRMFlipClosed:
                    SetDisplayState( EFalse );
                    break;
                    
                case EPSHWRMFlipStatusUninitialized:
                default:
                    // set display state only if the current flip position is known
                    break;
                }
            }
        }
    
    //Load keylock slide handling CR 
    TRAP_IGNORE( iSlideRepository = CRepository::NewL( KCRUidSlideSettings ) );
    iKeylockPolicy = CKeyLockPolicyApi::NewL( EPolicyActivateKeyguard );

    //Load PhoneCallStatus P&S  
    TInt err = iCallStateProperty.Attach(KPSUidCtsyCallInformation, KCTsyCallState);
    }

// ---------------------------------------------------------------------------
// CSysApDefaultKeyHandler::NewL
// ---------------------------------------------------------------------------
//
CSysApDefaultKeyHandler* CSysApDefaultKeyHandler::NewL( MSysapCallback& aCallback )
    {
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::NewL()") ) );
    
    CSysApDefaultKeyHandler* self = new( ELeave ) CSysApDefaultKeyHandler( aCallback) ;
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }


// ---------------------------------------------------------------------------
// CSysApDefaultKeyHandler::~CSysApDefaultKeyHandler
// ---------------------------------------------------------------------------
//
CSysApDefaultKeyHandler::~CSysApDefaultKeyHandler()
    {
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::~CSysApDefaultKeyHandler()") ) );
    
    iAknUiServer.Close();
    
    RWindowGroup groupWin = CCoeEnv::Static()->RootWin();
    groupWin.CancelCaptureKey( iCapturedEKeyCamera );
    groupWin.CancelCaptureKey( iCapturedEKeyTwistOpen );
    groupWin.CancelCaptureKey( iCapturedEKeyTwistClose );
    groupWin.CancelCaptureKey( iCapturedEKeyGripOpen );
    groupWin.CancelCaptureKey( iCapturedEKeyGripClose );
    groupWin.CancelCaptureKey( iCapturedEKeyPoC );
    delete iSlideRepository;
    delete iKeylockPolicy;
    }
    

// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::HandleKeyEventL()
// ----------------------------------------------------------------------------
//
TKeyResponse CSysApDefaultKeyHandler::HandleKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType )
{
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::HandleKeyEventL: aKeyEvent.iCode=%d, aKeyEvent.iScanCode=%d, aType=%d"),
                                aKeyEvent.iCode, aKeyEvent.iScanCode, aType ) );
    
    TKeyResponse response = EKeyWasNotConsumed;
    
    __ASSERT_DEBUG( aType == EEventKey, User::Invariant() ); // other than event keys should not provided
        
    if( aType == EEventKey )
        {
        response = EKeyWasConsumed; // set again in default case if not consumed
        
        switch ( aKeyEvent.iCode )
            {
            case EKeyGripOpen:
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyGripOpen") ) );
                RProperty::Set( KPSUidHWRM, KHWRMGripStatus, EPSHWRMGripOpen );
                iKeypadWasLocked = iKeylock->IsKeyLockEnabled();
                iKeylock->DisableKeyLock();
                if (!IsDeviceLocked())
                    {
                    iCallback.ExecCommandL( MSysapCallback::EResetKeyguardState );
                    iCallback.ExecCommandL( MSysapCallback::ECancelPowermenu );
                    }
                // apply default light control
                iCallback.ExecCommandL( MSysapCallback::EUpdateLights, TUpdateLightsBuf(EKeyGripOpen) );
                break;
            
            case EKeyGripClose:
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyGripClosed") ) );
                RProperty::Set( KPSUidHWRM, KHWRMGripStatus, EPSHWRMGripClosed );
                iKeylockPolicy->EnableKeyguardFeature();
                iCallback.ExecCommandL( MSysapCallback::ECancelPowermenu );
				if( !iSlideRepository )
					{ // default behavior is to always ask for keylock
					if ( !IsDeviceLocked() && UiReady() )
						{
						iKeylock->OfferKeyLock();
						}
					}
				else
					{ // keylock action is defined by user setting
                    TInt value = 0;
                    RProperty::Get(KPSUidStartup, KPSStartupUiPhase, value);
                    TBool startupOver = (value == EStartupUiPhaseAllDone);
                    if(startupOver)
                        {
						TInt status(0);
						TInt err = iCallStateProperty.Get( status );
						if (err == KErrNone)
							{
							switch ( status )
								{
								case EPSCTsyCallStateUninitialized:
								case EPSCTsyCallStateNone:
									{
                 	TInt keyGuardSetting;
									iSlideRepository->Get( KSlideKeyguard, keyGuardSetting );
									switch( ( TSlideSettingKeyguard ) keyGuardSetting )
										{
										case ESlideSettingsKeyguardActivatingOn: 
											iKeylock->EnableKeyLock();
											break;
										case ESlideSettingsKeyguardActivatingAskMe: 
											iKeylock->OfferKeyLock();
											break;
										case ESlideSettingsKeyguardActivatingOff: 
											//do nothing
											break;
										case ESlideSettingsKeyguardActivatingAutomatic: 
											if( iKeypadWasLocked )
												{
												iKeylock->EnableKeyLock();
												}
											break;
										}
									}
                            default: // any other state
                                break;
								}
                            }
                        }
					}
                // apply default light control
                iCallback.ExecCommandL( MSysapCallback::EUpdateLights, TUpdateLightsBuf(EKeyGripClose) );
                break;
            
            case EKeyFlipOpen: // simulated key event
                {
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyFlipOpen") ) );
                SetDisplayState( ETrue );
                // apply default light control
                iCallback.ExecCommandL( MSysapCallback::EUpdateLights, TUpdateLightsBuf(EKeyFlipOpen) );
                }
                break;
            
            case EKeyFlipClose: // simulated key event
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyFlipClose") ) );
                SetDisplayState( EFalse );
                // apply default light control
                iCallback.ExecCommandL( MSysapCallback::EUpdateLights, TUpdateLightsBuf(EKeyFlipClose) );
                break;
                
            case EKeyTwistOpen:
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyTwistOpen") ) );
                RProperty::Set( KPSUidHWRM, KHWRMTwistStatus, EPSHWRMTwistOpen );
                break;
                
            case EKeyTwistClose:
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyTwistClose") ) );
                RProperty::Set( KPSUidHWRM, KHWRMTwistStatus, EPSHWRMTwistClose );
                break;                

            case EKeyCamera:
                {   // PHOREPOC-DELTA (GCC): a scope for repeatCount -- the next case labels jumped over its
                    // initialisation ("jump to case label"; RVCT accepted it)
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyCamera") ) );
                
                TBool repeatCount(EFalse);
                if ( iCameraLongPress ) 
                    {
                    repeatCount = ( aKeyEvent.iRepeats > 0 ) ? ETrue: EFalse;
                    }
                else 
                    {
                    repeatCount = ETrue;
                    }

                if ( iCameraSupported && !IsDeviceLocked() && !DoShowKeysLockedNote() && repeatCount)
                    {
                    TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyCamera aKeyEvent.iRepeats = %d, repeatCount= %d"), aKeyEvent.iRepeats, repeatCount ) );
					iCallback.ExecCommandL( MSysapCallback::ECancelPowermenu ); 
                    ActivateApplicationL( KSysApCamcorderUid );
                    }
                break;
                }
                
            case EKeyPoC:
                TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::HandleKeyEventL: EKeyPoC") ) );
                LaunchPocL();
                break;                
             
            default:
                response = EKeyWasNotConsumed;
                TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::CSysApDefaultKeyHandler: key was not consumed.") ) ); 
                break;
                    
            }
        }
    
    return response;
}  
    
  

#ifndef SYSAP_USE_STARTUP_UI_PHASE    
// -----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::IsStateNormal()
// -----------------------------------------------------------------------------
//
TBool CSysApDefaultKeyHandler::IsStateNormal() const
    {
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::IsStateNormal")) );

    TInt state;
    TInt errorCode = RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    
    if ( errorCode == KErrNone )
        {
        return state == ESwStateNormalRfOn ||
               state == ESwStateNormalRfOff ||
               state == ESwStateNormalBTSap;
        }
    else
        {
        TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::IsStateNormal: Failed to read global system state, error code %d." ), errorCode ) );
        return EFalse;
        }
    }
#endif // SYSAP_USE_STARTUP_UI_PHASE

// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::UiReady()
// 
// ----------------------------------------------------------------------------

TBool CSysApDefaultKeyHandler::UiReady() const
    {
#ifdef SYSAP_USE_STARTUP_UI_PHASE    
    TInt state( 0 );
    
    TInt err = RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    
    if ( err != KErrNone || state == ESwStateEmergencyCallsOnly ) // emergency calls only state must block UI operations
        {
        TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::UiReady: KPSGlobalSystemState=%d, err=%d." ), state, err ) );
        return EFalse;
        }
    
    err = RProperty::Get( KPSUidStartup, KPSStartupUiPhase, state );
    
    if ( err != KErrNone )
        {
        TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::UiReady: Failed to read KPSStartupUiPhase, err=%d." ), err ) );
        return EFalse;
        }
    return state == EStartupUiPhaseAllDone;
#else // SYSAP_USE_STARTUP_UI_PHASE
    // if KPSSStartupUiPhase is not used, global system state normal is handled as UI idle state
    return IsStateNormal();
#endif // SYSAP_USE_STARTUP_UI_PHASE        
    }
    
// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::ActivateApplicationL()
// ----------------------------------------------------------------------------
//
void CSysApDefaultKeyHandler::ActivateApplicationL( const TUid aUid ) const
    {
    TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::ActivateApplicationL") ) );
    
    // Applications must not be activated before reaching normal system operation state
    if ( UiReady() )
        {
        CAknTaskList* apaTaskList = CAknTaskList::NewL( CCoeEnv::Static()->WsSession() );
        TApaTask apaTask = apaTaskList->FindRootApp( aUid ); // only root application should not be activated
        delete apaTaskList;
        
        if ( apaTask.Exists() )
            {
            TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::ActivateApplicationL: application brought to foreground") ) );
#ifdef _DEBUG
            // camery latency measurement environment instrumentation, don't remove
            if ( aUid == KSysApCamcorderUid )
                {
                TRACES( RDebug::Print( _L("e_CAM_PRI_OFF_TO_ON 1") ) );
                }
#endif // _DEBUG            
            CAknSgcClient::MoveApp ( apaTask.WgId(), ESgcMoveAppToForeground );
            }
        else
            {
            TInt err( KErrNone );
            TApaAppInfo appInfo;
            RApaLsSession apaLsSession;
            User::LeaveIfError( apaLsSession.Connect() );
            CleanupClosePushL( apaLsSession );
            err = apaLsSession.GetAppInfo( appInfo, aUid );
            if ( !err )
                {
                TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::ActivateApplicationL: starting application") ) );
                CApaCommandLine* apaCommandLine = CApaCommandLine::NewLC();
                apaCommandLine->SetExecutableNameL( appInfo.iFullName );
                apaCommandLine->SetCommandL( EApaCommandRun );
#ifdef _DEBUG
                // camery latency measurement environment instrumentation, don't remove
                if ( aUid == KSysApCamcorderUid )
                    {
                    TRACES( RDebug::Print( _L("e_CAM_PRI_OFF_TO_ON 1") ) );
                    }
#endif // _DEBUG                   
                apaLsSession.StartApp( *apaCommandLine );
                CleanupStack::PopAndDestroy( apaCommandLine );
                }
            else
                {
                TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::ActivateApplicationL err=%d"), err ) );
                }                
            // pop and destroy will also close this session
            CleanupStack::PopAndDestroy( &apaLsSession );
            }
        }
    }
    
// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::LaunchPocL()
// ----------------------------------------------------------------------------
//
void CSysApDefaultKeyHandler::LaunchPocL()
    {
    TRACES( RDebug::Print(_L("CSysApDefaultKeyHandler::LaunchPocL") ) );
    
    if ( UiReady() && !IsDeviceLocked() && !DoShowKeysLockedNote() )
        {
        CAiwServiceHandler* serviceHandler = CAiwServiceHandler::NewLC();
        serviceHandler->AttachL( R_POC_AIW_INTEREST );
            
        CAiwGenericParamList* inParams = CAiwGenericParamList::NewLC();
        inParams->AppendL(TAiwGenericParam( EGenericParamPoCLaunchView, TAiwVariant(EAiwPoCMainView)));
        serviceHandler->ExecuteServiceCmdL( KAiwCmdPoC, *inParams, serviceHandler->OutParamListL());
        
        CleanupStack::PopAndDestroy( inParams );
        CleanupStack::PopAndDestroy( serviceHandler);
        }
    }
    
// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::IsDeviceLocked()
// ----------------------------------------------------------------------------
TBool CSysApDefaultKeyHandler::IsDeviceLocked() const    
    {
    TInt value(0);
    TBool retval(EFalse);

    RProperty::Get( KPSUidCoreApplicationUIs, KCoreAppUIsAutolockStatus, value );
    if ( value > EAutolockOff )
        {
        retval = ETrue;
        }
    
    TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::IsDeviceLocked()=%d" ), retval ) );
    
    return retval;
    }


// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::SetDisplayState
// ----------------------------------------------------------------------------
void CSysApDefaultKeyHandler::SetDisplayState( TBool aFlipOpen ) const
    {
    TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::SetDisplayState: aFlipOpen=%d" ), aFlipOpen ) );
    
    if ( iCoverDisplaySupported )
        {
        // Turn on/off the main display 
        TInt err = UserSvr::HalFunction(EHalGroupDisplay, EDisplayHalSetState, (TAny*)aFlipOpen, NULL);
    
        if ( err == KErrNone )
            {
            TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::SetDisplayState: Main display state set: %d" ), aFlipOpen ) );
            }
        else
            {
            TRACES( RDebug::Print( _L( "CSysApDefaultKeyHandler::SetDisplayState: Main display state set failed: %d." ), err ) );
            }    
        }

    }

// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::DoShowKeysLockedNote
// ----------------------------------------------------------------------------
//
TBool CSysApDefaultKeyHandler::DoShowKeysLockedNote()
    {
    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::DoShowKeysLockedNote: iAknUiServerConnected=%d"), iAknUiServerConnected ) );
    
    TInt err(KErrNone);
    
    if ( !iAknUiServerConnected )
        {
        err = iAknUiServer.Connect();
        
        if ( err != KErrNone )
            {
            TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::DoShowKeysLockedNote: RAknUiServer::Connect failed, err=%d"), err ) );
            return ETrue; // assume that keypad/device is locked
            }
        iAknUiServerConnected = ETrue;            
        }
    
    TBool keysLocked;
    err = iAknUiServer.ShowKeysLockedNote( keysLocked );
    
    if ( err != KErrNone )
        {
        TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::DoShowKeysLockedNote: RAknUiServer::ShowKeysLockedNote failed, err=%d"), err ) );
        keysLocked = ETrue; // assume that keypad/device is locked
        }
        
    return keysLocked;        
    }

// ----------------------------------------------------------------------------
// CSysApDefaultKeyHandler::IsDeviceModeKey
// ----------------------------------------------------------------------------
//
TBool CSysApDefaultKeyHandler::IsDeviceModeKey( const TKeyEvent& aKeyEvent )
    {
    TBool response( EFalse );
    
    switch ( aKeyEvent.iCode )
        {
        case EKeyGripOpen:
        case EKeyGripClose:
        case EKeyFlipOpen:
        case EKeyFlipClose:
        case EKeyTwistOpen:
        case EKeyTwistClose:
            response = ETrue;
            break;
            
        default:
            break; // not a device mode key            
        }

    TRACES( RDebug::Print( _L("CSysApDefaultKeyHandler::IsDeviceModeKey: aKeyEvent.iCode=%d, response=%d"), aKeyEvent.iCode, response ) );
    
    return response;        
    }
