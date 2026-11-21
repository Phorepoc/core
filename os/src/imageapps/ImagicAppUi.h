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

#ifndef IMAGICAPPUI_H
#define IMAGICAPPUI_H

// INCLUDES
#include <eikapp.h>
#include <eikdoc.h>
#include <e32std.h>
#include <coeccntx.h>
#include <aknviewappui.h>       // View AppUi adds View handling to AppUi
#include <aknnavide.h>
#include <BAUTILS.H> 
#include <exifmodify.h> 
#include "ImagicConsts.h"
#include "HLPLCH.H" 
//#include <oommonitorclient.h>
//#include <oommonitorsession.h>

//Hitchcock
#ifdef HITCHCOCK
#include <hitchcock.h>
#include "ImagicHUIViewRotate.h"
#endif
// Image converter library API header
#include <ImageConversion.h>
// Bitmap transforms API header
#include <BitmapTransforms.h>
//#include "IEFileLoader.h"

//IE engine header
#include <IEEngine.h>
#include "debug.h"
#include <aknwaitdialog.h>
#include "ImagicContainerBrowser.h"
#include "IEImage.h"

// UID of view
//const TUid EditMainView = {1};
const TUid DummyUID = {1};
const TUid BrightnessAdjustment = {2};
const TUid ContrastAdjustement = {3};
const TUid BrowserView = {4};
const TUid Crop = {5};
const TUid SharpnessAdjustement = {6};
const TUid Resize = {7};
const TUid Zoom = {8};
const TUid Rotate = {9};
const TUid ColorAdjustement = {10};
const TUid GammaAdjustement = {11};
const TUid LocalContrastAdjustement = {12};
const TUid HUIRotate = {13};
const TUid HUICrop = {14};

//Error IDs
const TUid KErrorId= { -1 };    // MessageUid to send


// FORWARD DECLARATIONS
class CImagicHUIViewRotate;
class CImagicHUIViewCrop;
class CImagicViewBrowser;
class CImagicContainerBrowser;
class CImagicContainerCrop;
class CImagicViewWizardEdit; 
//IE Engine
class MIEEngineObserver;
class CIEEngine;

class CImagicUtils;
 

enum TStatus
    {
    ENormal = 1,
    EProcessing,
    EDone,
    ESaving,
    EError
    };

enum TImageQuality
    {
    ELQTextIndex,//OpenGL 128x128 texture index
    EHQTextIndex,//OpenGL 512x512 texture index
    ESuperHQTextIndex//OpenGL 2048x2048 texture index
    };

