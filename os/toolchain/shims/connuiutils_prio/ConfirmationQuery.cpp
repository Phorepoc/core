/*
* Copyright (c) 2006 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Implementation of class CConfirmationQuery.
*
*/


// INCLUDE FILES
#include "ConfirmationQuery.h"
#include "ConfirmationQueryNotif.h"
#include "ConnectionUiUtilitiesCommon.h"
#include "ConnectionDialogsLogger.h"
#include "ExpiryTimer.h"

#include <uikon/eiksrvui.h>
#include <StringLoader.h>
#include <ConnUiUtilsNotif.rsg>
#include <e32debug.h>
#include <aknglobalpopupprioritycontroller.h>
#include <GlobalWindowPriorities.h>

// PATCHED COPY (Phorepoc, core os/toolchain/shims/connuiutils_prio): the roaming / method-usage confirmation query registers with
// Avkon's global popup stack ABOVE ordinary notes (priority 0). Left at 0 it sat behind a note raised later ("2 new messages") in the
// common notifier app server, and its expiry timer then cancelled it unseen (MPM: CMPMConfirmDlgRoaming, EConfirmDlgVisitorNetwork).
// KGlobalWindowPriority_Alarm = 2 is above the key-lock and battery notes (1) and equal to an alarm (the later one is on top).
// TraceGroups prints the dialog's window-group priority and the front window groups, for the phone log.
static void TraceGroups( const TDesC& aWhy, CCoeControl& aPopup )
    {
    CCoeEnv* env = CCoeEnv::Static();
    RWsSession& ws = env->WsSession();
    const TInt own = env->RootWin().Identifier();
    RDebug::Print( _L( "[confq] %S: own wg %d priority %d, dialog window pos %d" ), &aWhy, own,
        ws.GetWindowGroupOrdinalPriority( own ), aPopup.DrawableWindow() ? aPopup.DrawableWindow()->OrdinalPosition() : -1 );
    CArrayFixFlat<TInt>* ids = new CArrayFixFlat<TInt>( 8 );
    if ( ids )
        {
        if ( ws.WindowGroupList( ids ) == KErrNone )
            {
            for ( TInt i = 0; i < ids->Count() && i < 6; i++ )
                {
                TBuf<64> name;
                ws.GetWindowGroupNameFromIdentifier( ids->At( i ), name );
                RDebug::Print( _L( "[confq]   z%d wg %d priority %d %S" ), i, ids->At( i ),
                    ws.GetWindowGroupOrdinalPriority( ids->At( i ) ), &name );
                }
            }
        delete ids;
        }
    }

static void RaiseQueryPriority( CCoeControl& aPopup )
    {
    TRAPD( err, AknGlobalPopupPriorityController::SetPopupPriorityL( aPopup, KGlobalWindowPriority_Alarm ) );
    _LIT( KRaised, "query shown" );
    RDebug::Print( _L( "[confq] SetPopupPriorityL(%d) -> %d" ), KGlobalWindowPriority_Alarm, err );
    TraceGroups( KRaised, aPopup );
    }



// CONSTANTS
#if defined(_DEBUG)
_LIT( KErrNullPointer, "NULL pointer" );
#endif


// ================= MEMBER FUNCTIONS =======================
//
// ---------------------------------------------------------
// CConfirmationQuery::CConfirmationQuery
// HOME NETWORK
// ---------------------------------------------------------
//
CConfirmationQuery::CConfirmationQuery( CConfirmationQueryNotif* aNotif )
: CAknListQueryDialog( &iDummy ),
  iNotif( aNotif ),
  iButtonGroupPreviouslyChanged( EFalse )
   {
   }

// ---------------------------------------------------------
// CConfirmationQuery::~CConfirmationQuery
// ---------------------------------------------------------
//
CConfirmationQuery::~CConfirmationQuery()
    {
    AknGlobalPopupPriorityController::RemovePopupPriority( *this );
    STATIC_CAST( CEikServAppUi*, 
                 CCoeEnv::Static()->AppUi() )->SuppressAppSwitching( EFalse );
    delete iExpiryTimer;
    iChoiceIds.Close();
    }

// ---------------------------------------------------------
// CConfirmationQuery::OkToExitL
// ---------------------------------------------------------
//
TBool CConfirmationQuery::OkToExitL( TInt aButtonId )
    {
    CLOG_ENTERFN( "CConfirmationQuery::OkToExitL" );
    TBool result( EFalse );
    TInt status = KErrCancel;
    if ( aButtonId == EAknSoftkeySelect || 
         aButtonId == EAknSoftkeyDone || aButtonId == EAknSoftkeyOk )
        {
        iNotif->SetSelectedChoiceL( iChoiceIds[ListBox()->CurrentItemIndex()] );
        result = ETrue;
        status = KErrNone;
        }
    else if ( aButtonId == EAknSoftkeyCancel )
        {
        status = KErrCancel;
        result = ETrue;
        }

    if ( result )
        {
        CLOG_WRITEF( _L( "aButtonId = %d" ), aButtonId );
        __ASSERT_DEBUG( iNotif, User::Panic( KErrNullPointer, KErrNone ) );
        iNotif->CompleteL( status );
        }

    CLOG_LEAVEFN( "CConfirmationQuery::OkToExitL" );

    return result;  
    }
    
// ---------------------------------------------------------
// CConfirmationQuery::PreLayoutDynInitL()
// ---------------------------------------------------------
//
void CConfirmationQuery::PreLayoutDynInitL()
    {      
    CAknListQueryDialog::PreLayoutDynInitL();
    RaiseQueryPriority( *this );

    STATIC_CAST( CEikServAppUi*, 
                CCoeEnv::Static()->AppUi() )->SuppressAppSwitching( ETrue );

    iExpiryTimer = CExpiryTimer::NewL( *this );
    iExpiryTimer->Start();
    }

