/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors: Juha Kauppinen, Mika Hokkanen
* 
* Description: Photo Browser
*
*/
 

// INCLUDE FILES
#include "ImagicAppUi.h"
#include <IEImage.h>
#include "ImagicViewBrowser.h"
#include "Imagic.hrh"
#include "MSVSTD.HRH"
#include <avkon.hrh>
#include <ImageConversion.h>
#include <aknutils.h>
#include "ImagicContainerBrowser.h"

//statuspane
#include <eikbtgpc.h> 
#include <avkon.rsg>

//for loading text from resource
#include <aknnotewrappers.h>
#include <stringloader.h>

#include <PhotoBrowser.rsg>
#include "ImagicUtils.h"
#include <apacmdln.h>
#include <aknsgcc.h>
#include <e32property.h>
extern TBool gPheSecondInstance;   // ImagicApp.cpp
#include <apgtask.h>
#include <apgwgnam.h>
#include <e32debug.h>




// ================= MEMBER FUNCTIONS =======================
//
// ----------------------------------------------------------
// CImagicAppUi::ConstructL()
// ----------------------------------------------------------
//
// PHOREPOC PATCH. A second launch of Photo Browser (File Manager -> StartDocument) cannot use TApaTask::SwitchOpenFile:
// the window server wants the SwEvent capability for it, which Photo Browser does not have (and an executable may only load
// DLLs that hold all of its capabilities). So the second process, which has the same secure id, writes the document name to a
// property of the first one (category = own secure id, writable by any process) and the first one picks it up here.
const TUint KPheOpenKey = 1;
class CPheOpenWatcher : public CActive
    {
public:
    CPheOpenWatcher(CImagicAppUi& aUi, TUid aCategory) : CActive(EPriorityStandard), iUi(aUi)
        {
        CActiveScheduler::Add(this);
        if (iProp.Attach(aCategory, KPheOpenKey) == KErrNone)
            {
            iProp.Subscribe(iStatus);
            SetActive();
            }
        }
    ~CPheOpenWatcher()
        {
        Cancel();
        iProp.Close();
        }
private:
    void RunL()
        {
        iProp.Subscribe(iStatus);
        SetActive();
        TFileName name;
        if (iProp.Get(name) == KErrNone && name.Length() > 0)
            TRAP_IGNORE(iUi.OpenFileL(name));
        }
    void DoCancel()
        {
        iProp.Cancel();
        }
    CImagicAppUi& iUi;
    RProperty iProp;
    };

void CImagicAppUi::ConstructL()
    {
    DP0_IMAGIC(_L("CImagicAppUi::ConstructL++"));
    // PHOREPOC PATCH: a second process (gPheSecondInstance, set in ImagicApp.cpp) hands its document to this one (see CPheOpenWatcher)
    
    //CArrayFix<TCoeHelpContext>* buf = CCoeAppUi::AppHelpContextL();
    //HlpLauncher::LaunchHelpApplicationL(iEikonEnv->WsSession(), buf);

    
    iTNGenerationOnGoing = ETrue;
    iMenuOn = EFalse;
    iAppForeGround = ETrue;
    
    //Set the font
    //SetFont();
    
    iImagesLoaded = EFalse;
    iImageIndex = 0;     
    iWzContainerSatus = EFalse;
    //iTotalNumOfImages = 0;
    //iNumOfImagesLoaded = 0;
    //iNumOfFacesLoaded = 0;
    iNumberOfIterations = 0;
    iUIDrawMode = EImages;
    iBrowserContainer = NULL;
#ifdef USE_OOM    
    ROomMonitorSession oomMonitor;
    oomMonitor.Connect();
    TInt errorCode = oomMonitor.RequestFreeMemory( 1024*12 );
    
    if ( errorCode != KErrNone )
        {
        // try one more time 
        errorCode = oomMonitor.RequestFreeMemory( 1024*12 );
        }
    oomMonitor.Close();
#endif    
    User::LeaveIfError(iFileServer.Connect());
    
    //Initialises this app UI with standard values.
    //The application’s standard resource file will be read unless
    //the ENoAppResourceFile or ENonStandardResourceFile flags are passed.
    BaseConstructL(0x08 | EAknEnableSkin); // Use ELayoutAwareAppFlag (0x08) to make the application support scalable UI on FP3 devices.
    if (gPheSecondInstance)
        return;   // PHOREPOC PATCH: this process only hands its document to the running instance (ProcessCommandParametersL)
    {
    const TUid cat = TUid::Uid(RProcess().SecureId().iId);
    RProperty::Define(cat, KPheOpenKey, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(TSecurityPolicy::EAlwaysPass));
    iOpenWatcher = new (ELeave) CPheOpenWatcher(*this, cat);
    }

    //Create engine and trap if there is error
    iIEngine = CIEEngine::NewL(*this);
	CleanupStack::PushL(iIEngine);
    
    //Browser view
    CImagicViewBrowser* viewBrowser = new (ELeave) CImagicViewBrowser;
    CleanupStack::PushL( viewBrowser );
    viewBrowser->ConstructL(this);
    AddViewL( viewBrowser );      // transfer ownership to CAknViewAppUi
    CleanupStack::Pop( viewBrowser );
    iViewIdBrowser = viewBrowser->Id(); // view id to get view from CAknViewAppUi
    
    SetDefaultViewL( *viewBrowser );
    SetActiveView(BrowserView);
    
    //disable statuspane to get full screen
    StatusPane()->MakeVisible(EFalse);
    
    //Creating Utility class
    iImagicUtils = CImagicUtils::NewL(iFileServer);
    
    //Create timer to release Browser view resources and init opengl
    iPeriodic = CPeriodic::NewL( CActive::EPriorityIdle );
    
    //Force orientation to be always landscape
    SetOrientationL(CAknAppUiBase::EAppUiOrientationLandscape);
    //SetOrientationL(CAknAppUiBase::EAppUiOrientationPortrait);

	CleanupStack::Pop(iIEngine);	

    DP0_IMAGIC(_L("CImagicAppUi::ConstructL--"));
    }


