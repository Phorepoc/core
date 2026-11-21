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
* Description:  Implementation of CConfirmationQueryNotif.
*
*/


// INCLUDE FILES
#include "ConfirmationQueryNotif.h"
#include "ConfirmationQuery.h"
#include "ConnectionUiUtilitiesCommon.h"
#include "ConnUiUtilsNotif.h"
#include "ConnectionDialogsLogger.h"

#include <ConnUiUtilsNotif.rsg>
#include <StringLoader.h>

#include <cmmanager.h>
#include <cmmanagerext.h>
#include <cmconnectionmethod.h>
#include <cmdestination.h>
#include <ConnectionUiUtilities.h>
#include <datamobilitycommsdattypes.h>
#include <ctsydomainpskeys.h>
#include <e32debug.h>

using namespace CMManager;

// ================= MEMBER FUNCTIONS =======================

// ---------------------------------------------------------
// CConfirmationQueryNotif::CConfirmationQueryNotif()
// ---------------------------------------------------------
//
CConfirmationQueryNotif::CConfirmationQueryNotif()
: iConnMethod( 0 ), 
  iChoice( EMsgQueryCancelled )
    {
    }


// ---------------------------------------------------------
// CConfirmationQueryNotif* CConfirmationQueryNotif::NewL()
// ---------------------------------------------------------
//
CConfirmationQueryNotif* CConfirmationQueryNotif::NewL( 
                                        const TBool aResourceFileResponsible )
    {
    CConfirmationQueryNotif* self = new ( ELeave ) CConfirmationQueryNotif();
    CleanupStack::PushL( self );
    self->ConstructL( KResourceFileName, aResourceFileResponsible );
    CleanupStack::Pop();

    return self;
    }


// ---------------------------------------------------------
// CConfirmationQueryNotif::~CConfirmationQueryNotif()
// ---------------------------------------------------------
//
CConfirmationQueryNotif::~CConfirmationQueryNotif()
    {   
    if ( iLaunchDialogAsync )
        {
        delete iLaunchDialogAsync;
        iLaunchDialogAsync = NULL;
        }
    }

// ---------------------------------------------------------
// CConfirmationQueryNotif::RegisterL()
// ---------------------------------------------------------
//
CConfirmationQueryNotif::TNotifierInfo CConfirmationQueryNotif::RegisterL()
    {
    iInfo.iUid = KUidConfirmationQuery;
    iInfo.iPriority = ENotifierPriorityHigh;
    iInfo.iChannel = KUidConfirmationQuery;

    return iInfo;
    }

