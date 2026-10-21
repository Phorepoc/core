// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phgskbd.cpp -- OURS (Phorepoc): the Settings (GS) entry for the keyboard framework (see phgskbd.h)
#include <aknViewAppUi.h>
#include <aknsettingpage.h>
#include <aknradiobuttonsettingpage.h>
#include <gsprivatepluginproviderids.h>
#include <akntitle.h>
#include <eikspane.h>
#include <implementationproxy.h>
#include <centralrepository.h>
#include <gscommon.hrh>
#include <phgskbdrsc.rsg>
#include "phgskbd.h"
#include "phimreg.h"

namespace
    {
    const TInt KHeights[] = { 80, 90, 100, 110, 120 };
    const TInt KHeightCount = sizeof( KHeights ) / sizeof( KHeights[0] );
    enum TItem { EItemKeyboard, EItemHeight, EItemTheme, EItemPopups, EItemCount };

    void SetFepLastUsedInput( TInt aMode )
        {
        CRepository* r = NULL;
        TRAPD( err, r = CRepository::NewL( TUid::Uid( 0x101F876D ) ) );
        if ( err == KErrNone )
            {
            r->Set( 0x26, aMode );
            delete r;
            }
        }

    void Line( CDesCArray& aArr, const TDesC& aCaption, const TDesC& aValue )
        {
        HBufC* b = HBufC::NewLC( aCaption.Length() + aValue.Length() + 8 );
        b->Des().Format( _L( " \t%S\t\t%S" ), &aCaption, &aValue );
        aArr.AppendL( *b );
        CleanupStack::PopAndDestroy( b );
        }
    }

// ---------------------------------------------------------------------------------------------------------------
// container

void CPhGsKbdContainer::ConstructL( const TRect& aRect )
    {
    iListBox = new ( ELeave ) CAknSettingStyleListBox;
    BaseConstructL( aRect, R_PHGSKBD_TITLE, 0 );
    }

CPhGsKbdContainer::~CPhGsKbdContainer()
    {
    delete iListBox;
    iListBox = NULL;
    delete iItems;
    }

void CPhGsKbdContainer::ConstructListBoxL( TInt /*aResLbxId*/ )
    {
    iListBox->ConstructL( this, EAknListBoxSelectionList );
    iItems = new ( ELeave ) CDesCArrayFlat( 6 );
    iListBox->Model()->SetItemTextArray( iItems );
    iListBox->Model()->SetOwnershipType( ELbmDoesNotOwnItemArray );
    RefreshItemsL();
    }

TInt CPhGsKbdContainer::CurrentIndex() const
    {
    return iListBox->CurrentItemIndex();
    }

void CPhGsKbdContainer::RefreshItemsL()
    {
    iItems->Reset();
    // 1. the default keyboard: the active registry entry's name
    RArray<TPhImEntry> list;
    CleanupClosePushL( list );
    PhImList( list );
    TUint32 active = 0;
    PhImActive( active );
    TBuf<KPhImNameMax> name;
    name.Copy( _L( "-" ) );
    for ( TInt i = 0; i < list.Count(); ++i )
        {
        if ( list[i].iEntryUid == active || ( active == 0 && i == 0 ) )
            {
            name.Copy( list[i].iName );
            break;
            }
        }
    CleanupStack::PopAndDestroy( &list );
    Line( *iItems, _L( "Default keyboard" ), name );
    // 2. 3. 4. phkbd's customisation
    TInt h = 100;
    PhImGetSetting( KPhImKeyKeyHeight, 100, h );
    TBuf<16> v;
    v.Format( _L( "%d %%" ), h );
    Line( *iItems, _L( "Key height" ), v );
    TInt theme = 0;
    PhImGetSetting( KPhImKeyTheme, 0, theme );
    Line( *iItems, _L( "Colour scheme" ), theme == 1 ? _L( "Light" ) : ( theme == 2 ? _L( "Blue" ) : _L( "Dark" ) ) );
    TInt popups = 1;
    PhImGetSetting( KPhImKeyPopups, 1, popups );
    Line( *iItems, _L( "Key press preview" ), popups ? _L( "On" ) : _L( "Off" ) );
    iListBox->HandleItemAdditionL();
    }

// ---------------------------------------------------------------------------------------------------------------
// view