CArrayFix<TCoeHelpContext>* CImagicAppUi::HelpContextL() const
    {
    /*
    //#warning "Please see comment about help and UID3..."
    CArrayFixFlat<TCoeHelpContext>* array = new(ELeave)CArrayFixFlat<TCoeHelpContext>(1);
    CleanupStack::PushL(array);
    array->AppendL(TCoeHelpContext(KUidrsdApp, KGeneral_Information));
    CleanupStack::Pop(array);
    return array;
    */
    }

void CImagicAppUi::CImagicAppUiReady()
    {
    iIEngine->AppUIReady();
    }

// ----------------------------------------------------
// CImagicAppUi::~CImagicAppUi()
// Destructor
// Frees reserved resources
// ----------------------------------------------------
//
CImagicAppUi::~CImagicAppUi()
    {
    DP0_IMAGIC(_L("CImagicAppUi::~CImagicAppUi++"));
    if(iImagicUtils)
        {
        delete iImagicUtils;
        iImagicUtils = NULL;
        }
    
    iWizardBitmapArray.Close();
    
    delete iOpenWatcher;
    if(iPeriodic && iPeriodic->IsActive())
        iPeriodic->Cancel();
    delete iPeriodic;

    // Doesn't delete engine yet, since container needs it when destroyed!
    //DestructEngine();
    
    iFileServer.Close();

    DP0_IMAGIC(_L("CImagicAppUi::~CImagicAppUi--"));
    }

void CImagicAppUi::DestructEngine()
    {
    DP0_IMAGIC(_L("CImagicAppUi::DestructEngine++"));    
    delete iIEngine;
    iIEngine = NULL;
    DP0_IMAGIC(_L("CImagicAppUi::DestructEngine--"));    
    }


/*TInt CImagicAppUi::GetErrorCode()
    {
    return iEngineCreationError;
    }*/
  
// ------------------------------------------------------------------------------
// CImagicAppUi::DynInitMenuPaneL(TInt aResourceId,CEikMenuPane* aMenuPane)
//  This function is called by the EIKON framework just before it displays
//  a menu pane. Its default implementation is empty, and by overriding it,
//  the application can set the state of menu items dynamically according
//  to the state of application data.
// ------------------------------------------------------------------------------
//
void CImagicAppUi::DynInitMenuPaneL(
    TInt /*aResourceId*/,CEikMenuPane* /*aMenuPane*/)
    {
    DP0_IMAGIC(_L("CImagicAppUi::DynInitMenuPaneL"));
    }


void CImagicAppUi::BrowserContainerInitialized()
    {
    if(((CImagicViewBrowser*) View(iViewIdBrowser))->GetContainer() != NULL)
        {
        iBrowserContainer = ((CImagicViewBrowser*) View(iViewIdBrowser))->GetContainer();
        iBrowserContainer->ImageListChanged(0, EFalse); // TODO: cheap trick to update coords
        }
    }


