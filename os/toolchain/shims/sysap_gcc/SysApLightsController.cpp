/*
* Copyright (c) 2005-2010 Nokia Corporation and/or its subsidiary(-ies). 
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:  CSysApLightsController implementation.
*
*/


// INCLUDE FILES
#include <activitymanager.h>
#include "SysApLightsController.h"
#include "SysApAppUi.h"
#include "CoreApplicationUIsInternalPSKeys.h"
#include <e32svr.h>
#include <hwrmdomainpskeys.h>
#include "SysApFeatureManager.h"
#ifdef RD_LIGHT_CONTROL_CHANGE
#include "sysaplightpluginhandler.h"
#endif // RD_LIGHT_CONTROL_CHANGE

#include "SysApFeatureManager.h"
#include "startupdomainpskeys.h"
#include <hal.h>
#include <hwrmpowerstatesdkpskeys.h>
// PHOREPOC delta (2026-10-15): this file's TRACES on in a release build -- the lights controller's every decision to the
// PhoneLab log (the ekern seam forwards lines starting "CSysApLightsController"); the "touch does not wake the backlight" hunt
#include <e32debug.h>
#undef TRACES
#define TRACES(x) x

// CONSTANTS

const TInt KLightsOffPeriodLockJustActivated ( 5 ); // 5 sec
const TInt KMaxLightsOffPeriodForScreenSaverAnimation ( 30 ); // 30 sec
const TInt KLightsOnRepeatMinPeriod( 3 ); // Do not set lights on again for concecutive calls within this period (seconds)
const TInt KLightsOnUnlockNotePeriodMs( 1500 ); // 1.5 second timeout when unlock note is shown

#ifdef RD_LIGHT_CONTROL_CHANGE
const TInt KMilliSecondCoefficient = 1000;
#endif // RD_LIGHT_CONTROL_CHANGE
// ========================== MEMBER FUNCTIONS ================================

// ----------------------------------------------------------------------------
// CSysApLightsController* CSysApLightsController::NewL( )
// ----------------------------------------------------------------------------

CSysApLightsController* CSysApLightsController::NewL(CSysApAppUi& aSysApAppUi, const TInt aTimeout, const TBool aCoverDisplay )
    {       
    TRACES( RDebug::Print( _L("CSysApLightsController::NewL") ) ); 
    CSysApLightsController* self = new ( ELeave ) CSysApLightsController( aSysApAppUi, aTimeout, aCoverDisplay );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop(); //self
    return self;
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ConstructL( )
// ----------------------------------------------------------------------------

void CSysApLightsController::ConstructL( )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::ConstructL: lights timeout=%d"), iLightsTimeout ) );
    TRACES( RDebug::Print( _L("CSysApLightsController::ConstructL: cover display supported=%d"), iCoverDisplaySupported ) );
    iSysApTimer = new ( ELeave ) CSysApTimer( *this );
       
    iLight = CHWRMLight::NewL();   
    
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler = CSysApLightPluginHandler::NewL();
#endif // RD_LIGHT_CONTROL_CHANGE    
    
    iSysApFeatureManager = CSysApFeatureManager::NewL();
    
    EnableActivityManagerL();

    SetFlipOpenNoLights( iFlipOpen, ETrue );

#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::EInitialLightsOn );
#endif // RD_LIGHT_CONTROL_CHANGE    
    SetLightsOnL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::CSysApLightsController() 
// ----------------------------------------------------------------------------

CSysApLightsController::CSysApLightsController( CSysApAppUi& aSysApAppUi, const TInt aTimeout, const TBool aCoverDisplay ) 
    : 
    iGripOpen( ETrue ), 
    iFlipOpen( ETrue ), 
    iSysApAppUi( aSysApAppUi ),
    iLightsTimeout( aTimeout ),
    iCoverDisplaySupported( aCoverDisplay ),
    iInactivityDetected( EFalse )
#ifdef RD_LIGHT_CONTROL_CHANGE
    , iKeyEventsObserved( EFalse )
#endif // RD_LIGHT_CONTROL_CHANGE    
    {
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::~CSysApLightsController()
// ----------------------------------------------------------------------------

CSysApLightsController::~CSysApLightsController()
    {
    TRACES( RDebug::Print( _L("~CSysApLightsController") ) );
    DisableActivityManager();

    if ( iSysApTimer )
        {
        if ( iSysApTimer->IsActive() )
            {
            iSysApTimer->Cancel();
#ifdef RD_LIGHT_CONTROL_CHANGE
            iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOffTimerStop,
                                              TPckgBuf<TBool>(EFalse) );
#endif // RD_LIGHT_CONTROL_CHANGE
            }
        
        delete iSysApTimer;
        }

#ifdef RD_LIGHT_CONTROL_CHANGE
    delete iLightPluginHandler;
#endif // RD_LIGHT_CONTROL_CHANGE
    
    delete iLight;   
	delete iSysApFeatureManager;

    }

// ----------------------------------------------------------------------------
// CSysApLightsController::AlarmOccuredL( const TBool aAlarmActive )
// ----------------------------------------------------------------------------

void CSysApLightsController::AlarmOccuredL( const TBool aAlarmActive )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::AlarmOccuredL: %d"), aAlarmActive ) ); 
    iAlarmAlerting = aAlarmActive;
    
    CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::EAlarmState, TPckgBuf<TBool>(aAlarmActive) );    
