/*
* Copyright (c) 2006 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  CCSXHHtmlTopicContainer class definition
*
*/


#include "CSXHHtmlTOC2.h"
#include "CSXHHtmlTOC1.h"
#include "CSXHDocument.h"
#include "CSXHAppUi.h"
#include "CSXHHtmlTopicView.h"
#include "CSXHViewIDs.h"
#include <cshelp.rsg>
#include "CsHelp.hrh"
#include "CSXHHtmlTopicContainer.h"
#include "AppLauncherForCSXH.h"
#include "csxhconstants.h"


#include <brctlinterface.h>
#include <browserlauncher.h>
#include <brctldefs.h>

#include <coemain.h>
#include <AknUtils.h>
#include <akntitle.h>
#include <aknnotewrappers.h> 
#include <AknGlobalNote.h>
#include <apmrec.h>
#include <apgcli.h>
#include <zipfile.h> 
#include <utf.h>
#include <charconv.h>
#include <bautils.h>

_LIT(KContentType,"text/html");
_LIT(KCsstextToRemovePathinfo,"<style>\n #APP_LAUNCH_LINK{display:none}\n</style>");


CBlankContainer::CBlankContainer()
    {
    }

CBlankContainer::~CBlankContainer()
    {
    }
    

void CBlankContainer::Draw( const TRect& aRect ) const
    {
    CWindowGc& gc = SystemGc();
    TRect rect = Rect();
    gc.Clear(rect);    
    }


// Standard construction sequence
CCSXHHtmlTopicContainer* CCSXHHtmlTopicContainer::NewL(const TRect& aRect,CCSXHDocument 
&aDocument,CCSXHHtmlTopicView *aView)
    {
    CCSXHHtmlTopicContainer* self = CCSXHHtmlTopicContainer::NewLC(aRect,aDocument,aView);
    CleanupStack::Pop(self);
    return self;
    }

CCSXHHtmlTopicContainer* CCSXHHtmlTopicContainer::NewLC(const TRect& aRect,CCSXHDocument 
&aDocument,CCSXHHtmlTopicView *aView)
    {
    CCSXHHtmlTopicContainer* self = new (ELeave) CCSXHHtmlTopicContainer(aDocument,aView);
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
    }

CCSXHHtmlTopicContainer::CCSXHHtmlTopicContainer(CCSXHDocument &aDocument,CCSXHHtmlTopicView *aView)
        : iTopic(NULL),iDocument(aDocument),iView(aView)
        ,iAppLauncher(NULL), iLoadHtml(EFalse), iPrevPageCount(0), iBack(EFalse)
        ,iContentLoading(ENoContent)

    {
    // no implementation required
    }

CCSXHHtmlTopicContainer::~CCSXHHtmlTopicContainer()
    {
    if(iBrCtrl)
        {
        iBrCtrl->SetFocus(EFalse);
        iBrCtrl->RemoveLoadEventObserver(this);
        delete iBrCtrl;
        iBrLibrary.Close();
        }

    delete iAppLauncher;
  
    delete iBlankContainer;

    }

