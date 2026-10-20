/*
* Copyright (c) 2002 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the License "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
*     This class provides connection/session related information.
*	  Both the requested and the stored AP's are here.
*	
*
*/


#include <browser_platform_variant.hrh>
#include "ConnManActiveConnector.h"
#include "ConnectionManagerLogger.h"
#include <mconnection.h>
#include <nifvar.h>
#include <CommDbConnPref.h>

#ifdef BRDO_OCC_ENABLED_FF
#include <extendedconnpref.h>
#include <e32debug.h>

// X4.68 DIAGNOSTIC (toolchain/shims/connman_diag, opt-in registry swap): is the connection start completed
// while the nested wait never wakes (a lost request signal)? Every 5 s print the connector's iStatus and the
// thread's pending signal count as the kernel sees it.
// (no writable static data in a DLL: the callback gets the connector, a CActive subclass reads its iStatus)
class CDiagPeek : public CActive
	{
public:
	static TInt StatusOf(CActive* aActive) { return static_cast<CDiagPeek*>(aActive)->iStatus.Int(); }
	static TBool ActiveOf(CActive* aActive) { return aActive->IsActive(); }
	};
static TInt DiagTick(TAny* aConnector)
	{
	CActive* a = static_cast<CActive*>(aConnector);
	RDebug::Printf("[conndiag] connector iStatus %d, IsActive %d (KRequestPending = %d)", CDiagPeek::StatusOf(a), CDiagPeek::ActiveOf(a), KRequestPending);
	return ETrue;
	}
#endif

//--------------------------------------------------------------------------
//CConnManActiveConnector::CConnManActiveConnector()
//--------------------------------------------------------------------------
CConnManActiveConnector::CConnManActiveConnector( RConnection& aConnection, 
                                                  TInt aPriority )
    : CActive( aPriority )
    , iConnection( aConnection )
	{
    
	CLOG_CREATE;
	CActiveScheduler::Add( this );//inserting this into the queue
	occPrefs = EDefault;
	}

//--------------------------------------------------------------------------
//CConnManActiveConnector::~CConnManActiveConnector()
//--------------------------------------------------------------------------
CConnManActiveConnector::~CConnManActiveConnector()
	{
    
	Cancel();//The standard way of destroying an Active object
	CLOG_CLOSE;
	}

//--------------------------------------------------------------------------
//CConnManActiveConnector::StartConnection()
//--------------------------------------------------------------------------
void CConnManActiveConnector::StartConnection( TCommDbConnPref* aSettings, TRequestStatus& aStatus)
{
    CLOG_WRITE( "CConnManActiveConnector:StartConnection AlwaysAsk/Ap is called");
    iExternalRequestStatus = &aStatus;

#ifdef BRDO_OCC_ENABLED_FF

    TExtendedConnPref extPref;
    CLOG_WRITE( "CConnManActiveConnector:StartConnection Setting OCC parameters");

#ifdef __WINS__ //This is only for emulator testing purpose

    //For emulator
    CLOG_WRITE( "CConnManActiveConnector:StartConnection Emulator OCC settings " );
    extPref.SetSnapPurpose(CMManager::ESnapPurposeUnknown);
    extPref.SetConnSelectionDialog(ETrue);
    extPref.SetForcedRoaming(EFalse);

#else

    //For hardware
    TUint32 bookmarkIap = aSettings->IapId();
    CLOG_WRITE_1( "CConnManActiveConnector:StartConnection Active Bookmark Iap: %d", bookmarkIap );
    if (bookmarkIap)
    {
       CLOG_WRITE( "CConnManActiveConnector:StartConnection Iap is found for this bookmark");
       extPref.SetSnapPurpose(CMManager::ESnapPurposeUnknown);
       extPref.SetIapId(bookmarkIap);
    }
    else
    {
       CLOG_WRITE( "CConnManActiveConnector:StartConnection Popup ap dialog");
       extPref.SetSnapPurpose(CMManager::ESnapPurposeUnknown);
       extPref.SetConnSelectionDialog(ETrue);
    }

#endif //__WINS__   

    extPref.SetNoteBehaviour(TExtendedConnPref::ENoteBehaviourDefault);
    TConnPrefList prefList;
    TRAP_IGNORE(prefList.AppendL(&extPref));
#endif //BRDO_OCC_ENABLED_FF

    if( aSettings )
    {
      #ifdef BRDO_OCC_ENABLED_FF
        iConnection.Start( prefList, iStatus );
      #else
        iConnection.Start( *aSettings, iStatus );
      #endif
    }
    else
        {
        iConnection.Start( iStatus );
        }

    SetActive();

	*iExternalRequestStatus = KRequestPending;
	}
	
