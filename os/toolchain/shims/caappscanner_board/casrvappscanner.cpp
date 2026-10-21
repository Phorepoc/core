/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Description: casrvappscanner.cpp
*
*/

#include <e32property.h>
#include <bautils.h>
#include <usif/scr/screntries.h>

#include "cadef.h"
#include "casrvappscanner.h"
#include "cautils.h"
#include "casrvmmchistory.h"
#include "pathinfo.h"
#include "casrvengutils.h"
#include "cainnerquery.h"
#include "cainnerentry.h"
#include "caarraycleanup.inl"
#include "castorageproxy.h"

#include "caarraycleanup.inl"

using namespace Usif;



// ==================== LOCAL FUNCTIONS ====================

/**
 * Identity function to search in an array of CCaInnerEntry.
 * Identity is the ID.
 * @param aLeft Search term.
 * @param aRight Array item.
 * @return ETrue if ID-s match.
 */
LOCAL_C TBool IdMatch( const CCaInnerEntry& aLeft,
        const CCaInnerEntry& aRight )
    {
    return aLeft.GetId() == aRight.GetId();
    }

/**
 * Identity function to search in an array of CCaInnerEntry.
 * Identity is the ID.
 * @param aLeft Search term.
 * @param aRight Array item.
 * @return ETrue if ID-s match.
 */
LOCAL_C TBool UidMatch( const CCaInnerEntry& aLeft,
        const CCaInnerEntry& aRight )
    {
    return aLeft.GetUid() == aRight.GetUid();
    }

