// Copyright (c) 2002-2010 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Phorepoc board copy (OURS delta on the EPL original): the licensee TSY stub's mock handler, extended in
// telephony stage T1 (docs/phorepoc/telephony-plan.md) with a FAKE MODEM (CFakeModem below): registration, signal,
// operator, IMEI/IMSI, mobile-originated and incoming calls, SMS loopback. Earlier deltas: the X4.72 lock info,
// the 2026-10-12 hardening (one-shot requests fail instead of hanging).

//  INCLUDE FILES

#include "OstTraceDefinitions.h"
#ifdef OST_TRACE_COMPILER_IN_USE
#include "cmmmockmesshandlerTraces.h"
#endif

#include "cmmmockmesshandler.h"
#include "cmmmessagerouter.h"
#include "cmmcustomstubext.h"
#include <pcktcs.h>
#include <ctsy/rmmcustomapi.h>
#include <ctsy/serviceapi/cmmsmsutility.h>
#include <ctsy/serviceapi/CMmUtility.h>
#include <ctsy/serviceapi/cmmgsmphonestorageutility.h>   // CStorageInfoData, CPhoneBookStoreEntry
#include <e32property.h>
#include <e32debug.h>
#ifdef PHE_LTSY
#include "pheltsy_at.h"   // modem rung M2: the AT engine (PHELTSY=1 builds only)
#endif

// callback timer interval
const TInt KTimeInterval = 1000000;
const TUint KTmpPckgSize = 15;

// ============================================================================================================
// Phorepoc T1: the FAKE MODEM. CTSY is driven exactly as a licensee TSY drives it: requests arrive in ExtFuncL,
// answers go back through MessageManager()->Complete(ipc, package, result), and the modem's own events (boot
// ready, registration, signal, an incoming call/SMS) are completions nobody asked for. Everything is scripted
// from the constants below; answers are posted to a small event queue that a 100 ms ticker drains (CTSY
// registers the client's request handle AFTER ExtFuncL returns, so a completion made inside ExtFuncL would
// find nobody waiting). CFakeModem derives from CPeriodic only so that the mock handler's iTimer member (the
// header is unchanged) can own it.
//
// The scripted world: operator "Phorepoc Mobile" (MCC 001 / MNC 01, WCDMA), 80 % signal, a SIM with IMSI
// 001010123456789, an IMEI, any dialled number connects after ~2 s, an SMS that is submitted is delivered
// back. Extra stimuli through P&S (category 0xE7015730 = phetelprobe's SID, which defines them): key 1 = ring
// this number (descriptor), key 2 = receive this text as an SMS.
// ============================================================================================================
_LIT(KFmOperatorLong, "Phorepoc Mobile");
_LIT(KFmOperatorShort, "Phorepoc");
_LIT(KFmMcc, "001");
_LIT(KFmMnc, "01");
_LIT8(KFmImei, "356938035643809");
_LIT8(KFmImsi, "001010123456789");
_LIT(KFmSmsc, "+358508771010");
const TInt KFmTick = 100000;               // 100 ms
const TInt KFmSignalBars = 5;              // absolute bars of 7 (SysAp hands the value to the Avkon signal pane as is)
const TInt KFmSignalRssi = -67;
const TUid KFmPropCat = { 0xE7015730 };
const TUint KFmPropRing = 1;
const TUint KFmPropSms = 2;

enum TFmEvent
    {
    EFmNone,
    EFmBoot,            // the modem comes up: ModemStatusReady, security, RF on
    EFmReg,             // answer GetNetworkRegistrationStatus (and announce the network)
    EFmNetwork,         // mode, current network, signal
    EFmIdent,           // IMEI / IMSI (GetPhoneId / GetSubscriberId)
    EFmOperatorName,    // CustomAPI operator name
    EFmProviderName,    // CustomAPI network provider name
    EFmDial,            // MO call: call info, Dialling
    EFmDialProgress,    // Connecting / Connected + the Dial completion
    EFmHangUp,          // Disconnecting, Idle, HangUp completion
    EFmAnswer,          // MT call: Connected + Answer completion
    EFmSmsSent,         // SendMessage completion (msg ref + empty submit report)
    EFmSmsDeliver,      // an MT SMS (the TPDU in iSms)
    EFmIccType,         // the card type (USIM); CTSY then asks for the active PIN
    EFmActivePin,       // the active PIN (PIN1)
    EFmPbInit,          // SIM phonebook store initialisation (sizes); iArg = slot of the store name in iPbName
    EFmPbCache,         // SIM phonebook cache: the ADN entries
    EFmPbInfo,          // used-entries count (SDN / VMBX)
    EFmSmspList,        // the SIM's SMS parameter sets (one: the service centre) -- Messaging's ncnlist reads them on first boot
#ifdef PHE_LTSY
    EFmSim,             // pheltsy: the SIM state from AT+CPIN -> EMobilePhoneNotifySecurityEvent
#endif
    EFmComplete         // a data-less completion of request iArg with result iArg2 (CTSY stores a request handle only AFTER ExtFuncL returns)
    };

struct TFmQueued
    {
    TFmEvent iEvent;
    TInt iTicks;
    TInt iArg;
    TInt iArg2;
    };