#endif // RD_LIGHT_CONTROL_CHANGE

    TBool blinkEnabled = ETrue; 
                
    if( iSysApFeatureManager->Supported( KSysApFeatureIdNoFlasUiInSilentMode ) ) 
        {
        blinkEnabled = EFalse;
        }

    // Incoming call overrides alarm when lights are concerned
    if( !iCallRinging && aAlarmActive )
        {
        ReserveLightL();
        SetLightsOnL( blinkEnabled ); // set lights blinking (if enabled)
        }
    else 
        {
        SetLightsOnL( iCallRinging && iSilent && blinkEnabled ); // keep on blinking if call is ringing in silent mode
        ReleaseLight();
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::AccessoryConnectedL()
// ----------------------------------------------------------------------------

void CSysApLightsController::AccessoryConnectedL( TBool aConnected )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::AccessoryConnectedL()" ) ) ); 

    if ( aConnected && !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::EAccessoryConnection, TPckgBuf<TBool>(aConnected) );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EAccessoryConnection, TPckgBuf<TBool>(aConnected) );
        }        
#endif // RD_LIGHT_CONTROL_CHANGE
    
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ChargerConnectedL()
// ----------------------------------------------------------------------------    
void CSysApLightsController::ChargerConnectedL( TBool aConnected )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::ChargerConnectedL(): aConnected=%d" ), aConnected ) );
    
    TBool isLocked( iSysApAppUi.DeviceLockState() || iSysApAppUi.KeyLockState() );
    
    // if the device is not locked user activity will switch on the lights upon charger removal
    if ( ((!isLocked && aConnected) || isLocked ) && !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::EChargerConnection, TPckgBuf<TBool>(aConnected) );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();   
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EChargerConnection, TPckgBuf<TBool>(aConnected) );
        }        
#endif // RD_LIGHT_CONTROL_CHANGE 
	TInt state( 0 );
	TInt error = RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    if ( error == KErrNone && state == ESwStateCharging ) 
		{
		EnableActivityManagerL();
		}		
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ChargingCompleteL()
// ----------------------------------------------------------------------------    
void CSysApLightsController::ChargingCompleteL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::ChargingCompleteL()" ) ) );
    
    if ( !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE    
        iLightPluginHandler->HandleEvent( SysApLightExtension::EChargingComplete );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE            
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EChargingComplete );
        }
#endif // RD_LIGHT_CONTROL_CHANGE        
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ProfileChangedL()
// ----------------------------------------------------------------------------    
void CSysApLightsController::ProfileChangedL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::ProfileChangedL()" ) ) ); 
    
    // If device is not locked, activity event turns on lights.
    if ( ( iSysApAppUi.DeviceLockState() || iSysApAppUi.KeyLockState() ) && !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();        
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::EProfileChanged );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EProfileChanged );
        }        
#endif // RD_LIGHT_CONTROL_CHANGE           
    }
    
// ----------------------------------------------------------------------------
// CSysApLightsController::MemoryCardInsertedL()
// ----------------------------------------------------------------------------    
void CSysApLightsController::MemoryCardInsertedL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::MemoryCardInsertedL()" ) ) ); 

    if ( !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::EMemoryCardInserted );
#endif // RD_LIGHT_CONTROL_CHANGE    
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EMemoryCardInserted );
        }
#endif // RD_LIGHT_CONTROL_CHANGE            
    
    }    

// ----------------------------------------------------------------------------
// CSysApLightsController::CallComingInL( const TBool aCallringing )
// ----------------------------------------------------------------------------

void CSysApLightsController::CallComingInL( const TBool aCallringing )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::CallComingInL: %d"), aCallringing ) ); 
    iCallRinging = aCallringing;
    
    CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::ECallStateRinging, TPckgBuf<TBool>(aCallringing) ); 
#endif // RD_LIGHT_CONTROL_CHANGE  

    if( aCallringing )
        {
        ReserveLightL();
        
        // Incoming call overrides alarm when lights are concerned
        if( iSilent )
            {
            
            // default we want blink while in call
            TBool blink = ETrue; 
            
            // in some touch products we don't want lights to blink while call is incoming
            if( iSysApFeatureManager->Supported( KSysApFeatureIdNoFlasUiInSilentMode ) )
                {
                blink = EFalse;
                }
            
            SetLightsOnL( blink ); // set lights blinking
            }
        else
            {
            SetLightsOnL();
            }
        }
    else 
        {
        SetLightsOnL();
        ReleaseLight();
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::PowerKeyPressedL()
// ----------------------------------------------------------------------------

void CSysApLightsController::PowerKeyPressedL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::PowerKeyPressedL()") ) ); 
    
    if ( !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();        
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::EPowerKeyPressed );
#endif // RD_LIGHT_CONTROL_CHANGE      
        SetLightsOnL();
        }
    else
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EPowerKeyPressed );
#endif // RD_LIGHT_CONTROL_CHANGE      
        }        
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetGripOpenL( const TBool aOpen )
// ----------------------------------------------------------------------------
void CSysApLightsController::SetGripOpenL( const TBool aOpen )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::SetGripOpenL( aOpen:%d )"), aOpen ) ); 
    iGripOpen = aOpen;

    CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::EGripOpenState, TPckgBuf<TBool>(aOpen) );
#endif // RD_LIGHT_CONTROL_CHANGE      
    
    TurnOffInactiveTargetsL();
    SetLightsOnL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetFlipOpenL( const TBool aOpen )
// ----------------------------------------------------------------------------
void CSysApLightsController::SetFlipOpenL( const TBool aOpen )
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::SetFlipOpenL( aOpen:%d )"), aOpen ) ); 

    SetFlipOpenNoLights( aOpen, EFalse );
    
    CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::EFlipOpenState, TPckgBuf<TBool>(aOpen) );
#endif // RD_LIGHT_CONTROL_CHANGE     

    TurnOffInactiveTargetsL();

    SetLightsOnL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetSilentModeOn( const TBool aSilent )
// ----------------------------------------------------------------------------

void CSysApLightsController::SetSilentModeOn( const TBool aSilent )
    {
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ESilentMode, TPckgBuf<TBool>(aSilent) );
#endif // RD_LIGHT_CONTROL_CHANGE  
    
    iSilent = aSilent;
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::HandleLightsRequireL( )
// ----------------------------------------------------------------------------

