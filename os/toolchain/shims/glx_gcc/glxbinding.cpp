// Phorepoc (toolchain/shims/glx_gcc): the release file with one fix -- the three SetAttributeT calls passed an
// auto_ptr<derived variant type> temporary for an auto_ptr<MulVariantType> parameter, two user conversions in a row
// (RVCT accepted it, standard C++ does not); they construct the auto_ptr<MulVariantType> directly.
/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:    bindings required to populate the visual item
*
*/




#include "glxbinding.h"                 // The binding is created by the binding set factory.
#include "glxmuliconprovider.h"         // Icon details 
#include <StringLoader.h>               // Class provides methods to load and format resource strings
#include <mul/mulvisualitem.h>          // Client need to use this class to add data in data model
#include <mul/imulwidget.h>
#include <glxthumbnailattributeinfo.h>  // Thumbnail attribute content ID
#include <mpxcollectionpath.h>          // Encapsulates a 'bookmark' to a specific collection entry
#include <glxlog.h>                     // Logging
#include <glxtracer.h>
#include "glxcommandbindingutility.h"   // collect binding handling here
#include "glxmulthumbnailvarianttype.h"
#include "mglxbindingobserver.h"        // Notifies that binding has changed
#include "glxustringconverter.h"        // converts the symbian types to UString type
#include "glxattributerequirement.h"    // class will set the required attribute to the medialist
#include "glxuiutility.h"
#include "glxdetailsboundcommand.h"     // Handles user commands

using namespace Alf;

// ----------------------------------------------------------------------------
// Sets the attribute to the visual item
// ----------------------------------------------------------------------------
//
static void SetAttributeT( Alf::MulVisualItem& aItem,  const Alf::mulvisualitem::TVisualAttribute& aTag/*const UString& aTag*/, 
        std::auto_ptr<MulVariantType> aValue )
    {
    TRACER("SetAttributeT");
    aItem.SetAttribute( aTag, aValue.get() );
    aValue.release();
    }
    
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxBinding::~CGlxBinding()
	{
	
	}
	
// ----------------------------------------------------------------------------
// Checks whether the attributes are relevant, returns EFalse always
// ----------------------------------------------------------------------------
//
TBool CGlxBinding::HasRelevantAttributes( 
        const RArray< TMPXAttribute >& /*aAttributes*/  ) const
	{
	TRACER("CGlxBinding::HasRelevantAttributes");
	return EFalse;
	}

// ----------------------------------------------------------------------------
// Notification whether any updation is required for the focus changed
// ----------------------------------------------------------------------------
//
CGlxBinding::TResponse CGlxBinding::HandleFocusChanged( TBool /*aIsGained*/ )
	{
	TRACER("CGlxBinding::HandleFocusChanged");
	return ENoUpdateNeeded;
	}
void  CGlxBinding::HandleItemChangedL(const CMPXCollectionPath& /*aPath*/ )
    {
    
    }
	    
// ----------------------------------------------------------------------------
// Set the binding observer 
// ----------------------------------------------------------------------------
//
void CGlxBinding::SetObserver( MGlxBindingObserver& aObserver )
	{
	TRACER("CGlxBinding::SetObserver");
	iObserver = &aObserver;
	}

// ----------------------------------------------------------------------------
// Initiates the update process when the binding changed
// ----------------------------------------------------------------------------
//
void CGlxBinding::Update()
	{
	TRACER("CGlxBinding::Update");
	__ASSERT_DEBUG( iObserver, User::Invariant() );
	iObserver->HandleBindingChanged( *this ); 
	}

// ----------------------------------------------------------------------------
// AddRequestL
// ----------------------------------------------------------------------------
//
 void CGlxBinding::AddRequestL( CGlxAttributeRequirements&
    /*aAttributeRequirement*/, TSize /*aSize*/ )
    {
    
    }

// ----------------------------------------------------------------------------
// Template Binding
//-----------------------------------------------------------------------------	

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxTemplateBinding::~CGlxTemplateBinding()  
    {
    
    }

// ----------------------------------------------------------------------------
// Two phase constructor
// ----------------------------------------------------------------------------
//
CGlxTemplateBinding* CGlxTemplateBinding::NewL(
    Alf::mulwidget::TLogicalTemplate aTemplate, 
	Alf::mulwidget::TLogicalTemplate aTemplateWhenFocused)
    {
    TRACER("CGlxTemplateBinding::NewL");
    CGlxTemplateBinding* self = CGlxTemplateBinding::NewLC( aTemplate, aTemplateWhenFocused );
    CleanupStack::Pop( self );
    return self;
    }