void CCSXHHtmlTopicContainer::ConstructL( const TRect& aRect )
    {
    // Create a window for this application view
    CreateWindowL();

    // Set the windows size
    SetRect(aRect);
    if( KErrNone != iBrLibrary.Load( _L( "BrowserEngine.dll" ) ) )
    	{
       	HBufC* ErrorMessage = CCSXHAppUi::GetCoeEnv()->AllocReadResourceLC(
    				R_CSHELP_RETRIEVE_NO_MEMORY_TEXT );      
    	CAknGlobalNote* note = CAknGlobalNote::NewLC();
    	note->ShowNoteL( EAknGlobalInformationNote, *ErrorMessage );
    	CleanupStack::PopAndDestroy( note ); 
    	CleanupStack::PopAndDestroy( ErrorMessage ); 
    	iDocument.SetDisplayTopic( iDocument.GetPrevTopic() );
		CCSXHAppUi::GetInstance()->HandleCommandL( ECSXHOpenItem );
		User::Leave( KErrNoMemory );
    	}     

#ifdef __WINS__    
    TLibraryFunction result = iBrLibrary.Lookup( 10 ); 
#else
    TLibraryFunction result = iBrLibrary.Lookup( 1 );   
#endif    
		
	FuncPtr_CreateBrowserControlL fptr  = ( FuncPtr_CreateBrowserControlL )result;
    
    iBrCtrl = ( *fptr )(
                    this,aRect,
                    TBrCtlDefs::ECapabilityDisplayScrollBar|
                    TBrCtlDefs::ECapabilityClientResolveEmbeddedURL|
#ifndef __SERIES60_30__                                 
                    TBrCtlDefs::ECapabilityCursorNavigation|
                    TBrCtlDefs::ECapabilityWebKitLite|
#endif                                  
                    TBrCtlDefs::ECapabilityClientNotifyURL,
                    TBrCtlDefs::ECommandIdBase, this, this, NULL, this
                    // PHOREPOC-DELTA (GCC): FuncPtr_CreateBrowserControlL declares default arguments on
                    // a function-pointer typedef; RVCT applied them, standard C++ (GCC) does not. Pass
                    // the three trailing defaults (dialogs provider, window / download observers) explicitly.
                    , NULL, NULL, NULL
                    );
    iBrCtrl->SetBrowserSettingL( TBrCtlDefs::ESettingsCSSFetchEnabled, 1 );    
    iBrCtrl->AddLoadEventObserverL( this );   
    
    
    SetSelectedFontSizeL( iView->GetCurrentFontSize() );
    iBrCtrl->MakeVisible( ETrue );
    
    iBlankContainer = new ( ELeave ) CBlankContainer();
    iBlankContainer->SetRect( Rect() );
    
    ShowBrowser();
    
    ActivateL();
    }

void CCSXHHtmlTopicContainer::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc = SystemGc();
    TRect rect = Rect();
    gc.Clear(rect);    
    }
void CCSXHHtmlTopicContainer::SetAndLoadInitialTopicL(CCSXHHtmlTOC2* aTopic)
	{
	iTopic = aTopic;
	TRAPD(err,LoadHtmlL());
    if( KErrNone != err )
	    {
	    HBufC* ErrorMessage = CCSXHAppUi::GetCoeEnv()->AllocReadResourceLC(
    				R_CSHELP_RETRIEVE_NO_MEMORY_TEXT);      
    	CAknGlobalNote* note = CAknGlobalNote::NewLC();
    	note->ShowNoteL(EAknGlobalInformationNote, *ErrorMessage);
    	CleanupStack::PopAndDestroy(note); 
    	CleanupStack::PopAndDestroy(ErrorMessage); 
    	iDocument.SetDisplayTopic(iDocument.GetPrevTopic());
		CCSXHAppUi::GetInstance()->HandleCommandL(ECSXHOpenItem);
		User::Leave(KErrNoMemory);	    
	    }
 
	}

void CCSXHHtmlTopicContainer::LoadHtmlL()
    {
    iBrCtrl->MakeVisible( EFalse ); 
    SetContentLoadState( EContentLoading );
    HBufC8 *htmlBuffer = STATIC_CAST(HBufC8*,iTopic->GetTopicContentL());
    if(htmlBuffer)
        {
        CleanupStack::PushL(htmlBuffer);
        TUid uid;
        uid.iUid = KCharacterSetIdentifierIso88591;
        _LIT8(KContentType,"text/html");
        TDataType dataType(KContentType());
        iTopic->GetHtmlUrlL(iUrlNoAnchors);
        iBack = EFalse;
        ClearHistoryL();
        iLoadHtml = ETrue;
        
        //RUNTIME
        //if it is context sensitive launch, Add CSS content to remove
        //Application launch link.
        if(iView->GetViewType() == CCSXHHtmlTopicView::EContextHtmlView)
	        {
	        HBufC8* ContextHtmlbuffer = HBufC8::NewLC(htmlBuffer->Size() + 100);
	        TPtr8 bufferPtr = ContextHtmlbuffer->Des();
	        bufferPtr.Copy(KCsstextToRemovePathinfo);
	        bufferPtr.Append(htmlBuffer->Des());
	        iBrCtrl->LoadDataL(iUrlNoAnchors,*ContextHtmlbuffer,dataType,uid);	
	    	CleanupStack::PopAndDestroy(ContextHtmlbuffer);    	
	        }
        else
	        {
	        iBrCtrl->LoadDataL(iUrlNoAnchors,*htmlBuffer,dataType,uid);	
	        }
        CleanupStack::PopAndDestroy(htmlBuffer);
        }
    else
        {
        iTopic->GetHtmlUrlL(iUrlNoAnchors);
        iBrCtrl->LoadUrlL(iUrlNoAnchors);
        }
    CheckForMSK();  
    }