void CSysApLightsController::HandleLightsRequireL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleLightsRequireL" ) ) );

    CancelLightsExplicitlyOffWithDelayL(); 
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsRequire );
#endif // RD_LIGHT_CONTROL_CHANGE 

    SetLightsOnL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::HandleRawKeyEventLightsRequireL
// ----------------------------------------------------------------------------
void CSysApLightsController::HandleRawKeyEventLightsRequireL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleLightsRequireL" ) ) );

    // This request comes from NspsWsPlugin via System Application Notification API
    if ( !iSysApAppUi.DeviceLockState() && !iSysApAppUi.KeyLockState()
#ifdef RD_LIGHT_CONTROL_CHANGE
        && iKeyEventsObserved
#endif // RD_LIGHT_CONTROL_CHANGE      
       )
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsRequireRawKeyEvent );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsRequireRawKeyEvent );
        }        
#endif // RD_LIGHT_CONTROL_CHANGE           
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::HandleForcedLightsVTRequireL( const TInt aLightParameter )
// ----------------------------------------------------------------------------

void CSysApLightsController::HandleForcedLightsVTRequireL( const TInt aLightsParameter )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleForcedLightsVTRequireL: aLightsParameter: %d" ), aLightsParameter ) );
    
    iForcedLightsOn = aLightsParameter != EForcedLightsOn ? EFalse : ETrue;

    CancelLightsExplicitlyOffWithDelayL();
    
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsForcedVideoTel,
                                      TPckgBuf<TBool>(aLightsParameter == EForcedLightsOn) );
#endif // RD_LIGHT_CONTROL_CHANGE
    
    if ( aLightsParameter != EForcedLightsOn)
        {
        ReleaseLight();
        }
    else 
        {
        ReserveLightL();
        }

    SetLightsOnL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::HandleForcedLightsSSRequireL( const TInt aLightParameter )
// ----------------------------------------------------------------------------

void CSysApLightsController::HandleForcedLightsSSRequireL( const TInt aLightsParameter )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleForcedLightsSSRequireL: aLightsParameter: %d" ), aLightsParameter ) );
    
#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsForcedScreenSaver,
                                      TPckgBuf<TBool>(aLightsParameter > 0) );
#endif // RD_LIGHT_CONTROL_CHANGE
    
    if ( aLightsParameter == 0)
        {
        SetLightsOffL();
        // PHOREPOC delta (2026-10-15): the screensaver's lights-off = the display OFF (the backlight at 0, not the
        // timeout's dimmed floor) -- the kernel clears it at the next lights-on
        HAL::Set( HAL::EBacklightState, 0 );
        }
    else if ( aLightsParameter > 0 )
        {
        SetDisplayLightsOnWithoutResettingInactivityTimersL();
        // PHOREPOC delta (2026-10-15): the forced-off timer only when the user is really inactive. A screensaver that
        // starts its display just as a touch dismisses it (boot 448: activity, then this request) armed it, and 30 s
        // later the lights went off in use -- with SysAp still counting the user as active, touches did not wake them
        if ( User::InactivityTime().Int() >= iLightsTimeout )
            {
            iSysApTimer->ActivateTimerL( KMaxLightsOffPeriodForScreenSaverAnimation );
            }
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOffTimerStart, 
                                          TPckgBuf<TInt>( KMaxLightsOffPeriodForScreenSaverAnimation*KMilliSecondCoefficient ) );
#endif // RD_LIGHT_CONTROL_CHANGE        
        }
    }
    
// ---------------------------------------------------------------------------- 
// CSysApLightsController::HandleForcedLightsATCRequireL( const TInt aLightParameter ) 
// ----------------------------------------------------------------------------     
void CSysApLightsController::HandleForcedLightsATCRequireL( const TInt aLightsParameter ) 
    { 
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleForcedLightsATCRequireL: aLightsParameter: %d" ), aLightsParameter ) ); 
     
    if ( aLightsParameter != EForcedLightsOn ) 
        { 
        TRACES( RDebug::Print( _L( "Cancel inactivity timer" ) ) ); 
#ifdef RD_LIGHT_CONTROL_CHANGE     
        iLightPluginHandler->HandleEvent( SysApLightExtension::EInactivityDetected ); 
#endif // RD_LIGHT_CONTROL_CHANGE 
        SetLightsOffL(); 
        } 
    else 
        { 
        CancelLightsExplicitlyOffWithDelayL(); 
#ifdef RD_LIGHT_CONTROL_CHANGE     
        iLightPluginHandler->HandleEvent( SysApLightExtension::EActivityDetected ); 
#endif // RD_LIGHT_CONTROL_CHANGE         
        SetLightsOnL(); 
        } 
    } 

// ----------------------------------------------------------------------------
// CSysApLightsController::DeviceLockStateChangedL()
// ----------------------------------------------------------------------------
void CSysApLightsController::DeviceLockStateChangedL( TBool aEnabled )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::DeviceLockStateChangedL(): aEnabled=%d" ), aEnabled ) );

    CancelLightsExplicitlyOffWithDelayL();

#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEvent( SysApLightExtension::EDeviceLockState, TPckgBuf<TBool>(aEnabled) );
#endif // RD_LIGHT_CONTROL_CHANGE
    EnableActivityManagerL(aEnabled);
    
    if ( aEnabled )
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction ); // complete previous event            
#endif // RD_LIGHT_CONTROL_CHANGE         
        SetLightsExplicitlyOffWithDelayL();
        }
    else
        {
        ReportUserActivity();
        if ( !Alerting() )
            {
            SetLightsOnL();    
            }
#ifdef RD_LIGHT_CONTROL_CHANGE
        else
            {
            iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction ); // complete previous event                
            }
