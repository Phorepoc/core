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
* Description:  CSysApAppUi implementation.
*
*/


// INCLUDES

#include "../../../src/phedevmenu/phedevmenu.h"	// PHOREPOC: UI shell keys + PheShellsInRom()
#include<akndiscreetpopup.h>
#include <avkon.hrh>
#include <avkon.rsg>
#include <eikon.hrh>
#include <apgwgnam.h>
#include <eiksrv.h>
#include <eikdll.h>
#include <AknSmallIndicator.h>
#include <AknBatteryNotify.h>
#include <AknSignalNotify.h>
#include <AknGlobalNote.h>
#include <AknGlobalListQuery.h>
#include <aknsoundsystem.h>
#include <apgcli.h>

#include <hwrmpowerstatesdkpskeys.h>
#include <hwrmdomainpskeys.h>

#include <PSVariables.h>
#include "CoreApplicationUIsPrivatePSKeys.h"
#include <ctsydomainpskeys.h>
#include <startupdomainpskeys.h>
#include <startupdomaincrkeys.h>

#ifndef SYMBIAN_ENABLE_SPLIT_HEADERS
  #include <ASShdAlarm.h>
#else
  #include <asshddefs.h>
  #include <ASShdAlarmCal.h>
  #include <asshdalarm.h>
#endif //SYMBIAN_ENABLE_SPLIT_HEADERS

#include <MProfileEngine.h>
#include <MProfileName.h>
#include <MProfilesNamesArray.h>
#include <MProfile.h>
#include <StringLoader.h>
#include <secuisystemlock.h>
#include <secui.h>
#include <settingsinternalcrkeys.h>

#include <AknNotifierController.h>
#include <eikappui.h>
#include <es_enum.h>
#include <data_caging_path_literals.hrh>
#include <eikon.hrh>

#include "SysApSimChanged.h"

#include <SysAp.rsg>
#include "SysApLightsController.h"

#include "SysApPubSubObserver.h"
#include "SysApProfileObserver.h"

#include "sysapcenreplightsettingsobserver.h"
#include "SysApCenRepLogsObserver.h"
#include "SysApCenRepBTObserver.h"
#include "SysApCenRepHacSettingObserver.h"
#include "SysApCenRepController.h"
#include "CoreApplicationUIsPrivateCRKeys.h"
#include "CoreApplicationUIsPrivatePSKeys.h"
#include <UikonInternalPSKeys.h>

#include "SysApStartupController.h"
#include "SysApConnectionMonitorObserver.h"
#include "SysApPowerKeyMenuObserver.h"
#include "SysApSsSettingsObserver.h"
#include "SysApAppUi.h"
#include "SysApShutdownImage.h"
#include "SysApEtelConnector.h"

#include "SysApApp.h"
#include "SysApMsgSimMemLowQuery.h"
#include "SysApNspsHandler.h"
#include "SysApWaitNote.h"
#include "SysApConfirmationQuery.h"
#include "SysApFeatureManager.h"
#include "SysApSubscriber.h"
#include "MSysApOfflineModeController.h"
#include "MSysApBtController.h"
#include "MSysApBtSapController.h"
#include "MSysApLocationPrivacyIndicator.h"
#include "MSysApUsbIndicator.h"

#include <sysap.mbg>

#include "SysApShutdownAnimation.h"

#include <hwrmfmtx.h>
#include <hwrmfmtxdomainpskeys.h>
#include <hwrmfmtxdomaincrkeys.h>
#include "sysapcenrepfmtxobserver.h"

#include "SysApKeySndHandler.h"

#include <ir_sock.h> // KIrdaPropertyCategory // KIrdaStatus

#include <NetworkHandlingDomainPSKeys.h>

// POC launching
#include <AiwServiceHandler.h>
#include <AiwPoCParameters.h>

#include <sacls.h>  // KUidPhonePwr

#include <wlaninternalpskeys.h>  // WLan indicator

#include <ecom/ecom.h>
#include "sysapkeymanagement.h"

#include "SysApMediatorObserver.h"

#include <secondarydisplay/SecondaryDisplaySysApAPI.h>
#include "aknSDData.h"

#include <AknTaskList.h>
#include <layoutmetadata.cdl.h>

#include "sysapdefaultkeyhandler.h"
#include "sysapaudioroutingobserver.h"
#include "sysapcenrepcallforwardingobserver.h"
#include "sysapcenrepmsgwaitingobserver.h"

#ifdef RD_MULTIPLE_DRIVE
 #include "sysapdrivelist.h"
 #include "sysapdriveunlockhandler.h"
 #include "sysapdriveejecthandler.h"
#endif // RD_MULTIPLE_DRIVE

#include "sysappsmcontroller.h"
#include "sysapbatteryinfocontroller.h"

#include <keylockpolicyapi.h>

#include <UsbWatcherInternalPSKeys.h> // USB transfer modes
#include <usbpersonalityids.h>
#include <smsuaddr.h>
#include <featmgr.h>
#include <DevEncProtectedPSKey.h>

// ---- PHOREPOC delta (2026-10-15): POWER KEY OPTIONS (user) --------------------------------------------------------------
// Phones today have no lock switch: the short and the long press of the power key each run a configurable action, read
// from the board repository 0xE70157B0 at every press (a change applies at once): key 1 = SHORT, key 2 = LONG;
// 0 = the power menu (the stock short press), 1 = lock / unlock (keylock toggle), 2 = switch off (the stock long press).
// Defaults: short = lock/unlock, long = the power menu. A short action other than the menu runs at KEY-UP (a long press
// must not also run it); a press while the keys are locked (the keylock notifier holds the key and sends
// EEikKeyLockPowerKeyPressed) unlocks when the short action is lock/unlock.
#include <centralrepository.h>
const TUid KCRUidPhPowerKey = { 0xE70157B0 };
enum { EPhPkMenu = 0, EPhPkLock = 1, EPhPkOff = 2 };
static TInt PhPowerKeyAction( TBool aLong )
    {
    TInt v = aLong ? EPhPkMenu : EPhPkLock;
    CRepository* rep = NULL;
    TRAPD( err, rep = CRepository::NewL( KCRUidPhPowerKey ) );
    if ( !err && rep )
        {
        rep->Get( aLong ? 2 : 1, v );
        delete rep;
        }
    return v;
    }
static TBool PhUnlockedThisPress = EFalse;		// the key-up after an unlocking press must not lock again
// the hold timing (user, 2026-10-15): the long action at 2 s while the key is still held, switch off at 5 s (as the menu's
// Switch off); the PMIC's own 10 s power-off stays. Timed from the press on the window server's key repeats.
const TInt KPhPkLongUs = 2000000, KPhPkOffUs = 5000000;
static TTime PhPressTime;
static TInt PhHoldStage = 0;
static TBool PhPressSeen = EFalse;	// this press's first event reached us (2026-10-18: see the EKeyPowerOff repeat branch)		// 0 = short so far, 1 = the long action ran, 2 = switching off
static TInt PhRestartSelection = KErrNotFound;	// the power menu's "Restart" index (PHOREPOC, 2026-10-15)
// the restart runs from the active scheduler AFTER the menu callback: DoShutdownL( ETrue ) frees SysAp's resources at
// once, and PowerKeyPopUpMenuSelectionDoneL then touched a freed member (boot 481: SysAp KERN-EXEC 3 at pc 0x50,
// sysmon CMonitor 8 -> kernel fault -> golden A)
static CAsyncCallBack* PhRestartCb = NULL;
// PHOREPOC (2026-10-18): "Switch to Symbian^3 / ^4" in the power menu when the image carries both UI shells -- the way back
// from S^4, which has no S^3 dialer for the developer menu's secret code. Writes the dev menu's key 1, then restarts
// (pheshellsel picks the shell at boot; src/phedevmenu/phedevmenu.h)
static TInt PhShellSelection = KErrNotFound;
static TInt PhShellTarget = 0;
static TInt PhDoRestart( TAny* aAppUi )
    {
    TRAP_IGNORE( static_cast<CSysApAppUi*>( aAppUi )->DoShutdownL( ETrue, RStarterSession::ELanguageSwitchReset ) );
    return KErrNone;
    }

// CONSTANTS
const TInt KPowerKeyMenuSelectionCancelled( -1 );
const TInt KPowerKeyMenuSelectionSwitchOff( 0 );
const TInt KModifierMask( 0 );
const TInt KDelayBeforeShuttingDownInAlarmAndChargingStates( 500000 );
const TInt KDelayBeforeShowingGprsSuspendedNote( 500000 );
const TInt KDelayBeforeNextScanningRound( 1000000 );
const TInt KActiveProfile( 0 ); //The value of ActiveProfile for accessory default profile selection in GS/SD.
const TInt KProfileListInitialIndex( 0 );
const TInt KMaxProfileNameLength( 64 );
const TInt KTimeToShowShutdownNote( 2000000 ); //2 seconds time for Goodbye note to show before being shutdown
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
const TInt KCoefficientToMakeMicroToMilliSeconds = 1000;
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

/* ENABLE ANIMATION: Add id of background image.
   Example: const TInt KBackgroundImageID = EMbmSysapQgn_graf_startup_bg;
   If there is no image defined, clear screen is used.*/
const TInt KBackgroundImageID = 0;
#ifndef RD_MULTIPLE_DRIVE
const TInt32 KAutolockAppUid = 0x100059B5;
const TInt KFileManagerAppUid = 0x101F84EB;
#endif // RD_MULTIPLE_DRIVE
const TInt KCoefficientKhzToMhz = 1000;
const TInt KDecimalsInMhzFrequency = 2;

const TInt KHWSwitchGroup( 29 );
const TInt KHWSwitchGrip( 2 );

_LIT_SECURITY_POLICY_PASS(KAlwaysPassPolicy);
_LIT_SECURITY_POLICY_C1(KWriteDeviceDataPolicy, ECapabilityWriteDeviceData);

// ============================ MEMBER FUNCTIONS ==============================

// ----------------------------------------------------------------------------
// CSysApAppUi::CSysApAppUi()
// ----------------------------------------------------------------------------

CSysApAppUi::CSysApAppUi() :
    iSysApCenRepLogsObserver( NULL ),
    iSysApCenRepHacSettingObserver( NULL ),
    iSysApPowerKeyMenuObserver( NULL ),
    iSysApShutdownImage( NULL ),
    iSysApConfirmationQuery( NULL ),
    iSysApConfirmationQueryForRestart( NULL ),
    iSignalNotify( NULL ),
#ifndef RD_MULTIPLE_DRIVE
    iMemoryCardDialog( 0 ),
#endif // RD_MULTIPLE_DRIVE
    iPowerKeyPopupMenuActive( EFalse ),
    iDisablePowerkeyMenu( EFalse ),
    iDeviceLockEnabled( EFalse ),
    iKeyLockEnabled( EFalse ),
    iShowkeypadActivatedNoteAfterSoftReject( EFalse ),
    iEmergencyCallActive( EFalse ),
    iSimChangedDone( EFalse ),
    iResourcesFreed( EFalse ),
#ifndef RD_MULTIPLE_DRIVE
    iHideFirstBeep( ETrue ),
#endif // RD_MULTIPLE_DRIVE
    iShutdownStarted( EFalse ),
    iIgnoreAccessorySpecificProfileChanges( ETrue ),
    iAccessoryJustConnected( EFalse ),
    iHandleNextProfileEvent( ETrue ),
    iActivateBt( EFalse ),
    iDeactivateBt( EFalse ),
    iGprsActivated( EFalse ),
    iGprsSuspended( EFalse ),
    iCallActivated( EFalse ),
    iGprsSuspendedNoteShown( EFalse ),
    iMMCEjectUsed( EFalse ),
#ifndef RD_MULTIPLE_DRIVE
    iMMCPowerMenuEjectUsed( EFalse ),
    iMMCInserted( EFalse ),
#endif // RD_MULTIPLE_DRIVE
    iTimer( NULL ),
    iSapTimer( NULL ),
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    iAnimTimer( NULL ),
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
    iSysApTimer( NULL ),
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    iAnimationShowingTime( 0 ),
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
    iShutdownContinued( EFalse ),
    iSysApAudioRoutingObserver( NULL ),
    iSysApCenRepCallForwardingObserver( NULL ),
    iSysApCenRepMsgWaitingObserver( NULL ),
	iKeyBoardRepeatCount(-1)
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CSysApAppUi()") ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ConstructL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ConstructL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: START") ) );
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying BaseConstructL( EAknEnableSkin )") ) );
    BaseConstructL( EAknEnableSkin );
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: BaseConstructL() OK") ) );
    
 
    /*SysAp is set as system application (Symbian terminology). This means some special privilege compared
      to other applications. For example it does not get closed when system is asked to close applications
    */
    iEikonEnv->SetSystem( ETrue );
	// For cdma 

	iTDEnable = FeatureManager::FeatureSupported( KFeatureIdFfTdScdma ); 

    iEikonEnv->WsSession().ComputeMode( RWsSession::EPriorityControlDisabled );

    static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PushContextL( R_AVKON_SILENT_SKEY_LIST );

    RThread().SetProcessPriority( EPriorityForeground );

    /*SysAp does not have any visible Status pane. An exception will be Charging state
    where SysAp shows Status Pane as it is the only running application.
    */
    TInt mode;
    User::LeaveIfError( RProperty::Get( KPSUidStartup, KPSGlobalStartupMode, mode ) );

    if( mode == EStartupModeCharging || mode == EStartupModeAlarm )
        {
        SetStatusPaneLayoutL(
            mode == EStartupModeCharging ? ESysApCharging : ESysApAlarm );

        SetKeyEventFlags(
            CAknAppUiBase::EDisableSendKeyShort |
            CAknAppUiBase::EDisableSendKeyLong );
        }
    else
        {
        SetStatusPaneLayoutL( ESysApNormal );
        }

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApFeatureManager::NewL()") ) );
    iSysApFeatureManager = CSysApFeatureManager::NewL();

    // Setup USB charging detector
    iSysApUsbChargerDetector.EnableUsbCharging(
        iSysApFeatureManager->Supported( KSysApFeatureIdChargerReminderNotes ) &&
        iSysApFeatureManager->Supported( KSysApFeatureIdUsbChargingWithoutReminderNotes ) );

    // Construct notifiers that are used frequently or in OOM situations.
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CAknBatteryNotify::NewL()") ) );
    iBatteryNotify = CAknBatteryNotify::NewL();
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CAknSignalNotify::NewL()") ) );
    iSignalNotify = CAknSignalNotify::NewL();
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying new ( ELeave ) CSysApMsgSimMemLowQuery") ) );
    iSysApMsgSimMemLowQuery = new( ELeave ) CSysApMsgSimMemLowQuery( iSysApFeatureManager->CoverDisplaySupported() );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApPubSubObserver::NewL()") ) );
    iSysApPubSubObserver = CSysApPubSubObserver::NewL( *this );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsPoCIndicator, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsPoCIndicator, ECoreAppUIsPocIndicatorUninitialized );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsPoCMissedIndicator, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsPoCMissedIndicator, ECoreAppUIsPocMissedIndicatorUninitialized );
    
    // initial value won't be set to KCoreAppUIsUSBFileTransfer, because UsbMscPersonality-plugin may also define it if USB cable is connected during boot
    // remove this definition after all clients have taken the USB personality ids in use
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsUSBFileTransfer, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsVideoSharingIndicator, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsVideoSharingIndicator, ECoreAppUIsVideoSharingIndicatorUninitialized );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApAccessoryObserver::NewL()") ) );
    iSysApAccessoryObserver = CSysApAccessoryObserver::NewL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApCenRepController::NewL()") ) );
    iSysApCenRepController = CSysApCenRepController::NewL();

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApCenRepLightSettingsObserver::NewL") ) );
    iSysApCenRepLightSettingsObserver = CSysApCenRepLightSettingsObserver::NewL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApCenRepBtObserver::NewL") ) );
    iSysApCenRepBtObserver = CSysApCenRepBtObserver::NewL( *this );

    if ( iSysApFeatureManager->FmTxSupported() )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApCenRepFmTxObserver::NewL") ) );        
        iSysApCenRepFmTxObserver = CSysApCenRepFmTxObserver::NewL( *this );    
        }

    // Define P&S keys "owned" by SysAp
    RProperty::Define( KPSUidUikon, KUikMMCInserted, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    //initially set the value as 0 assuming mmc is not inserted
    RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
    TDriveInfo driveInfo;
    TInt driveNumber; 
    TInt err;    
    RFs& fileServer = iEikonEnv->FsSession();  
    for ( driveNumber = EDriveD; driveNumber < EDriveZ; driveNumber++ )
         {
	  err = fileServer.Drive(driveInfo,driveNumber);
          if(driveNumber==EDriveF && err == KErrNone && driveInfo.iType == EMediaHardDisk &&  driveInfo.iDriveAtt & KDriveAttRemovable)     
        	{     
          TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: err = %d, driveInfo.iType = %d, driveInfo.iDriveAtt %d, KDriveAttRemovable = %d "),err,driveInfo.iType,driveInfo.iDriveAtt,KDriveAttRemovable) );     
        	RProperty::Set( KPSUidUikon, KUikMMCInserted, 1 );
               break;  // Memory card drive found...     
      		}
         } 

    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsHideAlarm, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsDisableKeyguard, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsSoftReject, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsUipInd, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsMessageToneQuit, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsNspsRawKeyEvent, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsLightsRawKeyEvent, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsMmcRemovedWithoutEject, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KLightsAlarmLightActive, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KLightsVTForcedLightsOn, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KLightsSSForcedLightsOn, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KLightsATCForcedLightsOn, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KLightsControl, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsTarmIndicator, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsMtvRecStatus, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsMtvDvbhStatus, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );
    RProperty::Define( KPSUidCoreApplicationUIs, KCoreAppUIsNewEmailStatus, RProperty::EInt, KAlwaysPassPolicy, KWriteDeviceDataPolicy );

    // Initialize P&S keys "owned" by SysAp
    RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsHideAlarm, ECoreAppUIsHideAlarmUninitialized );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApStartupController::NewL()") ) );
    iSysApStartupController = CSysApStartupController::NewL( *this, iSysApFeatureManager->OfflineModeSupported() );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApLightsController::NewL()") ) );
    iSysApLightsController = CSysApLightsController::NewL( *this,
                                                           iSysApCenRepLightSettingsObserver->GetLightsTimeout(),
                                                           iSysApFeatureManager->CoverDisplaySupported() );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApEtelConnector::NewL()") ) );
    iSysApEtelConnector = CSysApEtelConnector::NewL( *this );

    //SysAp needs to capture PowerKey events because it handles all functionality related to that
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CEikApplication::RootWin()") ) );
    RWindowGroup groupWin = iCoeEnv->RootWin();
    User::LeaveIfError ( iCapturedEKeyPowerOff = groupWin.CaptureKey( EKeyPowerOff, KModifierMask, KModifierMask ) );
    User::LeaveIfError ( iCapturedEKeyPowerOffUpAndDowns = groupWin.CaptureKeyUpAndDowns( EStdKeyDevice2, KModifierMask, KModifierMask ) );

    TRACES ( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApDefaultKeyHandler::NewL()") ) );
    iSysApDefaultKeyHandler = CSysApDefaultKeyHandler::NewL( *this );

    TInt status(0);
    err = UserSvr::HalFunction( KHWSwitchGroup, KHWSwitchGrip, &status, 0 );
    if( err == KErrNone )
	    {
	    if( status == 0 )
		    {
	        RProperty::Set( KPSUidHWRM, KHWRMGripStatus, EPSHWRMGripOpen );
		    }
	    else
		    {
		    RProperty::Set( KPSUidHWRM, KHWRMGripStatus, EPSHWRMGripClosed );
		    }
	    }
     else
	    {
	    TRACES ( RDebug::Print( _L("CSysApAppUi::ConstructL: error in getting slide status: err = %d"), err ) );
	    }

#ifndef RD_MULTIPLE_DRIVE
    if ( iSysApFeatureManager->MmcSupported() )
        {
        iSysApMMCObserver = CSysApMMCObserver::NewL( this, &iEikonEnv->FsSession(), iSysApFeatureManager->MmcHotSwapSupported() );
        }
#else // RD_MULTIPLE_DRIVE
    iSysApDriveList = CSysApDriveList::NewL( iEikonEnv->FsSession() );
    if ( iSysApFeatureManager->MmcSupported() )
        {
        iSysApMMCObserver = CSysApMMCObserver::NewL(
            iEikonEnv->FsSession(), *iSysApDriveList, *this, iSysApFeatureManager->MmcHotSwapSupported() );
        iSysApDriveUnlockHandler = CSysApDriveUnlockHandler::NewL(
            *iSysApDriveList, *this, iSysApFeatureManager->MemoryCardLockSupported() );
        iSysApDriveEjectHandler = CSysApDriveEjectHandler::NewL(
            *iSysApDriveList, *this, iEikonEnv->FsSession() );
        }
#endif // RD_MULTIPLE_DRIVE

    //Instantiate the KEF plugin manager
	//Trap constuction, since Sysap may live without iSysApKeyManagement
    TRAPD(keyManagementErr, iSysApKeyManagement=CSysApKeyManagement::NewL(CCoeEnv::Static()->RootWin(), *this));

    if (keyManagementErr)
    	{
    	TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL : CSysApKeyManagement::NewL returns error=%d"), keyManagementErr ) );
    	}

    iSysApFeatureManager->FeatureVariationCheckDone();

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApProfileObserver::NewL()") ) );
    iSysApProfileObserver = CSysApProfileObserver::NewL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL : trying MProfileEngine* CreateProfileEngineL()") ) );
    iProfileEngine = CreateProfileEngineL( &( iEikonEnv->FsSession() ) );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL : trying CreateSysApOfflineModeControllerL()") ) );
    iSysApOfflineModeController = CreateSysApOfflineModeControllerL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL : trying DoInitialSwStateCheckL()") ) );
    iSysApStartupController->DoInitialSwStateCheckL();

    iActiveProfileBeforeOfflineMode = iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileBeforeOfflineMode );

    // Initialize animdll for handling side volume keys
    // (needed before normal mode in case emergency number is dialed from PIN query)
    iSysApKeySndHandler = CSysApKeySndHandler::NewL(iEikonEnv->WsSession());

    // Initialize shutdown image (needed by nspshandler)
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApShutdownImage::NewL()") ) );
    iSysApShutdownImage = CSysApShutdownImage::NewL();

    // Initialize nsps handler. Needed for proper lights control during PIN query.
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: trying CSysApNspsHandler::NewL") ) );
    iSysApNspsHandler = CSysApNspsHandler::NewL( iEikonEnv->WsSession(), iSysApShutdownImage->ShutdownCoeControlWindow() );

#ifdef RD_LIGHT_CONTROL_CHANGE
    // NSPS Plugin has been loaded, inform lights controller that in can activate key event forwarding
    iSysApLightsController->KeyEventForwardingReady();
#endif // RD_LIGHT_CONTROL_CHANGE    

    // Create HAC setting observer now because telephony state may change before entering to normal state
    TRACES( RDebug::Print( _L("CCSysApAppUi::ConstructL  trying CSysApCenRepHacSettingObserver::NewL") ) );
    iSysApCenRepHacSettingObserver = CSysApCenRepHacSettingObserver::NewL( *this );
            
    TRACES( RDebug::Print( _L("CSysApAppUi::ConstructL: END") ) );
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::DeactivatePSMifBatteryNotLow()
// ----------------------------------------------------------------------------
 void CSysApAppUi::DeactivatePSMifBatteryNotLowL ()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::DeactivatePSMifBatteryNotLow: Start") ) );
    if ( iSysApFeatureManager->PowerSaveSupported()) //&& iSysApFeatureManager->Supported( KSysApFeatureIdBatteryInfoPopup ))
        {
        // Create batteruInfoController to get current battery status;
	if(iSysApBatteryInfoController == NULL)
	    {
	    iSysApBatteryInfoController = CSysApBatteryInfoController::NewL( 
                                                        iSysApCenRepController->GetInt( 
                                                                KCRUidCoreApplicationUIsConf,
                                                                KCoreAppUIsBatteryInformationLowThreshold ) );
	    }
        //Querry the battery level 
        TBool status = iSysApBatteryInfoController->IsBatteryInfoAboveThreshold();
        TRACES( RDebug::Print( _L("CCSysApAppUi::DeactivatePSMifBatteryNotLow  IsBatteryInfoAboveThreshold=%d"), status ) );
        // Querry to deactivate PSM if PSM is activated and battery status is above threshold
        if (status)
            {
			if(iSysApPsmController == NULL)
				{
				iSysApPsmController = CSysApPsmController::NewL( *this );
				}
            if ( iSysApPsmController->ShowDeactivateQuery())
                ShowQueryL( ESysApBattChargingPowerSavingQuery );
            else
                iSysApPsmController->DoEnablePartialPsm( EFalse );
            }
        }
    TRACES( RDebug::Print( _L("CSysApAppUi::DeactivatePSMifBatteryNotLow: End") ) );
    } 

// ----------------------------------------------------------------------------
// CSysApAppUi::~CSysApAppUi()
// ----------------------------------------------------------------------------

CSysApAppUi::~CSysApAppUi()
    {
    TRACES( RDebug::Print( _L("~CSysApAppUi() started") ) );
    if( !iResourcesFreed )
        {
        FreeResources();

        iStarterSession.Close();

        }

    delete iSysApShutdownImage;

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    if (iSysApShutdownAnimation)
        {
        RemoveFromStack( iSysApShutdownAnimation );
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
        delete iSysApShutdownAnimation;
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
        }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

  

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    if( iAnimTimer )
        {
        iAnimTimer->Cancel();
        }
    delete iAnimTimer;
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

    delete iSysApPowerKeyMenuObserver;

    delete iSysApStartupController;

#ifdef RD_MULTIPLE_DRIVE
    iInsertedMemoryCards.Close();
    delete  iSysApDriveEjectHandler;
    delete iSysApDriveUnlockHandler;
    delete iSysApDriveList;
#endif // RD_MULTIPLE_DRIVE

    TRACES( RDebug::Print( _L("~CSysApAppUi() completed") ) );
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::HandleLocationPrivacyIndicatorL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleLocationPrivacyIndicatorL( const TInt aState )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleLocationPrivacyIndicatorL() state=%d"), aState ) );
    if ( iSysApFeatureManager->LocationPrivacySupported() )
        {
        // Phase 1. Set all Location Privacy indicators off
        if ( iSysApLocationPrivacyIndicator->iIndicatorIdAcceptAll != KSysApNoIndicator )
            {
            SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdRejectAll, EAknIndicatorStateOff );
            }
        if ( iSysApLocationPrivacyIndicator->iIndicatorIdRejectAll != KSysApNoIndicator )
            {
            SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdRejectAll, EAknIndicatorStateOff );
            }
        if ( iSysApLocationPrivacyIndicator->iIndicatorIdAlwaysAsk != KSysApNoIndicator )
            {
            SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdAlwaysAsk, EAknIndicatorStateOff );
            }
        if ( iSysApLocationPrivacyIndicator->iIndicatorIdIndividualPrivacy != KSysApNoIndicator )
            {
            SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdIndividualPrivacy, EAknIndicatorStateOff );
            }
        // Phase 2. Set the required Location Privacy indicator on
        switch ( aState )
            {
            case EPSLocPrivAcceptAll:
                if ( iSysApLocationPrivacyIndicator->iIndicatorIdAcceptAll != KSysApNoIndicator )
                    {
                    SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdAcceptAll, EAknIndicatorStateOn );
                    }
                break;
            case EPSLocPrivRejectAll:
                if ( iSysApLocationPrivacyIndicator->iIndicatorIdRejectAll != KSysApNoIndicator )
                    {
                    SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdRejectAll, EAknIndicatorStateOn );
                    }
                break;
            case EPSLocPrivAlwaysAsk:
                if ( iSysApLocationPrivacyIndicator->iIndicatorIdAlwaysAsk != KSysApNoIndicator )
                    {
                    SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdAlwaysAsk, EAknIndicatorStateOn );
                    }
                break;
            case EPSLocPrivIndividualPrivacy:
                if ( iSysApLocationPrivacyIndicator->iIndicatorIdIndividualPrivacy != KSysApNoIndicator )
                    {
                    SetIndicatorStateL( iSysApLocationPrivacyIndicator->iIndicatorIdIndividualPrivacy, EAknIndicatorStateOn );
                    }
                break;
            default:
                break;
            }
        }
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType )
// ----------------------------------------------------------------------------

TKeyResponse CSysApAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType )
    {
    if ( ! iShutdownStarted )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::HandleKeyEventL: aKeyEvent.iCode:%d, aKeyEvent.iScanCode:%d, aType:%d, iIgnoreNextPowerKeyUpEvent:%d, iPowerKeyPopupMenuActive:%d, iLastPowerKeyWasShort:%d, iPowerKeyPopupMenuDismissed:%d"),
                               aKeyEvent.iCode, aKeyEvent.iScanCode, aType, iIgnoreNextPowerKeyUpEvent, iPowerKeyPopupMenuActive, iLastPowerKeyWasShort, iPowerKeyPopupMenuDismissed ) );

#ifdef _DEBUG
        // camery latency measurement environment instrumentation, don't remove
        if ( aType == EEventKey && aKeyEvent.iCode == EKeyCamera )
            {
            TRACES( RDebug::Print( _L("e_KEY_EVENT_SENDING 0") ) );
            }