TKeyResponse CCSXHHtmlTopicContainer::OfferKeyEventL(const TKeyEvent &  
aKeyEvent,TEventCode  aType )
    {
    TKeyResponse result(EKeyWasNotConsumed);
    
    if ((aKeyEvent.iCode == EKeyEnter )&& (aType == EEventKey)
    	&& (IsHyperLinkFocused()))
        { 
        HandleCommandOpenHyperLinkL();
        CheckForMSK();
        return EKeyWasConsumed; 
        }
    
    if(iBrCtrl)
        {
        result = iBrCtrl->OfferKeyEventL(aKeyEvent, aType);        
		CheckForMSK();
		}       
    return result;  
    }

TInt CCSXHHtmlTopicContainer::CountComponentControls() const
    {
    return 2; 
    }

CCoeControl* CCSXHHtmlTopicContainer::ComponentControl(TInt aIndex) const
    {
    switch (aIndex)
        {
        case 0:
            return iBottomControl;
        case 1:
            return iTopControl;
        default:
            return NULL;
        }
    }
    
void CCSXHHtmlTopicContainer::HandleResourceChange(TInt aType)
    {
    CCSXHAppUi::GetInstance()->PropagateResourceChange(aType); 
    }
void CCSXHHtmlTopicContainer::HandleResourceChangeImpl(TInt aType)
    {
    if (aType == KEikDynamicLayoutVariantSwitch)
        {
        iBrCtrl->HandleResourceChange(aType);
        TRect mainRect; 
        TRect statusPaneRect;
        AknLayoutUtils::LayoutMetricsRect(AknLayoutUtils::EMainPane,mainRect);
        SetRect(mainRect);
            if(iBrCtrl)
#ifndef __SERIES60_30__             
                iBrCtrl->SetRect(Rect());
#else                       
                iBrCtrl->SetRect(mainRect);
#endif              
        DrawNow();
        }
    else
        {
        CCoeControl::HandleResourceChange(aType);   
        }
    }

void CCSXHHtmlTopicContainer::SizeChanged()
    {
#ifndef __SERIES60_30__ 
    if( iBrCtrl )
        {
        if ( iContentLoading != EContentLoading )
            {
            iBlankContainer->SetRect( Rect() );
            iBrCtrl->SetRect( Rect() );
            iBrCtrl->MakeVisible( ETrue );
            }
        else
            {
            //Update the title bar
            CEikStatusPane* sp = CCSXHAppUi::GetInstance()->StatusPane();
            CAknTitlePane* titlePane = NULL;
            TRAPD( err, titlePane = STATIC_CAST(CAknTitlePane*,
                    sp->ControlL( TUid::Uid( EEikStatusPaneUidTitle ) ) ) );
            if ( KErrNone == err )
                {
                //If can't set Null text to title, no other good way to do further, 
                //so just ignore this leave
                TRAP_IGNORE( titlePane->SetTextL( KNullDesC ) );
                }
            iBlankContainer->SetRect( Rect() );
            iBrCtrl->SetRect( Rect() );            
            }
        }
#endif  
    }

void CCSXHHtmlTopicContainer::HandleCommandBackL()
    {
    iBrCtrl->HandleCommandL(TBrCtlDefs::ECommandIdBase + TBrCtlDefs::ECommandBack);
    }
 

TBool CCSXHHtmlTopicContainer::IsPrevPageAvbl()
    {
    if (iPrevPageCount > 0)
      	{
      	return ETrue;
      	}
     else
      	{
      	return EFalse;
      	}
    } 



void CCSXHHtmlTopicContainer::ClearHistoryL()
    {
    iBrCtrl->HandleCommandL(TBrCtlDefs::ECommandIdBase + TBrCtlDefs::ECommandClearHistory);
    iPrevPageCount = 0;
    }
 