#endif // RD_LIGHT_CONTROL_CHANGE         
                
        }        
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::KeylockStateChangedL()
// ----------------------------------------------------------------------------
void CSysApLightsController::KeylockStateChangedL( TBool aEnabled )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::KeylockStateChangedL(): aEnabled=%d" ), aEnabled ) );

    CancelLightsExplicitlyOffWithDelayL();

#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEvent( SysApLightExtension::EKeyLockState, TPckgBuf<TBool>(aEnabled) );
#endif // RD_LIGHT_CONTROL_CHANGE
    EnableActivityManagerL(aEnabled);
    
    if ( aEnabled )
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction ); // complete previous event           
#endif // RD_LIGHT_CONTROL_CHANGE         
        SetLightsExplicitlyOffWithDelayL();
        }
    else
        {
        ReportUserActivity();
        if ( !Alerting() )
            {
            SetLightsOnL();    
            }
#ifdef RD_LIGHT_CONTROL_CHANGE
        else
            {
            iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction ); // complete previous event                
            }
#endif // RD_LIGHT_CONTROL_CHANGE         
        }        
    }    

// ----------------------------------------------------------------------------
// CSysApLightsController::SwitchLightsOnSoftRejectL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SwitchLightsOnSoftRejectL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::SwitchLightsOnSoftRejectL()" ) ) );

    ReportUserActivity();
    
    // When soft reject is used in silent mode, the lights are set on because otherwise
    // blinking light disturb user when they are using phone UI
    if ( ( !iCallRinging || ( iCallRinging && iSilent ) )
        && !iAlarmAlerting ) 
        {
        CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE    
        iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOnSoftReject );
#endif // RD_LIGHT_CONTROL_CHANGE
        SetLightsOnL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOnSoftReject );
        }        
#endif // RD_LIGHT_CONTROL_CHANGE             
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsExplicitlyOffWithDelayL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsExplicitlyOffWithDelayL()
    {
#ifdef RD_LIGHT_CONTROL_CHANGE
    iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOffTimerStart, 
                                      TPckgBuf<TInt>(KLightsOffPeriodLockJustActivated*KMilliSecondCoefficient) );
    iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
#endif // RD_LIGHT_CONTROL_CHANGE    
    iSysApTimer->ActivateTimerL( KLightsOffPeriodLockJustActivated );
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::CancelLightsExplicitlyOffWithDelayL()
// ----------------------------------------------------------------------------

void CSysApLightsController::CancelLightsExplicitlyOffWithDelayL()
    {
    if ( iSysApTimer->IsActive() )
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOffTimerStop, 
                                          TPckgBuf<TBool>(EFalse) );
#endif // RD_LIGHT_CONTROL_CHANGE        
        iSysApTimer->Cancel();
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsOnUnlockNoteL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsOnUnlockNoteL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnUnlockNoteL()" ) ) );
 
    if ( !Alerting() ) 
        {
        CancelLightsExplicitlyOffWithDelayL();
        
#ifdef RD_LIGHT_CONTROL_CHANGE    
        iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOnUnlockNote );
#endif // RD_LIGHT_CONTROL_CHANGE             
        
        SetLightsOnL();
        
        iSysApTimer->ActivateMsTimerL( KLightsOnUnlockNotePeriodMs );
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOffTimerStart,
                                          TPckgBuf<TBool>(KLightsOnUnlockNotePeriodMs) );
#endif // RD_LIGHT_CONTROL_CHANGE        
        }
    else
        {
#ifdef RD_LIGHT_CONTROL_CHANGE    
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOnUnlockNote );
#endif // RD_LIGHT_CONTROL_CHANGE             
        }        
    }
    
// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsOnEcsQueryL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsOnEcsQueryL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::SetLightsOnEcsQueryL()") ) ); 
    
    if ( !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();        
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOnEcsQuery );
#endif // RD_LIGHT_CONTROL_CHANGE      
        SetLightsOnL();
        }
    else
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOnEcsQuery );
#endif // RD_LIGHT_CONTROL_CHANGE      
        }        
    }
    
// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsOnSecurityQueryL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsOnSecurityQueryL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::SetLightsOnSecurityQueryL()") ) ); 
    
    if ( !Alerting() )
        {
        CancelLightsExplicitlyOffWithDelayL();        
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOnSecQuery );
#endif // RD_LIGHT_CONTROL_CHANGE      
        SetLightsOnL();
        }
    else
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleEventNoAction( SysApLightExtension::ELightsOnSecQuery );
#endif // RD_LIGHT_CONTROL_CHANGE      
        }        
    }        

// ----------------------------------------------------------------------------
// CSysApLightsController::DoLightsTimeoutChangedL()
// ----------------------------------------------------------------------------

void CSysApLightsController::DoLightsTimeoutChangedL( const TInt aTimeout )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::DoLightsTimeoutChangedL(): aTimeout=%d" ), aTimeout ) );

    if ( aTimeout < KMinimumLightsTimeout )
        {
        iLightsTimeout = KMinimumLightsTimeout;
        }
    else if ( aTimeout > KMaximumLightsTimeout )
        {
        iLightsTimeout = KMaximumLightsTimeout;
        }
    else
        {
        iLightsTimeout = aTimeout;
        }
        
#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EInactivityTimeoutChanged, 
                                      TPckgBuf<TInt>(iLightsTimeout*KMilliSecondCoefficient) );
#endif // RD_LIGHT_CONTROL_CHANGE 
        
    EnableActivityManagerL(); // activity manager reset
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::EnableActivityManagerL()
// ----------------------------------------------------------------------------

void CSysApLightsController::EnableActivityManagerL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::EnableActivityManager: lights timeout: %d sec" ), iLightsTimeout ) );
    if ( iActivityManager )
        {
        DisableActivityManager();
        }
    iActivityManager = CUserActivityManager::NewL( CActive::EPriorityStandard );
    iActivityManager->Start( iLightsTimeout,
                             TCallBack( HandleInactiveEventL, this ), 
                             TCallBack( HandleActiveEventL, this ) );
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::EnableActivityManagerL(TBool aIsLocked)
// ----------------------------------------------------------------------------