// ----------------------------------------------------------------------------
// Two phase constructor
// ----------------------------------------------------------------------------
//
CGlxTemplateBinding* CGlxTemplateBinding::NewLC(
    Alf::mulwidget::TLogicalTemplate aTemplate, 
	Alf::mulwidget::TLogicalTemplate aTemplateWhenFocused)
    {
    TRACER("CGlxTemplateBinding::NewLC");
    CGlxTemplateBinding* self = new ( ELeave ) CGlxTemplateBinding();
    CleanupStack::PushL( self );
    self->iTemplate = aTemplate;
    self->iFocusedTemplate = aTemplateWhenFocused;
    return self;
    }
  
// ----------------------------------------------------------------------------
// PopulateT
// ----------------------------------------------------------------------------
//
void CGlxTemplateBinding::PopulateT( Alf::MulVisualItem& aItem, 
	const TGlxMedia& /*aMedia*/, TBool aIsFocused, TInt /*aTextureId*/ ) const
    {
    TRACER("CGlxTemplateBinding::PopulateT");
    // need to get the MUL field for the editable template from Akash
    
    aItem.SetAttribute(  Alf::mulvisualitem::KAttributeTemplate , 
        ( aIsFocused && iFocusedTemplate ) ? iFocusedTemplate : iTemplate );
    
    }
  
// ----------------------------------------------------------------------------
// HandleFocusChanged
// ----------------------------------------------------------------------------
//            
CGlxBinding::TResponse CGlxTemplateBinding::HandleFocusChanged( TBool /*aIsGained*/ )
    {
    TRACER("CGlxTemplateBinding::HandleFocusChanged");
    //Need further implementation
    return iFocusedTemplate ? EUpdateRequested: ENoUpdateNeeded;
    }
    
void CGlxTemplateBinding::HandleItemChangedL(const CMPXCollectionPath& /*aPath*/ )
    {
    
    }
    
// ----------------------------------------------------------------------------
// Single Tag Binding
//-----------------------------------------------------------------------------	
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxSingleTagBinding::CGlxSingleTagBinding()
	{
	
	}
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxSingleTagBinding::~CGlxSingleTagBinding()
    {
    TRACER("CGlxSingleTagBinding::~CGlxSingleTagBinding");
    }

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
void CGlxSingleTagBinding::BaseConstructL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/ )
    {
    TRACER("CGlxSingleTagBinding::BaseConstructL");
    iMulTag = aMulTag ;
    }

// ----------------------------------------------------------------------------
// Returns the MUL field
// ----------------------------------------------------------------------------
//
Alf::mulvisualitem::TVisualAttribute CGlxSingleTagBinding::MulTag() const
    {
    TRACER("CGlxSingleTagBinding::MulTag");
    return iMulTag;
    }


// ----------------------------------------------------------------------------
// String Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxStringBinding* CGlxStringBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        TInt aStringId )
	{
	TRACER("CGlxStringBinding::NewL");
	CGlxStringBinding* self = CGlxStringBinding::NewLC( aMulTag, aStringId );
	CleanupStack::Pop(self);
	return self;
	}

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxStringBinding* CGlxStringBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        TInt aStringId  )
    {
    TRACER("CGlxStringBinding::NewLC");
    CGlxStringBinding* self = new ( ELeave ) CGlxStringBinding();
    CleanupStack::PushL( self );
    self->ConstructL( aMulTag, aStringId );
    return self;
    }

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxStringBinding* CGlxStringBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        const char* const aString )
    {
    TRACER("CGlxStringBinding::NewL");
    CGlxStringBinding* self = CGlxStringBinding::NewLC( aMulTag, aString );
    CleanupStack::Pop( self );
    return self;
    }
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxStringBinding* CGlxStringBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        const char* const aString )
    {
    TRACER("CGlxStringBinding::NewLC");
    CGlxStringBinding* self = new ( ELeave ) CGlxStringBinding();
    CleanupStack::PushL( self );
    self->ConstructL( aMulTag, aString );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//   
CGlxStringBinding::CGlxStringBinding()
    {
    
    }
    
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxStringBinding::~CGlxStringBinding()
    {
    TRACER("CGlxStringBinding::~CGlxStringBinding");
    delete iStringBuffer;
    }

// ----------------------------------------------------------------------------
//  Two phase construction
// ----------------------------------------------------------------------------
//    
void CGlxStringBinding::ConstructL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        TInt aStringId )
	{
	TRACER("CGlxStringBinding::ConstructL");
	BaseConstructL( aMulTag );
    iStringBuffer = StringLoader::LoadL( aStringId );
	}

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
void CGlxStringBinding::ConstructL( const Alf::mulvisualitem::TVisualAttribute& aMulTag/*const char* const aMulTag*/,
        const char* const /*aString*/ )
	{
	TRACER("CGlxStringBinding::ConstructL");
	BaseConstructL( aMulTag );
	}
	
// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxStringBinding::PopulateT( MulVisualItem& aItem, const TGlxMedia& /*aMedia*/,
        TBool /*aIsFocused*/, TInt /*aTextureId*/ ) const
    {
    TRACER("CGlxStringBinding::PopulateT");
    aItem.SetAttribute( MulTag(), *iStringBuffer );
    }
	
	
// ----------------------------------------------------------------------------
// MPX attribute Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxMpxAttributeBinding* CGlxMpxAttributeBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const 
    aMulTag*/, const TMPXAttribute& aAttribute, TInt aFormatStringId )
    {
    TRACER("CGlxMpxAttributeBinding::NewL");
    CGlxMpxAttributeBinding* self = CGlxMpxAttributeBinding::NewLC
        ( aMulTag, aAttribute, aFormatStringId );
    CleanupStack::Pop( self );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxMpxAttributeBinding* CGlxMpxAttributeBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
        const TMPXAttribute& aAttribute, TInt aFormatStringId )
    {
    TRACER("CGlxMpxAttributeBinding::NewLC");
    CGlxMpxAttributeBinding* self = new ( ELeave ) CGlxMpxAttributeBinding( 
        aAttribute, aFormatStringId);
    CleanupStack::PushL( self );
    self->ConstructL( aMulTag );
    return self;
    }

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//    
void CGlxMpxAttributeBinding::ConstructL(const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/ )
    {
    TRACER("CGlxMpxAttributeBinding::ConstructL");
    BaseConstructL( aMulTag );
    }
    
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxMpxAttributeBinding::CGlxMpxAttributeBinding( const TMPXAttribute&
     aAttribute, TInt aFormatStringId ) : iAttribute( aAttribute ),
     iFormatStringId( aFormatStringId )
    {
    }

// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxMpxAttributeBinding::PopulateT( MulVisualItem& aItem/**/, const 
    TGlxMedia& aMedia, TBool /*aIsFocused*/, TInt /*aTextureId*/ ) const
    {
    TRACER("CGlxMpxAttributeBinding::PopulateT");
    //T is used for throws as per C++ standard.Hence used instead of "L"
    //Ignoring this for code scanner warnings - Leaving functions called in non-leaving functions.
    CGlxUStringConverter* stringConverter = CGlxUStringConverter::NewL();
    CleanupStack::PushL(stringConverter );
    HBufC* string = NULL;
    stringConverter->AsStringL( aMedia, iAttribute, iFormatStringId, string );  
    if ( string )
        {
        aItem.SetAttribute( MulTag(), *string );
        delete string;
        string = NULL;
        }
    CleanupStack::PopAndDestroy(stringConverter );
    }

// ----------------------------------------------------------------------------
// checks whether the attributes are relevant, returns EFalse always
// ----------------------------------------------------------------------------
//
TBool CGlxMpxAttributeBinding::HasRelevantAttributes( 
        const RArray< TMPXAttribute >& aAttributes ) const
    {
    TRACER("CGlxMpxAttributeBinding::HasRelevantAttributes");
    // only the member attribute is relevant; check if it is in the list
    TIdentityRelation< TMPXAttribute > match ( &TMPXAttribute::Match );
    return ( KErrNotFound != aAttributes.Find( iAttribute, match ) );
    }
	
// ----------------------------------------------------------------------------
// checks whether the focus has changed, returns the response status 
// ----------------------------------------------------------------------------
//	
CGlxBinding::TResponse CGlxMpxAttributeBinding::HandleFocusChanged( 
        TBool aIsGained )
    {
    TRACER("CGlxMpxAttributeBinding::HandleFocusChanged");
    //Need further implementation
    return aIsGained ? EUpdateRequested : ENoUpdateNeeded ;
    }
    
