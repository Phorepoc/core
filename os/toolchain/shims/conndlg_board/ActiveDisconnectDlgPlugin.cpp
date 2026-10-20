/*
* Copyright (c) 2005 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Implementation of CActiveDisconnectDlgPlugin.
*
*/


// INCLUDE FILES
#include "ActiveDisconnectDlgPlugin.h"
#include "ConnectionInfo.h"
#include "ConnectionDialogsLogger.h"
#include "DisconnectDlgActiveWaiter.h"


// CONSTANTS

_LIT( KMrouterName, "mRouter" );

// ================= MEMBER FUNCTIONS =======================

// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin* CActiveDisconnectDlgPlugin::NewL()
// ---------------------------------------------------------
//
CActiveDisconnectDlgPlugin* CActiveDisconnectDlgPlugin::NewL( 
                                              CDisconnectDialogUi* aPlugin )
    {
    CLOG_ENTERFN( "CActiveDisconnectDlgPlugin::NewL " );  
    
    CActiveDisconnectDlgPlugin* self = 
                         new ( ELeave ) CActiveDisconnectDlgPlugin( aPlugin );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    
    CLOG_LEAVEFN( "CActiveDisconnectDlgPlugin::NewL " );      

    return self;        
    }

// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::ConstructL()
// ---------------------------------------------------------
//
void CActiveDisconnectDlgPlugin::ConstructL()
    {     
    CActiveScheduler::Add( this );            
    }


// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::CActiveDisconnectDlgPlugin()
// ---------------------------------------------------------
//
CActiveDisconnectDlgPlugin::CActiveDisconnectDlgPlugin( 
                                                CDisconnectDialogUi* aPlugin )
: CActive( EPriorityNormal ), 
  iPlugin( aPlugin )
    {
    }
    

// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::~CActiveDisconnectDlgPlugin()
// ---------------------------------------------------------
//    
CActiveDisconnectDlgPlugin::~CActiveDisconnectDlgPlugin()
    {
    Cancel();
    }


// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::DoCancel()
// ---------------------------------------------------------
// 
void CActiveDisconnectDlgPlugin::DoCancel()
    {
    }
     
 
// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::RunL()
// ---------------------------------------------------------
//     
void CActiveDisconnectDlgPlugin::RunL()
    {
    CLOG_ENTERFN( "CActiveDisconnectDlgPlugin::RunL " );  
        
    CLOG_WRITEF( _L( "iStatus.Int() : %d" ), iStatus.Int() );
    CLOG_WRITEF( _L( "&iStatus : %d" ), &iStatus );
    
    if( iStatus == KErrNone )
        {     
        TInt result( KErrNone );
        CConnectionInfo* connectionInfo = CConnectionInfo::NewLC();

        result  = IsConnectionL( connectionInfo );

        CLOG_WRITEF(_L( "result: %d" ), result ); 
        if( result )
            {
            TInt queryResult( KErrNotFound );
            if( iPlugin->Prompt() )
                {
                queryResult = iPlugin->AskDisconnectL();
                }

            if( queryResult )
                {
                // Modell will own connectionInfo
                iPlugin->InitializedL( connectionInfo );
                CleanupStack::Pop( connectionInfo );
                }
            else
                {
                CleanupStack::PopAndDestroy( connectionInfo );
                }
            }
        else
            {
            CleanupStack::PopAndDestroy( connectionInfo );
            iPlugin->SetStartedFlag( EFalse );
            iPlugin->CompleteL( KErrNotFound );
            }
        }
    CLOG_LEAVEFN( "CActiveDisconnectDlgPlugin::RunL " );
    }


// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::StartSearchConnections()
// ---------------------------------------------------------
//    
void CActiveDisconnectDlgPlugin::StartSearchConnections()
    {    
    CLOG_ENTERFN( "CActiveDisconnectDlgPlugin::StartSearchConnections " );  
        
    SetActive();    
    iClientStatus = &iStatus;

    User::RequestComplete( iClientStatus, KErrNone );
    
    CLOG_LEAVEFN( "CActiveDisconnectDlgPlugin::StartSearchConnections " );
    }

    