// ------------------------------------------------------------------------------
// CImagicAppUi::HandleForegroundEventL(TBool aForeground)
//  This function is called by the  framework when the screen loses or gains focus.
//   i.e. when it goes to the background or to the foreground. Incoming call
//   softnote is an example.
// This event applies to the entire application, all views.
// ------------------------------------------------------------------------------
//
void CImagicAppUi::HandleForegroundEventL(TBool aForeground)
    {
    DP0_IMAGIC(_L("CImagicAppUi::HandleForegroundEventL++"));
    
    //SetOrientationL(CAknAppUiBase::EAppUiOrientationPortrait);
    if(iBrowserContainer)
        if (aForeground)
            {
            DP0_IMAGIC(_L("CImagicAppUi::HandleForegroundEventL - App Foreground"));
            
            //We were switched to foreground
            iAppForeGround = ETrue;
            //ScreenImmeadetaUpdate();
            
            if(iPeriodic && iPeriodic->IsActive())
                iPeriodic->Cancel();
            
            iIEngine->StartAccSensorMonitoring();
            
            iBrowserContainer->SetDeleteTextures(EFalse);
            
            iAppActiveState = ETrue;
            
            if(iViewNro == BrowserView)
                {
                if(iBrowserContainer && !iBrowserContainer->IsOpenGLInit())
                    {
                    iBrowserContainer->InitAfterPowerSaveL();
                    }
                else
                    {
                    if(iBrowserContainer)
                        iBrowserContainer->EnableDisplayDraw();
                    }
                }
            
            if(iBrowserContainer)
                {
                iBrowserContainer->DrawNow();
                iBrowserContainer->EnableDisplayDraw();
                }
            }
        else
            {//We were switched to background
            DP0_IMAGIC(_L("CImagicAppUi::HandleForegroundEventL - App Background"));
            
            iAppForeGround = EFalse;
            //ScreenImmeadetaUpdate();
            
            if(iViewNro == BrowserView)
                {
                //... disable frame loop timer ...
                //iBrowserContainer->DisableDisplayDraw();
                        
                //... start a timer for 3 seconds to call to a power save callback ...
                iPeriodic->Start( 3000000, 1000000000, TCallBack( CImagicAppUi::TimerCallBack, this ) );
                //iBrowserContainer = ((CImagicViewBrowser*) View(iViewIdBrowser))->GetContainer();
                }
            
            //iIEngine->StopAccSensorMonitoring();
            
            iAppActiveState = EFalse;
            if(iBrowserContainer)
                {
                iBrowserContainer->DrawNow();
                iBrowserContainer->DisableDisplayDraw();
                }
            }
            
    DP0_IMAGIC(_L("CImagicAppUi::HandleForegroundEventL--"));
    }

//Power save timer callback function
//Cleans memory allocations for openGl draving
TInt CImagicAppUi::TimerCallBack(TAny* aInstance)
    {
    DP0_IMAGIC(_L("CImagicAppUi::TimerCallBack++"));
    
    CImagicAppUi* instance = (CImagicAppUi*) aInstance;
    
    instance->iIEngine->StopAccSensorMonitoring();
    
    if(instance->iViewNro == BrowserView)
        {
        if(instance->iBrowserContainer && instance->iBrowserContainer->IsOpenGLInit())
            {
            DP0_IMAGIC(_L("CImagicAppUi::TimerCallBack - DeleteTextures"));
            //instance->iBrowserContainer->DeleteTextures();
            instance->iBrowserContainer->SetDeleteTextures(ETrue);
            }
        }
    
    DP0_IMAGIC(_L("CImagicAppUi::TimerCallBack--"));
    return 0;
    }

void  CImagicAppUi::SetTNGenerationFlag(TBool aValue)
    {
    iTNGenerationOnGoing = aValue;
    }

// ----------------------------------------------------
// CImagicAppUi::HandleKeyEventL(
//     const TKeyEvent& aKeyEvent,TEventCode /*aType*/)
// Here we handle key events: Right and left arrow key
//   to change view.
// ----------------------------------------------------
//
TKeyResponse CImagicAppUi::HandleKeyEventL(const TKeyEvent& /*aKeyEvent*/, TEventCode /*aType*/)
    {
    DP0_IMAGIC(_L("CImagicAppUi::HandleKeyEventL"));
    //No need to handle events here
    return EKeyWasNotConsumed;
    }