//--------------------------------------------------------------------------
//CConnManActiveConnector::StartConnection()
//--------------------------------------------------------------------------
void CConnManActiveConnector::StartConnection( TConnSnapPref* aSettings, TRequestStatus& aStatus)
{
    CLOG_WRITE( "CConnManActiveConnector:StartConnection SNAP is called");
    iExternalRequestStatus = &aStatus;

#ifdef BRDO_OCC_ENABLED_FF

    CLOG_WRITE( "CConnManActiveConnector:StartConnection Setting OCC parameters");
    TExtendedConnPref extPref;

#ifdef __WINS__ 

    //For emulator
    CLOG_WRITE( "CConnManActiveConnector:StartConnection Emulator OCC settings " );
    extPref.SetSnapPurpose(CMManager::ESnapPurposeUnknown);
    extPref.SetConnSelectionDialog(ETrue);
    extPref.SetForcedRoaming(EFalse);

#else
    
    //For hardware
    if( aSettings->Snap() == 0 )
        {
        CLOG_WRITE( "CConnManActiveConnector:StartConnection Using Internet Snap");
        extPref.SetSnapPurpose(CMManager::ESnapPurposeInternet);
        }
    else
        {
        CLOG_WRITE( "CConnManActiveConnector:StartConnection Using given Snap");
        extPref.SetSnapId(aSettings->Snap());
        }

#endif //__WINS__

    extPref.SetNoteBehaviour(TExtendedConnPref::ENoteBehaviourDefault);
    if ( occPrefs == ESilient )
        {
        CLOG_WRITE( "CConnManActiveConnector:StartConnection Setting OCC Silent behaviour");
        extPref.SetNoteBehaviour(TExtendedConnPref::ENoteBehaviourConnSilent);
        }
    else
        {
        CLOG_WRITE( "CConnManActiveConnector:StartConnection Setting OCC Default behaviour");
        }
    TConnPrefList prefList;
    TRAP_IGNORE(prefList.AppendL(&extPref));

#endif

    if( aSettings )
    {
      #ifdef BRDO_OCC_ENABLED_FF
        iConnection.Start( prefList, iStatus );
      #else
        iConnection.Start( *aSettings, iStatus );
      #endif
    }
    else
        {
        iConnection.Start( iStatus );
        }

    SetActive();

	*iExternalRequestStatus = KRequestPending;
	}	
	
//--------------------------------------------------------------------------
//CConnManActiveConnector::DoCancel()
//--------------------------------------------------------------------------
void CConnManActiveConnector::DoCancel()
	{
	CLOG_WRITE( "CConnManActiveConnector: DoCancel called");
    //This is work around fix for bug ESNA-855BUN
    iConnection.Close();
	User::RequestComplete( iExternalRequestStatus, KErrCancel );//completing user req
    
	CLOG_WRITE( "CConnManActiveConnector: DoCancel returned");
	}

//--------------------------------------------------------------------------
//CConnManActiveConnector::RunL()
//--------------------------------------------------------------------------
void CConnManActiveConnector::RunL()
	{
    
    CLOG_WRITE_1( "CConnManAct::RunL(): %d", iStatus.Int() );
	User::RequestComplete( iExternalRequestStatus, iStatus.Int() );
	}

