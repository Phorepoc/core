/*
* Copyright (c) 2002 Nokia Corporation and/or its subsidiary(-ies). 
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
* Description:  Implementation of plugins (base & all derived).
*
*/


// INCLUDE FILES
#include "ActiveSelectConnectionPlugin.h"
#include "SelectConnectionDialog.h"
#include "ConnectionDialogsLogger.h"
#include "ConnectionInfo.h"
#include "ConnectionInfoArray.h"
#include "sortsnaputils.h"
#include "cdbcols.h"

#include <featmgr.h>
#include <StringLoader.h>
#include <AknIconArray.h>
#include <AknsUtils.h>
#include <AknIconUtils.h>
#include <data_caging_path_literals.hrh>

#include <cmdestinationext.h>
#include <cmconnectionmethoddef.h>
#include <cmpluginwlandef.h>

#include <cmmanager.mbg>
#include <CConnDlgPlugin.rsg>
#ifndef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <commsdat.h>
#else
#include <commsdat.h>
#include <commsdat_partner.h>
#endif

#include <wlancontainer.h>
#include <metadatabase.h>

// CONSTANTS

// Category for all panics in this module
_LIT( KASCPCategory, "Select Connection Plugin" );

LOCAL_D const TInt KIconsGranularity = 4;
LOCAL_D const TInt KArrayGranularity = 4;
LOCAL_D const TInt KMaxEntryLength = CommsDat::KMaxTextLength*2 + 20;
/**
* For iPeriodic Timer, 60 seconds
*/
LOCAL_C const TInt KTickDefaultInterval = 60000000;
LOCAL_C const TInt KMicrosecondsToSecondsMultiplier = 1000000;

// background scan automatic scanning value
static const TUint KWlanBgScanIntervalAuto = 0xffffffff;

using namespace CMManager;


///////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::CActiveSelectConnectionPlugin()
// ---------------------------------------------------------
//
CActiveSelectConnectionPlugin::CActiveSelectConnectionPlugin( 
                                                MEikSrvNotifierBase2* aPlugin,
                                                const TUint32 aElementId,
                                                TUint32 aBearerSet )