TBool CCSXHHtmlTopicContainer::ResolveEmbeddedLinkL(const TDesC& /*aEmbeddedUrl*/,
                                           const TDesC& /*aCurrentUrl*/,
                                           TBrCtlLoadContentType /*aLoadContentType*/, 
                                           MBrCtlLinkContent& /*aEmbeddedLinkContent*/)
    {
    return EFalse;
    }
// --------------------------------------------------------------------------
// Handling for
// 1) Compressed HTML content
// 2) Application Launch Links
// 3) External Hyperlinks
// --------------------------------------------------------------------------
TBool CCSXHHtmlTopicContainer::ResolveLinkL(const TDesC& aUrl, const TDesC& aCurrentUrl,
                                   MBrCtlLinkContent& aBrCtlLinkContent)
    {
#ifdef FF_HELP_TUTORIAL_MERGE
    if( KErrNotFound != aUrl.Find( KTutorialFileExtn ))
        {
        iView->LaunchTutorial();
        return ETrue;
        }
#endif // FF_HELP_TUTORIAL_MERGE
    if(NULL == iAppLauncher)
		{
		iAppLauncher = AppLauncherForCSXH::NewL();	
		}
        //Handling for application launch.
    if(iAppLauncher->LaunchAppL(aUrl,aCurrentUrl))
        return ETrue;//Link is resolved locally.
        
    if(CheckForExternalLinkL(aUrl))     
        return ETrue;//Link is resolved locally.
    
    //Remove the Anchor, if any     
    TInt DotPos = aUrl.LocateReverseF('.');
    TInt HashPos = aUrl.LocateReverseF('#');
    if(KErrNotFound != HashPos && HashPos > DotPos )
        {
        //Direct assignment does not work
        iUrlNoAnchors.Copy(KEmptyString);
        iUrlNoAnchors.Append(aUrl.Mid(0,HashPos));
        }
    else
        {
        //Direct assignment does not work
        iUrlNoAnchors.Copy(KEmptyString);
        iUrlNoAnchors.Append(aUrl);
        }   
    
    HBufC8 *htmlBuffer = CCSXHHtmlTOC2::GetContentsFromUrlL(iUrlNoAnchors,CCSXHAppUi::GetCoeEnv(),iFeatureControl);
    if(htmlBuffer)
        {
        TPtrC p(NULL,0);
        aBrCtlLinkContent.HandleResolveComplete(KContentType,p,htmlBuffer);
        delete htmlBuffer;
        iLoadHtml = EFalse;
        if (iBack)
        	{
        	iPrevPageCount--;
        	iBack = EFalse;
        	}
        else
        	{
            iPrevPageCount++;
        	}

        return ETrue;//Link is resolved locally.    
        }
        
    return  EFalse;
    }

void CCSXHHtmlTopicContainer::CancelFetchL()
    {
    if (iBrCtrl)
        {
        iBrCtrl->HandleCommandL(
        	    		(TInt)TBrCtlDefs::ECommandCancelFetch + (TInt)TBrCtlDefs::ECommandIdBase ); 
        }
    }

void CCSXHHtmlTopicContainer::CancelAll()
    {

    }


void CCSXHHtmlTopicContainer::HandleResolveComplete(const TDesC& /*aContentType*/,
                                           const TDesC& /*aCharset*/,
                                           const HBufC8* /*aContentBuf*/)
    {

    }

void CCSXHHtmlTopicContainer::HandleResolveError(TInt /*aError*/)
    {
    
    }
void CCSXHHtmlTopicContainer::RefreshL(CCSXHHtmlTOC2 *aTopic)
    {
  	SetAndLoadInitialTopicL(aTopic);
    }