/**
* Application UI class. CAknViewAppUi derives from CAknAppUi and has view handling methods added.
* Provides support for the following features:
* - EIKON control architecture
* - view architecture
* - status pane
* 
*/
//CAknViewAppUi derived from CEikAppUi
class CImagicAppUi : public CAknViewAppUi, public MIEEngineObserver
    {
    public: // // Constructors and destructor

        /**
        * EPOC default constructor.
        */      
        void ConstructL();

        /**
        * Destructor.
        */      
        ~CImagicAppUi();
        // PHOREPOC PATCH: open the one picture File Manager passes (a file handle or a document name)
        void OpenFileL(const TDesC& aFileName);
        TBool ProcessCommandParametersL(CApaCommandLine& aCommandLine);
        void OpenPendingFile();
        void ShowCurrentOneByOne();
        TFileName iOpenFileName;
        TFileName iReapplyName;
        
    private:
        // From MEikMenuObserver
        // With this, we can dynamically activate/deactivate menu items.
        void DynInitMenuPaneL(TInt aResourceId,CEikMenuPane* aMenuPane);
        
        CArrayFix<TCoeHelpContext>* CImagicAppUi::HelpContextL() const;
        void ScreenImmeadetaUpdate();
        
    public:
        void DestructEngine();
        void SetActiveView(TUid aViewNro);
        TUid GetActiveView();
        
        TInt GetImageIndex();
        void SetImageIndex(TInt aIndex);
#ifdef _ACCELEROMETER_SUPPORTED_
        void ImageRotated(TImagicDeviceOrientation aDeviceOrientation);
#endif
        CIEEngine* GetEngine();
        
        void GetWizardImagesL(TInt aIndex, TIEFeature aFeature);
        void SavedImageReady(CFbsBitmap* croppedBitmap);
        //TInt GetTotalImages();
        
        void SetFileNameForEditing(TFileName aFileName);
        TFileName GetFileNameForEditing();
        static TInt TimerCallBack(TAny* aPtr);
        
        static TInt ExitTimerCallBack(TAny* aPtr);
        //void SetFont();
        
        //from class MIEEngineObserver
        void WizardImagesReadyL();
        void FeatureCompleteL(TIEFeature aFeature, TInt aError);
        void FeatureErrorL(TIEFeature aFeature, TInt aError);
        void ImagesLoadedL(TInt aError);
        void EditedImageSavedL(TInt aError);
        //void SetNumberOfImages(TInt aCount);
        //void SetNumberOfFaceImages(TInt aCount);
        void TNCreationCompleteL(TThumbSize aTnRes);
        void SingleTNCreationCompletedL(TInt aIndex, TThumbSize aTnRes);
        void FaceDetectionComplete();
        void SingleFaceDetectionComplete();
        void AllFilesScanned();
        void ImageListChanged(TInt aIndex, TBool aAdded);
        
        CImagicUtils *GetImagicUtils();
        TInt GetErrorCode();
        //TRgb GetTransparentWhite();
        //TRgb GetTransparentBlack();
        //const CFont* GetFont();
        
    public:
        TInt DeleteImage(TInt aIndex);
        void SetTNGenerationFlag(TBool aValue);
        void SetUIDrawMode(TImageArrayMode aMode);
        //TImageArrayMode GetUIDrawMode();
        void CImagicAppUiReady();
        void BrowserContainerInitialized();
        TInt GetGleMaxRes();
        TBool IsAppOnTop();
        
    private:
        
        TInt iNoOfFacesDetected;
        RArray<TRect> iFaceCoOrdinates;
        
    private:
        /**
        * From CEikAppUi, takes care of command handling.
        * @param aCommand command to be handled
        */
        void HandleCommandL(TInt aCommand);
        
        
        /**
        * From CEikAppUi, handles key events.
        * @param aKeyEvent Event to handled.
        * @param aType Type of the key event. 
        * @return Response code (EKeyWasConsumed, EKeyWasNotConsumed). 
        */
        virtual TKeyResponse HandleKeyEventL(
            const TKeyEvent& aKeyEvent,TEventCode aType);

        void HandleForegroundEventL(TBool aForeground);

	    /**
	    * From CEikAppUi, handles key events.
	    * @param aType The type of resources that have changed
	    */    
	    virtual void HandleResourceChangeL( TInt aType );  
	    
	    
    private: //Data
        /** Active object that is the timing source for the clearing OpenGL memory. */
        CPeriodic*                  iPeriodic;
        CActive*                    iOpenWatcher;   // PHOREPOC PATCH: picks up a file a second launch hands over (see ImagicAppUi.cpp)
        CImagicContainerBrowser*    iBrowserContainer;
        CImagicContainerCrop*       iCropContainer;
        CImagicUtils*               iImagicUtils;
        CIEEngine*                  iIEngine;
        
        TUid						iViewIdEditorMain;
		TUid 						iViewIdBrowser;
		TUid 						iViewIdWizardEdit;
		TUid 						iViewIdCrop;
		TUid                        iViewIdRotate;
		TUid                        iViewHUIIdRotate;
		TUid                        iViewHUIIdCrop;
		TInt						iImageIndex;
	    TUid						iViewNro;
	    
	    //TInt                        iTotalNumOfImages;
	    //TInt                        iNumOfImagesLoaded;
	    //TInt                        iNumOfFacesLoaded;
	    RArray<CFbsBitmap*>         iWizardBitmapArray;
	    //TIEFeature                  iFeature;
	    TInt                        iFeatureError;
	    
	    const CFont*                iFont;
	    TRgb                        iTransparentWhite;
        TRgb                        iTransparentBlack;
        TBool                       iImagesLoaded;
        TFileName                   iFileName;
        CAknWaitDialog*             iWaitDialog;
        
        TBool                       iWzContainerSatus;
        TBool                       iAppActiveState;
        
        TBool                       iWaitingForIdleTimer;
        TBool                       iWaitingForTimer;
        
        RFs                         iFileServer;
        TBool                       iTNGenerationOnGoing;
        
        CAknWaitDialog*             iExitWaitDialog;
        
        CPeriodic*                  iExitPeriodic;
        TInt                        iNumberOfIterations;
        
        RArray<TRect>               iCoordinates;
        TImageArrayMode             iUIDrawMode;
        TBool                       iMenuOn;
        TBool                       iAppForeGround;
        
        //ROomMonitorSession*         iMemoryRequester;
    };

#endif

// End of File