void CSysApLightsController::EnableActivityManagerL(TBool aIsLocked)
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::EnableActivityManager: isLocked: %d " ), aIsLocked ) );
    if ( iActivityManager )
        {
        DisableActivityManager();
        }
    iActivityManager = CUserActivityManager::NewL( CActive::EPriorityStandard );
    if(aIsLocked)
        {
        iActivityManager->Start( KMinimumLightsTimeout,
                                 TCallBack( HandleInactiveEventL, this ), 
                                 TCallBack( HandleActiveEventL, this ) );
        TRACES( RDebug::Print( _L( "CSysApLightsController::EnableActivityManager: locked: lights timeout: %d sec" ), 
        	                       KMinimumLightsTimeout ) );                             
        }
    else
        {
        iActivityManager->Start( iLightsTimeout ,
                                 TCallBack( HandleInactiveEventL, this ), 
                                 TCallBack( HandleActiveEventL, this ) );
        TRACES( RDebug::Print( _L( "CSysApLightsController::EnableActivityManager: not locked: lights timeout: %d sec" ), 
        	                       iLightsTimeout ) );                             
        }   
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::DisableActivityManager()
// ----------------------------------------------------------------------------

void CSysApLightsController::DisableActivityManager()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::DisableActivityManager()" ) ) );
    if ( iActivityManager )
        {
        iActivityManager->Cancel();
        delete iActivityManager;
        iActivityManager = NULL;
        }
    }

// ----------------------------------------------------------------------------
// TInt CSysApLightsController::HandleActiveEventL( TAny* aPtr )
// ----------------------------------------------------------------------------

TInt CSysApLightsController::HandleActiveEventL( TAny* aPtr )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleActiveEvent" ) ) );
    
    CSysApLightsController* lightsController = static_cast<CSysApLightsController*>(aPtr);
    
    lightsController->iInactivityDetected = EFalse;
    
    if ( !lightsController->Alerting() && !lightsController->iForcedLightsOn )
        {
        TBool locked = lightsController->iSysApAppUi.DeviceLockState() 
                        || lightsController->iSysApAppUi.KeyLockState();
        
        if ( !locked )
            {
            lightsController->CancelLightsExplicitlyOffWithDelayL();  
#ifdef RD_LIGHT_CONTROL_CHANGE
            lightsController->iLightPluginHandler->HandleEvent( SysApLightExtension::EActivityDetected );
#endif // RD_LIGHT_CONTROL_CHANGE            
            lightsController->SetLightsOnL();
            return KErrNone;
            }
        else if ( lightsController->iSysApAppUi.SysApFeatureManager().LockedStateLightsSupported() )
            {
            // lights on only if they are not already on
            if ( !lightsController->iLightsCurrentlyOn )
                {
                lightsController->CancelLightsExplicitlyOffWithDelayL();
#ifdef RD_LIGHT_CONTROL_CHANGE
                lightsController->iLightPluginHandler->HandleEvent( SysApLightExtension::EActivityDetected );
#endif // RD_LIGHT_CONTROL_CHANGE            
                lightsController->SetLockedStatePrimaryLightsL();
                return KErrNone;
                }                
            }
        }

#ifdef RD_LIGHT_CONTROL_CHANGE
        lightsController->iLightPluginHandler->HandleEventNoAction( SysApLightExtension::EActivityDetected );
#endif // RD_LIGHT_CONTROL_CHANGE   

        
    return KErrNone;
    }

// ----------------------------------------------------------------------------
// TInt CSysApLightsController::HandleInactiveEventL( TAny* aPtr )
// ----------------------------------------------------------------------------

TInt CSysApLightsController::HandleInactiveEventL( TAny* aPtr )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::HandleInactiveEvent" ) ) );
    
    CSysApLightsController* lightsController = STATIC_CAST( CSysApLightsController*, aPtr );
    
    lightsController->iInactivityDetected = ETrue;
    
#ifdef RD_LIGHT_CONTROL_CHANGE    
    lightsController->iLightPluginHandler->HandleEvent( SysApLightExtension::EInactivityDetected );
#endif // RD_LIGHT_CONTROL_CHANGE        
    
    if ( !lightsController->Alerting() && !lightsController->iForcedLightsOn )
        {
        lightsController->SetLightsOffL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        lightsController->iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
        }
#endif // RD_LIGHT_CONTROL_CHANGE   
        
    return KErrNone;
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ReportUserActivity()
// ----------------------------------------------------------------------------

void CSysApLightsController::ReportUserActivity() const
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::ReportUserActivity" ) ) );
    User::ResetInactivityTime();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::BatteryEmptyL()
// ----------------------------------------------------------------------------
void CSysApLightsController::BatteryEmptyL( TBool aEmpty )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::BatteryEmptyL: aEmpty=%d" ), aEmpty ) );
    iBatteryEmpty = aEmpty;
    
#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEvent( SysApLightExtension::EBatteryEmptyState, TPckgBuf<TBool>(aEmpty) );
#endif // RD_LIGHT_CONTROL_CHANGE   
    
    if ( aEmpty )
        {
        SetLightsOffL();
        }
#ifdef RD_LIGHT_CONTROL_CHANGE
    else
        {
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
        }
#endif // RD_LIGHT_CONTROL_CHANGE        
    }