void CGlxMpxAttributeBinding::HandleItemChangedL(const CMPXCollectionPath& /*aPath*/ )
    {
    
    }
// ----------------------------------------------------------------------------
// sends the attribute that is to be set to the medialist
// ----------------------------------------------------------------------------
//
 void CGlxMpxAttributeBinding::AddRequestL( CGlxAttributeRequirements&  
    aAttributeRequirement,TSize /*aSize*/ )
    {
    TRACER("CGlxMpxAttributeBinding::AddRequestL");
    aAttributeRequirement.AddAttributeL(iAttribute);
    }
 
// ----------------------------------------------------------------------------
// Thumbnail Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxThumbnailBinding* CGlxThumbnailBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/, 
	const TSize& aSize )
    {
    TRACER("CGlxThumbnailBinding::NewL");
    CGlxThumbnailBinding* self = CGlxThumbnailBinding::NewLC( aMulTag, aSize );
    CleanupStack::Pop( self );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxThumbnailBinding* CGlxThumbnailBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/, 
	const TSize& aSize )
    {
    TRACER("CGlxThumbnailBinding::NewLC");
    CGlxThumbnailBinding* self = new ( ELeave ) CGlxThumbnailBinding( aSize );
    CleanupStack::PushL( self );
    self->BaseConstructL( aMulTag );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxThumbnailBinding::CGlxThumbnailBinding( const TSize& aSize ) : 
    iThumbnailSize( aSize )
    {
    }
    
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxThumbnailBinding::~CGlxThumbnailBinding()
    {
    }
 
// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxThumbnailBinding::PopulateT( MulVisualItem& aItem, const TGlxMedia& 
    aMedia, TBool aIsFocused, TInt aTextureId ) const
    {
    TRACER("CGlxThumbnailBinding::PopulateT");
    //T is used for throws as per C++ standard.Hence used instead of "L"
    //Ignoring this for code scanner warnings - Leaving functions called in non-leaving functions.
    SetAttributeT( aItem, MulTag(), 
        std::auto_ptr< MulVariantType >( 
            GlxThumbnailVariantType::NewL( aMedia, iThumbnailSize, 
        aIsFocused, aTextureId ) ) );
        //@todo
    }

// ----------------------------------------------------------------------------
// checks whether the attributes are relevant, returns EFalse always
// ----------------------------------------------------------------------------
//
TBool CGlxThumbnailBinding::HasRelevantAttributes( 
        const RArray< TMPXAttribute >& aAttributes  ) const
    {
    
    TRACER("CGlxThumbnailBinding::HasRelevantAttributes");
    TMPXAttribute tnAttribQuality(KGlxMediaIdThumbnail, 
        GlxFullThumbnailAttributeId( ETrue,  iThumbnailSize.iWidth, 
        iThumbnailSize.iHeight ) );
                                                     
    TIdentityRelation< TMPXAttribute > match ( &TMPXAttribute::Match );
	if (KErrNotFound != aAttributes.Find( tnAttribQuality, match ) )
		{
	    GLX_DEBUG1("CGlxThumbnailBinding::HasRelevantAttributes (tnAttribQuality) - ETrue");
		return ETrue;
		}
	else
		{
		TMPXAttribute tnAttribSpeed(KGlxMediaIdThumbnail, 
        GlxFullThumbnailAttributeId( EFalse,  iThumbnailSize.iWidth, 
        iThumbnailSize.iHeight ) );
                                                     
		if ( KErrNotFound != aAttributes.Find( tnAttribSpeed, match ) )
            {
            GLX_DEBUG1("CGlxThumbnailBinding::HasRelevantAttributes (tnAttribSpeed) - ETrue");
            return ETrue;
            }
		}
    GLX_DEBUG1("CGlxThumbnailBinding::HasRelevantAttributes - EFalse");
	return EFalse;
    }

// ----------------------------------------------------------------------------
// invokes the necessary api that sets the thumbnail attribute to the medialist
// ----------------------------------------------------------------------------
//
void CGlxThumbnailBinding::AddRequestL( CGlxAttributeRequirements& 
    aAttributeRequirement,TSize /*aSize*/ )
    {
    TRACER("CGlxThumbnailBinding::AddRequestL");
    aAttributeRequirement.AddThumbnailL( iThumbnailSize );
    }
    

