/*
* Copyright (c) 2005-2006 Nokia Corporation and/or its subsidiary(-ies).
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
* Phorepoc (toolchain/shims/aifw_board): the release file plus "PH: aifw publisher" lines -- the uid
*   and result of every content publisher load (a homescreen widget whose publisher fails is removed with
*   "Problem occurred. Widget(s) removed." and nothing else in a release build says why).
*
* Description:  Content plugin manager class for Active idle framework.
*
*/

// System includes
#include <ecom/ecom.h>
#include <e32debug.h>
#include <ecom/implementationinformation.h>

// User includes
#include <aisystemuids.hrh>
#include <aifwpublisherinfo.h>
#include <hscontentpublisher.h>
#include <hspublisherinfo.h>
#include <aicontentobserver.h>
#include <aiuicontroller.h>
#include "caicpscommandbuffer.h"
#include "aiuicontrollermanager.h"
#include "aistatemanager.h"
#include "aipluginfactory.h"
#include "debug.h"

// Constants
const TUid KDeviceStatusPluginUid = 
    { AI_UID_ECOM_IMPLEMENTATION_CONTENTPUBLISHER_DEVSTAPLUGIN };

const TUid KProfilePluginUid =
    { AI_UID_ECOM_IMPLEMENTATION_CONTENTPUBLISHER_PROFILEPLUGIN };

const TUid KSapiPluginImplUid = { 0x20019594 };                
const TUid KWrtPluginImplUid = { 0x200286DD };                          

_LIT( KDeviceStatusPluginName, "DeviceStatus" );
_LIT( KProfilePluginName, "Profile" );

const TInt KQueueStartInterval( 500000 );

// ======== LOCAL FUNCTIONS ========
// ----------------------------------------------------------------------------
// IsRecyclable()
//
// ----------------------------------------------------------------------------
//
TBool IsRecyclable( const THsPublisherInfo& aInfo )
    {
    if ( ( aInfo.Name() == KProfilePluginName && 
        aInfo.Uid() == KProfilePluginUid ) ||
        ( aInfo.Name() == KDeviceStatusPluginName &&
        aInfo.Uid() == KDeviceStatusPluginUid ) )
        {
        return ETrue;
        }
    
    return EFalse;
    }

// ----------------------------------------------------------------------------
// IsSapiOrWrt()
//
// ----------------------------------------------------------------------------
//
TBool IsSapiOrWrt( const THsPublisherInfo& aInfo )
    {
    return ( aInfo.Uid() == KSapiPluginImplUid || 
             aInfo.Uid() == KWrtPluginImplUid );             
    }

// ----------------------------------------------------------------------------
// CleanupResetAndDestroy()
//
// ----------------------------------------------------------------------------
//
template< class T >
static void CleanupResetAndDestroy( TAny* aObj )
    {        
    static_cast< T* >( aObj )->ResetAndDestroy();    
    }

// ----------------------------------------------------------------------------
// CleanupResetAndDestroyPushL()
//
// ----------------------------------------------------------------------------
//
template< class T >
static void CleanupResetAndDestroyPushL( T& aArray )
    {
    CleanupStack::PushL( 
        TCleanupItem( &CleanupResetAndDestroy< T >, &aArray ) );
    }
    
// ======== MEMBER FUNCTIONS ========    