// ----------------------------------------------------
// CImagicAppUi::HandleCommandL(TInt aCommand)
// Here we handle commands on the application level.
// In addition, each view has their own HandleCommandL()
// ----------------------------------------------------
//
void CImagicAppUi::HandleCommandL(TInt aCommand)
    {
    DP0_IMAGIC(_L("CImagicAppUi::HandleCommandL"));
    
    switch ( aCommand )
        {
        case EEikCmdExit:
            {
            iIEngine->Stop();
            
            // send to background
            TApaTask apaTask( CEikonEnv::Static()->WsSession() );
            apaTask.SetWgId( iCoeEnv->RootWin().Identifier() );
            apaTask.SendToBackground();

            // Wait until engine is stopped
            while (iIEngine->IsRunning())
                {
                User::After(200000);   // 200ms
                }
            DP0_IMAGIC(_L("CImagicAppUi::HandleCommandL end wait"));
			if(iTNGenerationOnGoing)
				{
				TInt i = KErrNone;
	            //iIEngine->StopFaceDetection(i);
    	        iIEngine->StopTNGeneration(i);
				}
                
            Exit();
            break;
            }
            
        case EImagicCmdViewCmd1:
            {
			break;
            }
        // You can add your all application applying commands here.
        // You would handle here menu commands that are valid for all views.
        }
    
    }

TInt CImagicAppUi::ExitTimerCallBack(TAny* aInstance)
    {
    CImagicAppUi* instance = (CImagicAppUi*) aInstance;
    instance->iNumberOfIterations++;
    if(instance->iTNGenerationOnGoing)
        {
        if(instance->iNumberOfIterations == 10)
            {
            instance->iNumberOfIterations = 0;
            instance->iPeriodic->Cancel();
            //instance->CancelExitDialog();
            instance->iImagicUtils->CancelWaitDialog();
            User::Exit(KErrNone);
            }
        else
            {
            //nothing.. continue...
            }
        }
    else
        {
        instance->iPeriodic->Cancel();
        //instance->CancelExitDialog();
        instance->iImagicUtils->CancelWaitDialog();
        
        User::Exit(KErrNone);
        }
        
    return 0;
    }

// -----------------------------------------------------------------------------
// CImagicAppUi::HandleResourceChangeL( TInt aType )
// Called by framework when layout is changed.
// -----------------------------------------------------------------------------
//
void CImagicAppUi::HandleResourceChangeL( TInt aType )
    {
    DP0_IMAGIC(_L("CImagicAppUi::HandleResourceChangeL"));
    
    //on = aType = 268457666, off = aType = 268457667     
    
    if(iBrowserContainer != NULL)
        {
        if(aType == 268457666)
            {
            iMenuOn = ETrue;
            //ScreenImmeadetaUpdate();
            if(iBrowserContainer)
                {
                iBrowserContainer->SetScreenImmeadetaUpdate(ETrue);
                iBrowserContainer->DisableDisplayDraw();
                }
            }
        else if(aType == 268457667)
            {
            iMenuOn = EFalse;
            //ScreenImmeadetaUpdate();
            
            if(iBrowserContainer)
                {
                iBrowserContainer->SetScreenImmeadetaUpdate(EFalse);
                iBrowserContainer->EnableDisplayDraw();    
                }
            
            }
        
        iBrowserContainer->DrawNow();
        }
    
    CAknAppUi::HandleResourceChangeL( aType );
    
    // ADDED FOR SCALABLE UI SUPPORT
    // *****************************
    if ( aType==KEikDynamicLayoutVariantSwitch )
        {
		((CImagicViewBrowser*) View( iViewIdBrowser) )->HandleClientRectChange(  );
		}
    
    }

TBool CImagicAppUi::IsAppOnTop()
    {
    if(iMenuOn)
        {
        DP0_IMAGIC(_L("CImagicAppUi::IsAppOnTop: EFalse"));
        return EFalse;
        }
    else if(!iAppForeGround)
        {
        DP0_IMAGIC(_L("CImagicAppUi::IsAppOnTop: EFalse"));
        return EFalse;
        }
    else
        {
        DP0_IMAGIC(_L("CImagicAppUi::IsAppOnTop: ETrue"));
        return ETrue;
        }
    }

