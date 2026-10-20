// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phepowerkeyplugin.cpp -- OURS: the Power key Control Panel page (after GSAutoKeyguardPlugin / GSSensorPlugin)
//
// Two radio-button settings in CenRep 0xE70157B0 (owner SysAp, which reads them at every key press):
//   key 1 = short press, key 2 = long press (hold 2 s); value 0 = Power menu, 1 = Lock / unlock, 2 = Switch off.

#include <coeaui.h>
#include <bautils.h>
#include <aknViewAppUi.h>
#include <aknradiobuttonsettingpage.h>
#include <aknsettingpage.h>
#include <badesca.h>
#include <centralrepository.h>
#include <data_caging_paths.hrh>
#include <gsprivatepluginproviderids.h>
#include <gscommon.hrh>
#include <StringLoader.h>
#include <phepowerkeypluginrsc.rsg>

#include "phepowerkeyplugin.hrh"
#include "phepowerkeyplugin.h"
#include "phepowerkeyplugincontainer.h"

_LIT( KPhePowerKeyPluginResourceFileName, "z:phepowerkeypluginrsc.rsc" );

CPhePowerKeyPlugin* CPhePowerKeyPlugin::NewL( TAny* /*aInitParams*/ )
    {
    CPhePowerKeyPlugin* self = new( ELeave ) CPhePowerKeyPlugin();
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

CPhePowerKeyPlugin::CPhePowerKeyPlugin()
    : iResourceLoader( *iCoeEnv )
    {
    }

void CPhePowerKeyPlugin::ConstructL()
    {
    iRepository = CRepository::NewL( KPhePowerKeyCRUid );

    // find + open the resource file (as the stock plug-ins do), then the view's own resource
    TParse parse;
    parse.Set( KPhePowerKeyPluginResourceFileName, &KDC_RESOURCE_FILES_DIR, NULL );
    TFileName fileName( parse.FullName() );
    BaflUtils::NearestLanguageFile( iCoeEnv->FsSession(), fileName );
    iResourceLoader.OpenL( fileName );

    BaseConstructL( R_PHEPK_VIEW );
    }

CPhePowerKeyPlugin::~CPhePowerKeyPlugin()
    {
    iResourceLoader.Close();
    delete iRepository;
    }

TUid CPhePowerKeyPlugin::Id() const
    {
    return KPhePowerKeyPluginUid;
    }

void CPhePowerKeyPlugin::HandleCommandL( TInt aCommand )
    {
    switch ( aCommand )
        {
        case EGSCmdAppChange:
        case EAknSoftkeyChange:
            HandleListBoxSelectionL();
            break;
        case EAknSoftkeyBack:
            iAppUi->ActivateLocalViewL( KGSGenPluginUid );
            break;
        default:
            iAppUi->HandleCommandL( aCommand );
            break;
        }
    }

void CPhePowerKeyPlugin::GetCaptionL( TDes& aCaption ) const
    {
    // the resource file is already open
    HBufC* caption = StringLoader::LoadL( R_PHEPK_VIEW_CAPTION );
    aCaption.Copy( caption->Left( aCaption.MaxLength() ) );
    delete caption;
    }

TInt CPhePowerKeyPlugin::PluginProviderCategory() const
    {
    return KGSPluginProviderInternal;
    }

TInt CPhePowerKeyPlugin::ActionL( TUint32 aKey ) const
    {
    TInt value = 0;
    TInt err = iRepository->Get( aKey, value );
    if ( err != KErrNone || value < 0 || value >= KPhePowerKeyActions )
        {
        // a missing / out of range key: the board defaults (e70157b0.txt) -- short = lock / unlock, long = menu
        value = ( aKey == KPhePowerKeyShort ) ? 1 : 0;
        }
    return value;
    }

CPhePowerKeyPluginContainer* CPhePowerKeyPlugin::Container()
    {
    return static_cast<CPhePowerKeyPluginContainer*>( iContainer );
    }

void CPhePowerKeyPlugin::NewContainerL()
    {
    iContainer = new( ELeave ) CPhePowerKeyPluginContainer( *this );
    }

void CPhePowerKeyPlugin::HandleListBoxSelectionL()
    {
    ChangeL( Container()->CurrentFeatureId() );
    }

// the radio-button page of one setting; OK writes the repository, Cancel leaves it
void CPhePowerKeyPlugin::ChangeL( TInt aItemId )
    {
    TUint32 key = 0;
    TInt pageRes = 0;
    switch ( aItemId )
        {
        case EPhePkShortPress:
            key = KPhePowerKeyShort;
            pageRes = R_PHEPK_SHORT_SETTING_PAGE;
            break;
        case EPhePkLongPress:
            key = KPhePowerKeyLong;
            pageRes = R_PHEPK_LONG_SETTING_PAGE;
            break;
        default:
            return;
        }

    TInt current = ActionL( key );
    CDesCArrayFlat* items = iCoeEnv->ReadDesC16ArrayResourceL( R_PHEPK_VALUES );
    CleanupStack::PushL( items );

    CAknRadioButtonSettingPage* dlg =
        new( ELeave ) CAknRadioButtonSettingPage( pageRes, current, items );
    if ( dlg->ExecuteLD( CAknSettingPage::EUpdateWhenChanged ) )
        {
        User::LeaveIfError( iRepository->Set( key, current ) );
        Container()->UpdateListBoxL();
        }
    CleanupStack::PopAndDestroy( items );
    }