// ---------------------------------------------------------
// void CConfirmationQueryNotif::StartL
// ---------------------------------------------------------
//
void CConfirmationQueryNotif::StartL( const TDesC8& aBuffer, 
                                    TInt aReplySlot,
                                    const RMessagePtr2& aMessage )
    {
    if ( iLaunchDialogAsync )
        {
        if ( iLaunchDialogAsync->IsActive() )
           {
           // Dialog is already active
           aMessage.Complete( KErrServerBusy );
           return;
           }    
        }
    
    TInt err( KErrNone );
    TInt emergencyCallState( 0 );
    
    // Check if emergency call is ongoing. If it is then do not display the dialog.    
    err = RProperty::Get( KPSUidCtsyEmergencyCallInfo,
            KCTSYEmergencyCallInfo,
            emergencyCallState );

    // PATCHED COPY (Phorepoc, core os/toolchain/shims/connuiutils_prio): the roaming query cancelled itself unseen (MPM -3, no
    // [confq] line = no dialog was built). Trace which of StartL's silent-cancel conditions holds and which note was asked for.
    {
    TPckgBuf<TConnUiUiDestConnMethodNoteId> note;
    note.Copy( aBuffer.Left( Min( aBuffer.Length(), note.MaxLength() ) ) );
    RDebug::Printf( "[confq] StartL note %d: emergency call %d (err %d), screen saver %d, autolock %d, busy %d",
        (TInt)note().iNoteId, emergencyCallState, err, (TInt)ScreenSaverOn(), (TInt)AutolockOn(),
        (TInt)( iLaunchDialogAsync != NULL ) );
    }

    if ( (err == KErrNone && emergencyCallState) || ScreenSaverOn() || AutolockOn() )
        {
        // Emergency call, screen saver or autolock is active. Cancel the dialog. 
        CLOG_WRITE( "CConfirmationQueryNotif::StartL: Emergency call, Screen saver or Autolock is active." );
        aMessage.WriteL( aReplySlot, TPckg<TMsgQueryLinkedResults>( EMsgQueryCancelled ) );
        aMessage.Complete( KErrNone );
        return;
        }

    iChoice = EMsgQueryCancelled; 
    TPckgBuf<TConnUiUiDestConnMethodNoteId> input;
    input.Copy( aBuffer );
    iNoteInfo = input();
    
    iReplySlot = aReplySlot;
    iMessage = aMessage;
    iCancelled = EFalse;

    if ( iLaunchDialogAsync )
        {
        delete iLaunchDialogAsync;
        iLaunchDialogAsync = NULL;
        }
    
    if ( iNoteInfo.iNoteId == EConfirmMethodUsageQueryInHomeNetwork )
        {
        TCallBack cb( LaunchDialogL, this );
        iLaunchDialogAsync = new( ELeave ) CAsyncCallBack( cb, CActive::EPriorityHigh );    
        }
    else if ( iNoteInfo.iNoteId == EConfirmMethodUsageQueryInForeignNetwork )
        {
        TCallBack cb( LaunchDialogVisitorL, this );
        iLaunchDialogAsync = new( ELeave ) CAsyncCallBack( cb, CActive::EPriorityHigh );
        }
    else // EConfirmMethodUsageQueryNationalRoaming;
        {
        TCallBack cb( LaunchDialogNationalRoamingL, this );
        iLaunchDialogAsync = new( ELeave ) CAsyncCallBack( cb, CActive::EPriorityHigh );    
        }

    iLaunchDialogAsync->CallBack();
    }


// ---------------------------------------------------------
// void CConfirmationQueryNotif::Cancel()
// ---------------------------------------------------------
//
void CConfirmationQueryNotif::Cancel()
    {
    CLOG_ENTERFN( "CConfirmationQueryNotif::Cancel" );
    if ( !iCancelled )
        {
        iCancelled = ETrue;
        if ( !iMessage.IsNull() )
            {
            iMessage.Complete( KErrCancel );
            }
        
        if ( iDialog )
            {
            delete iDialog;
            iDialog = NULL;
            }
        
        if ( iDialogVisitor )
            {
            delete iDialogVisitor;
            iDialogVisitor = NULL;
            }
        }
    
    CLOG_LEAVEFN( "CConfirmationQueryNotif::Cancel" );
    }


// ---------------------------------------------------------
// void CConfirmationQueryNotif::CompleteL( TInt aStatus )
// ---------------------------------------------------------
//
void CConfirmationQueryNotif::CompleteL( TInt aStatus )
    {  
    CLOG_ENTERFN( "CConfirmationQueryNotif::CompleteL" );
    iCancelled = ETrue;   
    if ( !iMessage.IsNull() )
        {  
        if ( aStatus == KErrNone )
            {
            iMessage.WriteL( iReplySlot, TPckg<TMsgQueryLinkedResults>( iChoice ) );
            }
        iMessage.Complete( aStatus );
        }
      
    Cancel();
    
    CLOG_LEAVEFN( "CConfirmationQueryNotif::CompleteL" );
    }