// ---------------------------------------------------------
// CConfirmationQuery::TryExitL()
// ---------------------------------------------------------
//
void CConfirmationQuery::TryExitL( TInt aButtonId )
    {
    CLOG_ENTERFN( "CConfirmationQuery::TryExitL" );
    CAknListQueryDialog::TryExitL( aButtonId );
    CLOG_LEAVEFN( "CConfirmationQuery::TryExitL" );
    }

// ---------------------------------------------------------
// CConfirmationQuery::OfferKeyEventL
// ---------------------------------------------------------
//
TKeyResponse CConfirmationQuery::OfferKeyEventL( const TKeyEvent& aKeyEvent, 
                                                 TEventCode aType)
    {
    if( aType == EEventKey && aKeyEvent.iCode == EKeyPhoneSend )
        {
        // Let's not obscure the Dialer in the background
        if ( iExpiryTimer )
            {
            iExpiryTimer->Cancel();
            iExpiryTimer->StartShort();    
            }
        }
    
    return CAknListQueryDialog::OfferKeyEventL( aKeyEvent,aType ); 
    } 

// ---------------------------------------------------------
// CConfirmationQuery::SetChoices
// ---------------------------------------------------------
//
void CConfirmationQuery::SetChoices( RArray<TMsgQueryLinkedResults> aChoices )
    {
    iChoiceIds = aChoices;
    }

void CConfirmationQuery::HandleTimedOut()
    {
    TRAP_IGNORE( TryExitL(EAknSoftkeyCancel) );
    }


// ================= MEMBER FUNCTIONS =======================
//
// ---------------------------------------------------------
// CConfirmationQueryVisitor::CConfirmationQueryVisitor
// VISITOR NETWORK
// ---------------------------------------------------------
//
CConfirmationQueryVisitor::CConfirmationQueryVisitor( 
        CConfirmationQueryNotif* aNotif )
    : 
    iNotif( aNotif )
    {
    }
    
// ---------------------------------------------------------
// CConfirmationQueryVisitor::~CConfirmationQueryVisitor
// ---------------------------------------------------------
//
CConfirmationQueryVisitor::~CConfirmationQueryVisitor()
    {
    AknGlobalPopupPriorityController::RemovePopupPriority( *this );
    STATIC_CAST( CEikServAppUi*, 
        CCoeEnv::Static()->AppUi() )->SuppressAppSwitching( EFalse );
    delete iExpiryTimer;
    }

// ---------------------------------------------------------
// CConfirmationQueryVisitor::OkToExitL
// ---------------------------------------------------------
//
TBool CConfirmationQueryVisitor::OkToExitL( TInt aButtonId )
    {
    CLOG_ENTERFN( "CConfirmationQueryVisitor::OkToExitL" );
    TBool result( EFalse );
    TInt status = KErrCancel;
    
    if ( aButtonId == EAknSoftkeySelect || 
         aButtonId == EAknSoftkeyDone || aButtonId == EAknSoftkeyOk )
        {
        iNotif->SetSelectedChoiceL( EMsgQueryThisTime );
        result = ETrue;
        status = KErrNone;
        }
    else if ( aButtonId == EAknSoftkeyCancel )
        {
        status = KErrCancel;
        result = ETrue;
        }

    if ( result )
        {
        CLOG_WRITEF( _L( "aButtonId = %d" ), aButtonId );
        if ( iNotif )
            {
            iNotif->CompleteL( status );
            }
        }

    CLOG_LEAVEFN( "CConfirmationQueryVisitor::OkToExitL" );

    return result;  
    }
    
// ---------------------------------------------------------
// CConfirmationQueryVisitor::PreLayoutDynInitL()
// ---------------------------------------------------------
//
void CConfirmationQueryVisitor::PreLayoutDynInitL()
    {      
    CAknMessageQueryDialog::PreLayoutDynInitL();
    RaiseQueryPriority( *this );

    STATIC_CAST( CEikServAppUi*, 
                CCoeEnv::Static()->AppUi() )->SuppressAppSwitching( ETrue );

    iExpiryTimer = CExpiryTimer::NewL( *this );
    iExpiryTimer->Start();
    }

// ---------------------------------------------------------
// CConfirmationQueryVisitor::TryExitL()
// ---------------------------------------------------------
//
void CConfirmationQueryVisitor::TryExitL( TInt aButtonId )
    {
    CLOG_ENTERFN( "CConfirmationQueryVisitor::TryExitL" );
    CAknMessageQueryDialog::TryExitL( aButtonId );
    CLOG_LEAVEFN( "CConfirmationQueryVisitor::TryExitL" );
    }

// ---------------------------------------------------------
// CConfirmationQueryVisitor::OfferKeyEventL
// ---------------------------------------------------------
//
TKeyResponse CConfirmationQueryVisitor::OfferKeyEventL( const TKeyEvent& aKeyEvent, 
                                                 TEventCode aType)
    {
    if( aType == EEventKey && aKeyEvent.iCode == EKeyPhoneSend )
        {
        // Let's not obscure the Dialer in the background
        if ( iExpiryTimer )
            {
            iExpiryTimer->Cancel();
            iExpiryTimer->StartShort();    
            }
        }
    
    return CAknMessageQueryDialog::OfferKeyEventL( aKeyEvent,aType ); 
    } 

void CConfirmationQueryVisitor::HandleTimedOut()
    {
    TRAP_IGNORE( TryExitL(EAknSoftkeyCancel) );
    }

// End of File
