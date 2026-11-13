/*
* Copyright (c) 2006-2007 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0""
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:  Base class for all file manager containers
*
*/



// INCLUDE FILES
#include <aknlists.h>
#include <eikclbd.h>
#include <StringLoader.h>
#include <FileManagerUID.h>
#include "CFileManagerContainerBase.h"
#include "FileManager.hrh"


// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::CFileManagerContainerBase
// C++ default constructor can NOT contain any code, that
// might leave.
// -----------------------------------------------------------------------------
//
CFileManagerContainerBase::CFileManagerContainerBase()
    {
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ConstructL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ConstructL(
        const TRect& aRect,
        const TInt aFocusedIndex )
    {
    CreateWindowL();
    // Phorepoc: no WSERV clear on server-initiated redraws (as mmwidgets' containers, core ca30b34): the default clears
    // the whole list pane WHITE, and on a scroll / rotation the compositor showed that clear before the list's redraw
    // arrived (the display's frame log: white frames during a File manager scroll)
    Window().SetBackgroundColor();
    iListBox = CreateListBoxL();
    if ( !iListBox )
        {
        User::Leave( KErrGeneral );
        }
    SetListEmptyL();
    iListBox->CreateScrollBarFrameL( ETrue );
    iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(
        CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto );
    iListBox->SetObserver( this );
    iListBox->SetListBoxObserver( this );
    SetIndex( aFocusedIndex );
    iListBox->SetFocus( ETrue );
    iListBox->AddSelectionObserverL( this );
    SetRect( aRect );
    ActivateL();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::~CFileManagerContainerBase
//
// -----------------------------------------------------------------------------
//
CFileManagerContainerBase::~CFileManagerContainerBase()
    {
    delete iListBox;
    delete iEmptyText;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SizeChanged
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SizeChanged()
    {
    if ( iListBox )
        {
        iListBox->SetRect( Rect() );
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::CountComponentControls
// 
// -----------------------------------------------------------------------------
//
TInt CFileManagerContainerBase::CountComponentControls() const
    {
    return iListBox ? 1 : 0;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ComponentControl
// 
// -----------------------------------------------------------------------------
//
CCoeControl* CFileManagerContainerBase::ComponentControl(
        TInt /* aIndex */ ) const
    {
    return iListBox;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::OfferKeyEventL
// 
// -----------------------------------------------------------------------------
//
TKeyResponse CFileManagerContainerBase::OfferKeyEventL(
        const TKeyEvent& aKeyEvent, TEventCode aType )
    {
    return iListBox->OfferKeyEventL( aKeyEvent, aType );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBox
// 
// -----------------------------------------------------------------------------
//
CEikListBox& CFileManagerContainerBase::ListBox()
    {
    return *iListBox;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::RefreshListL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::RefreshListL( TInt aFocusedIndex )
    {
    if ( !iListBox )
        {
        return;
        }

    TBool isEmpty( ETrue );
    if ( iArray )
        {
        if ( iArray->MdcaCount() )
            {
            isEmpty = EFalse;
            iListBox->Model()->SetItemTextArray( iArray );
            // Remove ownership from listbox.
            iListBox->Model()->SetOwnershipType( ELbmDoesNotOwnItemArray );
            }
        }
    if ( isEmpty )
        {
        SetEmptyArrayL();
        }

    iListBox->Reset();
    SetIndex( aFocusedIndex );
    iListBox->UpdateScrollBarsL();
    iListBox->SetFocus( ETrue );

    if ( iEmptyText )
        {
        iListBox->View()->SetListEmptyTextL( *iEmptyText );
        }
    else
        {
        iListBox->View()->SetListEmptyTextL( KNullDesC );
        }
    iListBox->DrawDeferred();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetListEmptyL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetListEmptyL()
    {
    if ( !iListBox )
        {
        return;
        }
    SetEmptyArrayL();
    iListBox->View()->SetListEmptyTextL( KNullDesC );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetIndex
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetIndex( TInt aFocusedIndex )
    {
    if ( !iListBox )
        {
        return;
        }
    // Check that given index is valid
    if ( aFocusedIndex >= 0 && 
         aFocusedIndex < iListBox->Model()->NumberOfItems() )
        {
        iListBox->SetCurrentItemIndex( aFocusedIndex );
        }
    else
        {
        // if list is empty or value is negative then set focus to 0
        if ( !iListBox->Model()->NumberOfItems() || aFocusedIndex < 0 )
            {
            iListBox->SetCurrentItemIndex( 0 );
            }
        else 
            {
            // Something has been deleted in the end of the list
            // set focus to last one so the focus doesn't jump around
            iListBox->SetCurrentItemIndex(
                iListBox->Model()->NumberOfItems() - 1 );
            }
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetTextArray
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetTextArray( MDesCArray* aArray )
    {
    iArray = aArray;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetEmptyTextL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetEmptyTextL( TInt aTextId )
    {
    delete iEmptyText;
    iEmptyText = NULL;

    if ( aTextId )
        {
        iEmptyText = StringLoader::LoadL( aTextId );
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetHelpContext
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetHelpContext(
        const TDesC& aHelpContext )
    {
    iHelpContext = aHelpContext;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::HandleListBoxEventL
//
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::HandleListBoxEventL(
        CEikListBox* /*aListBox*/, TListBoxEvent aEventType )
    {
    switch ( aEventType )
        {
        case EEventItemSingleClicked: // FALLTHROUGH
        case EEventEnterKeyPressed:
            {
            static_cast< CAknAppUi* >( ControlEnv()->AppUi() )->
                ProcessCommandL( EFileManagerSelectionKey );
            break;
            }
        default:
            {
            break;
            }
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::HandleControlEventL
//
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::HandleControlEventL(
        CCoeControl* /* aControl*/, TCoeEvent /*aEventType*/ )
    {
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::FocusChanged
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::FocusChanged( TDrawNow aDrawNow )
    {
    CCoeControl::FocusChanged( aDrawNow );

    if ( iListBox )
        {
        iListBox->SetFocus( IsFocused(), aDrawNow );
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::GetHelpContext
// 
// -----------------------------------------------------------------------------
// 
void CFileManagerContainerBase::GetHelpContext(
        TCoeHelpContext& aContext ) const
	{
    if ( iHelpContext.Length() )
        {
        aContext.iMajor = TUid::Uid( KFileManagerUID3 );
	    aContext.iContext = iHelpContext;
	    }
    else
        {
        CCoeControl::GetHelpContext( aContext );
        }
	}

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxExists
// 
// -----------------------------------------------------------------------------
// 
TBool CFileManagerContainerBase::ListBoxExists() const
    {
    return iListBox ? ETrue : EFalse;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetEmptyArrayL
// 
// -----------------------------------------------------------------------------
// 
void CFileManagerContainerBase::SetEmptyArrayL()
    {
    CDesCArray* empty = new( ELeave ) CDesCArrayFlat( 1 );
    iListBox->Model()->SetItemTextArray( empty );
    // Transfer ownership to listbox.
    iListBox->Model()->SetOwnershipType( ELbmOwnsItemArray );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ProcessCommandL
// 
// -----------------------------------------------------------------------------
// 
void CFileManagerContainerBase::ProcessCommandL( TInt aCommandId )
    {
    switch ( aCommandId )
        {
        case EAknSoftkeyShiftMSK:
            {
            static_cast< CAknAppUi* >( ControlEnv()->AppUi() )->
                ProcessCommandL( EFileManagerToggleMark );
            break;
            }
            
        default:
            {
            break;
            }
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SelectionModeChanged
// 
// -----------------------------------------------------------------------------
// 
void CFileManagerContainerBase::SelectionModeChanged(
        CEikListBox* aListBox, TBool aSelectionModeEnabled )
    {
    if ( iListBox == aListBox &&
        iSelectionModeEnabled != aSelectionModeEnabled )
        {
        iSelectionModeEnabled = aSelectionModeEnabled;
        if ( !iSelectionModeEnabled )
            {
            // Update cba after selection mode gets done
            UpdateCba();
            }
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SelectionModeEnabled
// 
// -----------------------------------------------------------------------------
// 
TBool CFileManagerContainerBase::SelectionModeEnabled() const
    {
    return iSelectionModeEnabled;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::UpdateCba
// 
// -----------------------------------------------------------------------------
// 
void CFileManagerContainerBase::UpdateCba()
    {
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxCurrentItemIndex
// 
// -----------------------------------------------------------------------------
// 
TInt CFileManagerContainerBase::ListBoxCurrentItemIndex()
    {
    return iListBox->CurrentItemIndex();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxNumberOfItems
// 
// -----------------------------------------------------------------------------
//
TInt CFileManagerContainerBase::ListBoxNumberOfItems()
    {
    return iListBox->Model()->NumberOfItems();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSelectionIndexes
// 
// -----------------------------------------------------------------------------
//
const CArrayFix< TInt >* CFileManagerContainerBase::ListBoxSelectionIndexes()
    {
    return iListBox->SelectionIndexes();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSelectionIndexesCount
// 
// -----------------------------------------------------------------------------
//
TInt CFileManagerContainerBase::ListBoxSelectionIndexesCount()
    {
    return iListBox->SelectionIndexes()->Count();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxToggleItemL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxToggleItemL( TInt aIndex )
    {
    iListBox->View()->ToggleItemL( aIndex );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxIsItemSelected
// 
// -----------------------------------------------------------------------------
//
TBool CFileManagerContainerBase::ListBoxIsItemSelected( TInt aIndex )
    {
    return iListBox->View()->ItemIsSelected( aIndex );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSelectAllL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxSelectAllL()
    {
    TInt count( ListBoxNumberOfItems() );
    if ( count > 0 )
        {
        CArrayFixFlat<TInt>* array =
            new( ELeave ) CArrayFixFlat< TInt >( count );
        CleanupStack::PushL( array );
        for ( TInt i( 0 ); i < count; ++i )
            {
            array->AppendL( i );
            }
        iListBox->View()->SetSelectionIndexesL( array );
        CleanupStack::PopAndDestroy( array );
        }
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxClearSelection
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxClearSelection()
    {
    iListBox->View()->ClearSelection();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSetTextL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxSetTextL( const TDesC& aText )
    {
    iListBox->View()->SetListEmptyTextL( aText );
    iListBox->DrawDeferred();
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::IsSearchFieldVisible
// 
// -----------------------------------------------------------------------------
//
TBool CFileManagerContainerBase::IsSearchFieldVisible() const
    {
    return EFalse;
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::EnableSearchFieldL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::EnableSearchFieldL(
        TBool /*aEnable*/, const TDesC& /*aSearchText^*/ )
    {
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SetCurrentItemIndexAfterSearch
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::SetCurrentItemIndexAfterSearch(
        TInt /*aIndex*/ )
    {
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSelectItemL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxSelectItemL( TInt aIndex )
    {
    iListBox->View()->SelectItemL( aIndex );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxDeselectItem
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxDeselectItem( TInt aIndex )
    {
    iListBox->View()->DeselectItem( aIndex );
    }

// -----------------------------------------------------------------------------
// CFileManagerContainerBase::ListBoxSetSelectionIndexesL
// 
// -----------------------------------------------------------------------------
//
void CFileManagerContainerBase::ListBoxSetSelectionIndexesL(const CArrayFixFlat<TInt>* 
                                                                aSelectionIndexes)
	{
	iListBox->View()->SetSelectionIndexesL( aSelectionIndexes );
	}
	
// -----------------------------------------------------------------------------
// CFileManagerContainerBase::SearchFieldToListBoxIndex
// 
// -----------------------------------------------------------------------------
//
TInt CFileManagerContainerBase::SearchFieldToListBoxIndex( TInt aIndex )
    {
    return 0;
    }
//  End of File  