// --------------------------------------------------------------------------
// This callback function is used for the following
// 1) When an HTML page is loaded, this callback function will be called. If 
// this callback is invoked for hyperlink navigation, then the display topic
// has to be updated. For example, the Application Topics option menu is
// dependent on the dipslay topic.
// 2) Updation of the softkey from Options-Close to Options-Back (& Vice-Versa)
// in the context view as a result of hyperlink traveral
// --------------------------------------------------------------------------
void CCSXHHtmlTopicContainer::HandleBrowserLoadEventL(TBrCtlDefs::TBrCtlLoadEvent aLoadEvent, 
                                    TUint /*aSize*/, TUint16 /*aTransactionId*/)
    {
    if(aLoadEvent == TBrCtlDefs::EEventLoadFinished)
        {
        ShowBrowser();
        SetContentLoadState( EContentLoadFinished );
        if ( IsVisible() )
             {
               HBufC* title = iBrCtrl->PageInfoLC(TBrCtlDefs::EPageInfoTitle);
               if(!title)
                   {
                   CleanupStack::PopAndDestroy(title);
                   return;
                   }
               
               //Update the title bar
               CEikStatusPane* sp = CCSXHAppUi::GetInstance()->StatusPane();
               CAknTitlePane* titlePane = STATIC_CAST(CAknTitlePane*, 
               sp->ControlL(TUid::Uid(EEikStatusPaneUidTitle)));
               titlePane->SetTextL(*title);
               CleanupStack::PopAndDestroy(title);
             }
        
        //For the context sensitive view, the softkey texts need to be changed.
        //add label iLoadHtml for ESNN-7N9CLN.If the html is opened by 
        //loadHtmlL, the iLoadHtml will be set to ETrue, which 
        //can guarantee the RSK keep as "CLOSE" when context is sensitive.
        if ((!iLoadHtml) && (iView->GetViewType() == CCSXHHtmlTopicView::EContextHtmlView))
        {  
            iView->SetViewTypeL(IsPrevHtmlPageAvblL() ? 
                    CCSXHHtmlTopicView::EHtmlView : CCSXHHtmlTopicView::EContextHtmlView);
        }
        
        //TSW Error:TKOI-74KE89: Help crashes when closing after viewing a link topic
        //When user presses back keys very quickly, TOC2 objects use to destroy and 
        //then browser call back function triggers which leads to crash. This check 
        //will avoid crash.
        TUid viewId = iDocument.GetDisplayTopic()->GetViewID();
		if (viewId == KCSXHToc1ViewID || viewId == KCSXHKywdToc1ViewID
			|| viewId == KCSXHToc2ViewID || viewId == KCSXHKywdToc2ViewID
			|| viewId == KCSXHToc1AppHelpsViewID || viewId == KCSXHToc2AppHelpsViewID)
			{
			return;
			}
		
        //When user clicks on Application Topics, application topics 
        //corresponding to the topic being displayed has to be opened
        //Hence update the Display topic information if required.
        
        //Check if this event is triggered for the first load after construction
        TFileName url;
        iTopic->GetHtmlUrlL(url);
        if(url.Compare(iUrlNoAnchors) == 0)
        	{
        	iDocument.SetDisplayTopic(iTopic);
        	iNewTopic = NULL;
        	//Move the code from SetAndLoadInitialTopicL
        	//TSW Error:EJZG-7E2BUV
        	ClearHistoryL();
        	return;
        	}
            
        iNewTopic = iDocument.GetHtmlTopicForUrlL(iUrlNoAnchors);            
        CheckForMSK();
        }
    // Only when loadhtml is called, in that condition, last-displayed content may show;
    // then we should hide browser.
    else  if ( iContentLoading == EContentLoading )
        {
        HideBrowser();
        }
    }
    
CCSXHHelpContentBase* CCSXHHtmlTopicContainer::GetCurrActiveObject()
    	{
    	return iNewTopic;
    	}    
// --------------------------------------------------------------------------
// Back Key Handling
// --------------------------------------------------------------------------
TBool CCSXHHtmlTopicContainer::HandleBackKeyL()
    {
    //When this view is initially created, the HTML content is loaded 
    //using the LoadDataL function and hence it will not be part of the
    //History stack but all other subsequest navigation by the user 
    //using the hyperlinks will be part of the History Stack
    if(IsPrevPageAvbl())
        {
        iBack = ETrue;
        TRAPD(res,HandleCommandBackL());
        if(res == KErrNone)
            return ETrue;
        else
            ClearHistoryL();
        }
    ClearHistoryL();
        
    return EFalse;  
    }
TBool CCSXHHtmlTopicContainer::IsPrevHtmlPageAvblL()
    {
    if(IsPrevPageAvbl())
        return ETrue;
    
    return EFalse;  
    }