#endif // _DEBUG

        TBool haveStatusPane = ( StatusPane()== NULL ) ? EFalse : StatusPane()->IsVisible();
        TRACES( RDebug::Print( _L("CSysApAppUi::HandleKeyEventL: haveStatusPane = %d" ) ) );
        
        TKeyResponse response( EKeyWasNotConsumed );
        if (iSysApKeyManagement && aKeyEvent.iCode != EKeyPowerOff && aKeyEvent.iCode != 'E')
            {
            response = iSysApKeyManagement->HandleKeyEventL(aKeyEvent, aType );
            }
        
        if( aType == EEventKey )
            {
            switch ( aKeyEvent.iCode )
                {
#ifdef _DEBUG
                case 'E': //For testing
                    Exit();
                    break;
#endif
                case EKeyPowerOff:
                    {
                    // PHOREPOC power key options (see the top of the file)
                    const TInt shortAction = PhPowerKeyAction( EFalse );
                    if( aKeyEvent.iRepeats == 0 )
                        {
                        PhPressTime.UniversalTime();
                        PhHoldStage = 0;
                        PhPressSeen = ETrue;
                        iLastPowerKeyWasShort = ETrue;
                        iIgnoreNextPowerKeyRepeats = EFalse;
                        // the menu: as stock (shown on the press; a press with the menu open moves its selection)
                        if ( shortAction == EPhPkMenu || iPowerKeyPopupMenuActive )
                            {
                            HandleShortPowerKeyPressedL();
                            }
                        }
                    else
                        {
                        TTime now;
                        now.UniversalTime();
                        // PHOREPOC (2026-10-18, boot 1040: a 2.6 s hold on a dark screen switched the phone OFF): with the
                        // display off the press's first event wakes the lights and never reaches us, so PhPressTime was the
                        // PREVIOUS press -- hours old -> the 5 s switch-off at once. The first event we see starts the press.
                        if ( !PhPressSeen )
                            {
                            PhPressTime = now;
                            PhHoldStage = 0;
                            PhPressSeen = ETrue;
                            iLastPowerKeyWasShort = ETrue;
                            }
                        const TInt64 heldUs = now.MicroSecondsFrom( PhPressTime ).Int64();
                        if ( PhHoldStage == 0 && heldUs >= KPhPkLongUs )
                            {
                            PhHoldStage = 1;
                            iLastPowerKeyWasShort = EFalse;
                            const TInt longAction = PhPowerKeyAction( ETrue );
                            if ( longAction == EPhPkOff )
                                {
                                PhHoldStage = 2;
                                HandleLongPowerKeyPressedL();
                                }
                            else if ( longAction == EPhPkLock )
                                {
                                if ( iKeyLockEnabled )
                                    {
                                    KeyLock().DisableWithoutNote();
                                    }
                                else if ( !iDeviceLockEnabled )
                                    {
                                    KeyLock().EnableWithoutNote();
                                    }
                                }
                            else if ( !iGlobalListQuery )
                                {
                                HandleShortPowerKeyPressedL();		// the power menu
                                }
                            }
                        else if ( PhHoldStage == 1 && heldUs >= KPhPkOffUs )
                            {
                            PhHoldStage = 2;
                            if ( iGlobalListQuery )
                                {
                                CancelGlobalListQuery();		// the menu goes; the phone switches off as its Switch off
                                }
                            HandleLongPowerKeyPressedL();
                            }
                        }
                    }
                    break;

                default:
                	{
                	TKeyResponse response = EKeyWasNotConsumed;
                	if (iSysApKeyManagement)
                		{
                		response = iSysApKeyManagement->HandleKeyEventL(aKeyEvent, aType );
                		}

                    if ( response == EKeyWasNotConsumed ) // none of the plugins consumed the key, offer it to default handling
                        {
                        iSysApDefaultKeyHandler->HandleKeyEventL( aKeyEvent, aType );
                        }
                	}
                    break;
                }
            }
        else if ( aType == EEventKeyUp )
            {
            // PHOREPOC power key options: the short action at key-up (lock / switch off; the menu ran at the press)
            if ( aKeyEvent.iScanCode == EStdKeyDevice2 )
                {
                PhPressSeen = EFalse;		// the next press starts fresh (PHOREPOC 2026-10-18)
                if ( PhHoldStage != 0 )
                    {	// a long press: its action ran while held -- the release does nothing (and must not move the menu)
                    PhHoldStage = 0;
                    iLastPowerKeyWasShort = EFalse;
                    return EKeyWasConsumed;
                    }
                if ( PhUnlockedThisPress )
                    {
                    PhUnlockedThisPress = EFalse;
                    iLastPowerKeyWasShort = EFalse;
                    return EKeyWasConsumed;
                    }
                const TInt shortAction = PhPowerKeyAction( EFalse );
                if ( iLastPowerKeyWasShort && !iPowerKeyPopupMenuActive && shortAction != EPhPkMenu )
                    {
                    iLastPowerKeyWasShort = EFalse;
                    if ( shortAction == EPhPkOff )
                        {
                        HandleLongPowerKeyPressedL();
                        }
                    else if ( iKeyLockEnabled && !iDeviceLockEnabled )
                        {
                        // a TOGGLE: the key router lets only SysAp / Autolock capture EKeyPowerOff (KRestrictedKeys), so
                        // the keylock notifier never gets the press while locked -- SysAp unlocks here (boot 467)
                        KeyLock().DisableWithoutNote();
                        }
                    else if ( !iKeyLockEnabled && !iDeviceLockEnabled &&
                              StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) != EPSCTsyCallStateConnected )
                        {
                        KeyLock().EnableWithoutNote();
                        }
                    return EKeyWasConsumed;
                    }
                }
            TRACES( RDebug::Print( _L( "CSysApAppUi::HandleKeyEventL(): aType == EEventKeyUp, PowerKeyIsLockKey = %d, iLastPowerKeyWasShort = %d, iPowerKeyPopupMenuActive = %d, iCharging = %d" ), iSysApFeatureManager->PowerKeyIsLockKey(), iLastPowerKeyWasShort, iPowerKeyPopupMenuActive, iCharging ) );
            if ( !IsDefaultPowerKeyBehavior() && 
                 iLastPowerKeyWasShort && 
                 ( aKeyEvent.iScanCode == EStdKeyDevice2 ))
                {
                // if the power key is the lock key && the last keypress was 
                // short && the power menu is not active && an alarm is not
                // currently being shown, then lock the phone
                TInt alarmState=0, securityQueryState=0;
         		TInt errorCode = RProperty::Get( KPSUidCoreApplicationUIs, KCoreAppUIsDisableKeyguard, alarmState );
				TInt errorCode2 = RProperty::Get( KPSUidStartup, KStartupSecurityCodeQueryStatus, securityQueryState);
            	TRACES( RDebug::Print( _L( "CSysApAppUi::HandleKeyEventL(): Reading value of KCoreAppUIsDisableKeyguard - State Value: %d"),alarmState));
            	TRACES( RDebug::Print( _L( "CSysApAppUi::HandleKeyEventL(): Reading value of KStartupSecurityCodeQueryStatus - State Value: %d"),securityQueryState));
            	TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );
				//Disable keylock if Alarm is active or if a Security code query is active on the display
				if ( alarmState == ECoreAppUIsDisableKeyguard || securityQueryState == ESecurityQueryActive )
                 	{
					KeyLock().DisableWithoutNote();               	
                 	}
			    else
         		 	{
                    if( callState != EPSCTsyCallStateConnected && !iDeviceLockEnabled)
                        {
                        KeyLock().EnableWithoutNote();
                        }
         			}
                }
            else
                {
                if ( aKeyEvent.iScanCode == EStdKeyDevice2 )
                    {
                    if ( iIgnoreNextPowerKeyUpEvent )
                        {
                        if ( !iPowerKeyPopupMenuDismissed ) // If the popup menu has been dismissed, do nothing
                            {
                            iPowerKeyPopupMenuActive = ETrue;
                            iIgnoreNextPowerKeyUpEvent = EFalse;
                            }
                        }
                    else if ( iLastPowerKeyWasShort )
                        {
                        if ( iPowerKeyPopupMenuActive )
                            {
                            if ( iGlobalListQuery )
                                {
                                if ( iSysApFeatureManager->NoPowerKeySupported() )
                                    {
                                    CancelGlobalListQuery();
                                    }
                                else
                                    {
                                    iGlobalListQuery->MoveSelectionDown();
                                    }
                                }
                            }
                        }
                    }
                }
            }

        TRACES( RDebug::Print( _L("CSysApAppUi::HandleKeyEventL:ended, aKeyEvent.iCode:%d, aKeyEvent.iScanCode:%d, aType:%d, iIgnoreNextPowerKeyUpEvent:%d, iPowerKeyPopupMenuActive:%d, iLastPowerKeyWasShort:%d, iPowerKeyPopupMenuDismissed:%d"),
                               aKeyEvent.iCode, aKeyEvent.iScanCode, aType, iIgnoreNextPowerKeyUpEvent, iPowerKeyPopupMenuActive, iLastPowerKeyWasShort, iPowerKeyPopupMenuDismissed ) );
        }
    else // shutdown started
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::HandleKeyEventL: aKeyEvent.iCode:%d, aKeyEvent.iScanCode:%d, aType:%d"),
                               aKeyEvent.iCode, aKeyEvent.iScanCode, aType ) );

        // When shutdown has been started, only device mode key events are passed to plug-in framework
        // This for enabling plug-in activities during shutdown animation
        if ( IsDeviceModeKey( aKeyEvent ) ) // Other key events are not passed, because they must cancel the shutdown animation as stated in UI spefication
            {
            if ( aType == EEventKey && iSysApKeyManagement )
                {
                iSysApKeyManagement->HandleKeyEventL( aKeyEvent, aType );
                }
            }
    #ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION
        else if ( aType == EEventKey && iSysApShutdownAnimation && !iShutdownContinued )
            {
            if ( aKeyEvent.iCode != EKeyPowerOff || iLastPowerKeyWasShort )
                {
                iSysApShutdownAnimation->Cancel();
                NotifyShutdownAnimationSkip();
                }
            }

    #endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

        TRACES( RDebug::Print( _L("CSysApAppUi::HandleKeyEventL: ended") ) );
        }
    return EKeyWasConsumed;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleCommandL( TInt aCommand)
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleCommandL( TInt aCommand )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleCommandL aCommend: %d"), aCommand ) );
    switch ( aCommand )
        {
        case EEikCmdExit:   // EAknCmdExit
	    // Quick fix for Defect: UTUL-7ZQLJU 
            Exit();
            break;
        default:
            break;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleApplicationSpecificEventL(TInt aType,const TWsEvent& aEvent)
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleApplicationSpecificEventL(TInt aType,const TWsEvent& aEvent)
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleApplicationSpecificEventL: aType:%d"), aType ) );

    CAknAppUi::HandleApplicationSpecificEventL(aType, aEvent);
    
    if ( ResourcesFreed() )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::HandleApplicationSpecificEventL: discarded, shutting down") ) );
        return;
        }
    
    switch( aType )
        {
        case EEikKeyLockEnabled:
            iKeyLockEnabled = ETrue;
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApKeyguardActive, 1 );
            SetIndicatorStateL( EAknIndicatorKeyguard, EAknIndicatorStateOn );
            iSysApLightsController->KeylockStateChangedL( ETrue );
            break;
        case EEikKeyLockDisabled:
            iKeyLockEnabled = EFalse;
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApKeyguardActive, 0 );
            SetIndicatorStateL( EAknIndicatorKeyguard, EAknIndicatorStateOff );
            if (! iDeviceLockEnabled )
                {
                iSysApLightsController->KeylockStateChangedL( EFalse );
                if ( iSysApFeatureManager->MmcHotSwapSupported() )
                    {
                    if ( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) !=  EPSCTsyCallStateRinging && StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) !=  EPSCTsyCallStateAlerting )
                        {
                        RunUnlockNotifierL();
                        }
                    }
                }
            break;
        case EEikKeyLockPowerKeyPressed: //sent when power key is captured by keylockserver
            // PHOREPOC power key options: with the short action lock/unlock, a press while locked unlocks
            if ( iKeyLockEnabled && !iDeviceLockEnabled && PhPowerKeyAction( EFalse ) == EPhPkLock )
                {
                KeyLock().DisableWithoutNote();
                PhUnlockedThisPress = ETrue;
                }
            else
                {
                HandleShortPowerKeyPressedL();
                }
            break;

        case EEikKeyLockLightsOnRequest:
            iSysApLightsController->SetLightsOnUnlockNoteL();
            break;

        case EEikEcsQueryLights: // emergency note is shown
            iSysApLightsController->SetLightsOnEcsQueryL();
            break;

        case EEikSecurityQueryLights: // for device lock security query
            iSysApLightsController->SetLightsOnSecurityQueryL();
            break;

        default:
            break;
            }
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::InitializeStatusPaneAreaL()
// ----------------------------------------------------------------------------

void CSysApAppUi::InitializeStatusPaneAreaL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::InitializeStatusPaneAreaL(): START") ) );
    TInt state( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );

    //Also Charging status will be updated with the following function.
    UpdateBatteryBarsL( state );

#ifdef SYSAP_USE_STARTUP_UI_PHASE
    if ( OfflineModeActive() )
        {
        UpdateSignalBarsL( KAknSignalOffLineMode );
        }
    else
        {
        UpdateSignalBarsL();    
        }          
#else
    UpdateSignalBarsL();