#ifdef PHE_LTSY
class CFakeModem : public CPeriodic, public MPheAtObserver
#else
class CFakeModem : public CPeriodic
#endif
    {
public:
#ifdef PHE_LTSY
    ~CFakeModem() { delete iAt; }
    // pheltsy: the AT engine's changes become the same completions the scripted modem makes
    void PheRegChanged() { if ( iBooted ) Post( EFmReg, 1 ); }
    void PheNetChanged() { if ( iBooted ) Post( EFmNetwork, 1 ); }
    void PheSimChanged() { Post( EFmSim, iBooted ? 1 : 30 ); }
    void PheCodeResult( TInt aErr ) { PostComplete( EMobilePhoneVerifySecurityCode, aErr ); }
    // the SIM state as the security events CTSY's CMmSecurityTsy expects (it caches them until a client listens):
    // no card -> ENoICCFound, PIN -> EPin1Required (the S60 PIN query), PUK -> EPuk1Required, READY after a code -> EPin1Verified
    void DoSim()
        {
        if ( !iAt )
            return;
        RMobilePhone::TMobilePhoneSecurityEvent ev;
        switch ( iAt->Sim() )
            {
            case EPheSimAbsent: ev = RMobilePhone::ENoICCFound; break;
            case EPheSimPin: ev = RMobilePhone::EPin1Required; break;
            case EPheSimPuk: ev = RMobilePhone::EPuk1Required; break;
            case EPheSimReady:
                if ( iLastSim != EPheSimPin && iLastSim != EPheSimPuk )
                    {
                    iLastSim = EPheSimReady;
                    return;                        // no code was asked: nothing to tell
                    }
                ev = iLastSim == EPheSimPuk ? RMobilePhone::EPuk1Verified : RMobilePhone::EPin1Verified;
                break;
            default: return;
            }
        iLastSim = iAt->Sim();
        CMmDataPackage pkg;
        pkg.PackData( &ev );
        Complete( EMobilePhoneNotifySecurityEvent, &pkg );
        RDebug::Printf( "[pheltsy] security event %d", (TInt)ev );
        }
    TPheSim iLastSim;

    // ---- M4 calls over AT: the same CTSY call-state completions the scripted modem makes, driven by AT+CLCC
    TBool iDialPending, iAnswerPending, iHangPending;
    void PheCall( TInt aStat, TBool aMt, const TDesC8& aNumber )
        {
        RDebug::Printf( "[pheltsy] call: stat %d mt %d (status now %d)", aStat, aMt, (TInt)iCallStatus );
        switch ( aStat )
            {
            case 3:                                 // alerting: the far end rings
                if ( iCallStatus == RMobileCall::EStatusDialling )
                    CallStatus( RMobileCall::EStatusConnecting );
                break;
            case 0:                                 // active
                if ( iCallStatus == RMobileCall::EStatusDialling )
                    CallStatus( RMobileCall::EStatusConnecting );
                if ( iCallStatus != RMobileCall::EStatusConnected )
                    {
                    CallStatus( RMobileCall::EStatusConnected );
                    iAt->VoiceRoute( ETrue );       // M5: the earpiece + MIC1 <-> the modem's PCM
                    CCallDataPackage pkg;
                    pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
                    if ( iDialPending ) { iDialPending = EFalse; Complete( EEtelCallDial, &pkg ); }
                    if ( iAnswerPending ) { iAnswerPending = EFalse; Complete( EEtelCallAnswer, &pkg ); }
                    }
                break;
            case 4:                                 // incoming
            case 5:                                 // waiting (shown as a new incoming call; no hold yet)
                if ( iCallStatus == RMobileCall::EStatusIdle )
                    {
                    TBuf<RMobilePhone::KMaxMobileTelNumberSize> num;
                    num.Copy( aNumber.Left( num.MaxLength() ) );
                    DoRing( num );
                    }
                break;
            case -1:                                // no call any more
                CallGone( KErrNone );
                break;
            default:
                break;
            }
        (void)aMt;
        }
    // the call ended: our hang-up, a failed dial, or the far end
    void CallGone( TInt aErr )
        {
        if ( iCallStatus == RMobileCall::EStatusIdle )
            return;
        iAt->VoiceRoute( EFalse );
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        if ( iDialPending )
            {
            iDialPending = EFalse;
            CallStatus( RMobileCall::EStatusIdle );
            Complete( EEtelCallDial, &pkg, aErr ? aErr : KErrEtelNoAnswer );
            return;
            }
        if ( iAnswerPending )
            {
            iAnswerPending = EFalse;
            CallStatus( RMobileCall::EStatusIdle );
            Complete( EEtelCallAnswer, &pkg, KErrGeneral );
            return;
            }
        CallStatus( RMobileCall::EStatusDisconnecting );
        CallStatus( RMobileCall::EStatusIdle );
        if ( iHangPending )
            {
            iHangPending = EFalse;
            Complete( EEtelCallHangUp, &pkg );
            }
        RDebug::Printf( "[pheltsy] call %d released", iCallId );
        }
    void PheCallCmdResult( TInt aKind, TInt aErr )
        {
        if ( aErr == KErrNone )
            return;                                 // progress comes from AT+CLCC
        if ( aKind == CPheAt::ECmdDial )
            CallGone( KErrEtelNoAnswer );
        else if ( aKind == CPheAt::ECmdAnswer )
            CallGone( KErrGeneral );
        else if ( aKind == CPheAt::ECmdHangUp && iHangPending )
            {
            // AT+CHUP refused: the call may already be gone; AT+CLCC decides (CallGone completes HangUp)
            }
        }
    // ---- M4 SMS over AT (PDU mode)
    void PheSmsSent( TInt aErr, TInt aRef )
        {
        TInt16 ref = (TInt16)aRef;
        TBuf8<RMobileSmsMessaging::KGsmTpduSize> report;
        CMmDataPackage pkg;
        pkg.PackData( &ref, &report );
        Complete( EMobileSmsMessagingSendMessage, &pkg, aErr );
        }
    void PheSmsReceived( const TDesC8& aTpdu, const TDesC& aSmsc )
        {
        iSms.iSmsMsg.Copy( aTpdu.Left( iSms.iSmsMsg.MaxLength() ) );
        FillSmsCommon();
        if ( aSmsc.Length() )
            iSms.iServiceCentre.Copy( aSmsc.Left( iSms.iServiceCentre.MaxLength() ) );
        DoSmsDeliver();
        }
    CPheAt* iAt;
#endif
    static CFakeModem* NewL( CMmMessageRouter* aRouter )
        {
        CFakeModem* self = new( ELeave ) CFakeModem( aRouter );
        CleanupStack::PushL( self );
        self->ConstructL();
        CleanupStack::Pop( self );
        return self;
        }
    void Post( TFmEvent aEvent, TInt aTicks, TInt aArg = 0, TInt aArg2 = 0 )
        {
        for ( TInt i = 0; i < KQueue; i++ )
            {
            if ( iQueue[i].iEvent == EFmNone )
                {
                iQueue[i].iEvent = aEvent;
                iQueue[i].iTicks = aTicks;
                iQueue[i].iArg = aArg;
                iQueue[i].iArg2 = aArg2;
                return;
                }
            }
        }
    void StartDial( const TDesC& aNumber )
        {
        iNumber.Copy( aNumber.Left( iNumber.MaxLength() ) );
        iCallId = 1;
        iCallStatus = RMobileCall::EStatusDialling;
        Post( EFmDial, 1 );
        }
    void StartHangUp() { Post( EFmHangUp, 1 ); }
    void StartAnswer() { Post( EFmAnswer, 1 ); }
    void SmsSubmitted( const TDesC8* aTpdu )
        {
        iSmsSubmit.Zero();
        if ( aTpdu )
            iSmsSubmit.Copy( aTpdu->Left( iSmsSubmit.MaxLength() ) );
        Post( EFmSmsSent, 1 );
        }
    void BootSoon() { Post( EFmBoot, 25 ); }
    void PostIdent( TBool aImsi ) { Post( EFmIdent, 2, aImsi ); }
    void PostReg() { Post( EFmReg, 3 ); }
    // SIM phonebook (CTSY CMmPhoneBookStoreTsy): the request carries the store name; keep it for the answer
    void PostPb( TFmEvent aEvent, const CMmDataPackage* aPkg )
        {
        TName name;
        static_cast<const CPhoneBookDataPackage*>( aPkg )->GetPhoneBookName( name );
        iPbSlot = ( iPbSlot + 1 ) % KPbSlots;
        iPbName[iPbSlot] = name;
        Post( aEvent, 1, iPbSlot );
        }
    void PostComplete( TInt aIpc, TInt aResult ) { Post( EFmComplete, 1, aIpc, aResult ); }
    void PostOperatorName() { Post( EFmOperatorName, 2 ); }
    void PostProviderName() { Post( EFmProviderName, 2 ); }

private:
    enum { KQueue = 24, KPbSlots = 4 };
    TName iPbName[KPbSlots];
    TInt iPbSlot;
    CFakeModem( CMmMessageRouter* aRouter )
        : CPeriodic( CActive::EPriorityStandard ), iRouter( aRouter ), iCallId( 0 ),
          iCallStatus( RMobileCall::EStatusIdle ), iMsgRef( 0 ), iBooted( EFalse ),
          iRingSeen( EFalse ), iSmsSeen( EFalse ), iPoll( 0 ), iPbSlot( 0 )
        {
        }
    void ConstructL()
        {
        CTimer::ConstructL();
        CActiveScheduler::Add( this );
#ifdef PHE_LTSY
        TRAPD( atErr, iAt = CPheAt::NewL( *this ) );
        RDebug::Printf( "[pheltsy] AT engine -> %d (registration/network/signal from the EG25-G)", atErr );
#endif
        Start( KFmTick, KFmTick, TCallBack( Tick, this ) );
        }
    static TInt Tick( TAny* aThis )
        {
        static_cast<CFakeModem*>( aThis )->DoTick();
        return ETrue;
        }
    MmMessageManagerCallback* Mgr() { return iRouter->MessageManager(); }

    // ---- the completions (each is one modem answer / unsolicited event)
    void Complete( TInt aIpc, TInt aResult = KErrNone )
        {
        if ( Mgr() )
            Mgr()->Complete( aIpc, aResult );
        }
    void Complete( TInt aIpc, CMmDataPackage* aPackage, TInt aResult = KErrNone )
        {
        if ( Mgr() )
            Mgr()->Complete( aIpc, aPackage, aResult );
        }
    void CallStatus( RMobileCall::TMobileCallStatus aStatus )
        {
        iCallStatus = aStatus;
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        pkg.PackData( &aStatus );
        Complete( EMobileCallNotifyMobileCallStatusChange, &pkg );
        }
    void CallInfo( TInt aIpc, TBool aMt )
        {
        RMobileCall::TMobileCallInfoV1 info;
        info.iValid = RMobileCall::KCallId | RMobileCall::KCallRemoteParty | RMobileCall::KCallDialledParty;
        info.iService = RMobilePhone::EVoiceService;
        info.iStatus = iCallStatus;
        info.iCallId = iCallId;
        info.iEmergency = EFalse;
        info.iRemoteParty.iRemoteIdStatus = RMobileCall::ERemoteIdentityAvailable;
        info.iRemoteParty.iDirection = aMt ? RMobileCall::EMobileTerminated : RMobileCall::EMobileOriginated;
        info.iRemoteParty.iRemoteNumber.iTelNumber.Copy( iNumber );
        info.iRemoteParty.iRemoteNumber.iTypeOfNumber = ( iNumber.Length() && iNumber[0] == '+' )
            ? RMobilePhone::EInternationalNumber : RMobilePhone::EUnknownNumber;
        info.iRemoteParty.iRemoteNumber.iNumberPlan = RMobilePhone::EIsdnNumberPlan;
        if ( !aMt )
            {
            info.iDialledParty.iTelNumber.Copy( iNumber );
            info.iDialledParty.iTypeOfNumber = info.iRemoteParty.iRemoteNumber.iTypeOfNumber;
            info.iDialledParty.iNumberPlan = RMobilePhone::EIsdnNumberPlan;
            }
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        pkg.PackData( &info );
        Complete( aIpc, &pkg );
        }
    void DoBoot()
        {
        iBooted = ETrue;
        RDebug::Printf( "[fakemodem] boot: modem ready" );
        TRfStateInfo rf = ERfsStateInfoNormal;     // RF on: not in flight mode
        CMmDataPackage rfPkg;
        rfPkg.PackData( &rf );
        Complete( EMmTsyBootGetRFStatusIPC, &rfPkg );
        Complete( EMmTsyBootNotifySecurityReadyIPC );
        // CTSY now asks for the registration status, SIM status, SMS routing
        Complete( EMmTsyBootNotifyModemStatusReadyIPC );
        }
    void DoReg()
        {
        RMobilePhone::TMobilePhoneRegistrationStatus reg = RMobilePhone::ERegisteredOnHomeNetwork;
#ifdef PHE_LTSY
        if ( iAt ) reg = iAt->Net().iReg;   // pheltsy: AT+CREG
#endif
        RMobilePhone::TMobilePhoneNetworkStatus nw = RMobilePhone::ENetworkStatusCurrent;
        CMmDataPackage pkg;
        pkg.PackData( &reg, &nw );
        Complete( EMobilePhoneNotifyNetworkRegistrationStatusChange, &pkg );
        Complete( EMobilePhoneGetNetworkRegistrationStatus );
        RDebug::Printf( "[fakemodem] registered on the home network" );
        Post( EFmNetwork, 2 );
        }
    void DoNetwork()
        {
        RMobilePhone::TMobilePhoneNetworkMode mode = RMobilePhone::ENetworkModeWcdma;
#ifdef PHE_LTSY
        if ( iAt ) mode = iAt->Net().iMode;
#endif
        CMmDataPackage m;
        m.PackData( &mode );
        Complete( EMobilePhoneNotifyModeChange, &m );

        RMobilePhone::TMobilePhoneNetworkInfoV8 net;
        net.iMode = RMobilePhone::ENetworkModeWcdma;
        net.iStatus = RMobilePhone::ENetworkStatusCurrent;
        net.iBandInfo = RMobilePhone::EBandUnknown;
        net.iCountryCode.Copy( KFmMcc );
        net.iNetworkId.Copy( KFmMnc );
        net.iDisplayTag.Copy( KFmOperatorLong );
        net.iShortName.Copy( KFmOperatorShort );
        net.iLongName.Copy( KFmOperatorLong );
        net.iAccess = RMobilePhone::ENetworkAccessUtran;
#ifdef PHE_LTSY
        if ( iAt )                         // pheltsy: AT+COPS (numeric + long name)
            {
            const TPheNet& a = iAt->Net();
            net.iMode = a.iMode; net.iAccess = a.iAccess;
            net.iCountryCode.Copy( a.iMcc ); net.iNetworkId.Copy( a.iMnc );
            net.iDisplayTag.Copy( a.iName ); net.iShortName.Copy( a.iName.Left( net.iShortName.MaxLength() ) ); net.iLongName.Copy( a.iName );
            }
#endif
        net.iHsdpaAvailableIndicator = EFalse;
        net.iEgprsAvailableIndicator = EFalse;
        net.iHsupaAvailableIndicator = EFalse;
        RMobilePhone::TMobilePhoneLocationAreaV1 area;
        area.iAreaKnown = ETrue;
        area.iLocationAreaCode = 0x1234;
        area.iCellId = 0x00ABCD;
#ifdef PHE_LTSY
        if ( iAt ) { area.iAreaKnown = iAt->Net().iLac > 0; area.iLocationAreaCode = iAt->Net().iLac; area.iCellId = iAt->Net().iCellId; }
#endif
        CMmDataPackage n;
        n.PackData( &net, &area );
        Complete( EMobilePhoneNotifyCurrentNetworkChange, &n );

        TInt8 bars = KFmSignalBars;
        TInt32 rssi = KFmSignalRssi;
#ifdef PHE_LTSY
        if ( iAt ) { bars = (TInt8)iAt->Net().iBars; rssi = iAt->Net().iRssiDbm; }   // pheltsy: AT+CSQ
#endif
        CMmDataPackage s;
        s.PackData( &bars, &rssi );
        Complete( EMobilePhoneNotifySignalStrengthChange, &s );
        RDebug::Print( _L("[fakemodem] network '%S', signal %d of 7 bars"), &KFmOperatorLong, (TInt)bars );
        }
    void DoIdent( TBool aImsi )
        {
        TBuf8<KTmpPckgSize> id;
        id.Copy( aImsi ? KFmImsi() : KFmImei() );
#ifdef PHE_LTSY
        if ( iAt && ( aImsi ? iAt->Imsi() : iAt->Imei() ).Length() )   // pheltsy: AT+CIMI / AT+CGSN
            id.Copy( ( aImsi ? iAt->Imsi() : iAt->Imei() ).Left( KTmpPckgSize ) );
#endif
        CMmDataPackage pkg;
        pkg.PackData( &id );
        Complete( aImsi ? EMobilePhoneGetSubscriberId : EMobilePhoneGetPhoneId, &pkg );
        }
    void DoOperatorName()
        {
        RMmCustomAPI::TOperatorNameInfo info;
        info.iType = RMmCustomAPI::EOperatorNameFlexiblePlmn;
        info.iName.Copy( KFmOperatorLong );
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: AT+COPS long name (empty = no operator)
            info.iName.Copy( iAt->Net().iName.Left( info.iName.MaxLength() ) );
#endif
        CMmDataPackage pkg;
        pkg.PackData( &info );
        Complete( ECustomGetOperatorNameIPC, &pkg );
        }
    void DoProviderName()
        {
        TBuf<RMobilePhone::KMaxMobileNameSize> name( KFmOperatorLong );
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: the SIM's SPN (AT+QSPN), else the network name
            name.Copy( iAt->Net().iSpn.Length() ? iAt->Net().iSpn : iAt->Net().iName );
#endif
        TDes* namePtr = &name;
        CMmDataPackage pkg;
        pkg.PackData( &namePtr );
        Complete( ECustomGetNetworkProviderNameIPC, &pkg );
        }
    void DoDial()
        {
        RDebug::Print( _L("[fakemodem] dial %S -> call %d"), &iNumber, iCallId );
        CallInfo( EMobileCallGetMobileCallInfo, EFalse );
        CallStatus( RMobileCall::EStatusDialling );
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: ATD<number>; progress from AT+CLCC
            {
            iDialPending = ETrue;
            const TInt r = iAt->Dial( iNumber );
            if ( r != KErrNone )
                CallGone( r );
            return;
            }
#endif
        Post( EFmDialProgress, 6, 0 );          // "ringing" for 600 ms
        }
    void DoDialProgress( TInt aStep )
        {
        if ( iCallStatus == RMobileCall::EStatusIdle )
            return;                             // hung up meanwhile
        if ( aStep == 0 )
            {
            CallStatus( RMobileCall::EStatusConnecting );
            Post( EFmDialProgress, 15, 1 );     // answered after 1.5 s
            }
        else
            {
            CallStatus( RMobileCall::EStatusConnected );
            CCallDataPackage pkg;
            pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
            Complete( EEtelCallDial, &pkg );
            RDebug::Printf( "[fakemodem] call %d connected", iCallId );
            }
        }
    void DoHangUp()
        {
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: AT+CHUP; completed when AT+CLCC lists no call
            {
            iHangPending = ETrue;
            if ( iAt->HangUp() != KErrNone )
                CallGone( KErrNone );
            return;
            }
#endif
        CallStatus( RMobileCall::EStatusDisconnecting );
        CallStatus( RMobileCall::EStatusIdle );
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        Complete( EEtelCallHangUp, &pkg );
        RDebug::Printf( "[fakemodem] call %d released", iCallId );
        }
    void DoRing( const TDesC& aNumber )
        {
        iNumber.Copy( aNumber.Left( iNumber.MaxLength() ) );
        iCallId = 2;
        iCallStatus = RMobileCall::EStatusRinging;
        RMobileCall::TMobileCallInfoV1 info;
        info.iValid = RMobileCall::KCallId | RMobileCall::KCallRemoteParty;
        info.iService = RMobilePhone::EVoiceService;
        info.iStatus = RMobileCall::EStatusRinging;
        info.iCallId = iCallId;
        info.iRemoteParty.iRemoteIdStatus = RMobileCall::ERemoteIdentityAvailable;
        info.iRemoteParty.iDirection = RMobileCall::EMobileTerminated;
        info.iRemoteParty.iRemoteNumber.iTelNumber.Copy( iNumber );
        info.iRemoteParty.iRemoteNumber.iTypeOfNumber = RMobilePhone::EInternationalNumber;
        info.iRemoteParty.iRemoteNumber.iNumberPlan = RMobilePhone::EIsdnNumberPlan;
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        pkg.PackData( &info );
        Complete( EEtelLineNotifyIncomingCall, &pkg );
        CallStatus( RMobileCall::EStatusRinging );
        RDebug::Print( _L("[fakemodem] incoming call from %S"), &iNumber );
        }
    void DoAnswer()
        {
        CallStatus( RMobileCall::EStatusAnswering );
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: ATA; Connected + the Answer completion from AT+CLCC
            {
            iAnswerPending = ETrue;
            if ( iAt->Answer() != KErrNone )
                CallGone( KErrGeneral );
            return;
            }
#endif
        CallStatus( RMobileCall::EStatusConnected );
        CCallDataPackage pkg;
        pkg.SetCallIdAndMode( iCallId, RMobilePhone::EVoiceService );
        Complete( EEtelCallAnswer, &pkg );
        RDebug::Printf( "[fakemodem] incoming call answered" );
        }
    void DoSmsSent()
        {
#ifdef PHE_LTSY
        if ( iAt )                                  // pheltsy: AT+CMGS in PDU mode; completed from PheSmsSent
            {
            const TInt r = iAt->SendSms( iSmsSubmit );
            if ( r != KErrNone )
                PheSmsSent( r, 0 );
            return;
            }
#endif
        TInt16 ref = ++iMsgRef;
        TBuf8<RMobileSmsMessaging::KGsmTpduSize> report;       // no submit report
        CMmDataPackage pkg;
        pkg.PackData( &ref, &report );
        Complete( EMobileSmsMessagingSendMessage, &pkg );
        RDebug::Printf( "[fakemodem] SMS submitted, ref %d (%d byte TPDU) -> looped back", (TInt)ref, iSmsSubmit.Length() );
        // loop it back as a delivery: the submit's DA becomes the OA, same PID/DCS/UDL/UD
        BuildDeliverFromSubmit();
        Post( EFmSmsDeliver, 20 );
        }
    void BuildDeliverFromSubmit()
        {
        iSms.iSmsMsg.Zero();
        if ( iSmsSubmit.Length() < 8 )
            return;
        TUint8 first = iSmsSubmit[0];
        TInt p = 2;                                             // skip first octet + MR
        TInt daLen = iSmsSubmit[p];                             // digits
        TInt daBytes = 2 + ( daLen + 1 ) / 2;                   // len + TON + digits
        if ( p + daBytes + 2 >= iSmsSubmit.Length() )
            return;
        iSms.iSmsMsg.Append( 0x04 );                            // SMS-DELIVER, no more messages
        iSms.iSmsMsg.Append( iSmsSubmit.Mid( p, daBytes ) );    // OA
        p += daBytes;
        iSms.iSmsMsg.Append( iSmsSubmit[p] );                   // PID
        iSms.iSmsMsg.Append( iSmsSubmit[p + 1] );               // DCS
        p += 2;
        TInt vpf = ( first >> 3 ) & 3;                          // validity period: none / 1 / 7 octets
        p += ( vpf == 2 ) ? 1 : ( vpf == 1 || vpf == 3 ) ? 7 : 0;
        static const TUint8 scts[7] = { 0x62, 0x01, 0x61, 0x21, 0x30, 0x00, 0x00 };    // 2026-10-16 12:03:00
        iSms.iSmsMsg.Append( scts, 7 );
        if ( p < iSmsSubmit.Length() )
            iSms.iSmsMsg.Append( iSmsSubmit.Mid( p ) );         // UDL + UD
        FillSmsCommon();
        }
    // an SMS made from text (plain 7-bit packing, ASCII only)
    void BuildDeliverFromText( const TDesC& aText )
        {
        iSms.iSmsMsg.Zero();
        iSms.iSmsMsg.Append( 0x04 );
        iSms.iSmsMsg.Append( 0x0C );                            // OA: 12 digits
        iSms.iSmsMsg.Append( 0x91 );                            // international
        static const TUint8 oa[6] = { 0x53, 0x48, 0x01, 0x21, 0x11, 0x21 };
        iSms.iSmsMsg.Append( oa, 6 );
        iSms.iSmsMsg.Append( 0x00 );                            // PID
        iSms.iSmsMsg.Append( 0x00 );                            // DCS: GSM 7-bit
        static const TUint8 scts[7] = { 0x62, 0x01, 0x61, 0x21, 0x30, 0x00, 0x00 };
        iSms.iSmsMsg.Append( scts, 7 );
        TInt n = aText.Length() > 140 ? 140 : aText.Length();
        iSms.iSmsMsg.Append( (TUint8)n );                       // UDL in septets
        TUint32 acc = 0;
        TInt bits = 0;
        for ( TInt i = 0; i < n; i++ )
            {
            acc |= ( (TUint32)( aText[i] & 0x7F ) ) << bits;
            bits += 7;
            while ( bits >= 8 )
                {
                iSms.iSmsMsg.Append( (TUint8)( acc & 0xFF ) );
                acc >>= 8;
                bits -= 8;
                }
            }
        if ( bits > 0 )
            iSms.iSmsMsg.Append( (TUint8)( acc & 0xFF ) );
        FillSmsCommon();
        }
    void FillSmsCommon()
        {
        iSms.iServiceCentre.Copy( KFmSmsc );
        iSms.iMobileScTON = RMobilePhone::EInternationalNumber;
        iSms.iMobileScNPI = RMobilePhone::EIsdnNumberPlan;
        iSms.iMessageStatus = 0;
        iSms.iLocation = 0;
        iSms.iDeleteAfterClientAck = ETrue;
        iSms.iSmsClass2 = EFalse;
        }
    // the SIM's EFsmsp: one parameter set holding the service centre. ncnlist (CMsgSimOperation -> ReadSimParamsL -> the SMS stack ->
    // RMobileSmsMessaging::GetSmspList) copies it into the SMS service settings; without one, sending asks for a service centre number
    // the card: a USIM whose PIN1 is the active PIN (CTSY: EMmTsySimGetICCType -> EMmTsySecurityGetSimActivePinStateIPC -> the lock-info query,
    // which the handler answers "unlocked, disabled")
    void DoIccType()
        {
        TInt icc = 1;                                           // CMmPhoneGsmWcdmaExt::EICCTypeSim3G
        CMmDataPackage pkg;
        pkg.PackData( &icc );
        Complete( EMmTsySimGetICCType, &pkg );
        RDebug::Printf( "[fakemodem] SIM: USIM card" );
        }
    void DoActivePin()
        {
        RMobilePhone::TMobilePhoneSecurityCode pin = RMobilePhone::ESecurityCodePin1;
        CMmDataPackage pkg;
        pkg.PackData( &pin );
        Complete( EMmTsySecurityGetSimActivePinStateIPC, &pkg );
        RDebug::Printf( "[fakemodem] SIM: active PIN = PIN1 (not required)" );
        }
    // ---- the SIM's phonebooks. Init answers the store sizes; the ADN cache is three contacts (the Contacts SIM store reads them
    // from CTSY's cache); every other store is empty.
    void DoPbInit( TInt aSlot )
        {
        CStorageInfoData info;
        info.iADNNumOfEntries = 100;
        info.iADNTextLengthMax = 20;
        info.iADNNumberLengthMax = 20;
        info.iFDNNumOfEntries = 10;
        info.iFDNTextLengthMax = 20;
        info.iFDNNumberLengthMax = 20;
        info.iSDNNumOfEntries = 5;
        info.iSDNTextLengthMax = 20;
        info.iSDNNumberLengthMax = 20;
        info.iVMBXNumOfEntries = 1;
        info.iVMBXTextLengthMax = 20;
        info.iVMBXNumberLengthMax = 20;
        info.iMSISDNNumOfEntries = 1;
        info.iMSISDNTextLengthMax = 20;
        info.iMSISDNNumberLengthMax = 20;
        info.iMBDNNumOfEntries = 1;
        info.iMBDNTextLengthMax = 20;
        info.iMBDNNumberLengthMax = 20;
        CPhoneBookDataPackage pkg;
        pkg.SetPhoneBookName( iPbName[aSlot] );
        pkg.PackData( &info );
        Complete( EMmTsyPhoneBookStoreInitIPC, &pkg );
        RDebug::Print( _L("[fakemodem] SIM phonebook %S initialised"), &iPbName[aSlot] );
        }
    static void AddPbEntry( CArrayPtrSeg<CPhoneBookStoreEntry>* aList, TInt aLoc, const TDesC& aText, const TDesC& aNumber )
        {
        CPhoneBookStoreEntry* e = new( ELeave ) CPhoneBookStoreEntry();
        CleanupStack::PushL( e );
        e->ConstructL();
        e->iLocation = (TUint16)aLoc;
        e->iText = aText.AllocL();
        e->iNumber = aNumber.AllocL();
        aList->AppendL( e );
        CleanupStack::Pop( e );
        }
    void DoPbCache( TInt aSlot )
        {
        CArrayPtrSeg<CPhoneBookStoreEntry>* list = new CArrayPtrSeg<CPhoneBookStoreEntry>( 4 );
        if ( !list )
            return;
        TBool adn = !iPbName[aSlot].CompareF( KETelIccAdnPhoneBook );
        if ( adn )
            {
            TRAPD( e1, AddPbEntry( list, 1, _L("Phorepoc Support"), _L("+358401112233") ) );
            RDebug::Printf( "[fakemodem] pb entry 1 leave %d, count %d", e1, list->Count() );
            TRAP_IGNORE( AddPbEntry( list, 2, _L("Alice"), _L("+358407654321") ) );
            TRAP_IGNORE( AddPbEntry( list, 3, _L("Bob"), _L("0401234567") ) );
            }
        CPhoneBookDataPackage pkg;
        pkg.SetPhoneBookName( iPbName[aSlot] );
        pkg.PackData( &list );
        Complete( EMmTsyPhoneBookStoreCacheIPC, &pkg, adn ? KErrNone : KErrNotFound );
        list->ResetAndDestroy();
        delete list;
        RDebug::Print( _L("[fakemodem] SIM phonebook %S cached (%d entries)"), &iPbName[aSlot], adn ? 3 : 0 );
        }
    void DoPbInfo( TInt aSlot )
        {
        TInt used = 0;
        CPhoneBookDataPackage pkg;
        pkg.SetPhoneBookName( iPbName[aSlot] );
        pkg.PackData( &used );
        Complete( EMmTsyPhoneBookStoreGetInfoIPC, &pkg );
        }
    void DoSmspList()
        {
        CArrayPtrFlat<TSmsParameters>* list = new CArrayPtrFlat<TSmsParameters>( 2 );
        if ( !list )
            return;
        TSmsParameters* p = new TSmsParameters;
        if ( p )
            {
            p->iLocationNumber = 1;
            p->iParameterIndicator = KServiceCentreAddress | KProtocolID | KDataCodingScheme | KValidityPeriod;
            p->iProtocolId = 0;
            p->iDataCodingScheme = 0;
            p->iValidityPeriod = 167;                          // 1 day (relative)
            p->iServiceCenterAddress.Copy( KFmSmsc );
#ifdef PHE_LTSY
            if ( iAt && iAt->Smsc().Length() )      // pheltsy: AT+CSCA?
                p->iServiceCenterAddress.Copy( iAt->Smsc() );
#endif
            p->iMobileScTON = RMobilePhone::EInternationalNumber;
            p->iMobileScNPI = RMobilePhone::EIsdnNumberPlan;
            p->iMobileDeTON = RMobilePhone::EUnknownNumber;
            p->iMobileDeNPI = RMobilePhone::EUnknownNumberingPlan;
            p->iAlphaTagPresent = ETrue;
            p->iAlphaTagData.Copy( _L("Phorepoc SMSC") );
            TRAPD( err, list->AppendL( p ) );
            if ( err != KErrNone )
                delete p;
            }
        CMmDataPackage pkg;
        pkg.PackData( list );                                  // UnPackData( &sets ) takes the pointer itself
        Complete( EMobileSmsMessagingGetSmspListPhase1, &pkg );   // CTSY moves the sets into its own list (and deletes them)
        list->ResetAndDestroy();
        delete list;
        RDebug::Print( _L("[fakemodem] SIM SMS parameters: service centre %S"), &KFmSmsc );
        }
    void DoSmsDeliver()
        {
        if ( iSms.iSmsMsg.Length() == 0 )
            return;
        TBool ind = EFalse;
        TSmsMsg* sms = &iSms;
        CMmDataPackage pkg;
        pkg.PackData( &ind, &sms );
        Complete( EMobileSmsMessagingReceiveMessage, &pkg );
        RDebug::Printf( "[fakemodem] SMS delivered (%d byte TPDU)", iSms.iSmsMsg.Length() );
        }
    // external stimuli: P&S keys (defined by phetelprobe; absent = nothing to do)
    void PollStimuli()
        {
        TBuf<32> num;
        if ( RProperty::Get( KFmPropCat, KFmPropRing, num ) == KErrNone )
            {
            if ( num.Length() > 0 && !iRingSeen && iBooted && iCallStatus == RMobileCall::EStatusIdle )
                {
                iRingSeen = ETrue;
                DoRing( num );
                }
            else if ( num.Length() == 0 )
                iRingSeen = EFalse;
            }
        TBuf<140> txt;
        if ( RProperty::Get( KFmPropCat, KFmPropSms, txt ) == KErrNone )
            {
            if ( txt.Length() > 0 && !iSmsSeen && iBooted )
                {
                iSmsSeen = ETrue;
                BuildDeliverFromText( txt );
                DoSmsDeliver();
                }
            else if ( txt.Length() == 0 )
                iSmsSeen = EFalse;
            }
        }
    void DoTick()
        {
        if ( !Mgr() )
            return;
#ifdef PHE_LTSY
        if ( iAt )
            iAt->Tick();
#endif
        if ( ++iPoll >= 10 )
            {
            iPoll = 0;
            PollStimuli();
            }
        for ( TInt i = 0; i < KQueue; i++ )
            {
            if ( iQueue[i].iEvent == EFmNone )
                continue;
            if ( --iQueue[i].iTicks > 0 )
                continue;
            TFmEvent ev = iQueue[i].iEvent;
            TInt arg = iQueue[i].iArg;
            TInt arg2 = iQueue[i].iArg2;
            iQueue[i].iEvent = EFmNone;
            switch ( ev )
                {
                case EFmBoot: DoBoot(); break;
                case EFmReg: DoReg(); break;
                case EFmNetwork: DoNetwork(); break;
                case EFmIdent: DoIdent( arg ); break;
                case EFmOperatorName: DoOperatorName(); break;
                case EFmProviderName: DoProviderName(); break;
                case EFmDial: DoDial(); break;
                case EFmDialProgress: DoDialProgress( arg ); break;
                case EFmHangUp: DoHangUp(); break;
                case EFmAnswer: DoAnswer(); break;
                case EFmSmsSent: DoSmsSent(); break;
                case EFmSmsDeliver: DoSmsDeliver(); break;
                case EFmSmspList: DoSmspList(); break;
                case EFmIccType: DoIccType(); break;
                case EFmPbInit: DoPbInit( arg ); break;
                case EFmPbCache: DoPbCache( arg ); break;
                case EFmPbInfo: DoPbInfo( arg ); break;
                case EFmActivePin: DoActivePin(); break;
#ifdef PHE_LTSY
                case EFmSim: DoSim(); break;
#endif
                case EFmComplete: Complete( arg, arg2 ); break;
                default: break;
                }
            }
        }

    CMmMessageRouter* iRouter;
    TFmQueued iQueue[KQueue];
    TBuf<RMobilePhone::KMaxMobileTelNumberSize> iNumber;
    TInt iCallId;
    RMobileCall::TMobileCallStatus iCallStatus;
    TInt16 iMsgRef;
    TBool iBooted;
    TBool iRingSeen;
    TBool iSmsSeen;
    TInt iPoll;
    TBuf8<RMobileSmsMessaging::KGsmTpduSize> iSmsSubmit;
    TSmsMsg iSms;
    };