: iBearerSet( aBearerSet ),
  iReqBearerSet( aBearerSet ),
  iElementId( aElementId ),
  iPlugin( aPlugin ), 
  iDialogConn( NULL ),
  iIsWLANFeatureSupported( EFalse ),
  iRefreshInterval( KTickDefaultInterval ),
  iHighlightedItem( -1 ),
  iDefaultDestId( 0 ),
  iDefaultCMId( 0 )
    {
    }

    
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin* CActiveSelectConnectionPlugin::NewL()
// ---------------------------------------------------------
//
CActiveSelectConnectionPlugin* CActiveSelectConnectionPlugin::NewL( 
                                                MEikSrvNotifierBase2* aPlugin,
                                                const TUint32 aElementId,
                                                TUint32 aBearerSet )
    {
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::NewL " );  
    
    CActiveSelectConnectionPlugin* self = new ( ELeave ) 
                CActiveSelectConnectionPlugin( aPlugin, aElementId, 
                                               aBearerSet );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    
    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::NewL " );      

    return self;        
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::ConstructL()
// ---------------------------------------------------------
//
void CActiveSelectConnectionPlugin::ConstructL()
    {
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::ConstructL " );  

    CActiveBase::ConstructL();
    
    FeatureManager::InitializeLibL();

    iIsWLANFeatureSupported = FeatureManager::FeatureSupported( 
                                                KFeatureIdProtocolWlan );

    FeatureManager::UnInitializeLib();
    
    iBearerSet |= ECommDbBearerVirtual;    // To get also VPN APs

    // Phorepoc board delta (X4.68): Ethernet is the board's bearer, as on the emulator -- without it the
    // "Select connection" list drops every LAN access point ("No data")
    iBearerSet |= ECommDbBearerLAN;     // To add Ethernet AP

    if ( iIsWLANFeatureSupported )
        {
        CLOG_WRITE( "WLAN Supported!" );        
        iBearerSet |= ECommDbBearerWLAN; // To add WLan AP
        iRefreshInterval = GetRefreshIntervalL();
        }



    iItems = new( ELeave ) CConnectionInfoArray();
    iIcons = new( ELeave ) CAknIconArray( KIconsGranularity );

    iCmManagerExt.OpenL();

    TInt ret = iMPM.LaunchServer();
    if ( ret != KErrAlreadyExists )
        {
        User::LeaveIfError( ret );
        }
    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::ConstructL " );  
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::~CActiveSelectConnectionPlugin()
// ---------------------------------------------------------
//    
CActiveSelectConnectionPlugin::~CActiveSelectConnectionPlugin()
    {
    CLOG_ENTERFN( "~CActiveSelectConnectionPlugin" );
    Cancel();
    
    if ( iItems )
        {
        iItems->ResetAndDestroy();
        }
    delete iItems;    
    iItems = NULL;

    iCmManagerExt.Close();
    iMPM.Close();

    CLOG_LEAVEFN( "~CActiveSelectConnectionPlugin" );
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::Cancel()
// ---------------------------------------------------------
// 
void CActiveSelectConnectionPlugin::Cancel()
    {
    StopTimer();
    CActive::Cancel();

    if( iDialogConn && !( iDialogConn->GetOkToExit() ) )
        {
        CLOG_WRITEF( _L( "iDialogConn->GetOkToExit() : %b" ), iDialogConn->GetOkToExit() );
        delete iDialogConn;
        iDialogConn = NULL;
        }
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::DoCancel()
// ---------------------------------------------------------
// 
void CActiveSelectConnectionPlugin::DoCancel()
    {
    }

    
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::RunL()
// ---------------------------------------------------------
//     
void CActiveSelectConnectionPlugin::RunL()
    {
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::RunL " );  
    
    if( iStatus == KErrNone )
        {
        BuildConnectionListL();
        iDialogConn->RefreshDialogL( iItems, iIcons, ETrue, iHighlightedItem );
        }     
    
    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::RunL " );      
    }

    
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::ShowSelectConnectionL()
// ---------------------------------------------------------
//    
void CActiveSelectConnectionPlugin::ShowSelectConnectionL()
    {    
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::ShowSelectConnectionL" );
    
    BuildConnectionListL();

    iDialogConn = CSelectConnectionDialog::NewL( iPlugin, iRefreshInterval,
                                                 iBearerSet, 
                                                 iIsWLANFeatureSupported, 
                                                 iDefaultCMId );

    iDialogConn->PrepareAndRunLD( iItems, iIcons, EFalse, iHighlightedItem );

    if( iRefreshInterval )
        {
        StartTimerL( iRefreshInterval );
        }     

    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::ShowSelectConnectionL" );
    }
 
  
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::BuildConnectionListL()
// ---------------------------------------------------------
//
void CActiveSelectConnectionPlugin::BuildConnectionListL()
    {
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::BuildConnectionListL " );  
    
    // iItems and iIcons are allocated at construction and deallocated at 
    // destruction. Unless memory allocation has failed, they cannot be NULL.
    __ASSERT_ALWAYS( iItems, User::Panic( KASCPCategory, KErrGeneral ) );
    __ASSERT_ALWAYS( iIcons, User::Panic( KASCPCategory, KErrGeneral ) );
    
    iItems->ResetAndDestroy();
    iIcons->ResetAndDestroy();

    _LIT( KDriveZ, "z:" );  // ROM folder
    // Name of the MBM file containing icons
    _LIT( KManagerIconFilename, "cmmanager.mbm" );

    TFileName iconsFileName;

    iconsFileName.Append( KDriveZ );
    iconsFileName.Append( KDC_APP_BITMAP_DIR );
    iconsFileName.Append( KManagerIconFilename );

//    iIcons->AppendL( AknsUtils::CreateGulIconL( AknsUtils::SkinInstance(),
//                                KAknsIIDQgnIndiCbHotAdd,
//                                iconsFileName, 
//                                EMbmCmmanagerQgn_indi_cb_hot_add, 
//                                EMbmCmmanagerQgn_indi_cb_hot_add_mask ) );

    // Create color icon for marking the default connection
    //
    CFbsBitmap* bitmap = NULL;
    CFbsBitmap* mask   = NULL;               
    CGulIcon* icon     = CGulIcon::NewLC();
    
    AknsUtils::CreateColorIconL( AknsUtils::SkinInstance(), 
                    KAknsIIDQgnIndiCbHotAdd, 
                    KAknsIIDQsnIconColors, EAknsCIQsnIconColorsCG13, 
                    bitmap, 
                    mask, 
                    iconsFileName,
                    EMbmCmmanagerQgn_indi_cb_hot_add, 
                    EMbmCmmanagerQgn_indi_cb_hot_add_mask,
                    KRgbBlack );

    icon->SetBitmap( bitmap );
    icon->SetMask( mask );    
    iIcons->AppendL( icon );
                
    CleanupStack::Pop( icon ); 

    TCmDefConnValue defaultConnSetting;

    iCmManagerExt.ReadDefConnL( defaultConnSetting );
    if ( defaultConnSetting.iType == ECmDefConnDestination )
        {
        iDefaultDestId = defaultConnSetting.iId;
        }
    else if ( defaultConnSetting.iType == ECmDefConnConnectionMethod )
        {
        iDefaultCMId = defaultConnSetting.iId;
        }

    if ( iIsWLANFeatureSupported && (iReqBearerSet & ECommDbBearerWLAN) )
        {
        AddSearchForWLanItemL();
        }

    AddDestinationItemsL();

    CLOG_WRITEF( _L( "iItems count: %d" ), iItems->Count() );
    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::BuildConnectionListL " );
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::AddSearchForWLanItemL
// ---------------------------------------------------------
//    
void CActiveSelectConnectionPlugin::AddSearchForWLanItemL()
    {
    TUint32 easyWlanId = iCmManagerExt.EasyWlanIdL();
    if ( easyWlanId )
        {
        CLOG_WRITEF( _L( "easyWlanId %d" ), easyWlanId );

        RCmConnectionMethodExt cmConnMethodExt;
        TRAPD( err, cmConnMethodExt = iCmManagerExt.ConnectionMethodL( 
                                                                easyWlanId ) );
        if ( err == KErrNone )
            {
            CleanupClosePushL( cmConnMethodExt );

            CGulIcon* wlanIcon = ( CGulIcon* )
                            cmConnMethodExt.GetIntAttributeL( ECmBearerIcon );
            CleanupStack::PopAndDestroy();  // cmConnMethodExt

            if ( wlanIcon )
                {
                CleanupStack::PushL( wlanIcon );
                iIcons->AppendL( wlanIcon );

                // icon array took over ownership
                CleanupStack::Pop( wlanIcon );

                // +1 because it "Zero terminates" the string
                TBuf<CommsDat::KMaxTextLength+1> nameSearch;
                _LIT( KIconIdNameEmpty, "%d\t%s\t" );

                HBufC *msg = StringLoader::LoadL( R_WLAN_EASY_WLAN );
                nameSearch = *msg;
                nameSearch.ZeroTerminate();
                delete msg;

                TBuf<KMaxEntryLength> temp;
                temp.FillZ( KMaxEntryLength );
                temp.Format( KIconIdNameEmpty, iIcons->Count()-1,
                             nameSearch.Ptr() );

                CConnectionInfo* conIn = CConnectionInfo::NewL( easyWlanId, 
                                                    0, temp );
                CleanupStack::PushL( conIn );
                iItems->AppendL( conIn );
                CleanupStack::Pop( conIn );
                iHighlightedItem = 0;
                }
            }
        }
    }
        
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::AddDestinationItemsL
// ---------------------------------------------------------
//    
void CActiveSelectConnectionPlugin::AddDestinationItemsL()
    {
 /*   TInt errConnect = iMPM.Connect();
    CLOG_WRITEF( _L( "errConnect = %d" ), errConnect );
    if ( errConnect == KErrNone )
        {*/
        TSortSnapUtils sortSnapUtils( iMPM );
        RArray<TUint32> destIdArray( KArrayGranularity );
        TRAPD( err, sortSnapUtils.GetDestIdArrayL( iCmManagerExt, destIdArray ) );
        //TRAPD( err, iCmManagerExt.AllDestinationsL( destIdArray ) );
        // Append the destinations to the list
        CLOG_WRITEF( _L( "GetDestIdArrayL returned with  %d" ), err );
    if ( err == KErrNone )
            {
            CleanupClosePushL( destIdArray );

            CLOG_WRITEF( _L( "destIdArray.Count() =  %d" ), destIdArray.Count() );
            for ( TInt i = 0; i < destIdArray.Count(); i++ )
                {
                RCmDestinationExt cmDestExt = iCmManagerExt.DestinationL( 
                                                            destIdArray[i] );
                CleanupClosePushL( cmDestExt );
                if ( /*cmDestExt.ElementId() != iElementId &&*/ 
                     !cmDestExt.MetadataL( ESnapMetadataHiddenAgent ) )
                    {
                    CLOG_WRITEF( _L( "Destination id %d, Total number of methods %d" ), 
                                destIdArray[i], cmDestExt.ConnectionMethodCount() );

#ifdef _DEBUG
                    for ( TInt j = 0; 
                            j < cmDestExt.ConnectionMethodCount(); j++ )
                        {
                        RCmConnectionMethodExt cmConnMethodExt;
                        TRAP( err, cmConnMethodExt = 
                                            cmDestExt.ConnectionMethodL( j ) );
                        if ( err == KErrNone )
                            {
                            CleanupClosePushL( cmConnMethodExt );
                            HBufC* bufMethod = 
                                cmConnMethodExt.GetStringAttributeL( ECmName );
                            CleanupStack::PushL( bufMethod );
                            TBuf<CommsDat::KMaxTextLength+1> nameMethod;
                            nameMethod = *bufMethod;
                            nameMethod.ZeroTerminate();

                            /*CLOG_WRITEF( _L( "ConnMethod id %d, Method name %s" ), 
                                cmConnMethodExt.GetIntAttributeL( ECmIapId ),
                                nameMethod.Ptr() );*/

                            CleanupStack::PopAndDestroy( bufMethod );
                            CleanupStack::PopAndDestroy();  // cmConnMethodExt
                            }
                        }
#endif

                        AddDestinationItemL( cmDestExt, destIdArray[i], i == 0,
                                             destIdArray[i] == iDefaultDestId,
                                             sortSnapUtils );
                    }
                CleanupStack::PopAndDestroy();  // cmDestExt
                }
            CleanupStack::PopAndDestroy(); // destIdArray
            }
        AddUncategorizedItemL( sortSnapUtils );
/*        }*/
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::AddDestinationItemL
// ---------------------------------------------------------
//    
void CActiveSelectConnectionPlugin::AddDestinationItemL( 
                                                RCmDestinationExt aDestination,
                                                TUint32 aDestId, 
                                                TBool aFirstItem, 
                                                TBool aDefaultDest,
                                                TSortSnapUtils& aSortSnapUtils )
    {
    TMpmSnapBuffer list;

    TInt errMPM = aSortSnapUtils.GetSortSnapData( aDestId, list );
    CLOG_WRITEF( _L( "errMPM = %d. list.Count() = %d" ), errMPM, list.Count() );

    if ( errMPM == KErrNone )
        {
        for ( TInt numCM = 0; numCM < list.Count(); numCM++ )
            {
            RCmConnectionMethodExt cmConnMethodExt;
            TRAPD( err, cmConnMethodExt = iCmManagerExt.ConnectionMethodL(
                                                        list.iIapId[numCM] ) );

            CLOG_WRITEF( _L( "Trapped err = %d" ), err );

            if ( err == KErrNone )
                {
                CleanupClosePushL( cmConnMethodExt );
                if ( iBearerSet & cmConnMethodExt.GetIntAttributeL( 
                                                    ECmCommsDBBearerType ) )
                    {
                    HBufC* bufBest = cmConnMethodExt.GetStringAttributeL(
                                                                    ECmName );

                    CleanupStack::PopAndDestroy();      // cmConnMethodExt
                    CleanupStack::PushL( bufBest );

                    _LIT( KIconIdNameBest, "%d\t%s\t\"%s\"" );
                    _LIT( KIconIdNameBestDefaultConn, "%d\t%s\t\"%s\"\t0" );

                    TBuf<CommsDat::KMaxTextLength+1> nameBest;
                    nameBest = *bufBest;
                    nameBest.ZeroTerminate();
                    CleanupStack::PopAndDestroy( bufBest );

                    TBuf<CommsDat::KMaxTextLength+1> nameDest;
                    HBufC* name = aDestination.NameLC();
                    nameDest = *name;
                    nameDest.ZeroTerminate();
                    CleanupStack::PopAndDestroy( name );

                    CGulIcon* destIcon = aDestination.IconL();
                    if ( destIcon )
                        {
                        CleanupStack::PushL( destIcon );
                        iIcons->AppendL( destIcon );

                        // icon array took over ownership
                        CleanupStack::Pop( destIcon );
                    
                        TBuf<KMaxEntryLength> temp;
                        temp.FillZ( KMaxEntryLength );
                        if ( aDefaultDest )
                            {
                            temp.Format( KIconIdNameBestDefaultConn, 
                                         iIcons->Count()-1,
                                         nameDest.Ptr(), nameBest.Ptr() );
                            }
                        else
                            {
                            temp.Format( KIconIdNameBest, 
                                         iIcons->Count()-1,
                                         nameDest.Ptr(), nameBest.Ptr() );
                            }

                        CConnectionInfo* conIn = 
                            CConnectionInfo::NewL( 0, aDestId, temp );
                        CleanupStack::PushL( conIn );
                        iItems->AppendL( conIn );
                        CleanupStack::Pop( conIn );

                        if ( aFirstItem || 
                             aDestination.MetadataL( ESnapMetadataHighlight ) )
                            {   // It sets the highlight to the
                                // first Destination (Internet),
                                // but if it finds another with
                                // the bit set, then it changes
                            iHighlightedItem = iItems->Count()-1;
                            }
                        }

                    numCM = list.Count();   // break;
                    }
				else
					{
                    CleanupStack::PopAndDestroy();  // cmConnMethodExt
					}
                }
            }
        }
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::AddUncategorizedItemL
// ---------------------------------------------------------
//    
void CActiveSelectConnectionPlugin::AddUncategorizedItemL(TSortSnapUtils& aSortSnapUtils )
    {
    TMpmSnapBuffer list;

    TInt errMPM = aSortSnapUtils.GetSortSnapData( 0, list );
    CLOG_WRITEF( _L( "Uncategorized: errMPM = %d. list.Count() = %d" ), errMPM, list.Count() );

    if ( errMPM == KErrNone )
        {
        TUint numOfUncatToBeListed = 0;

        for ( TInt numCM = 0; numCM < list.Count(); numCM++ )
            {
            RCmConnectionMethodExt cmConnMethodExt;
            TRAPD( err, cmConnMethodExt = iCmManagerExt.ConnectionMethodL(
                                                        list.iIapId[numCM] ) );
            if ( err == KErrNone )
                {
                CleanupClosePushL( cmConnMethodExt );
                if ( iBearerSet & cmConnMethodExt.GetIntAttributeL( 
                                                    ECmCommsDBBearerType ) )
                    {
                    numOfUncatToBeListed++;
                    }
                CleanupStack::PopAndDestroy();  // cmConnMethodExt
                }
            }

        if ( ( iBearerSet & ECommDbBearerWLAN ) && iIsWLANFeatureSupported )
            {
            numOfUncatToBeListed += CountAdhocWlanIapsL();	
            }
            
        if ( numOfUncatToBeListed > 0 )
            {
            CGulIcon* uncatIcon = 
                            ( CGulIcon* )iCmManagerExt.UncategorizedIconL();
            if ( uncatIcon )
                {
                CleanupStack::PushL( uncatIcon );
                iIcons->AppendL( uncatIcon );
                CleanupStack::Pop( uncatIcon );  // icon array took over ownership

                // +1 because it "Zero terminates" the string
                TBuf<CommsDat::KMaxTextLength+1> nameUncat;
                TBuf<CommsDat::KMaxTextLength+1> numUncat;

                HBufC *bufUncat = StringLoader::LoadL( R_LIST_UNCATEGORIZED );
                nameUncat = *bufUncat;
                nameUncat.ZeroTerminate();
                delete bufUncat;

                HBufC *bufNumOfUncat;
                if ( numOfUncatToBeListed == 1 )
                    {
                    bufNumOfUncat = StringLoader::LoadL( R_CONN_METHODS_ONE );
                    }
                else
                    {
                    bufNumOfUncat = StringLoader::LoadL( R_CONN_METHODS_MANY, 
                                                        numOfUncatToBeListed );
                    }

                numUncat = *bufNumOfUncat;
                numUncat.ZeroTerminate();
                delete bufNumOfUncat;

                TBuf<KMaxEntryLength> temp;
                _LIT( KIconIdNameNumber, "%d\t%s\t%s" );

                temp.FillZ( KMaxEntryLength );
                temp.Format( KIconIdNameNumber, iIcons->Count()-1, nameUncat.Ptr(),
                             numUncat.Ptr() );

                CConnectionInfo* conIn = CConnectionInfo::NewL( 0, 0, temp );
                CleanupStack::PushL( conIn );
                iItems->AppendL( conIn );        
                CleanupStack::Pop( conIn );
                if ( iHighlightedItem < 0 )
                    {
                    iHighlightedItem = 0;
                    }
                }
            }
        }
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::GetElementIDL
// ---------------------------------------------------------
//    
TUint32 CActiveSelectConnectionPlugin::GetElementIDL( TUint32 aIAPId,
                                                      TUint32 aDestinationId )
    {
    TUint32 retval = 0;
    TInt err;

    if ( aDestinationId )
        {
        RCmDestinationExt cmDestExt;
        TRAP( err, cmDestExt = iCmManagerExt.DestinationL( aDestinationId ) );
        if ( err == KErrNone )
            {
            retval = cmDestExt.ElementId();
            cmDestExt.Close();
            }
        }
    else if ( aIAPId )
        {
        RCmConnectionMethodExt cmConnMethodExt;
        TRAP( err, cmConnMethodExt = iCmManagerExt.ConnectionMethodL( 
                                                                    aIAPId ) );
        if ( err == KErrNone )
            {
            CleanupClosePushL( cmConnMethodExt );
            retval = cmConnMethodExt.GetIntAttributeL( ECmElementID );
            CleanupStack::PopAndDestroy();  // cmConnMethodExt
            }
        }

    return retval;
    }


// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::GetRefreshIntervalL()
// ---------------------------------------------------------
//    
TInt CActiveSelectConnectionPlugin::GetRefreshIntervalL( )
    {
    CLOG_ENTERFN( "CActiveSelectConnectionPlugin::GetRefreshInterval" );  

    TInt variant( KTickDefaultInterval );  


    CMDBSession* session = CMDBSession::NewL(KCDLatestVersion);
    CleanupStack::PushL( session );
    
  	//session->OpenTransactionL();
    TMDBElementId tableId = 0;
    
    tableId = CCDWlanDeviceSettingsRecord::TableIdL( *session);
        
    CCDWlanDeviceSettingsRecord* record = new(ELeave)
    		CCDWlanDeviceSettingsRecord(tableId); 		
    CleanupStack::PushL(record);
    
    record->iWlanDeviceSettingsType = KWlanUserSettings;
    
    if(record->FindL( *session ))
    	{
    	record->LoadL( *session );
        
    	// In case iBgScanInterval is -1 (= automatic) then just use 
    	// KTickDefaultInterval as refresh interval
        if ( record->iBgScanInterval != KWlanBgScanIntervalAuto
             && record->iBgScanInterval > 0 )
            {
            variant = record->iBgScanInterval;
            variant = variant * KMicrosecondsToSecondsMultiplier;    
            }
    	}
    CleanupStack::PopAndDestroy( record ); 
    
    //session->CommitTransactionL();

    CleanupStack::PopAndDestroy( session ); 
    
    TUint32 variantToLog = variant;

    CLOG_WRITEF( _L( "Refresh interval (microseconds) : %d" ), variantToLog );
    
    CLOG_LEAVEFN( "CActiveSelectConnectionPlugin::GetRefreshInterval" );
    
    return variant;            
    }
    
// ---------------------------------------------------------
// CActiveSelectConnectionPlugin::CountAdhocWlanIapsL()
// ---------------------------------------------------------
//
TInt CActiveSelectConnectionPlugin::CountAdhocWlanIapsL()
    {
    CLOG_ENTERFN( "CActiveSelectExplicit::CountAdhocWlanIapsL " );
    
    // Creating a session with the latest version
    CMDBSession* db = CMDBSession::NewLC( CMDBSession::LatestVersion() );
    db->SetAttributeMask( ECDProtectedWrite );
    
    // Create wlan service record set (RArray (or RPtrArray?))
    CMDBRecordSet<CCDWlanServiceRecord>* wlanSet =
        new (ELeave)CMDBRecordSet<CCDWlanServiceRecord>(   CCDWlanServiceRecord::TableIdL( *db )   );
    CleanupStack::PushL( wlanSet ); 
    
    // Create wlan service record
    CCDWlanServiceRecord* wlanRecord =
        new (ELeave)CCDWlanServiceRecord(   CCDWlanServiceRecord::TableIdL( *db )   );
    
    CleanupStack::PushL( wlanRecord );
    
    // Set ConnectionMode field in wlan service record (see wlancontainer.h)
    // This works with EWlanOperatingModeAdhoc, fails with EWlanConnectionModeAdhoc
    wlanRecord->iWlanConnMode.SetL( EWlanOperatingModeAdhoc );
    
    // Append wlan service record to wlan service record set (currently empty)
    wlanSet->iRecords.AppendL( wlanRecord );
    CleanupStack::Pop( wlanRecord ); // ownership moved to RPointerArray
    
    TInt wlanRecordCount( 0 );
    
    // Find matcing wlan service records
    if( wlanSet->FindL( *db ) )
        {
        wlanRecordCount = wlanSet->iRecords.Count();
        
        for ( TInt i = 0; i < wlanRecordCount; i++ )
            {
            // Load the current record again to be sure all fields are up to date
            wlanRecord->SetElementId( wlanSet->iRecords[i]->ElementId() );
    
            // Load will only look at ElementId and updates the rest of the fields
            wlanRecord->LoadL( *db );
              
            if ( wlanRecord->iWLanSSID.IsNull() )
                {
                // Count active Easy WLAN using adhoc out
                wlanRecordCount--;
                break; 
                }    
            }
        
        CLOG_WRITEF( _L("wlanSet record count = %d"), wlanRecordCount );
        }
    else
        {
        CLOG_WRITEF( _L("no matcing wlan records found." ));
        }
        
    CleanupStack::PopAndDestroy( wlanSet );
    CleanupStack::PopAndDestroy( db );
    
    CLOG_LEAVEFN( "CActiveSelectExplicit::CountAdhocWlanIapsL" );
    
    return ( wlanRecordCount );
    }
    
// End of File