#endif // SYSAP_USE_STARTUP_UI_PHASE
    
    SetSignalIndicatorL();
    SetIhfIndicatorL();
    SetHacIndicatorL();
    LogsObserverL().HandleUiReadyL();

    if ( iSysApEtelConnector )
        {
        iSysApEtelConnector->ReadSimSmsStoreStatusInStartup();
        }

    SetEnvelopeIndicatorL();

    TRACES( RDebug::Print( _L("CSysApAppUi::InitializeStatusPaneAreaL(): accessories") ) );

    TAccMode accessoryState(EAccModeHandPortable);
    if ( iSysApAccessoryObserver )
        {
        accessoryState = iSysApAccessoryObserver->GetAccessoryMode();
        }

    if( accessoryState == EAccModeWiredHeadset ||
        accessoryState == EAccModeWirelessHeadset ||
        accessoryState == EAccModeHeadphones )
        {
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if( accessoryState == EAccModeLoopset )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if( accessoryState == EAccModeTextDevice )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if( accessoryState == EAccModeWiredCarKit || accessoryState == EAccModeWirelessCarKit )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if( accessoryState == EAccModeTVOut )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOn );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if (accessoryState == EAccModeHDMI )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
                SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
                SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
                SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
                SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
                SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff);
                SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOn );
        }
    else // all indicators off
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }

    TRACES( RDebug::Print( _L("CSysApAppUi::InitializeStatusPaneAreaL(): msg") ) );
    state = StateOfProperty( KUidSystemCategory, KUidOutboxStatusValue );
    if( state == ESADocumentsInOutbox )
        {
        SetIndicatorStateL( EAknIndicatorOutbox, EAknIndicatorStateOn );
        }
    else
        {
        SetIndicatorStateL( EAknIndicatorOutbox, EAknIndicatorStateOff );
        }

    state = StateOfProperty( KAlarmServerPubSubCategory, KWakeupAlarmPubSubKey );
    
    if( state == EActiveWakeupAlarmSet )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::InitializeStatusPaneAreaL called the SetIndicatorStateL with EActiveWakeupAlarmSet ")));
        SetIndicatorStateL( EAknIndicatorAlarmClock, EAknIndicatorStateOn );
        }
    else
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::InitializeStatusPaneAreaL called the SetIndicatorStateL with EActiveNoWakeupAlarmsSet ")));
        SetIndicatorStateL( EAknIndicatorAlarmClock, EAknIndicatorStateOff );
        }

    state = StateOfProperty( KIrdaPropertyCategory, KIrdaStatus );
    if( state == TIrdaStatusCodes::EIrLoaded || state == TIrdaStatusCodes::EIrDisconnected || state == TIrdaStatusCodes::EIrBlocked )
        {
        SetIndicatorStateL( EAknIndicatorIrActive, EAknIndicatorStateAnimate );
        }
    else if( state == TIrdaStatusCodes::EIrConnected  )
        {
        SetIndicatorStateL( EAknIndicatorIrActive, EAknIndicatorStateOn );
        }
    else
        {
        SetIndicatorStateL( EAknIndicatorIrActive, EAknIndicatorStateOff );
        }

    iSysApCenRepCallForwardingObserver->UpdateCallForwardingIndicatorsL();

    iSysApCenRepMsgWaitingObserver->UpdateMessageWaitingIndicatorsL();

    state = StateOfProperty( KPSUidNetworkInfo, KNWHomeZoneStatus );
    if( state == ENWNone || state == ENWCityZone )
        {
        SetIndicatorStateL( EAknIndicatorHomeZone, EAknIndicatorStateOff );
        }
    else if( state == ENWHomeZone )
        {
        SetIndicatorStateL( EAknIndicatorHomeZone, EAknIndicatorStateOn );
        }

    // Initialize WLan state
    state = StateOfProperty( KPSUidWlan, KPSWlanIndicator );
    HandleWlanIndicatorL(state);

    // Initialize Tarm state
    state = StateOfProperty( KPSUidCoreApplicationUIs, KCoreAppUIsTarmIndicator );
    HandleTarmIndicatorL(state);

    TRACES( RDebug::Print( _L("CSysApAppUi::InitializeStatusPaneAreaL(): END") ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleWlanIndicator()
// ----------------------------------------------------------------------------
void CSysApAppUi::HandleWlanIndicatorL( TInt aValue )
    {
    if ( aValue == EPSWlanIndicatorAvailable )
        {
        SetIndicatorStateL( EAknIndicatorWlanActive,       EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActiveSecure, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanAvailable,    EAknIndicatorStateOn  );
        }
    else if ( aValue == EPSWlanIndicatorActive )
        {
        SetIndicatorStateL( EAknIndicatorWlanAvailable,    EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActiveSecure, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActive,       EAknIndicatorStateOn  );
        }
    else if ( aValue == EPSWlanIndicatorActiveSecure )
        {
        SetIndicatorStateL( EAknIndicatorWlanAvailable,    EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActive,       EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActiveSecure, EAknIndicatorStateOn  );
        }
    else // No indication required
        {
        SetIndicatorStateL( EAknIndicatorWlanAvailable,    EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActive,       EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorWlanActiveSecure, EAknIndicatorStateOff );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleTarmIndicatorL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleTarmIndicatorL( TInt aValue )
    {
    if ( aValue == ECoreAppUIsTarmTerminalSecurityOnIndicatorOn )
        {
        SetIndicatorStateL( EAknIndicatorTARM, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTARMModuleOn, EAknIndicatorStateOn );
        }
    else if ( aValue == ECoreAppUIsTarmMngActiveIndicatorOn )
        {
        SetIndicatorStateL( EAknIndicatorTARMModuleOn, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTARM, EAknIndicatorStateOn );
        }
    else
        {
        SetIndicatorStateL( EAknIndicatorTARM, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTARMModuleOn, EAknIndicatorStateOff );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleShortPowerKeyPressedL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleShortPowerKeyPressedL()
    {
    TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );

    TRACES (
        RDebug::Print(
        _L("CSysApAppUi::HandleShortPowerKeyPressedL() callState:%d, iDeviceLockEnabled:%d" ),
        callState,
        iDeviceLockEnabled );
    );
    iSysApLightsController->PowerKeyPressedL();

    RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsHideAlarm, ECoreAppUIsHideAlarm );


    if( UiReady() && !iDeviceLockEnabled )
        {
        if ( iPowerKeyPopupMenuActive )
            {
            //Do nothing here. EEventKeyUp moves selection in the list!
            }
        else if ( !iKeyLockEnabled
            && callState != EPSCTsyCallStateAlerting
            && callState != EPSCTsyCallStateRinging
            && callState != EPSCTsyCallStateDialling )
            //Show power key menu
            {
            ShowPowerKeyPopUpMenuL();
            }
        }
    else 
        {
        TInt swState( StateOfProperty( KPSUidStartup, KPSGlobalSystemState ) );
        
        if( swState == ESwStateAlarm )
            //From Alarm state even a short press causes shutdown but charger state need to be checked
            {
            TInt chargerState( StateOfProperty( KPSUidHWRMPowerState, KHWRMChargingStatus ) );
            if( chargerState == EChargingStatusNotConnected )
                //shutdown
                {
                StartShutDownTimerOnAlarmAndChargingStates();
                }
            else
                //to charging state
                {
                SetStarterState( RStarterSession::ECharging );
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleLongPowerKeyPressedL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleLongPowerKeyPressedL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleLongPowerKeyPressedL()") ) );

    TInt swState;
    RProperty::Get( KPSUidStartup, KPSGlobalSystemState, swState );

    iSysApLightsController->PowerKeyPressedL();

    if( swState == ESwStateStartingCriticalApps ||
        swState == ESwStateSelfTestOK ||
        swState == ESwStateSecurityCheck ||
        swState == ESwStateCriticalPhaseOK ||
        swState == ESwStateEmergencyCallsOnly ||
        swState == ESwStateNormalRfOn ||
        swState == ESwStateNormalRfOff ||
        swState == ESwStateNormalBTSap ||
        swState == ESwStateFatalStartupError )
        {
        if( !iDeviceLockEnabled )
            {
            if( iPowerKeyPopupMenuActive )
                {
                if ( iGlobalListQuery )
                    {
                    iGlobalListQuery->SelectItem();
                    }
                }
            else if( !iIgnoreNextPowerKeyRepeats )
                {
                if( !iKeyLockEnabled && !IsEncryptionOperationOngoingL() )
                    {
                    DoShutdownL( EFalse, KDummyReason );
                    }
                }
            iIgnoreNextPowerKeyRepeats = ETrue;
            }
        }
    else if( swState == ESwStateAlarm )
        {
        //Just comment here to highlight that this is not possible since
        //from alarm state already a short press always shuts the device down.
        }
    else if( swState == ESwStateCharging )
        //Startup to Normal state
        {
        if( !iIgnoreNextPowerKeyRepeats )
            {
            iIgnoreNextPowerKeyRepeats = ETrue;

            SetStarterState( RStarterSession::ENormal );
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::FreeResources()
// ----------------------------------------------------------------------------

void CSysApAppUi::FreeResources()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::FreeResources") ) );

    delete iSysApBatteryInfoController;
    delete iSysApPsmController;

    delete iSysApAudioRoutingObserver;

    delete iProfileNamesArray;
    iProfileNamesArray = NULL;

    if ( iTimer )
        {
        iTimer->Cancel();
        delete iTimer;
        }

    if ( iSapTimer )
        {
        iSapTimer->Cancel();
        delete iSapTimer;
        }

    if ( iSysApTimer )
        {
        iSysApTimer->Cancel();
        delete iSysApTimer;
        }

    delete iGlobalListQuery;

    RWindowGroup groupWin = iCoeEnv->RootWin();
    groupWin.CancelCaptureKey( iCapturedEKeyPowerOff );
    groupWin.CancelCaptureKeyUpAndDowns( iCapturedEKeyPowerOffUpAndDowns );

    delete iSysApDefaultKeyHandler;

    if ( iProfileEngine )
        {
        iProfileEngine->Release();
        }

    delete iSysApSsSettingsObserver;
    delete iSysApEtelConnector;

    delete iSysApProfileObserver;
    delete iSysApNspsHandler;
    delete iSysApKeySndHandler;
    delete iSysApCenRepMsgWaitingObserver;
    delete iSysApCenRepCallForwardingObserver;
    delete iSysApConnectionMonitorObserver;
    delete iSysApCenRepLightSettingsObserver;
    delete iSysApCenRepLogsObserver;
    delete iSysApCenRepBtObserver;
    delete iSysApCenRepFmTxObserver;
    delete iSysApCenRepHacSettingObserver;
    delete iSysApCenRepController;

    delete iSysApPubSubObserver;

    delete iSysApMMCObserver;
    delete iSysApMsgSimMemLowQuery;
    delete iSysApWaitNote;
    delete iSysApConfirmationQuery;
    delete iSysApConfirmationQueryForRestart;
    delete iSysApOfflineModeController;
    delete iSysApUsbIndicatorController;
    delete iSysApBtController;
    delete iSysApBtSapController;
    delete iSignalNotify;
    delete iBatteryNotify;
    delete iSysApSystemLock;
    delete iSysApLocationPrivacyIndicator;
    delete iSysApAccessoryObserver;
    delete iSysApMediatorObserver;

    delete iSysApKeyManagement;
    iSysApKeyManagement = NULL;
    delete iSysApLightsController;
    iKeyLock.Close();
    delete iSysApFeatureManager;

    // Cleanup ECom, used by key management and light control
    REComSession::FinalClose();

    iResourcesFreed = ETrue;
    TRACES( RDebug::Print( _L("CSysApAppUi::FreeResources:END") ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowUiNoteL( const TSysApNoteIds aNote ) const
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowUiNoteL( const TSysApNoteIds aNote ) const
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowUiNoteL aNote: %d"), aNote ) );
    TInt tone( EAvkonSIDNoSound );
    TAknGlobalNoteType noteType( EAknGlobalBatteryLowNote );
    CAknGlobalNote* note = CAknGlobalNote::NewLC();
    TInt secondaryDisplayId(SecondaryDisplay::ECmdNoNote);

    switch ( aNote )
        {
        case EBatteryLowNote:
            noteType = EAknGlobalBatteryLowNote;
            tone = EAvkonSIDBatteryLowTone;
            break;
        case EBatteryFullNote:
            noteType = EAknGlobalBatteryFullNote;
            tone = EAvkonSIDInformationTone;
            break;
        case ERechargeBatteryNote:
            iSysApLightsController->BatteryEmptyL( ETrue );
            noteType = EAknGlobalRechargeBatteryNote;
            tone = EAvkonSIDRechargeBatteryTone;
            break;
        case ENotChargingNote:
            noteType = EAknGlobalNotChargingNote;
            tone = EAvkonSIDWrongCharger;
            break;
        case EInsertSimNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowInsertSimNote;
            break;
        case EMemoryCardRemovedWithoutEjectNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowMemoryCardRemovedWithoutEjectNote;
            break;
        case EGprsSuspendedNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowGprsSuspendedNote;
            break;
        case EGprsResumedNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowGprsResumedNote;
            break;
        case EShutdownNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowShutdownNote;
            break;
        case ECannotActivateOfflineModeNote:
            noteType = EAknGlobalErrorNote;
            tone = EAvkonSIDErrorTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowCannotActivateOfflineModeNote;
            break;
        case ECannotDeactivateOfflineModeNote:
            noteType = EAknGlobalErrorNote;
            tone = EAvkonSIDErrorTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowCannotDeactivateOfflineModeNote;
            break;
        case EKeypadActiveNote:
            noteType = EAknGlobalInformationNote;
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowKeypadActiveNote;
            break;
        case EFmTxAccessoryStandbyNote:
            noteType = EAknGlobalInformationNote; 
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowFmTxStandbyInAccessoryConnectionNote;
            break;
        case EFmTxAccessoryForbidsNote:
            noteType = EAknGlobalInformationNote; 
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowFmTxCannotEnableWhileAccessoryConnectedNote;
            break;
        case EFmTxVolumeDisabledNote:
            noteType = EAknGlobalInformationNote; 
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowFmTxVolumeDisabledNote;
            break;
        case EFmTxOnNote:
            noteType = EAknGlobalInformationNote; 
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowFmTxTuneRadioToFrequencyNote;
            break;
        case EFmTxOffNote:
            noteType = EAknGlobalInformationNote; 
            tone = EAvkonSIDInformationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowFmTxDisabledNote;
            break;
        case EBatteryFullUnplugChargerNote:
            noteType = EAknGlobalBatteryFullUnplugNote;
            tone = EAvkonSIDInformationTone;
            break;
        case EUnplugChargerNote:
            noteType = EAknGlobalUnplugChargerNote;
            tone = EAvkonSIDInformationTone;
            break;
        case EPowerSaveModeActivated:
        		noteType = EAknGlobalConfirmationNote;
            tone = EAvkonSIDConfirmationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowPowerSavingActivatedNote;
            break;
        case EPowerSaveModeDeactivated:
            noteType = EAknGlobalConfirmationNote;
            tone = EAvkonSIDConfirmationTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowPowerSavingDeactivatedNote;
            break;
        case ECannotActivatePowerSaveMode:
            noteType = EAknGlobalWarningNote;
            tone = EAvkonSIDWarningTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowCannotActivatePowerSavingNote;
            break;
        case ECannotDeactivatePowerSaveMode:
            noteType = EAknGlobalWarningNote;
            tone = EAvkonSIDWarningTone;
            secondaryDisplayId = SecondaryDisplay::ECmdShowCannotDeactivatePowerSavingNote;
            break;
        default:
            break;

        }

    TInt swState( StateOfProperty( KPSUidStartup, KPSGlobalSystemState ) );

    if( UiReady() || swState == ESwStateSecurityCheck)
        {
        HBufC* noteStringBuf = NULL;

        switch ( aNote )
            {
            case EInsertSimNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_SU_NOTE_INSERT_SIM, iEikonEnv );
                break;
            case EMemoryCardRemovedWithoutEjectNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_MEMS_EJECT_ERROR, iEikonEnv );
                note->SetSoftkeys( R_AVKON_SOFTKEYS_OK_EMPTY );
                break;
            case EGprsSuspendedNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_GPRS_CONF_GPRS_SUSPENDED, iEikonEnv );
                break;
            case EGprsResumedNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_GPRS_CONF_GPRS_RESUMED, iEikonEnv );
                break;
            case EShutdownNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_PWRC_INFO_GOODBYE, iEikonEnv);
                break;
            case ECannotActivateOfflineModeNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_ERR_OFFLINE_UNABLE, iEikonEnv );
                break;
            case ECannotDeactivateOfflineModeNote:
                noteStringBuf = StringLoader::LoadLC( R_QTN_ERR_OFFLINE_UNABLE_PROFILE, iEikonEnv );
                break;
            case EKeypadActiveNote:
                if ( iSysApFeatureManager->PenEnabled() )
                    {
                    noteStringBuf = StringLoader::LoadLC( R_QTN_KEYG_CONF_DISPLAY_KEYS_ACTIVE,
                                                          iEikonEnv );
                    }
                else
                    {
                    noteStringBuf = StringLoader::LoadLC( R_QTN_KEYG_CONF_KEYPAD_ACTIVE,
                                                          iEikonEnv );
                    }
                note->SetAnimation( R_QGN_NOTE_KEYGUARD_OPEN_ANIM );
                break;
            case EFmTxOnNote:
                {
                const TInt KFrequencyMaxLength(7);
                // read frequency
                TInt frequencykHz = iSysApCenRepFmTxObserver->Frequency();
                TReal frequencyMHz = static_cast<TReal>( frequencykHz ) / KCoefficientKhzToMhz; // kHz to Mhz
                TBuf<KFrequencyMaxLength> frequencyAsString;
                frequencyAsString.AppendNum( frequencyMHz, TRealFormat( KFrequencyMaxLength, KDecimalsInMhzFrequency ) );
                AknTextUtils::LanguageSpecificNumberConversion( frequencyAsString );
                noteStringBuf 
                      = StringLoader::LoadLC( R_QTN_FMTX_SYSAP_INFO_NOTE_CHANGE_FREQ, 
                                              frequencyAsString,
                                              iEikonEnv );
                break;
                }
            case EFmTxAccessoryForbidsNote:
                {
                noteStringBuf = StringLoader::LoadLC( R_QTN_FMTX_SYSAP_INFO_NOTE_ACCESSORY_FORBIDS, 
                                                       iEikonEnv );
                break;
                }
            case EFmTxAccessoryStandbyNote:
                {                                     
                noteStringBuf = StringLoader::LoadLC( R_QTN_FMTX_SYSAP_INFO_NOTE_ACCESSORY_STANDBY, 
                                                       iEikonEnv );
                break;
                }
            case EFmTxVolumeDisabledNote:
                {
                noteStringBuf = StringLoader::LoadLC( R_QTN_FMTX_SYSAP_INFO_NOTE_VOLUME_DISABLED, 
                                                       iEikonEnv );
                break;
                }
            case EFmTxOffNote:
                {
                noteStringBuf = StringLoader::LoadLC( R_QTN_FMTX_SYSAP_INFO_NOTE_DISABLED, 
                                                       iEikonEnv );
                break;
                }
            case EPowerSaveModeActivated:
                noteStringBuf = StringLoader::LoadLC( R_QTN_POWER_SAVING_ACTIVATED_CONF_NOTE,  iEikonEnv );
                 break;
            case EPowerSaveModeDeactivated:
                noteStringBuf = StringLoader::LoadLC( R_QTN_POWER_SAVING_DEACTIVATED_CONF_NOTE,  iEikonEnv );
                 break;
            case ECannotActivatePowerSaveMode:
                noteStringBuf = StringLoader::LoadLC( R_QTN_POWER_SAVING_FAILED_WARNING_NOTE, iEikonEnv );
                break;
            case ECannotDeactivatePowerSaveMode:
                noteStringBuf = StringLoader::LoadLC( R_QTN_POWER_SAVING_DEACTIVATION_FAILED_WARNING_NOTE, iEikonEnv ); 
                break;
            default:
                break;
            }

        note->SetTone( tone );

        // Set secondary display data if necessary
        if ( iSysApFeatureManager->CoverDisplaySupported() && secondaryDisplayId != SecondaryDisplay::ECmdNoNote)
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowUiNoteL - Notifying secondary display") ) );
            CAknSDData* sd;
            
            if ( aNote == EFmTxOnNote ) // frequency information is passed to secondary display
                {
                SecondaryDisplay::TFmTxFrequencyInKhzPckg pckg( iSysApCenRepFmTxObserver->Frequency() );
                sd = CAknSDData::NewL( SecondaryDisplay::KCatSysAp, secondaryDisplayId, pckg );
                }
            else
                {
                sd = CAknSDData::NewL(SecondaryDisplay::KCatSysAp, secondaryDisplayId, KNullDesC8);
                }
             
            note->SetSecondaryDisplayData(sd); // ownership to notifier client
            }

        if ( noteStringBuf )
            {
            TPtr textBuffer = noteStringBuf->Des();
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowUiNoteL Next:note->ShowNoteL" ) ) );
            note->ShowNoteL( noteType, textBuffer );
            CleanupStack::PopAndDestroy( ); // noteStringbuf
            }
        else
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowUiNoteL Next:note->ShowNoteL" ) ) );
            note->ShowNoteL( noteType, KNullDesC );
            }
        }
    else if( swState == ESwStateAlarm || swState == ESwStateCharging )
        {
        if ( aNote != EUnplugChargerNote ) // don't play tone when charger is disconnected in charging state
            {
            static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PlaySound( tone );    
            }
        }

    CleanupStack::PopAndDestroy(); // note
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowChargingNoteL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowChargingNoteL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowChargingNoteL") ) );
    TBool showNote( ETrue );

    TInt swState( StateOfProperty( KPSUidStartup, KPSGlobalSystemState ) );
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowChargingNoteL: swState: %d"), swState ) );

    if( UiReady() || swState == ESwStateSecurityCheck )
        {
        if ( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) ==  EPSCTsyCallStateRinging )
            {
            showNote = EFalse;
            }
        TRACES( RDebug::Print( _L("CSysApAppUi::ShowChargingNoteL KCTsyCallState=%d"), StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) ) );
        if ( showNote ) // Power Mgmt UI spec defines that no Charging note is shown while the phone is ringing/alerting
            {
            CAknGlobalNote* chargingNote = CAknGlobalNote::NewLC();
            chargingNote->SetTone( EAvkonSIDChargingBatteryTone );
            chargingNote->ShowNoteL( EAknGlobalChargingNote, KNullDesC );
            CleanupStack::PopAndDestroy( chargingNote );
            }
        }
    else if( swState == ESwStateAlarm || swState == ESwStateCharging )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::ShowChargingNoteL - Just play tone") ) );
        static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PlaySound( EAvkonSIDChargingBatteryTone );
        }
    else
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::ShowChargingNoteL - No note shown") ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleChargerNotesL()
// ----------------------------------------------------------------------------
void CSysApAppUi::HandleChargerNotesL( const TInt aValue )
    {
    TBool showNote(StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) !=  EPSCTsyCallStateRinging);
    if ( aValue == EChargingStatusCharging )
        {
        iSysApLightsController->BatteryEmptyL( EFalse );
        TRACES( RDebug::Print( _L("SysAp: charger connected") ) );
        iSysApLightsController->ChargerConnectedL( ETrue );
        ShowChargingNoteL();
        }
    //Display Not Charging note
    else if ( aValue == EChargingStatusError )
        {
        if(showNote)
            {
            ShowUiNoteL( ENotChargingNote );
            }
        }
    else if ( aValue == EChargingStatusChargingComplete )
        {
        iSysApLightsController->ChargingCompleteL();
        
        TSysApNoteIds note( EBatteryFullNote );
        
        if ( !iSysApUsbChargerDetector.HostOnlyUsbChargingUsed() &&
             iSysApFeatureManager->Supported( KSysApFeatureIdChargerReminderNotes ) )
            {
            note = EBatteryFullUnplugChargerNote;
            }
        iSysApUsbChargerDetector.Reset();
        if(showNote)
            {
            ShowUiNoteL( note );
            }
        }
    else if ( aValue == EChargingStatusNotConnected )
        {
        TRACES( RDebug::Print( _L("SysAp: charger removed") ) );
        iSysApLightsController->ChargerConnectedL( EFalse );    
                
        if ( !iSysApUsbChargerDetector.HostOnlyUsbChargingUsed() &&
             iSysApFeatureManager->Supported( KSysApFeatureIdChargerReminderNotes ) )
            {
			if(showNote)
                {
                ShowUiNoteL( EUnplugChargerNote );
                }
            }
		iSysApUsbChargerDetector.Reset();
        }
    else if ( aValue == EChargingStatusNotCharging )
        {
        TRACES( RDebug::Print( _L("SysAp: Not charging") ) );
        }
    else if ( aValue == EChargingStatusAlmostComplete )
        {
        TRACES( RDebug::Print( _L("SysAp: Charging almost complete") ) );
        }
    else if ( aValue == EChargingStatusChargingContinued )
        {
        //in this case we simply want to scroll the battery bars and not turn light on or show ui note
        // set lights controller iBatteryEmpty to EFalse
        iSysApLightsController->BatteryEmptyL( EFalse );
        TRACES( RDebug::Print( _L("SysAp: charging continues") ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowProfileNoteL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowProfileNoteL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowProfileNoteL") ) );

    if ( UiReady() )
        {
      
        TBufC<KMaxProfileNameLength> profileName;
        HBufC* noteStringBuf = NULL;
        ActiveProfileNameL( profileName.Des() );
        noteStringBuf = StringLoader::LoadLC( R_TEXT_MODE_SELECTED, profileName, iEikonEnv );
        TPtr textBuffer = noteStringBuf->Des();
        AknTextUtils::DisplayTextLanguageSpecificNumberConversion( textBuffer );

       
        // Set secondary display data if necessary
        if ( iSysApFeatureManager->CoverDisplaySupported() )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowProfileNoteL - Notifying secondary display") ) );
            SecondaryDisplay::TProfileName sdProfileName;
            sdProfileName.Append(profileName.Left(SecondaryDisplay::KProfileNameMaxLen));
            SecondaryDisplay::TProfileNotePckg pckg(sdProfileName);
            CAknSDData* sd = CAknSDData::NewL(SecondaryDisplay::KCatSysAp, SecondaryDisplay::ECmdShowProfileNote, pckg);
           
            }


        CAknDiscreetPopup::ShowGlobalPopupL(textBuffer,KNullDesC, KAknsIIDNone, KNullDesC);
         
        CleanupStack::PopAndDestroy(); // noteStringbuf 

        iSysApLightsController->ProfileChangedL(); // All notes should always turn lights on.
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowQueryL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowQueryL( const TSysApConfirmationQueryIds aQueryId, const TDesC& aValue )
    {
    CancelGlobalListQuery();

    if ( aQueryId == ESysApRestartPhoneQuery )
        {
        iDisablePowerkeyMenu = ETrue;
        // Cancel any other query that might be outstanding (i.e. ESysApRemoveMmcNote),
        // to avoid multiple overlapping queries causing problems
        if ( iSysApConfirmationQuery )
            {
            iSysApConfirmationQuery->Cancel();
            }

        if ( !iSysApConfirmationQueryForRestart )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowQueryL: trying CSysApConfirmationQuery::NewL() for restart") ) );
            iSysApConfirmationQueryForRestart = CSysApConfirmationQuery::NewL( *this );
            }

        iSysApConfirmationQueryForRestart->ShowQueryL( aQueryId, iEikonEnv );
        }
    else
        {
        if ( !iSysApConfirmationQuery )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::ShowQueryL: trying CSysApConfirmationQuery::NewL()") ) );
            iSysApConfirmationQuery = CSysApConfirmationQuery::NewL( *this );
            }

        iSysApConfirmationQuery->ShowQueryL( aQueryId, aValue, iEikonEnv );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UpdateBatteryBarsL( const TInt aState )
// ----------------------------------------------------------------------------

void CSysApAppUi::UpdateBatteryBarsL( const TInt aState )
    {
    TInt state( StateOfProperty( KPSUidHWRMPowerState, KHWRMChargingStatus ) );
    TRACES( RDebug::Print( _L("CSysApAppUi::UpdateBatteryBarsL aState: %d, chargerState:%d"), aState, state ) );
    HandleUsbCharger( state );
    if( state == EChargingStatusCharging ||
        state == EChargingStatusChargingContinued ||
        state == EChargingStatusAlmostComplete )
        {
        if ( !iCharging )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::UpdateBatteryBarsL: Starting Charging") ) );
            iBatteryNotify->StartChargingL();
            iCharging = ETrue;
            }

        // When Avkon supports battery state sending while charging,
        // add setting battery level here also.
        }
    else
        {
        if ( iCharging )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::UpdateBatteryBarsL: Stopping Charging") ) );
            iBatteryNotify->StopChargingL();
            iCharging = EFalse;
            }

        if( state == KErrUnknown || aState == KErrUnknown )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::UpdateBatteryBarsL: CAknBatteryNotify::SetBatteryLevelL(0)") ) );
            iBatteryNotify->SetBatteryLevelL( 0 );
            }
        else
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::UpdateBatteryBarsL: CAknBatteryNotify::SetBatteryLevelL(%d)"), aState - EBatteryLevelLevel0 ) );
            iBatteryNotify->SetBatteryLevelL( aState - EBatteryLevelLevel0 );
            }
        }
    
    if ( !iSysApPsmController ) // created here if first state change has not occurred yet
        {
        iSysApPsmController = CSysApPsmController::NewL( *this );        
        }

    if ( iSysApPsmController )
        {
        if ( iSysApPsmController->FullPsmEnabled() )
            {
            iBatteryNotify->SetBatteryStateL( EAknBatteryIndicatorPowerSave );
            }
        else
            {
            iBatteryNotify->SetBatteryStateL( EAknBatteryIndicatorNormal );
            }            
        }
        
    if ( iSysApBatteryInfoController )        
        {
        iSysApBatteryInfoController->BatteryLevelUpdatedL();
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UpdateSignalBarsL()
// ----------------------------------------------------------------------------

void CSysApAppUi::UpdateSignalBarsL()
    {
    UpdateSignalBarsL(iSysApEtelConnector->GetSignalBars());
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UpdateSignalBarsL( const TInt aState )
// ----------------------------------------------------------------------------

void CSysApAppUi::UpdateSignalBarsL( const TInt aState )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::UpdateSignalBarsL aState: %d"), aState ) );

    if( aState == KAknSignalOffLineMode || (iSysApOfflineModeController->OfflineModeActive() && !iEmergencyCallActive) )
        {
        iSignalNotify->SetSignalLevelL( KAknSignalOffLineMode );
        }
    else if( aState <= 0 )
        {
        // Do not update indicator show zero bars if we should be showing X over bars
        // This is required for indicator to work correctly in BT SAP mode.
        // Cannot use check for BT SAP enabled status, as zero bars notify comes before enabled status changes.
        if ( !iIgnoreZeroNetworkBarNotifications )
            {
            iSignalNotify->SetSignalLevelL( 0 );
            }
        }
    else
        {
        iSignalNotify->SetSignalLevelL( aState );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetSignalIndicatorL()
// ----------------------------------------------------------------------------

void CSysApAppUi::SetSignalIndicatorL()
    {
    TSysApNetworkMode networkMode( ESysApGSM );

    TInt networkModeFromNetworkInfo( StateOfProperty( KPSUidNetworkInfo, KNWTelephonyNetworkMode ) );
    if ( networkModeFromNetworkInfo == ENWNetworkModeWcdma )
        {
        networkMode = ESysApWCDMA;
        }
    else
        {
        networkMode = ESysApGSM;
        }

    TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorL: networkMode = %d" ), networkMode ) );

    if( iSysApOfflineModeController->OfflineModeActive() && !iEmergencyCallActive )
        {
        // The device is in Offline Mode
        if ( iSysApFeatureManager->Supported( KSysApFeatureIdGprs_G_Icon ) )
            {
            iSignalNotify->SetGprsStateL( EAknSignalGprsIndicatorOff );
            }
        else
            {
            iSignalNotify->SetCommonPacketDataStateL( EAknSignalCommonPacketDataIndicatorOff );
            }
        iSignalNotify->SetWcdmaStateL( EAknSignalWcdmaIndicatorOff );
        iSignalNotify->SetHsdpaStateL( EAknSignalHsdpaIndicatorOff);
        }
    else
        {
        // The device is in Online Mode
        switch ( networkMode )
            {
            case ESysApGSM:
                SetSignalIndicatorGsmL();
                break;

            case ESysApWCDMA:
                SetSignalIndicatorWcdmaL();
                break;

            default:
                break;
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetSignalIndicatorGsmL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SetSignalIndicatorGsmL()
    {
    TInt signalGprsIndicatorState( 0 );
    TInt signalIndicatorState( 0 );
	TInt bearerValue = EBearerUnknown;

	if(iSysApConnectionMonitorObserver)
		{
		bearerValue = iSysApConnectionMonitorObserver->GetBearerValue();
		}

	if(bearerValue == EBearerEdgeGPRS)
		{
		TInt egprsStatus = StateOfProperty( KUidSystemCategory, KPSUidGprsStatusValue );
	    TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorGsmL gprsStatus: %d" ), egprsStatus ) );
	    iSignalNotify->SetWcdmaStateL( EAknSignalWcdmaIndicatorOff );
	    iSignalNotify->SetHsdpaStateL( EAknSignalHsdpaIndicatorOff );
	    switch ( egprsStatus )
	        {
	        case EPSGprsContextActive:
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorContext;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorContext;
	            iGprsActivated = ETrue;
	            iGprsSuspended = EFalse;
	            HandleGprsNotesL();
	            break;

	        case EPSGprsContextActivating:
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorEstablishingContext;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorEstablishingContext;
	            iGprsActivated = EFalse;
	            iGprsSuspended = EFalse;
	            break;

	        case EPSGprsSuspend:
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorSuspended;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorSuspended;
	            iGprsSuspended = ETrue;
	            HandleGprsNotesL();
	            break;

	        case EPSGprsAttach:
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorAttached;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorAttached;
	            iGprsActivated = EFalse;
	            iGprsSuspended = EFalse;
	            break;

	        case EPSGprsMultibleContextActive:
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorMultipdp;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorMultipdp;
	            iGprsActivated = ETrue;
	            iGprsSuspended = EFalse;
	            HandleGprsNotesL();
	            break;

	        case EPSGprsUnattached:
	        default:
	        	  if ( iTDEnable )
	            {
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorAvailable;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorAvailable;
	            }
	            else
	            {
	            signalGprsIndicatorState = EAknSignalEdgeIndicatorOff;
	            signalIndicatorState = EAknSignalCommonPacketDataIndicatorOff;
	            }
              iGprsSuspendedNoteShown = EFalse;
	            iGprsActivated = EFalse;
	            iGprsSuspended = EFalse;
	            iCallActivated = EFalse;
	            break;
	        }
        }
	else
		{

		TInt gprsStatus = StateOfProperty( KUidSystemCategory, KPSUidGprsStatusValue );
    	TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorGsmL gprsStatus: %d" ), gprsStatus ) );
    	iSignalNotify->SetWcdmaStateL( EAknSignalWcdmaIndicatorOff );
    	iSignalNotify->SetHsdpaStateL( EAknSignalHsdpaIndicatorOff );
    	switch ( gprsStatus )
            {
            case EPSGprsContextActive:
                signalGprsIndicatorState = EAknSignalGprsIndicatorContext;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorContext;
                iGprsActivated = ETrue;
                iGprsSuspended = EFalse;
                HandleGprsNotesL();
                break;

            case EPSGprsContextActivating:
                signalGprsIndicatorState = EAknSignalGprsIndicatorEstablishingContext;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorEstablishingContext;
                iGprsActivated = EFalse;
                iGprsSuspended = EFalse;
                break;

            case EPSGprsSuspend:
                signalGprsIndicatorState = EAknSignalGprsIndicatorSuspended;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorSuspended;
                iGprsSuspended = ETrue;
                HandleGprsNotesL();
                break;

            case EPSGprsAttach:
                signalGprsIndicatorState = EAknSignalGprsIndicatorAttached;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorAttached;
                iGprsActivated = EFalse;
                iGprsSuspended = EFalse;
                break;

            case EPSGprsMultibleContextActive:
                signalGprsIndicatorState = EAknSignalGprsIndicatorMultipdp;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorMultipdp;
                iGprsActivated = ETrue;
                iGprsSuspended = EFalse;
                HandleGprsNotesL();
                break;

            case EPSGprsUnattached:
            default:
                if ( iTDEnable )
	              {
	              signalGprsIndicatorState = EAknSignalGprsIndicatorAvailable;
	              signalIndicatorState = EAknSignalCommonPacketDataIndicatorAvailable;
	              }
	              else
	              {
                signalGprsIndicatorState = EAknSignalGprsIndicatorOff;
                signalIndicatorState = EAknSignalCommonPacketDataIndicatorOff;
                }

                iGprsSuspendedNoteShown = EFalse;
                iGprsActivated = EFalse;
                iGprsSuspended = EFalse;
                iCallActivated = EFalse;
                break;
            }
		}

	if(bearerValue == EBearerEdgeGPRS) //EdgeGPRS connection
		{
		if( iSysApFeatureManager->Supported( KSysApFeatureIdUseEdgeGPRSIcon ) )
	    	{
	    	iSignalNotify->SetEdgeStateL( signalGprsIndicatorState );
	    	}
	    else
        	{
        	iSignalNotify->SetCommonPacketDataStateL( signalIndicatorState );
        	}
		}
	else
		{

	    if ( iSysApFeatureManager->Supported( KSysApFeatureIdGprs_G_Icon ) )
    	    {
        	iSignalNotify->SetGprsStateL( signalGprsIndicatorState );
        	}
    	else
        	{
        	TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorGsmL SetCommonPacketDataStateL( %d )" ), signalIndicatorState ) );
        	iSignalNotify->SetCommonPacketDataStateL( signalIndicatorState );
        	}
		}
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetSignalIndicatorWcdmaL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SetSignalIndicatorWcdmaL()
    {
    TInt wcdmaStatus;
    wcdmaStatus = StateOfProperty( KUidSystemCategory, KPSUidWcdmaStatusValue );
    TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorWcdmaL: wcdmaStatus: %d" ), wcdmaStatus ) );

    if ( iSysApFeatureManager->Supported( KSysApFeatureIdGprs_G_Icon ) )
        {
        iSignalNotify->SetGprsStateL( EAknSignalGprsIndicatorOff );
        }
    else
        {
        iSignalNotify->SetCommonPacketDataStateL( EAknSignalCommonPacketDataIndicatorOff );
        }

    TInt signalWcdmaIndicatorState;
    TInt signalHsdpaIndicatorState;

    switch ( wcdmaStatus )
        {
        case EPSWcdmaContextActive:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorContext;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorContext;
            break;
        case EPSWcdmaContextActivating:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorEstablishingContext;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorEstablishingContext;
            break;
        case EPSWcdmaSuspend:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorSuspended;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorSuspended;
            break;
        case EPSWcdmaAttach:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorAttached;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorAttached;
            break;
        case EPSWcdmaMultipleContextActive:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorMultipdp;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorMultipdp;
            break;
        case EPSWcdmaUnattached:
        default:
            signalWcdmaIndicatorState = EAknSignalWcdmaIndicatorAvailable;
            signalHsdpaIndicatorState = EAknSignalHsdpaIndicatorAvailable;
            break;
        }

    TBool showHsdpaAvailable = EFalse;

    if ( iSysApFeatureManager->Supported( KSysApFeatureIdUseHsdpaAvailableIcon ) )
        {
        // HSDPA Resource Availability information is a configurable feature.
        showHsdpaAvailable = iSysApEtelConnector->HsdpaAvailable();
        }
    
    TBool isHsdpaBearer = iSysApEtelConnector->IsBearerHsdpa();
    
    TRACES( RDebug::Print( _L("CSysApAppUi::SetSignalIndicatorWcdmaL: isHdpaBearer=%d, showHsdpaAvailable=%d" ),
            isHsdpaBearer, showHsdpaAvailable ) );
    
    if ( isHsdpaBearer || showHsdpaAvailable )
        {
        iSignalNotify->SetWcdmaStateL( EAknSignalWcdmaIndicatorOff );
    	iSignalNotify->SetHsdpaStateL( signalHsdpaIndicatorState );
        }
    else
        {
        iSignalNotify->SetHsdpaStateL( EAknSignalHsdpaIndicatorOff );
        iSignalNotify->SetWcdmaStateL( signalWcdmaIndicatorState );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleGprsNotesL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleGprsNotesL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleGprsNotesL" ) ) );
    if ( iSysApFeatureManager->Supported( KSysApFeatureIdGPRSNotes ) )
        {
        // In WCDMA system, receiving or making calls has no effect to the possibly
        // existing PDP contexts i.e. GPRS notes are not needed.
        TInt networkMode = StateOfProperty( KPSUidNetworkInfo, KNWTelephonyNetworkMode );
        TRACES( RDebug::Print( _L("CSysApAppUi::HandleGprsNotesL: networkMode %d" ), networkMode ) );
        if ( networkMode != ENWNetworkModeWcdma )
            {
            if ( iGprsActivated && iGprsSuspended && iCallActivated && !iGprsSuspendedNoteShown )
                {
                StartGprsSuspendedTimer();
                }
            else if ( iGprsSuspendedNoteShown && !iGprsSuspended )
                {
                iGprsSuspendedNoteShown = EFalse;
                iGprsSuspended = EFalse;
                iCallActivated = EFalse;
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetIndicatorStateL( TInt aIndicator, TInt aState )
// ----------------------------------------------------------------------------

void CSysApAppUi::SetIndicatorStateL( const TInt aIndicator, const TInt aState ) const
    {
    CAknSmallIndicator* theIndicator = CAknSmallIndicator::NewLC( TUid::Uid( aIndicator ) );
    theIndicator->SetIndicatorStateL( aState );
    CleanupStack::PopAndDestroy(); //theIndicator
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::GoOnlineL()
// ----------------------------------------------------------------------------

void CSysApAppUi::GoOnlineL( TBool aDoProfileChange )
    {
    if ( iSysApFeatureManager->OfflineModeSupported() )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::GoOnlineL: going from off-line into on-line: aDoProfileChange=%d" ), aDoProfileChange ) );
        // in case profile change is not needed, event handling for subsequent profile changes are must be allowed
        iHandleNextProfileEvent = !aDoProfileChange;
        iActivateBt = iSysApOfflineModeController->MustBtBeActivated();

        if ( aDoProfileChange )
            {
            TRAPD( err, ActivateProfileL( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileBeforeOfflineMode ) ) );
            if ( err == KErrNotFound )
                {
                // Activate general profile instead, if previously used profile was not found
                TRACES( RDebug::Print( _L("CSysApAppUi::GoOnlineL: Previously used profile was not found, restoring to general profile" ) ) );
                ActivateProfileL(KGeneralProfileId);
                }
            }

        iSysApOfflineModeController->SwitchFromOfflineToOnlineModeL();
        TRACES( RDebug::Print( _L("CSysApAppUi::GoOnlineL: iActivateBt = %d" ), iActivateBt ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::GoOfflineL()
// ----------------------------------------------------------------------------

void CSysApAppUi::GoOfflineL()
    {
    if ( iSysApFeatureManager->OfflineModeSupported() )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::GoOfflineL" ) ) );
        iHandleNextProfileEvent = EFalse;
        iDeactivateBt = ETrue;
        ActivateProfileL( KOfflineModeProfileId );
        iSysApOfflineModeController->SwitchFromOnlineToOfflineModeL();
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SwitchFromOnlineToOfflineModeL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SwitchFromOnlineToOfflineModeL()
    {
    if ( iSysApFeatureManager->FmTxSupported() )
        {
        TFmTxState state = static_cast<TFmTxState>(StateOfProperty( KPSUidHWRMFmTx, KHWRMFmTxStatus ));
        switch ( state )
            {
            case EFmTxStateActive:
            case EFmTxStateInactive:
            case EFmTxStateScanning:
                ShowQueryL( ESysApUseFmTxInOfflineQuery ); // show "use fm tx in offline?" if FM TX is on
                break;
            
            default:
                // do nothing
                break;                
            }
        }
    iSysApOfflineModeController->SwitchFromOnlineToOfflineModeL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::OfflineModeChangedL()
// ----------------------------------------------------------------------------
void CSysApAppUi::OfflineModeChangedL()
    {
#ifdef SYSAP_USE_STARTUP_UI_PHASE    
    // if UI is not ready, don't update indicators
    if ( !UiReady() )
        {
        return;
        }
#endif // SYSAP_USE_STARTUP_UI_PHASE
    
    // Update network bar indicator
    if ( OfflineModeActive() )
        {
        UpdateSignalBarsL( KAknSignalOffLineMode );
        }
    else
        {
        UpdateSignalBarsL();    
        }        
    
    // Update network mode indicator
    SetSignalIndicatorL();
    
    // Update HAC indicator
    SetHacIndicatorL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::OfflineModeActive()
// ----------------------------------------------------------------------------
TBool CSysApAppUi::OfflineModeActive()
    {
    return iSysApOfflineModeController->OfflineModeActive();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoNotActivateBt()
// ----------------------------------------------------------------------------
void CSysApAppUi::DoNotActivateBt()
    {
    iSysApOfflineModeController->DoNotActivateBt();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetBtPowerState()
// ----------------------------------------------------------------------------
TInt CSysApAppUi::SetBtPowerState( TBool aBtState )
    {
    if ( iSysApBtController )
        {
        return iSysApBtController->SetPowerState( aBtState );
        }
    else
        {
        return KErrNotReady;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::KeyLockState() const
// ----------------------------------------------------------------------------

TBool CSysApAppUi::KeyLockState() const
    {
    return iKeyLockEnabled;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DeviceLockState() const
// ----------------------------------------------------------------------------

TBool CSysApAppUi::DeviceLockState() const
    {
    return iDeviceLockEnabled;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ChangeFmTxStateL()
// ----------------------------------------------------------------------------     
void CSysApAppUi::ChangeFmTxStateL( TBool aEnable )
    {
    // disable fm transmission
    CHWRMFmTx* fmtx = CHWRMFmTx::NewLC();
    if ( aEnable )
       {
       fmtx->EnableL();
       }
    else 
       {
       fmtx->DisableL();
       }
    CleanupStack::PopAndDestroy( fmtx );
    } 

// ----------------------------------------------------------------------------
// CSysApAppUi::SetIhfIndicatorL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SetIhfIndicatorL()
    {
    TBool accessoryConnected ( EFalse );

    TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );

    TAccMode accMode(EAccModeHandPortable);

    if ( iSysApAccessoryObserver )
        {
        accMode = iSysApAccessoryObserver->GetAccessoryMode();

        if ( accMode != EAccModeHandPortable )
            {
            accessoryConnected = ETrue;
            }
        }

    if( (callState != EPSCTsyCallStateNone && IhfEnabledL() && !accessoryConnected)
        || accMode == EAccModeMusicStand )  // Music stand always shows IHF indicator
        {
        SetIndicatorStateL( EAknIndicatorIHFActive, EAknIndicatorStateOn );

        if ( accMode == EAccModeMusicStand )
            {
            // hide other accessory indicators to prevent displaying of duplicate icons in some cases
            // e.g. when wireless hf is connected
            SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
            SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
            SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
            SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
            SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
            SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
            SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
            }
        }
    else
        {
        SetIndicatorStateL( EAknIndicatorIHFActive, EAknIndicatorStateOff );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleSmsStorageNotificationL( TBool aSimStoreFull )
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleSmsStorageNotificationL( TBool aSimStoreFull )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleSmsStorageNotificationL: aSimStoreFull: %d "), aSimStoreFull ) );

    if ( aSimStoreFull )
        {
        HBufC* noteStringBuf;
        noteStringBuf = StringLoader::LoadLC( R_QTN_MEMLO_MEMORY_LOW_SIM_MES, iEikonEnv );
        TPtr textBuffer = noteStringBuf->Des();
        iSysApMsgSimMemLowQuery->StartL( textBuffer );
        CleanupStack::PopAndDestroy();
        }

    SetEnvelopeIndicatorL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleNspsRawKeyEventL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleNspsRawKeyEventL()
    {
#ifdef __SYSAP_MODULE_TEST
    ModuleTestShowUiNoteL( _L("Network wakeup from NSPS") );
#endif

    if ( iSysApEtelConnector )
        {
        iSysApEtelConnector->CommandNetCsWakeupOnNsps();
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleNetworkNspsNotification( RMmCustomAPI::TNspsStatus aNspsStatus )
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleNetworkNspsNotification( RMmCustomAPI::TNspsStatus aNspsStatus )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleNetworkNspsNotification aNspsStatus:%d, iNsps:%d )" ), aNspsStatus, iNsps ) );
    if( iSysApNspsHandler )
        {
        if( aNspsStatus == RMmCustomAPI::ENspsOn )
            {
            if( !iNsps )
                {
#ifdef __SYSAP_MODULE_TEST
                TRAPD( err, ModuleTestShowUiNoteL( _L("Setting NSPS on") ) );
#endif
                iSysApNspsHandler->SetNspsOn();
                iNsps = ETrue;
                }
            }
        else if( aNspsStatus == RMmCustomAPI::ENspsOff )
            {
            if( iNsps )
                {
#ifdef __SYSAP_MODULE_TEST
                TRAPD( err, ModuleTestShowUiNoteL( _L("Setting NSPS off") ) );
#endif
                iSysApNspsHandler->SetNspsOff();
                iNsps = EFalse;
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleMessageTonePlayingQuitability( TInt aTonePlayingStatus )
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleMessageTonePlayingQuitability( TInt aTonePlayingStatus )
    {
    if( iSysApNspsHandler )
        {
        if( aTonePlayingStatus == ECoreAppUIsStopTonePlaying )
            {
            iSysApNspsHandler->SetMessageToneNotPlaying();
            }
        else if( aTonePlayingStatus == ECoreAppUIsTonePlaying )
            {
            iSysApNspsHandler->SetMessageTonePlaying();
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ActivateKeyeventForwardingForLights()
// ----------------------------------------------------------------------------

void CSysApAppUi::ActivateKeyeventForwardingForLights(TBool aActivate)
    {
    if( iSysApNspsHandler )
        {
        if ( aActivate )
            {
            iSysApNspsHandler->ActivateKeyeventForwardingForLights();
            }
        else
            {
            iSysApNspsHandler->DeActivateKeyeventForwardingForLights();
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::StartShutDownTimerOnAlarmAndChargingStates()
// ----------------------------------------------------------------------------

void CSysApAppUi::StartShutDownTimerOnAlarmAndChargingStates()
    {
    if( !iAlarmOrChargingStateShutdownStarted )
        {
        if ( !iTimer )
            {
            TRAPD( err, iTimer = CPeriodic::NewL( EPriorityNormal ) );

            if ( err != KErrNone )
                {
                TRACES( RDebug::Print( _L("CSysApAppUi::StartShutDownTimerOnAlarmAndChargingStates: CPeriodic::NewL failed: %d"), err ) );
                return;
                }
            }
        iTimer->Cancel();
        iTimer->Start( KDelayBeforeShuttingDownInAlarmAndChargingStates,
                       KDelayBeforeShuttingDownInAlarmAndChargingStates,
                       TCallBack( DoShutdownOnAlarmStateAfterCallBack, this ) );
        iAlarmOrChargingStateShutdownStarted = ETrue;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoShutdownOnAlarmStateAfterCallBack( TAny* aObject )
// ----------------------------------------------------------------------------

TInt CSysApAppUi::DoShutdownOnAlarmStateAfterCallBack( TAny* aObject )
    {
    CSysApAppUi* appUi = STATIC_CAST( CSysApAppUi*, aObject );

    if ( appUi->iTimer )
        {
        appUi->iTimer->Cancel();
        }

    TRAP_IGNORE( appUi->DoShutdownL( EFalse, KDummyReason ) );

    return KErrNone;
    }

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
// ----------------------------------------------------------------------------
// CSysApAppUi::StartAnimTiming()
// ----------------------------------------------------------------------------
void CSysApAppUi::StartAnimTiming()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::StartAnimTiming() Start animation timer, time %d ms" ), iAnimationShowingTime  ) );

    if ( !iAnimTimer )
        {
        TRAPD( err, iAnimTimer = CPeriodic::NewL( EPriorityNormal ) );

        if ( err != KErrNone )
            {
            TRACES( RDebug::Print( _L("CSysApAppUi::StartAnimTiming() CPeriodic::NewL failed %d " ), err ) );
            return;
            }
        }

    iAnimTimer->Start(
        iAnimationShowingTime*KCoefficientToMakeMicroToMilliSeconds,
        iAnimationShowingTime*KCoefficientToMakeMicroToMilliSeconds,
        TCallBack( DoStopAnimTiming, this ) );
    }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

// ----------------------------------------------------------------------------
// CSysApAppUi::DoStopAnimTiming( TAny* aObject )
// ----------------------------------------------------------------------------

TInt CSysApAppUi::DoStopAnimTiming( TAny* aObject )
    {
    TInt err(KErrNone);
    CSysApAppUi* appUi = STATIC_CAST( CSysApAppUi*, aObject );

    // This method could theoretically be called by two timers (iAnimTimer and one in CSysApShutdownAnimation),
    // so a check is needed to prevent multiple executions.
    if ( !(appUi->iShutdownContinued) )
        {
        appUi->iShutdownContinued = ETrue;

        TRACES( RDebug::Print( _L("CSysApAppUi::DoStopAnimTiming() Animation timer completed or animation skipped" ) ) );

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
        if ( appUi->iAnimTimer )
            {
            appUi->iAnimTimer->Cancel();
            }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

        if ( appUi->iTimer )
            {
            appUi->iTimer->Cancel();
            }

        TRACES( RDebug::Print( _L("CSysApAppUi::DoStopAnimTiming() Call ContinueShutdown(...)" ) ) );
        appUi->ContinueShutdown();
        TRACES( RDebug::Print( _L("CSysApAppUi::DoStopAnimTiming() end") ) );
        }

    return err;
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::StartGprsSuspendedTimer()
// ----------------------------------------------------------------------------

void CSysApAppUi::StartGprsSuspendedTimer()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::StartGprsSuspendedTimer()") ) );
    if( !iAlarmOrChargingStateShutdownStarted ) // not relevant if shutting down
        {
        if ( !iTimer )
            {
            TRAPD( err, iTimer = CPeriodic::NewL( EPriorityNormal ) );

            if ( err != KErrNone )
                {
                TRACES( RDebug::Print( _L("CSysApAppUi::StartGprsSuspendedTimer: CPeriodic::NewL failed: %d"), err ) );
                return;
                }
            }

        iTimer->Cancel();
        iTimer->Start( KDelayBeforeShowingGprsSuspendedNote,
                       KDelayBeforeShowingGprsSuspendedNote,
                       TCallBack( ShowGprsSuspendedNoteAfterCallBack, this ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowGprsSuspendedNoteAfterCallBack( TAny* aObject )
// ----------------------------------------------------------------------------

TInt CSysApAppUi::ShowGprsSuspendedNoteAfterCallBack( TAny* aObject )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowGprsSuspendedNoteAfterCallBack") ) );

    CSysApAppUi* appUi = STATIC_CAST( CSysApAppUi*, aObject );

    if ( appUi->iTimer )
        {
        appUi->iTimer->Cancel();
        }

    // Note is needed if call is still active and used network is not WCDMA
    TInt callType = appUi->StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallType );
    TInt networkMode = appUi->StateOfProperty( KPSUidNetworkInfo, KNWTelephonyNetworkMode );
    TRACES( RDebug::Print(
        _L("CSysApAppUi::ShowGprsSuspendedNoteAfterCallBack: callType %d networkMode %d"),
        callType, networkMode ) );
    if ( EPSCTsyCallTypeCSVoice == callType && ENWNetworkModeWcdma != networkMode )
        {
        appUi->iGprsSuspendedNoteShown = ETrue;
        }
    return KErrNone;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleAccessoryConnectedL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleAccessoryConnectedL( TAccMode aAccessoryState )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleAccessoryConnectedL( aAccessoryState: %d ) "), aAccessoryState ) );

    if ( aAccessoryState == EAccModeWirelessHeadset ||
         aAccessoryState == EAccModeWiredHeadset ||
         aAccessoryState == EAccModeHeadphones )
        {
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if ( aAccessoryState == EAccModeLoopset )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if ( aAccessoryState == EAccModeTextDevice )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if ( aAccessoryState == EAccModeWirelessCarKit || aAccessoryState == EAccModeWiredCarKit )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOn );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if ( aAccessoryState == EAccModeTVOut )
        {
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOn );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );
        }
    else if (aAccessoryState == EAccModeHDMI )
            {
            SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
                    SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
                    SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
                    SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
                    SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
                    SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff);
                    SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOn );
            }

    TInt swState( StateOfProperty( KPSUidStartup, KPSGlobalSystemState ) );
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleAccessoryConnectedL: swState: %d"), swState ) );

    if( UiReady() || swState == ESwStateSecurityCheck )
        {
        iSysApLightsController->AccessoryConnectedL( ETrue );
        }

    if ( ( ! iIgnoreAccessorySpecificProfileChanges ) && ( ! iSysApOfflineModeController->OfflineModeActive() ) )
        {
        TInt profileId( 0 );
        TInt currentProfile( 0 );
        currentProfile = ActiveProfileId();

        if ( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApAccessoryConnected ) < 1 )
            // accessory not connected already
             {
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApAccessoryConnected, 1 );
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApActiveProfileBeforeAccessoryConnected, currentProfile );
            }

        if ( aAccessoryState == EAccModeWirelessCarKit )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApWirelessCarkit );
            }
        else if( aAccessoryState == EAccModeWiredHeadset ||
                 aAccessoryState == EAccModeWirelessHeadset )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApHeadset );
            }
        else if( aAccessoryState == EAccModeLoopset  )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApLoopset );
            }
        else if( aAccessoryState == EAccModeTextDevice )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApTty );
            }
        else if( aAccessoryState == EAccModeTVOut || aAccessoryState == EAccModeHDMI )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApTvOut );
            }
        else if( aAccessoryState == EAccModeHeadphones )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApHeadphones );
            }
        else if ( aAccessoryState == EAccModeWiredCarKit )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApCarkit );
            }
        else if ( aAccessoryState == EAccModeMusicStand )
            {
            profileId = iSysApCenRepController->ProfileIdForSelectedAccessory( ESysApMusicStand );
            }

        // Carkit and music stand have also light on permanently option that needs to be checked
        if( aAccessoryState == EAccModeWiredCarKit || aAccessoryState == EAccModeMusicStand )
            {
            // When device is placed on a handsfree cradle, keyguard must be disabled (unless device is locked)
            // Note: It is assumed that if carkit/music stand is connected, the phone is in the cradle.
            if ( iKeyLockEnabled || iDeviceLockEnabled || iKeyLockOnBeforeCall || iKeyLockOnBeforeAlarm )
                {
                iKeyLockOnBeforeCradle = ETrue;

                if ( iKeyLockEnabled && !iDeviceLockEnabled )
                    {
                    KeyLock().DisableWithoutNote();
                    }
                }
            }

        if( profileId != KActiveProfile )
            {
            /*-1 because the first item in Accessory default profiles shared data values
            is KActiveProfile and that must be subtracted from the index of profile to be activated*/

            profileId -= 1;

            TRACES( RDebug::Print( _L("CSysApAppUi::HandleAccessoryConnectedL: current profile: %d, accessory profile: %d" ),
                               currentProfile, profileId ) );

            if ( profileId != currentProfile )
                {
                iAccessoryJustConnected = ETrue;
                iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileUpdateRequired, 1 );
                ActivateProfileL( profileId );
                }
            }
        }
    SetIhfIndicatorL();
    SetHacIndicatorL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleAccessoryDisconnectedL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleAccessoryDisconnectedL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleAccessoryDisconnectedL()" ) ) );


    TAccMode accessoryState(EAccModeHandPortable);
    if ( iSysApAccessoryObserver )
        {
        accessoryState = iSysApAccessoryObserver->GetAccessoryMode();
        }

    if ( accessoryState == EAccModeHandPortable )
        {
        SetIndicatorStateL( EAknIndicatorTvOut, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorLoopset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadset, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorTTY,     EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorHeadsetUnavailable, EAknIndicatorStateOff );
        SetIndicatorStateL( EAknIndicatorCarkit, EAknIndicatorStateOff );
        SetIndicatorStateL(  EAknIndicatorHDMI, EAknIndicatorStateOff );

        iSysApLightsController->AccessoryConnectedL( EFalse );

        if ( ! iIgnoreAccessorySpecificProfileChanges )
            {
            TInt activeProfile ( ActiveProfileId() );

            TInt activeProfileBeforeConnectingAccessory(
                 iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApActiveProfileBeforeAccessoryConnected )
                                                        );
            if ( activeProfileBeforeConnectingAccessory < 0 ) // error
                {
                activeProfileBeforeConnectingAccessory = 0; // General Profile
                }
            TRACES( RDebug::Print( _L("CSysApAppUi::HandleAccessoryDisconnectedL: active profile now: %d, active profile before: %d," ),
                                   activeProfile, activeProfileBeforeConnectingAccessory ) );

            if ( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileUpdateRequired ) == 1 )
                {
                iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileUpdateRequired, 0 );
                if ( activeProfile != activeProfileBeforeConnectingAccessory && ! iSysApOfflineModeController->OfflineModeActive() )
                    {
                    ActivateProfileL( activeProfileBeforeConnectingAccessory );
                    }
                }
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApAccessoryConnected, 0 );
            }
        }

    SetIhfIndicatorL();
    SetHacIndicatorL();

    // If lock disabled because of accessory, reactivate it, unless phone call or alarm is ongoing
    if ( iKeyLockOnBeforeCradle )
        {
        iKeyLockOnBeforeCradle = EFalse;
        if ( !iKeyLockOnBeforeCall && !iKeyLockOnBeforeAlarm )
            {
            if ( !iDeviceLockEnabled )
                {
                KeyLock().EnableKeyLock();
                }
            else
                {
                KeyLock().EnableAutoLockEmulation();
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleAccessoryProfileInStartupL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleAccessoryProfileInStartupL()
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::HandleAccessoryProfileInStartupL" ) ) );

    if ( !iSysApOfflineModeController->OfflineModeActive() )
        {
        iIgnoreAccessorySpecificProfileChanges = EFalse;
        TBool accessoryConnectedInShutdown( EFalse );
        TInt accessoryTemp( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApAccessoryConnected ) );
        if ( accessoryTemp == 1 )
            {
            accessoryConnectedInShutdown = ETrue;
            }

        TBool accessoryConnectedNow ( EFalse );

        TAccMode accessoryState(EAccModeHandPortable);
        if ( iSysApAccessoryObserver )
            {
            accessoryState = iSysApAccessoryObserver->GetAccessoryMode();
            }

        if ( accessoryState != EAccModeHandPortable )
            {
            accessoryConnectedNow = ETrue;
            }
        TRACES( RDebug::Print( _L( "CSysApAppUi::HandleAccessoryProfileInStartupL: accessoryConnectedInShutdown: %d, accessoryConnectedNow: %d" ),
                              accessoryConnectedInShutdown, accessoryConnectedNow ) );

        if ( accessoryConnectedInShutdown && !accessoryConnectedNow )
            {
            HandleAccessoryDisconnectedL();
            }
        else if ( !accessoryConnectedInShutdown && accessoryConnectedNow )
            {
            HandleAccessoryConnectedL( accessoryState );
            }
        else if ( !accessoryConnectedNow )
            {
            TInt activeProfile ( ActiveProfileId() );
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApActiveProfileBeforeAccessoryConnected, activeProfile );
            }
        }
    }

#ifndef RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::ResolveUidFromThread()
// ----------------------------------------------------------------------------
TInt32 CSysApAppUi::ResolveUidFromThread( TUint aThreadId ) const
    {
    TInt resolvedUid = 0;

    RThread appThread;
    TInt err = appThread.Open( aThreadId );

    if ( err == KErrNone )
        {
        resolvedUid = appThread.SecureId().iId;
        }

    appThread.Close();

    TRACES( RDebug::Print( _L("CSysApAppUi::ResolveUidFromThread: err=%d, aThreadId=%d, resolvedUid=0x%x"),
                           err, aThreadId, resolvedUid ) );

    return resolvedUid;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CloseUIAppsInHotSwapL()
// ----------------------------------------------------------------------------
void CSysApAppUi::CloseUIAppsInHotSwapL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL") ) );

    CArrayFixFlat<TInt>* wgIds=new(ELeave) CArrayFixFlat<TInt>(4);
    CleanupStack::PushL(wgIds);
    User::LeaveIfError(iCoeEnv->WsSession().WindowGroupList(0, wgIds));
    TInt lastEntry=wgIds->Count()-1;
    TRACES( RDebug::Print( _L( "CSysApAppUi::CloseUIAppsInHotSwapL: Found %d UI applications running." ), lastEntry + 1 ) );
    TInt numberOfApplicationsToShutDown( 0 );
    iApplicationScanningRoundNumber++;

    for ( TInt i=lastEntry; i>=0; i--)
        {
        CApaWindowGroupName* doomedApp = CApaWindowGroupName::NewLC(iCoeEnv->WsSession(),wgIds->At(i));

        TBool systemApp = doomedApp->IsSystem();
        TBool hiddenApp = doomedApp->Hidden();

        if ( systemApp || hiddenApp )
            {
            TRACES (
                    TPtrC caption=doomedApp->Caption();
                    RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Privileged app \"%S\" UID 0x%x will not be closed, system: %d, hidden: %d" ),
                                   &caption,
                                   doomedApp->AppUid().iUid,
                                   systemApp,
                                   hiddenApp);
                    );
            }
        else if ( ( iFileManagerCloseDisabled && doomedApp->AppUid().iUid == KFileManagerAppUid ) ||
                  doomedApp->AppUid().iUid == KAutolockAppUid )
            {
            // An additional failure protection: Autolock must not be closed in any circumstances
            TRACES (
                    TPtrC caption=doomedApp->Caption();
                    RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Privileged app \"%S\" will not be closed, UID 0x%x"),
                                   &caption,
                                   doomedApp->AppUid().iUid );
                    );
            }
        else if ( doomedApp->AppUid().iUid == 0 && !iTimeToKill )
            {
#ifdef _DEBUG
            TApaTask task(iCoeEnv->WsSession());
            task.SetWgId(wgIds->At(i));
            TUint threadId = (TUint)(task.ThreadId());
            TInt32 uid = ResolveUidFromThread( threadId );
            // CApaWindowGroupName stores the application UID, and in early application startup state AppUid()-method returns 0
            // In order not to accidentally close application that hasn't yet its window group properly set up, decicision whether
            // to close the application is postponed, in maximum to the end of the waiting period.
            TRACES ( RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Unknown app will not be closed yet, threadId=%d, UID 0x%x, size=%d"),
                                    threadId, uid, sizeof(TApaTask) ) );
#endif // _DEBUG
            numberOfApplicationsToShutDown++; // wait note must be displayed
            }
        else
            {
            numberOfApplicationsToShutDown++;
            TApaTask* task = new (ELeave) TApaTask(iCoeEnv->WsSession());
            CleanupDeletePushL(task);
            task->SetWgId(wgIds->At(i));

            TRACES (
                    const TDesC& caption = doomedApp->Caption();
                    const TDesC& docname = doomedApp->DocName();
                    const TDesC& wgname = doomedApp->WindowGroupName();
                    TUid uid = doomedApp->AppUid();
                    RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Closing app \"%S\" (ThreadId %d, WgId %d, UID 0x%X); Docname: %S, WGName : %S"),
                                   &caption,
                                   (TUint)(task->ThreadId()),
                                   wgIds->At(i),
                                   uid.iUid,
                                   &docname,
                                   &wgname);
                    );

            ResolveUidFromThread( (TUint)(task->ThreadId()) );

            if ( iApplicationScanningRoundNumber == 1 )
                {
                task->EndTask(); // applications are kindly requested to close themselves on the first round
                }
            else if ( iTimeToKill )
                {
                TBool doKill = ETrue;

                // final check, window group may still be uninitialized so use thread id for checking UID
                if ( doomedApp->AppUid().iUid == 0 )
                    {
                    if ( ResolveUidFromThread( (TUint)(task->ThreadId()) ) == KAutolockAppUid )
                        {
                        doKill = EFalse;
                        }
                    }

                if ( doKill )
                    {
                    TRACES( RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Killing app \"%S\""), &caption ) );
                    task->KillTask(); // used after timeout on the last round
                    }
                }

            CleanupStack::PopAndDestroy(); // task
            }
        CleanupStack::PopAndDestroy();  // doomedApp
        }
    CleanupStack::PopAndDestroy(); // wgIds

    if ( numberOfApplicationsToShutDown > 0 && iApplicationScanningRoundNumber <= KMaxExitTimeInHotSwap )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::CloseUIAppsInHotSwapL: Show wait note, unless already showing") ) );
        if ( !iSysApWaitNote )
            {
            HBufC* noteStringBuf = StringLoader::LoadLC( R_QTN_MEMC_WAIT_EJECT, iEikonEnv );
            iSysApWaitNote = CSysApWaitNote::NewL( iSysApFeatureManager->CoverDisplaySupported() );
            iSysApWaitNote->ShowNoteL( EClosingApplicationsNote, noteStringBuf );
            CleanupStack::PopAndDestroy();
            }

        if ( iApplicationScanningRoundNumber >= KMaxExitTimeInHotSwap )
            {
            iTimeToKill = ETrue;
            }

        if ( !iSysApTimer )
            {
            iSysApTimer = new ( ELeave ) CSysApTimer( *this );
            }

        iSysApTimer->ActivateTimerL( KApplicationScanningInterval );
        }
    else
        {
        iFileManagerCloseDisabled = EFalse;
        CompleteAppsShuttingInHotSwapL();
        }
    }

#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::TimerExpiredL()
// ----------------------------------------------------------------------------
void CSysApAppUi::TimerExpiredL()
    {
#ifndef RD_MULTIPLE_DRIVE
    CloseUIAppsInHotSwapL();
#endif // RD_MULTIPLE_DRIVE
    }

#ifndef RD_MULTIPLE_DRIVE
// ----------------------------------------------------------------------------
// CSysApAppUi::CompleteAppsShuttingInHotSwapL()
// ----------------------------------------------------------------------------
void CSysApAppUi::CompleteAppsShuttingInHotSwapL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CompleteAppsShuttingInHotSwapL iMMCEjectUsed=%d, iMMCInserted=%d"), iMMCEjectUsed, iMMCInserted ) );
    CancelWaitNote();

    if ( iMMCEjectUsed ) // From Powerkey Menu
        {
        DismountMMC();
        TRACES( RDebug::Print( _L( "CSysApAppUi::CompleteAppsShuttingInHotSwapL: Show note: Remove MMC and press OK...." ) ) );
        ShowQueryL( ESysApRemoveMmcNote );
        }
    else if ( !iMMCInserted ) // The MMC might have been already re-mounted, that's why !iMMCInserted
        {
        ShowMMCDismountedDialogL();
        }
    }
#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::InitCloseSimApplicationsL()
// ----------------------------------------------------------------------------
void CSysApAppUi::InitCloseSimApplicationsL()
    {
    iApplicationScanningRoundNumberInBtSap = 0;
    iTimeToKillInBtSap = EFalse;
    CloseSimApplicationsL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CloseSimApplicationsL()
// ----------------------------------------------------------------------------
void CSysApAppUi::CloseSimApplicationsL()
    {
    iApplicationScanningRoundNumberInBtSap++;
    TRACES( RDebug::Print( _L("CSysApAppUi::CloseSimApplicationsL: scanning round = %d"),
                           iApplicationScanningRoundNumberInBtSap ) );

    if ( iApplicationScanningRoundNumberInBtSap == KMaxExitTimeInBtSap )
        {
        iTimeToKillInBtSap = ETrue;
        }

    const TInt KNumberOfSimDependantApps( 7 );
    TUid simDependantAppUid[ KNumberOfSimDependantApps ];
    simDependantAppUid[ 0 ] = KSysApGSUid;
    simDependantAppUid[ 1 ] = KSysApSATUid;
    simDependantAppUid[ 2 ] = KSysApMailboxUid;
    simDependantAppUid[ 3 ] = KSysApSpeedDialingUid;
    simDependantAppUid[ 4 ] = KSysApSimDirectoryUid;
    simDependantAppUid[ 5 ] = KSysApMCEUid;
    simDependantAppUid[ 6 ] = KSysApCellBroadcastMessagesUid;

    TBool appsExiting( EFalse );
    TApaTaskList apaTaskList( CCoeEnv::Static()->WsSession() );

    for ( TInt i = 0; i < KNumberOfSimDependantApps; i++ )
        {
        TApaTask apaTask = apaTaskList.FindApp( simDependantAppUid[ i ] );
        if ( apaTask.Exists() )
            {
            appsExiting = ETrue;
            if ( iApplicationScanningRoundNumberInBtSap == 1 )
                {
                apaTask.EndTask();  // applications are kindly requested to close themselves on the first round
                }
            else if ( iTimeToKillInBtSap )
                {
                apaTask.KillTask(); // used after timeout on the last round
                }
            }
        }

    if ( !appsExiting || iTimeToKillInBtSap )
        {
        iSysApBtSapController->SimApplicationsClosed();
        }
    else
        {
        if ( !iSapTimer )
            {
            iSapTimer = CPeriodic::NewL( EPriorityNormal );
            }
        iSapTimer->Cancel();
        iSapTimer->Start( KDelayBeforeNextScanningRound,
                          KDelayBeforeNextScanningRound,
                          TCallBack( DoCloseSimApplicationsAfterCallBack, this ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoCloseSimApplicationsAfterCallBack
// ----------------------------------------------------------------------------
TInt CSysApAppUi::DoCloseSimApplicationsAfterCallBack( TAny* aObject )
    {
    CSysApAppUi* appUi = STATIC_CAST( CSysApAppUi*, aObject );

    if ( appUi->iSapTimer )
        {
        appUi->iSapTimer->Cancel();
        }

    TRAP_IGNORE( appUi->CloseSimApplicationsL() );

    return KErrNone;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::AlertUiAlertingL( const TBool aAlerting )
// ----------------------------------------------------------------------------
void CSysApAppUi::AlertUiAlertingL( const TBool aAlerting )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::AlertUiAlerting aAlerting:%d"), aAlerting ) );

    if ( aAlerting )
        {
        TInt swState;
        RProperty::Get( KPSUidStartup, KPSGlobalSystemState, swState );

        if( swState == ESwStateCharging )
            {
            SetStarterState( RStarterSession::EAlarm );
            }
        }

    iSysApLightsController->AlarmOccuredL( aAlerting );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::AlertUiKeyLockOff( const TBool aKeyLockOff )
// ----------------------------------------------------------------------------

void CSysApAppUi::AlertUiKeyLockOff( const TBool aKeyLockOff )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::AlertUiKeyLockOff aKeyLockOff:%d, devicelock:%d"), 
        aKeyLockOff, iDeviceLockEnabled ) );
    
    if( aKeyLockOff )
        {
        // Notification to release keypad might come several times if e.g. calls are received
        if ( !iKeyLockOnBeforeAlarm )
            {
            if ( iKeyLockEnabled || iKeyLockOnBeforeCradle || iKeyLockOnBeforeCall )
                {
                iKeyLockOnBeforeAlarm = ETrue;
                }
            }
        KeyLock().DisableWithoutNote();
        }
    else
        {
        if (iDeviceLockEnabled)
            {
            // Re-enable keylock in devicelock case only if phone call is not ongoing or is 
            // connected. Otherwise e.g. messages can be read using softkeys during ongoing call.
            TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );
            if ( callState == EPSCTsyCallStateNone || callState == EPSCTsyCallStateConnected )
                {
                KeyLock().EnableAutoLockEmulation();
                }
            }
        else if ( iKeyLockOnBeforeAlarm && !iKeyLockOnBeforeCradle && !iKeyLockOnBeforeCall )
            {
            KeyLock().EnableKeyLock();
            }
        iKeyLockOnBeforeAlarm = EFalse;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleLightsRequireL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleLightsRequireL() const
    {
    iSysApLightsController->HandleLightsRequireL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleRawKeyEventLightsRequireL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleRawKeyEventLightsRequireL() const
    {
    iSysApLightsController->HandleRawKeyEventLightsRequireL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleForcedLightsVTRequireL( const TInt aLightParameter )
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleForcedLightsVTRequireL( const TInt aLightsParameter ) const
    {
    iSysApLightsController->HandleForcedLightsVTRequireL( aLightsParameter );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleForcedLightsSSRequireL( const TInt aLightParameter )
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleForcedLightsSSRequireL( const TInt aLightsParameter ) const
    {
    iSysApLightsController->HandleForcedLightsSSRequireL( aLightsParameter );
    }

// ---------------------------------------------------------------------------- 
// CSysApAppUi::HandleForcedLightsATCRequireL( const TInt aLightParameter ) 
// ---------------------------------------------------------------------------- 
void CSysApAppUi::HandleForcedLightsATCRequireL( const TInt aLightsParameter ) const 
    { 
    iSysApLightsController->HandleForcedLightsATCRequireL( aLightsParameter ); 
    } 
      
// ----------------------------------------------------------------------------
// CSysApAppUi::SwitchLightsOnSoftRejectL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SwitchLightsOnSoftRejectL()
    {
    iSysApLightsController->SwitchLightsOnSoftRejectL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::StateOfProperty()
// ----------------------------------------------------------------------------
TInt CSysApAppUi::StateOfProperty( const TUid& aCategory, const TUint aKey ) const
    {
    TInt err( KErrNone );
    TInt value( 0 );
    err = RProperty::Get( aCategory, aKey, value );
    if ( err )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::StateOfProperty. RProperty::Get: err=%d"), err ) );
        return err;
        }
    return value;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::BluetoothPowerMode()
// ----------------------------------------------------------------------------
TInt CSysApAppUi::BluetoothPowerMode() const
    {
    return iSysApCenRepBtObserver->BluetoothPowerMode();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ActivateProfileL( const TInt aProfileId ) const
// ----------------------------------------------------------------------------

void CSysApAppUi::ActivateProfileL( const TInt aProfileId ) const
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ActivateProfile aProfileId: %d"), aProfileId ) );
    //No use of returning the Profile error code since it only tells if the activation was succesfull or not.
    iProfileEngine->SetActiveProfileL( aProfileId );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ActivateOnlineProfileL() const
// ----------------------------------------------------------------------------

void CSysApAppUi::ActivateOnlineProfileL() const
    {
    ActivateProfileL( iProfileToBeActivated );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ActiveProfileNameL( TPtr aProfileName ) const
// ----------------------------------------------------------------------------

void CSysApAppUi::ActiveProfileNameL( TPtr aProfileName )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::ActiveProfileNameL START") ) );
    MProfile* profile = iProfileEngine->ActiveProfileL();
    const MProfileName& mProfileName = profile->ProfileName();
    aProfileName = mProfileName.Name();
    profile->Release();
    TRACES( RDebug::Print( _L("CSysApAppUi::ActiveProfileNameL END") ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CheckSilentModeL()
// ----------------------------------------------------------------------------
void CSysApAppUi::CheckSilentModeL()
    {
    TBool isSilent( EFalse );
    MProfile* profile = iProfileEngine->ActiveProfileL();
    isSilent = profile->IsSilent();
    profile->Release();
    if ( isSilent )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::CheckSilentModeL: active profile is SILENT") ) );
        iSysApLightsController->SetSilentModeOn( ETrue );
        }
    else
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::CheckSilentModeL: active profile is NOT SILENT") ) );
        iSysApLightsController->SetSilentModeOn( EFalse );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleProfileChangedL( const TInt aValue )
// ----------------------------------------------------------------------------
void CSysApAppUi::HandleProfileChangedL( const TInt aNewProfileId )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleProfileChangedL: aNewProfileId=%d"), aNewProfileId ) );
    CheckSilentModeL();
    if ( iHandleNextProfileEvent )
        {
        if ( iAccessoryJustConnected )
            {
            iAccessoryJustConnected = EFalse;
            }
        else
            {
            // if the user delibarately changes profiles, the selected profile will
            // remain active after disconnecting an accessory.
            iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileUpdateRequired, 0 );
            }

		// Added as a part of REQ 415-6750 : Conditional UI-note for "Activated profile"
        TInt   noNeedToShowTheNote = 0;      // if True, then note should not be shown
        TInt    err = KErrNone;                            
                                     
      	err = RProperty::Get( KPSUidCoreApplicationUIs, KCoreAppUIsProfileActivatedNote, 
                              noNeedToShowTheNote );   
		/*
  		If there occured an error, it is likely caused by the non-existing property. 
  		So trying to set it only if no error occured for maintaining the default behavior. 
		*/       
        if( KErrNone == err )
            { 
            TInt error = RProperty::Set( KPSUidCoreApplicationUIs, KCoreAppUIsProfileActivatedNote, 
                                     ECoreAppUIsProfileActivatedNoteNotShown );
            }
               
        if(!noNeedToShowTheNote)  //    The value doesn't deny the showing, then...
            {
            ShowProfileNoteL();
            }    
        }

    if ( aNewProfileId != KOfflineModeProfileId )
        {
        iActiveProfileBeforeOfflineMode = aNewProfileId;
        iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApProfileBeforeOfflineMode, aNewProfileId );
        }

    if ( aNewProfileId == KOfflineModeProfileId && ! iSysApOfflineModeController->OfflineModeActive() )
        {
        if ( BtSapEnabled() )
            {
            // Choosing "Offline Mode" will disconnect BT SAP
            // SwitchFromOnlineToOfflineModeL() will be called from CSysApBtSapController
            iSysApBtSapController->SwitchingToOffline();
            iSysApBtSapController->Disconnect();
            }
        else
            {
            SwitchFromOnlineToOfflineModeL();
            }
        }
    else if ( aNewProfileId != KOfflineModeProfileId && iSysApOfflineModeController->OfflineModeActive() )
        {
        if ( BtSapEnabled() )
            {
            iSysApOfflineModeController->DoNotActivateRF(); // Will be activated after BT SAP disconnection
            }
        iSysApOfflineModeController->SwitchFromOfflineToOnlineModeL();
        }
    iHandleNextProfileEvent = ETrue;
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::RestoreProfileL()
// ----------------------------------------------------------------------------

void CSysApAppUi::RestoreProfileL( const TBool aOffline )
    {
    iHandleNextProfileEvent = EFalse;
    if ( aOffline )
        {
        ActivateProfileL( KOfflineModeProfileId );
        }
    else
        {
        ActivateProfileL( iActiveProfileBeforeOfflineMode );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ActiveProfileId()
// ----------------------------------------------------------------------------

TInt CSysApAppUi::ActiveProfileId()
    {
    return iProfileEngine->ActiveProfileId();
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::AddMmcMenuItemsL()
// ----------------------------------------------------------------------------
//
void CSysApAppUi::AddMmcMenuItemsL( CDesCArray*& aProfileNameCDesCArray, RArray<TInt>& aItemIdArray,
                                    TInt& aPowerMenuItemIndex )
    {
    TInt propertyValue( StateOfProperty( KPSUidUsbWatcher,
            KUsbWatcherSelectedPersonality ) );

    HBufC* itemStringBuf;
#ifndef RD_MULTIPLE_DRIVE
    iPowerkeyMenuEjectSelection = KErrAccessDenied;
    if ( !IsEncryptionOperationOngoingL() )
        {
        if ( iSysApFeatureManager->MmcHotSwapSupported() &&
                iMMCInserted &&
                iSysApFeatureManager->EjectRequiredInPowerMenu() &&
                propertyValue != KUsbPersonalityIdMS )
            {
            iPowerkeyMenuEjectShown = ETrue;
            TRACES( RDebug::Print(_L("CSysApAppUi::AddMmcMenuItemsL: adding \"Eject\"" ) ) );
            itemStringBuf = StringLoader::LoadLC( R_QTN_PWRC_EJECT_MMC, iEikonEnv );
            aProfileNameCDesCArray->AppendL( itemStringBuf->Des() );
            CleanupStack::PopAndDestroy(); // itemStringBuf
            if ( iSysApFeatureManager->CoverDisplaySupported() )
                {
                aItemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemEjectMMC);
                }
            iPowerkeyMenuEjectShown = ETrue;
            iPowerkeyMenuEjectSelection = aPowerMenuItemIndex;
            aPowerMenuItemIndex++;
            }
        }

#else // RD_MULTIPLE_DRIVE
    iPowerkeyMenuEjectSelectionBase = KErrAccessDenied;
    if ( !IsEncryptionOperationOngoingL() )
        {
        if ( iSysApFeatureManager->MmcHotSwapSupported()
                && iSysApFeatureManager->EjectRequiredInPowerMenu()
                && propertyValue != KUsbPersonalityIdMS )
            {
            // Reset old eject status and dialog
            iSysApDriveList->ResetDrivesToEject();
            if ( iSysApConfirmationQuery )
                {
                if ( iSysApConfirmationQuery->CurrentQuery() == ESysApEjectMmcQuery )
                    {
                    iSysApConfirmationQuery->Cancel();
                    }
                }

            // Append memory cards for eject selection
            TInt count( iInsertedMemoryCards.Count() );
            TInt corruptedMedia = 0;
            for ( TInt i( 0 ); i < count; ++i )
                {
                TVolumeInfo info;
                TInt err = iEikonEnv->FsSession().Volume(info, iInsertedMemoryCards[ i ].iDrive);
                TRACES( RDebug::Print(_L("CSysApAppUi::AddMmcMenuItemsL: corrupted media, iDrive (%d) Error: %d" ),iInsertedMemoryCards[ i ].iDrive, err ) );
                if(err != KErrNone)
                    {
                    corruptedMedia++;
                    continue;
                    }
                itemStringBuf = iSysApDriveList->GetFormattedDriveNameLC(
                        iInsertedMemoryCards[ i ].iDrive,
                        R_QTN_PWRC_EJECT_MEMORY_STORAGE );
                aProfileNameCDesCArray->AppendL( *itemStringBuf );
                CleanupStack::PopAndDestroy( itemStringBuf );

                if ( iSysApFeatureManager->CoverDisplaySupported() )
                    {
                    aItemIdArray.AppendL(
                            SecondaryDisplay::EPwrMenuItemEjectItemBase + i );
                    }
                }
            if ( (count-corruptedMedia) > 0 )
                {
                TRACES( RDebug::Print(_L("CSysApAppUi::AddMmcMenuItemsL: added \"Eject\"" ) ) );
                iPowerkeyMenuEjectShown = ETrue;
                iPowerkeyMenuEjectSelectionBase = aPowerMenuItemIndex;
                aPowerMenuItemIndex += (count-corruptedMedia);
                }
            }
        }
#endif // RD_MULTIPLE_DRIVE
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::ShowPowerKeyPopUpMenuL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowPowerKeyPopUpMenuL()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL iPowerKeyPopupMenuActive: %d" ),
                          iPowerKeyPopupMenuActive ) );

    if ( !iPowerKeyPopupMenuActive && !iDisablePowerkeyMenu 
         && iSysApPowerKeyMenuObserver ) // normal state construction has been executed
        {
        iPowerKeyPopupMenuDismissed = EFalse;
        iPowerkeyMenuPowerOffShown = EFalse;
        CancelGlobalListQuery();

        iGlobalListQuery = CAknGlobalListQuery::NewL();

        iSysApPowerKeyMenuObserver->Cancel();

        CDesCArray* profileNameCDesCArray; // Array for Powerkey Menu items

        RArray<TInt> itemIdArray; // needed for cover UI
        CleanupClosePushL(itemIdArray);

        // make sure old profile names array is clean
        delete iProfileNamesArray;
        iProfileNamesArray = NULL;

        // Must pop iProfileNamesArray here as cannot leave trap harness with uneven push/pop count.
        // This is because profileEngine doesn't provide non-LC version of ProfilesNamesArrayLC
        TRAPD( err,
              iProfileNamesArray = iProfileEngine->ProfilesNamesArrayLC();
              CleanupStack::Pop();
             );

        if ( err != KErrNone )
            {
            // creating menu failed, return (i.e. just don't show the menu)
            TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL iProfileEngine->ProfilesNamesArrayLC() ERROR: %d" ), err ) );
            iNumberOfProfileNamesInPowerKeyMenu = 0;
            }
        else
            {
            iNumberOfProfileNamesInPowerKeyMenu = iProfileNamesArray->MdcaCount();
            TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL iNumberOfProfileNamesInPowerKeyMenu: %d" ),
                                  iNumberOfProfileNamesInPowerKeyMenu ) );
            }

        if ( BtSapEnabled() )
            {
            profileNameCDesCArray = new( ELeave ) CDesCArrayFlat( iNumberOfProfileNamesInPowerKeyMenu + 4 );
            }
        else
            {
            profileNameCDesCArray = new( ELeave ) CDesCArrayFlat( iNumberOfProfileNamesInPowerKeyMenu + 3 );
            }

        CleanupStack::PushL( profileNameCDesCArray );
        profileNameCDesCArray->Reset();
        HBufC* itemStringBuf;

        TInt powerMenuItemIndex = 0;

        // "Switch off" menu item
        if ( !IsEncryptionOperationOngoingL() )
            {
        itemStringBuf = StringLoader::LoadLC( R_QTN_PWRC_SWITCH_OFF, iEikonEnv );
        profileNameCDesCArray->InsertL( 0, itemStringBuf->Des() );
        CleanupStack::PopAndDestroy(); // itemStringBuf
        if ( iSysApFeatureManager->CoverDisplaySupported() )
            {
            itemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemSwitchOff);
            }
            iPowerkeyMenuPowerOffShown = ETrue;
        powerMenuItemIndex++;
            }

        // PHOREPOC delta (2026-10-15, user): "Restart" right after "Switch off" -- SysAp's own reset path
        // (DoShutdownL( ETrue ) -> SSM -> the board's power controller, which comes back to the same slot)
        PhRestartSelection = KErrNotFound;
        if ( !IsEncryptionOperationOngoingL() )
            {
            profileNameCDesCArray->InsertL( powerMenuItemIndex, _L("Restart") );
            PhRestartSelection = powerMenuItemIndex;
            powerMenuItemIndex++;
            }
        PhShellSelection = KErrNotFound;
        if ( !IsEncryptionOperationOngoingL() && PheShellsInRom() == ( KPheShellsS3 | KPheShellsS4 ) )
            {
            TInt shell = KPheShellS3;
            CRepository* rep = NULL;
            TRAP_IGNORE( rep = CRepository::NewL( KPheDevMenuRepoUid ) );
            if ( rep ) { rep->Get( KPheDevKeyShell, shell ); delete rep; }
            PhShellTarget = shell == KPheShellS4 ? KPheShellS3 : KPheShellS4;
            profileNameCDesCArray->InsertL( powerMenuItemIndex,
                PhShellTarget == KPheShellS4 ? _L("Switch to Symbian^4") : _L("Switch to Symbian^3") );
            PhShellSelection = powerMenuItemIndex;
            powerMenuItemIndex++;
            }

        iPowerkeyMenuLockKeypadShown     = EFalse;
        iPowerkeyMenuExitSapShown        = EFalse;
        iPowerkeyMenuLockSystemShown     = EFalse;
        iPowerkeyMenuEjectShown          = EFalse;

        iPowerkeyMenuLockKeypadSelection = KErrAccessDenied;
        iPowerkeyMenuExitSapSelection    = KErrAccessDenied;
        iPowerkeyMenuLockSystemSelection = KErrAccessDenied;

#ifndef RD_MULTIPLE_DRIVE
        iPowerkeyMenuEjectSelection     = KErrAccessDenied;
#else // RD_MULTIPLE_DRIVE
        iPowerkeyMenuEjectSelectionBase = KErrAccessDenied;
#endif // RD_MULTIPLE_DRIVE

        // "Lock keypad" menu item
		
		TInt callState ( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );
		TInt callType ( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallType ) );
		if ( !( callState == EPSCTsyCallStateConnected && callType == EPSCTsyCallTypeH324Multimedia ) )
			{
       /* Commenting code for disable lock keys and screen when cover is Open
		if ( ( iSysApFeatureManager->GripNotSupported() && !iSysApFeatureManager->SlideSupported() ) ||
             ( (!iSysApFeatureManager->GripNotSupported() || iSysApFeatureManager->SlideSupported() ) && 
             ( StateOfProperty( KPSUidHWRM, KHWRMGripStatus ) ==  EPSHWRMGripClosed ) ) )
            // "Lock keypad" command is shown always when there is no grip, and if there
            // there is no grip, only when the grip is closed.
				{
		*/
				if ( CKeyLockPolicyApi::KeyguardAllowed() )
					{
					if ( iSysApFeatureManager->PenEnabled() )
						{
						itemStringBuf = StringLoader::LoadLC( R_QTN_PWRC_LOCK_DISPLAY, iEikonEnv );
						}
					else
						{
						itemStringBuf = StringLoader::LoadLC( R_QTN_PWRC_LOCK_KEYS, iEikonEnv );
						}                
					profileNameCDesCArray->AppendL( itemStringBuf->Des() );
					CleanupStack::PopAndDestroy(); // itemStringBuf
					if ( iSysApFeatureManager->CoverDisplaySupported() )
						{
						itemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemLockKeypad);
                        }
                    iPowerkeyMenuLockKeypadShown = ETrue;
                    iPowerkeyMenuLockKeypadSelection = powerMenuItemIndex;
                    powerMenuItemIndex++;
                    }
           //     }
              } 
        if ( iSysApFeatureManager->PowerKeyIsLockKey() )
            {
            AddMmcMenuItemsL( profileNameCDesCArray,
                    itemIdArray, powerMenuItemIndex );
            }
	    // "Exit SIM access profile" menu item
        if ( BtSapEnabled() )
            {
            TRACES( RDebug::Print(_L( "CSysApAppUi::ShowPowerKeyPopUpMenuL: show \"Exit SIM access profile\" item" ) ) );
            itemStringBuf = StringLoader::LoadLC( R_QTN_PWRC_EXIT_SIM_ACCESS, iEikonEnv );
            profileNameCDesCArray->AppendL( itemStringBuf->Des() );
            CleanupStack::PopAndDestroy(); // itemStringBuf
            if ( iSysApFeatureManager->CoverDisplaySupported() )
                {
                itemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemExitBtSap);
                }
            iPowerkeyMenuExitSapShown = ETrue;
            iPowerkeyMenuExitSapSelection = powerMenuItemIndex;
            powerMenuItemIndex++;
            }

        // Profile menu items
        iProfileItemsOffset = powerMenuItemIndex;
        TInt arrayIndex( 0 );
        TBufC<KMaxProfileNameLength> profileName;

        for ( arrayIndex = 0; arrayIndex < iNumberOfProfileNamesInPowerKeyMenu; arrayIndex++ )
            {
            profileName = iProfileNamesArray->MdcaPoint( arrayIndex );
            TPtr profileNamePtr = profileName.Des();
            AknTextUtils::DisplayTextLanguageSpecificNumberConversion( profileNamePtr );
            profileNameCDesCArray->AppendL( profileNamePtr );

            if ( iSysApFeatureManager->CoverDisplaySupported() )
                {
                TInt profileId = ( iProfileNamesArray->ProfileName( arrayIndex ) )->Id();
                itemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemProfileItemBase+profileId);
                }
            powerMenuItemIndex++;
            }

        // "Lock device" menu item

        callState = StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState );
        
        TInt devLockStatus( EAutolockStatusUninitialized );
        devLockStatus = StateOfProperty( KPSUidCoreApplicationUIs, KCoreAppUIsAutolockStatus );  // check whether Autolock has been started
        

        if ( callState == EPSCTsyCallStateNone && devLockStatus != EAutolockStatusUninitialized )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL: adding \"Lock device\", devLockStatus=%d" ), devLockStatus ) );
			iPowerkeyMenuLockSystemShown = ETrue;
            itemStringBuf = StringLoader::LoadLC( R_QTN_SET_SEC_LOCK_SYSTEM, iEikonEnv );
            profileNameCDesCArray->AppendL( itemStringBuf->Des() );
            CleanupStack::PopAndDestroy(); // itemStringBuf
            if ( iSysApFeatureManager->CoverDisplaySupported() )
                {
                itemIdArray.AppendL(SecondaryDisplay::EPwrMenuItemLockDevice);
                }
            iPowerkeyMenuLockSystemShown = ETrue;
            iPowerkeyMenuLockSystemSelection = powerMenuItemIndex;
            powerMenuItemIndex++;
            }
        if ( !iSysApFeatureManager->PowerKeyIsLockKey() )
            {
                AddMmcMenuItemsL( profileNameCDesCArray, itemIdArray, powerMenuItemIndex );
            }
        
        // Activate/deactive power save mode
        if ( iSysApPsmController ) // variable feature, not create if power save is not used
            {
            TBool showActivate = !(iSysApPsmController->FullPsmEnabled());
            
            TInt textId = ( showActivate ? R_QTN_PWRC_ACTIVATE_POWER_SAVING : R_QTN_PWRC_DEACTIVATE_POWER_SAVING );
            
            itemStringBuf = StringLoader::LoadLC( textId, iEikonEnv );
            iSysApPsmController->SetNextUiOperation( showActivate );
            
            if ( iSysApFeatureManager->CoverDisplaySupported() )
                {
                itemIdArray.AppendL( showActivate ? 
                                     SecondaryDisplay::EPwrMenuItemActivatePowerSaving :
                                     SecondaryDisplay::EPwrMenuItemDeactivatePowerSaving );
                }
                
            profileNameCDesCArray->AppendL( itemStringBuf->Des() );
            CleanupStack::PopAndDestroy( itemStringBuf );
                
            iSysApPsmController->SetUiItemId( powerMenuItemIndex );
            powerMenuItemIndex++;    
            }
        
        TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL NumberOfAllItemsInPowerKeyMenu: %d" ),
                              profileNameCDesCArray->Count() ) );

        iSysApPowerKeyMenuObserver->Start();

        // Set secondary display data if necessary
        if ( iSysApFeatureManager->CoverDisplaySupported() )
            {
            CAknSDData* sd = CAknSDData::NewL(SecondaryDisplay::KCatSysAp, SecondaryDisplay::ECmdShowPowerKeyListQuery, KNullDesC8);
            sd->InsertGlobalListQueryItemIdsL(itemIdArray);
            iGlobalListQuery->SetSecondaryDisplayData(sd); // ownership to notifier client
            }

        iGlobalListQuery->ShowListQueryL( (MDesCArray*) profileNameCDesCArray,
                                          iSysApPowerKeyMenuObserver->iStatus,
                                          KProfileListInitialIndex );

        TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL: list query shown" ) ) );

        CleanupStack::PopAndDestroy( profileNameCDesCArray ); // profileNameCDesCArray
        TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL: profileNameCDesCArray popped" ) ) );

        CleanupStack::Pop(&itemIdArray);
        itemIdArray.Close();

        // Without following variable set ETrue powerkey up event would change the hightlighted item to be
        // the second one instead of the wanted first one.
        iIgnoreNextPowerKeyUpEvent = ETrue;

        if ( iSysApBatteryInfoController )        
            {
            iSysApBatteryInfoController->PowerMenuShownL();
            }
        
        TRACES( RDebug::Print(_L("CSysApAppUi::ShowPowerKeyPopUpMenuL:end" ) ) );
    }
}