// ======== MEMBER FUNCTIONS ========

CMmMockMessHandler::CMmMockMessHandler()
    {
    }

void CMmMockMessHandler::ConstructL( CMmMessageRouter* aMessageRouter )
    {
    iMessageRouter = aMessageRouter;

    if ( !iTimer )
        {
        // Phorepoc T1: the "timer" is the fake modem (a CPeriodic)
        CFakeModem* modem = CFakeModem::NewL( aMessageRouter );
        iTimer = modem;
        modem->BootSoon();
 OstTraceDef0(OST_TRACE_CATEGORY_DEBUG, TRACE_INTERNALS, CMMMOCKMESSHANDLER_CONSTRUCTL_1, "TSY: CMmMockMessHandler::ConstructL: timer created" );
        }
    }

CMmMockMessHandler* CMmMockMessHandler::NewL(
    CMmMessageRouter* aMessageRouter )
    {
    CMmMockMessHandler* self = new( ELeave ) CMmMockMessHandler();
    CleanupStack::PushL( self );
    self->ConstructL( aMessageRouter );
    CleanupStack::Pop();
    return self;
    }

CMmMockMessHandler::~CMmMockMessHandler()
    {
    delete iTimer;
    iTimer = NULL;
    }

// ---------------------------------------------------------------------------
// CMmMockMessHandler::ExtFuncL
// Handles stub requests
// (other items were commented in a header)
// ---------------------------------------------------------------------------
//
TInt CMmMockMessHandler::ExtFuncL(
    TInt aIpc,
    const CMmDataPackage* aDataPackage)
    {
OstTraceDef1(OST_TRACE_CATEGORY_DEBUG, TRACE_INTERNALS, CMMMOCKMESSHANDLER_EXTFUNCL_1, "TSY: CMmMockMessHandler::ExtFuncL. IPC: %d", aIpc );
    RDebug::Printf( "[fakemodem] request ipc %d", aIpc );

    // callback indicators
    TInt ret( KErrNotSupported );
    TBool complete( EFalse );
    CFakeModem* modem = static_cast<CFakeModem*>( iTimer );
#ifdef PHE_LTSY
    // pheltsy: the PIN / PUK the S60 query collected goes to the SIM as AT+CPIN; answered from PheCodeResult
    if ( aIpc == EMobilePhoneVerifySecurityCode && modem && modem->iAt && aDataPackage )
        {
        RMobilePhone::TMobilePhoneSecurityCode* type = NULL;
        RMobilePhone::TCodeAndUnblockCode* codes = NULL;
        aDataPackage->UnPackData( &type, &codes );
        if ( type && codes && ( *type == RMobilePhone::ESecurityCodePin1 || *type == RMobilePhone::ESecurityCodePuk1 ) )
            {
            const TBool puk = *type == RMobilePhone::ESecurityCodePuk1;
            // ETel: for a PUK, iCode = the new PIN and iUnblockCode = the PUK
            return modem->iAt->EnterCode( codes->iCode, puk ? TPtrC( codes->iUnblockCode ) : TPtrC() );
            }
        return KErrNotSupported;
        }
#endif

    // datapackage for complete
    CMmDataPackage data;
    CMmDataPackage* package;
    package = NULL;
    TBuf8<KTmpPckgSize> temp;
    TUint8 routingActivated = 0x0;
    TInt mode( 0 );
    // Phorepoc board delta (X4.72): the device-lock answer of a phone with no lock code
    RMobilePhone::TMobilePhoneLockStatus lockStatus( RMobilePhone::EStatusUnlocked );
    RMobilePhone::TMobilePhoneLockSetting lockSetting( RMobilePhone::ELockSetDisabled );

    switch ( aIpc )
        {
        // return KErrNone without complete: NOTIFICATIONS only (they complete when something happens -- with no modem, never)
        // and their cancels
        case EMobileConferenceCallNotifyConferenceEvent:
        case EMobileConferenceCallNotifyCapsChangeCancel:
        case EMobileConferenceCallNotifyConferenceStatusChangeCancel:
        case EMobileConferenceCallNotifyConferenceEventCancel:
        case EMobileConferenceCallNotifyCapsChange:
        case EMmTsyONStoreNotifyStoreEventIPC:
        case EMobileBroadcastMessagingReceiveMessage:
        case EPacketNotifyDynamicCapsChange:
        case EPacketNotifyContextAdded:
        case EPacketNotifyStatusChange:
            ret = KErrNone;
            break;

        // Phorepoc hardening (2026-10-12, docs/phorepoc/telephony-stub.md): ONE-SHOT requests the mock used to accept and never
        // complete (its "return KErrNone without complete" group) -- every caller waited forever (the phonebook / SIM store,
        // call settings, SMS store). A phone with no modem and no SIM answers them at once: not supported. CTSY completes the
        // client with the error returned from ExtFuncL, so no data package is needed.
        case EMobilePhoneGetWaitingStatusPhase1:
        case EMobilePhoneGetALSLine:
        case EMobilePhoneGetCallForwardingStatusPhase1:
        case EMobilePhoneSetLockSetting:
        case EMobilePhoneGetBarringStatusPhase1:
        case EMmTsyPhoneBookStoreWriteIPC:
        case EMmTsyONStoreDeleteIPC:
        case EMmTsyONStoreReadIPC:
        case EMobilePhoneStoreGetInfo:
        case EMobileSmsMessagingAckSmsStored:
        case EMobileSmsMessagingNackSmsStored:
        case EMobilePhoneStoreDelete:
        case EMobilePhoneStoreDeleteAll:
        case EMobilePhoneStoreRead:
        case EMobilePhoneStoreWrite:
        case EMobileUssdMessagingSendMessage:
        case EMobileSmsMessagingGetMessageStoreInfo:
        case EMobilePhoneStoreReadAllPhase1:
        case EPacketContextAddPacketFilter:
            ret = KErrNotSupported;
            break;

        // the call life timer update (CTSY sends it while a call runs and at hang-up; fire-and-forget): KErrNotSupported
        // here made the Phone UI show "Not allowed" after End call (boot 883)
        case EMmTsyUpdateLifeTimeIPC:
            ret = KErrNone;
            break;

        // return KErrNone and complete
        case EMobilePhoneSetALSLine:
        case EMobilePhoneSelectNetwork:
        case EMmTsyBootNotifySimStatusReadyIPC:
        case EMobilePhoneSetCallWaitingStatus:
        case EMobilePhoneStopDTMFTone:
        case EMobilePhoneSetNetworkSelectionSetting:
        case EMobilePhoneSetCallForwardingStatus:
        case EMobilePhoneSetCallBarringStatus:
        case EMobilePhoneSendNetworkServiceRequest:
        case EMobilePhoneChangeSecurityCode:
        case EMobilePhoneAbortSecurityCode:
        case EMobilePhoneSetSSPassword:
        case EMobilePhoneSelectNetworkCancel:
        case EMobilePhoneGetDetectedNetworksCancel:
        case EMobileCallDialEmergencyCall:
        case EMobileCallSwap:
        case EMobileCallHold:
        case EMobileCallResume:
        case EMobileCallGoOneToOne:
        case EMobileCallTransfer:
        case EMobileCallSetDynamicHscsdParams:
        case EMobileConferenceCallAddCall:
        case EMobileConferenceCallCreateConference:
        case EMobileConferenceCallSwap:
        case EMobileConferenceCallHangUp:
        case EMobileConferenceCallNotifyConferenceStatusChange:
        case EMmTsyPhoneBookStoreDeleteIPC:
        case EMmTsyPhoneBookStoreDeleteAllIPC:
        case EMmTsyONStoreWriteIPC:
        case EMmTsyONStoreStoreAllIPC:
        case EMmTsyONStoreDeleteAllIPC:
        case EMobileSmsMessagingStoreSmspList:
        case EMobileBroadcastMessagingReceiveMessageCancel:
        case EMobileSmsMessagingSetMoSmsBearer:
        case ECustomTerminateCallIPC:
        case ECustomNetWakeupIPC:
        case ECustomSetAlsBlockedIPC:
        case ECustomClearCallBlackListIPC:
        case ECustomResetNetServerIPC:
        case ECustomSimLockActivateIPC:
        case ECustomSimLockDeActivateIPC:
        case ECustomDeleteSimCbTopicIPC:
        case ECustomCheckSecurityCodeIPC:
        case ECustomCheckSecurityCodeCancelIPC:
        case ECustomDisablePhoneLockIPC:
        case EPacketSetAttachMode:
        case EPacketSetDefaultContextParams:
            complete = ETrue;
            ret = KErrNone;
            break;

        // ---- Phorepoc T1: calls. The request is accepted here (KErrNone, no completion) and the fake modem answers
        // through its event queue: call info + Dialling + Connecting + Connected, then the Dial completion.
        case EEtelCallDial:
        case EMobileCallDialNoFdnCheck:
        case EMobileCallDialISV:
            {
            const CCallDataPackage* call = static_cast<const CCallDataPackage*>( aDataPackage );
            TInt callId( -1 );
            RMobilePhone::TMobileService callMode( RMobilePhone::EVoiceService );
            call->GetCallIdAndMode( callId, callMode );
            RMobileCall::TMobileCallInfoV1* info = NULL;
            TAny* params = NULL;
            // packed: ( call params, mobile call info ); the info's dialled party carries the number
            aDataPackage->UnPackData( &params, &info );
            if ( info )
                modem->StartDial( info->iDialledParty.iTelNumber );
            else
                modem->StartDial( _L("0") );
            ret = KErrNone;
            }
            break;
        case EEtelCallHangUp:
            modem->StartHangUp();
            ret = KErrNone;
            break;
        case EEtelCallAnswer:
        case EMobileCallAnswerISV:
            modem->StartAnswer();
            ret = KErrNone;
            break;

        // ---- T1: SMS. SendMessage is packed as a TSendSmsDataAndAttributes; the fake modem completes it and loops
        // the message back as an incoming one.
        case EMobileSmsMessagingSendMessage:
        case EMobileSmsMessagingSendMessageNoFdnCheck:
            {
            TSendSmsDataAndAttributes* send = NULL;
            aDataPackage->UnPackData( &send );
            modem->SmsSubmitted( send ? send->iMsgData : NULL );
            ret = KErrNone;
            }
            break;
        case EMobileSmsMessagingResumeSmsReception:
            complete = ETrue;
            ret = KErrNone;
            break;
        case EMmTsySimRefreshRegisterIPC:
            // registered for SIM refresh: without it a phonebook Read distrusts its cache and re-caches (and fails)
            complete = ETrue;
            ret = KErrNone;
            break;
        case EMmTsyPhoneBookStoreInitIPC:
            modem->PostPb( EFmPbInit, aDataPackage );
            ret = KErrNone;
            break;
        case EMmTsyPhoneBookStoreCacheIPC:
            modem->PostPb( EFmPbCache, aDataPackage );
            ret = KErrNone;
            break;
        case EMmTsyPhoneBookStoreGetInfoIPC:
            modem->PostPb( EFmPbInfo, aDataPackage );
            ret = KErrNone;
            break;
        case EMmTsySimGetICCType:
            modem->Post( EFmIccType, 1 );
            ret = KErrNone;
            break;
        case EMmTsySecurityGetSimActivePinStateIPC:
            modem->Post( EFmActivePin, 1 );
            ret = KErrNone;
            break;
        case EMobileSmsMessagingGetSmspListPhase1:
            modem->Post( EFmSmspList, 1 );                   // answered from the queue (CTSY records the request handle after ExtFuncL)
            ret = KErrNone;
            break;

        // return KErrNotSupported and complete
        case EMobilePhoneSendDTMFTones:
        case EMobilePhoneSendDTMFTonesCancel:
        case EMobilePhoneGetIdentityServiceStatus:
        case ECustomStartSimCbTopicBrowsingIPC:
            ret = KErrNotSupported;
            complete = ETrue;
            break;

        // return KErrNotSupported
        case EMobilePhoneGetPhoneStoreInfo:
        case EMobilePhoneGetDetectedNetworksV2Phase1:
        case EMobilePhoneGetCipheringIndicatorStatus:
        case EMobilePhoneContinueDTMFStringSending:
        case EMobilePhoneStartDTMFTone:
        case EMobilePhoneGetServiceTable:
        case EMobilePhoneVerifySecurityCode:
        case EMobileCallActivateCCBS:
        case EMobilePhoneAcceptCCBSRecall:
        case EMobilePhoneClearBlacklist:
        case EMobileCallDeflect:
        case EMobileConferenceCallCreateConferenceCancel:
        case EMobileConferenceCallAddCallCancel:
        case EMobileConferenceCallSwapCancel:
        case EMobileConferenceCallHangUpCancel:
        case EMobileConferenceCallGetCaps:
        case EMobileConferenceCallEnumerateCalls:
        case EMmTsyENStoreReadAllPhase1IPC:
        case EMmTsyENStoreReadIPC:
        case EMmTsyENStoreGetInfoIPC:
        case EMmTsyONStoreGetInfoIPC:
        case EMmTsySmsSendSatMessage:
        case ECustomCheckAlsPpSupportIPC:
        case ECustomGetCipheringInfoIPC:
        case ECustomGetAlsBlockedIPC:
        case ECustomCheckEmergencyNumberIPC:
        case ECustomCheckTwoDigitDialSupportIPC:
        case ECustomReadSimFileIPC:
        case ECustomGetLifeTimeIPC:
        case ECustomSendAPDUReqIPC:
        case ECustomIsBlockedIPC:
        case EPacketAttach:
        case EPacketDetach:
        case EPacketGetNtwkRegStatus:
        case EPacketSetPrefBearer:
        case EPacketRejectActivationRequest:
        case EPacketContextGetDataVolumeTransferred:
        case EPacketContextInitialiseContext:
        case EPacketContextDelete:
        case EPacketContextSetConfig:
        case EPacketContextActivate:
        case EPacketContextDeactivate:
        case EPacketContextModifyActiveContext:
        case EPacketQoSSetProfileParams:
            ret = KErrNotSupported;
            break;

        // ---- T1: network, identity (answered by the fake modem a few ticks later)
        case EMobilePhoneGetNetworkRegistrationStatus:
            modem->PostReg();
            ret = KErrNone;
            break;
        case ECustomGetOperatorNameIPC:
            modem->PostOperatorName();
            ret = KErrNone;
            break;
        case ECustomGetNetworkProviderNameIPC:
            modem->PostProviderName();
            ret = KErrNone;
            break;

        // return KErrNotReady
        case EMobileConferenceCallGetMobileCallInfo:
            ret = KErrNotReady;
            break;

        // return KErrNotFound
        case EMmTsyPhoneBookStoreReadIPC:
            ret = KErrNotFound;
            break;


        case EPacketInitProxiesIPC:
            GetHomeNetworkInfo();
            ret = KErrNone;
            break;
        case EPacketGetAttachMode:
            complete  = ETrue;
            mode = 1;          // 1 = When needed
            data.PackData( &mode );
            package = &data;
            ret = KErrNone;
            break;
        case EMobilePhoneGetLockInfo:
            // Phorepoc board delta (X4.72): the mock returned KErrNone WITHOUT completing, so any caller waited
            // forever -- SSM's DeviceSecurityCheck (UI SWP list) timed out and the boot shut down. Answer as a
            // modem does for a phone without a device lock: unlocked, lock setting disabled.
            complete = ETrue;
            data.PackData( &lockStatus, &lockSetting );
            package = &data;
            ret = KErrNone;
            break;
        case EMobilePhoneGetPhoneId:
            modem->PostIdent( EFalse );
            ret = KErrNone;
            break;
        case EMobilePhoneGetSubscriberId:
            modem->PostIdent( ETrue );
            ret = KErrNone;
            break;
        case EMobilePhoneGetHomeNetwork:
            complete = ETrue;
            data.PackData( &iMyNetworkInfo );
            package = &data;
            ret = KErrNone;
            break;
        case EMmTsyPhoneGetPin1DisableSupportedIPC:
            complete = ETrue;
            data.PackData ( &complete ); //Needed value for packet is ETrue
            package = &data;
            ret = KErrNone;
            break;
        case EMmTsyActivateSmsRouting:
            routingActivated = 0x1;    //routing activated
            data.PackData( &routingActivated );
            package = &data;
            ret = KErrNone;
            complete = ETrue;
            break;

        default:
            ret = KErrNotSupported;
            break;
        }

    if( complete )
        {
        if( NULL != package )
            {
            iMessageRouter->MessageManager()->Complete( aIpc, package, ret );
            }
         else
             {
             // CTSY records a request's handle only after ExtFuncL returns: an immediate Complete() finds nobody waiting and
             // the request hangs for good (the SMS stack's SetMoSmsBearer did). Answer on the fake modem's next tick.
             modem->PostComplete( aIpc, ret );
             }

        return KErrNone;
        }

    return ret;
    }