// ==================== MEMBER FUNCTIONS ====================

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
CCaSrvAppScanner* CCaSrvAppScanner::NewL( CCaStorageProxy& aCaStorageProxy,
        Usif::RSoftwareComponentRegistry& aSoftwareRegistry,
        CCaSrvEngUtils& aUtils )
    {
    CCaSrvAppScanner* scanner = new ( ELeave ) CCaSrvAppScanner(
            aCaStorageProxy, aSoftwareRegistry, aUtils );
    CleanupStack::PushL( scanner );
    scanner->ConstructL();
    CleanupStack::Pop( scanner );
    return scanner;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
CCaSrvAppScanner::~CCaSrvAppScanner()
    {
    Cancel();
    iApaLsSession.Close();
    delete iMmcHistory;
    iFs.Close();
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
CCaSrvAppScanner::CCaSrvAppScanner( CCaStorageProxy& aCaStorageProxy,
        Usif::RSoftwareComponentRegistry& aSoftwareRegistry,
        CCaSrvEngUtils& aUtils ) :
    CActive( CActive::EPriorityStandard ),
    iCaStorageProxy( aCaStorageProxy ),
    iSoftwareRegistry(aSoftwareRegistry), iSrvEngUtils( aUtils )
    {
    CActiveScheduler::Add( this );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::ConstructL()
    {
    User::LeaveIfError( iFs.Connect() );
    TFileName path;
    User::LeaveIfError( iFs.PrivatePath( path ) );
    TUint attribute;
    if( iFs.Att( path, attribute ) == KErrNotFound )
        {
        TInt mdRes = iFs.MkDirAll( path );
        if ( mdRes != KErrNone )
            {
            User::Leave( mdRes );
            }
        }
    iMmcHistory = CCaSrvMmcHistory::NewL();
    iMmcHistory->LoadL( iFs, KCaMmcHistoryFname() );

    User::LeaveIfError( iApaLsSession.Connect() );
    iApaLsSession.SetNotify( EFalse, iStatus );
    iCollectionDownloadId = 0;
    iAllCollectionId = 0;

    UpdateApplicationEntriesL();
    MakeNotEmptyCollectionsVisibleL();

    SetActive();
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::RunL()
    {
    User::LeaveIfError( iStatus.Int() );

#ifdef PHOREPOC_S3_APPARC
    // PHOREPOC board delta (S4.5): our AppArc is Symbian^3's, which has no per-app update list
    // (TApaAppUpdateInfo / RApaLsSession::UpdatedAppsInfoL came with Symbian^4's). On each AppArc change
    // notification this package's own earlier revision (201023) rescanned every application; do that:
    // UpdateApplicationEntriesL adds, updates and removes (HandleMissingItemsL) against the full app list.
    UpdateApplicationEntriesL();
    MakeNotEmptyCollectionsVisibleL();
#else
    // AppArc app scan complete, we have the app list.
    RArray<TApaAppUpdateInfo> updatedAppsInfo;
    CleanupClosePushL( updatedAppsInfo );
    iApaLsSession.UpdatedAppsInfoL( updatedAppsInfo );

    for( TInt i = 0; i < updatedAppsInfo.Count(); i++ )
    	{
		TApaAppUpdateInfo::TApaAppAction action = updatedAppsInfo[i].iAction;
        RPointerArray<CCaInnerEntry> resultArray;
        CleanupResetAndDestroyPushL( resultArray );
        GetCaAppEntriesL( updatedAppsInfo[i].iAppUid.iUid, resultArray );
		if( action == TApaAppUpdateInfo::EAppPresent
				|| action == TApaAppUpdateInfo::EAppInfoChanged )
			{
			TInt id;
			if( !resultArray.Count() )
				{
                id = AddAppEntryL(
                        updatedAppsInfo[i].iAppUid.iUid, UpdateMmcHistoryL() );
                AddEntryToDownloadedCollectionL( id );
				}
			else
			    {
                ASSERT( resultArray.Count() == 1 );
                id = resultArray[0]->GetId();
                UpdateAppEntryL( resultArray[0], UpdateMmcHistoryL(), ETrue );
                }
            MakeNotEmptyCollectionsVisibleL();
			}
		else if( action == TApaAppUpdateInfo::EAppNotPresent )
			{
			HandleMissingItemsL( resultArray );
			}
		else
			{
			User::Leave( KErrNotSupported );
			}
        CleanupStack::PopAndDestroy( &resultArray );
    	}
    CleanupStack::PopAndDestroy( &updatedAppsInfo );
#endif

    iApaLsSession.SetNotify( EFalse, iStatus );
    SetActive();
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::DoCancel()
    {
    iApaLsSession.CancelNotify();
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::RunError( TInt /*aError*/)
    {
    // Ignore the error (what else could we do?).
    // When next update occurs, we will run again.
    return KErrNone;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::UpdateApplicationEntryL(
        RPointerArray<CCaInnerEntry>& aCaEntries,
        TUint aAppUid, TUint aMmcId )
    {
    RPointerArray<CCaInnerEntry> resultArray;
    CleanupResetAndDestroyPushL( resultArray );
    GetCaAppEntriesL( aAppUid, resultArray );

    // This app is not in the storage, add it now.
    // We don't add hidden items, there are too many of them!
    // do not display Menu app
    if( !resultArray.Count() )
        {
        AddAppEntryL( aAppUid, aMmcId );
        }//if

    // "removable", "missing" and "visible"  flags update
    for( TInt j = 0; j < resultArray.Count(); j++ )
        {
        //We need to handle first run of appscanner,
        //there might be some incorrect data. If this will have impact
        //on performance we may run this methods only at start up.

        UpdateAppEntryL( resultArray[j], aMmcId );
        TInt index = aCaEntries.Find( resultArray[j],
                TIdentityRelation<CCaInnerEntry>( IdMatch ) );
        if( index != KErrNotFound )
            {
            delete aCaEntries[index];
            aCaEntries.Remove( index );
            }
        }//for
    CleanupStack::PopAndDestroy( &resultArray );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::UpdateAppEntryL(
        CCaInnerEntry* aEntry, TUint aMmcId, TBool aAlwaysUpdate )
    {
    TBool missingFlagChanged = HandleMissingFlagsUpdate( aEntry );
    TBool visibleFlagChanged = HandleVisibleFlagUpdate( aEntry );

    // To handle case with mmc card. When mmc attritube is updated then
    // used flag should be change. When aplication is update but
    // it is not appear then this flag should be change too.
    if( aAlwaysUpdate && !missingFlagChanged )
        {
        HandleUsedFlagUpdate( aEntry );
        }
    TBool toUpdate = missingFlagChanged || visibleFlagChanged;
    toUpdate = SetApaAppInfoL( aEntry ) || toUpdate;
    toUpdate = HandleRemovableFlagAndMmcAttrUpdateL(
            aEntry, aMmcId ) || toUpdate;
    toUpdate = RemoveUninstallFlagL( aEntry ) || toUpdate;

    if( toUpdate || aAlwaysUpdate )
        {
        //update app in storage
        TItemAppearance itemAppearanceChange = EItemAppearanceNotChanged;
        if( missingFlagChanged || visibleFlagChanged )
            {
            itemAppearanceChange = EItemAppeared;
            }
        if( !missingFlagChanged )
            {
            AddEntryToPredefinedCollectionL( aEntry );
            if( aEntry->GetFlags() & ERemovable )
                {
                AddEntryToDownloadedCollectionL( aEntry->GetId() );
                }
            }
        iCaStorageProxy.AddL( aEntry, EFalse, itemAppearanceChange );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::HandleMmcAttrUpdateL(
        const Usif::CComponentEntry* aEntry,
        CCaInnerEntry* aItem, TUint aMmcId )
    {
    TBool toChange( EFalse );
    if( aEntry && aEntry->IsRemovable() )
        {
        TChar currentDriveLetter;
        if( aEntry->SoftwareType().Compare( KCaAttrAppTypeValueNative() ) != 0 )
            {
            TDriveList driveList = aEntry->InstalledDrives();
            for( TInt driveNr=EDriveY; driveNr >= EDriveA; driveNr-- )
                {
                if( driveList[driveNr] )
                    {
                    User::LeaveIfError( iFs.DriveToChar( driveNr,
                            currentDriveLetter ) );
                    }
                }
            }
        else
            {
            TApaAppInfo* appInfo = new ( ELeave ) TApaAppInfo();
            if( !iApaLsSession.GetAppInfo( *appInfo,
                    TUid::Uid( aItem->GetUid() ) ) )
                {
                currentDriveLetter = appInfo->iFullName[0];
                }
            }

        if( IsCharInDrive( currentDriveLetter,
                DriveInfo::EDefaultRemovableMassStorage ) )
            {
            //app is instaled on mmc - KCaAttrMmcId attribute update
            RBuf uidString;
            uidString.CleanupClosePushL();
            uidString.CreateL( KUidChars );
            uidString.Format( KHexFormat, aMmcId );
            toChange = AddAttributeL( aItem, KCaAttrMmcId, uidString );
            CleanupStack::PopAndDestroy( &uidString );
            }
        else if ( IsCharInDrive(
                currentDriveLetter, DriveInfo::EDefaultMassStorage ) )
            {
            //its app installed on mass storage, we need to leave it
            //in case of connecting usb in mass storage mode
            toChange = AddAttributeL( aItem, KCaAttrMmcId, KCaMassStorage );
            }
        else
            {
            toChange = RemoveAttributeL( aItem, KCaAttrMmcId );
            }
        }
    else
        {
        toChange = RemoveAttributeL( aItem, KCaAttrMmcId );
        //its installed on c: drive - remove attribute
        }
    return toChange;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::UpdateApplicationEntriesL()
    {
    TUint currentMmcId = UpdateMmcHistoryL();
    // get all Content arsenal enties with type application
    RPointerArray<CCaInnerEntry> resultArray;
    CleanupResetAndDestroyPushL( resultArray );
    GetCaAppEntriesL( resultArray );
    HandleHsAppEntryL( resultArray );
    RemoveSatAppL( resultArray );

    RArray<TUint> appUids;
    CleanupClosePushL( appUids );
    GetApaItemsL( appUids );
    RemoveApp( appUids, KSatUid.iUid );
    RemoveApp( appUids, KHsAppUid.iUid );

    //for every item in apaAndCrItems array
    for( TInt i = 0; i < appUids.Count(); i++ )
        {
        // if there was leave for any item we ignore it
        // and proceed to the next one
        TRAP_IGNORE(UpdateApplicationEntryL(
                resultArray, appUids[i], currentMmcId));
        }
    // Here the big list with items that refer to missing apps.
    HandleMissingItemsL( resultArray );
    CleanupStack::PopAndDestroy( &appUids );
    CleanupStack::PopAndDestroy( &resultArray );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::HandleRemovableFlagAndMmcAttrUpdateL(
        CCaInnerEntry* aItem, TUint aMmcId )
    {
    TBool toChange( EFalse );
    TComponentId compId = GetComponentIdFromEntryL( aItem );

    TBool isRemovable( EFalse );
    if( compId )
        {
        CComponentEntry* entry = CComponentEntry::NewLC();
        iSoftwareRegistry.GetComponentL( compId , *entry );
        isRemovable = entry->IsRemovable();
        toChange = HandleMmcAttrUpdateL( entry, aItem, aMmcId );
        CleanupStack::PopAndDestroy( entry );
        }

    if( ( aItem->GetFlags() & EVisible ) && !isRemovable )
        {
        if( ( aItem->GetFlags() & ERemovable ) != 0 )
            {
            aItem->SetFlags( aItem->GetFlags() & ~ERemovable );
            toChange = ETrue;
            }
        }
    else
        {
        if( ( aItem->GetFlags() & ERemovable ) == 0 && isRemovable )
            {
            aItem->SetFlags( aItem->GetFlags() | ERemovable );
            toChange = ETrue;
            }
        }
    return toChange;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::RemoveUninstallFlagL( CCaInnerEntry* aItem )
    {
    TBool toChange( EFalse );
    if( aItem->GetFlags() & EUninstall )
        {
		aItem->SetFlags( aItem->GetFlags() & ~EUninstall );
		toChange = ETrue;
        }
    return toChange;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::HandleMissingFlagsUpdate(
        CCaInnerEntry* aItem )
    {
    TBool ret( EFalse );
    if( aItem->GetFlags() & EMissing )
        {
        //application found so we unset "missing" flag
        aItem->SetFlags( aItem->GetFlags() & ~EMissing );
        ret = ETrue;
        }
    return ret;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::HandleVisibleFlagUpdate( CCaInnerEntry* aItem )
    {
    TBool toChange( EFalse );
    if( !( aItem->GetFlags() & EVisible ) )
        {
        aItem->SetFlags( aItem->GetFlags() | EVisible );
        toChange = ETrue;
        }
    return toChange;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::HandleUsedFlagUpdate( CCaInnerEntry* aItem )
    {
    TBool changed( EFalse );
    if( aItem->GetFlags() & EUsed )
        {
        aItem->SetFlags( aItem->GetFlags() & ~EUsed );
        changed = ETrue;
        }
    return changed;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::RemoveApp( RArray<TUint>& aArray, TInt32 aUid )
    {
    TInt id = aArray.Find( aUid );
    if( id != KErrNotFound )
        {
        aArray.Remove( id );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::RemoveSatAppL( RPointerArray<CCaInnerEntry>& aArray )
    {
    CCaInnerEntry* sat = CCaInnerEntry::NewL();
    sat->SetUid( KSatUid.iUid );
    TInt index = aArray.Find(
            sat, TIdentityRelation<CCaInnerEntry>( UidMatch ) );
    if ( index != KErrNotFound )
        {
        delete aArray[index];
        aArray.Remove( index );
        }
    delete sat;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::HandleHsAppEntryL(
        RPointerArray<CCaInnerEntry>& aArray )
    {
    CCaInnerEntry* appEntry = CCaInnerEntry::NewLC();
    appEntry->SetUid( KHsAppUid.iUid );
    TInt index = aArray.Find(
            appEntry, TIdentityRelation<CCaInnerEntry>( UidMatch ) );

    if ( index != KErrNotFound )
        {
        // hs app already in storage - ensure it is hidden
        // and remove from resultArray
        if ( aArray[index]->GetFlags() & EVisible )
            {
            aArray[index]->SetFlags( aArray[index]->GetFlags() & ~EVisible);
            iCaStorageProxy.AddL( aArray[index], EFalse, EItemDisappeared );
            }
        delete aArray[index];
        aArray.Remove( index );
        }
    else
        {
        // if not found add as not visible to the storage
        appEntry->SetEntryTypeNameL( KCaTypeApp );
        appEntry->SetFlags( 0 );
        appEntry->SetRole( EItemEntryRole );
        SetApaAppInfoL( appEntry );
#ifdef PHOREPOC_S3_APPARC
        // PHOREPOC board delta (S4.5): no S^4 homescreen application registered yet (S4.6) -> no AppArc info, an
        // empty caption, and the storage refuses the row (EN_TEXT NOT NULL: KSqlErrConstraint), which failed the
        // whole scanner and so every scan. Skip the hidden placeholder until that application exists.
        if ( appEntry->GetText().Length() )
#endif
        iCaStorageProxy.AddL( appEntry );
        }
    CleanupStack::PopAndDestroy( appEntry );
    }


// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::GetApaItemsL( RArray<TUint>& aArray )
    {
    TApaAppInfo* appInfo = new ( ELeave ) TApaAppInfo();
    CleanupStack::PushL(appInfo);
    TApaAppCapabilityBuf appCap;

    User::LeaveIfError( iApaLsSession.GetAllApps( 0 ) );
    // for every application get uid, hidden and missing attribute
    // and add to aArray.
    while( KErrNone == iApaLsSession.GetNextApp( *appInfo ) )
        {
        User::LeaveIfError( iApaLsSession.GetAppCapability(
                appCap, appInfo->iUid ) );
        // "Hidden" status according to AppArc.
        if( !appCap().iAppIsHidden )
            {
            aArray.AppendL( appInfo->iUid.iUid );
            }
        }
    CleanupStack::PopAndDestroy(appInfo);
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::GetCaAppEntriesL(
        RPointerArray<CCaInnerEntry>& aArray )
    {
    CCaInnerQuery* allAppQuery = CCaInnerQuery::NewLC();
    CDesC16ArrayFlat* appType =
            new ( ELeave ) CDesC16ArrayFlat( KGranularityOne );
    CleanupStack::PushL( appType );
    appType->AppendL( KCaTypeApp );
    allAppQuery->SetEntryTypeNames( appType );
    CleanupStack::Pop( appType );
    iCaStorageProxy.GetEntriesL( allAppQuery, aArray );
    CleanupStack::PopAndDestroy( allAppQuery );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::GetCollectionDownloadIdL()
    {
    if( iCollectionDownloadId == 0 )
        {
        RPointerArray<CCaInnerEntry> resultArray;
        CleanupResetAndDestroyPushL( resultArray );
        CCaInnerQuery* allAppQuery = CCaInnerQuery::NewLC();
        CDesC16ArrayFlat* appType =
                new ( ELeave ) CDesC16ArrayFlat( KGranularityOne );
        CleanupStack::PushL( appType );
        appType->AppendL( KCaTypeCollectionDownload );
        allAppQuery->SetEntryTypeNames( appType );
        CleanupStack::Pop( appType );
        iCaStorageProxy.GetEntriesL( allAppQuery, resultArray );
        CleanupStack::PopAndDestroy( allAppQuery );
        if( resultArray.Count() )
            {
            iCollectionDownloadId = resultArray[0]->GetId();
            }
        CleanupStack::PopAndDestroy( &resultArray );
        }
    return iCollectionDownloadId;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::GetAllCollectionIdL()
    {
    if( iAllCollectionId == 0 )
        {
        CCaInnerQuery *getAllCollectionIdQuery = CCaInnerQuery::NewLC();
        CDesC16ArrayFlat *typenameArray = new(ELeave) CDesC16ArrayFlat(
                KGranularityOne );
        CleanupStack::PushL( typenameArray );
        typenameArray->AppendL( KCaTypeMenuCollections );
        getAllCollectionIdQuery->SetEntryTypeNames( typenameArray );
        CleanupStack::Pop( typenameArray );

        RArray<TInt> idArray;
        CleanupClosePushL( idArray );
        iCaStorageProxy.GetEntriesIdsL( getAllCollectionIdQuery,
                idArray );
        if( idArray.Count() )
            {
            iAllCollectionId = idArray[0];
            }
        CleanupStack::PopAndDestroy( &idArray );
        CleanupStack::PopAndDestroy( getAllCollectionIdQuery );
        }
    return iAllCollectionId;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::GetCaAppEntriesL( TInt aUid,
        RPointerArray<CCaInnerEntry>& aArray )
    {
    CCaInnerQuery* allAppQuery = CCaInnerQuery::NewLC();
    allAppQuery->SetUid( aUid );
    iCaStorageProxy.GetEntriesL( allAppQuery, aArray );
    CleanupStack::PopAndDestroy( allAppQuery );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::AddEntryToDownloadedCollectionL( TInt aEntryId )
    {
    TCaOperationParams params;
    params.iOperationType = TCaOperationParams::EPrepend;
    params.iGroupId = GetCollectionDownloadIdL();
    params.iBeforeEntryId = 0; // Not used.

    RArray<TInt> entryIds;
    CleanupClosePushL( entryIds );
    entryIds.AppendL( aEntryId );
    iCaStorageProxy.OrganizeL( entryIds, params );

    CleanupStack::PopAndDestroy( &entryIds );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::AddEntryToPredefinedCollectionL(
        CCaInnerEntry* aEntry )
    {
    TApaAppCapabilityBuf capability;
    User::LeaveIfError( iApaLsSession.GetAppCapability( capability,
            TUid::Uid( aEntry->GetUid() ) ) );

    if( capability().iGroupName.Length() )
        {
        // appgroup_name is defined for this app. Find or create folder.
        CCaInnerQuery *innerQuery = CCaInnerQuery::NewLC();
        innerQuery->SetRole( CCaInnerQuery::Group );
        innerQuery->AddAttributeL( KCaAppGroupName,
                capability().iGroupName );

        // get entries by attributes
        RPointerArray<CCaInnerEntry> resultArrayItems;
        CleanupResetAndDestroyPushL( resultArrayItems );
        iCaStorageProxy.GetEntriesL( innerQuery, resultArrayItems );

        RArray<TInt> entryIds;
        CleanupClosePushL( entryIds );
        entryIds.AppendL( aEntry->GetId() );
        TCaOperationParams organizeParams;
        organizeParams.iBeforeEntryId = 0;
        organizeParams.iOperationType = TCaOperationParams::EAppend;

        if( resultArrayItems.Count() )
            {
            // collection with appgroup_name exist - add entry
            // to this collection
            organizeParams.iGroupId = resultArrayItems[0]->GetId();
            }
        else
            {
            // create new collection
            TInt predefinedCollectionId = CreatePredefinedCollectionL(
                    capability().iGroupName );

            organizeParams.iGroupId = predefinedCollectionId;

            // add new collection to all collection
            AddCollectionToAllCollectionL( predefinedCollectionId );
            }

        iCaStorageProxy.OrganizeL( entryIds, organizeParams );

        CleanupStack::PopAndDestroy( &entryIds );
        CleanupStack::PopAndDestroy( &resultArrayItems );
        CleanupStack::PopAndDestroy( innerQuery );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::CreatePredefinedCollectionL( const TDesC& aGroupName )
    {
    CCaInnerEntry *innerEntry = CCaInnerEntry::NewLC();
    innerEntry->SetTextL( aGroupName );
    innerEntry->SetEntryTypeNameL( KCaTypeCollection );
    innerEntry->SetRole( CCaInnerQuery::Group );
    innerEntry->AddAttributeL( KCaAppGroupName, aGroupName );
    innerEntry->SetFlags( EVisible | ERemovable );
    innerEntry->SetIconDataL( KCollectionIconFileName,
            KNullDesC, KNullDesC );
    iCaStorageProxy.AddL( innerEntry );
    // Get new collection Id
    TInt newCollectionId = innerEntry->GetId();
    CleanupStack::PopAndDestroy( innerEntry );

    return newCollectionId;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::AddCollectionToAllCollectionL( TInt aCollectionId )
    {
    RArray<TInt> entryIds;
    CleanupClosePushL( entryIds );
    entryIds.AppendL( aCollectionId );

    TCaOperationParams organizeParams;
    organizeParams.iBeforeEntryId = 0;
    organizeParams.iOperationType = TCaOperationParams::EAppend;
    organizeParams.iGroupId = GetAllCollectionIdL();
    iCaStorageProxy.OrganizeL( entryIds, organizeParams );
    CleanupStack::PopAndDestroy( &entryIds );
    }



// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::RemoveEntryFromDownloadedL( TInt aEntryId )
    {
    TCaOperationParams params;
    params.iOperationType = TCaOperationParams::ERemove;
    params.iGroupId = GetCollectionDownloadIdL();
    params.iBeforeEntryId = 0; // Not Used

    RArray<TInt> entryIds;
    CleanupClosePushL( entryIds );
    entryIds.AppendL( aEntryId );
    iCaStorageProxy.OrganizeL( entryIds, params );

    CleanupStack::PopAndDestroy( &entryIds );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::AddAppEntryL( TUint aUid, TUint aCurrentMmcId )
    {
    TInt id(KErrNotFound);
    // Now add the app entry.
    CCaInnerEntry* appEntry = CCaInnerEntry::NewLC();

    appEntry->SetEntryTypeNameL( KCaTypeApp );
    appEntry->SetUid( aUid );
    appEntry->SetFlags( EVisible );
    appEntry->SetRole( EItemEntryRole );

    SetApaAppInfoL( appEntry );
    HandleRemovableFlagAndMmcAttrUpdateL( appEntry, aCurrentMmcId );

    iCaStorageProxy.AddL( appEntry );
    id = appEntry->GetId();

    AddEntryToPredefinedCollectionL( appEntry );

    CleanupStack::PopAndDestroy( appEntry );
    return id;
    }

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//
TBool CCaSrvAppScanner::SetApaAppInfoL( CCaInnerEntry* aEntry )
    {
    TBool changed( EFalse );
    TApaAppInfo* info = new ( ELeave ) TApaAppInfo();
    CleanupStack::PushL(info);
    if( KErrNone == iSrvEngUtils.GetAppInfo( *aEntry, *info ) )
        {
        changed = SetAppCaptionL( aEntry, info ) || changed;
        changed = UpdateComponentIdL( *aEntry ) || changed;

        // check if its java app and add attr for entries
        TUid appTypeUid;
        if( KErrNone == iApaLsSession.GetAppType( appTypeUid, info->iUid ) )
            {
            if( appTypeUid == KMidletApplicationTypeUid )
                {
                changed = SetJavaAppL( aEntry ) || changed;
                }
            else if( appTypeUid == KCWRTApplicationTypeUid )
                {
                changed = SetCWRTAppL( aEntry ) || changed;
                }
            }

        TApaAppCapabilityBuf appCap;
        TInt screenNumber = 0;

        User::LeaveIfError( iApaLsSession.GetAppCapability
            ( appCap, info->iUid ) );
        User::LeaveIfError( iApaLsSession.GetDefaultScreenNumber
            ( screenNumber, info->iUid ) );

        const TBool hidden = appCap().iAppIsHidden || screenNumber != 0;

        const TBool visible = aEntry->GetFlags() & EVisible;

        if ( hidden && visible )
            {
            changed = ETrue;
            aEntry->SetFlags(aEntry->GetFlags() & ~EVisible);
            }
        }

    CleanupStack::PopAndDestroy( info );
    return changed;
    }

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//
TBool CCaSrvAppScanner::SetAppCaptionL(
        CCaInnerEntry* aEntry, TApaAppInfo* info )
    {
    TBool changed(EFalse);
    if( aEntry->GetText().Compare( info->iCaption ) != KErrNone )
        {
        aEntry->SetTextL( info->iCaption );
        changed = ETrue;
        }
    changed = AddAttributeL( aEntry, KCaAttrShortName, info->iShortCaption )
            || changed;

    return changed;
    }

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//
TBool CCaSrvAppScanner::SetJavaAppL( CCaInnerEntry* aEntry )
    {
    TBool changed = AddAttributeL(
            aEntry, KCaAttrAppType, KCaAttrAppTypeValueJava );
    TComponentId compId = GetComponentIdFromEntryL( aEntry );
    CPropertyEntry* propertyEntry = iSoftwareRegistry.GetComponentPropertyL(
            compId, KCaScrPropertyAppSettings );
    CleanupStack::PushL( propertyEntry );

    if( propertyEntry
            && propertyEntry->PropertyType()
                    == CPropertyEntry::ELocalizedProperty )
        {
        changed = AddAttributeL( aEntry, KCaAttrAppSettingsPlugin,
                static_cast<CLocalizablePropertyEntry*>(
                        propertyEntry)->StrValue() ) || changed;
        }
    CleanupStack::PopAndDestroy( propertyEntry );
    return changed;
    }

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
//
TBool CCaSrvAppScanner::SetCWRTAppL( CCaInnerEntry* aEntry )
    {
    TBool changed = AddAttributeL( aEntry, KCaAttrAppType, KCaAttrAppTypeValueCWRT );
    TComponentId compId = GetComponentIdFromEntryL( aEntry );

    CPropertyEntry* isMiniview = iSoftwareRegistry.GetComponentPropertyL(
            compId, KCaScrPropertyIsMiniviewSupported );
    CleanupStack::PushL( isMiniview );

    // check first if we support mini view
    if( isMiniview && isMiniview->PropertyType() == CPropertyEntry::EIntProperty
            && static_cast<CIntPropertyEntry*>( isMiniview)->IntValue() )
        {
        CPropertyEntry* appId = iSoftwareRegistry.GetComponentPropertyL(
                compId, KCaScrPropertyAppId );
        CleanupStack::PushL( appId );

        if ( appId
                && appId->PropertyType() == CPropertyEntry::ELocalizedProperty )
            {
            changed = AddAttributeL(
                    aEntry, KCaAttrAppWidgetUri, KCaAttrAppWidgetUriCWRTValue )
                        || changed;
            changed = AddAttributeL( aEntry, KCaAttrAppWidgetParamWebAppId,
                    static_cast<CLocalizablePropertyEntry*>(
                            appId)->StrValue() ) || changed;
            }
        CleanupStack::PopAndDestroy( appId );
        }
    CleanupStack::PopAndDestroy( isMiniview );
    return changed;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::AddAttributeL( CCaInnerEntry* aEntry, const TDesC& aKey,
        const TDesC& aValue )
    {
    TBool changed(EFalse);
    RBuf attrValue;
    attrValue.CleanupClosePushL();
    attrValue.CreateL( KCaMaxAttrValueLen );

    TBool found = aEntry->FindAttribute( aKey, attrValue );
    if( !( found && attrValue.Compare( aValue ) == KErrNone ) )
        {
        aEntry->AddAttributeL( aKey, aValue );
        changed = ETrue;
        }
    CleanupStack::PopAndDestroy( &attrValue );
    return changed;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::RemoveAttributeL( CCaInnerEntry* aEntry,
        const TDesC& aKey )
    {
    TBool toChange( EFalse );
    RBuf attrVal;
    attrVal.CleanupClosePushL();
    attrVal.CreateL( KCaMaxAttrValueLen );
    if( aEntry->FindAttribute( KCaAttrMmcId, attrVal ) )
        {
        aEntry->RemoveAttributeL( aKey );
        toChange = ETrue;
        }
    CleanupStack::PopAndDestroy( &attrVal );
    return toChange;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::HandleMissingItemsL(
        RPointerArray<CCaInnerEntry>& aCaEntries )
    {
    for( TInt i = 0; i < aCaEntries.Count(); i++ )
        {
        RBuf attrVal;
        attrVal.CleanupClosePushL();
        attrVal.CreateL( KCaMaxAttrValueLen );
        if( aCaEntries[i]->FindAttribute( KCaAttrMmcId(), attrVal ) )
            {
            TUint mmcId = 0;
            MenuUtils::GetTUint( attrVal, mmcId );
            if( ( mmcId && KErrNotFound != iMmcHistory->Find( mmcId )
                        && mmcId != CurrentMmcId() )
                    || ( attrVal == KCaMassStorage()
                        && IsDriveInUse( DriveInfo::EDefaultMassStorage ) ) )
                {
                // This item is on an MMC which is currently
                // in the MMC history or on a mass storage in use.
                // Set it "missing" but keep it.
                SetMissingFlagL( aCaEntries[i] );
                }
            else
                {
                aCaEntries[i]->RemoveAttributeL( KCaAttrMmcId() );
                ClearAllFlagsL( aCaEntries[i] );
                }
            }
        else
            {
            ClearAllFlagsL( aCaEntries[i] );
            }
        CleanupStack::PopAndDestroy( &attrVal );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::SetMissingFlagL( CCaInnerEntry* aEntry )
    {
    if( !( aEntry->GetFlags() & EMissing ) )
        {
        aEntry->SetFlags( ( aEntry->GetFlags() | EMissing ) & ~EUninstall );
        iCaStorageProxy.AddL( aEntry, EFalse, EItemDisappeared );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::ClearAllFlagsL( CCaInnerEntry* aEntry )
    {
    if( aEntry->GetFlags() & EVisible )
        {
        aEntry->SetFlags( 0 );
        iCaStorageProxy.AddL( aEntry, EFalse, EItemDisappeared );
        }
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TUint CCaSrvAppScanner::UpdateMmcHistoryL()
    {
    TUint mmcId = CurrentMmcId();
    if( mmcId )
        {
        iMmcHistory->InsertL( mmcId );
        iMmcHistory->SaveL( iFs, KCaMmcHistoryFname() );
        }
    return mmcId;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TUint CCaSrvAppScanner::CurrentMmcId() const
    {
    // Get mmc id. Errors are ignored.
    TUint mmcId = 0;
    TInt mmcDrive;
    TInt err = DriveInfo::GetDefaultDrive(
            DriveInfo::EDefaultRemovableMassStorage, mmcDrive );
    if( !err )
        {
        TVolumeInfo volumeInfo;
        err = iFs.Volume( volumeInfo, mmcDrive );
        if( !err )
            {
            mmcId = volumeInfo.iUniqueID;
            }
        }
    return mmcId;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::IsCharInDrive( const TChar& aDriveLetter,
        const DriveInfo::TDefaultDrives& aDefaultDrive ) const
    {
    TBool retVal( EFalse );
    TInt mmcDrive;
    TInt err = DriveInfo::GetDefaultDrive( aDefaultDrive, mmcDrive );
    if( !err )
        {
        TInt fileDrive;
        err = RFs::CharToDrive( aDriveLetter, fileDrive );
        if( !err && fileDrive == mmcDrive )
            {
            retVal = ETrue;
            }
        }
    return retVal;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::IsDriveInUse(
        const DriveInfo::TDefaultDrives& aDefaultDrive )
    {
    TBool inUse( EFalse );
    TInt drive;

    TInt err = DriveInfo::GetDefaultDrive( aDefaultDrive, drive );
    if( err == KErrNone )
        {
        TUint status;
        err = DriveInfo::GetDriveStatus( iFs, drive, status );
        if( err == KErrNone && ( status & DriveInfo::EDriveInUse ) )
            {
            inUse = ETrue;
            }
        }

    return inUse;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::MakeNotEmptyCollectionsVisibleL()
    {
    RPointerArray<CCaInnerEntry> resultArray;
    CleanupResetAndDestroyPushL( resultArray );
    CCaInnerQuery* hiddenCollectionsQuery = CCaInnerQuery::NewLC();
    CDesC16ArrayFlat* entryType = new ( ELeave ) CDesC16ArrayFlat(
            KGranularityOne );
    CleanupStack::PushL( entryType );
    entryType->AppendL( KCaTypeCollection );
    hiddenCollectionsQuery->SetEntryTypeNames( entryType );
    hiddenCollectionsQuery->SetFlagsOff( EVisible );
    iCaStorageProxy.GetEntriesL( hiddenCollectionsQuery, resultArray );
    CleanupStack::Pop( entryType );
    CleanupStack::PopAndDestroy( hiddenCollectionsQuery );
    if( resultArray.Count() )
        {
        for( TInt i=0; i<resultArray.Count(); i++ )
            {
            // for any not visible collection
            MakeCollectionVisibleIfHasVisibleEntryL( resultArray[i] );
            }
        }
    CleanupStack::PopAndDestroy( &resultArray );
   }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
void CCaSrvAppScanner::MakeCollectionVisibleIfHasVisibleEntryL(
        CCaInnerEntry* aEntry )
    {
    RPointerArray<CCaInnerEntry> resultEntriesArray;
    CleanupResetAndDestroyPushL( resultEntriesArray );
    CCaInnerQuery* visibleEntriesQuery = CCaInnerQuery::NewLC();
    visibleEntriesQuery->SetParentId( aEntry->GetId() );
    visibleEntriesQuery->SetFlagsOn( EVisible );
    visibleEntriesQuery->SetFlagsOff( EMissing );
    iCaStorageProxy.GetEntriesL( visibleEntriesQuery, resultEntriesArray );
    if( resultEntriesArray.Count() )
        {
        // set collection visible if hidden
        if( HandleVisibleFlagUpdate( aEntry ) )
            {
            // update here this collection
            iCaStorageProxy.AddL( aEntry );
            }
        }
    CleanupStack::PopAndDestroy( visibleEntriesQuery );
    CleanupStack::PopAndDestroy( &resultEntriesArray );
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TInt CCaSrvAppScanner::GetComponentIdFromEntryL( CCaInnerEntry* aEntry )
    {
    TInt id(0);
    RBuf componentId;
    componentId.CleanupClosePushL();
    componentId.CreateL( KCaMaxAttrValueLen );

    TBool found = aEntry->FindAttribute( KCaComponentId, componentId );
    if( found )
        {
        TLex parser;
        parser.Assign( componentId );
        parser.Val( id );
        }
    CleanupStack::PopAndDestroy( &componentId );

    return id;
    }

// ---------------------------------------------------------
//
// ---------------------------------------------------------
//
TBool CCaSrvAppScanner::UpdateComponentIdL( CCaInnerEntry& aEntry )
    {
    TBool updated( EFalse );
    TComponentId componentId( 0 );

    TRAPD(err, componentId = iSoftwareRegistry.GetComponentIdForAppL(
            TUid::Uid( aEntry.GetUid() ) ) )

    if ( componentId > 0 && err == KErrNone )
        {
        RBuf newComponentId;
        newComponentId.CleanupClosePushL();
        newComponentId.CreateL( sizeof(TComponentId) + 1 );
        newComponentId.AppendNum( componentId );

        // 'add' or 'update' the component id attribute value
        updated = AddAttributeL( &aEntry, KCaComponentId, newComponentId );

        CleanupStack::PopAndDestroy( &newComponentId );
        }
    return updated;
    }