// ----------------------------------------------------------------------------
// CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL( TInt aSelection )
// ----------------------------------------------------------------------------

void CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL( TInt aSelection )
    {
    iPowerKeyPopupMenuDismissed = ETrue;
    iPowerKeyPopupMenuActive = EFalse;
    TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: START aSelection:%d" ), aSelection ) );
    if ( aSelection == KPowerKeyMenuSelectionCancelled )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: Powerkey menu cancelled" ) ) );
        }
    else if( aSelection < KPowerKeyMenuSelectionCancelled )
        {
        iIgnoreNextPowerKeyUpEvent = EFalse;
        }
    else
        {
        // PHOREPOC: "Restart"
        if ( PhShellSelection != KErrNotFound && aSelection == PhShellSelection )
            {
            CRepository* rep = NULL;
            TRAP_IGNORE( rep = CRepository::NewL( KPheDevMenuRepoUid ) );
            if ( rep ) { rep->Set( KPheDevKeyShell, PhShellTarget ); delete rep; }
            if ( !PhRestartCb )
                {
                PhRestartCb = new ( ELeave ) CAsyncCallBack( TCallBack( PhDoRestart, this ), CActive::EPriorityStandard );
                }
            PhRestartCb->CallBack();
            }
        else if ( PhRestartSelection != KErrNotFound && aSelection == PhRestartSelection )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Restart\" selected" ) ) );
            if ( !PhRestartCb )
                {
                PhRestartCb = new ( ELeave ) CAsyncCallBack( TCallBack( PhDoRestart, this ), CActive::EPriorityStandard );
                }
            PhRestartCb->CallBack();
            }
        // first menu item <=> Switch off
        else if ( aSelection == KPowerKeyMenuSelectionSwitchOff )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Switch off\" selected" ) ) );
            DoShutdownL( EFalse, KDummyReason );
            }
        // 2nd menu item: lock display & keys
        else if ( iPowerkeyMenuLockKeypadShown && aSelection == iPowerkeyMenuLockKeypadSelection )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Lock keypad\" selected" ) ) );
            KeyLock().EnableKeyLock();
            }
        // BT
        else if ( iPowerkeyMenuExitSapShown && aSelection == iPowerkeyMenuExitSapSelection )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Exit SAP\" selected" ) ) );
            ShowQueryL( ESysApBtSapDisconnectQuery );
            }
        // Profile Items
        else if ( ( aSelection >= iProfileItemsOffset ) && ( aSelection < iProfileItemsOffset + iNumberOfProfileNamesInPowerKeyMenu ) )
            {
            __ASSERT_DEBUG( iProfileNamesArray, User::Invariant() );

            if ( iProfileNamesArray )
                {
                iProfileToBeActivated = ( iProfileNamesArray->ProfileName( aSelection - iProfileItemsOffset ) )->Id();

                TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: profile id: %d selected" ), iProfileToBeActivated ) );
                if ( ! iSysApOfflineModeController->OfflineModeActive() ||
                    ( iProfileToBeActivated ) == KOfflineModeProfileId )
                    {
                    ActivateProfileL( iProfileToBeActivated );
                    }
                else
                    {
                    // iProfileToBeActivated profile will be activated from iSysApOfflineModeController if ok
                    iSysApOfflineModeController->GoOnlineIfOkL();
                    }
                }
            }
        // device lock
        else if ( iPowerkeyMenuLockSystemShown && aSelection == iPowerkeyMenuLockSystemSelection )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Lock system\" selected" ) ) );
            iSysApSystemLock->SetLockedL();
            }