// ---------------------------------------------------------------------------
// CMmMockMessHandler::GetHomeNetwork
// Creates and sends default values of Home network
// (other items were commented in a header)
// ---------------------------------------------------------------------------
//
void CMmMockMessHandler::GetHomeNetwork()
    {
OstTraceDef0(OST_TRACE_CATEGORY_DEBUG, TRACE_INTERNALS, CMMMOCKMESSHANDLER_GETHOMENETWORK_1, "TSY: CMmMockMessHandler::GetHomeNetwork.");

    iMyNetworkInfo.iMode = RMobilePhone::ENetworkModeWcdma,
    iMyNetworkInfo.iStatus = RMobilePhone::ENetworkStatusCurrent,
    iMyNetworkInfo.iBandInfo = RMobilePhone::EBandUnknown;
    iMyNetworkInfo.iCountryCode = KFmMcc;
    _LIT(KCdmaSID, "");
    iMyNetworkInfo.iCdmaSID = KCdmaSID;
    _LIT(KAnalogSID, "");
    iMyNetworkInfo.iAnalogSID = KAnalogSID;
    iMyNetworkInfo.iNetworkId = KFmMnc;
    iMyNetworkInfo.iDisplayTag = KFmOperatorLong;
    iMyNetworkInfo.iShortName = KFmOperatorShort;
    iMyNetworkInfo.iLongName = KFmOperatorLong;
    }

// ---------------------------------------------------------------------------
// CMmMockMessHandler::GetHomeNetworkInfo
// Requests for GetHomeNetwork
// (other items were commented in a header)
// ---------------------------------------------------------------------------
//
void CMmMockMessHandler::GetHomeNetworkInfo()
    {
OstTraceDef0(OST_TRACE_CATEGORY_DEBUG, TRACE_INTERNALS, CMMMOCKMESSHANDLER_GETHOMENETWORKINFO_1, "TSY: CMmMockMessHandler::GetHomeNetworkInfo.");
    // GetHomeNetwork() fills iMyNetworkInfo; the answer goes back to CTSY (it stores it in its extension)
    GetHomeNetwork();
    TRAP_IGNORE( ExtFuncL( EMobilePhoneGetHomeNetwork, NULL ); );
    }

// ---------------------------------------------------------------------------
// CMmMockMessHandler::TimerCallback
// Unused since T1: the timer is the fake modem's ticker.
// ---------------------------------------------------------------------------
//
TInt CMmMockMessHandler::TimerCallback( TAny* /*aThis*/ )
    {
    return EFalse;
    }

//  End of File
