// OURS COPY: RCL_3 eiksrvsp.cpp unchanged except the blocks marked OURS (7.51.15: secondary-screen server status panes).
/*
* Copyright (c) 2002-2008 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Server side status pane implementation.
*
*/


#include "eiksrvsp.h"
#include <AknDef.h>
#include <AknUtils.h>
#include <AknStatuspaneUtils.h>
#include "aknstatuspanedatapublisher.h"

// ===========================================================================
// class CEikServStatusPane
// Server side status pane implementation.
// ===========================================================================

// ---------------------------------------------------------------------------
// CEikServStatusPane::NewL
// Factory method for the server side status pane.
// ---------------------------------------------------------------------------
//
CEikServStatusPane* CEikServStatusPane::NewL( CEikonEnv& aEikEnv,
                                              RWindowGroup* aParent, 
                                              TInt aCoreResId )
    {
    CEikServStatusPane* self =
        new (ELeave) CEikServStatusPane( aEikEnv, aParent );
    CleanupStack::PushL( self );
    self->ConstructL( aCoreResId );
    CleanupStack::Pop( self );
    return self;
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::~CEikServStatusPane
// Destructor.
// ---------------------------------------------------------------------------
//
CEikServStatusPane::~CEikServStatusPane()
    {
    CAknSgcClient::SetStatusPaneRedrawCoordinator(NULL);
    delete iDataPublisher;
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::SetBlankWinObserver
// Sets the status pane observer for the screen blanker.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::SetBlankWinObserver(
    MEikStatusPaneObserver* aBlankWinObserver )
    {
    iBlankWinObserver = aBlankWinObserver;
    
    // If we have blankwin, then disable statuspanes own blanker.
    if ( iBlankWinObserver )
        {
        DisableClearer( ETrue );
        }
    else
        {
        DisableClearer( EFalse );
        }
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::CEikServStatusPane
// C++ constructor.
// OURS (multi-window, docs/phorepoc/hildon-port.md 7.51.15): set while akncapserver builds the server status pane of a WSERV screen
// other than 0 (AknCapServerEntry.cpp). Such a pane draws the same model on its own screen, but the system-wide parts stay single:
// the indicator data publisher (a P&S property only one may define) and the SGC redraw coordinator remain screen 0's pane's.
TBool PheSecondaryServStatusPane = EFalse;

// ---------------------------------------------------------------------------
//
CEikServStatusPane::CEikServStatusPane( CEikonEnv& aEikEnv,
                                        RWindowGroup* aParent )
    : CEikStatusPaneBase( aEikEnv, aParent )
    {
    if ( !PheSecondaryServStatusPane )      // OURS (7.51.15): screen 0's pane stays the redraw coordinator
        {
        CAknSgcClient::SetStatusPaneRedrawCoordinator( this );
        }
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::ConstructL
// Second-phase constructor.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::ConstructL( TInt aCoreResId )
    {
    BaseConstructL( aCoreResId );
    if ( !PheSecondaryServStatusPane )      // OURS (7.51.15): ONE system data publisher (a P&S property), screen 0's
        {
        iDataPublisher = CAknStatusPaneDataPublisher::NewL();
        }
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::CreateModelL
// Creates the server side status pane model.
// ---------------------------------------------------------------------------
//
CEikStatusPaneModelBase* CEikServStatusPane::CreateModelL(
    TInt aCoreResId ) const
    {
    return CEikServStatusPaneModel::NewL( iEikEnv, aCoreResId, *this );
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::IsApp
// Returns EFalse as server side status pane.
// ---------------------------------------------------------------------------
//
TBool CEikServStatusPane::IsApp() const
    {
    return EFalse;
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::SetFaded
// Does nothing, as fading is now handled by the Window Server and the
// Application Framework.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::SetFaded( TBool /*aFaded*/ )
    {
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::BlockServerStatusPaneRedrawsL
// Status pane redraw blocker is not used anymore.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::BlockServerStatusPaneRedrawsL()
    {
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::RedrawServerStatusPane
// Redraws the server side status pane.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::RedrawServerStatusPane()
    {
    // All status pane drawing is done on the application side.
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::HandleShapeChangeL
// Handles event caused by status pane shape change.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::HandleShapeChangeL() const
    {
    if ( iBlankWinObserver )
        {
        iBlankWinObserver->HandleStatusPaneSizeChange();
        }
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::PrepareForAppExit
// Prepares status pane for application exit.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::PrepareForAppExit()
    {
    CommonPrepareForAppExit();  
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::SetCurrentAppStatuspaneResourceId
// Sets the current status pane layout resource ID of the application side.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::SetCurrentAppStatuspaneResourceId( TInt aResourceId )
    {
    iCurrentAppStatuspaneResourceId = aResourceId;              
    DisableClearer( ETrue );  // Makes clearer invisible.
    DisableClearer( EFalse ); // Enables clearer again for next event.
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::CurrentAppStatuspaneResourceId
// Returns the current status pane layout resource ID in the application side.
// ---------------------------------------------------------------------------
//
TInt CEikServStatusPane::CurrentAppStatuspaneResourceId()
    {
    return iCurrentAppStatuspaneResourceId;
    }


// ---------------------------------------------------------------------------
// CEikServStatusPane::HandleResourceChange
// Handles resource change events.
// ---------------------------------------------------------------------------
//
void CEikServStatusPane::HandleResourceChange( TInt aType )
    {
    CEikStatusPaneBase::HandleResourceChange( aType );

    if ( aType == KEikDynamicLayoutVariantSwitch )
        {
        iCurrentAppStatuspaneResourceId = CurrentLayoutResId(); 
        }
    
    if ( iDataPublisher )   // OURS (7.51.15): a secondary screen's pane has none
        {
        iDataPublisher->SetIndicatorPopupPosition();
        }
    }


void CEikServStatusPane::SwitchLayoutL( TInt aLayoutResourceId )
    {
    AknLayoutUtils::TAknCbaLocation prevCbaLocation = AknLayoutUtils::CbaLocation();

    if( aLayoutResourceId != CurrentLayoutResId() )
        {
        CEikStatusPaneBase::SwitchLayoutL( aLayoutResourceId );
        if ( iDataPublisher )   // OURS (7.51.15): a secondary screen's pane has none
        {
        iDataPublisher->SetIndicatorPopupPosition();
        }
        }

    if ( prevCbaLocation != AknLayoutUtils::CbaLocation() )
        {
        CEikAppUi *appUi = dynamic_cast<CEikAppUi *>(CCoeEnv::Static()->AppUi());
        if ( appUi )
            {
            appUi->ReportResourceChangedToAppL( KEikDynamicLayoutVariantSwitch );
            }
        }
    }


// ===========================================================================
// class CEikServStatusPaneModel
// Server side status pane model class implementation.
// ===========================================================================


// ---------------------------------------------------------------------------
// CEikServStatusPaneModel::NewL
// Two-phased constructor.
// ---------------------------------------------------------------------------
//
CEikServStatusPaneModel* CEikServStatusPaneModel::NewL(
    CEikonEnv& aEikEnv,
    TInt aCoreResId, 
    const MEikServStatusPaneShapeChangeObserver& aShapeObserver )
    {
    CEikServStatusPaneModel* self =
        new (ELeave) CEikServStatusPaneModel( aEikEnv, aShapeObserver );
    CleanupStack::PushL( self );
    self->ConstructL( aCoreResId );
    CleanupStack::Pop( self );
    return self;
    }


// ---------------------------------------------------------------------------
// CEikServStatusPaneModel::~CEikServStatusPaneModel
// Destructor.
// ---------------------------------------------------------------------------
//
CEikServStatusPaneModel::~CEikServStatusPaneModel()
    {
    }


// ---------------------------------------------------------------------------
// CEikServStatusPaneModel::CEikServStatusPaneModel
// C++ constructor.
// ---------------------------------------------------------------------------
//
CEikServStatusPaneModel::CEikServStatusPaneModel(
    CEikonEnv& aEikEnv, 
    const MEikServStatusPaneShapeChangeObserver& aShapeObserver )
    : CEikStatusPaneModelBase( aEikEnv ), iShapeObserver( aShapeObserver )
    {
    }


// ---------------------------------------------------------------------------
// CEikServStatusPaneModel::ConstructL
// Second-phase constructor.
// ---------------------------------------------------------------------------
//
void CEikServStatusPaneModel::ConstructL( TInt aCoreResId )
    {
    BaseConstructL( aCoreResId );

    CIdSet* ids = LegalIds();
    TInt count  = ids->Count();

    iLayouts = new (ELeave) CLayoutIdSet( count );
    for ( TInt i = 0; i < count; ++i )
        {
        LoadLayoutL( ids->At( i ) );
        }

    SetLayoutL( iCurrentResId, ETrue );
    }


// ---------------------------------------------------------------------------
// CEikServStatusPaneModel::SetLayoutL
// Sets the status pane layout.
// ---------------------------------------------------------------------------
//
void CEikServStatusPaneModel::SetLayoutL( TInt aLayoutResId,
                                          TBool aChangeStatusPaneNow )
    {
    CEikStatusPaneModelBase::SetLayoutL( aLayoutResId, aChangeStatusPaneNow );
    iShapeObserver.HandleShapeChangeL();
    }

// End of file