#ifndef RD_MULTIPLE_DRIVE
        //eject single MMC
        else if ( iPowerkeyMenuEjectShown && aSelection == iPowerkeyMenuEjectSelection )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Eject\" selected" ) ) );
            ShowQueryL( ESysApEjectMmcQuery );
            }
#else // RD_MULTIPLE_DRIVE
        //eject nth MMC
        else if ( iPowerkeyMenuEjectShown &&
                aSelection >= iPowerkeyMenuEjectSelectionBase &&
                aSelection < iPowerkeyMenuEjectSelectionBase + iInsertedMemoryCards.Count() )
            {
            iDriveToEject =
                iInsertedMemoryCards[ aSelection - iPowerkeyMenuEjectSelectionBase ].iDrive;
            TRACES( RDebug::Print(
                _L( "CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Eject\" selected, drive=%d" ),
                iDriveToEject ) );
            iSysApDriveList->ResetDrivesToEject();
            RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
            EjectMMCL();
            }
#endif // RD_MULTIPLE_DRIVE
        else if ( iSysApPsmController && aSelection == iSysApPsmController->UiItemId() )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: \"Activate|Deactivate power save\" selected" ) ) );
            iSysApPsmController->DoEnableFullPsm( iSysApPsmController->NextUiOperation() );
            }

        }

    delete iProfileNamesArray;
    iProfileNamesArray = NULL;
    delete iGlobalListQuery;
    iGlobalListQuery = NULL;

    TRACES( RDebug::Print(_L("CSysApAppUi::PowerKeyPopUpMenuSelectionDoneL: END" ) ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetDeviceLockEnabledL( const TBool aLockEnabled )
// ----------------------------------------------------------------------------

void CSysApAppUi::SetDeviceLockEnabledL( const TBool aLockEnabled )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::SetDeviceLockEnabledL aLockEnabled:%d" ), aLockEnabled ) );
    TBool currentState( iDeviceLockEnabled );
    
    iDeviceLockEnabled = aLockEnabled;
    
    if ( currentState != iDeviceLockEnabled ) // filter out unnecessary light status modifications
        {
        iSysApLightsController->DeviceLockStateChangedL( iDeviceLockEnabled );
        }
    
 /*   if( iDeviceLockEnabled )
        {
        if( iKeyLockEnabled )
            {
            KeyLock().DisableWithoutNote();
            }
        }*/
    if(! iDeviceLockEnabled )
        {
        // let's not activate keylock in case device lock was disabled during call or in cradle
        iKeyLockOnBeforeCall = EFalse;
        iKeyLockOnBeforeCradle = EFalse;

        // Memory card needs to be unlocked when device is unlocked
        if ( iSysApFeatureManager->MmcHotSwapSupported() )
            {
            RunUnlockNotifierL();
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoNotEnableKeylock()
// ----------------------------------------------------------------------------

void CSysApAppUi::DoNotEnableKeylock()
    {
    if ( !iKeyLockOnBeforeCradle )
        {
        iShowkeypadActivatedNoteAfterSoftReject = iKeyLockOnBeforeCall;
        }

    iKeyLockOnBeforeCall = EFalse;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoShutdownL( const TBool aReset, const TSWStartupReason aSWStartupReason )
// ----------------------------------------------------------------------------

void CSysApAppUi::DoShutdownL( const TBool aReset, const TInt aResetReason )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL: aResetReason:%d, aReset:%d" ),
        aResetReason, aReset ) );
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    TBool animationenabled( EFalse );
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

    if( OkToInitiateShutdown() )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL: Was OkToToInitiateShutdown" ) ) );


        if ( !aReset && iSysApFeatureManager->Supported(KSysApFeatureIdGoodbyeNote) )
            {
            TRAPD( ignore, ShowShutdownNoteL() );
            if ( ignore ) ignore = 0; // hide compiler warning about not using variable
            }

        if( !aReset )
            {
    #ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION
            TRAPD( err, ShowAnimationL() );
            if ( err )
                {
                TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL ShowAnimationL() leaved: %d" ), err ) );
                CompleteShutdown(aReset, aResetReason);
                }
            }
        else // aReset
            {
            CompleteShutdown(aReset, aResetReason);
            }
    #else // RD_STARTUP_ANIMATION_CUSTOMIZATION
            TRAPD( err, animationenabled = ShowAnimationL() );
            if ( err )
                {
                TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL ShowAnimationL() leaved: %d" ), err ) );
                }
            }

        if ( !animationenabled )
            {
            CompleteShutdown(aReset, aResetReason);
            }
    #endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
        }
    else
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL: Was not OkToToInitiateShutdown" ) ) );
        }
    TRACES( RDebug::Print(_L("CSysApAppUi::DoShutdownL: END" ) ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ContinueShutdown()
// ----------------------------------------------------------------------------

void CSysApAppUi::ContinueShutdown()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ContinueShutdown() started" ) ) );
    CompleteShutdown();
    TRACES( RDebug::Print(_L("CSysApAppUi::ContinueShutdown() completed" ) ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowAnimationL()
// ----------------------------------------------------------------------------

#ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION
void
#else // RD_STARTUP_ANIMATION_CUSTOMIZATION
TBool
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
CSysApAppUi::ShowAnimationL()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ShowAnimationL(): START" ) ) );

    TRACES( RDebug::Print( _L("CSysApAppUi::ShowAnimationL: Initialise shutdown animation") ) );

#ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION

    PrepareForShutdownAnimation();

    iSysApShutdownAnimation = CSysApShutdownAnimation::NewL( *iSysApShutdownImage );
    iSysApShutdownAnimation->Play( TCallBack( DoStopAnimTiming, this ) );

    TRACES( RDebug::Print(_L("CSysApAppUi::ShowAnimationL(): End" ) ) );

#else // RD_STARTUP_ANIMATION_CUSTOMIZATION
    iSysApShutdownAnimation = CSysApShutdownAnimation::NewL( this );
    AddToStackL( iSysApShutdownAnimation );
    iAnimationShowingTime = iSysApShutdownAnimation->ShowingTime();
    TRACES( RDebug::Print( _L("CSysApAppUi::ShowAnimationL: Shutdown animation initialised. Animation time = %d") ,iAnimationShowingTime) );

    TBool ret_val( EFalse );

    if ( iAnimationShowingTime )
        {
        if ( iSysApFeatureManager->CoverDisplaySupported() )
            {
            // Construct mediator observer
            iSysApMediatorObserver = CSysApMediatorObserver::NewL( this );

            // Sync animation
            TInt err = iSysApMediatorObserver->SyncShutdownAnimation();

            if ( err != KErrNone )
                {
                // Pretend coverUI synced instantly if error in issuing command.
                ShutdownAnimationSyncOK();
                }
            }
        else
            {
            // Pretend coverUI synced instantly when it is not supported.
            ShutdownAnimationSyncOK();
            }

        ret_val = ETrue;
        }

    TRACES( RDebug::Print(_L("CSysApAppUi::ShowAnimationL(): returns: %d" ),ret_val ) );
    return ret_val;
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
    }

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
// ----------------------------------------------------------------------------
// CSysApAppUi::ShutdownAnimationSyncOK()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShutdownAnimationSyncOK()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ShutdownAnimationSyncOK(): Call PrepareForShutdownAnimation()" ) ) );
    PrepareForShutdownAnimation();//SysAp's internal preparation for ShutDown with animation
    TRACES( RDebug::Print(_L("CSysApAppUi::ShutdownAnimationSyncOK(): Called PrepareForShutdownAnimation()" ) ) );
    }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

// ----------------------------------------------------------------------------
// CSysApAppUi::SkipShutdownAnimation()
// ----------------------------------------------------------------------------

void CSysApAppUi::SkipShutdownAnimation()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::SkipShutdownAnimation() " ) ) );

#ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION
    if ( iSysApShutdownAnimation )
        {
        iSysApShutdownAnimation->Cancel();
        }
#else // RD_STARTUP_ANIMATION_CUSTOMIZATION
    if ( iAnimationShowingTime )
        {
        iSysApShutdownAnimation->EndAnimation();
        }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::NotifyShutdownAnimationSkip()
// ----------------------------------------------------------------------------

void CSysApAppUi::NotifyShutdownAnimationSkip()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::NotifyShutdownAnimationSkip() " ) ) );

    if ( iSysApMediatorObserver )
        {
        iSysApMediatorObserver->ShutdownAnimationSkipped();
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CompleteShutdown()
// ----------------------------------------------------------------------------

void CSysApAppUi::CompleteShutdown( const TBool aReset, const TInt aResetReason )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::CompleteShutdown(): START" ) ) );

    PrepareForShutdownImage();//SysAp's internal preparation for ShutDown with image

    FreeResources();

    if ( aReset )
        {
        __ASSERT_DEBUG( aResetReason >= RStarterSession::ELanguageSwitchReset &&
                        aResetReason <= RStarterSession::EDataRestoreReset,
                        User::Invariant() );
        StarterSession().Reset( static_cast<RStarterSession::TResetReason>( aResetReason ) );
        }
    else
        {
        StarterSession().Shutdown();
        }

    StarterSession().Close();

    TRACES( RDebug::Print(_L("CSysApAppUi::CompleteShutdown(): END" ) ) );
    }

#ifndef SYSAP_USE_STARTUP_UI_PHASE
// ----------------------------------------------------------------------------
// CSysApAppUi::DoStateChangedL(const RStarterSession::TGlobalState aSwState)
// This method is not called after boot has finished.
// ----------------------------------------------------------------------------

void CSysApAppUi::DoStateChangedL(const RStarterSession::TGlobalState aSwState)
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL: %d" ), aSwState ) );

    switch ( aSwState )
        {
        case RStarterSession::ENormal:
            SetStatusPaneLayoutL( ESysApNormal );
            break;
        case RStarterSession::ECharging:
            SetStatusPaneLayoutL( ESysApCharging );
            break;
        case RStarterSession::EAlarm:
            SetStatusPaneLayoutL( ESysApAlarm );
            break;
        default:
            break;
        };

    TRAPD( simChangedErr, DoSimChangedFromPreviousBootL() );
    TRACES( RDebug::Print( _L("CSysApAppUi::DoStateChangedL: simChangedErr = %d" ), simChangedErr ) );
    simChangedErr = simChangedErr; // suppress 'variable not used' warning
    LogsObserverL().HandleSimChangedCheckDoneL();

    if ( iSysApFeatureManager->PowerSaveSupported() )
        {
        // create controller before checking battery state, so that power saving can be enabled during boot if needed
        if ( !iSysApPsmController ) // created only in first state change
            {
            iSysApPsmController = CSysApPsmController::NewL( *this );        
            }

        // in charger boot explicitly disable partial power save mode
        if ( aSwState == RStarterSession::ECharging && !iCharging )
            {
            iSysApPsmController->ChargerConnected();
            iSysApPsmController->DoEnablePartialPsm( EFalse ); // disable partial power save now
            }
        }

    TInt state( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );
    //Also Charging status will be updated with the following function.
    UpdateBatteryBarsL( state );

    if( IsStateNormal() )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL to normal state.") ) );

        DoSwStateNormalConstructionL();

        InitializeStatusPaneAreaL();
        CheckSilentModeL();
        HandleAccessoryProfileInStartupL();

        if ( iSysApFeatureManager->MmcSupported() )
            {
#ifndef RD_MULTIPLE_DRIVE
            MountMMC();
            MMCStatusChangedL();
            iHideFirstBeep = EFalse;
#else // RD_MULTIPLE_DRIVE
            iSysApDriveList->MountDrive( iSysApDriveList->DefaultMemoryCard() );
            UpdateInsertedMemoryCardsL();
#endif // RD_MULTIPLE_DRIVE
            }
        if ( iSysApFeatureManager->MmcHotSwapSupported() )
            {
            iSysApMMCObserver->StartMountObserver();
            }

        
        if ( iSysApPsmController )
            {
            if ( iCharging ) // if charger is connected on boot PSM queries may need to be shown
                {
                HandleChargingStatusL( StateOfProperty( KPSUidHWRMPowerState, KHWRMChargingStatus ) );
                }
            }
        
        TInt batteryStatus = StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryStatus );
        TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL: batteryStatus %d" ), batteryStatus ) );
        if(  batteryStatus == EBatteryStatusLow || batteryStatus == EBatteryStatusEmpty )
            {
            // low and empty battery states are informed to the user in device startup
            HandleBatteryStatusL( batteryStatus );
            }
        else if ( iSysApPsmController && !iCharging )
        	{
				TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL: batteryStatus %d, iCharging %d -> disable partial psm" ), batteryStatus, iCharging ) );

				iSysApPsmController->BatteryLow( EFalse );
				iSysApPsmController->DoEnablePartialPsm( EFalse );
			}

        iSysApBtController = CreateSysApBtControllerL( *this );
        iSysApBtSapController = CreateSysApBtSapControllerL( *this );

        if ( iActivateBt )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL Activating BT" ) ) );
            SetBtPowerState( ETrue );
            }

        if ( iDeactivateBt )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL Deactivating BT" ) ) );
            SetBtPowerState( EFalse );
            }
        iSysApLocationPrivacyIndicator = CreateSysApLocationPrivacyIndicatorL( *this );
        iSysApLocationPrivacyIndicator->InitL();

        if ( ! iSysApUsbIndicatorController )
            {
            TRAPD ( usbErr, iSysApUsbIndicatorController = CreateSysApUsbIndicatorL( *this ) );
            if ( usbErr )
                {
                TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL: error in constructing USB ind. controller %d" ), usbErr ) );
                }
            }

        // Other late initializations.
        // In order to prevent unexpected behaviour e.g. in OOM situations, these calls are made to ensure
        // that the services are connected when they are needed for the first time.
        if ( !StarterSession().Handle() )
            {
            User::Leave( KErrBadHandle );
            }

        if ( !KeyLock().Handle() )
            {
            User::Leave( KErrBadHandle );
            }