void CImagicAppUi::ScreenImmeadetaUpdate()
    {
    if(iMenuOn || !iAppForeGround)
        iBrowserContainer->SetScreenImmeadetaUpdate(ETrue);
    else
        iBrowserContainer->SetScreenImmeadetaUpdate(EFalse);
    }


void CImagicAppUi::SetImageIndex(TInt aIndex)
    {
    DP0_IMAGIC(_L("CImagicAppUi::SetImageIndex"));
    
    if(aIndex >= iIEngine->GetTotalNumOfImages())
        aIndex = 0;
    if(aIndex < 0)
        aIndex = iIEngine->GetTotalNumOfImages()-1;
    
    iImageIndex = aIndex;
    }

TInt CImagicAppUi::GetImageIndex()
	{
	DP0_IMAGIC(_L("CImagicAppUi::GetImageIndex"));
	return iImageIndex;
	}

#ifdef _ACCELEROMETER_SUPPORTED_
void CImagicAppUi::ImageRotated(TImagicDeviceOrientation aDeviceOrientation)
    {
    DP1_IMAGIC(_L("CImagicAppUi::ImageRotated, angle: %d"),aDeviceOrientation);
    iBrowserContainer->PhoneRotated(aDeviceOrientation);
    }
#endif

void CImagicAppUi::SetActiveView(TUid aViewNro)
	{
	DP0_IMAGIC(_L("CImagicAppUi::SetActiveView"));
	iViewNro = aViewNro;
	}

TUid CImagicAppUi::GetActiveView()
    {
    DP0_IMAGIC(_L("CImagicAppUi::GetActiveView"));
    return iViewNro;
    }


//Callback from engine that loaded Bitmap image is ready for drawing
void CImagicAppUi::ImagesLoadedL(TInt aError)
    {
    DP0_IMAGIC(_L("CImagicAppUi::ImagesLoaded++"));
    
    if(iViewNro == BrowserView)
        {
        ((CImagicViewBrowser*) View(iViewIdBrowser))->BitmapLoadedByEngineL(aError);
        }
    
    DP0_IMAGIC(_L("CImagicAppUi::ImagesLoaded--"));
    }


//To get engine interface for other class usage
CIEEngine* CImagicAppUi::GetEngine()
    {
    DP0_IMAGIC(_L("CImagicAppUi::GetEngine"));
    
    return iIEngine;
    }


void CImagicAppUi::SetUIDrawMode(TImageArrayMode aMode)
    {
    iUIDrawMode = aMode;
    }

/*
TImageArrayMode CImagicAppUi::GetUIDrawMode()
    {
    return iUIDrawMode;
    }

TRgb CImagicAppUi::GetTransparentWhite()
    {
    return iTransparentWhite;
    }

TRgb CImagicAppUi::GetTransparentBlack()
    {
    return iTransparentBlack;
    }

const CFont* CImagicAppUi::GetFont()
    {
    return iFont;
    }
*/

CImagicUtils* CImagicAppUi::GetImagicUtils()
    {
    return iImagicUtils;
    }


/*void CImagicAppUi::SetFont()
    {
    DP0_IMAGIC(_L("CImagicAppUi::SetFont"));
    
    // set the font
    iFont = AknLayoutUtils::FontFromId(EAknLogicalFontPrimaryFont);
    //Set alpha colors
    iTransparentWhite=TRgb(KRgbWhite);
    iTransparentWhite.SetAlpha(128);
    iTransparentBlack=TRgb(KRgbBlack);
    iTransparentBlack.SetAlpha(128+64);
    }*/


void CImagicAppUi::ImageListChanged(TInt aIndex, TBool aAdded)
    {
    DP2_IMAGIC(_L("CImagicAppUi::ImageListChanged %d %d"), aIndex, aAdded);
    if (iBrowserContainer)
        iBrowserContainer->ImageListChanged(aIndex, aAdded);
    }

//This is called when single face Detection has been completed
void CImagicAppUi::SingleFaceDetectionComplete()
    {
    DP0_IMAGIC(_L("CImagicAppUi::SingleFaceDetectionComplete"));
    
    //((CImagicViewBrowser*) View(iViewIdBrowser))->SingleFaceDetectionComplete();
    }

//Callback function from engine that BackGround Face Detection has been completed
void CImagicAppUi::FaceDetectionComplete()
    {
    DP0_IMAGIC(_L("CImagicAppUi::FaceDetectionComplete"));
    
    ((CImagicViewBrowser*) View(iViewIdBrowser))->FaceDetectionComplete();
    }

