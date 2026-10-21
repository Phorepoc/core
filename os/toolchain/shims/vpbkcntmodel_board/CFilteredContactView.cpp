// PHOREPOC board delta (X4.84): the release file with one fix -- the destructor's double close of the native view
// when a custom filtered view exists (see ~CFilteredContactView). Everything else unchanged.
/*
* Copyright (c) 2002-2007 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Contacts Model store filtered contact view implementation.
*
*/


// INCLUDES
#include "CFilteredContactView.h"

// VPbkCntModel
#include "CContactStore.h"
#include "CFieldFactory.h"
#include "CCustomFilteredContactView.h"
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
#include <VPbkPrivateUid.h>    // KFindPluingUID
#include <CVPbkSortOrder.h>

// VPbkEngUtils
#include <RLocalizedResourceFile.h>

// System includes
#include <cntview.h>
#include <barsread.h>

// Debugging headers
#include <VPbkDebug.h>

namespace VPbkCntModel {

// CONSTANTS

_LIT( KResourceFile, "VPbkCntModelRes.rsc" );

/// Field type filter mapping structure
struct TVPbkFieldTypeFilterMapping
    {
    /// Virtual Phonebook field type resource id
    TUint iFieldTypeResourceId;
    /// Contacst Model native filter matching the field type
    TUint iNativeFilter;
    };

/// Last element marker
const TUint KLastElement = 0;

/// Array of field type filter mappings
static const TVPbkFieldTypeFilterMapping FilterMapping[] =
    {
        { R_CNTMODEL_NATIVE_FILTER_EMAIL_ADDRESS,
          CContactDatabase::EMailable },

        { R_CNTMODEL_NATIVE_FILTER_PHONE_NUMBER,
          CContactDatabase::EPhonable },

        { R_CNTMODEL_NATIVE_FILTER_SMS_ADDRESS,
          CContactDatabase::ESmsable },

        { R_CNTMODEL_NATIVE_FILTER_FAX_NUMBER,
          CContactDatabase::EFaxable },

        { R_CNTMODEL_NATIVE_FILTER_RINGING_TONE,
          CContactDatabase::ERingTone },

        { R_CNTMODEL_NATIVE_FILTER_ASSISTANT_PHONE_NUMBER,
          CContactDatabase::EPhonable },

        { R_CNTMODEL_NATIVE_FILTER_TOP_CONTACT,
          CContactDatabase::ECustomFilter2 },

        /// This has to be the last
        { KLastElement,
          CContactDatabase::EUnfiltered }
    };


/**
 * Appends native filter aFilter based on the given field type resource id.
 *
 * @param aFieldTypeResourceId  Field type resource id.
 * @param aFilter               Native Contacts Model filter.
 */
inline void BuildNativeFilter( const TInt aFieldTypeResourceId,
        TUint& aFilter )
    {
    for ( TInt i=0;
            FilterMapping[i].iFieldTypeResourceId != KLastElement; ++i )
        {
        if ( aFieldTypeResourceId == FilterMapping[i].iFieldTypeResourceId )
            {
            aFilter |= FilterMapping[i].iNativeFilter;
            break;
            }
        }
    }

/**
 * Checks wheter custom filtering is needed. This is the case
 * when the native filters cannot provide all the necessary filtering.
 *
 * @param aFieldType    The field type which matched selectors field type
 *                      collection.
 * @param aFilterType   The filter type which matched aFieldType, or NULL.
 * @param aSelector     The field type selector, the filtering criteria.
 * @return  ETrue if custom filtering is needed.
 */
TBool RequiresCustomFiltering(
        const MVPbkFieldType& aFieldType,
        const MVPbkFieldType* aFilterType,
        const CVPbkFieldTypeSelector& aSelector )
    {
    TBool ret = ETrue;

    if ( aFilterType )
        {
        const TVPbkFieldVersitProperty* matchingProperty =
            aSelector.MatchingVersitProperty( aFieldType );
        if ( matchingProperty )
            {
            const TInt maxMatchPriority =
                aFilterType->VersitProperties().Count() - 1;
            for ( TInt matchPriority = 0;
                 matchPriority <= maxMatchPriority; ++matchPriority )
                {
                if ( matchingProperty->Matches(
                    aFilterType->VersitProperties()[matchPriority]) )
                    {
                    ret = EFalse;
                    break;
                    }
                }
            }
        else
            {
            TVPbkNonVersitFieldType matchingType =
                aSelector.MatchingNonVersitType( aFieldType );
            if ( aFieldType.NonVersitType() == matchingType )
                {
                ret = EFalse;
                }
            }
        }

    return ret;
    }

/**
 * Checks does the given field type match any of the native filter types.
 *
 * @param aFieldType                The field type to inspect.
 * @param aSupportedFilterTypes     Field types which are supported
 *                                  by the native filters.
 * @return  Native filter field type which matched aFieldType,
 *          or NULL if no matches.
 */
inline const MVPbkFieldType* MatchesNativeFilter
    ( const MVPbkFieldType& aFieldType,
      const CVPbkFieldTypeList& aSupportedFilterTypes )