#ifdef __SYSAP_MODULE_TEST
        ModuleTestShowUiNoteL( _L("SysAp: SW state normal!") );
#endif
        }

    // Allow lights
    iSysApLightsController->AllowLightsOn();
    }

#else // SYSAP_USE_STARTUP_UI_PHASE

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleUiReadyAfterBootL()
// Called when startup UI activities has been finished
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleUiReadyAfterBootL()
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::HandleUiReadyAfterBootL" ) ) );
    
    SetStatusPaneLayoutL( ESysApNormal );
    
    TInt state( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );
    //Also Charging status will be updated with the following function.
    UpdateBatteryBarsL( state );
    
    DoSwStateNormalConstructionL();

    InitializeStatusPaneAreaL();
    CheckSilentModeL();
    HandleAccessoryProfileInStartupL();
    DeactivatePSMifBatteryNotLowL ();
    
    if ( iSysApFeatureManager->MmcSupported() )
        {
#ifndef RD_MULTIPLE_DRIVE
        MountMMC();
        MMCStatusChangedL();
        iHideFirstBeep = EFalse;
#else // RD_MULTIPLE_DRIVE
        iSysApDriveList->MountDrive( iSysApDriveList->DefaultMemoryCard() );
        UpdateInsertedMemoryCardsL();
#endif // RD_MULTIPLE_DRIVE
        }
    
    if ( iSysApFeatureManager->MmcHotSwapSupported() )
        {
        iSysApMMCObserver->StartMountObserver();
        }

    if ( iSysApPsmController )
        {
        if ( iCharging ) // if charger is connected on boot PSM queries may need to be shown
            {
            HandleChargingStatusL( StateOfProperty( KPSUidHWRMPowerState, KHWRMChargingStatus ) );
            }
        }
    
    TInt batteryStatus = StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryStatus );
    TRACES( RDebug::Print(_L("CSysApAppUi::HandleUiReadyAfterBootL: batteryStatus %d" ), batteryStatus ) );
    if(  batteryStatus == EBatteryStatusLow || batteryStatus == EBatteryStatusEmpty )
        {
        // low and empty battery states are informed to the user in device startup
        HandleBatteryStatusL( batteryStatus );
        }
    else if ( iSysApPsmController && !iCharging )
      	{
		    TRACES( RDebug::Print(_L("CSysApAppUi::HandleUiReadyAfterBootL: batteryStatus %d, iCharging %d -> disable partial psm" ), batteryStatus, iCharging ) );

			iSysApPsmController->BatteryLow( EFalse );
			iSysApPsmController->DoEnablePartialPsm( EFalse );
        }

    iSysApLocationPrivacyIndicator = CreateSysApLocationPrivacyIndicatorL( *this );
    iSysApLocationPrivacyIndicator->InitL();

    if ( ! iSysApUsbIndicatorController )
        {
        TRAPD ( usbErr, iSysApUsbIndicatorController = CreateSysApUsbIndicatorL( *this ) );
        if ( usbErr )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::HandleUiReadyAfterBootL: error in constructing USB ind. controller %d" ), usbErr ) );
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoStateChangedL(const RStarterSession::TGlobalState aSwState)
// This method is not called after boot has finished.
// ----------------------------------------------------------------------------

void CSysApAppUi::DoStateChangedL(const RStarterSession::TGlobalState aSwState)
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL: %d" ), aSwState ) );

    switch ( aSwState )
        {
        case RStarterSession::ENormal:
            // status pane layout will updated when UI is ready
            break;
        case RStarterSession::ECharging:
            SetStatusPaneLayoutL( ESysApCharging );
            break;
        case RStarterSession::EAlarm:
            SetStatusPaneLayoutL( ESysApAlarm );
            break;
        default:
            break;
        };

    TRAPD( simChangedErr, DoSimChangedFromPreviousBootL() );
    TRACES( RDebug::Print( _L("CSysApAppUi::DoStateChangedL: simChangedErr = %d" ), simChangedErr ) );
    simChangedErr = simChangedErr; // suppress 'variable not used' warning
    LogsObserverL().HandleSimChangedCheckDoneL();

    if ( iSysApFeatureManager->PowerSaveSupported() )
        {
        // create controller before checking battery state, so that power saving can be enabled during boot if needed
        if ( !iSysApPsmController ) // created only in first state change
            {
            iSysApPsmController = CSysApPsmController::NewL( *this );        
            }

        // in charger boot explicitly disable partial power save mode
        if ( aSwState == RStarterSession::ECharging )
            {
            iSysApPsmController->ChargerConnected();
            iSysApPsmController->DoEnablePartialPsm( EFalse ); // disable partial power save now
            }
        }

    if ( aSwState == RStarterSession::ECharging || aSwState == RStarterSession::EAlarm )
        {
        TInt state( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );
        //Also Charging status will be updated with the following function.
        UpdateBatteryBarsL( state );    
        }
        
    if( IsStateNormal() )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL to normal state.") ) );

        iSysApBtController = CreateSysApBtControllerL( *this );
        iSysApBtSapController = CreateSysApBtSapControllerL( *this );

        if ( iActivateBt )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL Activating BT" ) ) );
            SetBtPowerState( ETrue );
            }

        if ( iDeactivateBt )
            {
            TRACES( RDebug::Print(_L("CSysApAppUi::DoStateChangedL Deactivating BT" ) ) );
            SetBtPowerState( EFalse );
            }

        // Other late initializations.
        // In order to prevent unexpected behaviour e.g. in OOM situations, these calls are made to ensure
        // that the services are connected when they are needed for the first time.
        if ( !StarterSession().Handle() )
            {
            User::Leave( KErrBadHandle );
            }

        if ( !KeyLock().Handle() )
            {
            User::Leave( KErrBadHandle );
            }

#ifdef __SYSAP_MODULE_TEST
        ModuleTestShowUiNoteL( _L("SysAp: SW state normal!") );
#endif
        }

    // Allow lights
    iSysApLightsController->AllowLightsOn();
    }


#endif // SYSAP_USE_STARTUP_UI_PHASE

// ----------------------------------------------------------------------------
// CSysApAppUi::UiReady()
// 
// ----------------------------------------------------------------------------

TBool CSysApAppUi::UiReady() const
    {
#ifdef SYSAP_USE_STARTUP_UI_PHASE    
    return iSysApStartupController->UiReady(); 
#else // SYSAP_USE_STARTUP_UI_PHASE
    // if startup UI phase information is not used, global system state normal is handled as UI idle state
    return IsStateNormal();
#endif // SYSAP_USE_STARTUP_UI_PHASE        
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::PrepareForShutdownAnimation()
// ----------------------------------------------------------------------------
void CSysApAppUi::PrepareForShutdownAnimation()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownAnimation() begin") ) );

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownAnimation() showtime = %d"), iAnimationShowingTime ) );
    if ( iAnimationShowingTime )
        {
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION
        static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PlaySound( EAvkonSIDPowerOffTone );
        CancelGlobalListQuery();

        if ( iSysApPowerKeyMenuObserver )
            {
            iSysApPowerKeyMenuObserver->Cancel();
            }

        // deactivate notifiers before showing the shutdown image
        AknNotifierController::CancelAllNotifications();
        AknNotifierController::HideAllNotifications(ETrue);
        RWindowGroup groupWin = iCoeEnv->RootWin();
        iCapturedAppskey = groupWin.CaptureKey( EKeyApplication, KModifierMask, KModifierMask );
        iCapturedAppskeyUpAndDowns = groupWin.CaptureKeyUpAndDowns( EStdKeyApplication0, KModifierMask, KModifierMask );
        iEikonEnv->RootWin().SetOrdinalPosition(0, ECoeWinPriorityAlwaysAtFront );

        TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownAnimation() Draw background image" ) ) );

        ShowShutdownImage( KBackgroundImageID );

#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
        TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownAnimation() Show Shutdown animation" ) ) );
        TInt err(0);
        TRAP( err, iSysApShutdownAnimation->StartL( iLastPowerKeyWasShort ) );
        if ( err )
            {
            TRACES( RDebug::Print( _L("SysAp: Shutdown animation fails. Error code: %d" ), err ) );
            // Start animation timing immediatily if animation starting fails.
            // Otherwise animation will call StartAnimTiming when it is ready.
            StartAnimTiming();
            }
        }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownAnimation() end") ) );
    }


// ----------------------------------------------------------------------------
// CSysApAppUi::PrepareForShutdownImage()
// ----------------------------------------------------------------------------

void CSysApAppUi::PrepareForShutdownImage()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownImage() START" ) ) );

    // If RD_STARTUP_ANIMATION_CUSTOMIZATION is enabled, the following actions are always taken in PrepareForShutdownAnimation
#ifndef RD_STARTUP_ANIMATION_CUSTOMIZATION
    if ( !iAnimationShowingTime ) // actions already taken in PrepareForShutdownAnimation
        {
        static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PlaySound( EAvkonSIDPowerOffTone );
        CancelGlobalListQuery();
        
        if ( iSysApPowerKeyMenuObserver )
            {
            iSysApPowerKeyMenuObserver->Cancel();
            }
        
        // deactivate notifiers before showing the shutdown image
        AknNotifierController::CancelAllNotifications();
        AknNotifierController::HideAllNotifications(ETrue);
        RWindowGroup groupWin = iCoeEnv->RootWin();
        iCapturedAppskey = groupWin.CaptureKey( EKeyApplication, KModifierMask, KModifierMask );
        iCapturedAppskeyUpAndDowns = groupWin.CaptureKeyUpAndDowns( EStdKeyApplication0, KModifierMask, KModifierMask );
        }
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

#ifndef __SYSAP_MODULE_TEST //to make errors during shutdown sequence being seen more easily
    iEikonEnv->RootWin().SetOrdinalPosition(0, ECoeWinPriorityAlwaysAtFront );
    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownImage() Show Shutdown image" ) ) );
    ShowShutdownImage( EMbmSysapQgn_startup_screen );