// ---------------------------------------------------------------
// void CConfirmationQueryNotif::SetSelectedChoiceL( TInt aChoice )
// ----------------------------------------------------------------
//
void CConfirmationQueryNotif::SetSelectedChoiceL ( TInt aChoice )
    {
    CLOG_ENTERFN( "CConfirmationQueryNotif::SetSelectedChoiceL" );
    iChoice = (TMsgQueryLinkedResults)aChoice;
    CLOG_LEAVEFN( "CConfirmationQueryNotif::SetSelectedChoiceL" );
    }

// ---------------------------------------------------------
// TInt CConfirmationQueryNotif::LaunchDialogL()
// ---------------------------------------------------------
//
TInt CConfirmationQueryNotif::LaunchDialogL( TAny* aObject )
    {
    CLOG_ENTERFN( "CCConfirmationQueryNotif::LaunchDialogL" );
    CConfirmationQueryNotif* myself =
                            static_cast<CConfirmationQueryNotif*>( aObject );
    
    myself->iDialog = new ( ELeave ) CConfirmationQuery( myself );
    myself->iDialog->PrepareLC( R_MESSAGE_QUERY );
                            
    // Create the list box items
    RArray<TMsgQueryLinkedResults> choices;
    CDesCArrayFlat* array = new (ELeave) CDesCArrayFlat(2);
    CleanupStack::PushL( array );
    HBufC* heading = NULL;
    HBufC* messageBase = NULL;
    HBufC* automatic = NULL;
    HBufC* thisTime = NULL;

    automatic = StringLoader::LoadLC( R_QTN_OCC_LIST_CS_DATA_HOME_NW_AUTOMATIC );
    thisTime = StringLoader::LoadLC( R_QTN_OCC_LIST_CS_DATA_HOME_NW_THIS_TIME );
                                
    heading = StringLoader::LoadLC( R_QTN_OCC_PRMPT_CS_DATA_HOME_NW );
    messageBase = StringLoader::LoadLC( R_QTN_OCC_DETAIL_CS_DATA_HOME_NW );
    // the order of the query options depends on the location
    choices.Append(EMsgQueryAutomatically);
    choices.Append(EMsgQueryThisTime);
    array->AppendL( *automatic );
    array->AppendL( *thisTime );
 
    // Set the dialog heading and message text
    myself->iDialog->Heading()->SetTextL(*heading);
    myself->iDialog->MessageBox()->SetMessageTextL(messageBase);
    CleanupStack::PopAndDestroy( messageBase );
    CleanupStack::PopAndDestroy( heading );
                            
    // Set the options to the listbox
    myself->iDialog->SetChoices(choices);

    myself->iDialog->SetItemTextArray( array );
    myself->iDialog->SetOwnershipType(ELbmOwnsItemArray); // ownership transferred to listbox
    myself->iDialog->ListBox()->HandleItemAdditionL();
                            
    CleanupStack::Pop( thisTime );
    CleanupStack::Pop( automatic );
    CleanupStack::Pop( array );
    myself->iDialog->RunLD();
                                 
    CLOG_LEAVEFN( "CConfirmationQueryNotif::LaunchDialogL" );
    return 0;   
    }