// PHOREPOC delta (2026-10-18): safety net. A lights-off that is not the inactivity timeout (screensaver, keyguard,
// explicit off) leaves the activity manager counting the user as active, so it never reports the next touch and the
// lights stay off in use. A second, 1 s activity watcher is armed on such a lights-off; the next activity after it
// runs the normal active handler once. It is never deleted inside its own callback (RunL re-arms it afterwards).
static CUserActivityManager* gPheLightsNet = NULL;
static TBool gPheLightsNetArmed = EFalse;
static TInt PheLightsNetInactive( TAny* ) { return KErrNone; }
static TInt PheLightsNetActive( TAny* aPtr )
    {
    if ( gPheLightsNetArmed )
        {
        gPheLightsNetArmed = EFalse;
        RDebug::Printf( "PH: sysap lights safety net: activity -> lights on" );
        TRAP_IGNORE( static_cast<CSysApLightsController*>( aPtr )->HandleRawKeyEventLightsRequireL() );
        }
    return KErrNone;
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsOffL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsOffL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOffL: iBatteryEmpty=%d, iLightsCurrentlyOn=%d, iForcedLightsOn=%d" ),
                           iBatteryEmpty, iLightsCurrentlyOn, iForcedLightsOn ) );
    
    if ( !iBatteryEmpty && iForcedLightsOn )
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
#endif // RD_LIGHT_CONTROL_CHANGE
        return;
        }

	TInt err(KErrNone);
    TInt state( 0 );
    TInt error = RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    
    if ( error == KErrNone && state != ESwStateCharging ) 
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
		if ( !iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandOff ) )
			{
			TRAP(err, iLight->LightOffL(CHWRMLight::ESystemTarget));
			}
#else //  RD_LIGHT_CONTROL_CHANGE
		TRAP(err, iLight->LightOffL(CHWRMLight::ESystemTarget));
#endif // RD_LIGHT_CONTROL_CHANGE  
		}
	else
		{
		TRAP(err, iLight->LightOffL(CHWRMLight::ESystemTarget));
		iSysApAppUi.StopChargingBatteryL();
		//To switch off the display 
		TInt result = HAL::Set( HALData::EDisplayState, 0 );
		}            
    // Ignore unreserved in use warnings.
    if ( err != KErrNone && err != KErrInUse )
        {
        // Only trace as leaving here accomplishes nothing.
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOffL  failed: err: %d" ), err ) );
        }
    iLightsCurrentlyOn = EFalse;
    if ( !iInactivityDetected )
        {
        if ( !gPheLightsNet )
            {
            gPheLightsNet = CUserActivityManager::NewL( CActive::EPriorityStandard );
            gPheLightsNet->Start( 1, TCallBack( PheLightsNetInactive, this ), TCallBack( PheLightsNetActive, this ) );
            }
        gPheLightsNetArmed = ETrue;
        RDebug::Printf( "PH: sysap lights safety net armed (lights off without inactivity)" );
        }

#ifdef __SYSAP_MODULE_TEST
    iSysApAppUi.Beep();
#endif

#ifndef RD_LIGHT_CONTROL_CHANGE    
    iSysApAppUi.ActivateKeyeventForwardingForLights(ETrue);
#else // RD_LIGHT_CONTROL_CHANGE
    iKeyEventsObserved = ETrue;