#endif
    TRACES( RDebug::Print( _L("CSysApAppUi::PrepareForShutdownImage() END" ) ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetStatusPaneLauoutL( const TSysApPhoneState aState )
// ----------------------------------------------------------------------------

void CSysApAppUi::SetStatusPaneLayoutL( const TSysApPhoneState aState )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::SetStatusPaneLayoutL aState: %d"), aState ) );
    if( aState == ESysApNormal )
        //Status pane cannot be even visible on Normal mode
        {
        StatusPane()->MakeVisible( EFalse );
        StatusPane()->SwitchLayoutL( R_AVKON_STATUS_PANE_LAYOUT_EMPTY );
        }
    else if( aState == ESysApCharging )
        //SysAp's statuspane is visible on Charging mode
        {
        StatusPane()->MakeVisible( ETrue );
        StatusPane()->SwitchLayoutL( R_AVKON_STATUS_PANE_LAYOUT_POWER_OFF_RECHARGE );
        }
    else if( aState == ESysApAlarm )
    	{
    	if ( !Layout_Meta_Data::IsLandscapeOrientation() )
    		{
    		// Portrait mode
	        StatusPane()->MakeVisible( ETrue );
	        StatusPane()->SwitchLayoutL( R_AVKON_STATUS_PANE_LAYOUT_POWER_OFF_RECHARGE );
    		}
    	else
    		{
    		// Landscape mode
	        TInt layout = R_AVKON_STATUS_PANE_LAYOUT_POWER_OFF_RECHARGE;
	        // For side softkey devices, use different layout
	        if (AVKONENV->StatusPaneResIdForCurrentLayout(R_AVKON_STATUS_PANE_LAYOUT_USUAL) == R_AVKON_STACON_PANE_LAYOUT_USUAL_SOFTKEYS_RIGHT)
	        	layout = R_AVKON_STACON_PANE_LAYOUT_EMPTY_SOFTKEYS_RIGHT;
	        else if (AVKONENV->StatusPaneResIdForCurrentLayout(R_AVKON_STATUS_PANE_LAYOUT_USUAL) == R_AVKON_STACON_PANE_LAYOUT_USUAL_SOFTKEYS_LEFT)
	        	layout = R_AVKON_STACON_PANE_LAYOUT_EMPTY_SOFTKEYS_LEFT;

	        StatusPane()->MakeVisible( ETrue );
	        StatusPane()->SwitchLayoutL( layout );
    		}
    	}
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::OkToInitiateShutdown()
// ----------------------------------------------------------------------------

TBool CSysApAppUi::OkToInitiateShutdown()
    {
    TInt swState;
    RProperty::Get( KPSUidStartup, KPSGlobalSystemState, swState );

    if( !iShutdownStarted &&
        swState == ESwStateStartingCriticalApps ||
        swState == ESwStateSelfTestOK ||
        swState == ESwStateSecurityCheck ||
        swState == ESwStateCriticalPhaseOK ||
        swState == ESwStateEmergencyCallsOnly ||
        swState == ESwStateCharging ||
        swState == ESwStateAlarm ||
        swState == ESwStateNormalRfOn ||
        swState == ESwStateNormalRfOff ||
        swState == ESwStateNormalBTSap ||
        swState == ESwStateFatalStartupError )
        {
        iShutdownStarted = ETrue;
        return ETrue;
        }
    else
        {
        return EFalse;
        }
    }

/**
 * To check the for an emergency call. 
 * 
 * @return ETrue if there is an emergency call active otherwise, EFalse.
 */
TBool IsEmergencyCall()
        {
        TBool retVal( EFalse );
        TInt err( KErrNone );
        TInt state( 0 );
     
        err = RProperty::Get(KPSUidCtsyEmergencyCallInfo, KCTSYEmergencyCallInfo, state );
        if ( err == KErrNone && state )
            {
            retVal = ETrue;            
            }
        return retVal;
        }	

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleCurrentCallStateChangeL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleCurrentCallStateChangeL( TInt aCurrentCallState )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleCurrentCallStateChangeL: KUidCurrentCall: %d" ), aCurrentCallState ) );

    if( iPowerKeyPopupMenuActive )
        {
        CancelGlobalListQuery();
        }

    switch ( aCurrentCallState )
        {
        case EPSCTsyCallStateRinging:  
            {
            iSysApLightsController->CallComingInL( ETrue );
            // Disable keylock when a call is coming in
            if ( iKeyLockEnabled || iDeviceLockEnabled || iKeyLockOnBeforeCradle || iKeyLockOnBeforeAlarm )
                {
                TRACES( RDebug::Print( _L("CSysApAppUi::HandleCurrentCallStateChangeL: EPSCTsyCallStateRinging: disable keylock") ) );
                iKeyLockOnBeforeCall = ETrue;

                if ( iKeyLockEnabled || iDeviceLockEnabled )
                    {
                    if ( !iSysApFeatureManager->TouchUnlockStrokeSupported() )
                        {
                        KeyLock().DisableWithoutNote();
                        }
                    }
                }
            break;
            }

        case EPSCTsyCallStateDialling:
            {
            // Disable keypad lock during an emergency call
            // no need to disable the key lock when a call is made using the wireless car-kit
            // but if the call is an emergency one then we will disable the keypad lock
            if ( iKeyLockEnabled || iDeviceLockEnabled || iKeyLockOnBeforeCradle )
                {
                TRACES( RDebug::Print( _L("CSysApAppUi::HandleCurrentCallStateChangeL: EPSCTsyCallStateDialling: disable keylock") ) );
                iKeyLockOnBeforeCall = ETrue;

                if ( IsEmergencyCall() && (iKeyLockEnabled || iDeviceLockEnabled ))
                    {
                    KeyLock().DisableWithoutNote();
                    }
                }

            // Enable signal & network indicators when an emergency call is made in Offline Mode
            if( iSysApOfflineModeController->OfflineModeActive() )
                    {
                    // Signal indicators not updated with VoIP call
                    if ( StateOfProperty(KPSUidCtsyCallInformation, KCTsyCallType) != EPSCTsyCallTypeVoIP) 
                        {
                        iEmergencyCallActive = ETrue;
                        UpdateSignalBarsL();
                        SetSignalIndicatorL();
                        }
                    }
            break;
            }
            
        case EPSCTsyCallStateConnected:
            {
            if (StateOfProperty(KPSUidCtsyCallInformation, KCTsyCallType) == EPSCTsyCallTypeCSVoice)
                {
                // Check if GPRS suspended note is required
                iCallActivated = ETrue;
                HandleGprsNotesL();
                }
            if ( iDeviceLockEnabled )
                {
                // Enable keylock via autolock emulation if device lock enabled. 
                // Otherwise e.g. messages can be read using softkeys during ongoing call.
                KeyLock().EnableAutoLockEmulation();
                }
            break;
            }

        case EPSCTsyCallStateNone:
            {
            // reset timers in ScreenSaver and Autolock
            User::ResetInactivityTime();

            if ( iEmergencyCallActive )
                {
                iEmergencyCallActive = EFalse;
                UpdateSignalBarsL( 0 );
                SetSignalIndicatorL();
                }
            if ( iKeyLockOnBeforeCall )
                {
                TRACES( RDebug::Print( _L("CSysApAppUi::HandleCurrentCallStateChangeL: EPSCTsyCallStateNone: enable keylock") ) );
                iKeyLockOnBeforeCall = EFalse;
                if ( !iDeviceLockEnabled )
                    {
                    if ( !iKeyLockOnBeforeCradle && !iKeyLockOnBeforeAlarm &&
                          !iSysApFeatureManager->TouchUnlockStrokeSupported() )
                        {
                        if ( iSysApCenRepController->GetInt( KCRUidCommonTelephonySettings, KSettingsSummaryAfterCall ) == 1 )
                            {
                            KeyLock().EnableWithoutNote(); // Let's not hide "Summary After Call" dialog
                            }
                        else
                            {
                            KeyLock().EnableKeyLock();
                            }
                        }
                    }
                else
                    {
                    KeyLock().EnableAutoLockEmulation();
                    }
                }
            if ( iShowkeypadActivatedNoteAfterSoftReject )
                {
                ShowUiNoteL( EKeypadActiveNote );
                iShowkeypadActivatedNoteAfterSoftReject = EFalse;
                }
            iCallActivated = EFalse;
            break;
            }
            
        default:
            break;
        }

    if ( aCurrentCallState != EPSCTsyCallStateRinging )
        {
        iSysApLightsController->CallComingInL( EFalse );
        }

    SetIhfIndicatorL();
    SetHacIndicatorL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoSwStateNormalConstructionL()
// ----------------------------------------------------------------------------

void CSysApAppUi::DoSwStateNormalConstructionL()
    {
    // Let's ensure that the lights will remain on for 15 secs after sw state normal
    User::ResetInactivityTime();

    TRACES( RDebug::Print( _L("CSysApAppUi::DoSwStateNormalConstructionL : START" ) ) );

    // In case of unexpected reset (e.g. hidden boot) the keylock must be enabled silently. Locking is done
    // prior to time-consuming initializations, because otherwise UI is in unlocked state for a few seconds.
    if ( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsSysAp, KSysApKeyguardActive ) == 1  )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::DoSwStateNormalConstructionL: enabling keylock") ) );
        KeyLock().EnableWithoutNote();
        }

    // Initiate bearer change observation
    iSysApConnectionMonitorObserver = CSysApConnectionMonitorObserver::NewL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::DoSwStateNormalConstructionL: trying CSysApSsSettingsObserver::NewL()") ) );
    iSysApSsSettingsObserver = CSysApSsSettingsObserver::NewL( *this );

    TRACES( RDebug::Print( _L("CSysApAppUi::DoSwStateNormalConstructionL: trying new ( ELeave ) CSysApPowerKeyMenuObserver") ) );
    iSysApPowerKeyMenuObserver = new( ELeave ) CSysApPowerKeyMenuObserver( *this );

    TRACES( RDebug::Print( _L("SysAp: trying CSystemLock::NewL" ) ) );
    iSysApSystemLock = CSystemLock::NewL();

    iSysApCenRepCallForwardingObserver = CSysApCenRepCallForwardingObserver::NewL( *this );

    iSysApCenRepMsgWaitingObserver = CSysApCenRepMsgWaitingObserver::NewL( *this );

    
    if ( iSysApFeatureManager->Supported( KSysApFeatureIdBatteryInfoPopup ) )
        {
		if( iSysApBatteryInfoController == NULL)
            {
			iSysApBatteryInfoController = CSysApBatteryInfoController::NewL( iSysApCenRepController->GetInt( KCRUidCoreApplicationUIsConf, 
                                                                                                     KCoreAppUIsBatteryInformationLowThreshold ) );    
	    	}
        }
    

    TRACES( RDebug::Print( _L("CSysApAppUi::DoSwStateNormalConstructionL : END" ) ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DoSimChangedFromPreviousBootL()
// ----------------------------------------------------------------------------

void CSysApAppUi::DoSimChangedFromPreviousBootL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::DoSimChangedFromPreviousBootL: iSimChangedDone=%d" ), iSimChangedDone ) );

    if ( !iSimChangedDone  )
        {
        iSimChangedDone = ETrue;
        CSysApSimChanged* simChanged = CSysApSimChanged::NewL( *this, iEikonEnv->FsSession() );
        CleanupStack::PushL( simChanged );
        simChanged->HandleSimChangedL();
        CleanupStack::PopAndDestroy( simChanged );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CancelWaitNote()
// ----------------------------------------------------------------------------

void CSysApAppUi::CancelWaitNote()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CancelWaitNote" ) ) );
    if ( iSysApWaitNote )
        {
        iSysApWaitNote->Cancel();
        delete iSysApWaitNote;
        iSysApWaitNote = NULL;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CancelGlobalListQuery()
// ----------------------------------------------------------------------------

void CSysApAppUi::CancelGlobalListQuery()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CancelGlobalListQuery" ) ) );
    if ( iGlobalListQuery )
        {
        iGlobalListQuery->CancelListQuery();
        delete iGlobalListQuery;
        iGlobalListQuery = NULL;
        }
    }

#ifndef RD_MULTIPLE_DRIVE
// ----------------------------------------------------------------------------
// CSysApAppUi::MountMMC()
// ----------------------------------------------------------------------------

TInt CSysApAppUi::MountMMC()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::MountMMC") ) );
    TInt err ( KErrNotSupported );
    if ( iSysApFeatureManager->MmcSupported() )
        {
#ifdef __WINS__ // Let's sleep a second in WINS
        User::After( 1000000 );
#endif
        err = iEikonEnv->FsSession().MountFileSystem( KFSName, KMMCDrive );
        TRACES( RDebug::Print( _L("CSysApAppUi::MountMMC: RFs::MountFileSystem() returned: %d"), err ) );
        if ( err == KErrInUse )
            {
            User::After( 1000000 );
            err = iEikonEnv->FsSession().MountFileSystem( KFSName, KMMCDrive );
            TRACES( RDebug::Print( _L("CSysApAppUi::MountMMC: RFs::MountFileSystem() returned: %d"), err ) );
            }
        }
    return err;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DismountMMC()
// ----------------------------------------------------------------------------

TInt CSysApAppUi::DismountMMC()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::DismountMMC") ) );
#ifdef __WINS__ // Let's sleep a second in WINS
        User::After( 1000000 );
#endif

    TInt err ( KErrNotSupported );
    if ( iSysApFeatureManager->MmcSupported() )
        {
        err = iEikonEnv->FsSession().DismountFileSystem( KFSName, KMMCDrive );
        TRACES( RDebug::Print( _L("CSysApAppUi::DismountMMC: RFs::DismountFileSystem() returned: %d"), err ) );
        }
    return err;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectMMCL()
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectMMCL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::EjectMMCL") ) );
    iMMCEjectUsed = ETrue;
    iMMCPowerMenuEjectUsed = ETrue;
    iTimeToKill = EFalse;
    iApplicationScanningRoundNumber = 0;
    CloseUIAppsInHotSwapL();
    }

#else // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectMMCL()
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectMMCL()
    {
    // Called from eject confirm query, check drive inserted before start
    TInt insertedIndex( CSysApDriveList::Find(
        iInsertedMemoryCards, iDriveToEject ) );

    TRACES( RDebug::Print(
        _L( "CSysApAppUi::EjectMMCL: iMMCEjectUsed: %d, drive: %d, index: %d "),
        iMMCEjectUsed, iDriveToEject, insertedIndex ) );

    if ( insertedIndex != KErrNotFound )
        {
        iSysApDriveList->MarkDriveToEject(
            iDriveToEject, CSysApDriveList::EEjectFromMenu );
        iSysApDriveEjectHandler->StartEject();
        }
    iMMCEjectUsed = EFalse;
    }

#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::DisconnectBtSap()
// ----------------------------------------------------------------------------

void CSysApAppUi::DisconnectBtSap()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::DisconnectBtSap") ) );
    iSysApBtSapController->Disconnect();
    }

#ifndef RD_MULTIPLE_DRIVE
// ----------------------------------------------------------------------------
// CSysApAppUi::RunUnlockNotifierL()
// ----------------------------------------------------------------------------

void CSysApAppUi::RunUnlockNotifierL( TSysApMemoryCardStatus aMemoryCardStatus )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL: START") ) );
    if ( iSysApFeatureManager->MmcSupported() )
        {
        if ( UiReady() )
            {
            TSysApMemoryCardStatus memoryCardStatus;
            if ( aMemoryCardStatus == ESysApMemoryCardStatusNotKnown )
                {
                memoryCardStatus = iSysApMMCObserver->MemoryCardStatus();
                }
            else
                {
                memoryCardStatus = aMemoryCardStatus;
                }

            TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL: memoryCardStatus=%d"), memoryCardStatus ) );

            switch ( memoryCardStatus )
                {
                case ESysApMemoryCardInserted:
                    TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL MMC inserted") ) );
                    if ( aMemoryCardStatus == ESysApMemoryCardInserted )
                        {
                        RProperty::Set( KPSUidUikon, KUikMMCInserted, 1 );
                        }
                    break;
                case ESysApMemoryCardLocked:
                    {
                    TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL MMC locked") ) );
                    RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
                    TInt callState = StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState );

                    if (  (! ( iDeviceLockEnabled || iKeyLockEnabled ) ) &&
                          callState != EPSCTsyCallStateRinging && // We don't want to see the MMC passwd query
                          callState != EPSCTsyCallStateAlerting ) // when the user is e.g. making an emergency call
                        {
                        if ( iSysApFeatureManager->MemoryCardLockSupported() )
                            {
                            if ( ! iMemoryCardDialog )
                                {
                                TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL: ACTIVATE MMC passwd query") ) );
                                CAknMemoryCardDialog* mmcDialog = CAknMemoryCardDialog::NewLC( this );
                                iMemoryCardDialog = mmcDialog; // temporary variable used for hiding codescanner error 
                                iMemoryCardDialog->SetSelfPointer( &iMemoryCardDialog );
                                iMemoryCardDialog->UnlockCardLD(); // when UnlockCardLD completes it calls UnlockComplete()
                                }
                            }
                        else
                            {
                            // Since locked cards are not supported, notify user that card is locked.
                            ShowQueryL( ESysApMemoryCardLockedNote );
                            }
                        }
                    }
                    break;

                case ESysApMemoryCardNotInserted:
                default:
                    TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL MMC not inserted") ) );
                    if ( aMemoryCardStatus == ESysApMemoryCardNotInserted )
                        {
                        RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
                        }
                }
            }
        }
    TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL: END") ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UnlockComplete() from MAknMemoryCardDialogObserver
// ----------------------------------------------------------------------------

void CSysApAppUi::UnlockComplete( TInt aResult )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::UnlockComplete result: %d"), aResult ) );
    if ( aResult == KErrNone )
        {
        RProperty::Set( KPSUidUikon, KUikMMCInserted, 1 );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::MMCStatusChangedL() from MSysApMemoryCardObserver
// ----------------------------------------------------------------------------

void CSysApAppUi::MMCStatusChangedL()
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL START: iMMCInserted: %d, iMMCEjectUsed: %d, iHideFirstBeep: %d, iHideNextBeep: %d" ), iMMCInserted, iMMCEjectUsed, iHideFirstBeep, iHideNextBeep ) );

    if ( iSysApFeatureManager->MmcSupported() && !iShutdownStarted )
        {
        TSysApMemoryCardStatus memoryCardStatus = iSysApMMCObserver->MemoryCardStatus();
        TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL memoryCardStatus=%d" ), memoryCardStatus ) );

        switch ( memoryCardStatus )
            {
            case ESysApMemoryCardInserted:
            case ESysApMemoryCardLocked:
                TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL: MMC drive mounted" ) ) );
                if ( ! iMMCInserted ) // MMC was not inserted before
                    {
                    TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL: MMC newly inserted" ) ) );
                    CancelWaitNote(); // just to be sure, the user might keep on closing and opening the MMC latch
                    // An MMC has been newly inserted, so play a sound and check if its password protected

                    if ( UiReady() )
                        {
                        if ( !iHideFirstBeep && !iHideNextBeep ) // starting up
                            {
                            Beep();
                            // Switch lights on
                            iSysApLightsController->MemoryCardInsertedL();
                            }
                        iHideNextBeep = EFalse;
                        }

                    iMMCPowerMenuEjectUsed = EFalse;
                    iMMCEjectUsed = EFalse;
                    iMMCInserted = ETrue;

                    // No need to show ESysApRemoveMmcNote after MMC already mounted
                    if ( iSysApConfirmationQuery )
                        {
                        if ( iSysApConfirmationQuery->CurrentQuery() == ESysApRemoveMmcNote )
                            {
                            iSysApConfirmationQuery->Cancel();
                            }
                        }

                    // Check if the MMC is locked and unlock it if necessary
                    RunUnlockNotifierL( memoryCardStatus );
                    }
                break;
            default:
                TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL: MMC drive not mounted" ) ) );
                delete iMemoryCardDialog; // sets itself to null
                RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
                if ( iMMCInserted )
                    {
                    // No need to show ESysApEjectMmcQuery after MMC already removed
                    if ( iSysApConfirmationQuery )
                        {
                        if ( iSysApConfirmationQuery->CurrentQuery() == ESysApEjectMmcQuery )
                            {
                            iSysApConfirmationQuery->Cancel();
                            }
                        }

                    iMMCInserted = EFalse;

                    TInt propertyValue( StateOfProperty( KPSUidUsbWatcher, KUsbWatcherSelectedPersonality ) );
                    if ( !iMMCEjectUsed && propertyValue != KUsbPersonalityIdMS )
                        {
                        // if USB file transfer active, do not close applications
                        // if eject selected in MMC App, MMC App takes care of the following and
                        // if eject selected from powerkeymenu, applications have already been shutdown
                        iTimeToKill = EFalse;
                        iApplicationScanningRoundNumber = 0;
                        CloseUIAppsInHotSwapL();
                        }
                    }
                else
                    {
                    // If MMC was not previously inserted and eject was chosed from power key menu, attempt to remount.
                    if ( iMMCPowerMenuEjectUsed )
                        {
                        TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL: Attempt to remount" ) ) );
                        MountMMC();
                        // If mount was successful, unnecessary note will be canceled in insert notification handling.
                        }
                    }
                break;
            }

        // Update memory card indicator status
        SetMemoryCardIndicatorL();
        }
    TRACES( RDebug::Print( _L( "CSysApAppUi::MMCStatusChangedL   END: iMMCInserted: %d, iMMCEjectUsed: %d, iHideFirstBeep: %d, iHideNextBeep: %d" ), iMMCInserted, iMMCEjectUsed, iHideFirstBeep, iHideNextBeep ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowMMCDismountedDialogL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowMMCDismountedDialogL()
    {
    TInt propertyValue( StateOfProperty( KPSUidUsbWatcher, KUsbWatcherSelectedPersonality ) );

    // Do not show any note if USB file transfer is active.
    if ( propertyValue != KUsbPersonalityIdMS )
        {
        if ( iSysApFeatureManager->MemoryCardHatchSupported() )
            { // MMC hatch has been opened and MMC has been dismounted
            TRACES( RDebug::Print( _L( "CSysApAppUi::ShowMMCDismountedDialogL: Show note: Remove MMC and press OK." ) ) );
            ShowQueryL( ESysApRemoveMmcNote );
            }
        else
            {
            // MMC has been removed and dismounted
            TRACES( RDebug::Print( _L( "CSysApAppUi::ShowMMCDismountedDialogL: Show note: You might have lost some data." ) ) );
            // No confirmation but let's set this true to enable MMC passwd query
            ShowUiNoteL( EMemoryCardRemovedWithoutEjectNote );
            }
        }
    else
        {
        TRACES( RDebug::Print( _L( "CSysApAppUi::ShowMMCDismountedDialogL: No note shown, USB file transfer caused dismount." ) ) );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::MMCDismountedDialogConfirmed
// ----------------------------------------------------------------------------

void CSysApAppUi::MMCDismountedDialogConfirmed()
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::MMCDismountedDialogConfirmed") ) );

    // Try to remount just in case when eject was chosen from power key menu
    if ( iMMCPowerMenuEjectUsed )
        {
        iMMCPowerMenuEjectUsed = EFalse;

        if ( MountMMC() == KErrLocked ) // In case a locked card was not removed after all
            {
            TRAPD( err, MMCStatusChangedL() ); // This will update power menu and memory card icon. Also memory card password is requeried.
            if ( err != KErrNone )
                {
                TRACES( RDebug::Print( _L( "CSysApAppUi::MMCDismountedDialogConfirmed: MMCStatusChangedL failed, err=%d"), err ) );
                }
            }
        }

    }

#else // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::MMCInsertedL
// ----------------------------------------------------------------------------

void CSysApAppUi::MMCInsertedL()
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::MMCInsertedL") ) );

    if ( iSysApFeatureManager->MmcSupported() && !iShutdownStarted )
		{
		TBool normalState( UiReady() );

		if ( normalState )
			{
			TInt defaultMemoryCard( iSysApDriveList->DefaultMemoryCard() );
			TSysApMemoryCardStatus memoryCardStatus( iSysApDriveList->MemoryCardStatus( defaultMemoryCard ) );

			TRACES( RDebug::Print(
				_L( "CSysApAppUi::MMCInsertedL: drive: %d, memoryCardStatus: %d" ), defaultMemoryCard, memoryCardStatus ) );

			switch ( memoryCardStatus )
				{
				case ESysApMemoryCardInserted: // Fall through
				case ESysApMemoryCardLocked:
					{
					// Reset eject and unlock of inserted memory card
					iSysApDriveList->ResetDriveToEject( defaultMemoryCard );
					iSysApDriveList->ResetDriveUnlockQueryShown( defaultMemoryCard );

					CancelWaitNote();

					if ( memoryCardStatus == ESysApMemoryCardInserted )
						{
						TRACES( RDebug::Print(
							_L( "CSysApAppUi::MMCInsertedL: memoryCardStatus ESysApMemoryCardInserted: %d" ), memoryCardStatus ) );
						RProperty::Set( KPSUidUikon, KUikMMCInserted, 1 );
						}
					else
						{
						TRACES( RDebug::Print(
							_L( "CSysApAppUi::MMCInsertedL: memoryCardStatus ESysApMemoryCardLocked: %d" ), memoryCardStatus ) );
						RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
						}
					break;
					}
				case ESysApMemoryCardNotInserted:
					{
					// Always reset eject and unlock of removed memory card
					TBool isEject( iSysApDriveList->IsDriveToEject( defaultMemoryCard ) );

					TRACES( RDebug::Print(
						_L( "CSysApAppUi::MMCInsertedL: isEject: %d, drive: %d" ),
						isEject, defaultMemoryCard ) );

					iSysApDriveList->ResetDriveToEject( defaultMemoryCard );
					iSysApDriveList->ResetDriveUnlockQueryShown( defaultMemoryCard );

					TRACES( RDebug::Print(
						_L( "SysApAppUi::MMCInsertedL: memoryCardStatus ESysApMemoryCardNotInserted: %d" ), memoryCardStatus ) );

					RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
					break;
					}
				case ESysApMemoryCardStatusNotKnown: // Fall through
				default:
					{
					break;
					}
				}
				RunUnlockNotifierL();
			}
		}
	}


// ----------------------------------------------------------------------------
// CSysApAppUi::RunUnlockNotifierL()
// ----------------------------------------------------------------------------

void CSysApAppUi::RunUnlockNotifierL()
    {
    TRACES( RDebug::Print(
        _L("CSysApAppUi::RunUnlockNotifierL: START iDeviceLockEnabled: %d, iKeyLockEnabled: %d" ),
         iDeviceLockEnabled, iKeyLockEnabled ) );

    if ( !iSysApFeatureManager->MmcSupported() ||
         iDeviceLockEnabled ||
         iKeyLockEnabled ||
         !UiReady() )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::RunUnlockNotifierL: END") ) );
        return;
        }

    // We don't want to see the MMC passwd query
    // when the user is e.g. making an emergency call
    TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );
    if ( callState != EPSCTsyCallStateRinging &&
        callState != EPSCTsyCallStateAlerting )
        {
        iSysApDriveUnlockHandler->StartUnlock();
        }

    TRACES( RDebug::Print(
        _L("CSysApAppUi::RunUnlockNotifierL: END callState: %d"), callState ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::MMCStatusChangedL() from MSysApMemoryCardObserver
// ----------------------------------------------------------------------------

void CSysApAppUi::MMCStatusChangedL( TInt aDrive )
    {
    TRACES( RDebug::Print(
        _L( "CSysApAppUi::MMCStatusChangedL START: MMCCount: %d, iMMCEjectUsed: %d" ),
        iInsertedMemoryCards.Count(), iMMCEjectUsed ) );

    if ( iShutdownStarted || !iSysApFeatureManager->MmcSupported() )
        {
        TRACES( RDebug::Print(
            _L( "CSysApAppUi::MMCStatusChangedL iShutdownStarted: %d END" ),
            iShutdownStarted ) );
        return;
        }

    TBool normalState( UiReady() );

    TInt defaultMemoryCard( iSysApDriveList->DefaultMemoryCard() );
    TSysApMemoryCardStatus memoryCardStatus( iSysApDriveList->MemoryCardStatus( aDrive ) );
    TInt insertedIndex( CSysApDriveList::Find( iInsertedMemoryCards, aDrive ) );

    TRACES( RDebug::Print(
        _L( "CSysApAppUi::MMCStatusChangedL: normalState: %d, index: %d, drive: %d, memoryCardStatus: %d" ),
        normalState, insertedIndex, aDrive, memoryCardStatus ) );

    switch ( memoryCardStatus )
        {
        case ESysApMemoryCardInserted: // Fall through
        case ESysApMemoryCardLocked:
            {
            if ( insertedIndex == KErrNotFound ) // Not inserted before
                {
                // Reset eject and unlock of inserted memory card
                iSysApDriveList->ResetDriveToEject( aDrive );
                iSysApDriveList->ResetDriveUnlockQueryShown( aDrive );

                CancelWaitNote();

                if ( aDrive == defaultMemoryCard )
                    {
                    if ( memoryCardStatus == ESysApMemoryCardInserted )
                        {
                       	RProperty::Set( KPSUidUikon, KUikMMCInserted, 1 );
                        }
                    else
                        {
                        RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
                        }
                    }

                if ( normalState )
                    {
                    // Ignore extra beep from USB file transfer
                    TBool ignoreBeep(
                        iSysApDriveList->IsDriveInsertBeepIgnored( aDrive ) );

                    TRACES( RDebug::Print(
                        _L( "CSysApAppUi::MMCStatusChangedL: ignoreBeep: %d, drive: %d" ),
                        ignoreBeep, aDrive ) );

                    if ( !ignoreBeep )
                        {
                        Beep();
                        iSysApLightsController->MemoryCardInsertedL();
                        }

                    iSysApDriveList->ResetDriveInsertBeepIgnored( aDrive );

                    // Keep ignoring extra beep if USB file transfer is active
                    TInt propertyValue( StateOfProperty( KPSUidUsbWatcher, KUsbWatcherSelectedPersonality ) );

                    TRACES( RDebug::Print(
                        _L( "CSysApAppUi::MMCStatusChangedL: usbState: %d" ), propertyValue ) );

                    if ( propertyValue == KUsbPersonalityIdMS )
                        {
                        iSysApDriveList->MarkDriveInsertBeepIgnored( aDrive );
                        }
                    }

                if ( iDriveToDismount == aDrive && iSysApConfirmationQuery )
                    {
                    if ( iSysApConfirmationQuery->CurrentQuery() == ESysApRemoveMmcNote )
                        {
                        // User put back ejected memory card or did not remove it
                        iSysApConfirmationQuery->Cancel();
                        }
                    }
                RunUnlockNotifierL();
                }
            break;
            }
        case ESysApMemoryCardNotInserted:
            {
            // Always reset eject and unlock of removed memory card
            TBool isEject( iSysApDriveList->IsDriveToEject( aDrive ) );

            TRACES( RDebug::Print(
                _L( "CSysApAppUi::MMCStatusChangedL: isEject: %d, drive: %d" ),
                isEject, aDrive ) );

            iSysApDriveList->ResetDriveToEject( aDrive );
            iSysApDriveList->ResetDriveUnlockQueryShown( aDrive );

            if ( insertedIndex != KErrNotFound ) // Inserted before
                {
                if ( memoryCardStatus == ESysApMemoryCardNotInserted )
                    {
                    // Reset extra beep ignore if memory card was removed without eject
                    iSysApDriveList->ResetDriveInsertBeepIgnored( aDrive );
                    }

                CancelGlobalListQuery(); // Cancel power menu

                if ( aDrive == defaultMemoryCard )
                    {
                    RProperty::Set( KPSUidUikon, KUikMMCInserted, 0 );
                    }

                if ( iSysApConfirmationQuery )
                    {
                    if ( iSysApConfirmationQuery->CurrentQuery() == ESysApEjectMmcQuery )
                        {
                        // User removed memory card too early
                        iSysApConfirmationQuery->Cancel();
                        // Allow application closing to execute.
                        // Also FileManager is closed in this case, regardless where the eject was initiated from.
                        iMMCEjectUsed = EFalse;
                        }
                    }

                // Stop unlock of removed memory card
                iSysApDriveUnlockHandler->StopUnlock( aDrive );

                if ( !iMMCEjectUsed && !isEject )
                    {
                    TInt propertyValue( StateOfProperty( KPSUidUsbWatcher, KUsbWatcherSelectedPersonality ) );

                    TRACES( RDebug::Print(
                        _L( "CSysApAppUi::MMCStatusChangedL: usbState: %d" ), propertyValue ) );

                    if ( propertyValue != KUsbPersonalityIdMS )
                        {
                        if ( iSysApFeatureManager->MemoryCardHatchSupported() )
                            {
                            // Store drive removed without eject and start eject handling
                            iSysApDriveList->MarkDriveToEject(
                                aDrive, CSysApDriveList::EEjectRemovedWithoutEject );
                            iSysApDriveEjectHandler->StartEject();
                            }
                        else
                            {
                            // Just show the note
                            ShowUiNoteL( EMemoryCardRemovedWithoutEjectNote );
                            }
                        }
                    }
                iMMCEjectUsed = EFalse;
                }
            break;
            }
        case ESysApMemoryCardStatusNotKnown: // Fall through
        default:
            {
            break;
            }
        }

    // Update inserted memory cards
    iSysApDriveList->GetMemoryCardsL(
        iInsertedMemoryCards, CSysApDriveList::EIncludeInserted );

    // Update memory card indicator status
    SetMemoryCardIndicatorL();

    TRACES( RDebug::Print(
        _L( "CSysApAppUi::MMCStatusChangedL END: MMCCount: %d, iMMCEjectUsed: %d" ),
        iInsertedMemoryCards.Count(), iMMCEjectUsed ) );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowMMCDismountedDialogL()
// ----------------------------------------------------------------------------

TBool CSysApAppUi::ShowMMCDismountedDialogL(
        TInt aDrive, CSysApDriveList::TDriveEjectType aEjectType )
    {
    TRACES( RDebug::Print(
        _L( "CSysApAppUi::ShowMMCDismountedDialogL: aDrive: %d, aEjectType: %d" ),
        aDrive, aEjectType ) );

    TBool ret( EFalse );
    iDriveToDismount = aDrive;
    HBufC* driveName = iSysApDriveList->GetFormattedDriveNameLC(
        aDrive,
        R_QTN_EJECT_REMOVE_MEMORY_INFO );

    if ( aEjectType == CSysApDriveList::EEjectFromMenu )
        {
        TRACES( RDebug::Print(
            _L( "CSysApAppUi::ShowMMCDismountedDialogL: Show note: Remove MMC and press OK...." ) ) );
        ShowQueryL( ESysApRemoveMmcNote, *driveName );
        ret = ETrue;
        }
    else
        {
        // Memory card was removed without eject
        TInt propertyValue( StateOfProperty( KPSUidUsbWatcher, KUsbWatcherSelectedPersonality ) );

        // Do not show any note if USB file transfer is active.
        if ( propertyValue != KUsbPersonalityIdMS )
            {
            if ( iSysApFeatureManager->MemoryCardHatchSupported() )
                {
                // MMC hatch has been opened and MMC has been dismounted
                TRACES( RDebug::Print(
                    _L( "CSysApAppUi::ShowMMCDismountedDialogL: Show note: Remove MMC and press OK." ) ) );
                ShowQueryL( ESysApRemoveMmcNote, *driveName );
                ret = ETrue;
                }
            else
                {
                iSysApDriveList->ResetDrivesToEject();

                // MMC has been removed and dismounted
                TRACES( RDebug::Print( _L(
                    "CSysApAppUi::ShowMMCDismountedDialogL: Show note: You might have lost some data." ) ) );

                // No confirmation but let's set this true to enable MMC passwd query
                ShowUiNoteL( EMemoryCardRemovedWithoutEjectNote );
                }
            }
        else
            {
            iSysApDriveList->ResetDrivesToEject();
            TRACES( RDebug::Print(
                _L( "CSysApAppUi::ShowMMCDismountedDialogL: No note shown, USB file transfer caused dismount." ) ) );
            }
        }

    CleanupStack::PopAndDestroy( driveName );

    TRACES( RDebug::Print(
        _L( "CSysApAppUi::ShowMMCDismountedDialogL: ret: %d" ), ret ) );

    return ret;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::MMCDismountedDialogConfirmed
// ----------------------------------------------------------------------------

void CSysApAppUi::MMCDismountedDialogConfirmed()
    {
    TRACES( RDebug::Print(
        _L( "CSysApAppUi::MMCDismountedDialogConfirmed: iDriveToDismount: %d" ),
        iDriveToDismount ) );

    // Try remount to check if drive was put back
    iSysApDriveList->MountDrive( iDriveToDismount );
    if ( !iSysApDriveEjectHandler->CompleteDismount( iDriveToDismount ) )
        {
        // When ready, check for locked memory cards and update indicators
        TRAP_IGNORE( SetMemoryCardIndicatorL() );
        TRAP_IGNORE( RunUnlockNotifierL() );
        }
    }

#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectStarted()
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectStarted( TBool ejectStarted )
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::EjectStarted" ) ) );
    iMMCEjectUsed = ejectStarted;
    
#ifndef RD_MULTIPLE_DRIVE
    iFileManagerCloseDisabled = ejectStarted;
#endif // RD_MULTIPLE_DRIVE
}

// ----------------------------------------------------------------------------
// CSysApAppUi::DoLightsTimeoutChangedL
// ----------------------------------------------------------------------------
void CSysApAppUi::DoLightsTimeoutChangedL( const TInt aTimeout )
    {
    iSysApLightsController->DoLightsTimeoutChangedL( aTimeout );
    }

#ifdef __SYSAP_MODULE_TEST

// ----------------------------------------------------------------------------
// CSysApAppUi::ModuleTestShowUiNoteL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ModuleTestShowUiNoteL( const TDesC& noteText ) const
    {
    CAknGlobalNote* note = CAknGlobalNote::NewLC();
    note->SetTone( EAvkonSIDNoSound );
    note->ShowNoteL( EAknGlobalInformationNote, noteText );
    CleanupStack::PopAndDestroy(); // note
    }

#endif

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowShutdownNoteL()
// ----------------------------------------------------------------------------
void CSysApAppUi::ShowShutdownNoteL()
    {
    TRACES( RDebug::Print( _L( "CSysApAppUi::ShowShutdownNoteL") ) );
    ShowUiNoteL( EShutdownNote );
    User::After( KTimeToShowShutdownNote );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::Beep()
// ----------------------------------------------------------------------------
void CSysApAppUi::Beep()
    {
    static_cast<CAknAppUi*>(iEikonEnv->EikAppUi())->KeySounds()->PlaySound( EAvkonSIDInformationTone );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SimSupported()
// ----------------------------------------------------------------------------
TBool CSysApAppUi::SimSupported()
    {
    return iSysApFeatureManager->SimSupported();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::IsStateNormal()
//
// ----------------------------------------------------------------------------
TBool CSysApAppUi::IsStateNormal() const
    {
    TInt state;
    TInt errorCode =
        RProperty::Get( KPSUidStartup, KPSGlobalSystemState, state );
    if ( errorCode == KErrNone )
        {
        return state == ESwStateNormalRfOn ||
               state == ESwStateNormalRfOff ||
               state == ESwStateNormalBTSap;
        }
    else
        {
        TRACES( RDebug::Print( _L( "CSysApAppUi::IsStateNormal: Failed to read global system state, error code %d." ), errorCode ) );
        return EFalse;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::BatteryEmptyL()
// ----------------------------------------------------------------------------

void CSysApAppUi::BatteryEmptyL()
    {
    iSysApLightsController->BatteryEmptyL( ETrue );
    // Do not reactivate keyguard in the next startup
    iSysApCenRepController->SetInt( KCRUidCoreApplicationUIsSysAp, KSysApKeyguardActive, 0 );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::Max()
// ----------------------------------------------------------------------------

TInt CSysApAppUi::Max( const TInt aA, const TInt aB, const TInt aC )
     {
     TInt max( aA );
     if ( aB > max )
         {
         max = aB;
         }
     if ( aC > max )
         {
         max = aC;
         }
     return max;
     }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetStarterState()
// ----------------------------------------------------------------------------

TInt CSysApAppUi::SetStarterState( const RStarterSession::TGlobalState aState )
    {
    if ( iSysApStartupController->GlobalStateChangeAllowed( aState ) )
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::SetStarterState: aState=%d" ), aState ) );
        return StarterSession().SetState( aState );
        }
    else
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::SetStarterState: aState=%d - not allowed" ), aState ) );
        return KErrNone;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::GetBtSapDeviceNameL()
// ----------------------------------------------------------------------------

HBufC* CSysApAppUi::GetBtSapDeviceNameL()
    {
    return iSysApBtSapController->GetBtSapDeviceNameL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowAlarmIndicatorL()
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowAlarmIndicatorL( TBool aShowIndicator )
    {
    if ( aShowIndicator)
        {
        SetIndicatorStateL( EAknIndicatorAlarmClock, EAknIndicatorStateOn );
        }
    else
        {
        SetIndicatorStateL( EAknIndicatorAlarmClock, EAknIndicatorStateOff );
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SimulateFlipKeyEventL()
// ----------------------------------------------------------------------------

void CSysApAppUi::SimulateFlipKeyEventL( TBool aFlipOpen )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::SimulateFlipKeyEventL: aFlipOpen: %d" ), aFlipOpen ) );

    /*
       Flip key is constructed when a flip notification via Pub&Sub is received. The reason behind
       providing this as a key event is to enable unified handling of keys also with flip, and because
       flip is captured&consumed by Avkon it is not possible to capture flip key events directly
       from default handler/plugins.
    */

    TKeyEvent flipEvent;
    flipEvent.iCode = aFlipOpen ? EKeyFlipOpen : EKeyFlipClose;
    flipEvent.iScanCode = 0;
    flipEvent.iModifiers = 0;
	flipEvent.iRepeats = 0;

    HandleKeyEventL( flipEvent, EEventKey );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetFlipOpenL()
// ----------------------------------------------------------------------------

void CSysApAppUi::SetFlipOpenL( TBool aFlipOpen )
    {
    iSysApLightsController->SetFlipOpenL( aFlipOpen );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetGripOpenL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SetGripOpenL( TBool aGripOpen )
    {
    iSysApLightsController->SetGripOpenL( aGripOpen );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetNetworkConnectionAllowed()
// ----------------------------------------------------------------------------

void CSysApAppUi::SetNetworkConnectionAllowed( TCoreAppUIsNetworkConnectionAllowed aNetworkConnectionAllowed )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::SetNetworkConnectionAllowed: status: %d" ), aNetworkConnectionAllowed ) );

    iSysApCenRepController->SetInt( KCRUidCoreApplicationUIs, KCoreAppUIsNetworkConnectionAllowed, (TInt) aNetworkConnectionAllowed );

    // Also set the Symbian PS key used for the same purpose:
    RProperty::Set(KUidSystemCategory, KUidPhonePwr.iUid, aNetworkConnectionAllowed ? ESAPhoneOn : ESAPhoneOff);
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::GetTelServerL()
// ----------------------------------------------------------------------------

RTelServer* CSysApAppUi::GetTelServer()
    {
    if ( iSysApEtelConnector )
        {
        return iSysApEtelConnector->GetTelServer();
        }
    else
        {
        return NULL;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CoverDisplaySupported()
// ----------------------------------------------------------------------------

TBool CSysApAppUi::CoverDisplaySupported()
    {
    return iSysApFeatureManager->CoverDisplaySupported();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::StarterSession()
// ----------------------------------------------------------------------------

RStarterSession& CSysApAppUi::StarterSession()
    {
    if ( !iStarterSession.Handle() )
        {
        // All server connections are tried to be made KTriesToConnectServer times because occasional
        // fails on connections are possible at least on some servers
        TRACES( RDebug::Print( _L("CSysApAppUi::StarterSession: trying RStarterSession::Connect()") ) );
        TInt thisTry = 0;
        TInt err;
        while ( ( err = iStarterSession.Connect() ) != KErrNone && ( thisTry++ ) <= KTriesToConnectServer )
            {
            User::After( KTimeBeforeRetryingServerConnection );
            }

        if ( err != KErrNone )
            {
            // What do in error case?
            TRACES( RDebug::Print( _L("CSysApAppUi::StarterSession: RStarterSession::Connect() failed with %d"), err ) );
            }
        }

    return iStarterSession;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::KeyLock()
// ----------------------------------------------------------------------------

RAknKeylock2& CSysApAppUi::KeyLock()
    {
    if ( !iKeyLock.Handle() )
        {
        // All server connections are tried to be made KTriesToConnectServer times because occasional
        // fails on connections are possible at least on some servers
        TInt thisTry = 0;
        TInt err;

        TRACES( RDebug::Print( _L("CSysApAppUi::KeyLock: trying RAknKeylock2::Connect()") ) );
        thisTry = 0;
        while ( ( err = iKeyLock.Connect() ) != KErrNone && ( thisTry++ ) <= KTriesToConnectServer )
            {
            User::After( KTimeBeforeRetryingServerConnection );
            }

        if ( err != KErrNone )
            {
            // What do in error case?
            TRACES( RDebug::Print( _L("CSysApAppUi::KeyLock: RAknKeylock2::Connect() failed with %d"), err ) );
            }
        }

    return iKeyLock;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::CenRepController()
// ----------------------------------------------------------------------------

CSysApCenRepController& CSysApAppUi::CenRepController()
    {
    __ASSERT_ALWAYS( iSysApCenRepController, User::Panic( _L("CSysApAppUi::CenRepController"), KErrBadHandle ) );

    return *iSysApCenRepController;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ExecCommandL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::ExecCommandL( TSysapCommand aCommand )
    {
    ExecCommandL( aCommand, KNullDesC8 );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ExecCommandL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::ExecCommandL( TSysapCommand aCommand, const TDesC8& aParam )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ExecCommandL: aCommand=%d"), aCommand ) );

    switch ( aCommand )
        {
        case EResetKeyguardState: // reset the internal keyguard flags of the SysAp, except for iKeyLockOnBeforeCradle
            iKeyLockOnBeforeCall = EFalse;
            iKeyLockOnBeforeAlarm = EFalse;
            break;

        case ECancelPowermenu: // hide power menu if it is visible
            if( iPowerKeyPopupMenuActive )
                {
                CancelGlobalListQuery();
                }
            break;

        case EUpdateLights: // apply key specific light handling
            {
            TUpdateLightsBuf lightBuf;
            lightBuf.Copy( aParam );

            switch ( lightBuf() )
                {
                case EKeyGripClose:
                    SetGripOpenL( EFalse );
                    break;

                case EKeyGripOpen:
                    SetGripOpenL( ETrue );
                    break;

                case EKeyFlipClose:
                    SetFlipOpenL( EFalse );
                    break;

                case EKeyFlipOpen:
                    SetFlipOpenL( ETrue );
                    break;

                default:
                    // No specific light handling for other keys.
                    break;
                }
            }
            break;

        default:
            // do nothing in release builds since no harm is done
            __ASSERT_DEBUG( EFalse, User::Panic( _L("CSysApAppUi::ExecCommandL: Invalid command"), KErrArgument ) );
            break;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ExecQueryL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::ExecQueryL( TSysapQuery aQuery, TDes8& aReturn )
    {
    ExecQueryL( aQuery, aReturn, KNullDesC8 );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ExecQueryL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::ExecQueryL( TSysapQuery aQuery, TDes8& aReturn, const TDesC8& /*aParam*/ )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ExecQueryL: aQuery=%d"), aQuery ) );

    switch ( aQuery )
        {
        case EGetHwrmLight: // get handle to HWRM client session. Returns CHWRMLight*.
            {
            THwrmLightBuf retBuf( iSysApLightsController->GetHwrmLight() );
            aReturn.Copy( retBuf );
            }
            break;

        case EGetKeylock: // get handle to keylock client session. Returns RAknKeylock2*.
            {
            TKeyLockBuf retBuf( &(KeyLock()) );
            aReturn.Copy( retBuf );
            }
            break;

        default:
            __ASSERT_DEBUG( EFalse, User::Panic( _L("CSysApAppUi::ExecQueryL: Invalid query"), KErrArgument ) );
            User::Leave( KErrArgument );
            break;
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowShutdownImage
// ----------------------------------------------------------------------------
//
TInt CSysApAppUi::ShowShutdownImage( TInt aBitmapId )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ShowShutdownImage") ) );

    TRAPD( err, ShowShutdownImageL( aBitmapId ) );

    if ( err != KErrNone )
        {
        TRACES( RDebug::Print(_L("CSysApAppUi::ShowShutdownImageL failed, err=%d"), err ) );
        }

    return err;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowShutdownImageL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::ShowShutdownImageL( TInt aBitmapId )
    {
    TRACES( RDebug::Print(_L("CSysApAppUi::ShowShutdownImageL") ) );

    if ( !iSysApShutdownImage ) // Shutdown image may be shown without entering normal state (e.g. charging), so construct here
        {
        TRACES( RDebug::Print( _L("CSysApAppUi::ShowShutdownImageL: trying CSysApShutdownImage::NewL()") ) );
        iSysApShutdownImage = CSysApShutdownImage::NewL();
        }

    TRACES( RDebug::Print( _L("CSysApAppUi::ShowShutdownImageL: trying CSysApShutdownImage::ShowShutdownImageL()") ) );
    iSysApShutdownImage->ShowShutdownImageL( aBitmapId );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::IgnoreZeroNetworkBarNotifications
// ----------------------------------------------------------------------------
//
void CSysApAppUi::IgnoreZeroNetworkBarNotifications(TBool aIgnore)
    {
    iIgnoreZeroNetworkBarNotifications = aIgnore;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::BtSapEnabled()
// ----------------------------------------------------------------------------

TBool CSysApAppUi::BtSapEnabled()
    {
    if ( iSysApBtSapController )
        {
        return iSysApBtSapController->BtSapEnabled();    
        }
    
    return EFalse;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::DeactivateBT
// ----------------------------------------------------------------------------
//
void CSysApAppUi::DeactivateBT()
    {
    // Will deactivate BT when changing to normal mode
    iDeactivateBt = ETrue;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetHacIndicatorL()
// ----------------------------------------------------------------------------
void CSysApAppUi::SetHacIndicatorL()
    {

    if ( !iSysApCenRepHacSettingObserver )
        {
        // Do nothing if HAC setting observer has not been created yet
        // This occurs in case some party is trying to set HAC indicator during
        // ConstructL(), e.g. when switching from offline to online during startup
        TRACES( RDebug::Print( _L("CSysApAppUi::SetHacIndicatorL: do nothing, HAC observer does not exist yet") ) );
        return;
        }

    if ( iSysApCenRepHacSettingObserver->HacMode() )
        {
        TBool accessoryConnected ( EFalse );

        TAccMode accMode(EAccModeHandPortable);

        if ( iSysApAccessoryObserver )
            {
            accMode = iSysApAccessoryObserver->GetAccessoryMode();

            if ( accMode != EAccModeHandPortable )
                {
                accessoryConnected = ETrue;
                }
            }

        TInt indicatorState = EAknIndicatorStateOn;

        if ( !accessoryConnected ) // When accessory is not connected, check offline and call state
            {
            if ( OfflineModeActive() ) // HAC indicator is not shown in offline mode
                {
                indicatorState = EAknIndicatorStateOff;
                }
            else
                {
                // during active call with IHF activated HAC indicator is not shown
                TInt callState( StateOfProperty( KPSUidCtsyCallInformation, KCTsyCallState ) );
                if ( callState < KErrNone )
                    {
                    callState = EPSCTsyCallStateNone;
                    }

                if ( callState != EPSCTsyCallStateNone && IhfEnabledL() )
                    {
                    indicatorState = EAknIndicatorStateOff;
                    }
                }

            }
        else
            {
            // If an accessory is connected, HAC indicator should not be shown
            indicatorState = EAknIndicatorStateOff;
            }

        SetIndicatorStateL( EAknIndicatorHAC, indicatorState );

        }
    else
        {
        // If HAC is not active, always disable indicator regardless of accessory state
        SetIndicatorStateL( EAknIndicatorHAC, EAknIndicatorStateOff );
        }


    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleResourceChangeL()
// ----------------------------------------------------------------------------

void CSysApAppUi::HandleResourceChangeL( TInt aType )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleResourceChangeL aType: %d"), aType ) );
    CAknAppUi::HandleResourceChangeL(aType);

    if ( aType == KEikDynamicLayoutVariantSwitch )
        {
#ifdef RD_STARTUP_ANIMATION_CUSTOMIZATION
        iSysApShutdownImage->SetRect( ApplicationRect() );
#endif // RD_STARTUP_ANIMATION_CUSTOMIZATION

        TInt mode;
        User::LeaveIfError( RProperty::Get( KPSUidStartup, KPSGlobalStartupMode, mode ) );

        if ( mode == EStartupModeAlarm )
            {
            SetStatusPaneLayoutL( ESysApAlarm );
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetMemoryCardIndicatorL
// ----------------------------------------------------------------------------

void CSysApAppUi::SetMemoryCardIndicatorL()
    {
    if ( iSysApFeatureManager->Supported( KSysApFeatureIdMemoryCardIcon ) )
        {
#ifndef RD_MULTIPLE_DRIVE
        TSysApMemoryCardStatus memoryCardStatus = iSysApMMCObserver->MemoryCardStatus();

        TRACES( RDebug::Print( _L("CSysApAppUi::SetMemoryCardIndicatorL: memoryCardStatus=%d"), memoryCardStatus ) );

        if ( memoryCardStatus == ESysApMemoryCardInserted || memoryCardStatus == ESysApMemoryCardLocked )
#else // RD_MULTIPLE_DRIVE
        TInt insertedCount( CSysApDriveList::NonUsbDriveCount( iInsertedMemoryCards ) );

        TRACES( RDebug::Print( _L("CSysApAppUi::SetMemoryCardIndicatorL: insertedCount=%d"), insertedCount ) );

        if ( insertedCount > 0 )
#endif // RD_MULTIPLE_DRIVE
            {
            SetIndicatorStateL( EAknIndicatorMemoryCard, EAknIndicatorStateOn );
            }
        else
            {
            SetIndicatorStateL( EAknIndicatorMemoryCard, EAknIndicatorStateOff );
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SysApFeatureManager
// ----------------------------------------------------------------------------

CSysApFeatureManager& CSysApAppUi::SysApFeatureManager()
    {
    __ASSERT_DEBUG( iSysApFeatureManager, User::Panic( _L("iSysApFeatureManager == NULL"), KErrBadHandle ) );

    return *iSysApFeatureManager;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetUsbAttachStatus
// ----------------------------------------------------------------------------
#ifndef RD_MULTIPLE_DRIVE
void CSysApAppUi::SetUsbAttachStatus( const TBool aUsbAttached )
    {
    // Prevent double beep when USB file transfer gets activated but
    // allow MMC inserted beep when:
    // a) USB file transfer is active
    // b) MMC is not inserted when USB cable is attached
    iHideNextBeep = ( iMMCInserted && aUsbAttached );
    }
#else // RD_MULTIPLE_DRIVE
void CSysApAppUi::SetUsbAttachStatus( const TBool aUsbAttached )
    {
    TRACES( RDebug::Print(
        _L( "CSysApAppUi::SetUsbAttachStatus: aUsbAttached: %d" ),
        aUsbAttached ) );

    if ( aUsbAttached )
        {
        // For ignoring extra beeps caused by USB file transfer
        iSysApDriveList->ResetDrivesInsertBeepIgnored();
        iSysApDriveList->MarkDrivesInsertBeepIgnored( iInsertedMemoryCards );
        }
    }
#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::IhfEnabledL
// ----------------------------------------------------------------------------

TBool CSysApAppUi::IhfEnabledL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::IhfEnabledL(): iSysApAudioRoutingObserver=0x%x"), iSysApAudioRoutingObserver ) );

    if ( !iSysApAudioRoutingObserver ) // create observer if needed
        {
        iSysApAudioRoutingObserver = CSysApAudioRoutingObserver::NewL( *this );
        }

    return iSysApAudioRoutingObserver->IhfEnabled();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::IsDeviceModeKey
// ----------------------------------------------------------------------------

TBool CSysApAppUi::IsDeviceModeKey( const TKeyEvent& aKeyEvent ) const
    {
    return CSysApDefaultKeyHandler::IsDeviceModeKey( aKeyEvent );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::ResourcesFreed
// ----------------------------------------------------------------------------

TBool CSysApAppUi::ResourcesFreed() const
    {
    return iResourcesFreed;
    }

/* 
----------------------------------------------------------------------------
CSysApAppUi::IsEncryptionOperationOngoingL()
Introduced the code as a part of Sub : 405-3362 - Power Management SUB for ES 
recapture functionalities in platform which support multidrive as well. 
-------------------------------------------------------------------------------
*/
TBool CSysApAppUi::IsEncryptionOperationOngoingL() const
    {
		TBool deFeatureSupported(EFalse);
		FeatureManager::InitializeLibL();
		deFeatureSupported = FeatureManager::FeatureSupported( KFeatureIdFfDeviceEncryptionFeature);
		FeatureManager::UnInitializeLib();

		if(deFeatureSupported)
			{         
			/**
			 * Store the last memory status changed
			 * 0: Idle. It can be Encrypted or Decrypted
			 * 1: Encrypting
			 * 2: Decrypting
			 **/
			 
			RProperty deProperty;
			User::LeaveIfError(deProperty.Attach(KDevEncProtectedUid, KDevEncOperationKey,EOwnerThread));
			TInt deValue = 0;
			if((deProperty.Get(deValue)== KErrNone)&&( deValue == EOpEncrypting || deValue == EOpDecrypting))
				{
				deProperty.Close();
				return ETrue;
				}
			else
				{
				deProperty.Close();
				return EFalse;
				}
			}
		else
			{
			return EFalse;
			}
    }
// ----------------------------------------------------------------------------
// CSysApAppUi::EtelConnector
// ----------------------------------------------------------------------------

CSysApEtelConnector* CSysApAppUi::EtelConnector() const
    {
    return iSysApEtelConnector;
    }

#ifndef RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectUsed
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectUsed( TInt /*aDrive*/ )
    {
    }

#else // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::ShowEjectWaitNoteL
// ----------------------------------------------------------------------------

void CSysApAppUi::ShowEjectWaitNoteL( TInt aDriveToEject )
    {
    if ( iSysApWaitNote )
        {
        return;
        }
    HBufC* text = iSysApDriveList->GetFormattedDriveNameLC(
        aDriveToEject,
        0, // Not used
        R_QTN_EJECTING_MEMORY_NAME_WAIT );
    iSysApWaitNote = CSysApWaitNote::NewL(
        iSysApFeatureManager->CoverDisplaySupported() );
    iSysApWaitNote->ShowNoteL( EClosingApplicationsNote, text );
    CleanupStack::PopAndDestroy( text );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::IsEjectQueryVisible
// ----------------------------------------------------------------------------

TBool CSysApAppUi::IsEjectQueryVisible()
    {
    if ( !iSysApConfirmationQuery )
        {
        return EFalse;
        }
    TInt queryId( iSysApConfirmationQuery->CurrentQuery() );
    return ( queryId == ESysApEjectMmcQuery || queryId == ESysApRemoveMmcNote );
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UpdateInsertedMemoryCardsL
// ----------------------------------------------------------------------------

void CSysApAppUi::UpdateInsertedMemoryCardsL()
    {
    // Update inserted memory cards
    iSysApDriveList->GetMemoryCardsL(
        iInsertedMemoryCards, CSysApDriveList::EIncludeInserted );

    // Update memory card indicator status
    SetMemoryCardIndicatorL();

    // Handle unlock
    RunUnlockNotifierL();
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectUsed
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectUsed( TInt aDrive )
    {
    // Check drive inserted before starting eject confirm query
    TInt insertedIndex( CSysApDriveList::Find( iInsertedMemoryCards, aDrive ) );

    TRACES( RDebug::Print(
        _L( "CSysApAppUi::EjectUsed: drive: %d, index: %d" ),
        aDrive, insertedIndex ) );

    if ( insertedIndex == KErrNotFound )
        {
        return;
        }

    iMMCEjectUsed = ETrue;
    iDriveToEject = aDrive;
    iSysApDriveList->ResetDrivesToEject();
    TRAPD( err, EjectMMCL() );
    if ( err != KErrNone )
        {
        TRACES( RDebug::Print(
            _L( "CSysApAppUi::EjectUsed: err: %d" ), err ) );
        iMMCEjectUsed = EFalse;
        }
    }

#endif // RD_MULTIPLE_DRIVE

// ----------------------------------------------------------------------------
// CSysApAppUi::EjectMMCCanceled
// ----------------------------------------------------------------------------

void CSysApAppUi::EjectMMCCanceled()
    {
    // Called from eject confirm query, reset eject status
#ifdef RD_MULTIPLE_DRIVE
    iMMCEjectUsed = EFalse;
#endif // RD_MULTIPLE_DRIVE
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::UsbChargerDetector
// ----------------------------------------------------------------------------
//
TSysApUsbChargerDetector& CSysApAppUi::UsbChargerDetector()
    {
    return iSysApUsbChargerDetector;
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleUsbCharger
// ----------------------------------------------------------------------------
//
void CSysApAppUi::HandleUsbCharger( const TInt aValue )
    {        
    if ( aValue == EChargingStatusCharging ||
         aValue == EChargingStatusChargingContinued ||
         aValue == EChargingStatusAlmostComplete )
        {
        iSysApUsbChargerDetector.SetChargingUsed( ETrue );
        if ( !iSysApUsbIndicatorController &&
             iSysApFeatureManager->Supported( KSysApFeatureIdChargerReminderNotes ) &&
             iSysApFeatureManager->Supported( KSysApFeatureIdUsbChargingWithoutReminderNotes ) )
            {
            // Start observing USB state for the reminder note
            TRAPD ( usbErr, iSysApUsbIndicatorController = CreateSysApUsbIndicatorL( *this ) );
            if ( usbErr )
                {
                TRACES( RDebug::Print(
                    _L("CSysApAppUi::HandleUsbCharger: error in constructing USB ind. controller %d" ),
                    usbErr ) );
                }
            }
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::SetEnvelopeIndicatorL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::SetEnvelopeIndicatorL()
    {
    TInt phoneStoreStatus( StateOfProperty( KUidPSSMSStackCategory, KUidPSSMSStackDiskSpaceMonitorKey ) );
    TInt inboxStatus( StateOfProperty( KUidSystemCategory, KUidInboxStatusValue ) );
    TBool simStoreFull( iSysApEtelConnector && iSysApEtelConnector->IsSimSmsStoreFull() );

    TRACES( RDebug::Print(
        _L("CSysApAppUi::SetEnvelopeIndicatorL() phoneStoreStatus %d, simStoreFull %d, inboxStatus %d"),
        phoneStoreStatus, simStoreFull, inboxStatus ) );

    if ( phoneStoreStatus == ESmsDiskSpaceFull || simStoreFull )
        {
        // Blink the Envelope indicator
        SetIndicatorStateL( EAknIndicatorEnvelope, EAknIndicatorStateAnimate );
        }
    else if ( inboxStatus == ESADocumentsInInbox )
        {
        // Show the Envelope indicator.
        SetIndicatorStateL( EAknIndicatorEnvelope, EAknIndicatorStateOn );
        }
    else
        {
        // Hide the Envelope indicator.
        SetIndicatorStateL( EAknIndicatorEnvelope, EAknIndicatorStateOff );
        }
    }
    
// ----------------------------------------------------------------------------
// CSysApAppUi::LogsObserverL
// ----------------------------------------------------------------------------
//
CSysApCenRepLogsObserver& CSysApAppUi::LogsObserverL()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::LogsObserverL()") ) );
    
    // Create Logs observer when it is needed for the first time
    if ( !iSysApCenRepLogsObserver )
        {
        iSysApCenRepLogsObserver = CSysApCenRepLogsObserver::NewL( *this );
        }
    
    return *iSysApCenRepLogsObserver;
    }
    
// ----------------------------------------------------------------------------
// CSysApAppUi::NotifyPowerSaveModeL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::NotifyPowerSaveModeL( TSysApPsmStatus aStatus )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::NotifyPowerSaveModeL aStatus: %d"), aStatus ) );

    // cancel any active power saving query because user has changed the state manually
    CancelQuery( ESysApBattChargingPowerSavingQuery );
    CancelQuery( ESysApBattLowPowerSavingQuery );

    switch ( aStatus )
        {
        case MSysApPsmControllerNotifyCallback::EPsmActivationComplete:
            UpdateBatteryBarsL( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );
            ShowUiNoteL( EPowerSaveModeActivated );
           break;
        
        case MSysApPsmControllerNotifyCallback::EPsmDeactivationComplete:
            UpdateBatteryBarsL( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );
            ShowUiNoteL( EPowerSaveModeDeactivated );
            break;
            
        case MSysApPsmControllerNotifyCallback::EPsmActivationFailed:
            ShowUiNoteL( ECannotActivatePowerSaveMode );
            break;
            
        case MSysApPsmControllerNotifyCallback::EPsmDeactivationFailed:
            ShowUiNoteL( ECannotDeactivatePowerSaveMode );
            break;            
        
        default:
            break;   
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleBatteryStatusL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::HandleBatteryStatusL( const TInt aValue )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleBatteryStatusL aValue: %d"), aValue ) );
    
    if ( aValue == EBatteryStatusEmpty )
        {
        //Display Recharge Battery note
        ShowUiNoteL( ERechargeBatteryNote );
        BatteryEmptyL();
        }
    else if ( aValue == EBatteryStatusLow )
        {
        if ( iSysApPsmController && UiReady() )
            {
            iSysApPsmController->BatteryLow( ETrue );
            
            if ( iSysApPsmController->ShowActivateQuery())
                {
                // show activation query, replaces the first battery low query
                ShowQueryL( ESysApBattLowPowerSavingQuery );
                }
            else // default low warning note must be shown
                {
                // activate partial power save mode on first low warning
                // Enabling Partial Psm automatically disabled 
                //iSysApPsmController->DoEnablePartialPsm( ETrue ); // activated on first warning note 
                //Display Battery Low note.
                ShowUiNoteL( EBatteryLowNote );    
                }                
            }
        else
            {
            //Display Battery Low note.
            ShowUiNoteL( EBatteryLowNote );    
            }            
        }
    
    if ( iSysApBatteryInfoController )
        {
        iSysApBatteryInfoController->BatteryStatusUpdated( aValue );
        }
            
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleChargingStatusL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::HandleChargingStatusL( const TInt aValue )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleChargingStatusL aValue: %d"), aValue ) );
    
    TBool showNote( ETrue );
    
    UpdateBatteryBarsL( StateOfProperty( KPSUidHWRMPowerState, KHWRMBatteryLevel ) );

    if ( iSysApPsmController && UiReady() ) 
        {
        if ( iCharging ) // iCharging updated in UpdateBatteryBarsL
            {
            // cancel activation query if charger is connected while query is on display
            CancelQuery( ESysApBattLowPowerSavingQuery );
            }
             
        if ( iCharging && !iSysApPsmController->ChargerConnected() ) // first time after charger connection
            {
            iSysApPsmController->ConnectCharger( ETrue );
            
            if ( iSysApPsmController->ShowDeactivateQuery() )
                {
                ShowQueryL( ESysApBattChargingPowerSavingQuery );
                // Query is on the display. Don't show the note.
                showNote = EFalse;               
                }
            else
                {
                iSysApPsmController->DoEnablePartialPsm( EFalse );
                }                
            }
        else if ( aValue == EChargingStatusNotConnected )
            {
            iSysApPsmController->ConnectCharger( EFalse );
            
            // cancel deactivation query if charger is disconnected while query is on display 
            CancelQuery( ESysApBattChargingPowerSavingQuery );
            }            
        }
    if( showNote )
        {
        HandleChargerNotesL( aValue );    
        }
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleActivatePsmQueryResponse
// ----------------------------------------------------------------------------
//
void CSysApAppUi::HandleActivatePsmQueryResponse( TBool aEnable )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleActivatePsmQueryResponse aEnable: %d"), 
        aEnable ) );
    
    __ASSERT_DEBUG( iSysApPsmController, User::Invariant() );
    
    if ( aEnable )
        {
        iSysApPsmController->DoEnableFullPsm( ETrue );    
        }
    
    // Enabling Partial Psm when the user selects 'No' is diabled.
    /*
    else
        {
        //iSysApPsmController->DoEnablePartialPsm( ETrue );  // bhaskar     
        }
     */
              
    }

// ----------------------------------------------------------------------------
// CSysApAppUi::HandleDeactivatePsmQueryResponse
// ----------------------------------------------------------------------------
//
void CSysApAppUi::HandleDeactivatePsmQueryResponse( TBool aDisable )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::HandleDeactivatePsmQueryResponse aDisable: %d"), 
        aDisable ) );

    __ASSERT_DEBUG( iSysApPsmController, User::Invariant() );
    
    if ( aDisable )
        {
        iSysApPsmController->DoEnableFullPsm( EFalse );        
        }
    }
    
// ----------------------------------------------------------------------------
// CSysApAppUi::CancelQuery
// ----------------------------------------------------------------------------
//
void CSysApAppUi::CancelQuery( TSysApConfirmationQueryIds aQueryId )
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::CancelQuery aQueryId: %d"), aQueryId ) );

    if ( iSysApConfirmationQuery )
        {
        if ( aQueryId == iSysApConfirmationQuery->CurrentQuery() || aQueryId == ESysApNoQuery )
            {
            iSysApConfirmationQuery->Cancel();
            }
        }
    }

TBool CSysApAppUi::IsDefaultPowerKeyBehavior()
    {
    TRACES( RDebug::Print( _L("CSysApAppUi::IsDefaultPowerKeyBehavior" ) ) );
    TBool powerKeyIsLockKey = iSysApFeatureManager->PowerKeyIsLockKey();
    if ( !powerKeyIsLockKey )
        return ETrue; //no need for further processing
    TInt securityDialogStatus = StateOfProperty( KPSUidStartup, KStartupSecurityCodeQueryStatus );
    TRACES( RDebug::Printf( "CSysApAppUi::IsDefaultPowerKeyBehavior: securityDialogStatus = %d", securityDialogStatus ) );
    if ( securityDialogStatus == ESecurityQueryActive  )
        {
        // handle the exception from the exception. Currently used only when a long power key press is detected
        return ETrue; 
        }
    //make sure we can turn off device when 'sim invalid' dlg is displayed
    TInt simStatus = StateOfProperty( KPSUidStartup, KStartupSimSecurityStatus ); 
    TBool simBogus = ( simStatus == ESimRejected ) || ( simStatus == ESimUnaccepted ) || (simStatus == ESimInvalid );
    
    // check if the phone has fully booted into idle  
    TInt startupPhase = StateOfProperty ( KPSUidStartup, KPSStartupUiPhase );
    TBool bootCompleted = (EStartupUiPhaseAllDone == startupPhase);
    
	// Another exception from the exception:
    // check if we're in charging or alert mode (only then we have a status pane instance)    
    TBool haveStatusPane = ( StatusPane()== NULL ) ? EFalse : StatusPane()->IsVisible();
    TBool defaultPowerKeyBehaviour = !bootCompleted || simBogus || haveStatusPane || iPowerKeyPopupMenuActive || !powerKeyIsLockKey;
    
    TRACES( RDebug::Printf( "CSysApAppUi::IsDefaultPowerKeyBehavior: Sim Status = %d, haveStatusPane = %d, power key menu active? %d" , simStatus, haveStatusPane, iPowerKeyPopupMenuActive  ) );       
    TRACES( RDebug::Printf( "CSysApAppUi::IsDefaultPowerKeyBehavior returns %d", defaultPowerKeyBehaviour ) );   
    TRACES( RDebug::Printf( "CSysApAppUi::IsDefaultPowerKeyBehavior: startupPhase = %d", startupPhase ) );
    return defaultPowerKeyBehaviour;
    }
	
// ----------------------------------------------------------------------------
// CSysApAppUi::StartChargingBatteryL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::StartChargingBatteryL() 
	{
	TRACES( RDebug::Print( _L("CSysApAppUi::StartChargingBatteryL") ) );
    iBatteryNotify->StartChargingL();
    }
	
// ----------------------------------------------------------------------------
// CSysApAppUi::StopChargingBatteryL
// ----------------------------------------------------------------------------
//
void CSysApAppUi::StopChargingBatteryL()
	{
	TRACES( RDebug::Print( _L("CSysApAppUi::StopChargingBatteryL") ) );
    iBatteryNotify->StopChargingL();
    }
	
// End of File