CPhGsKbdPlugin* CPhGsKbdPlugin::NewL( TAny* /*aInitParams*/ )
    {
    CPhGsKbdPlugin* self = new ( ELeave ) CPhGsKbdPlugin;
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

CPhGsKbdPlugin::CPhGsKbdPlugin()
    {
    }

CPhGsKbdPlugin::~CPhGsKbdPlugin()
    {
    if ( iContainer )
        {
        AppUi()->RemoveFromViewStack( *this, iContainer );
        delete iContainer;
        iContainer = NULL;
        }
    }

void CPhGsKbdPlugin::ConstructL()
    {
    OpenLocalizedResourceFileL( KPhGsKbdResourceFile, iResourceLoader );
    BaseConstructL( R_PHGSKBD_VIEW );
    }

TUid CPhGsKbdPlugin::Id() const
    {
    return KPhGsKbdPluginUid;
    }

void CPhGsKbdPlugin::HandleClientRectChange()
    {
    if ( iContainer )
        {
        iContainer->SetRect( ClientRect() );
        }
    }

void CPhGsKbdPlugin::DoActivateL( const TVwsViewId& aPrevViewId, TUid aCustomMessageId, const TDesC8& aCustomMessage )
    {
    CGSBaseView::DoActivateL( aPrevViewId, aCustomMessageId, aCustomMessage );
    }

void CPhGsKbdPlugin::DoDeactivate()
    {
    CGSBaseView::DoDeactivate();
    }

void CPhGsKbdPlugin::GetCaptionL( TDes& aCaption ) const
    {
    aCaption.Copy( _L( "Keyboards" ) );
    }

TInt CPhGsKbdPlugin::PluginProviderCategory() const
    {
    return KGSPluginProviderInternal;
    }

void CPhGsKbdPlugin::NewContainerL()
    {
    iContainer = new ( ELeave ) CPhGsKbdContainer;
    }

void CPhGsKbdPlugin::HandleCommandL( TInt aCommand )
    {
    switch ( aCommand )
        {
        case EGSMSKCmdAppChange:
        case EGSCmdAppChange:
            HandleListBoxSelectionL();
            break;
        case EAknSoftkeyBack:
            AppUi()->ActivateLocalViewL( KGSGenPluginUid );
            break;
        default:
            AppUi()->HandleCommandL( aCommand );
            break;
        }
    }

TBool CPhGsKbdPlugin::ChooseL( CDesCArray* aItems, TInt& aIndex )
    {
    CAknSettingPage* dlg = new ( ELeave ) CAknRadioButtonSettingPage( R_PHGSKBD_SETTING_PAGE, aIndex, aItems );
    return dlg->ExecuteLD( CAknSettingPage::EUpdateWhenChanged );
    }

void CPhGsKbdPlugin::HandleListBoxSelectionL()
    {
    switch ( PhContainer()->CurrentIndex() )
        {
        case EItemKeyboard: ChooseKeyboardL(); break;
        case EItemHeight:   ChooseKeyHeightL(); break;
        case EItemTheme:    ChooseThemeL(); break;
        case EItemPopups:   ChoosePopupsL(); break;
        default: break;
        }
    PhContainer()->RefreshItemsL();
    PhContainer()->DrawNow();
    }

// the default keyboard: every registered one (the stock ones included), the choice is persisted by the registry. The pen
// input mode the FEP opens first follows from the entry's mode (the ITU-T keypad or a QWERTY): the FEP's own "last used
// portrait input" is set to match, so the next editor opens in the chosen keyboard.
void CPhGsKbdPlugin::ChooseKeyboardL()
    {
    RArray<TPhImEntry> list;
    CleanupClosePushL( list );
    User::LeaveIfError( PhImList( list ) );
    TUint32 active = 0;
    PhImActive( active );
    CDesCArrayFlat* items = new ( ELeave ) CDesCArrayFlat( 4 );
    CleanupStack::PushL( items );
    TInt cur = 0;
    for ( TInt i = 0; i < list.Count(); ++i )
        {
        items->AppendL( list[i].iName );
        if ( list[i].iEntryUid == active )
            {
            cur = i;
            }
        }
    const TInt old = cur;
    if ( ChooseL( items, cur ) && cur != old )
        {
        User::LeaveIfError( PhImSetActive( list[cur].iEntryUid ) );
        // the FEP's "last used portrait input" (AknFep repository 101F876D key 0x26): ITU-T = 8, portrait QWERTY = 128
        SetFepLastUsedInput( list[cur].iMode );
        }
    CleanupStack::PopAndDestroy( 2, &list );
    }

void CPhGsKbdPlugin::ChooseKeyHeightL()
    {
    CDesCArrayFlat* items = new ( ELeave ) CDesCArrayFlat( KHeightCount );
    CleanupStack::PushL( items );
    TInt cur = 2;
    TInt h = 100;
    PhImGetSetting( KPhImKeyKeyHeight, 100, h );
    for ( TInt i = 0; i < KHeightCount; ++i )
        {
        TBuf<16> b;
        b.Format( _L( "%d %%" ), KHeights[i] );
        items->AppendL( b );
        if ( KHeights[i] == h )
            {
            cur = i;
            }
        }
    const TInt old = cur;
    if ( ChooseL( items, cur ) && cur != old )
        {
        PhImSetSetting( KPhImKeyKeyHeight, KHeights[cur] );
        }
    CleanupStack::PopAndDestroy( items );
    }

void CPhGsKbdPlugin::ChooseThemeL()
    {
    CDesCArrayFlat* items = new ( ELeave ) CDesCArrayFlat( 3 );
    CleanupStack::PushL( items );
    items->AppendL( _L( "Dark" ) );
    items->AppendL( _L( "Light" ) );
    items->AppendL( _L( "Blue" ) );
    TInt cur = 0;
    PhImGetSetting( KPhImKeyTheme, 0, cur );
    const TInt old = cur;
    if ( ChooseL( items, cur ) && cur != old )
        {
        PhImSetSetting( KPhImKeyTheme, cur );
        }
    CleanupStack::PopAndDestroy( items );
    }

void CPhGsKbdPlugin::ChoosePopupsL()
    {
    CDesCArrayFlat* items = new ( ELeave ) CDesCArrayFlat( 2 );
    CleanupStack::PushL( items );
    items->AppendL( _L( "On" ) );
    items->AppendL( _L( "Off" ) );
    TInt on = 1;
    PhImGetSetting( KPhImKeyPopups, 1, on );
    TInt cur = on ? 0 : 1;
    const TInt old = cur;
    if ( ChooseL( items, cur ) && cur != old )
        {
        PhImSetSetting( KPhImKeyPopups, cur == 0 ? 1 : 0 );
        }
    CleanupStack::PopAndDestroy( items );
    }

// ---------------------------------------------------------------------------------------------------------------
// ECom

const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY( 0xE7015821, CPhGsKbdPlugin::NewL )
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy( TInt& aTableCount )
    {
    aTableCount = sizeof( ImplementationTable ) / sizeof( TImplementationProxy );
    return ImplementationTable;
    }