#endif // RD_LIGHT_CONTROL_CHANGE
    
    if ( iSysApAppUi.SysApFeatureManager().LockedStateLightsSupported() )
        {
        // If low intensity lights are supported in locked state, then the lights must be set on with low intensity
        // as until active timeout has been reached
        // Otherwise we may end up into a situation where lights are not lit because activity cannot be detected without first entering
        // inactive state
        if ( !iInactivityDetected ) // inactivity not yet detected, so use small intensity lights
            {
            TBool locked = iSysApAppUi.DeviceLockState() || iSysApAppUi.KeyLockState();
    
            if ( locked ) // applied only if the device is in locked state
                {
                SetLockedStatePrimaryLightsL();
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetLightsOnL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLightsOnL( TBool aBlinking )
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnL: aBlinking=%d, iGripOpen=%d, iFlipOpen=%d"  ), 
        aBlinking, iGripOpen, iFlipOpen ) );
    
    ReportUserActivity();

    if( iBatteryEmpty )
        {
        SetLightsOffL();
        return;
        }

#ifndef RD_LIGHT_CONTROL_CHANGE        
    // Deactivate raw keyevent forwarding as those are only interesting when lights are off due inactivity.
    iSysApAppUi.ActivateKeyeventForwardingForLights(EFalse);     
#else // RD_LIGHT_CONTROL_CHANGE
    iKeyEventsObserved = EFalse;
#endif // RD_LIGHT_CONTROL_CHANGE    

    TInt err(KErrNone);
    
    if ( aBlinking )
        {
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnL - LIGHTS BLINK" ) ) );
#ifdef RD_LIGHT_CONTROL_CHANGE
        if ( !iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandBlink ) )
            {
            TRAP(err, iLight->LightBlinkL(CHWRMLight::ESystemTarget));
            }
#else // RD_LIGHT_CONTROL_CHANGE  
        TRAP(err, iLight->LightBlinkL(CHWRMLight::ESystemTarget));
#endif // RD_LIGHT_CONTROL_CHANGE  
        // Do not remember this lights on, if lights were set to blink (as next normal lights on must be done anyway)
        iLightsCurrentlyOn = EFalse;
        }
    else
        {
        // Filter excess consequtive lights on calls within short time period as they are likely to be caused by same event.
        TTime now;
        now.HomeTime();
        
        if ( !iLightsCurrentlyOn || ( iAllowLightsOn  && (iLastLightsOnTime <= now - TTimeIntervalSeconds(KLightsOnRepeatMinPeriod) ) ) 
                || Alerting() )
            {
            TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnL - LIGHTS ON" ) ) );
#ifdef RD_LIGHT_CONTROL_CHANGE
            if ( iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandOn ) )
                {
                return;
                }
#endif // RD_LIGHT_CONTROL_CHANGE  
            TRAP(err, iLight->LightOnL(CHWRMLight::ESystemTarget));

            if ( err == KErrNone )
                {
                iLightsCurrentlyOn = ETrue;
                iLastLightsOnTime.HomeTime(); 
                }
				
			TInt state( 0 );
			TInt error = RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    		if ( error == KErrNone && state == ESwStateCharging ) 
				{
				TInt value = iSysApAppUi.StateOfProperty( KPSUidHWRMPowerState, KHWRMChargingStatus );
				if (value == EChargingStatusChargingComplete )
					{
					iSysApAppUi.StopChargingBatteryL();
					}
				else
				    {
                    if((value==EChargingStatusCharging)||
				       (value==EChargingStatusAlmostComplete)||
				       (value==EChargingStatusChargingContinued))
                            {
                            iSysApAppUi.StartChargingBatteryL();
                            }
                    //To switch on the display 
                    TInt result = HAL::Set( HALData::EDisplayState, 1 );
				    }
				}
            }
        else
            {
            TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnL - LIGHTS ON denied - Not necessary" ) ) );
#ifdef RD_LIGHT_CONTROL_CHANGE
            iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
#endif // RD_LIGHT_CONTROL_CHANGE   
            }
        }

    if ( err != KErrNone )
        {
        // Only trace as leaving here accomplishes nothing.
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetLightsOnL failed: err: %d" ),  err ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetDisplayLightsOnWithoutResettingInactivityTimersL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetDisplayLightsOnWithoutResettingInactivityTimersL()
    {    
    if( iBatteryEmpty )
        {
        SetLightsOffL();
        return;
        }

    TInt err(KErrNone);

    TRACES( RDebug::Print( _L( "CSysApLightsController::SetDisplayLightsOnWithoutResettingInactivityTimersL LIGHTS ON" ) ) );

    TTime now;
    now.HomeTime();
    if ( !iLightsCurrentlyOn || ( iAllowLightsOn  && (iLastLightsOnTime <= now - TTimeIntervalSeconds(KLightsOnRepeatMinPeriod) ) ) )
        {
#ifdef RD_LIGHT_CONTROL_CHANGE
        if ( iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandOn ) )
            {
            return;
            }
#endif // RD_LIGHT_CONTROL_CHANGE         
        TRAP(err, iLight->LightOnL(CHWRMLight::ESystemTarget));

        if ( err != KErrNone )
            {
            // Only trace as leaving here accomplishes nothing.
            TRACES( RDebug::Print( _L( "CSysApLightsController::SetDisplayLightsOnWithoutResettingInactivityTimersL  failed  err: %d" ), err ) );
            }
        else
            {
            iLightsCurrentlyOn = ETrue;
            iLastLightsOnTime.HomeTime(); 
            }
        }
    else
        {
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetDisplayLightsOnWithoutResettingInactivityTimersL - LIGHTS ON denied - Not necessary" ) ) );
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
#endif // RD_LIGHT_CONTROL_CHANGE   
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::TimerExpiredL()
// ----------------------------------------------------------------------------

void CSysApLightsController::TimerExpiredL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::TimerExpiredL" ) ) );
    
#ifdef RD_LIGHT_CONTROL_CHANGE    
    iLightPluginHandler->HandleEvent( SysApLightExtension::ELightsOffTimerStop, TPckgBuf<TBool>(ETrue) );
#endif // RD_LIGHT_CONTROL_CHANGE     
    
    SetLightsOffL();
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ReserveLightL()
// ----------------------------------------------------------------------------

void CSysApLightsController::ReserveLightL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::ReserveLightL: iReserved=%d" ), iReserved ) );
    
    if ( !iReserved )
        {
        iLight->ReserveLightL( CHWRMLight::ESystemTarget, EFalse, ETrue);
        iReserved = ETrue;
        }
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ReleaseLight()
// ----------------------------------------------------------------------------

void CSysApLightsController::ReleaseLight()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::ReleaseLight(): iCallRinging=%d, iAlarmAlerting=%d, iForcedLightsOn=%d" ),
                           iCallRinging, iAlarmAlerting, iForcedLightsOn ) );
    
    if ( !Alerting() && !iForcedLightsOn)
        {
        iLight->ReleaseLight(CHWRMLight::ESystemTarget);
        iReserved = EFalse;
        }
    }
    
// ----------------------------------------------------------------------------
// CSysApLightsController::AllowLightsOn()
// ----------------------------------------------------------------------------

void CSysApLightsController::AllowLightsOn()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::AllowLightsOn: iAllowLightsOn=%d" ), iAllowLightsOn ) );
    
    iAllowLightsOn = ETrue;
  
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::SetFlipOpenNoLights( const TBool aOpen, const TBool aFetchFlipStatus )
// ----------------------------------------------------------------------------
void CSysApLightsController::SetFlipOpenNoLights( const TBool aOpen, const TBool aFetchFlipStatus )
    {
    // if aReadFlipStatus is ETrue, aOpen is ignored and flip status is read directly from P&S key
    TRACES( RDebug::Print( _L("CSysApLightsController::SetFlipOpenNoLights( aOpen:%d, aFetchFlipStatus:%d )"), 
                                                                            aOpen, aFetchFlipStatus ) ); 
    if ( aFetchFlipStatus )
        {
        TInt err;
        TInt flipStatus;
        err = RProperty::Get( KPSUidHWRM, KHWRMFlipStatus, flipStatus );
        if ( err )
            {
            TRACES( RDebug::Print( _L("CSysApLightsController::SetFlipOpenNoLights. RProperty::Get: err=%d."), err ) );
            return;
            }
        if ( flipStatus == EPSHWRMFlipOpen || flipStatus == EPSHWRMFlipStatusUninitialized )
            {
            iFlipOpen = ETrue;
            }
        TRACES( RDebug::Print( _L("CSysApLightsController::SetFlipOpenNoLights: KHWRMFlipStatus read, flipStatus=%d"), flipStatus ) );          
        }
    else
        {
        iFlipOpen = aOpen;
        }
    }
    