// ---------------------------------------------------------
// CActiveDisconnectDlgPlugin::IsConnectionL
// ---------------------------------------------------------
//
TBool CActiveDisconnectDlgPlugin::IsConnectionL( 
                                            CConnectionInfo* aConnectionInfo )
    {
    CLOG_ENTERFN( "CActiveDisconnectDlgPlugin::IsConnectionL" );
    
    TBool result( EFalse );
    
    TUint i( 0 );
    TUint connectionCount( 0 );
    TUint connNum( 0 );
    TUint subConnectionCount( 0 );
    TUint connId( 0 );    
    TInt bearer( 0 );
    TInt connStatus( 0 );

    //TRequestStatus status;
    TName apName;

    RConnectionMonitor connMon;
    result = connMon.ConnectL();
    CLOG_WRITEF( _L( "result: %d" ), result );     
    
    CDisconnectDlgActiveWaiter* waiter = CDisconnectDlgActiveWaiter::NewL();
    CleanupStack::PushL( waiter );
    
    connMon.GetConnectionCount( connNum, waiter->iStatus );
    waiter->WaitForRequest();
    //User::WaitForRequest( status );
    CLOG_WRITEF( _L( "status: %d" ), waiter->iStatus.Int() );  
    CLOG_WRITEF( _L( "connNum: %d" ), connNum );     
           
    for( i = 1; i < ( connNum + 1 ) && !connectionCount; ++i )
        {
        connMon.GetConnectionInfo( i, connId, subConnectionCount );

        connMon.GetStringAttribute( connId, 0, KIAPName, apName, waiter->iStatus );
        waiter->WaitForRequest();
        //User::WaitForRequest( status );
        CLOG_WRITEF( _L( "KIAPName status: %d" ), waiter->iStatus.Int() );     
         
    
        connMon.GetIntAttribute( connId, 0, KBearer, bearer, waiter->iStatus );
        waiter->WaitForRequest();
        //User::WaitForRequest( status );
        CLOG_WRITEF( _L( "KBearer status: %d" ), waiter->iStatus.Int() );     
        

        connMon.GetIntAttribute( connId, 0, KConnectionStatus, connStatus, 
                                 waiter->iStatus );
        waiter->WaitForRequest();
        //User::WaitForRequest( status );
        CLOG_WRITEF( _L( "KConnectionStatus status: %d" ), waiter->iStatus.Int() );     
        
        TUint iapId( 0 );
        connMon.GetUintAttribute( connId, 0, KIAPId, iapId, waiter->iStatus ); 
        waiter->WaitForRequest();
        //User::WaitForRequest( status );
        CLOG_WRITEF(_L( "KIAPId status: %d" ), waiter->iStatus.Int() );  
        CLOG_WRITEF(_L( "iapId: %d" ), iapId );
        
        TConnMonTimeBuf timeBuf;
        connMon.GetPckgAttribute( connId, 0, KStartTime, timeBuf, waiter->iStatus );
        waiter->WaitForRequest();
        CLOG_WRITEF(_L( "KStartTime status: %d" ), waiter->iStatus.Int() ); 
       
        switch( bearer )
            {
            case EBearerGPRS :
            case EBearerEdgeGPRS :
            case EBearerExternalGPRS : 
            case EBearerExternalEdgeGPRS :
            case EBearerWCDMA :
            case EBearerExternalWCDMA :
            // Phorepoc board delta (X4.68): Ethernet is the board's bearer, as on the emulator
            case EBearerLAN :
                {
                if ( ( connStatus == KLinkLayerOpen ) ||
                     ( connStatus == KConnectionOpen && 
                       bearer >= EBearerExternalCSD ) )
                    {      
                    apName.Trim();
                    if( apName.Left( KMrouterName().Length() ).
                        CompareF( KMrouterName ) )
                        {
                        ++connectionCount;
                        aConnectionInfo->SetIapNameL( apName );
                        aConnectionInfo->SetBearerType( bearer );
                        aConnectionInfo->SetConnId( connId );
                        aConnectionInfo->SetIAPId( iapId );
                        aConnectionInfo->SetStartTime( timeBuf() );
                        
                        TConnMonClientEnumBuf clientEnum;
                        connMon.GetPckgAttribute( connId, 0, KClientInfo,
                                                    clientEnum, waiter->iStatus );
                        waiter->WaitForRequest();
                        //User::WaitForRequest( status );
                        aConnectionInfo->SetClientInfo( clientEnum() );

                        CLOG_WRITEF( _L( "KClientInfo status: %d" ), waiter->iStatus.Int() );
                        }                    
                    }
                break;
                }
            default :
                {
                break;
                }                
            }
        }
    CleanupStack::PopAndDestroy( waiter );    
    connMon.Close();
    result = connectionCount ? ETrue : EFalse;

    CLOG_WRITEF( _L( "connectionCount: %d" ), connectionCount );     
    CLOG_WRITEF( _L( "result: %d" ), result );     
    CLOG_LEAVEFN( "CActiveDisconnectDlgPlugin::IsConnectionL" );

    return result;      
    }    
    
    
// End of File