//Callback function from engine that Face Browsing creation has been completed

void CImagicAppUi::SingleTNCreationCompletedL(TInt /*aIndex*/, TThumbSize aTnRes)
    {
    DP1_IMAGIC(_L("CImagicAppUi::SingleTNCreationCompletedL - res: %d"),aTnRes);
    
    iBrowserContainer->NewImageAdded();
    iBrowserContainer->SetLoadingOn(ETrue);
    //iBrowserContainer->DrawScreen();
    iBrowserContainer->DrawNow();
    }

//Callback function from engine that TN creation has been completed
void CImagicAppUi::TNCreationCompleteL(TThumbSize aTnRes)
    {
    DP0_IMAGIC(_L("CImagicAppUi::TNCreationComplete++"));
    
    iTNGenerationOnGoing = EFalse; 
    ((CImagicViewBrowser*) View(iViewIdBrowser))->TNCreationComplete();
    
    iBrowserContainer->DrawNow();
    // PHOREPOC PATCH: the thumbnail run ends with the grid shown again; put the picture File Manager opened back
    if (iReapplyName.Length() > 0)
        {
        if (iBrowserContainer->GetDrawMode() == CImagicContainerBrowser::EGrid)
            {
            iOpenFileName = iReapplyName;
            OpenPendingFile();
            }
        iReapplyName.Zero();
        }
    
            
    /*TApplicationFeature appFeature = ((CImagicViewBrowser*)View(iViewIdBrowser))->GetAppFeature(); 
    
    //This is in case we were editing and we did not have 320x320 tn created
    if(appFeature == EAppFeatureEditing && aTnRes == ESize32x32)
        {
        iTNGenerationOnGoing = EFalse; 
        ((CImagicViewBrowser*) View(iViewIdBrowser))->TNCreationComplete();
        }
    
    else if(appFeature == EAppFeatureNone )
        {
        iTNGenerationOnGoing = EFalse; 
        ((CImagicViewBrowser*) View(iViewIdBrowser))->TNCreationComplete();
        }
    
    else if((appFeature == EAppFeatureEditing || appFeature == EAppFeatureCropping) &&  (aTnRes == ESize512x512 || aTnRes == ENotDefined))
        {
        iTNGenerationOnGoing = EFalse;
        ((CImagicViewBrowser*) View(iViewIdBrowser))->TNCreationComplete();
        
        }*/
    DP0_IMAGIC(_L("CImagicAppUi::TNCreationComplete--"));
    }


TInt CImagicAppUi::DeleteImage(TInt aIndex)
    {
    DP0_IMAGIC(_L("CImagicAppUi::DeleteImage++"));
    
    TInt err = iIEngine->DeleteFile(aIndex);
    
    DP0_IMAGIC(_L("CImagicAppUi::DeleteImage--"));
    
    return err;
    }

void CImagicAppUi::AllFilesScanned()
    {
    DP0_IMAGIC(_L("CImagicAppUi::AllFilesScanned++"));
    
    if(iIEngine->GetTotalNumOfImages() <= 0)
        GetImagicUtils()->ExecuteQueryDialog(0/*GetErrorCode()*/, R_NO_IMAGES_DIALOG);
    
    iBrowserContainer->DrawNow();
    OpenPendingFile();
    DP0_IMAGIC(_L("CImagicAppUi::AllFilesScanned--"));
    }

TInt CImagicAppUi::GetGleMaxRes()
    {
    return iBrowserContainer->GetGleMaxRes();
    }



// End of File


// PHOREPOC PATCH (not in the 2010 release): open the one picture File Manager hands over. The picture is looked up in the
// engine's list (the folders it scans) and shown in the one-by-one view.
void CImagicAppUi::OpenFileL(const TDesC& aFileName)
    {
    RDebug::Printf("PH: imagic OpenFileL (a file handed over by a second launch), length %d", aFileName.Length());
    iOpenFileName = aFileName;
    OpenPendingFile();
    }