void CCSXHHtmlTopicContainer::SetSelectedFontSizeL(TInt aValue)
    {
    iBrCtrl->SetBrowserSettingL(TBrCtlDefs::ESettingsFontSize,aValue);  
    }
    
TInt CCSXHHtmlTopicContainer::GetCurrentValueL(TInt aSetting)
    {
        return iBrCtrl->BrowserSettingL(aSetting);  
    }   

void CCSXHHtmlTopicContainer::UpdateSoftkeyL(TBrCtlKeySoftkey /*aKeySoftkey*/,
                                    const TDesC& /*aLabel*/,
                                    TUint32 /*aCommandId*/,
                                    TBrCtlSoftkeyChangeReason /*aBrCtlSoftkeyChangeReason*/)
    {
    }

void CCSXHHtmlTopicContainer::UpdateBrowserVScrollBarL(TInt /*aDocumentHeight*/, 
                                              TInt /*aDisplayHeight*/,
                                              TInt /*aDisplayPosY*/ ) 
    {
    if(iBrCtrl)
        CheckForMSK();
}
void CCSXHHtmlTopicContainer::UpdateBrowserHScrollBarL(TInt /*aDocumentWidth*/, 
                                              TInt /*aDisplayWidth*/,
                                              TInt /*aDisplayPosX*/ )
    {
    if(iBrCtrl)
        CheckForMSK();
    }
void CCSXHHtmlTopicContainer::NotifyLayoutChange( TBrCtlLayout /*aNewLayout*/ )
    {
    }
        
void CCSXHHtmlTopicContainer::UpdateTitleL( const TDesC& /*aTitle*/ )
    {
    }

void CCSXHHtmlTopicContainer::CheckForMSK()
    {
    if(IsHyperLinkFocused())
        {
        iView->SetMiddleSoftKey(ETrue); 
        }
    else
        {
        iView->SetMiddleSoftKey(EFalse);    
        }
    }
    
TBool CCSXHHtmlTopicContainer::IsHyperLinkFocused() 
    {
    if(TBrCtlDefs::EElementAnchor == iBrCtrl->FocusedElementType())
        return ETrue;
    else
        return EFalse;
    }
    
void CCSXHHtmlTopicContainer::HandleCommandOpenHyperLinkL()
    {
    iBrCtrl->HandleCommandL(TBrCtlDefs::ECommandIdBase + TBrCtlDefs::ECommandOpen);
    }

TBool CCSXHHtmlTopicContainer::CheckForExternalLinkL(const TDesC& aUrl)
	{
    TBool Result = EFalse;
        
    if( KErrNotFound != aUrl.Find(_L("http://")) |
        KErrNotFound != aUrl.Find(_L("https://")) |
        KErrNotFound != aUrl.Find(_L("ftp://")))
        {
        iAppLauncher->LaunchBrowserNGL(aUrl);           
        Result = ETrue;
        }
        
    return Result;
	}

void CCSXHHtmlTopicContainer::SetContentLoadState( TContentLoadState aLoadState )
    {
    if ( iContentLoading == aLoadState )
        {
        return;
        }
    else if ( NeedRefresh( aLoadState ) )
        {
        iContentLoading = aLoadState;
        SizeChanged();
        }
    else
        {
        iContentLoading = aLoadState;
        }
    }

TBool CCSXHHtmlTopicContainer::NeedRefresh( const TContentLoadState aNewLoadState ) const
    {
    if ( 
            ( iContentLoading == EContentLoading && aNewLoadState != EContentLoading )
            ||( iContentLoading != EContentLoading && aNewLoadState == EContentLoading )
        )
        return ETrue;
    else
        return EFalse;
    }

void CCSXHHtmlTopicContainer::HideBrowser()
    {
    SetTopAndBottomContainer( iBlankContainer, iBrCtrl );
    }

void CCSXHHtmlTopicContainer::ShowBrowser()
    {
    SetTopAndBottomContainer( iBrCtrl, iBlankContainer );
    }

void CCSXHHtmlTopicContainer::SetTopAndBottomContainer( CCoeControl* aTopControl, 
        CCoeControl* aBottomControl )
    {
    iTopControl = aTopControl;
    iBottomControl = aBottomControl;
    }
    
//end of the file

