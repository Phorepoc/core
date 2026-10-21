// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepowerkeyplugincontainer.cpp -- OURS: the list container of the Power key page (after GSAutoKeyguardPluginContainer)
#include <bldvariant.hrh>
#include <aknlists.h>
#include <eikclb.h>
#include <gslistbox.h>
#include <phepowerkeypluginrsc.rsg>

#include "phepowerkeyplugin.hrh"
#include "phepowerkeyplugin.h"
#include "phepowerkeyplugincontainer.h"

CPhePowerKeyPluginContainer::CPhePowerKeyPluginContainer( CPhePowerKeyPlugin& aView )
    : iView( aView )
    {
    }

CPhePowerKeyPluginContainer::~CPhePowerKeyPluginContainer()
    {
    delete iValueNames;
    delete iListboxItemArray;
    }

void CPhePowerKeyPluginContainer::ConstructL( const TRect& aRect )
    {
    iListBox = new( ELeave ) CAknSettingStyleListBox;
    BaseConstructL( aRect, R_PHEPK_VIEW_TITLE, R_PHEPK_LBX );
    }

void CPhePowerKeyPluginContainer::ConstructListBoxL( TInt aResLbxId )
    {
    iListBox->ConstructL( this, EAknListBoxSelectionList );
    iListboxItemArray = CGSListBoxItemTextArray::NewL( aResLbxId, *iListBox, *iCoeEnv );
    iListBox->Model()->SetItemTextArray( iListboxItemArray );
    iListBox->Model()->SetOwnershipType( ELbmDoesNotOwnItemArray );
    iValueNames = iCoeEnv->ReadDesC16ArrayResourceL( R_PHEPK_VALUES );
    UpdateListBoxL();
    }

void CPhePowerKeyPluginContainer::SetItemL( TInt aItemId, TUint32 aKey )
    {
    TInt action = iView.ActionL( aKey );
    if ( action < 0 || action >= iValueNames->MdcaCount() )
        {
        action = 0;
        }
    iListboxItemArray->SetDynamicTextL( aItemId, iValueNames->MdcaPoint( action ) );
    iListboxItemArray->SetItemVisibilityL( aItemId, CGSListBoxItemTextArray::EVisible );
    }

void CPhePowerKeyPluginContainer::UpdateListBoxL()
    {
    SetItemL( EPhePkShortPress, KPhePowerKeyShort );
    SetItemL( EPhePkLongPress, KPhePowerKeyLong );
    iListBox->HandleItemAdditionL();
    }

TInt CPhePowerKeyPluginContainer::CurrentFeatureId() const
    {
    return iListboxItemArray->CurrentFeature();
    }