// ----------------------------------------------------------------------------
// sends response whether update required or not depending on the focus gained
// ----------------------------------------------------------------------------
//    
CGlxBinding::TResponse CGlxThumbnailBinding::HandleFocusChanged( TBool aIsGained )
    {
    TRACER("CGlxThumbnailBinding::HandleFocusChanged");
    if(aIsGained)
        {
        
        return EUpdateRequested;
        }
        
    return ENoUpdateNeeded;
    }
// ----------------------------------------------------------------------------
// FullScreenThumbnail Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxFullScreenThumbnailBinding* CGlxFullScreenThumbnailBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/ )
    {
    TRACER("CGlxFullScreenThumbnailBinding::NewL");
    CGlxFullScreenThumbnailBinding* self = CGlxFullScreenThumbnailBinding::NewLC( aMulTag );
    CleanupStack::Pop( self );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxFullScreenThumbnailBinding* CGlxFullScreenThumbnailBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag/*const char* const aMulTag*/
	)
    {
    TRACER("CGlxFullScreenThumbnailBinding::NewLC");
    CGlxFullScreenThumbnailBinding* self = new ( ELeave ) CGlxFullScreenThumbnailBinding();
    CleanupStack::PushL( self );
    self->BaseConstructL( aMulTag );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxFullScreenThumbnailBinding::CGlxFullScreenThumbnailBinding( ) 
    {
    }
    
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxFullScreenThumbnailBinding::~CGlxFullScreenThumbnailBinding()
    {
    }
 
// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxFullScreenThumbnailBinding::PopulateT( MulVisualItem& aItem, const TGlxMedia& 
    aMedia, TBool aIsFocused, TInt aTextureId ) const
    {
    TRACER("CGlxFullScreenThumbnailBinding::PopulateT");
    //T is used for throws as per C++ standard.Hence used instead of "L"
    //Ignoring this for code scanner warnings - Leaving functions called in non-leaving functions.
    SetAttributeT( aItem, MulTag(), 
        std::auto_ptr< MulVariantType >( 
        GlxThumbnailVariantType::NewL( aMedia, iFullScreenThumbnailSize, 
        aIsFocused, aTextureId ) ) );
        
    }

// ----------------------------------------------------------------------------
// checks whether the attributes are relevant, returns EFalse always
// ----------------------------------------------------------------------------
//
TBool CGlxFullScreenThumbnailBinding::HasRelevantAttributes( 
        const RArray< TMPXAttribute >& aAttributes  ) const
    {
    
    TRACER("CGlxFullScreenThumbnailBinding::HasRelevantAttributes");
    TMPXAttribute tnAttribQuality(KGlxMediaIdThumbnail, 
        GlxFullThumbnailAttributeId( ETrue,  iFullScreenThumbnailSize.iWidth, 
        iFullScreenThumbnailSize.iHeight ) );
                                                     
    TIdentityRelation< TMPXAttribute > match ( &TMPXAttribute::Match );
    if ( KErrNotFound != aAttributes.Find( tnAttribQuality, match ) )
        {
        GLX_DEBUG1("CGlxFullScreenThumbnailBinding::HasRelevantAttributes (tnAttribQuality) - ETrue");
        return ETrue;        
        }
    else
        {      
        TMPXAttribute tnAttribSpeed(KGlxMediaIdThumbnail, 
            GlxFullThumbnailAttributeId( EFalse,  iFullScreenThumbnailSize.iWidth, 
            iFullScreenThumbnailSize.iHeight ) );
        if ( KErrNotFound != aAttributes.Find( tnAttribSpeed, match ) )
            {
            GLX_DEBUG1("CGlxFullScreenThumbnailBinding::HasRelevantAttributes (tnAttribSpeed) - ETrue");
            return ETrue;
            }
        }

    GLX_DEBUG1("CGlxFullScreenThumbnailBinding::HasRelevantAttributes - EFalse");
    return EFalse;   
    }

// ----------------------------------------------------------------------------
// invokes the necessary api that sets the thumbnail attribute to the medialist
// ----------------------------------------------------------------------------
//
void CGlxFullScreenThumbnailBinding::AddRequestL( CGlxAttributeRequirements& aAttributeRequirement,TSize aSize)
    {
    TRACER("CGlxFullScreenThumbnailBinding::AddRequestL");
    iFullScreenThumbnailSize = aSize;
    aAttributeRequirement.AddThumbnailL( iFullScreenThumbnailSize );
	CGlxUiUtility* uiUtility = CGlxUiUtility::UtilityL();
	CleanupStack::PushL( uiUtility );
	aAttributeRequirement.AddThumbnailL( uiUtility->GetGridIconSize() );
	uiUtility->Close();
	CleanupStack::Pop( uiUtility );
    }
    