void CConnManActiveConnector::SetOccPreferences(TSetOCCPreferences aOCCPreferences)
    {
    CLOG_WRITE_1( "CConnManActiveConnector::SetOccPreferences : %d", aOCCPreferences );
    occPrefs = aOCCPreferences;
    }

//------------------------------------------------------ CActiveConnectorSyncWrapper -------------------------

//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::NewL()
//--------------------------------------------------------------------------
CActiveConnectorSyncWrapper* 
        CActiveConnectorSyncWrapper::NewL( RConnection& aConnection
                                           , TInt aPriority )
	{
	CActiveConnectorSyncWrapper * self = CActiveConnectorSyncWrapper::NewLC( aConnection, aPriority );
	CleanupStack::Pop();
	return self;
	}


//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::NewLC()
//--------------------------------------------------------------------------	
CActiveConnectorSyncWrapper * CActiveConnectorSyncWrapper::NewLC( RConnection& aConnection, TInt aPriority)
	{
    CActiveConnectorSyncWrapper * self = new ( ELeave )CActiveConnectorSyncWrapper( aPriority );
    CleanupStack::PushL( self );
    self->ConstructL( aConnection );
    return self;
	}


//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::Connect()
//--------------------------------------------------------------------------		
TInt CActiveConnectorSyncWrapper::Connect( TCommDbConnPref* aSettings )
	{
	iActiveConnector->StartConnection( aSettings, iStatus );
	SetActive();
	iWait.Start();
	return iStatus.Int();
	}


//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::ConnectSnap()
//--------------------------------------------------------------------------

TInt CActiveConnectorSyncWrapper::ConnectSnap( TConnSnapPref* aSettings )
	{
	iActiveConnector->StartConnection( aSettings, iStatus );
	SetActive();
	RDebug::Printf("[conndiag] ConnectSnap: nested wait starts");
	CPeriodic* diag = CPeriodic::New(CActive::EPriorityHigh);
	if (diag)
		diag->Start(5000000, 5000000, TCallBack(DiagTick, iActiveConnector));
	iWait.Start();
	delete diag;
	RDebug::Printf("[conndiag] ConnectSnap: nested wait ended, %d", iStatus.Int());
	return iStatus.Int();
	}

//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::~CActiveConnectorSyncWrapper()
//--------------------------------------------------------------------------
CActiveConnectorSyncWrapper::~CActiveConnectorSyncWrapper()
	{
	Cancel();
	delete iActiveConnector;
	}

//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::RunL()
//--------------------------------------------------------------------------
void CActiveConnectorSyncWrapper::RunL()
	{
	 
	 if(iWait.IsStarted())
	 {
		iWait.AsyncStop();	
	 }
	
	}


//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::DoCancel()
//--------------------------------------------------------------------------
void CActiveConnectorSyncWrapper::DoCancel()
	{
    
    iActiveConnector->Cancel();
   	if(iWait.IsStarted())
	{
	  iWait.AsyncStop();	
	}
	
	}

//--------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::ConstructL()
//--------------------------------------------------------------------------
void CActiveConnectorSyncWrapper::ConstructL( RConnection& aConnection )
    {
	iActiveConnector = new ( ELeave ) CConnManActiveConnector( aConnection, Priority() );
	CLOG_ATTACH( iActiveConnector, this );
	}


//-------------------------------------------------------------------------------
//CActiveConnectorSyncWrapper::CActiveConnectorSyncWrapper()
//-------------------------------------------------------------------------------
CActiveConnectorSyncWrapper::CActiveConnectorSyncWrapper( TInt aPriority ): CActive( aPriority )
	{
	CActiveScheduler::Add( this );
	}

void CActiveConnectorSyncWrapper::SetOccPreferences(TSetOCCPreferences aOCCPreferences)
    {
    if ( iActiveConnector )
        iActiveConnector->SetOccPreferences(aOCCPreferences);
    }
//EOF
