// PHOREPOC board delta: the release file with one fix -- ~CFilteredGroupView closes the native base view a second time when a custom
// filtered view exists (use-after-free; the same bug as CFilteredContactView.cpp, X4.84). Everything else unchanged.
/*
* Copyright (c) 2002-2007 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Contacts Model store filtered contact view implementation.
*
*/


// INCLUDES
#include "CFilteredGroupView.h"

// VPbkCntModel
#include "CContactStore.h"
#include "CFieldFactory.h"
#include "CCustomFilteredGroupView.h"
#include <VPbkCntModelRes.rsg>
#include "VPbkCntModelRemoteViewPreferences.h"
#include "NamedRemoteViewViewDefinitionStoreUtility.h"


// VPbkEng
#include <CVPbkFieldTypeSelector.h>
#include <VPbkDataCaging.hrh>
#include <CVPbkFieldType.h>
#include <CVPbkFieldTypeList.h>
#include <CVPbkContactViewDefinition.h>
#include <TVPbkFieldVersitProperty.h>
#include <VPbkSendEventUtility.h>
#include <CVPbkSortOrder.h>

// VPbkEngUtils
#include <RLocalizedResourceFile.h>

// System includes
#include <cntview.h>
#include <barsread.h>

// Debugging headers
#include <VPbkDebug.h>