// ----------------------------------------------------------------------------
// CAiPluginFactory::CAiPluginFactory()
//
// ----------------------------------------------------------------------------
//
CAiPluginFactory::CAiPluginFactory( CAiUiControllerManager& aManager )
    : CTimer( CActive::EPriorityUserInput - 1 ), 
      iUiControllerManager( aManager ), iAllowFlush( ETrue )
    {
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::~CAiPluginFactory()
//
// ----------------------------------------------------------------------------
//
CAiPluginFactory::~CAiPluginFactory()
    {
    Cancel();
    
    delete iStarter;
    
    delete iCommandBuffer;
    
    iLoadQueue.Reset();
    iDestroyQueue.Reset();
    
    // All publishers should be already deleted from CAiFw::HandleUiShutdown       
    iPublishers.ResetAndDestroy();
    
    iEComPlugins.ResetAndDestroy();        
    
    REComSession::FinalClose();
    }
    
// ----------------------------------------------------------------------------
// CAiPluginFactory::NewL()
//
// ----------------------------------------------------------------------------
//
CAiPluginFactory* CAiPluginFactory::NewL( CAiUiControllerManager& aManager )
    {
    return new ( ELeave ) CAiPluginFactory( aManager ); 
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::ConstructL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::ConstructL()
    {             
    ListImplementationsL();
    
    iCommandBuffer = CAiCpsCommandBuffer::NewL();
    
    // Ensure interface is available
    iCommandBuffer->GetCPSInterfaceL();
    
    CActiveScheduler::Add( this );
    
    CTimer::ConstructL();
    
    iStarter = CPeriodic::NewL( CActive::EPriorityIdle );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::ListImplementationsL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::ListImplementationsL()
    {
    iEComPlugins.ResetAndDestroy();
    
    REComSession::ListImplementationsL( 
        KInterfaceUidHsContentPlugin, iEComPlugins );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::LoadPlugin()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::LoadPlugin( const TAiFwPublisherInfo& aInfo )
    {    
    const THsPublisherInfo& info( aInfo.Info() );
    
    for ( TInt i = 0; i < iDestroyQueue.Count(); i++ )
        {
        if ( iDestroyQueue[i] == info )
            {
            iDestroyQueue.Remove( i );
            break;
            }
        }
           
    if ( IsSapiOrWrt( info ) || IsRecyclable( info ) || !PluginByInfo( info ) )
        {
        TBool append( ETrue );
        
        for ( TInt i = 0; i < iLoadQueue.Count(); i++ )       
            {
            if ( iLoadQueue[i] == info )
                {                
                append = EFalse;
                break;
                }
            }    
                
        // Plugin is not in load queue and is not currently loaded
        if( append )
            {                               
            iLoadQueue.Append( aInfo );
                        
            HandleQueueChanged();
            }    
        }    
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DestroyPlugin()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DestroyPlugin( const TAiFwPublisherInfo& aInfo )
    {
    const THsPublisherInfo& info( aInfo.Info() );
           
    for ( TInt i = 0; i < iLoadQueue.Count(); i++ )
        {
        if ( iLoadQueue[i] == info )
            {
            iLoadQueue[i].Callback( KErrCancel );
            
            iLoadQueue.Remove( i );                       
            break;
            }
        }
        
    if ( PluginByInfo( info ) )
        {
        TBool append( ETrue );
        
        for ( TInt i = 0; i < iDestroyQueue.Count(); i++ )
            {
            if ( iDestroyQueue[i] == info )
                {
                append = EFalse;
                break;
                }        
            }
        
        // Plugin is not in destroy queue and is currently loaded
        if ( append )
            {                       
            iDestroyQueue.Append( aInfo );
            
            HandleQueueChanged();
            }    
        } 
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::HandleQueueChanged()
// 
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::HandleQueueChanged()
    {
    __PRINTS( "CAiPluginFactory::HandleQueueChanged" );

    if ( iLoadQueue.Count() == 0 && iDestroyQueue.Count() == 0 )
        {
        __PRINTS( "CAiPluginFactory::HandleQueueChanged - done, queues empty" );
        
        return;
        }
            
    Cancel();
    iStarter->Cancel();

    __PRINTS( "CAiPluginFactory::HandleQueueChanged, iStarter->Start()" );

    if ( iLoadQueue.Count() == 1 && iDestroyQueue.Count() == 0 &&
        iLoadQueue[0].Reason() == EAiFwPluginStartup )
        {
        ProcessQueue( this );
        }
    else
        {
        iStarter->Start( KQueueStartInterval, 0, 
            TCallBack( ProcessQueue, this ) );        
        }               

    __PRINTS( "CAiPluginFactory::HandleQueueChanged - done" );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DoCreatePlugin()
//
// ----------------------------------------------------------------------------
//
TInt CAiPluginFactory::DoCreatePlugin( 
    const TAiFwPublisherInfo& aPublisherInfo )
    {
    __PRINTS( "*** CAiPluginFactory::DoCreatePlugin: Start ***" );

    const THsPublisherInfo& info( aPublisherInfo.Info() );
    
    if ( IsRecyclable( info ) )
        {
        CHsContentPublisher* plugin( PluginByUid( info.Uid() ) );

        if ( plugin )
            {
            if ( info.Namespace() == KNullDesC8 )
                {
                // No namespace available
                __PRINTS( "*** CAiPluginFactory::CreatePlugin: Done -\
                               Failed to Load Plug-in: KErrNotSupported ***" );
                return KErrNotSupported;
                }

            // Plugin already exists, update its namespace
            THsPublisherInfo& pubinfo( 
                const_cast< THsPublisherInfo& >( plugin->PublisherInfo() ) );

            pubinfo.iNamespace.Copy( info.Namespace() );

            __PRINTS( "*** CAiPluginFactory::CreatePlugin: Done -\
                           Plugin recycled ***" );

            return KErrNone;
            }
        }

    TBool implFound( EFalse );

    for( TInt i = 0; i < iEComPlugins.Count(); i++ )
        {
        CImplementationInformation* information( iEComPlugins[i] );

        if( information->ImplementationUid() == info.Uid() )
            {
            implFound = ETrue;
            break;
            }
        }
    
    if( info.Namespace() == KNullDesC8 || !implFound )
        {
        RDebug::Printf( "PH: aifw publisher %08x: not loaded, %s", info.Uid().iUid,
            implFound ? "no namespace" : "no ECOM implementation" );
        // No namespace available or no ecom implementation available
        __PRINTS( "*** CAiPluginFactory::CreatePlugin: Done -\
                       Failed to Load Plug-in: KErrNotSupported ***" );
        
        return KErrNotSupported;
        }
           
    CHsContentPublisher* plugin( PluginByInfo( info ) );

    if( plugin )
        {
        __PRINTS( "*** CAiPluginFactory::CreatePlugin: Done -\
                       No need to Load Plug-in: KErrAlreadyExists ***" );
        
        return KErrAlreadyExists;
        }

    TInt err( KErrNone );

    TRAP( err, DoCreatePluginL( aPublisherInfo ) );
    RDebug::Printf( "PH: aifw publisher %08x load %d", info.Uid().iUid, err );
    
    __PRINTS( "*** CAiPluginFactory::CreatePlugin: Done - Load Plug-in ***" );

    return err;
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DoDestroyPlugin()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DoDestroyPlugin( 
    const TAiFwPublisherInfo& aPublisherInfo )                             
    {
    __PRINTS( "*** CAiPluginFactory::DoDestroyPlugin: Start ***" );
    
    const THsPublisherInfo& info( aPublisherInfo.Info() );
      
    if ( IsRecyclable( info ) )
        {
        // Don't destroy recyclable plugin
        __PRINTS( "*** CAiPluginFactory::DestroyPlugin: Done - Keeping recyclable Plug-in ***" );
        
        return;
        }
   
    if ( IsSapiOrWrt( info ) )
        {
        if ( aPublisherInfo.Reason() == EAiFwPageShutdown )
            {
            // Don't destroy sapi or wrt plugin when page is changed        
            __PRINTS( "*** CAiPluginFactory::DestroyPlugin: Done - Keeping SAPI/WRT plugin ***" );
            
            return;
            }
        }
               
    CHsContentPublisher* plugin( PluginByInfo( info ) );
    
    if ( plugin )
        {
        iPublishers.Remove( iPublishers.Find( plugin ) );
        
        delete plugin;
        plugin = NULL;            
        }
    
    __PRINTS( "*** CAiPluginFactory::DestroyPlugin: Done ***" );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DestroyPlugin()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DestroyPlugin( const TUid& aUid )
    {
    __PRINTS( "*** CAiPluginFactory::DestroyPlugin: Start ***" );
    
    CHsContentPublisher* plugin( PluginByUid( aUid ) );
    
    if ( plugin )
        {                
        iPublishers.Remove( iPublishers.Find( plugin ) );
        
        __PRINT( __DBG_FORMAT( 
            "CAiPluginFactory::DestroyPlugin: name: %S" ), 
                &plugin->PublisherInfo().Name() ); 

        delete plugin;
        plugin = NULL;                
        }
    
    __PRINTS( "*** CAiPluginFactory::DestroyPlugin: Done ***" );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DestroyAllPlugins()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DestroyAllPlugins()
    {       
    Cancel();
    iStarter->Cancel();
    
    iLoadQueue.Reset();
    iDestroyQueue.Reset();
           
    for ( TInt i = 0; i < iPublishers.Count(); i++ )
        {
        CHsContentPublisher* plugin( iPublishers[i] );
    
        // Do shutdown state transition
        iStateManager->StopPlugin( *plugin, EAiFwSystemShutdown );        
        }    
    
    FlushCommandBuffer();
    
    // Finally get rid of all plugins
    iPublishers.ResetAndDestroy();
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DoCreatePluginL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DoCreatePluginL(
    const TAiFwPublisherInfo& aPublisherInfo )
    {       
    const THsPublisherInfo& info( aPublisherInfo.Info() );
    
    __PRINT( __DBG_FORMAT( "\t[I]\t Loading plug-in uid=%x name=%S"), 
    info.Uid(), &(info.Name() ) );

    iPublishers.ReserveL( iPublishers.Count() + 1 );
    
    CHsContentPublisher* plugin( NULL );
    
    __TIME( "CAiPluginFactory::CreatePluginL Create plug-in:",                                  
    plugin = CHsContentPublisher::NewL( info ) );            
    
    CleanupStack::PushL( plugin );
    
    if ( IsSapiOrWrt( info ) )
        {
        plugin->SetProperty( CHsContentPublisher::ECpsCmdBuffer, 
            static_cast< MAiCpsCommandBuffer* >( iCommandBuffer ) );    
        }
    
    __TIME( "FW: Subscribe content observers",    
    SubscribePluginL( *plugin, info ) );             
                                                      
    __TIME( "FW: Configure Plugin",
    ConfigurePluginL( *plugin, info ) );
    
    // Take plugin's ownership
    iPublishers.Append( plugin );
    CleanupStack::Pop( plugin );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::SubscribePluginL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::SubscribePluginL(
    CHsContentPublisher& aContentPublisher,        
    const THsPublisherInfo& aPublisherInfo )    
    {
    RPointerArray< CAiUiController >& 
        controllers( iUiControllerManager.UiControllers() );
            
    for( TInt i = 0; i < controllers.Count(); i++ )
        {
        MAiContentObserver& observer( 
            controllers[i]->GetContentObserver() );
        
        if ( observer.RequiresSubscription( aPublisherInfo ) )
            {
			// Subscribe observer only if it understands the plugin
            aContentPublisher.SubscribeL( observer );
            }                   
        }
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::ConfigurePluginL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::ConfigurePluginL(     
    CHsContentPublisher& aContentPublisher,
    const THsPublisherInfo& aPublisherInfo )
    {    
    RAiSettingsItemArray settings;
    CleanupResetAndDestroyPushL( settings );
    
    RPointerArray< CAiUiController >& 
        controllers( iUiControllerManager.UiControllers() );
    
    for( TInt i = 0; i < controllers.Count(); i++ )
        {
        // Get settings for plug-in        
        controllers[i]->GetSettingsL( aPublisherInfo, settings );
        }
    
    // Configure plug-in with its settings
    aContentPublisher.ConfigureL( settings );
    
    CleanupStack::PopAndDestroy( &settings );    
    }
        
// ----------------------------------------------------------------------------
// CAiPluginFactory::PluginByInfo()
// Gets plugin by publisher info. Only this overload returns the exact match
// ----------------------------------------------------------------------------
//
CHsContentPublisher* CAiPluginFactory::PluginByInfo( 
    const THsPublisherInfo& aPublisherInfo ) const
    {       
    for( TInt i = 0; i < iPublishers.Count(); i++ )
        {         
        const THsPublisherInfo& info( iPublishers[i]->PublisherInfo() ); 
                                                                             
        if( aPublisherInfo == info ) 
            {
            return iPublishers[i];
            }
        }
    
    return NULL;
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::PluginByUid()
// Gets plugin by UID
// ----------------------------------------------------------------------------
//
CHsContentPublisher* CAiPluginFactory::PluginByUid( const TUid& aUid ) const
    {
    for( TInt i = 0; i < iPublishers.Count(); i++ )
        {
        const THsPublisherInfo& info( iPublishers[i]->PublisherInfo() );
                                                               
        if( info.Uid() == aUid )
            {
            return iPublishers[i];            
            }
        }
    
    return NULL;        
    }
    
// ----------------------------------------------------------------------------
// CAiPluginFactory::PluginByName()
// Gets plugin by name
// ----------------------------------------------------------------------------
//
CHsContentPublisher* CAiPluginFactory::PluginByName( 
    const TDesC& aName ) const
    {
    for( TInt i = 0; i < iPublishers.Count(); i++ )
        {
        const THsPublisherInfo& info( iPublishers[i]->PublisherInfo() );
                                                               
        if( info.Name() == aName )
            {
            return iPublishers[i];            
            }
        }
    
    return NULL;    
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::Publishers()
//
// ----------------------------------------------------------------------------
//
RPointerArray< CHsContentPublisher >& CAiPluginFactory::Publishers() const
    {
    return iPublishers;
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::SetStateManager()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::SetStateManager( CAiStateManager* aStateManager )
    {
    iStateManager = aStateManager;
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::FlushCommandBuffer()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::FlushCommandBuffer()
    {
    __PRINTS( "CAiPluginFactory::FlushCommandBuffer" );
    
    if ( iCommandBuffer && iAllowFlush && !IsActive() )
        {
        iCommandBuffer->Flush();
        }
    
    __PRINTS( "CAiPluginFactory::FlushCommandBuffer - done" );
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::ProcessQueue()
//
// ----------------------------------------------------------------------------
//
/* static */ TInt CAiPluginFactory::ProcessQueue( TAny* aAny )
    {
    CAiPluginFactory* self = static_cast< CAiPluginFactory* >( aAny );
    
    self->iStarter->Cancel();
    
    if ( !self->IsActive() )
        {
        self->After( 0 );
        }
    
    return KErrNone;
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::RunL()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::RunL()
    {
    __PRINTS( "CAiPluginFactory::RunL" );
    
    if ( iStarter->IsActive() )
        {
        __PRINTS( "CAiPluginFactory::RunL - done, iStarter->IsActive()" );
        
        return;
        }
    
    iAllowFlush = EFalse;
           
    if ( iDestroyQueue.Count() > 0 )
        {        
        TAiFwPublisherInfo info( iDestroyQueue[ 0 ] );         
        iDestroyQueue.Remove( 0 );
               
        // Resolve plugin
        CHsContentPublisher* plugin( PluginByInfo( info.Info() ) ); 

        if ( plugin )
            {                  
            // Do shutdown state transition
            iStateManager->StopPlugin( *plugin, info.Reason() );
            
            // Destroy plugin
            DoDestroyPlugin( info );                        
            }        
        }
    else if ( iLoadQueue.Count() > 0 )
        {
        TAiFwPublisherInfo info( iLoadQueue[ 0 ] );            
        iLoadQueue.Remove( 0 );
    
        // Create plugin
        TInt retval( DoCreatePlugin( info ) );
        
        if ( retval == KErrNone || retval == KErrAlreadyExists )
            {
            CHsContentPublisher* plugin( PluginByInfo( info.Info() ) );
            
            if( plugin )
                {
                // Do startup state transition    
                iStateManager->StartPlugin( *plugin, info.Reason() );                
                }    
            else
                {
                retval = KErrNotFound;
                }            
            }
        
        info.Callback( retval );
        }

    iAllowFlush = ETrue;
    
    After( 0 );
    
    if ( iLoadQueue.Count() == 0 && iDestroyQueue.Count() == 0 )
        {
        __PRINTS( "CAiPluginFactory::RunL - queues now empty" );
        
        Cancel();
        
        FlushCommandBuffer();                             
        }
    
    __PRINTS( "CAiPluginFactory::RunL - done" );    
    }

// ----------------------------------------------------------------------------
// CAiPluginFactory::DoCancel()
//
// ----------------------------------------------------------------------------
//
void CAiPluginFactory::DoCancel()
    {
    __PRINTS( "CAiPluginFactory::DoCancel" );       
    }

// End of file