    {
    const MVPbkFieldType* matchingType = NULL;

    const TInt maxMatchPriority =
        aFieldType.VersitProperties().Count() - 1;

    for ( TInt matchPriority = 0;
         matchPriority <= maxMatchPriority && !matchingType; ++matchPriority )
        {
        matchingType = aSupportedFilterTypes.FindMatch(
            aFieldType.VersitProperties()[matchPriority],
                matchPriority );
        }

    if ( !matchingType && aFieldType.NonVersitType() != EVPbkNonVersitTypeNone )
        {
        matchingType = aSupportedFilterTypes.FindMatch
            ( aFieldType.NonVersitType() );
        }

    return matchingType;
    }

/**
 * Converts VPbk field type filter to a native filter.
 *
 * @param aSource                   Field type filter to convert.
 * @param aFieldTypeList            List of supported field types
 *                                  of the store.
 * @param aFs                       File server session.
 * @param aCustomFilteringNeeded    Indicates whether the native filters
 *                                  are enough, or do we have to do custom
 *                                  filtering.
 * @return  Native Contacts model contact view filter.
 */
TUint ConvertFieldTypeFilterL
        ( const CVPbkFieldTypeSelector& aSource,
        const MVPbkFieldTypeList& aFieldTypeList,
        RFs& aFs, TBool& aCustomFilteringNeeded  )
    {
    TUint ret = CContactDatabase::EUnfiltered;

    // Build own field type list containing field types supported
    // by native filters
    VPbkEngUtils::RLocalizedResourceFile resFile;
    resFile.OpenLC( aFs,
        KVPbkRomFileDrive, KDC_RESOURCE_FILES_DIR, KResourceFile );

    TResourceReader reader;
    reader.SetBuffer( resFile.AllocReadLC
        ( R_CNTMODEL_NATIVE_FILTER_FIELD_TYPES ) );

    CVPbkFieldTypeList* supportedFilterTypes =
        CVPbkFieldTypeList::NewL( resFile, reader );
    CleanupStack::PushL( supportedFilterTypes );

    // Go through the store's field type list
    TInt typeCount = aFieldTypeList.FieldTypeCount();
    for ( TInt i=0; i < typeCount; ++i )
        {
        const MVPbkFieldType& fieldType =
            aFieldTypeList.FieldTypeAt( i );

        if ( aSource.IsFieldTypeIncluded( fieldType ) )
            {
            // We have a match. Now lets inspect does it match
            // any of the native filter field types
            const MVPbkFieldType* matchingType =
                MatchesNativeFilter( fieldType, *supportedFilterTypes );

            if ( matchingType )
                {
                // We found a match to a native filter, lets build
                // the native filter
                BuildNativeFilter( matchingType->FieldTypeResId(), ret );
                }

            // Then, find out do we need to custom filtering
            TBool customFilteringRequired = RequiresCustomFiltering
                ( fieldType, matchingType, aSource );
            aCustomFilteringNeeded = customFilteringRequired;

            if ( !matchingType && customFilteringRequired )
                {
                // Custom filtering is required and there is no appropriate
                // native filter which could be used as a base filter.
                // In this case the custom filter has to handle all
                // the filtering. So reset the native filter and break out.
                ret = CContactDatabase::EUnfiltered;
                break;
                }
            }
        }

    CleanupStack::PopAndDestroy( 3 ); // supportedFilterTypes,
                                      // reader, resFile

    return ret;
    }

// --------------------------------------------------------------------------
// CFilteredContactView::CFilteredContactView
// --------------------------------------------------------------------------
//
inline CFilteredContactView::CFilteredContactView
        ( CContactStore& aParentStore,
          RFs& aFs ) :
            CViewBase( aParentStore ),
            iFs( aFs )
    {
    }

// --------------------------------------------------------------------------
// CFilteredContactView::~CFilteredContactView
// --------------------------------------------------------------------------
//
CFilteredContactView::~CFilteredContactView()
    {
    // PHOREPOC fix (X4.84): the custom filtered view is a CViewBase whose iView IS our native view (iFilteredView,
    // or iBaseView when there is no native filter -- ConstructL passes *iView); ~CViewBase closes it with the
    // custom view as observer -- the only one it was opened with -> the native view is deleted there. Closing it
    // again below was a use-after-free (Phonebook 2 panicked KERN-EXEC 3 at every switch-off).
    const TBool nativeClosedByCustom = ( iCustomFilteredView != NULL );

    // Customically filtered view has to be destructed first
    delete iCustomFilteredView;
    delete iFilter;

    // Set iView to NULL, so that CViewBase does not close it
    iView = NULL;

    if ( nativeClosedByCustom )
        {
        // already closed (and deleted) by the custom view
        }
    else if ( iFilteredView )
        {
        //after iFilteredView->Close( *iNativeObserver )
        //both iFilteredView and iBaseView are deleted
        iFilteredView->Close( *iNativeObserver );
        }
    else if ( iBaseView )
        {
        //but iBaseView's pointer is still not NULL
        //make sure iBaseView is unable to call its member function
        iBaseView->Close( *iNativeObserver );
        }
    }

// --------------------------------------------------------------------------
// CFilteredContactView::NewLC
// --------------------------------------------------------------------------
//
CFilteredContactView* CFilteredContactView::NewLC(
        const CVPbkContactViewDefinition& aViewDefinition,
        MVPbkContactViewObserver& aObserver,
        CContactStore& aParentStore,
        const MVPbkFieldTypeList& aSortOrder,
        RFs& aFs )
    {
    CFilteredContactView* self = new ( ELeave ) CFilteredContactView
        ( aParentStore, aFs );
    CleanupStack::PushL( self );
    self->ConstructL( aViewDefinition, aObserver, aSortOrder );
    return self;
    }

// --------------------------------------------------------------------------
// CFilteredContactView::ConstructL
// --------------------------------------------------------------------------
//
void CFilteredContactView::ConstructL(
        const CVPbkContactViewDefinition& aViewDefinition,
        MVPbkContactViewObserver& aObserver,
        const MVPbkFieldTypeList& aSortOrder )
    {
    CViewBase::ConstructL( aViewDefinition, aObserver, aSortOrder );
    }

// --------------------------------------------------------------------------
// CFilteredContactView::Type
// --------------------------------------------------------------------------
//
TVPbkContactViewType CFilteredContactView::Type() const
    {
    return EVPbkContactsView;
    }

// --------------------------------------------------------------------------
// CFilteredContactView::DoInitializeViewL
// --------------------------------------------------------------------------
//
void CFilteredContactView::DoInitializeViewL(
        const CVPbkContactViewDefinition& aViewDefinition,
        RContactViewSortOrder& aViewSortOrder )
    {
    // Construct the native filter
    if ( aViewDefinition.FieldTypeFilter() )
        {
        const CFieldFactory& fieldFactory = Store().FieldFactory();

        // Copy construct the filter
        CVPbkFieldTypeSelector* filter = CVPbkFieldTypeSelector::NewL(
                *aViewDefinition.FieldTypeFilter() );
        if ( iFilter )
            {
            delete iFilter;
            iFilter = NULL;
            }
        
        iFilter = filter;
        filter = NULL;
        
        // whenever a new iFilter created, set it as new fieldTypeSelector
        // iCustomFilteredView, to avoid such case that new iFilter created
        // but iCustomFilteredView is still the old one using the old iFilter,
        // it will panic since the old iFilter is deleted.
        if ( iCustomFilteredView )
            {
            iCustomFilteredView->SetFieldTypeSelector( iFilter );
            }
        
        iNativeFilter = ConvertFieldTypeFilterL
            ( *iFilter, fieldFactory, iFs, iCustomFilteringNeeded );
        }
    if ( aViewDefinition.ContactSelector() )
        {
        iCustomFilteringNeeded = ETrue;
        // Custom filtering and MVPbkContactSelector filtering cannot be used at the same time.
        iNativeFilter = 0; // no filter
        }
    if ( iCustomFilteringNeeded )
        {
        // Stop observing the base view, the custom view
        // will observe it and report back
        // Construction of the iCustomFilteredView should be done in two
        // phases. Due to that there is dependencies between views in this
        // and iCustomFilteredView class.

        CCustomFilteredContactView* customFilteredView = new (ELeave) CCustomFilteredContactView( Store(), iFilter,
                *this, aViewDefinition.ContactSelector() );
        
        if ( iCustomFilteredView )
            {
            delete iCustomFilteredView;
            iCustomFilteredView = NULL;
            }
        iCustomFilteredView = customFilteredView;
        customFilteredView = NULL;
        
        ConstructBaseViewsL( aViewDefinition, *iCustomFilteredView,
                aViewSortOrder );

        if ( iFilteredView )
            {
            iBaseView->Close( *iCustomFilteredView );
            }

        iCustomFilteredView->ConstructL
            ( aViewDefinition, *this, *iSortOrder, *iView );

        iView = &iCustomFilteredView->ContactViewBase();
        }
    else
        {
        ConstructBaseViewsL( aViewDefinition, *this, aViewSortOrder );

        // If we built Contact's Model's native filtered view we must
        // stop observing the base view since otherwise we get double
        // view events and VPbk composite view does not tolerate double
        // events
        if ( iFilteredView )
            {
            iBaseView->Close( *this );
            }
        }
    }

// --------------------------------------------------------------------------
// CFilteredContactView::DoTeardownView
// --------------------------------------------------------------------------
//
void CFilteredContactView::DoTeardownView()
    {
    }

// --------------------------------------------------------------------------
// CFilteredContactView::DoChangeSortOrderL
// --------------------------------------------------------------------------
//
TBool CFilteredContactView::DoChangeSortOrderL(
        const CVPbkContactViewDefinition& aViewDefinition,
		RContactViewSortOrder& aSortOrder )
    {
    TBool canBeChanged = ETrue;
    if ( iRemoteView )
        {
        if ( RemoteViewName( aViewDefinition ).Compare(
                KVPbkAllContactsViewName ) == 0 )
            {
            // Set Contacts Model default view setting only if client
            // is using KVPbkAllContactsViewName shared view.
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
// CFilteredContactView::ContactViewReady
// --------------------------------------------------------------------------
//
void CFilteredContactView::ContactViewReady(
        MVPbkContactViewBase& aView )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredContactView::ContactViewReady(0x%x)"), &aView);

    /* when custom filtering view is used, MVPbkContactViewObserver doesn't support
     * sortOrder changed notification, CViewBase will send viewReady instead,
     * use it to keep sort order up to date */
    if ( iCustomFilteringNeeded )
        {
        DoUpdateTypeListL();
        }

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
// CFilteredContactView::ContactViewUnavailable
// --------------------------------------------------------------------------
//
void CFilteredContactView::ContactViewUnavailable(
        MVPbkContactViewBase& aView )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredContactView::ContactViewUnavailable(0x%x)"), &aView);

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
// CFilteredContactView::ContactAddedToView
// --------------------------------------------------------------------------
//
void CFilteredContactView::ContactAddedToView(
        MVPbkContactViewBase& aView,
        TInt aIndex, const MVPbkContactLink& aContactLink )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredContactView::ContactAddedToView(0x%x)"), &aView);

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
// CFilteredContactView::ContactRemovedFromView
// --------------------------------------------------------------------------
//
void CFilteredContactView::ContactRemovedFromView(
        MVPbkContactViewBase& aView,
        TInt aIndex, const MVPbkContactLink& aContactLink )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredContactView::ContactRemovedFromView(0x%x)"), &aView);

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
// CFilteredContactView::ContactViewError
// --------------------------------------------------------------------------
//
void CFilteredContactView::ContactViewError(
        MVPbkContactViewBase& aView,
        TInt aError, TBool aErrorNotified )
    {
    VPBK_DEBUG_PRINT(VPBK_DEBUG_STRING
        ("CFilteredContactView::ContactViewError(0x%x)"), &aView);

    VPbkEng::SendEventToObservers( *this, aError, aErrorNotified, iObservers,
        &MVPbkContactViewObserver::ContactViewError );
    VPbkEng::SendEventToObservers( *this, aError, aErrorNotified,
        iFilteringObservers, &MVPbkContactViewObserver::ContactViewError );
    }

// --------------------------------------------------------------------------
// CFilteredContactView::ContactViewObserverExtension
// --------------------------------------------------------------------------
//
TAny* CFilteredContactView::ContactViewObserverExtension( TUid aExtensionUid )
    {
    if( aExtensionUid == KVPbkContactViewObserverExtension2Uid )
        {
        return static_cast<MVPbkContactViewObserverExtension*>( this );
        }
    return NULL;
    }

// --------------------------------------------------------------------------
// CFilteredContactView::FilteredContactRemovedFromView
// --------------------------------------------------------------------------
//
void CFilteredContactView::FilteredContactRemovedFromView(
		MVPbkContactViewBase& aView )
    {
    const TInt count = iObservers.Count();

    for( TInt i = 0; i < count; i++ )
        {
        MVPbkContactViewObserver* observer = iObservers[i];

        TAny* extension = observer->ContactViewObserverExtension(
              KVPbkContactViewObserverExtension2Uid );

        if( extension )
            {
            MVPbkContactViewObserverExtension* contactViewExtension =
                  static_cast<MVPbkContactViewObserverExtension*>( extension );

            if( contactViewExtension )
                {
                contactViewExtension->FilteredContactRemovedFromView( aView );
                }
            }
        }
    }

// --------------------------------------------------------------------------
// CFilteredContactView::ConstructBaseViewsL
// --------------------------------------------------------------------------
//
void CFilteredContactView::ConstructBaseViewsL
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
                KVPbkDefaultContactViewPrefs );

        iBaseView = iRemoteView;
        }
    else
        {
     
        iBaseView = CContactLocalView::NewL( *iNativeObserver,
                Store().NativeDatabase(), aViewSortOrder,
                KVPbkDefaultContactViewPrefs );
        }

    // Set base class view pointer
    iView = iBaseView;

    // If there is a native filter, construct a native filtered view
    if ( iNativeFilter > 0 )
        {
        iFilteredView = CContactFilteredView::NewL
            ( *iNativeObserver, Store().NativeDatabase(),
            *iBaseView, iNativeFilter );
        // Set find plugin also for BaseView, otherwise searching doesn't
        // work properly with other languages in other clients.
        iView->SetViewFindConfigPlugin( TUid::Uid( KFindPluginUID ) );
        iView = iFilteredView;
        }
    }

} // namespace VPbkCntModel

// End of File