namespace VPbkCntModel {

// --------------------------------------------------------------------------
// CFilteredGroupView::CFilteredGroupView
// --------------------------------------------------------------------------
//
inline CFilteredGroupView::CFilteredGroupView
        ( CContactStore& aParentStore ) :
            CViewBase( aParentStore )
    {
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::~CFilteredGroupView
// --------------------------------------------------------------------------
//
CFilteredGroupView::~CFilteredGroupView()
    {
    // PHOREPOC fix: the custom filtered view is a CViewBase whose iView IS our native base view (ConstructL passes *iView);
    // ~CViewBase closes it with the custom view as observer (the only one it was opened with), which deletes the native view.
    // Closing it again below was a use-after-free (Phonebook 2 KERN-EXEC 3 on leaving the add-recipient list).
    const TBool nativeClosedByCustom = ( iCustomFilteredView != NULL );

    // Customically filtered view has to be destructed first
    delete iCustomFilteredView;
    delete iFilter;

    // Set iView to NULL, so that CViewBase does not close it
    iView = NULL;

    if ( iBaseView && !nativeClosedByCustom )
        {
        iBaseView->Close( *iNativeObserver );
        }
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::NewLC
// --------------------------------------------------------------------------
//
CFilteredGroupView* CFilteredGroupView::NewLC(
        const CVPbkContactViewDefinition& aViewDefinition,
        MVPbkContactViewObserver& aObserver,
        CContactStore& aParentStore,
        const MVPbkFieldTypeList& aSortOrder,
        RFs& aFs )
    {
    CFilteredGroupView* self =
        new ( ELeave ) CFilteredGroupView( aParentStore );
    CleanupStack::PushL( self );
    self->ConstructL( aViewDefinition, aObserver, aSortOrder, aFs );
    return self;
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ConstructL
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ConstructL(
        const CVPbkContactViewDefinition& aViewDefinition,
        MVPbkContactViewObserver& aObserver,
        const MVPbkFieldTypeList& aSortOrder,
        RFs& /*aFs*/ )
    {
    const CFieldFactory& fieldFactory = Store().FieldFactory();

    // Construct the native filter
    if ( aViewDefinition.FieldTypeFilter() )
        {
        // Copy construct the filter
        iFilter = CVPbkFieldTypeSelector::NewL( 
                *aViewDefinition.FieldTypeFilter() );
        }

    CViewBase::ConstructL( aViewDefinition, aObserver, aSortOrder );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::Type
// --------------------------------------------------------------------------
//
TVPbkContactViewType CFilteredGroupView::Type() const
    {
    return EVPbkGroupsView;
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::DoInitializeViewL
// --------------------------------------------------------------------------
//
void CFilteredGroupView::DoInitializeViewL(
        const CVPbkContactViewDefinition& aViewDefinition,
        RContactViewSortOrder& aViewSortOrder )
    {
    // Stop observing the base view, the custom view
    // will observe it and report back
    // Construction of the iCustomFilteredView should be done in two 
    // phases. Due to that there is dependencies between views in this 
    // and iCustomFilteredView class.
    CCustomFilteredGroupView* customFilteredView = 
        new (ELeave) CCustomFilteredGroupView( Store(), iFilter, 
            *this );
    
    if ( iCustomFilteredView )
        {
        delete iCustomFilteredView;
        iCustomFilteredView = NULL;
        }
    iCustomFilteredView = customFilteredView;
    customFilteredView = NULL;
    
    ConstructBaseViewsL( aViewDefinition, *iCustomFilteredView, 
            aViewSortOrder );
            
    iCustomFilteredView->ConstructL
        ( aViewDefinition, *this, *iSortOrder, *iView );

    iView = &iCustomFilteredView->ContactViewBase();
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::DoTeardownView
// --------------------------------------------------------------------------
//
void CFilteredGroupView::DoTeardownView()
    {
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::DoChangeSortOrderL
// --------------------------------------------------------------------------
//
TBool CFilteredGroupView::DoChangeSortOrderL( 
        const CVPbkContactViewDefinition& aViewDefinition,
		RContactViewSortOrder& aSortOrder )
    {
    TBool canBeChanged = ETrue;
    if ( iRemoteView )
        {
        if ( RemoteViewName( aViewDefinition ).Compare( 
                KVPbkAllGroupsViewName ) == 0 )
            {
            // Set Contacts Model default view setting only if client
            // is using KVPbkAllGroupsViewName shared view
            NamedRemoteViewViewDefinitionStoreUtility::
                SetNamedRemoteViewViewDefinitionL(
                    KVPbkAllContactsViewName, aSortOrder, 
                    KVPbkDefaultContactViewPrefs);
            }
        iRemoteView->ChangeSortOrderL( aSortOrder );
        }
    else
        {
        // CContactLocalView doesn't support ChangeSortOrderL
        canBeChanged = EFalse;
        }
        
    return canBeChanged;
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ContactViewReady
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ContactViewReady(
        MVPbkContactViewBase& aView )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredGroupView::ContactViewReady(0x%x)"), &aView);

    VPbkEng::SendViewEventToObservers( *this, iFilteringObservers,
        &MFilteredViewSupportObserver::ContactViewReadyForFiltering,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, iObservers,
        &MVPbkContactViewObserver::ContactViewReady,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, iFilteringObservers,
        &MVPbkContactViewObserver::ContactViewReady,
        &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ContactViewUnavailable
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ContactViewUnavailable(
        MVPbkContactViewBase& aView )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredGroupView::ContactViewUnavailable(0x%x)"), &aView);

    VPbkEng::SendViewEventToObservers( *this, iFilteringObservers,
        &MFilteredViewSupportObserver::ContactViewUnavailableForFiltering,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, iObservers,
        &MVPbkContactViewObserver::ContactViewUnavailable,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, iFilteringObservers,
        &MVPbkContactViewObserver::ContactViewUnavailable,
        &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ContactAddedToView
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ContactAddedToView(
        MVPbkContactViewBase& aView,
        TInt aIndex, const MVPbkContactLink& aContactLink )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredGroupView::ContactAddedToView(0x%x)"), &aView);

    VPbkEng::SendViewEventToObservers( *this, aIndex, aContactLink, 
        iObservers,
        &MVPbkContactViewObserver::ContactAddedToView,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, aIndex, aContactLink, 
        iFilteringObservers,
        &MVPbkContactViewObserver::ContactAddedToView,
        &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ContactRemovedFromView
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ContactRemovedFromView(
        MVPbkContactViewBase& aView,
        TInt aIndex, const MVPbkContactLink& aContactLink )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredGroupView::ContactRemovedFromView(0x%x)"), &aView);

    VPbkEng::SendViewEventToObservers( *this, aIndex, aContactLink, 
        iObservers,
        &MVPbkContactViewObserver::ContactRemovedFromView,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendViewEventToObservers( *this, aIndex, aContactLink, 
        iFilteringObservers,
        &MVPbkContactViewObserver::ContactRemovedFromView,
        &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ContactViewError
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ContactViewError(
        MVPbkContactViewBase& aView,
        TInt aError, TBool aErrorNotified )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredGroupView::ContactViewError(0x%x)"), &aView);

    VPbkEng::SendEventToObservers( *this, aError, aErrorNotified, iObservers,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendEventToObservers( *this, aError, aErrorNotified, 
        iFilteringObservers, &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredGroupView::ConstructBaseViewsL
// --------------------------------------------------------------------------
//
void CFilteredGroupView::ConstructBaseViewsL
        ( const CVPbkContactViewDefinition& aViewDefinition,
          MContactViewObserver& aObserver,
          RContactViewSortOrder& aViewSortOrder )
    {
    iNativeObserver = &aObserver;

    // Construct the all contacts view first
    if ( RemoteViewDefinition( aViewDefinition ) )
        {
        iRemoteView = CContactNamedRemoteView::NewL( 
                *iNativeObserver, RemoteViewName( aViewDefinition ), 
                Store().NativeDatabase(), aViewSortOrder, 
                KVPbkDefaultGroupsViewPrefs );
                
        iBaseView = iRemoteView;
        }
    else
        {
        iBaseView = CContactLocalView::NewL( *iNativeObserver, 
                Store().NativeDatabase(), aViewSortOrder, 
                KVPbkDefaultGroupsViewPrefs );
        }
   
    // Set base class view pointer
    iView = iBaseView;
    }

} // namespace VPbkCntModel

// End of File