TBool CImagicAppUi::ProcessCommandParametersL(CApaCommandLine& aCommandLine)
    {
    if (gPheSecondInstance)
        {
        // the document name, or the file handle File Manager passes (the name is then taken from the handle)
        TFileName doc(aCommandLine.DocumentName());
        RFile file;
        aCommandLine.GetFileByHandleL(file);
        if (doc.Length() == 0 && file.SubSessionHandle() != KNullHandle)
            file.FullName(doc);
        file.Close();
        RDebug::Printf("PH: imagic second launch, document name length %d", doc.Length());
        if (doc.Length() > 0)
            {
            const TInt r = RProperty::Set(TUid::Uid(RProcess().SecureId().iId), KPheOpenKey, doc);
            RDebug::Printf("PH: imagic handed the document to the running instance (property) = %d", r);
            RWsSession& ws = iEikonEnv->WsSession();
            const TInt myWg = iEikonEnv->RootWin().Identifier();
            TInt wgId = 0;
            do
                {
                CApaWindowGroupName::FindByAppUid(Application()->AppDllUid(), ws, wgId);
                if (wgId >= 0 && wgId != myWg)
                    {
                    CAknSgcClient::MoveApp(wgId, ESgcMoveAppToForeground);
                    break;
                    }
                } while (wgId >= 0);
            }
        User::Leave(KLeaveExit);
        }

    // the document name only: GetFileByHandleL() adopts a handle from the creator and panics (KERN-EXEC 0) when the app is started without one
    iOpenFileName = aCommandLine.DocumentName();
    RDebug::Printf("PH: imagic first launch, document name length %d", iOpenFileName.Length());
    OpenPendingFile();
    return CAknViewAppUi::ProcessCommandParametersL(aCommandLine);
    }

void CImagicAppUi::OpenPendingFile()
    {
    if (iOpenFileName.Length() == 0 || !iIEngine || !iBrowserContainer)
        return;
    const TInt total = iIEngine->GetTotalNumOfImages();
    for (TInt i = 0; i < total; i++)
        {
        TFileName n;
        if (iIEngine->GetImageName(i, n, EFullSize) == KErrNone && n.CompareF(iOpenFileName) == 0)
            {
            iReapplyName = iOpenFileName;
            iOpenFileName.Zero();
            SetImageIndex(i);
            iBrowserContainer->SetCurrentIndex(i);
            ShowCurrentOneByOne();
            return;
            }
        }
    // not in the list (a folder the finder does not scan): once the scan is over, add the picture and show it
    if (iIEngine->IsScanningFiles())
        return;
    TFileName name = iOpenFileName;
    iOpenFileName.Zero();
    RFs fs;
    if (fs.Connect() != KErrNone)
        return;
    TEntry entry;
    TSize size;
    TInt err = fs.Entry(name, entry);
    if (err == KErrNone)
        {
        CImageDecoder* dec = NULL;
        TRAP(err, dec = CImageDecoder::FileNewL(fs, name));
        if (err == KErrNone)
            {
            size = dec->FrameInfo().iOverallSizeInPixels;
            delete dec;
            }
        }
    fs.Close();
    if (err != KErrNone)
        return;
    CIEImageList& list = iIEngine->GetImageList();
    CImageData* data = list.GetImageData(name);
    if (!data)
        {
        TRAP(err, data = list.CreateImageDataL(name, entry.iModified, 0));
        if (err != KErrNone || !data)
            return;
        data->SetFileTime(entry.iModified);
        data->SetSize(size);
        list.AddImage(data);
        }
    const TInt idx = list.GetImageIndex(data);
    if (idx < 0)
        return;
    iReapplyName = name;
    SetImageIndex(idx);
    iBrowserContainer->SetCurrentIndex(idx);
    ShowCurrentOneByOne();
    }

// PHOREPOC PATCH: open the current picture the way the OK key does (the grid's own select path, which sets up the one-by-one view)
void CImagicAppUi::ShowCurrentOneByOne()
    {
    RDebug::Printf("PH: show picture %d, view mode was %d (0 = grid)", iBrowserContainer->GetCurrentIndex(), (TInt)iBrowserContainer->GetDrawMode());
    if (iBrowserContainer->GetDrawMode() != CImagicContainerBrowser::EGrid)
        iBrowserContainer->SetDrawMode(CImagicContainerBrowser::EGrid);
    if (iBrowserContainer->GetDrawMode() == CImagicContainerBrowser::EGrid)
        {
        TKeyEvent ev;
        ev.iCode = EKeyEnter;
        ev.iScanCode = EStdKeyEnter;
        ev.iModifiers = 0;
        ev.iRepeats = 0;
        TRAP_IGNORE(iBrowserContainer->OfferKeyEventL(ev, EEventKey));
        }
    }