// ----------------------------------------------------------------------------
// CSysApLightsController::TurnOffInactiveTargetsL
// ----------------------------------------------------------------------------
//
void CSysApLightsController::TurnOffInactiveTargetsL()
    {
    TRACES( RDebug::Print( _L("CSysApLightsController::TurnOffInactiveTargetsL: iFlipOpen=%d, iGripOpen=%d, iCoverDisplaySupported=%d"),
                            iFlipOpen, iGripOpen, iCoverDisplaySupported ) );         
   
    TInt turnoffTarget = CHWRMLight::ENoTarget;

    // Then turn off inactive lights depending on device configuration
    // Flip state is interesting only when cover display is supported
    if ( iCoverDisplaySupported )
        {
        if ( iFlipOpen )
            {
            turnoffTarget = CHWRMLight::ESecondaryDisplayAndKeyboard;
            }
        else
            {
            turnoffTarget = CHWRMLight::EPrimaryDisplayAndKeyboard;
            }
        }
    else
        {
        if ( !iFlipOpen )
            {
            turnoffTarget = CHWRMLight::EPrimaryKeyboard;
            } 
        }
        
    if ( iGripOpen )
        {
        // No action, grip is open or not supported
        }
    else
        {
        turnoffTarget |= CHWRMLight::EPrimaryKeyboard;
        }
    
    // Don't try to turn off unsupported targets
    turnoffTarget &= iLight->SupportedTargets();
 
    if ( turnoffTarget != CHWRMLight::ENoTarget )
        {
        TRACES( RDebug::Print( _L("CSysApLightsController::TurnOffInactiveTargetsL: turnoffTarget=0x%x"), turnoffTarget ) ); 

#ifdef RD_LIGHT_CONTROL_CHANGE
        if ( iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandOff ) )
            {
            iLightsCurrentlyOn = EFalse; // ensure that forthcoming commands set lights on
            return;
            }
#endif // RD_LIGHT_CONTROL_CHANGE 

        TRAPD(err, iLight->LightOffL( turnoffTarget, KHWRMInfiniteDuration, EFalse ) );

        if ( err != KErrNone )
            {
            TRACES( RDebug::Print(_L("CSysApLightsController::TurnOffInactiveTargetsL: Turning off inactive targets failed") ) );
            }
        } 

    iLightsCurrentlyOn = EFalse;    
    }
          

// ----------------------------------------------------------------------------
// CSysApLightsController::SetLockedStatePrimaryLightsL()
// ----------------------------------------------------------------------------

void CSysApLightsController::SetLockedStatePrimaryLightsL()
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::SetLockedStatePrimaryLightsL"  ) ) );
    
    TInt err(KErrNone);
    
    TInt target = ResolveLockedStateLightTarget();

    // Turn lights on if allowed by CSysApAppUi (to reduce boottime calls).
    if ( iAllowLightsOn && (target != CHWRMLight::ENoTarget) )
        {
        TInt intensity = iSysApAppUi.SysApFeatureManager().LockedStateLightsIntensity();
        
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetLockedStatePrimaryLightsL - LIGHTS ON: target=%d intensity=%d"), target, intensity ) );
 
#ifdef RD_LIGHT_CONTROL_CHANGE
        if ( iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandOn ) )
            {
            return;
            }
#endif // RD_LIGHT_CONTROL_CHANGE 
        TRAP(err, iLight->LightOnL( CHWRMLight::ESystemTarget, // always use system target, resolved target is used only for denying light command
                                    KHWRMInfiniteDuration,
                                    intensity,
                                    ETrue ) );
        }
    else
        {
        TRACES( RDebug::Print( _L( "CSysApLightsController::SetLockedStatePrimaryLightsL LIGHTS ON denied - Not necessary" ) ) );
#ifdef RD_LIGHT_CONTROL_CHANGE
        iLightPluginHandler->HandleCommand( SysApLightExtension::ELightCommandNoAction );
#endif // RD_LIGHT_CONTROL_CHANGE         
        }


    }

// ----------------------------------------------------------------------------
// CSysApLightsController::ResolveLockedStateLightTarget()
// ----------------------------------------------------------------------------
    
TInt CSysApLightsController::ResolveLockedStateLightTarget() const
    {
    TRACES( RDebug::Print( _L( "CSysApLightsController::ResolveLockedStateLightTarget" ) ) );
    
    // This is a bit questionable whether SysAp should take any considerations about the target now when
    // system target is in use. However, alternative way would be to define a new API into HWRM for handling this situation.
    TInt target(CHWRMLight::ESystemTarget); // always allow system target, except when a device with Cover UI has flip closed
    
    if ( iCoverDisplaySupported && !iFlipOpen ) 
        {
        target = CHWRMLight::ENoTarget;
        }
    
    return target;    
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::GetHwrmLight()
// ----------------------------------------------------------------------------
//   
CHWRMLight* CSysApLightsController::GetHwrmLight()
    { 
    return iLight;
    }

// ----------------------------------------------------------------------------
// CSysApLightsController::Alerting()
// The device is alerting, either due to a call or an alarm
// ----------------------------------------------------------------------------
//  
TBool CSysApLightsController::Alerting() const
    {
    return iAlarmAlerting || iCallRinging;
    }

#ifdef RD_LIGHT_CONTROL_CHANGE
// ----------------------------------------------------------------------------
// CSysApLightsController::KeyEventForwardingReady()
// 
// ----------------------------------------------------------------------------
//  
void CSysApLightsController::KeyEventForwardingReady()
    {
    iSysApAppUi.ActivateKeyeventForwardingForLights( ETrue );
    }
#endif // RD_LIGHT_CONTROL_CHANGE

// End of File