// ---------------------------------------------------------
// TInt CConfirmationQueryNotif::LaunchDialogVisitorL()
// ---------------------------------------------------------
//
TInt CConfirmationQueryNotif::LaunchDialogVisitorL( TAny* aObject )
    {
    CLOG_ENTERFN( "CCConfirmationQueryNotif::LaunchDialogVisitorL" );
    CConfirmationQueryNotif* myself =
                            static_cast<CConfirmationQueryNotif*>( aObject );

    myself->iDialogVisitor = new ( ELeave ) CConfirmationQueryVisitor( myself );
    myself->iDialogVisitor->PrepareLC( R_VISITOR_QUERY );

    HBufC* heading  = NULL;
    HBufC* message  = NULL;
    TInt resourceId = R_QTN_OCC_DETAIL_CS_DATA_FOREIGN_NW;
    heading         = StringLoader::LoadLC( 
                      R_QTN_OCC_PRMPT_CS_DATA_FOREIGN_NW );

    CDesCArrayFlat* strings = new( ELeave ) CDesCArrayFlat( 2 );
    CleanupStack::PushL( strings );

    HBufC* messageBase = StringLoader::LoadL( resourceId, *strings );
    CleanupStack::PopAndDestroy( strings );

    CleanupStack::PushL( messageBase );
    TInt lenMsg = messageBase->Des().Length();

    message = HBufC::NewL( lenMsg );
    TPtr messagePtr = message->Des();
    messagePtr.Append( messageBase->Des() ); 

    CleanupStack::PopAndDestroy( messageBase );

    CleanupStack::PushL( message );
    myself->iDialogVisitor->SetMessageTextL( *message );
    CleanupStack::PopAndDestroy( message );

    myself->iDialogVisitor->QueryHeading()->SetTextL( *heading );
    CleanupStack::PopAndDestroy( heading );

    myself->iDialogVisitor->RunLD();
    CLOG_LEAVEFN( "CConfirmationQueryNotif::LaunchDialogVisitorL" );
    return 0;
    }

// ------------------------------------------------------------
// TInt CConfirmationQueryNotif::LaunchDialogNationalRoamingL()
// ------------------------------------------------------------
//
TInt CConfirmationQueryNotif::LaunchDialogNationalRoamingL( TAny* aObject )
    {
    CLOG_ENTERFN( "CCConfirmationQueryNotif::LaunchDialogNationalRoamingL" );
    CConfirmationQueryNotif* myself =
                            static_cast<CConfirmationQueryNotif*>( aObject );
    
    myself->iDialog = new ( ELeave ) CConfirmationQuery( myself );
    myself->iDialog->PrepareLC( R_MESSAGE_QUERY );
                            
    // Create the list box items
    RArray<TMsgQueryLinkedResults> choices;
    CDesCArrayFlat* array = new (ELeave) CDesCArrayFlat(2);
    CleanupStack::PushL( array );
    HBufC* heading = NULL;
    HBufC* messageBase = NULL;
    HBufC* automatic = NULL;
    HBufC* thisTime = NULL;

    automatic = StringLoader::LoadLC( R_QTN_OCC_LIST_CS_DATA_HOME_NW_AUTOMATIC );
    thisTime = StringLoader::LoadLC( R_QTN_OCC_LIST_CS_DATA_HOME_NW_THIS_TIME );
                                
    heading = StringLoader::LoadLC( R_QTN_OCC_PRMPT_CS_DATA_HOME_NW );
    messageBase = StringLoader::LoadLC( R_QTN_OCC_DETAIL_CS_DATA_NATIONAL_ROAMING );
    // the order of the query options depends on the location
    choices.Append(EMsgQueryAutomatically);
    choices.Append(EMsgQueryThisTime);
    array->AppendL( *automatic );
    array->AppendL( *thisTime );
 
    // Set the dialog heading and message text
    myself->iDialog->Heading()->SetTextL(*heading);
    myself->iDialog->MessageBox()->SetMessageTextL(messageBase);
    CleanupStack::PopAndDestroy( messageBase );
    CleanupStack::PopAndDestroy( heading );
                            
    // Set the options to the listbox
    myself->iDialog->SetChoices(choices);

    myself->iDialog->SetItemTextArray( array );
    myself->iDialog->SetOwnershipType(ELbmOwnsItemArray); // ownership transferred to listbox
    myself->iDialog->ListBox()->HandleItemAdditionL();
                            
    CleanupStack::Pop( thisTime );
    CleanupStack::Pop( automatic );
    CleanupStack::Pop( array );
    myself->iDialog->RunLD();
                                 
    CLOG_LEAVEFN( "CConfirmationQueryNotif::LaunchDialogNationalRoamingL" );
    return 0;
    }

// End of File