// ----------------------------------------------------------------------------
// sends response whether update required or not depending on the focus gained
// ----------------------------------------------------------------------------
//    
CGlxBinding::TResponse CGlxFullScreenThumbnailBinding::HandleFocusChanged( TBool aIsGained )
    {
    TRACER("CGlxFullScreenThumbnailBinding::HandleFocusChanged");
    if(aIsGained)
        {
        
        return EUpdateRequested;
        }
        
    return ENoUpdateNeeded;
    }
    
// ----------------------------------------------------------------------------
// Icon Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxIconBinding* CGlxIconBinding::NewL( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
     const CGlxMulIconProvider* aMulIcon, const TSize& aSize)
    {
    TRACER("CGlxIconBinding::NewL");
    CGlxIconBinding* self = CGlxIconBinding::NewLC( aMulTag, aMulIcon, aSize );
    CleanupStack::Pop( self );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxIconBinding* CGlxIconBinding::NewLC( const Alf::mulvisualitem::TVisualAttribute& aMulTag /*const char* const aMulTag*/,
     const CGlxMulIconProvider* aMulIcon, const TSize& aSize)
    {
    TRACER("CGlxIconBinding::NewLC");
    CGlxIconBinding* self = new ( ELeave ) CGlxIconBinding( aMulIcon, aSize );
    CleanupStack::PushL( self );
    self->BaseConstructL( aMulTag );
    return self;
    }

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxIconBinding::~CGlxIconBinding()
    {
    delete iIconProvider;
    }
 
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxIconBinding::CGlxIconBinding( const CGlxMulIconProvider* aIconProvider, 
       const TSize& aSize ) : iIconProvider( aIconProvider ), iIconSize(aSize)
    {
    
    }

// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxIconBinding::PopulateT( MulVisualItem& aItem, 
           const TGlxMedia& aMedia, TBool /*aIsFocused*/, TInt /*aTextureId*/ ) const
    {
    
    TRACER("CGlxIconBinding::PopulateT");
    if(iIconProvider)
    {
    SetAttributeT( aItem, MulTag(), 
        std::auto_ptr< MulVariantType >( 
        GlxIconVariantType::NewL( iIconProvider->IconIdL(aMedia), 
                iIconProvider->ResourceFileName(), iIconSize ) ) );	
    }
    
    }

// ----------------------------------------------------------------------------
// Command Binding
//-----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxCommandBinding* CGlxCommandBinding::NewL( MGlxBoundCommand* aCommand )
	{
	TRACER("CGlxCommandBinding::NewL");
	CGlxCommandBinding* self = CGlxCommandBinding::NewLC( aCommand );
	CleanupStack::Pop( self );
    return self;
	}
	
// ----------------------------------------------------------------------------
// Two phase construction
// ----------------------------------------------------------------------------
//
CGlxCommandBinding* CGlxCommandBinding::NewLC( MGlxBoundCommand* aCommand )
    {
    TRACER("CGlxCommandBinding::NewLC");
    __ASSERT_DEBUG( aCommand ,User::Invariant()); 
    CGlxCommandBinding* self = new ( ELeave ) CGlxCommandBinding();
    self->iCommand = aCommand; 
    // cannot pass aCommand since abstract MGlxCommand pointer is a few bytes off
    // from the concrete derived class pointer
    CleanupStack::PushL( self );
    return self;
    }
    
// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
//
CGlxCommandBinding::CGlxCommandBinding()
    {
    }
    
// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
//
CGlxCommandBinding::~CGlxCommandBinding()
    {
    if(iCommand)
        {
        iCommand->Close();
        }
    }    
    
// ----------------------------------------------------------------------------
// populate visual item
// ----------------------------------------------------------------------------
//
void CGlxCommandBinding::PopulateT( Alf::MulVisualItem& aItem, const TGlxMedia& /*aMedia*/,
        TBool /*aIsFocused*/, TInt /*aTextureId*/ ) const
    {
    TRACER("CGlxCommandBinding::PopulateT");
    GlxCommandBindingUtility::SetT( aItem, *iCommand );
    }
  
