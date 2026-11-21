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
#include "ImagicContainerBrowser.h"
#include "ImagicViewBrowser.h"
#include "TextureLoader.h"

#include <e32math.h>
#include <e32debug.h>
#include <BitmapTransforms.h> 
#include <e32cmn.h>
#include <hal.h>
#include "Imagic.hrh"
#include "project.h"
#include <hal.h>
#include "DrawUtility.h"
#include "ImagicConsts.h"
#include <PhotoBrowser.rsg>


const float CImagicContainerBrowser::KMinOneByOneZoom = 1;
const float CImagicContainerBrowser::KMaxOneByOneZoom = 4.2;
const TInt  CImagicContainerBrowser::KDoubleTapZoomOneByOne1 = KMinOneByOneZoom;
const TInt  CImagicContainerBrowser::KDoubleTapZoomOneByOne2 = KMaxOneByOneZoom*0.71;
const TReal CImagicContainerBrowser::KAngle2Start128Loading = 3;
const TReal CImagicContainerBrowser::KAngle2Start128LoadingHwAcc = 6;
const TInt  CImagicContainerBrowser::KGridSizeY = 3;

/*const*/ TInt  CImagicContainerBrowser::K512TNImageBuffer = 1;//number of pictures to be loaded when using dynamic loading in grid
const TInt  CImagicContainerBrowser::K128TNImageBuffer = 8;//number of pictures to be loaded when using dynamic loading in grid
/*const*/ TInt  CImagicContainerBrowser::K32TNImageBuffer = 300;//number of pictures to be loaded when using dynamic loading in grid
/*const*/ TInt  CImagicContainerBrowser::K32TNImageUnLoadBuffer = K32TNImageBuffer*3;

const TReal KLoadingImageAspectRatio = 1.23;
const TInt  KNumOf32ThumbsLoadedBefore128Thumb = 10;

#ifdef ADAPTIVE_FRAMERATE
#ifdef __WINS__
const TInt  KDisplayDrawFreq = 20000; 
const TInt  KPowerSaveDisplayDrawFreq = 20000;
const TInt  KWaitTicksAfterDraw = 0;
#else
const TInt  KDisplayDrawFreq = 60000; 
const TInt  KPowerSaveDisplayDrawFreq = 135000;
const TInt  KWaitTicksAfterDraw = 0;
#endif
#else
const TInt  KDisplayDrawFreq = 60000;//display update freq in micro secons, 60.000us = 16.7FPS
const TInt  KPowerSaveDisplayDrawFreq = 95000;//display update freq in micro secons, 12.5FPS
#endif

const float CImagicContainerBrowser::KSpacingX = 1.2;// Picture spacing in the grid
const float CImagicContainerBrowser::KSpacingY = 1.15;// Picture spacing in the grid
// Space between pictures in one by one
const float CImagicContainerBrowser::KOneByOneSpacing=1.1;

const float KFindFaceSearchRange = 0.01;
const TInt KPowerSavePeriodicDelay = 300000;//0.3s
const TInt KTouchDelay = 1000000;//1s. longer because it scrolls even after user action
const TInt KPowerSavePeriodicInterval = 40000000;//0.4s
const TReal KInitDrawZoom = 0.1;

#define INT_MAX ((int)(((unsigned int)-1)>>1))
// macro to check if OneByOne view is zooming in. +0.01f for safety
#define IS_NOT_IN_ZOOM_ONEBYONE ((iDrawOneByOne->GetDrawOneByOneTargetZoom()) < (KDoubleTapZoomOneByOne1 + 0.01f))
#define IS_ALMOST_ZERO (0.001)

const float KUpdatesPerSecond = 1.0/15;

//App UI Feature definition flags
#define USE_LOW_DRAW_SPEED_WHILE_LOADING


// Texture coordinate data
const GLfixed CImagicContainerBrowser::iGlobalTexCoords[] =
   {
   //bitmap has to be flipped over
   0,       1<<16,
   1<<16,   1<<16,
   0,       0,
   1<<16,   0
   };
     


// ================= MEMBER FUNCTIONS =======================

// ---------------------------------------------------------
// CImagicContainerBrowser::ConstructL(const TRect& aRect)
// EPOC two phased constructor
// ---------------------------------------------------------
//
void CImagicContainerBrowser::ConstructL(CImagicAppUi* aImagicAppUi, CImagicViewBrowser* aView, const TRect& /*aRect*/)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::ConstructL++"));
    
    iDisplayDrawFreq = KDisplayDrawFreq;
    iUserInputGiven = EFalse;
    iDeleteTextures = EFalse;
    iLastEventFromKeys = EFalse;
    
    iImagicAppUi = aImagicAppUi;
    iIEngine = iImagicAppUi->GetEngine();
    iView = aView;
    
    iDrawGrid = CDrawGrid::NewL(this, iCurrentIndex);
    iDrawOneByOne = CDrawOneByOne::NewL(this, iCurrentIndex);
    iDrawFaceBrowsing = CDrawFaceBrowsing::NewL(this, iCurrentIndex);
    

#ifdef ADAPTIVE_FRAMERATE
    iWaitDrawTicks = 0;
#endif  
    
    //iPlayedWithDrag = EFalse;
#ifdef HOLD_SELECTION_ONDRAG
    iHoldSelection = EFalse;
    iOneByOneSlideByDrag = EFalse;
#endif
#ifdef MOMENTUM_MOVE
    iMomentumMove = EFalse;
#endif
#ifdef RD_FACEFRAME
    //iDrawFaceFrame = EFalse;
#endif
    
    // Create the native window
    CreateWindowL();
    // Take the whole screen into use
    SetExtentToWholeScreen();
    
    // If the device supports touch, construct long tap detector
    if ( AknLayoutUtils::PenEnabled() )
        {
        // Enable drag events listening
        EnableDragEvents();
        
#ifdef USE_AVKON_LONGTAP_DETECTOR
        // Enable long tap detection
        iLongTapDetector = CAknLongTapDetector::NewL(this);
        iLongTapDetector->SetTimeDelayBeforeAnimation(200000); // Delay before animation is set to 2 seconds in this example. Defualt is 0.15 seconds
        iLongTapDetector->SetLongTapDelay(400000); // Long tap delay is set to 5 seconds in this example. Defualt is 0.8 seconds
#endif
#ifdef USE_AVKON_TACTILE_FEEDBACK
        iTouchFeedBack = MTouchFeedback::Instance();
        iTouchFeedBack->SetFeedbackEnabledForThisApp(ETrue);
#endif
        
        iGesture = CGesture::NewL(this);
//        iGesture->SetThresholdOfTap(10);            // tap must have movement with 10 pixels
//        iGesture->SetThresholdOfMovement(50);       // experimental value on Ivalo
//        iGesture->SetThresholdOfFastMove(100);      // fast move if > 100 pixel/100ms 
//        iGesture->SetMonitoringTime(100000);        // 100ms
        }
    else
        {
#ifdef USE_AVKON_LONGTAP_DETECTOR
        iLongTapDetector = NULL;
#endif
        iGesture = NULL;
        }

    //Activate view
    ActivateL();
    
    //Create critical section
    iDrawLock.CreateLocal();
    
    //Create an active object for animating the scene, keep priority high!
    //EPriorityIdle, EPriorityLow, EPriorityStandard, EPriorityUserInput, EPriorityHigh
    iPeriodic = CPeriodic::NewL( CActive::EPriorityUserInput);
#ifdef USE_LOW_DRAW_SPEED_WHILE_LOADING
    iPowerSavePeriodic = CPeriodic::NewL( CActive::EPriorityUserInput);//keep this high, othervice can not quarantee that draw freq drop is getting run time
    iPowerSavePeriodic->Start(KPowerSavePeriodicDelay, KPowerSavePeriodicInterval, TCallBack( CImagicContainerBrowser::PowerSaveCallBack, this ) );
#endif
    
    // Initialize OpenGL
    InitAfterPowerSaveL();
    
    //Create texture loader
    iTextureLoader = new (ELeave) CTextureLoader(iImagicAppUi, this, iView, &iDrawLock);
    iTextureLoader->ConstructL();
    
    iTextureLoader->CreateIconTextures();		
    
    iDisplayRotation = 0;//Set initial display rotation to 0. This controls the whole display rotation, not single picture 
    iDisplayRotationTarget = 0;//Set initial display rotation to 0
    
    iDrawFunction = EGrid;
    iPreferHighResLoading = EFalse;
    iDrawGrid->InitDrawGrid();
    
    //iSelector = CRemConInterfaceSelector::NewL();
    //iTarget = CRemConCoreApiTarget::NewL( *iSelector, *this ); 
    //iSelector->OpenTargetL();
        
    
	//Create Draw utility class
	iDrawUtility = CDrawUtility::NewL(this);
	
    DP0_IMAGIC(_L("CImagicContainerBrowser::ConstructL--"));
    }
    
void CImagicContainerBrowser::InitAfterPowerSaveL()
    {
    OpenGLInitL();
    InitL();
    
    iDrawOnes = 0;
    // Load loading icon
    iLoadingTextureIndex = 0;
#ifdef EMPTY_IMAGE_AS_BMP    
    CFbsBitmap loadingBitmap;
    TInt error = loadingBitmap.Load(KLoadingFileName);
    if (error == KErrNone)
        {
        iLoadingTextureIndex = iTextureLoader->CreateTexture(&loadingBitmap, EFalse);
        }
#endif    
    
#if 0
    iExitTextureIndex = 0;
    CFbsBitmap exitBitmap;
    error = exitBitmap.Load(KExitFileName);
    if (error == KErrNone)
        {
        iExitTextureIndex = iTextureLoader->CreateTexture(&exitBitmap, EFalse);
        }
    
    iMenuTextureIndex = 0;
    CFbsBitmap menuBitmap;
    error = menuBitmap.Load(KMenuFileName);
    if (error == KErrNone)
        {
        iMenuTextureIndex = iTextureLoader->CreateTexture(&menuBitmap, EFalse);
        }
#endif
    
#ifdef SHADOW_PHOTOS    
    iShadowTextureIndex = 0;
    CFbsBitmap shadowBitmap;
    error = shadowBitmap.Load(KShadowFileName);
    if (error == KErrNone)
        {
        iShadowTextureIndex = iTextureLoader->CreateTexture(&shadowBitmap, EFalse);        
        }
#endif    
    
    }

void CImagicContainerBrowser::InitL()
   {
   DP0_IMAGIC(_L("CImagicContainerBrowser::GridDataInit++"));
       
#ifdef _ACCELEROMETER_SUPPORTED_
    //iDeviceOrientation = TSensrvOrientationData::EOrientationDisplayRightUp;//Landscape
    //iDeviceOrientation = TImagicDeviceOrientation::EOrientationDisplayRightUp;//Portrait
    //iDeviceOrientation = 3;//EOrientationDisplayRightUp = Landscape
    iDeviceOrientation = iIEngine->GetDeviceOrientation();
#endif
    
    DP1_IMAGIC(_L("CImagicContainerBrowser::GridDataInit - Device orientation: %d"),iDeviceOrientation);
    
       iScreenImmeadetaUpdate = EFalse;
    
       iDrawNow = EFalse;
       iDynamicLoadingOn = ETrue;
       
       iNewImageAdded = EFalse;
       iPreferHighResLoading = EFalse;
       
       if(iImagicAppUi->GetImageIndex())
           iCurrentIndex = iImagicAppUi->GetImageIndex();
       else
           iCurrentIndex = 0;
       
       //Read user settings from database <--------------------
       
       //Set default draw function
       iDrawFunction = EGrid;
              
       //Init key data here
       ResetKeyData();
           
       DP0_IMAGIC(_L("CImagicContainerBrowser::GridDataInit--"));
   }



void CImagicContainerBrowser::OpenGLInitL()
   {
   DP0_IMAGIC(_L("CImagicContainerBrowser::OpenGLInit++"));
   
       // Open GL hasn't been initialized
       iOpenGlInitialized = EFalse;
       
       // Describes the format, type and size of the color buffers and
       // ancillary buffers for EGLSurface
       EGLConfig config;
    
       // Get the display for drawing graphics
       iEglDisplay = eglGetDisplay( EGL_DEFAULT_DISPLAY );
       if ( iEglDisplay == NULL )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglGetDisplay failed - GL Error: %d"),glGetError());
           _LIT(KGetDisplayFailed, "eglGetDisplay failed");
           User::Panic( KGetDisplayFailed, 0 );
           }
       
       // Initialize display
       if ( eglInitialize( iEglDisplay, NULL, NULL ) == EGL_FALSE )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglInitialize failed - GL Error: %d"),glGetError());
           _LIT(KInitializeFailed, "eglInitialize failed");
           User::Panic( KInitializeFailed, 0 );
           }
    
       // Pointer for EGLConfigs
       EGLConfig *configList = NULL;
       EGLint numOfConfigs   = 0;
       EGLint configSize     = 0;
    
       // Get the number of possible EGLConfigs
       if ( eglGetConfigs( iEglDisplay, configList, configSize, &numOfConfigs ) == EGL_FALSE )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglGetConfigs failed - GL Error: %d"),glGetError());
           _LIT(KGetConfigsFailed, "eglGetConfigs failed");
           User::Panic( KGetConfigsFailed, 0 );
           }
    
       configSize = numOfConfigs;
    
       // Allocate memory for the configList
       configList = (EGLConfig*) User::Alloc( sizeof(EGLConfig)*configSize );
       if ( configList == NULL )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - config alloc failed - GL Error: %d"),glGetError());
           _LIT(KConfigAllocFailed, "config alloc failed");
           User::Panic( KConfigAllocFailed, 0 );
           }
    
       // Define properties for the wanted EGLSurface. To get the best possible
       // performance, choose an EGLConfig with a buffersize matching the current
       // window's display mode
       TDisplayMode DMode = Window().DisplayMode();
       TInt BufferSize = 0;
    
       switch(DMode)
       {
       case(EColor4K):
           BufferSize = 12;
           break;
       case(EColor64K):
           BufferSize = 16;
           break;
       case(EColor16M):
           BufferSize = 24;
           break;
       case(EColor16MU):
       case(EColor16MA):
           BufferSize = 32;
           break;
       default:
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - unsupported displaymode - GL Error: %d"),glGetError());
           _LIT(KDModeError, "unsupported displaymode");
           User::Panic( KDModeError, 0 );
           break;
       }
       
       // Define properties for the wanted EGLSurface
       const EGLint attrib_list[] =
           {
           EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
           EGL_BUFFER_SIZE,  BufferSize,
           EGL_DEPTH_SIZE,   16,
           EGL_NONE
           };


       // No configs with antialising were found. Try to get the non-antialiased config
       if ( eglChooseConfig( iEglDisplay, attrib_list, configList, configSize, &numOfConfigs ) == EGL_FALSE )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglChooseConfig failed - GL Error: %d"),glGetError());
           _LIT( KChooseConfigFailed, "eglChooseConfig failed" );
           User::Panic( KChooseConfigFailed, 0 );
           }

       if ( numOfConfigs == 0 )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - Can't find the requested config. - GL Error: %d"),glGetError());
           // No configs found without antialiasing
           _LIT( KNoConfig, "Can't find the requested config." );
           User::Panic( KNoConfig, 0 );
           }
           
       // Choose the best EGLConfig. EGLConfigs returned by eglChooseConfig are
       // sorted so that the best matching EGLConfig is first in the list.
       config = configList[0];
    
       // Free configList, as it's not used anymore.
       User::Free( configList );
    
       // Create a window where the graphics are blitted
       iEglSurface = eglCreateWindowSurface( iEglDisplay, config, &Window(), NULL );
       if ( iEglSurface == NULL )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglCreateWindowSurface failed - GL Error: %d"),glGetError());
           _LIT(KCreateWindowSurfaceFailed, "eglCreateWindowSurface failed");
           User::Panic( KCreateWindowSurfaceFailed, 0 );
           }
    
       // Create a rendering context
       iEglContext = eglCreateContext( iEglDisplay, config, EGL_NO_CONTEXT, NULL );
       if ( iEglContext == NULL )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglCreateContext failed - GL Error: %d"),glGetError());
           _LIT(KCreateContextFailed, "eglCreateContext failed");
           User::Panic( KCreateContextFailed, 0 );
           }
    
       // Make the context current. Binds context to the current rendering thread
       // and surface.
       if ( eglMakeCurrent( iEglDisplay, iEglSurface, iEglSurface, iEglContext ) == EGL_FALSE )
           {
           DP1_IMAGIC(_L("CImagicContainerBrowser::ConstructL - eglMakeCurrent failed - GL Error: %d"),glGetError());
           _LIT(KMakeCurrentFailed, "eglMakeCurrent failed");
           User::Panic( KMakeCurrentFailed, 0 );
           }
       
#ifdef SHADOW_PHOTOS        
       glClearColor(1,1,1, 0);
#else       
       glClearColor(0,0,0, 0);
#endif       
       //glClearDepth(1.0f);                                 // Depth Buffer Setup
       glEnable(GL_DEPTH_TEST);                            // Enables Depth Testing
       //glDisable(GL_DEPTH_TEST);                            // Enables Depth Testing
       //glDepthMask(GL_FALSE);
       glDepthFunc(GL_LEQUAL);                             // The Type Of Depth Testing To Do
       // TODO,  check the perf gain
       //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // Really Nice Perspective Calculations
       glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);  // Fast Perspective Calculations
       glShadeModel(GL_FLAT);//GL_FLAT ,GL_SMOOTH TODO, check perf if smooth is on
       //glShadeModel(GL_SMOOTH);//GL_FLAT ,GL_SMOOTH TODO, check perf if smooth is on
       //glDisable( GL_LINE_SMOOTH  );
       //glEnable with the arguments GL_LINE_SMOOTH or GL_POINT_SMOOTH.
       //glEnable(GL_POLYGON_SMOOTH);
       //glEnable(GL_POINT_SMOOTH);
       //glEnable( GL_LINE_SMOOTH  );
       //glEnable(GL_MULTISAMPLE_ARB);
       glEnable( GL_TEXTURE_2D  );
       glDisable( GL_LIGHTING  );//disable for performance reasons
       //Disable alpha blending
       glDisable(GL_BLEND);
       glDisable(GL_ALPHA_TEST);
       
       
       //GLint params;
       glGetIntegerv( GL_MAX_TEXTURE_SIZE, &iGLMaxRes );
       DP1_IMAGIC(_L("CImagicContainerBrowser::OpenGLInit - OpenGL max image Res size: %d"), iGLMaxRes);
       
       glMatrixMode( GL_MODELVIEW );
       
       
       iOpenGlInitialized = ETrue;
       
       //Start draw timer
       DP0_IMAGIC(_L("CImagicContainerBrowser::OpenGLInit - Start Draw timer"));
       iPeriodic->Start( 100, KDisplayDrawFreq, TCallBack( CImagicContainerBrowser::DrawCallBackL, this ) );
       
       DP0_IMAGIC(_L("CImagicContainerBrowser::OpenGLInit--"));
   }

//This is called when we want to reduce screen drawing
TInt CImagicContainerBrowser::PowerSaveCallBack(TAny *aInstance)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::PowerSaveCallBack++"));
    
    ((CImagicContainerBrowser*) aInstance)->PowerSave();
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::PowerSaveCallBack--"));
    return 0;
    }

    
void CImagicContainerBrowser::PowerSave()
    {
    iPowerSavePeriodic->Cancel();
    
    if(iDisplayDrawFreq == KDisplayDrawFreq)
        {
#ifndef ADAPTIVE_FRAMERATE		
        iDisplayDrawFreq = KPowerSaveDisplayDrawFreq;
        DisableDisplayDraw();
        if(iImagicAppUi->IsAppOnTop())
            EnableDisplayDraw();
#endif		
        }
    }

TInt CImagicContainerBrowser::DisableDrawTimer( TAny* aInstance )
   {
   DP0_IMAGIC(_L("CImagicContainerBrowser::DisableDrawTimer++"));
   
   // Get pointer to instance
   CImagicContainerBrowser* instance = (CImagicContainerBrowser*) aInstance;
   //instance->iPeriodicTimerActive = EFalse;
   instance->DisableDisplayDraw();
   
   DP0_IMAGIC(_L("CImagicContainerBrowser::DisableDrawTimer--"));
   return 0;
   }

/*----------------------------------------------------------------------*/
// Destructor
CImagicContainerBrowser::~CImagicContainerBrowser()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::~CImagicContainerBrowser++"));
    
    //delete iPeriodic;
    if(iPeriodic)
        {
        if(iPeriodic->IsActive())
            iPeriodic->Cancel();
            
        delete iPeriodic;   
        iPeriodic = NULL;
        }
        
    delete iDrawUtility;
    
    //Cancelling CTimer...
    if(iPowerSavePeriodic)
        {
        if(iPowerSavePeriodic->IsActive())
            iPowerSavePeriodic->Cancel();
            
        delete iPowerSavePeriodic;   
        iPowerSavePeriodic = NULL;
        }
        
    DeleteTextures();
    iDrawLock.Close();
    
    if(iTextureLoader)
        {
        delete iTextureLoader;
        iTextureLoader = NULL;
        }
    
    //iFloatCoordinates.Close();
    
#ifdef USE_AVKON_LONGTAP_DETECTOR
    if(iLongTapDetector)
        {
        delete iLongTapDetector;
        iLongTapDetector = NULL;
        }
#endif

    if(iGesture)
        {
        delete iGesture;
        iGesture = NULL;
        }

    //Destruct all engine components
    iImagicAppUi->DestructEngine();
    
    delete iDrawGrid;
    delete iDrawOneByOne;
    delete iDrawFaceBrowsing;
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::~CImagicContainerBrowser--"));
    }

/*----------------------------------------------------------------------*/
// CTextureManager::DeleteTextures
// Deletes named textures by calling glDeleteTextures.
//
void CImagicContainerBrowser::DeleteTextures()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DeleteTextures++"));

    iIEngine->CancelFullSizeLoading();
    
    if(iPeriodic && iPeriodic->IsActive())
        iPeriodic->Cancel();
    
    eglMakeCurrent( iEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( iEglDisplay, iEglSurface );
    eglDestroyContext( iEglDisplay, iEglContext );
    eglTerminate( iEglDisplay );
    
    //Delete OpenGL memory allocations
    TInt num = iIEngine->GetTotalNumOfImages();
    for(TInt i=0; i < num; i++ )
        {
        CImageData* data = iIEngine->GetImageData(i);
        
        if(data->iGridData.iGlLQ128TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ128TextIndex );
        if(data->iGridData.iGlLQ32TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ32TextIndex );
        if(data->iGridData.iGlHQ512TextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlHQ512TextIndex);
        if(data->iGridData.iGlSuperHQTextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlSuperHQTextIndex);
        
        data->iGridData.iGlLQ32TextIndex = 0;
        data->iGridData.iGlLQ128TextIndex = 0;
        data->iGridData.iGlHQ512TextIndex = 0;
        data->iGridData.iGlSuperHQTextIndex = 0;
        }
       
    iOpenGlInitialized = EFalse;
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DeleteTextures--"));
    }

/*
//Prepares container to filename array swap
void CImagicContainerBrowser::SwapArrays()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SwapArrays++"));
    
    //Delete OpenGL memory allocations
    TInt num = iIEngine->GetTotalNumOfImages();
    for(TInt i=0; i < num; i++ )
        {
        CImageData* data = iIEngine->GetImageData(i);
        
        if(data->iGridData.iGlLQ128TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ128TextIndex );
        if(data->iGridData.iGlLQ32TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ32TextIndex );
        if(data->iGridData.iGlHQ512TextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlHQ512TextIndex);
        if(data->iGridData.iGlSuperHQTextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlSuperHQTextIndex);
        
        data->iGridData.iGlLQ32TextIndex = 0;
        data->iGridData.iGlLQ128TextIndex = 0;
        data->iGridData.iGlHQ512TextIndex = 0;
        data->iGridData.iGlSuperHQTextIndex = 0;
        }
    
    iOpenGlInitialized = EFalse;
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SwapArrays--"));
    }
*/

void CImagicContainerBrowser::InitFaceBrowsing()
    {
    iDrawFaceBrowsing->InitFaceBrowsing();    
    }


/*----------------------------------------------------------------------*/
// Interpolates given value into target value with step
//
void CImagicContainerBrowser::Interpolate(float &aValue, const float aTarget, const float aStep)
	{
	//DP0_IMAGIC(_L("CImagicContainerBrowser::Interpolate"));
	// Calculate new value
	float diff = aTarget-aValue;
//    aValue += diff * aStep * iTimeDiff * 30; 
	float timediff = Min(0.1f, iTimeDiff); // so max value of timediff is 100tick (100ms)
	aValue += diff * aStep * timediff * 30; 
	
	// Check that value is in range
    if (aValue > aTarget && diff > 0)
        aValue = aTarget;
    if (aValue < aTarget && diff < 0)
        aValue = aTarget;
	}

/*----------------------------------------------------------------------*/
// Makes sure that given value is within limits
//
void CImagicContainerBrowser::CheckLimits(float &aValue, const float aMin, const float aMax)
	{
	DP0_IMAGIC(_L("CImagicContainerBrowser::CheckLimits"));
	if (aValue < aMin)
		aValue = aMin;
	if (aValue > aMax)
		aValue = aMax;
	}

/*----------------------------------------------------------------------*/
// Handle rotation keys
//
void CImagicContainerBrowser::HandleRotationKeys(void)
	{
	//DP0_IMAGIC(_L("CImagicContainerBrowser::HandleRotationKeys"));
	// Handle rotation
	CImageData* imageData = iIEngine->GetImageData(iCurrentIndex);
	if(imageData)
	    imageData->iGridData.iTargetRotationAngle += iKeyData.iRotate*90;
	iKeyData.iRotate = 0;
	}


TInt CImagicContainerBrowser::GetFreeRam()
    {
    TInt mem = 0;
    TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CImagicContainerBrowser::CheckFreeRam - mem: %d"),mem);
    return mem;
    }

            

/*----------------------------------------------------------------------*/
// Checks limits for iFileIndex and starts loading image when 
// new image was selected
//
void CImagicContainerBrowser::DynamicLoadingL()
    {
    if(!iDynamicLoadingOn && !iNewImageAdded)
        {
        DP2_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - return #Loading is off, iDynamicLoadingOn:%d, iNewImageAdded:%d"),iDynamicLoadingOn,iNewImageAdded);
        return;
        }

    // Check that loader is not running
    if(iTextureLoader->IsRunning())
        {
        DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - return #Loader is running"));
        
/*        if(iIsLoaderRunning > 30)
            {
            DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - return #Loader is running >30 times in loop"));
            iIsLoaderRunning = 0;
            iDynamicLoadingOn = EFalse;
            iNewImageAdded = EFalse;
            }
        
        iIsLoaderRunning++;*/
        
        return;
        }
    
    if(iPreferHighResLoading)
        {
        DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - return #Prefer High res loading"));
        return;
        }

    DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL++"));
    
    /*TInt imageBuffer = iIEngine->IsScanningFiles() ? 
                        K128TNImageBuffer + KNumOf32ThumbsLoadedBefore128Thumb : 
                        K32TNImageBuffer;*/
    
    TInt imageBuffer = K32TNImageBuffer; 
    
    //DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL load closest unloaded image"));
   
    for (TInt i = 0;i < imageBuffer; i++)
        {
        // Check to positive and negative direction from current picture
        for (TInt j = 0; j < 2; j++)
            {
            // Calculate image index
            TInt index = iCurrentIndex + (j ?  i : -i);
            
            // Check that index is valid
            if (index >= 0 && index < iIEngine->GetTotalNumOfImages())
                {
                CImageData* imageData = iIEngine->GetImageData(index);
                // Load tiny thumbnail
                if( !imageData->iGridData.iCorrupted &&
                    imageData->iGridData.iGlLQ32TextIndex == 0 &&
                    (imageData->IsImageReady(ESize32x32) || 
                     imageData->IsImageReady(EExifThumb)) //&& 
                    //Abs(iCurrentIndex-index) <= K32TNImageBuffer
                    )
                    {
                    TRAPD(err, iTextureLoader->LoadL(imageData, ESize32x32));
                    if(err == KErrNone)
                        {
                        DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - start loading 32x32"));
                        i = imageBuffer + 1;
                        iNewImageAdded = EFalse;//should be set off only when we tried to load new image
                        break; //Image loading did start, break out
                        }
                    }
                }
    
            // Load higher resolution thumb only after couple of tiny thumbs 
            TInt i2 = i - KNumOf32ThumbsLoadedBefore128Thumb;
            if (i2 < 0)
                continue;
                
            // Calculate image index
            index = iCurrentIndex + (j ? i2 : -i2);

            // Check that index is valid
            if (index < 0 || index >= iIEngine->GetTotalNumOfImages())
                continue;

            CImageData* imageData = iIEngine->GetImageData(index);
            // Higher resolution thumbnail exist
            if(!imageData->iGridData.iCorrupted &&
               imageData->iGridData.iGlLQ128TextIndex == 0 &&
                (imageData->IsImageReady(ESize128x128) || 
                 imageData->IsImageReady(EExifThumb)) && 
                Abs(iCurrentIndex-index) <= K128TNImageBuffer)
                {
                //Do not load 128 TN before tilted to flat
                if(IsHwAcceleration() && Abs(iDrawGrid->GetCurrentGridTilt()) < KAngle2Start128LoadingHwAcc)
                    {
                    TRAPD(err, iTextureLoader->LoadL(imageData, ESize128x128));
                    if(err == KErrNone)
                        {
                        DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - start loading 128x128"));
                        i = imageBuffer + 1;                        
                        iNewImageAdded = EFalse;//should be set off only when we tried to load new image
                        break; //Image loading did start, break out
                        }
                    }   
                if(!IsHwAcceleration() && Abs(iDrawGrid->GetCurrentGridTilt()) < KAngle2Start128Loading)
                    {
                    TRAPD(err, iTextureLoader->LoadL(imageData, ESize128x128));
                    if(err == KErrNone)
                        {
                        DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL - start loading 128x128"));
                        i = imageBuffer + 1;                        
                        iNewImageAdded = EFalse;//should be set off only when we tried to load new image
                        break; //Image loading did start, break out
                        }
                    }
                }
            }
        }
    
    iNewImageAdded = EFalse;
    
    // Wait until something new happen before continue loading
    iDynamicLoadingOn = iIEngine->IsScanningFiles();
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicLoadingL--"));
    }

void CImagicContainerBrowser::NewImageAdded()
    {
    iNewImageAdded = ETrue;    
    }

void CImagicContainerBrowser::ImageListChanged(TInt aIndex, TBool aAdded)
    {
    if (iDrawGrid)
        iDrawGrid->UpdateImageCoordinates(aIndex);
    
    if (IsUserInputGiven())
        {
        /* TODO if (iCurrentIndex >= aIndex) 
            {
            if (aAdded)
                iCurrentIndex++;
            else 
                iCurrentIndex--;
            CheckIndexLimits(iCurrentIndex);
            }*/
        }
    iDynamicLoadingOn = ETrue;
    }

/*----------------------------------------------------------------------*/
// Unloads images from Grid
//
void CImagicContainerBrowser::DynamicUnLoading()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading++"));
    
    CImageData* imageData = NULL;
    
    // Loop through all pictures
    for (TInt i=0; i<iIEngine->GetTotalNumOfImages(); i++)
        {
        // Check if picture is loaded
        imageData = iIEngine->GetImageData(i);

        if(imageData->iGridData.iGlLQ32TextIndex!=0)
            if(i<(iCurrentIndex-(K32TNImageUnLoadBuffer+1)) || i>(iCurrentIndex+(K32TNImageUnLoadBuffer+1)))
                {
                // Unload picture
                iTextureLoader->UnloadLQ32Tex( imageData );
                DP1_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading - Unload 32x32 [%d] texture!!!!!"),i);
                }
        if(imageData->iGridData.iGlLQ128TextIndex != 0)
            if(i<(iCurrentIndex-(K128TNImageBuffer+1)) || i>(iCurrentIndex+(K128TNImageBuffer+1)))
                {
                // Unload picture
                iTextureLoader->UnloadLQ128Tex( imageData );
                DP1_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading - Unload 128x128 [%d] texture!!!!!"),i);
                }
        if(imageData->iGridData.iGlHQ512TextIndex != 0 )
            if((i < (iCurrentIndex-(K512TNImageBuffer+1))) || (i > (iCurrentIndex+(K512TNImageBuffer+1))))
                {
                //Unload picture
                iTextureLoader->UnloadLQ512Tex( imageData );
                DP1_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading - Unload 512x512 [%d] texture!!!!!"),i);
                }
                
        if(imageData->iGridData.iGlSuperHQTextIndex != 0)
            if(i != iCurrentIndex)
                {
                //Unload picture
                iTextureLoader->ReleaseSuperHResTexture( imageData );
                DP1_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading - Unload Superresolution [%d] texture!!!!!"),i);
                }
        }
        
    DP0_IMAGIC(_L("CImagicContainerBrowser::DynamicUnLoading--"));
    }

            
/*----------------------------------------------------------------------*/
// Calculates widht and height with aspect ratio
//
#if 0
void CImagicContainerBrowser::BubbleEffect(TInt& x, TInt& y, float& z)
    {
    if(iBubbleEffect)
        {
        // Selected image in coordinates x,y
        iSelectedX = iCurrentIndex/iGridSizeY;
        iSelectedY =- (iCurrentIndex%iGridSizeY);
                    
        // Distance to selected
        iDistanceX = iSelectedX-x;
        iDistanceY = iSelectedY-y;
        // Squared
        if(iDistanceX<0) iDistanceX*= -1;
        if(iDistanceY<0) iDistanceY*= -1;
        
        // Distance
        iDiff=iDistanceX+iDistanceY;
        // Convert distance to depth
        // http://en.wikipedia.org/wiki/Gaussian_function
        /*
        if (iDiff==0) z=4.2;
        if (iDiff==1) z=3.973029769;
        if (iDiff==2) z=3.363097092;
        if (iDiff==3) z=2.547428771;
        if (iDiff==4) z=1.72667162;
        if (iDiff==5) z=1.047279277;
        if (iDiff==6) z=0.56840819;
        if (iDiff==7) z=0.27605982;
        if (iDiff==8) z=0.119975103;
        if (iDiff==9) z=0.046657785;
        if (iDiff==10) z=0.016236865;
        if (iDiff>10) z=0;
        */
        if (iDiff==0) z=5.0;
        if (iDiff==1) z=4.2;
        if (iDiff==2) z=3.4;
        if (iDiff==3) z=2.1;
        if (iDiff==4) z=1.3;
        if (iDiff==5) z=0.8;
        if (iDiff==6) z=0.4;
        if (iDiff==7) z=0.3;
        if (iDiff>7) z=0.3;
        //if (iDiff==8) z=0.1;
        //if (iDiff==9) z=0.05;
        //if (iDiff==10) z=0.01;
        //if (iDiff>10) z=0;
        }
    }
#endif

/*----------------------------------------------------------------------*/
// Calculates widht and height with aspect ratio
//
void CImagicContainerBrowser::CalculateImageSize(float& aWidth, float& aHeight, const float aAspectRatio/*display aspectratio*/)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::CalculateImageSize"));
	// Check picture orientation
    TBool landscape = EFalse;
    TBool tmp = EFalse;
    CImageData* imageData = iIEngine->GetImageData(iCurrentIndex);
    
    if(imageData->GetAspectRatio() > 1)
        landscape = ETrue;
    
	//Change landscape<->portrait if pic is rotated 90 or 270
    //if(imageData->iGridData.iTargetRotationAngle%90==0 && imageData->iGridData.iTargetRotationAngle%180!=0)
    //if(Abs(imageData->iGridData.iTargetRotationAngle-90) <= 0.1 && Abs(imageData->iGridData.iTargetRotationAngle-270) <= 0.1)
    TInt angle = imageData->iGridData.iTargetRotationAngle;
    if(angle % 90 == 0 && angle % 180 != 0)
        {
        tmp=ETrue;
        landscape = !landscape;
        }
    
	//Calculate new width and height
    aWidth=0.5;
    aHeight=0.5;

    if(landscape)
        {
        //Fix aspect ratio
        aHeight/=aAspectRatio;
        if(imageData->GetAspectRatio() < aAspectRatio && imageData->GetAspectRatio() >= 1)
            {
            aHeight = aHeight * (aAspectRatio/imageData->GetAspectRatio());
            aWidth = aWidth * (aAspectRatio/imageData->GetAspectRatio());
            }
        //If portrait picture aspect ratio is between 0.75 - 1 
        if(imageData->GetAspectRatio() > (1.0 / aAspectRatio) && imageData->GetAspectRatio() < 1)
            {
            aHeight = aHeight * (aAspectRatio*imageData->GetAspectRatio());
            aWidth = aWidth * (aAspectRatio*imageData->GetAspectRatio());
            }
        }
    else
        {
        //Fix aspect ratio
        aWidth *= aAspectRatio;
        }
}



/*----------------------------------------------------------------------*/
// Draws one by one view
//
void CImagicContainerBrowser::GetCurrentFilenameL(TFileName& aFilename, TThumbSize aRes)
    {
    iIEngine->GetFileNameL(iCurrentIndex, aRes, aFilename);
    }

void CImagicContainerBrowser::SetCurrentFaceNro(TInt aNro)
    {
    iDrawFaceBrowsing->SetCurrentFaceNro(aNro);
    }

#if 0
/*----------------------------------------------------------------------*/
// Gets current screen coords
//
void CImagicContainerBrowser::ConvertScreenCoords2QvgaCoords(TPoint& aPoint, TRect& aRect)
    {
    CImageData* imageData = iIEngine->GetImageData(iCurrentIndex);
    
    TInt pictureWidth, pictureHeigth;
    pictureWidth=320;
    pictureHeigth=320;
    
    //Calculate coords from alogorith to OpenGL -0.5 - +0.5 coords and fill the array
    float tmpX;
    float tmpY;
    if(imageData->GetAspectRatio() > 1)
        {
        //Convert from center of screen to corner coords
        tmpX = iDrawOneByOne->GetDrawOneByOneXY().iX + 0.5;
        tmpY = iDrawOneByOne->GetDrawOneByOneXY().iY + (0.5/imageData->GetAspectRatio());
        }
    else//portrait
        {
        //Convert from center of screen to corner coords
        tmpX = iDrawOneByOne->GetDrawOneByOneXY().iX + (0.5*imageData->GetAspectRatio());
        tmpY = iDrawOneByOne->GetDrawOneByOneXY().iY + 0.5;
        }
    
    aPoint.iX = tmpX * pictureWidth;
    aPoint.iY = tmpY * pictureHeigth;
            
    TInt rectWidth = pictureWidth;
    TInt rectHeigth = pictureHeigth;
    
    rectWidth = rectWidth/(iDrawOneByOne->GetDrawOneByOneZoom()*2);
    rectHeigth = rectHeigth/(iDrawOneByOne->GetDrawOneByOneZoom()*2);
    
    aRect.iTl.iX = aPoint.iX-(rectWidth/2); 
    aRect.iTl.iY = aPoint.iY-(rectHeigth/2);
    aRect.iBr.iX = aPoint.iX+(rectWidth/2); 
    aRect.iBr.iY = aPoint.iY+(rectHeigth/2);
    
    }
#endif

void CImagicContainerBrowser::LoadHighResImage(CImageData* imageData, TInt aIndex)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHighResImage++"));
    
    //Make sure that 512 for current image is loaded
    if(imageData->iGridData.iGlHQ512TextIndex == 0)
        {
        LoadHQ512Image(imageData, aIndex);
        return;
        }
        
#ifdef SUPERZOOM
    //Load "super zoom" image if in zoom or face browser
    if(imageData->iGridData.iGlSuperHQTextIndex == 0 && 
       (!IS_NOT_IN_ZOOM_ONEBYONE || iDrawOneByOne->IsMagGlassOn() ))
        {
        //iPreferHighResLoading = ETrue;
        TRAPD(err, iTextureLoader->LoadL(imageData, EFullSize));
        if( err == KErrNone || err == KErrInUse )
            iPreferHighResLoading = ETrue;
        
        /*if(err != KErrNone)
            if(err == KErrAlreadyExists || err == KErrInUse)
                iPreferHighResLoading = ETrue;
            else
                iPreferHighResLoading = EFalse;*/
        }
#endif

    CImageData* imageDataLoad = NULL;
    //Here we load 512x512 resolution images, one to both sides of current up to limit
    for (TInt i = 1; i <= K512TNImageBuffer; i++)
        {
        // Check to positive and negative direction from current picture
        for (TInt j = 0; j < 2; j++)
            {
            // Calculate image index
            TInt index = iCurrentIndex + (j ?  i : -i);
            
            if(index >= 0 && index < iIEngine->GetTotalNumOfImages())
                {
                imageDataLoad = iIEngine->GetImageData(index);
                if(imageDataLoad == NULL)
                    break;
                
                if(imageDataLoad->iGridData.iGlHQ512TextIndex == 0)
                    {
                    DP1_IMAGIC(_L("CImagicContainerBrowser::LoadHighResImage -------------- Start High res image loading, index: %d"), index);
                    LoadHQ512Image(imageDataLoad, index);
                    return;
                    }
                    
                }
            }
        }
            
        
    DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHighResImage--"));
    }


void CImagicContainerBrowser::LoadHQ512Image(CImageData* imageData, TInt aIndex)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHQ512Image++"));
    
    //iPreferHighResLoading = ETrue;
    
    if(/*iDrawOneByOne->GetDrawOneByOneZoom() > 0.99 &&*/ imageData)
        {
        TInt error;
        if((imageData->iGridData.iGlLQ128TextIndex == 0) && (iCurrentIndex == aIndex))
            TRAPD(error, iTextureLoader->LoadL(imageData, ESize128x128));
        
        if(imageData->iGridData.iGlHQ512TextIndex == 0)
            {
            //iPreferHighResLoading = ETrue;
            TRAPD(err, iTextureLoader->LoadL(imageData, ESize512x512));
/*            if(err != KErrNone)
                if(err == KErrAlreadyExists || err == KErrInUse)
                    iPreferHighResLoading = ETrue;
                else
                    iPreferHighResLoading = EFalse;*/
            
            if( err == KErrNone || err == KErrInUse )
                {
                DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHQ512Image - set dyn loading off"));
                iPreferHighResLoading = ETrue;
                iDynamicLoadingOn = EFalse; //set to false to be able to get run time for high res loading
                }
            else if(err == KErrAlreadyExists)
                {
                DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHQ512Image - set dyn loading on"));
                iDynamicLoadingOn = ETrue;
                }
            }

        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::LoadHQ512Image--"));
    }
    
	
void CImagicContainerBrowser::ShowMagGlass(const TBool aState)
    {
    // do nothing if it's already in the state
    if (/*iMagGlassOn*/ iDrawOneByOne->IsMagGlassOn() == aState || !IS_NOT_IN_ZOOM_ONEBYONE)
        return;

    iDrawOneByOne->SetMagGlassPrevStatus(iDrawOneByOne->IsMagGlassOn());
    iDrawOneByOne->SetMagGlassStatus(aState);
    }



void CImagicContainerBrowser::SetMinMagFilterLinear(TBool aValue)
    {
    if(iDrawFunction == EOneByOne || iDrawFunction == EFaceBrowsing)
        if(IsHwAcceleration())
            iMinMagFilterSetting=ETrue;
        else
            iMinMagFilterSetting=aValue;
    else
        iMinMagFilterSetting=aValue;
    }

TBool CImagicContainerBrowser::IsHwAcceleration()
    {
    return (iGLMaxRes == 2048)? ETrue: EFalse;
    }

void CImagicContainerBrowser::SetMinMagFilterLinearDo(TBool aValue)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetMinMagFilterLinear++"));
    
    CImageData* imageData = iIEngine->GetImageData(iCurrentIndex);
    
    if(iCurrentBindedIndex == imageData->iGridData.iGlLQ128TextIndex || 
       iCurrentBindedIndex == imageData->iGridData.iGlLQ32TextIndex)
        {
        DP0_IMAGIC(_L("CImagicContainerBrowser::SetMagFilterLinear - Linear"));
        iMagFilterLinear = ETrue;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    else if(aValue)
        {
        DP0_IMAGIC(_L("CImagicContainerBrowser::SetMagFilterLinear - Linear"));
        iMagFilterLinear = ETrue;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    else
        {
        DP0_IMAGIC(_L("CImagicContainerBrowser::SetMagFilterLinear - Nearest"));
        iMagFilterLinear = EFalse;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    
    
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetMinMagFilterLinear--"));
    }

void CImagicContainerBrowser::DrawFaceBrowsingIcon()
    {
#ifdef ICONS_ENABLAD
    if((!iMagGlassOn && iDrawFunction==EFaceBrowser) || 
       (iDrawFunction==EFaceBrowser) && (iIconTextureIndexes.Count() >= 1))
        {
        iDrawUtility->DrawIcon(Size(), iIconTextureIndexes[0]);
        SetMinMagFilterLinearDo(ETrue);
        }
#endif
    }

void CImagicContainerBrowser::DrawZoomIcon()
    {
#ifdef ICONS_ENABLAD
    if((!iMagGlassOn && iDrawFunction==EOneByOne && (iDrawOneByOneZoom-1.0) > 0.01) && 
       (iIconTextureIndexes.Count()>=2))
        {
        iDrawUtility->DrawIcon(Size(), iIconTextureIndexes[1]);
        SetMinMagFilterLinearDo(ETrue);
        }
#endif
    }

void CImagicContainerBrowser::SetDeleteTextures(TBool aValue)
    {
    DeleteTextures();
    iDeleteTextures = aValue;    
    }



//---------------------------------------------------------------------------------	
//DrawCallBack for OpenGL
TInt CImagicContainerBrowser::DrawCallBackL( TAny* aInstance )
    {
	// Get pointer to instance
    ((CImagicContainerBrowser*) aInstance)->DrawL();
    return 0;
    }
    
void CImagicContainerBrowser::DrawL()
    {
#ifdef ADAPTIVE_FRAMERATE
    // No drawing if timer is not zero
    if (iWaitDrawTicks != 0)
        {
        if (iWaitDrawTicks > 0)
            iWaitDrawTicks--;    
        return;
        }
    
    iWaitDrawTicks = -1;
    
    //TInt startTick =
#endif        

    
    
    if(iDeleteTextures)
        {
        if(iPeriodic->IsActive())
            iPeriodic->Cancel();
        DeleteTextures();
        
        return;
        }
    
    
    TSize size = Size();    
    BeginDrawing();

    
    
    if(!iScreenRotateOngoing)
        {
        
        // Call proper draw function
        switch (iDrawFunction)
            {
            case EGrid:
                
                if(iIEngine->IsScanningFiles())
                    {
                    if(iDrawOnes == 0)
                        iDrawOnes = 1;//make sure we draw screen ones when start application
                    }
#if 0
                if(iDynamicLoadingOn)
                    {
                    DP0_IMAGIC(_L("CImagicContainerBrowser::DrawCallBack - Continue Image Loading"));
                    DynamicLoadingL();
                    }
                
#endif           
                                
                if(iDrawGrid->IsDrawingNeededGrid() || iDrawOnes == 1)
                    {
                    drawZoom = 1; inPictureX = 0; inPictureY = 0;
                    iDrawGrid->DrawGridL(size);
                    EndDrawing();
                    if(iIEngine->GetTotalNumOfImages() > 0)
                        iDrawOnes = -1;
                    }
                
                break;
            
            case EOneByOne:
                if(iDrawOneByOne->IsDrawingNeededOneByOne())
                    {
                    iDrawOneByOne->DrawOnebyOneL(size);
                    EndDrawing();
                    }
                break;
            
            case EFaceBrowser:
                if(iDrawFaceBrowsing->IsDrawingNeededFaceBrowsing())
                    {
                    iDrawFaceBrowsing->DrawFaceBrowsing(size);
                    drawZoom = 1; inPictureX = 0; inPictureY = 0;
                    iDrawFaceBrowsing->GetFBZoomAndLocation(drawZoom, inPictureX, inPictureY);
                    EndDrawing();
                    }
                break;
            
            default:
                // Should never come here
                break;
            }
        }

#ifdef ADAPTIVE_FRAMERATE    
    iWaitDrawTicks = KWaitTicksAfterDraw;    // wait 20ms
#endif    
    }

void CImagicContainerBrowser::BeginDrawing()
    {
    // Calculate used time between two frames
    iTimeNow = User::NTickCount();

#ifdef __WINS__
    // In the emulator the tickcount runs at 200Hz
    iTimeDiff = (GLfloat)(iTimeNow - iLastTime)/200;
    iLastTime = iTimeNow;
#else    
    // In the HW the tickcount runs at 1000Hz
    iTimeDiff = (GLfloat)(iTimeNow - iLastTime)/1000;
    iLastTime = iTimeNow;
#endif
    
    //Prevent screen draw while rotating screen
    //if(!iScreenRotateOngoing)
        {
        // Get window size
        //aSize = Size();    
        
        // Enable client state
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer( 2, GL_FIXED, 0, iGlobalTexCoords );
        }
        
    }
    
void CImagicContainerBrowser::EndDrawing()
    {
#ifdef LOADING_ANIMATION
    // Handle loading animation
    glColor4f(1,1,1,1);
    iDrawUtility->Update();
    iDrawUtility->Draw(Size());
    
    TBool tmp = iMagFilterLinear;
    iMagFilterLinear = ETrue;


    DrawFaceBrowsingIcon();
    DrawZoomIcon();

    iMagFilterLinear = tmp;
#endif

    /*iMenuAlpha-=0.05;
    if(iMenuAlpha < 0)
        iMenuAlpha = 0;
    iDrawUtility->DrawIcon2(Size(), iLoadingTextureIndex, iMenuAlpha);*/
    
    
    //Prevent screen draw while rotating screen
    //if(!iScreenRotateOngoing)
        {
        // Disable client state
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    
        
        // Call eglSwapBuffers, which blit the graphics to the window
        if( !eglSwapBuffers( iEglDisplay, iEglSurface ) )
            {
            _LIT(KTextOpenGlError, "ERROR in OpenGL draw: eglSwapBuffers error");
            CAknErrorNote* note = new ( ELeave ) CAknErrorNote(ETrue);
            TBuf<256> text;
            text.Format(KTextOpenGlError);
            note->ExecuteLD( text );
            
            //suspend or some other power event occurred, context lost
            OpenGLInitL(); /* reinitialize EGL */
        
            InitL(); /* reinitialize grid data */
            iDrawFunction = EGrid;
            }
        
        // Check for errors
        //EGLint err = eglGetError();
        EGLint err = glGetError();
        //err=EGL_CONTEXT_LOST;
        //if(err != EGL_SUCCESS)
        TBool init = EFalse;
        //while(err != EGL_SUCCESS)GL_NO_ERROR
        while(err != GL_NO_ERROR)
            {
            /*_LIT(KTextOpenGlError, "ERROR in OpenGL draw: %d");
            CAknErrorNote* note = new ( ELeave ) CAknErrorNote(ETrue);
            TBuf<256> text;
            text.Format(KTextOpenGlError, err);
            note->ExecuteLD( text );*/
            
            //suspend or some other power event occurred, context lost
            /*DeleteTextures();
            OpenGLInitL();
            InitL();*/
            iDrawFunction = EGrid;
            err = glGetError();
            init = ETrue;
            }
        
        if(init)
            {
            DeleteTextures();
            OpenGLInitL(); /* reinitialize EGL */
            InitL(); /* reinitialize grid data */
            }
    
        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::SizeChanged()
// Called by framework when the view size is changed
// ---------------------------------------------------------
//
void CImagicContainerBrowser::SizeChanged()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SizeChanged"));
    
    iScreenRotateOngoing = ETrue;
    //TSize size = this->Size();
    iScreenSize = this->Size();
    iScreenAspectRatio = (TReal)iScreenSize.iWidth / (TReal)iScreenSize.iHeight;
    
    // Reinitialize viewport and projection.
    glViewport( 0, 0, iScreenSize.iWidth, iScreenSize.iHeight );
    
    if(iScreenSize.iHeight > iScreenSize.iWidth)
        {//Portrait
        iDisplayRotation = -90;
        iDisplayRotationTarget = 0;
        }
    else
        {//Landscape
        iDisplayRotation = 90;
        }
    
    iScreenRotateOngoing = EFalse;
    
    iDrawNow = ETrue;
    
    }

float CImagicContainerBrowser::GetDisplayRotTargetAngle()
    {
    return iDisplayRotationTarget;
    }

float CImagicContainerBrowser::GetDisplayRotAngle()
    {
    return iDisplayRotation;
    }

void CImagicContainerBrowser::SetDisplayRotAngle(float aValue)
    {
    iDisplayRotationTarget = aValue;
    }


// ---------------------------------------------------------
// CImagicContainerBrowser::CountComponentControls() const
// ---------------------------------------------------------
//
TInt CImagicContainerBrowser::CountComponentControls() const
    {
    return 0; // return nbr of controls inside this container
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::ComponentControl(TInt aIndex) const
// ---------------------------------------------------------
//
CCoeControl* CImagicContainerBrowser::ComponentControl(TInt /*aIndex*/) const
    {
   
    return NULL;
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::Draw(const TRect& aRect) const
// ---------------------------------------------------------
//
void CImagicContainerBrowser::Draw(const TRect& /*aRect*/) const
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::Draw"));

    CImagicContainerBrowser* self=const_cast<CImagicContainerBrowser*>(this);
    self->iDrawNow = ETrue;
    
    /*if(iScreenImmeadetaUpdate)
        self->DrawL();*/
    }

void CImagicContainerBrowser::SetScreenImmeadetaUpdate(TBool aValue)
    {
    iScreenImmeadetaUpdate = aValue;    
    }


// ---------------------------------------------------------
// CImagicContainerBrowser::HandleControlEventL(
//     CCoeControl* aControl,TCoeEvent aEventType)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandleControlEventL(CCoeControl* /*aControl*/,TCoeEvent /*aEventType*/)
    {
    // Add your control event handler code here
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::HandlePointerEventL(
//     const TPointerEvent& aPointerEvent)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandlePointerEventL(const TPointerEvent& aPointerEvent)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandlePointerEventL++"));

    EnableDisplayDraw();
    
    //Set normal display draw speed if we were in low draw freq
#ifdef USE_LOW_DRAW_SPEED_WHILE_LOADING
    SetDrawFreqToNormal(KPowerSavePeriodicDelay);
#endif
    
    iUserInputGiven = ETrue;
    iLastEventFromKeys = EFalse;
    iOnTheEdge=ETrue;
            
    /* Lets not put this on, othervise does not work in S60 3.2
    // Do nothing if non-touch
    if (!AknLayoutUtils::PenEnabled() )
        {
        return;
        }
*/

#ifdef USE_AVKON_LONGTAP_DETECTOR
    // Pass the pointer event to Long tap detector component
    iLongTapDetector->PointerEventL(aPointerEvent);
#endif

#ifdef USE_AVKON_TACTILE_FEEDBACK
    if (aPointerEvent.iType == TPointerEvent::EButton1Down)
            {
            // Give feedback to user (vibration)
            iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic);
            }
#endif
    
    if ( AknLayoutUtils::PenEnabled() )
        {
        iGesture->PointerEventL(aPointerEvent);
        }

    // Call base class HandlePointerEventL()
    CCoeControl::HandlePointerEventL(aPointerEvent);

    SetLastTouchPoint(aPointerEvent.iPosition);
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandlePointerEventL--"));
    }

void CImagicContainerBrowser::SetLastTouchPoint(const TPoint& aPos)
    {

    iTouchPointThreshold = EFalse;
    
    if(Abs(aPos.iX-iLastTouchPoint.iX) > 2)//TODO, mika: check correct size
        {
        iTouchPointThreshold = ETrue;
        }
    if(Abs(aPos.iY-iLastTouchPoint.iY) > 2)
        {
        iTouchPointThreshold = ETrue;
        }
    
    //Change touch point only if we moved more than 5 pixel
    //if(changeTouchPoint)
        iLastTouchPoint = aPos;
    }

TBool CImagicContainerBrowser::IsTouchPointThresholdExeed()
    {
    return iTouchPointThreshold;
    }

TPoint CImagicContainerBrowser::GetLastTouchPoint(void)
    {
    return iLastTouchPoint;
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::HandleLongTapEventL(
//     const TPoint& aPenEventLocation, const TPoint& aPenEventScreenLocation)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandleLongTapEventL(const TPoint& /*aPenEventLocation*/, 
                                                  const TPoint& /*aPenEventScreenLocation*/)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleLongTapEventL++"));
    
    //THIS FUNCTION IS NOT USED, USE INSTEAD DoLongTap() TO HANDLE LONG TAP EVENTS!!!
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleLongTapEventL--"));
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::FindImageInScreen(
//     const TPoint aPos, TInt& aResultIndex)
// ---------------------------------------------------------
//
TBool CImagicContainerBrowser::FindImageInScreen(const TPoint aPos, TInt& aResultIndex)
    {
    GLfloat modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);

    GLfloat projection[16];
    glGetFloatv(GL_PROJECTION_MATRIX, projection);

    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLfloat x = aPos.iX;
    GLfloat y = this->Size().iHeight - 1 - aPos.iY;
    GLfloat objX[2], objY[2], objZ[2]; // [0] for near, [1] for far

    // Get coords in near and far plane. The coords give the ray from near to far plane.
    gluUnProject(x,y,0.0,modelview,projection,viewport,&objX[0],&objY[0],&objZ[0]);
    gluUnProject(x,y,1.0,modelview,projection,viewport,&objX[1],&objY[1],&objZ[1]);

    DP3_IMAGIC(_L("####### UnProject(z=0, near):X=%6.4f,Y=%6.4f,Z=%6.4f"), objX[0],objY[0],objZ[0]);
    DP3_IMAGIC(_L("####### UnProject(z=1.0,far):X=%6.4f,Y=%6.4f,Z=%6.4f"), objX[1],objY[1],objZ[1]);
    DP3_IMAGIC(_L("####### Device (%d):x=%6.3f,y=%6.3f,z=not known"), iCurrentIndex, x,y);
    DP3_IMAGIC(_L("####### Current(%d):x=%6.4f,y=%6.4f"),iCurrentIndex, (iCurrentIndex / CImagicContainerBrowser::KGridSizeY) * KSpacingX, -(iCurrentIndex % CImagicContainerBrowser::KGridSizeY) * KSpacingY);

    GLfloat perspectiveDepth = objZ[0] - objZ[1]; // near value is bigger. depth = near - far.

    aResultIndex = -1; // gives invalid value if not found

    // The ray is linear. Can get xy at a depth by multiply '(a depth)/(near/far depth)'
    TInt numOfImages = iIEngine->GetTotalNumOfImages();
    for (TInt i = 0; i < numOfImages; ++i)
        {
        GLfloat itemZ = iIEngine->GetImageData(i)->iGridData.iZ;
        GLfloat itemDepthCoord = -itemZ - iDrawGrid->GetGridZoom(); // -  objZ[0]; // camera shifted by iDrawGridZoom. perspective starts from near.
        GLfloat itemDepth = itemDepthCoord / perspectiveDepth;

        GLfloat itemX = objX[0] + itemDepth * (objX[1] - objX[0]); // = near + u(far - near)
        GLfloat itemY = objY[0] + itemDepth * (objY[1] - objY[0]); // = near + u(far - near)

        CImageData* imageData = iIEngine->GetImageData(i);
        GLfloat itemScale = imageData->iGridData.iScale;
    
        GLfloat width  = KSpacingX; // * itemScale; // TODO: check!. No need to consider scaling. Why?
        GLfloat height = KSpacingY; // * itemScale;
        GLfloat objx = imageData->iGridData.iX;
        GLfloat objy = imageData->iGridData.iY;
        //GLfloat objx =  (i / CImagicContainerBrowser::KGridSizeY) * width;
        //GLfloat objy = -(i % CImagicContainerBrowser::KGridSizeY) * height;
        //TODO: consider aspect ratio

        DP3_IMAGIC(_L("####### UnProject: itemX=%f, itemY=%f, itemZ=%f"), itemX, itemY, itemZ);
        DP4_IMAGIC(_L("####### UnProject: itemDepthCoord=%f, perspectiveDepth=%f, itemDepth=%f, itemScale=%f"),
            itemDepthCoord, perspectiveDepth, itemDepth, itemScale);
        DP4_IMAGIC(_L("#######(%d) %6.4f < x(%6.4f) < %6.4f"), i, objx-width/2, itemX, objx+width/2);
        DP4_IMAGIC(_L("#######(%d) %6.4f < y(%6.4f) < %6.4f"), i, objy-height/2, itemY, objy+height/2);

        if (objx-width/2 <= itemX && itemX < objx + width/2)
            {
            if (objy-height/2 <= itemY && itemY < objy + height/2)
                {
                aResultIndex = i;
                break;
                }
            }
        }

#if 0
    // just trial code. useful for checking unprojection results
    z = 0.9997; // best reasonable fixed value from experience
    gluUnProject(x,y,z,modelview,projection,viewport,&objX,&objY,&objZ);
    RDebug::Print(_L("####### UnProject(z=%8.6f):X=%6.4f,Y=%6.4f,Z=%6.4f"), z,objX,objY,objZ);
#endif
        
    //DP1_IMAGIC(_L("####### SELECTED =====> %d"), selectedIndex);

    return (aResultIndex != -1)? ETrue: EFalse;
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::GetMaxX()
// ---------------------------------------------------------
//
TReal CImagicContainerBrowser::GetMaxX() const
    {
    TInt images = iIEngine->GetTotalNumOfImages();
    return images > 0 ? iIEngine->GetImageData(images - 1)->iGridData.iX : 0;
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::FindImageInOGl(
//     const FloatCoords aPos, TInt& aResultIndex)
// ---------------------------------------------------------
//
TBool CImagicContainerBrowser::FindNearestImageInOGl(const FloatCoords aPos, TInt& aResultIndex)
    {
    float width  = KSpacingX;
    float height = KSpacingY;
    float itemX = aPos.iX;
    float itemY = aPos.iY;
    TInt  image_num = iIEngine->GetTotalNumOfImages();

    DP2_IMAGIC(_L("1: itemXY=(%6.4f,%6.4f)"), itemX, itemY);

    itemX = Max(itemX, 0);
    itemX = Min(itemX, GetMaxX()); //image_num / CImagicContainerBrowser::KGridSizeY) * width);
    itemY = Max(itemY, 0);
    itemY = Min(itemY, (CImagicContainerBrowser::KGridSizeY - 1) * height);

    DP2_IMAGIC(_L("2: itemXY=(%6.4f,%6.4f)"), itemX, itemY);
    
    aResultIndex = -1; // gives invalid value if not found

    // The ray is linear. Can get xy at a depth by multiply '(a depth)/(near/far depth)'
    for (TInt i=0; i<image_num; ++i)
        {
        CImageData* imageData = iIEngine->GetImageData(i);
        GLfloat itemZ = imageData->iGridData.iZ;
        GLfloat objx = imageData->iGridData.iX;
        GLfloat objy = -imageData->iGridData.iY;
        //GLfloat objx = (i / iGridSizeY) * width;
        //GLfloat objy = (i % iGridSizeY) * height;

        if (objx-width/2 <= itemX && itemX < objx + width/2)
            {
            if (objy-height/2 <= itemY && itemY < objy + height/2)
                {
                aResultIndex = i;
                break;
                }
            }
        }

    return (aResultIndex != -1)? ETrue: EFalse;
    }


// ---------------------------------------------------------
// CImagicContainerBrowser::ConvertCoordsFromScreen2OGl(
//     const TPoint aPos, FloatCoords& aCoord)
// ---------------------------------------------------------
//
FloatCoords CImagicContainerBrowser::ConvertCoordsFromScreen2OGl(const TPoint aPos)
    {
    // Calculte OpenGL coords in current image for OneByOne view
    TSize size = this->Size();
    FloatCoords coord;
    
    // OpenGL coord (-0.5 - +0.5) = (relative position in screen) * (coordinate system)
    coord.iX = (((float)aPos.iX / size.iWidth ) - 0.5) * (iDrawOneByOne->GetDrawOneByOneWidth() * 2);
    coord.iY = (((float)aPos.iY / size.iHeight) - 0.5) * (iDrawOneByOne->GetDrawOneByOneHeight() * 2);

//    RDebug::Print(_L("ConvertCoordsFromScreen2OGl: aPos(%d,%d)/size(%d,%d)*ortho(%6.4f,%6.4f) = coord(%6.4f,%6.4f)"),
//            aPos.iX, aPos.iY, size.iWidth, size.iHeight, iDrawOnebyOneW, iDrawOnebyOneH, coord.iX, coord.iY);
    
    // scale -> translate -> rotate when display
    // translate -> scale on calculation. TODO: rotate needs to be considered.
    coord.iX = (coord.iX + iDrawOneByOne->GetDrawOneByOneXY().iX) / iDrawOneByOne->GetDrawOneByOneZoom(); 
    coord.iY = (coord.iY - iDrawOneByOne->GetDrawOneByOneXY().iY) / iDrawOneByOne->GetDrawOneByOneZoom();

//    RDebug::Print(_L("ConvertCoordsFromScreen2OGl: coord(%6.4f,%6.4f)+trans(%6.4f,%6.4f)*zoom(%6.4f)"),
//            coord.iX, coord.iY, iDrawOneByOneX, iDrawOneByOneY, iDrawOneByOneZoom);

    return coord;
    }


// ---------------------------------------------------------
// CImagicContainerBrowser::FindNearestFace(
//     const TPoint aPos, TInt& aResultIndex)
// ---------------------------------------------------------
//
TBool CImagicContainerBrowser::FindNearestFace(const TPoint aPos, TInt& aResultIndex)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::FindNearestFace++"));

    // number of faces in current image(iCurrentIndex). iCoordinates must be already set
    TInt facecount = iDrawFaceBrowsing->GetFaceCount();

    TBool isCloseEnough = EFalse;
    TBool found = EFalse;
    float closestDistance = 0.0;  // this is actully square of distance.

//    ConvertScreenCoords2QvgaCoords(TPoint& aPoint, TRect& aRect)
    
    FloatCoords coord = ConvertCoordsFromScreen2OGl(aPos);

    if(!GetScreenOrientation())
        {
        // swap x and y in portrait
        float temp = coord.iX;
        coord.iX = -coord.iY;
        coord.iY = temp ;
        }
    
    DP4_IMAGIC(_L("CImagicContainerBrowser::FindNearestFace: aPos(%d,%d) => Coord(%6.4f,%6.4f)"),aPos.iX,aPos.iY,coord.iX,coord.iY);
    
    for(TInt i=0; i<facecount; ++i)
        {
        FloatCoords tmp = iDrawFaceBrowsing->ConvertCoordsFromAlgo2OGl(i);

        DP3_IMAGIC(_L("CImagicContainerBrowser::FindNearestFace: ====> face %d(%6.4f,%6.4f)"), i, tmp.iX,tmp.iY);
        
        float distance;
        float diff_x = Abs(tmp.iX - coord.iX);
        float diff_y = Abs(tmp.iY - coord.iY);
        
        distance = diff_x*diff_x + diff_y*diff_y;

        DP3_IMAGIC(_L("CImagicContainerBrowser::FindNearestFace: diff x=%6.4f, y=%6.4f. distance=%8.5f)"), diff_x, diff_y, distance);
        
        if (distance < closestDistance || !found)
            {
            found = ETrue;
            aResultIndex = i;
            closestDistance = distance;
            }
        
        if (distance < KFindFaceSearchRange) isCloseEnough = ETrue;
        }

    DP1_IMAGIC(_L("CImagicContainerBrowser::FindNearestFace(%d)--"), isCloseEnough);

    return isCloseEnough;
    }


// ---------------------------------------------------------
// CImagicContainerBrowser::HandleGestureBeganL(
//     const TPoint& aPos)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandleGestureBeganL(const TPoint& aPos)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleGestureBeganL"));
    // do nothing
#ifdef SELECT_ON_TOUCHDOWN
    TInt sel;
    
    if ((iDrawFunction == EGrid) && FindImageInScreen(aPos, sel)) {
        iCurrentIndex = sel;
        iDrawNow = ETrue;
    }
#endif
    
#ifdef MOMENTUM_MOVE
    iMomentumMove = EFalse;
#endif

#ifdef RD_FACEFRAME
    //iDrawFaceFrame = ETrue;
    iDrawNow = ETrue;
#endif
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::HandleGestureMovedL(
//     const TPoint& aPos, const TGestureType aType)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandleGestureMovedL(const TPoint& aPos, const TGestureType aType)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleGestureMovedL++"));

    if (IS_GESTURE_TAPnDRAG(aType)) // tap&drag for zoom and rotate
        {
        DP0_IMAGIC(_L("IS_GESTURE_TAPnDRAG"));
        // Gesture movement and coord movement are opposite. EFalse for no wrap
        DoTapAndDrag(aPos, aType);
        }
    else if (IS_GESTURE_DRAG(aType)) // just a drag. not Tap and Drag
        {
        DP0_IMAGIC(_L("IS_GESTURE_DRAG"));
        // actions for dragging
        // This drag event occurs even finger movement is still within tap threshold
        DoDrag(aPos, aType);
#ifdef HOLD_SELECTION_ONDRAG
        iHoldSelection = ETrue;
#endif
        }
    else if (IS_GESTURE_CURSORSIMULATION(aType))
        {
        DP0_IMAGIC(_L("IS_GESTURE_CURSORSIMULATION"));
        DoCursorSimulation(aPos, aType);
        }
    else if (IS_GESTURE_LONGTAPPING(aType))
        {
        DP0_IMAGIC(_L("IS_GESTURE_LONGTAPPING"));
        DoLongTapping(aPos, aType);
        }

    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleGestureMovedL--"));
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::HandleGestureEndedL(
//     const TPoint& aPos, const TGestureType aType)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::HandleGestureEndedL(const TPoint& aPos, const TGestureType aType)
    {
#ifdef HOLD_SELECTION_ONDRAG
    // User doesn't touching screen and auto-move can start
    iHoldSelection = EFalse;
#endif
#ifdef RD_FACEFRAME
    //iDrawFaceFrame = EFalse;
#endif
    
// TODO: FIXME: unno temp fix endless cursor movement.
// These need to be set false when key was released. TODO: how to do in dragging in touch... 
    iTouchMoveData.iRight = EFalse;
    iTouchMoveData.iLeft = EFalse;
    iTouchMoveData.iUp = EFalse;
    iTouchMoveData.iDown = EFalse;//*mika*
    
    iOnTheEdge=EFalse;
    
    if (IS_GESTURE_SINGLETAP(aType))
        {
        DoSingleTap(aPos, aType);
        }
    else if (IS_GESTURE_DOUBLETAP(aType))
        {
        DoDoubleTap(aPos, aType);
        }
    else if (IS_GESTURE_LONGTAP(aType))
        {
        DoLongTap(aPos, aType);
        }
    else // Then DoFlick if not single/double tap
        {
        DoFlick(aPos, aType);
        }

    // Disable MagGlass when user releases finger 
    ShowMagGlass(EFalse);
    
    //Set normal display draw speed if we were in low draw freq
#ifdef USE_LOW_DRAW_SPEED_WHILE_LOADING
    SetDrawFreqToNormal(KTouchDelay);
#endif
    }

void CImagicContainerBrowser::DoTapAndDrag(const TPoint& /*aPos*/, const TGestureType aType)
    {
#ifdef TAP_AND_GESTURE
    // unno experimental code
    if (aType & EGestureUp)
        {
        iTouchMoveData.iZoomOut=ETrue;
        iDrawNow = ETrue;
        }
    if (aType & EGestureDown)
        {
        iTouchMoveData.iZoomIn=ETrue;
        iDrawNow = ETrue;
        }
    if (aType & EGestureLeft)
        {
        iTouchMoveData.iRotate -= 1;
        iDrawNow = ETrue;
        }
    if (aType & EGestureRight)
        {
        iTouchMoveData.iRotate += 1;
        iDrawNow = ETrue;
        }
#endif
    }

void CImagicContainerBrowser::DoCursorSimulation(const TPoint& /*aPos*/, const TGestureType aType)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoCursorSimulation++"));

    TInt x = 0, y = 0;
    TBool wrap = (iDrawFunction == EGrid)? EFalse: ETrue;

    // do nothing if it's in the mode with no cursor simulation
#ifndef CURSORSIMULATION_IN_GRID
    if (iDrawFunction == EGrid) return;
#endif

#ifndef CURSORSIMULATION_IN_ONEBYONE
#ifdef HOLD_SELECTION_ONDRAG
    if (iDrawFunction == EOneByOne) return;
#else
    if (((iDrawFunction == EOneByOne) && (!IS_NOT_IN_ZOOM_ONEBYONE))) return;

    // allow only left&right to move prev/next image. (do nothing if up or down)
    if((iDrawFunction == EOneByOne) && ((aType & EGestureUp) || (aType & EGestureDown))) return;
#endif // HOLD_SELECTION_ONDRAG
#endif

    // cursor move for dragging
    if (aType & EGestureUp)    ++y;
    if (aType & EGestureDown)  --y;
    if (aType & EGestureLeft)  ++x;
    if (aType & EGestureRight) --x;

    DP3_IMAGIC(_L("CImagicContainerBrowser::DoCursorSimulation : aType=%d, x=%d, y=%d"), aType, x, y);

    if (x || y) // move index only if x or y are changed
        MoveIndex(x, y, wrap);

    DP0_IMAGIC(_L("CImagicContainerBrowser::DoCursorSimulation--"));
    }

void CImagicContainerBrowser::DoDrag(const TPoint& aPos, const TGestureType aType)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoDrag++"));
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    ResetKeyData();
    ResetTouchData();
    ResetDirKeyData();
    
#ifdef HOLD_SELECTION_ONDRAG
    float minX, maxX, minY, maxY;
    TInt num = iIEngine->GetTotalNumOfImages();
#endif
#ifndef CURSORSIMULATION_IN_ONEBYONE
    TSize size = this->Size();
    FloatCoords coord;
#endif

    // do nothing if it's stationally (not beyond tap threshold) 
    if (IS_GESTURE_STATIONARY(aType)) return;

    //iDrawNow = ETrue;

    switch (iDrawFunction)
        {
        case EGrid: {
#ifdef HOLD_SELECTION_ONDRAG
            // Drag to shift view
            coord.iX = (GLfloat)aPos.iX / 120; //160; // TODO: calc right value from z coord & scale
            coord.iY = (GLfloat)aPos.iY / 90;  //120; // TODO: calc right value from z coord & scale

            minX = -KSpacingX/2; // allow to move one-picture-size away from the edge
            maxX = +KSpacingX/2 + GetMaxX();

            minY = 0; // don't allow to move away from the edge
            maxY = (CImagicContainerBrowser::KGridSizeY-1)*KSpacingY;

            if (iDrawGrid->GetGridXY().iX < minX - minX || maxX + minX < iDrawGrid->GetGridXY().iX) coord.iX /= 3;
            if (iDrawGrid->GetGridXY().iY < minY - minY || maxY + minY < iDrawGrid->GetGridXY().iY) coord.iY /= 3;
            
            TGridXY tmp = iDrawGrid->GetGridXY();
            tmp.iX -= coord.iX;
            tmp.iY -= coord.iY;
            CheckLimits(tmp.iX, minX, maxX);
            CheckLimits(tmp.iY, minY, maxY);
            iDrawGrid->SetGridXY(tmp);
#endif
            break; }
        case EOneByOne:
#ifndef CURSORSIMULATION_IN_ONEBYONE
            if (!IS_NOT_IN_ZOOM_ONEBYONE)
                {
                // Drag only in zoomed image
                coord.iX = (((float)aPos.iX / size.iWidth ) * (iDrawOneByOne->GetDrawOneByOneWidth() * 2)) / iDrawOneByOne->GetDrawOneByOneZoom();
                coord.iY = (((float)aPos.iY / size.iHeight) * (iDrawOneByOne->GetDrawOneByOneHeight() * 2)) / iDrawOneByOne->GetDrawOneByOneZoom();

                iDrawOneByOne->ChangeDrawOneByOneTargetX(-coord.iX);
                iDrawOneByOne->ChangeDrawOneByOneTargetY(-coord.iY);
                }
#ifdef HOLD_SELECTION_ONDRAG
            else if (!iDrawOneByOne->IsMagGlassOn() /*iMagGlassOn*/) // not in zoom, magglass off
                {
                // drag in x coord direction to move prev/next image
                coord.iX = (((float)aPos.iX / size.iWidth ) * (iDrawOneByOne->GetDrawOneByOneWidth() * 2)) / iDrawOneByOne->GetDrawOneByOneZoom();
                //iOneByOneFlow += coord.iX;
                float tmp = iDrawOneByOne->GetImgeFlowLocation();
                tmp+=coord.iX;
                iDrawOneByOne->SetImgeFlowLocation(tmp);
                
                iOneByOneSlideByDrag = ETrue;
                DP1_IMAGIC(_L("Sliding iOneByOneFlow=%6.4f"), iDrawOneByOne->GetImgeFlowLocation());
                }
#endif // HOLD_SELECTION_ONDRAG
#endif
            break;
        case EFaceBrowser:
// TODO: Integrate face browing and dragging!
            
            DP0_IMAGIC(_L("CImagicContainerBrowser::DoDrag - EFaceBrowser"));
            //DoFlick(aPos, aType);
        default:
            break;
        }
    
    iDrawNow = ETrue;
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoDrag--"));
    
    }

void CImagicContainerBrowser::DoFlick(const TPoint& aPos, const TGestureType aType)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoFlick++"));
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    
#ifndef CURSORSIMULATION_IN_ONEBYONE
    TSize size = this->Size();
    FloatCoords coord;
#endif
#ifdef MOMENTUM_MOVE
    float vX, vY, tX, tY;
    float minX, maxX;
#ifndef FLICK_ONLY_IN_X_IN_GRID
    float minY, maxY;
#endif
    TInt x, y, absX, absY;
    TBool valid_x, valid_y;
    TInt num = iIEngine->GetTotalNumOfImages();
#endif

    iDrawNow = ETrue;
    
    switch (iDrawFunction)
        {
        case EGrid: {
            DP0_IMAGIC(_L("CImagicContainerBrowser::DoFlick - EGrid"));
#ifdef MOMENTUM_MOVE
            // TODO: FIXME: Good value needed. Not 180. Consider non-liner speed.
            // TODO: 128 should be the size of images in Screen (pixels) at z = 0 (not zoomed)
const TInt FLICKUNIT=128; // movement of 128 px/s = 1 flick unit speed
//const TInt FLICK_2ND_GEAR = 8;
const TInt FLICK_2ND_GEAR = 16;
const TInt FLICK_3ND_GEAR = 8;
            x = aPos.iX;
            y = aPos.iY;
            absX = Abs(x);
            absY = Abs(y);

            valid_x = (absX && (absX * 5 > absY * 3))? ETrue: EFalse;
            valid_y = (absY && (absY * 5 > absX * 4))? ETrue: EFalse;

            if (!valid_x || (absX < FLICKUNIT / 3))
                {
                vX = tX = 0.0f;
                }
            else if (absX < FLICKUNIT)
                {
                vX = ((float)x / absX) * KSpacingX;
                tX = (       x / absX) * KSpacingX;

                if (tX>0 && iDrawGrid->GetGridTargetXY().iX < iDrawGrid->GetGridXY().iX) tX = 0;
                if (tX<0 && iDrawGrid->GetGridTargetXY().iX > iDrawGrid->GetGridXY().iX) tX = 0;
                }
            else if (absX < FLICKUNIT * FLICK_2ND_GEAR)
                {
                vX = ((float)x / FLICKUNIT) * KSpacingX;
                tX = (       x / FLICKUNIT) * KSpacingX;
                }
            else
                {
                float u = absX / FLICKUNIT;
                float v = u*u - 2*FLICK_3ND_GEAR*u + FLICK_3ND_GEAR*FLICK_3ND_GEAR + FLICK_3ND_GEAR;
                if (x < 0) v = -v;
                vX = ((float)v) * KSpacingX;
                tX = (       v) * KSpacingX;
                }

            vY = (valid_y)? ((float)y / FLICKUNIT) * KSpacingY: 0.0f;
            tY = (valid_y)? (       y / FLICKUNIT) * KSpacingY: 0.0f;
            
            DP4_IMAGIC(_L("vX/Y=(%6.4f, %6.4f), tX/Y=(%6.4f,%6.4f)"), vX, vY, tX, tY);
            DP2_IMAGIC(_L("aPos.iX/Y=(%d,%d)"), aPos.iX, aPos.iY);
            
            iMomentumMove = ETrue;

            iMomentumSpeedX = Abs(vX);
            //iDrawGridTargetX += (-tX);
            TGridXY tmp = iDrawGrid->GetGridTargetXY();
            tmp.iX += (-tX);
            
            minX = -KSpacingX/2; // allow to move one-picture-size away from the edge
            maxX = +KSpacingX/2 + GetMaxX();

            //CheckLimits(iDrawGridTargetX, minX, maxX);
            CheckLimits(tmp.iX, minX, maxX);
            iDrawGrid->SetGridTargetXY(tmp);

#ifndef FLICK_ONLY_IN_X_IN_GRID
            iMomentumSpeedY = Abs(vY);
            iDrawGridTargetY += (-tY);
            minY = 0; // don't allow to move away from the edge
            maxY = (iGridSizeY-1)*KSpacingY;
            CheckLimits(iDrawGridTargetY, minY, maxY);
#else
            iMomentumSpeedY = 0;
#endif
#endif
            break; }
        case EOneByOne:
            DP0_IMAGIC(_L("CImagicContainerBrowser::DoFlick - EOneByOne"));
            
#ifndef CURSORSIMULATION_IN_ONEBYONE
            vX = (float)aPos.iX / 2; // pixels of movement in 0.5 sec
            vY = (float)aPos.iY / 2;
            
            coord.iX = ((vX / size.iWidth ) * (iDrawOneByOne->GetDrawOneByOneWidth() * 2)) / iDrawOneByOne->GetDrawOneByOneZoom();
            coord.iY = ((vY / size.iHeight) * (iDrawOneByOne->GetDrawOneByOneHeight() * 2)) / iDrawOneByOne->GetDrawOneByOneZoom();

            iDrawOneByOne->ChangeDrawOneByOneTargetX(-coord.iX);
            iDrawOneByOne->ChangeDrawOneByOneTargetY(-coord.iY);
            
#endif
#ifdef HOLD_SELECTION_ONDRAG
            // Find current index if sliding is done by dragging in OneByOne view
            // iOneByOneSlideByDrag is set EFalse in HandleMovingKeysOneByOne() 
            // Flick is not currently affecting even though this is in DoFlick()
            DP2_IMAGIC(_L("Checking iOneByOneSlideByDrag (%d), iOneByOneFlow=%6.4f"), iOneByOneSlideByDrag, iDrawOneByOne->GetImgeFlowLocation());
            if (iOneByOneSlideByDrag)
                {
                if (iDrawOneByOne->GetImgeFlowLocation() > +KOneByOneSpacing / 10) 
                    MoveIndex(-1, 0, ETrue);
                if (iDrawOneByOne->GetImgeFlowLocation() < -KOneByOneSpacing / 10) 
                    MoveIndex(+1, 0, ETrue);
                }
#endif
            break;
        case EFaceBrowser:
            DP0_IMAGIC(_L("CImagicContainerBrowser::DoFlick - EFaceBrowser"));
            if(GetScreenOrientation())
                {
                if (aType & EGestureLeft)  MoveIndex( 1, 0, ETrue);
                if (aType & EGestureRight) MoveIndex(-1, 0, ETrue);
                }
            else
                {
                if (aType & EGestureDown)  MoveIndex( 1, 0, ETrue);
                if (aType & EGestureUp)    MoveIndex(-1, 0, ETrue);
                }
            break;
        default:
            break;
        }
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoFlick--"));
    }

void CImagicContainerBrowser::DoSingleTap(const TPoint& aPos, const TGestureType /*aType*/)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoSingleTap++"));
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    
    TInt sel;
    
    switch (iDrawFunction)
        {
        case EGrid:
            if (FindImageInScreen(aPos, sel))       // do nothing if user touches none
                {
                if (iCurrentIndex == sel)
                    {
                    SelectIndex();          // Open image if user touches current image
                    }
                else
                    {
                    iCurrentIndex = sel;    // focus touched image if it's not current one
                    iDrawNow = ETrue;
                    }
                }
            break;
        case EOneByOne:
            // TODO: Add face browsing by signle tap, maybe on zoom?
            DP1_IMAGIC(_L("DoSingleTap:iDrawOneByOneZoom=%6.4f"), iDrawOneByOne->GetDrawOneByOneZoom());
            if (IS_NOT_IN_ZOOM_ONEBYONE)
                {
                /*TInt dx = 50 - aPos.iX;
                TInt dy = 50 - aPos.iY;
                
                // start face browing if user taps at top left when face icon is shown 
                if (iFaceExists && (dx*dx + dy*dy < 50*50))
                    iView->HandleCommandL(EImagicCmdViewFaceBrowsing);
                else*/
                    SelectIndex(); // exits only when non zoomed.
                }
#ifdef SINGLETAP_CLOSE_IN_ZOOM
            else
                {
                // Stop zooming if it's in zoom
                //iDrawOneByOne->SetDrawOneByOneTargetZoom(KDoubleTapZoomOneByOne1);//mika, single tap disabled in zzomed in picture
                iDrawNow = ETrue;
                }
#endif
            break;
        case EFaceBrowser:
#ifdef SINGLETAP_CLOSE_IN_ZOOM
            // TODO: FIXME. Temporary hack to start OnebyOne view. Pretend it's grid and do select action 
            //iDrawFunction = EGrid;
            //SetDrawMode(EGrid);
            /*SelectIndex();
            SetDrawMode(EOneByOne);*/
            
#endif
            break;
        default:
            break;
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoSingleTap--"));
    
    }

void CImagicContainerBrowser::DoDoubleTap(const TPoint& aPos, const TGestureType /*aType*/)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoDoubleTap++"));
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    
#ifdef DOUBLETAP_SELECT_IN_GRID
    TInt sel;
#endif
    iDrawNow = ETrue;
    
    switch (iDrawFunction)
        {
#define TOGGLE_VALUE(tgt, val1, val2)  (tgt)=((tgt)!=(val1))?(val1):(val2)
        case EGrid:
#ifdef DOUBLETAP_SELECT_IN_GRID
            if (FindImageInScreen(aPos, sel))       // do nothing if user touches none
                {
                iCurrentIndex = sel;    // focus touched image if it's not current one
                SelectIndex();          // Open image if user touches current image
                }
#endif
#ifdef ENABLE_GRID_ZOOM
#ifdef DOUBLETAP_ZOOMGRID
            TOGGLE_VALUE(iDrawGridTargetZoom, KDoubleTapZoomGrid1, KDoubleTapZoomGrid2);
#endif
#endif
            break;
        case EOneByOne:
#ifdef DOUBLETAP_FACEBROWSING
            if (IS_NOT_IN_ZOOM_ONEBYONE)
                {
                iView->HandleCommandL(EImagicCmdViewFaceBrowsingWithCoordinates);
                }
#endif
            // toggle zooming on and off if MagGlass is off
            if(!iDrawOneByOne->IsMagGlassOn()/*iMagGlassOn*/)
                {
                //TOGGLE_VALUE(iDrawOneByOneTargetZoom, KDoubleTapZoomOneByOne1, KDoubleTapZoomOneByOne2);
                if(iDrawOneByOne->GetDrawOneByOneTargetZoom() == KDoubleTapZoomOneByOne1)
                    iDrawOneByOne->SetDrawOneByOneTargetZoom(KDoubleTapZoomOneByOne2);
                else
                    iDrawOneByOne->SetDrawOneByOneTargetZoom(KDoubleTapZoomOneByOne1);
                }

            if (iDrawOneByOne->GetDrawOneByOneTargetZoom() == KDoubleTapZoomOneByOne2)
                {
                // if we are zooming in, tapped position is set as center
                TSize size = this->Size();
                float imageWidth, imageHeight;
                CalculateImageSize(imageWidth, imageHeight, (float)size.iWidth/(float)size.iHeight);

                /*iDrawOneByOneTargetX = (2*iImageWidth) *(((float)aPos.iX / size.iWidth ) - 0.5);
                iDrawOneByOneTargetY = (2*iImageHeight)*(((float)aPos.iY / size.iHeight) - 0.5);*/
                TDrawOneByOneXY tmp; 
                tmp.iX = (2*imageWidth) *(((float)aPos.iX / size.iWidth ) - 0.5);
                tmp.iY = (2*imageHeight)*(((float)aPos.iY / size.iHeight) - 0.5);
                iDrawOneByOne->SetDrawOneByOneTargetXY(tmp);

                /*if(!GetScreenOrientation())
                    {
                    float tmp = iDrawOneByOneTargetX;
                    iDrawOneByOneTargetX = -iDrawOneByOneTargetY; 
                    iDrawOneByOneTargetY = tmp;
                    }*/
                }
            break;
        case EFaceBrowser:
#ifdef DOUBLETAP_FACEBROWSING
            // TODO: FIXME. Temporary hack to start OnebyOne view. Pretend it's grid and do select action 
            //iDrawFunction = EGrid;
            drawZoom = 1; inPictureX = 0; inPictureY = 0;
            iDrawFaceBrowsing->GetFBZoomAndLocation(drawZoom, inPictureX, inPictureY);
            iDrawOneByOne->InitDrawOnebyOne(drawZoom, inPictureX, inPictureY);
            
            SelectIndex();
            SetDrawMode(EOneByOne/*EGrid*/);
#endif
            break;
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DoDoubleTap--"));
    }

void CImagicContainerBrowser::DoLongTapping(const TPoint& aPos, const TGestureType aType)
    {
    DP1_IMAGIC(_L("CImagicContainerBrowser::DoLongTapping aPos.iX: %d"), aPos.iX);
    DP1_IMAGIC(_L("CImagicContainerBrowser::DoLongTapping aPos.iY: %d"), aPos.iY);
    
    //ResetTouchData();//mika to test if "stucking" problem is fixed
    
    // touch movement is stationary for enough long time 
    switch (iDrawFunction)
        {
        case EOneByOne:
            ShowMagGlass(ETrue);
            break;
        case EGrid:
            iView->ProcessCommandL(EAknSoftkeyOptions);
            /*TInt sel;
            if (FindImageInScreen(aPos, sel))       // do nothing if user touches none
                {
                if (iCurrentIndex == sel)
                    {
                    iView->ProcessCommandL(EAknSoftkeyOptions);
                    //iView->ProcessCommandL(EImagicCmdViewBrowserShowImageInfo);
                    }
                }*/
                
            /*else if(aPos.iX > 100)
                {
                iView->ProcessCommandL(EAknSoftkeyOptions);
                }*/
#if 0
            else
                {
                //iView->ProcessCommandL(EAknSoftkeyOptions);
                }
#endif           
            //iView->ProcessCommandL(EAknSoftkeyOptions);//TODO, options should be shown from softkeys 
            break;

        case EFaceBrowser:
            break;
        default:
            // do nothing
            break;
        }
    
    iDrawNow = ETrue;
    
    }

void CImagicContainerBrowser::DoLongTap(const TPoint& aPos, const TGestureType aType)
    {
    // do nothing so far.
    iDrawNow = ETrue;
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::MoveIndex(
//     TInt aMoveX, TInt aMoveY, TBool aWrap)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::MoveIndex(TInt aMoveX, TInt aMoveY, TBool aWrap)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::MoveIndex++"));
    
    TInt num = iIEngine->GetTotalNumOfImages();
    // Reset key data
    iTouchMoveData.iRight = EFalse;
    iTouchMoveData.iLeft = EFalse;
    iTouchMoveData.iUp = EFalse;
    iTouchMoveData.iDown = EFalse;//*mika*
    iTouchMoveData.iX = 0;
    iTouchMoveData.iY = 0;

    if (aMoveX < 0) // left key press
        {
        if (aWrap || iCurrentIndex >= CImagicContainerBrowser::KGridSizeY)
            {
            if(iDrawFunction == EFaceBrowser)
                //iFaceNro--; // prev face in face browser
                iDrawFaceBrowsing->DecFaceNumber();
            else
                {
                iTouchMoveData.iX    = aMoveX;
                iTouchMoveData.iLeft = ETrue;
                }
            iDrawNow       = ETrue;
            }
        }
    
    if (aMoveX > 0) // right key press
        {
        if (aWrap || iCurrentIndex < num - CImagicContainerBrowser::KGridSizeY)
            {
            if(iDrawFunction == EFaceBrowser)
                //iFaceNro++; // next face in face browser
                iDrawFaceBrowsing->IncFaceNumber();
            else
                {
                iTouchMoveData.iX     = aMoveX;
                iTouchMoveData.iRight = ETrue;
                }
            iDrawNow        = ETrue;
            }
        }

    if (aMoveY < 0) // up key press
        {
        if (aWrap || iCurrentIndex % CImagicContainerBrowser::KGridSizeY > 0)
            {
            iTouchMoveData.iY  = aMoveY;
            iTouchMoveData.iUp = ETrue;
            iDrawNow = ETrue;
            }
        }
    
    if (aMoveY > 0) // down key press
        {
        if ( aWrap || 
            ((iCurrentIndex % CImagicContainerBrowser::KGridSizeY < CImagicContainerBrowser::KGridSizeY - 1) &&
             (iCurrentIndex < num - 1)))
            {
            iTouchMoveData.iY    = aMoveY;
            iTouchMoveData.iDown = ETrue;
            iDrawNow = ETrue;
            }
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::MoveIndex--"));
    }

// ---------------------------------------------------------
// CImagicContainerBrowser::SelectIndex(
//     void)
// ---------------------------------------------------------
//
void CImagicContainerBrowser::SelectIndex(void)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SelectIndex++"));
    
    iDrawNow = ETrue;
    
    if(iDrawFunction == EGrid)
        {
        //Open one by one mode
        iDrawOneByOne->InitDrawOnebyOne(KInitDrawZoom/*drawZoom*/, inPictureX, inPictureY);
        
        //Set Grid view
        if(iDrawFunction == EFaceBrowser)
            {
            //iDrawFunction = EGrid;
            SetDrawMode(EGrid);
            }
        
        iView->SetFaceBrowsingMode(EFaceBrowserNone);
        }
    else if(iDrawFunction == EOneByOne)
        {
/*#ifdef SUPERZOOM
        //Unload high res textures when going back to Grid
        CImageData* aGridData = iIEngine->GetImageData(iCurrentIndex); 
        iTextureLoader->ReleaseSuperHResTexture( aGridData );
#endif*/  
        
        //Return to previous mode
        iDrawGrid->InitDrawGrid();
        //iDrawFunction = iDrawOneByOnePreviousFunc;
        //iDrawFunction = EGrid;
        SetDrawMode(EGrid);
        iView->SetFaceBrowsingMode(EFaceBrowserNone);
        }
    else if(iDrawFunction == EFaceBrowser)
        {
        iDrawGrid->InitDrawGrid();
        //iDrawFunction = EGrid;
        //SetDrawMode(EGrid);
        SetDrawMode(EOneByOne);
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SelectIndex--"));
    }

//#endif

void CImagicContainerBrowser::SetLoadingOn(TBool aValue)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetLoadingOn++"));
	DP1_IMAGIC(_L("CImagicContainerBrowser::SetLoadingOn - value: %d"), aValue);
    
    iDynamicLoadingOn = aValue;
    //iDrawNow = ETrue;
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetLoadingOn--"));
    }


void CImagicContainerBrowser::ImageLoadedL(TInt aError, CFbsBitmap* aBitmap, TThumbSize aResolution)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL++"));
    
    //Check do we need to unload or load more images ------>
    //LoadUnloadImages();
    DynamicUnLoading();
    
    TInt mem = 0;
    TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - Free RAM: %d"), mem);
    
    if(mem <= 16000000){
        DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - buffer size: 1"));
        K512TNImageBuffer = 1;
        }
    if(mem <= 20000000){
        DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - buffer size: 2"));
        K512TNImageBuffer = 2;
        }
    if(mem > 24000000){
        DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - buffer size: 3"));
        K512TNImageBuffer = 3;
        }  
    if(mem > 28000000){
        DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - buffer size: 4"));
        K512TNImageBuffer = 4;
        }
    if(mem > 32000000){
        DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL - buffer size: 5"));
        K512TNImageBuffer = 5;
        }
    
    if(aResolution == ESize512x512 || aResolution == EFullSize)
        {
        iPreferHighResLoading = EFalse;
        iDynamicLoadingOn = ETrue;
        DynamicLoadingL();
        }
    
    iTextureLoader->ImageLoadedL(aError, aBitmap, iGLMaxRes);
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::ImageLoadedL--"));
    }


//This is correcting the aspact ratio to be correct for OpengGL drawing
void CImagicContainerBrowser::SetPictureVertices(CImageData* aData, GLfixed* aVertices)
    {
    //DP0_IMAGIC(_L("CImagicContainerBrowser::SetPictureVertices"));
    
    GLfixed vx, vy;		
   
    // Use real image
    if(aData->iGridData.iGlLQ128TextIndex != 0 ||
       aData->iGridData.iGlLQ32TextIndex != 0 ||
       aData->iGridData.iGlHQ512TextIndex != 0)
        {
        if(aData->GetAspectRatio() > 1)
            {
			vx = 1<<15;
            vy = (0.5/aData->GetAspectRatio())*(1<<16);
            }
        else
            {
            vx = (0.5*aData->GetAspectRatio())*(1<<16);
			vy = 1<<15;
            }
        }
    // Use loading image
	else	
		{
		vx = 24000;		
    	vy = vx / KLoadingImageAspectRatio;
		}
    
    aVertices[0*2+0] = -vx;
    aVertices[0*2+1] = -vy;
    
    aVertices[1*2+0] = vx;
    aVertices[1*2+1] = -vy;
    
    aVertices[2*2+0] = -vx;
    aVertices[2*2+1] = vy;
    
    aVertices[3*2+0] = vx;
    aVertices[3*2+1] = vy;

    }

void CImagicContainerBrowser::DisplayDeleteQueryDialogL(TInt aResourceId)
    {
    CAknQueryDialog* dlg;
    dlg = new ( ELeave ) CAknQueryDialog();
    TInt result = dlg->ExecuteLD( aResourceId );
    if(result != KErrNone)
        {
        //Delete file
        DeleteImageL();
        }
    }

//Delete current image
void CImagicContainerBrowser::DeleteImageL()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DeleteImageL++"));
    
    //Delete image from engine
    //Delete grid image from OpenGL memory
    //TInt num = iIEngine->GetTotalNumOfImages();
    CImageData* data = iIEngine->GetImageData(iCurrentIndex);
    
    if(data->iGridData.iGlLQ128TextIndex)
        glDeleteTextures( 1, &data->iGridData.iGlLQ128TextIndex );
    if(data->iGridData.iGlLQ32TextIndex)
        glDeleteTextures( 1, &data->iGridData.iGlLQ32TextIndex );
    if(data->iGridData.iGlHQ512TextIndex != 0)
        glDeleteTextures(1, &data->iGridData.iGlHQ512TextIndex);
    if(data->iGridData.iGlSuperHQTextIndex != 0)
        glDeleteTextures(1, &data->iGridData.iGlSuperHQTextIndex);
    
    data->iGridData.iGlLQ32TextIndex = 0;
    data->iGridData.iGlLQ128TextIndex = 0;
    data->iGridData.iGlHQ512TextIndex = 0;
    data->iGridData.iGlSuperHQTextIndex = 0;
                    
    //Delete image from engine
    TInt err = iImagicAppUi->DeleteImage(iCurrentIndex);
    if (err != KErrNone)
        {
        iImagicAppUi->GetImagicUtils()->ExecuteQueryDialog(R_CANNOT_DELETE_DIALOG);
        }
    
    iDrawOneByOne->SetDrawOneByOneTargetZoom(1);
    iDrawNow = ETrue;
    
    iDrawGrid->UpdateImageCoordinates(iCurrentIndex);
    
    if(iIEngine->GetTotalNumOfImages() <= 0)
        {
        iImagicAppUi->GetImagicUtils()->ExecuteQueryDialog(0, R_NO_IMAGES_DIALOG);
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DeleteImageL--"));
    }
     


//Returns TRUE if screen is landscape
TBool CImagicContainerBrowser::GetScreenOrientation()
    {
    //Landscape
#ifdef _ACCELEROMETER_SUPPORTED_
    //if(iDeviceOrientation == EOrientationDisplayLeftUp)//Landscape
    if(iDeviceOrientation == EOrientationDisplayLeftUp || iDeviceOrientation == EOrientationDisplayRigthUp)//Landscape
        {
        DP1_IMAGIC(_L("CImagicContainerBrowser::GetScreenOrientation - Landscape: %d"),iDeviceOrientation);
        iDeviceOrientationAngle = 0;
        return ETrue;
        }
    else if(iDeviceOrientation == EOrientationDisplayDown)
        {
        DP1_IMAGIC(_L("CImagicContainerBrowser::GetScreenOrientation - Portrait: %d"),iDeviceOrientation);
        iDeviceOrientationAngle = -90;
        return EFalse;
        }
#else
    if(this->Size().iWidth > this->Size().iHeight)

        {
        DP1_IMAGIC(_L("CImagicContainerBrowser::GetScreenOrientation - Landscape: %d"),iDeviceOrientation);
        return ETrue;
        }
    else
        {
        DP1_IMAGIC(_L("CImagicContainerBrowser::GetScreenOrientation - Portrait: %d"),iDeviceOrientation);
        return EFalse;
        }
#endif
    }

//This should be called when reading touch UI events or key events to set draw freq to normal
void CImagicContainerBrowser::SetDrawFreqToNormal(TInt aTimerDelay)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetDrawFreqToNormal++"));
    
    //Set normal display draw speed if we were in power save
    if(iDisplayDrawFreq == KPowerSaveDisplayDrawFreq)
        {
        iDisplayDrawFreq = KDisplayDrawFreq;
        DisableDisplayDraw();
        if(iImagicAppUi->IsAppOnTop())
            EnableDisplayDraw();
        }
    //Reset power save timer
    iPowerSavePeriodic->Cancel();
    //And start it again, but only if we are loading images
    if(iTextureLoader->IsActiveAndRunning() && iDrawFunction == EGrid)
        {
        iPowerSavePeriodic->Start(/*KPowerSavePeriodicDelay*/aTimerDelay, KPowerSavePeriodicInterval, TCallBack( CImagicContainerBrowser::PowerSaveCallBack, this ) );
        }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetDrawFreqToNormal--"));
    }
        
void CImagicContainerBrowser::ResetZoomKeys()
    {
    iKeyData.iZoomInKey=EFalse;
    iKeyData.iZoomOutKey=EFalse;    
    }
                
  
TKeyResponse CImagicContainerBrowser::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::OfferKeyEventL"));

    //Set normal display draw speed if we were in low draw freq
#ifdef USE_LOW_DRAW_SPEED_WHILE_LOADING
    SetDrawFreqToNormal(KPowerSavePeriodicDelay);
#endif
    
    //if(iImagicAppUi->IsAppOnTop())
        EnableDisplayDraw();
    
    iUserInputGiven = ETrue;
    iLastEventFromKeys = ETrue;
    
    TKeyResponse ret = EKeyWasNotConsumed;
	
	// Set default values
	TInt addValue=0;
	TBool pressValue=EFalse;
	ResetKeyData();
	ResetTouchData();
	ResetDirKeyData();
	iDrawOneByOne->ChangeDrawOneByOneTargetX(0);
    iDrawOneByOne->ChangeDrawOneByOneTargetY(0);
	
	// Set button as pressed and add press counter by one
    if (aType == EEventKey)
        {
        iDrawGrid->KeyEvent();
        iDrawOneByOne->KeyEvent();
        iDrawFaceBrowsing->KeyEvent();
                
        iDrawNow = ETrue;
        addValue=1;
        pressValue=ETrue;
        }
    
    if (aType == EEventKeyDown)
        {
        iDrawGrid->KeyPressed();
        iDrawOneByOne->KeyPressed();
        iDrawFaceBrowsing->KeyPressed();
        
        /*if(iDrawFunction == EOneByOne && IS_NOT_IN_ZOOM_ONEBYONE)
            iDrawOneByOne->InitDrawOnebyOne();*/
        
        iIntheEndOfGrid = EFalse;
        iOnTheEdge=ETrue;
        iKeyPressedDown=ETrue;
        pressValue=ETrue;
        ///
        iDrawNow = ETrue;
        addValue=1;
        }
    
    if (aType == EEventKeyUp)
        {
        iDrawGrid->KeyReleased();
        iDrawOneByOne->KeyReleased();
        iDrawFaceBrowsing->KeyReleased();
        
        iOnTheEdge=EFalse;
        iKeyPressedDown=EFalse;
        pressValue=EFalse;
        addValue=0;
        //iKeyCounter=0;
        
        if(iIntheEndOfGrid)
            iJumpOver = ETrue;
        else
            iJumpOver = EFalse;
        }
	    
    CImageData* imageData = iIEngine->GetImageData(iCurrentIndex);
    TInt imageRotation = 0 - (TReal)imageData->GetOrientation();
    
    
	//if (aType==EEventKey /*|| aType==EEventKeyUp || aType==EEventKeyDown*/)
//	if(iKeyCounter == 0)
		{
		// Assume that key will be handled
		ret = EKeyWasConsumed;
		
		// Check for key
		switch (aKeyEvent.iScanCode)
			{
			// Vertical movement
		    case EStdKeyDownArrow:
		        if(iDrawFunction == EOneByOne && IS_NOT_IN_ZOOM_ONEBYONE){
		        }
		        else{
		        iKeyData.iY+=addValue;
		        iKeyData.iDown=pressValue;
		        }
#ifdef _ACCELEROMETER_SUPPORTED_
		        if((iDeviceOrientation == EOrientationDisplayDown &&
                   iDrawFunction == EFaceBrowser && aType == EEventKey))
		            {
		            iDrawFaceBrowsing->DecFaceNumber();
		            }
		        else if( (iDrawFunction == EFaceBrowser && aType == EEventKey &&
                         (imageRotation == -90 || imageRotation == -270)) )
                    {
                    if(imageRotation == -90)
                        iDrawFaceBrowsing->DecFaceNumber();
                    else
                        iDrawFaceBrowsing->IncFaceNumber();
                    }
                    
#endif
		        ResetZoomKeys();
				break;
				
	        case EStdKeyUpArrow:
	            if(iDrawFunction == EOneByOne && IS_NOT_IN_ZOOM_ONEBYONE){
	            }
	            else{
	            iKeyData.iY-=addValue;
	            iKeyData.iUp=pressValue;
	            }
#ifdef _ACCELEROMETER_SUPPORTED_
	            if((iDeviceOrientation == EOrientationDisplayDown &&
                   iDrawFunction == EFaceBrowser && aType == EEventKey))
	                {
                   iDrawFaceBrowsing->IncFaceNumber();
	                }
	            else if( (iDrawFunction == EFaceBrowser && aType == EEventKey &&
                         (imageRotation == -90 || imageRotation == -270)) )
                    {
                    if(imageRotation == -90)
                        iDrawFaceBrowsing->IncFaceNumber();
                    else
                        iDrawFaceBrowsing->DecFaceNumber();
                    }
                
#endif
	            ResetZoomKeys();
				break;
			
            //Horisontal movement
		    case EStdKeyLeftArrow:
			    if(
#ifdef _ACCELEROMETER_SUPPORTED_
			       (iDeviceOrientation == EOrientationDisplayLeftUp &&
#endif
			       iDrawFunction == EFaceBrowser && aType == EEventKey) ||
			       
			       (iDrawFunction == EFaceBrowser && aType == EEventKey &&
                   imageRotation == (imageRotation == -90 || imageRotation == -270)) )
			        {
			        iDrawFaceBrowsing->DecFaceNumber();
			        }
			    iKeyData.iX-=addValue;
			    iKeyData.iLeft=pressValue;
                ResetZoomKeys();
			    break;
				
			case EStdKeyRightArrow:
			    if(
#ifdef _ACCELEROMETER_SUPPORTED_
			       (iDeviceOrientation == EOrientationDisplayLeftUp &&
#endif
			       iDrawFunction == EFaceBrowser && aType == EEventKey) ||
			       
			       (iDrawFunction == EFaceBrowser && aType == EEventKey &&
                   imageRotation == (imageRotation == -90 || imageRotation == -270)) )
			        {
			       iDrawFaceBrowsing->IncFaceNumber();
			        }
			    iKeyData.iX+=addValue;
			    iKeyData.iRight=pressValue;
                ResetZoomKeys();
			    break;
			
			// Rotation
#if 1
			case '1':
			case 'A':
			case EStdKeyNkp1:
			    {
			    if (/*aType==EEventKey *//*|| aType==EEventKeyUp || */aType==EEventKeyDown)
			        {
                    CImageData* imageData = iIEngine->GetImageData(iImagicAppUi->GetImageIndex());
                    TInt rotAngle = imageData->GetOrientation();
                    imageData->SetOrientation((rotAngle + 90)%360);
                    iIEngine->SetImageRotation(iImagicAppUi->GetImageIndex());
                    iIEngine->SetDBChanged(imageData);
			        }
			    
				ResetZoomKeys();
				ResetDirKeyData();
			    break;
			    }

			case '3':
			case 'S':
			case EStdKeyNkp3:
			    {
			    if (/*aType==EEventKey *//*|| aType==EEventKeyUp || */aType==EEventKeyDown)
			        {
                    CImageData* imageData = iIEngine->GetImageData(iImagicAppUi->GetImageIndex());
                    TInt rotAngle = imageData->GetOrientation();
                    imageData->SetOrientation((rotAngle + 270)%360);
                    iIEngine->SetImageRotation(iImagicAppUi->GetImageIndex());
                    iIEngine->SetDBChanged(imageData);
			        }
			                    
				ResetZoomKeys();
				ResetDirKeyData();
				break;
			    }
#endif		

			case EStdKeySpace:
			case EKeySpace:
			case '0':
			case EStdKeyNkp0:
#ifdef _ACCELEROMETER_SUPPORTED_
			    //Check that accelerometer did not found by engine
			    DP0_IMAGIC(_L("CImagicContainerBrowser::OfferKeyEvent - rotate pressed"));
			    if(!iIEngine->IsAccelerometerExists() && aType == EEventKeyDown)
			        {
			        if(iIEngine->GetDeviceOrientation() == EOrientationDisplayLeftUp)
                        iIEngine->SetDeviceOrientation(EOrientationDisplayDown);
                    else if(iIEngine->GetDeviceOrientation() == EOrientationDisplayDown)
                        iIEngine->SetDeviceOrientation(EOrientationDisplayLeftUp);
                    }
			    
			    ResetZoomKeys();
			    ResetDirKeyData();
#endif
				break;
				
			//case EStdKeySpace:
            //case EKeySpace:
			case 'M':	
			    if(aType == EEventKeyDown)
			        {
                    iView->ProcessCommandL(EAknSoftkeyOptions);
                    ResetZoomKeys();
                    ResetDirKeyData();
			        }
				break;
				
			// Zooming
			case 'Q':
			case EStdKeyNkpAsterisk:
			case '*':
			case EStdKeyIncVolume:
			    
			    ResetDirKeyData();
			    
			    if(iDrawFunction == EOneByOne)
			        {
			        //iKeyData.iZoom+=addValue;
			        //iKeyData.iZoomIn=pressValue;
			        iKeyData.iZoomInKey=ETrue;
			        iKeyData.iZoomOutKey=EFalse;
			        }
			    if(iDrawFunction == EGrid)
			        {
			        //Open one by one mode
			        iDrawOneByOne->InitDrawOnebyOne(KInitDrawZoom/*drawZoom*/, inPictureX, inPictureY);
                    //Set Grid view
                    if(iDrawFunction == EFaceBrowser)
                        {
                        //iDrawFunction = EGrid;
                        SetDrawMode(EGrid);
                        }
                    
                    iView->SetFaceBrowsingMode(EFaceBrowserNone);
                    }
			    //ResetDirKeyData();
				break;
			
            case 'W':
            case EStdKeyHash:
			case '#':
			case EStdKeyDecVolume:
			    
			    ResetDirKeyData();
			    
			    if(iDrawFunction == EOneByOne)
			        {
			        //iKeyData.iZoom-=addValue;
			        //iKeyData.iZoomOut=pressValue;
			        iKeyData.iZoomInKey=EFalse;
			        iKeyData.iZoomOutKey=ETrue;
			        }
			        
			    if(iDrawFunction == EOneByOne && IS_NOT_IN_ZOOM_ONEBYONE && aType == EEventKeyDown)
			        {
/*#ifdef SUPERZOOM
                    iIEngine->CancelFullSizeLoading();
                    
                    //Unload high res textures when going back to Grid
                    CImageData* aGridData = iIEngine->GetImageData(iCurrentIndex); 
                    iTextureLoader->ReleaseSuperHResTexture( aGridData );
#endif*/  
                    //Return to previous mode
                    iDrawGrid->InitDrawGrid();
                    SetDrawMode(EGrid);
                    
                    iView->SetFaceBrowsingMode(EFaceBrowserNone);
			        }
			    //ResetDirKeyData();
				break;
			
			/*case 'F':
				iView->HandleCommandL(EImagicCmdViewFaceBrowsing);
				break;*/
			
			case EKeyBackspace:
			case EStdKeyBackspace:
                iView->HandleCommandL(EImagicCmdViewBrowserDelete);
                ResetZoomKeys();
                ResetDirKeyData();
                break;
                
			case 'I':
			    if(aType == EEventKeyDown)
			        {
                    iView->HandleCommandL(EImagicCmdViewBrowserShowImageInfo);
                    ResetZoomKeys();
                    ResetDirKeyData();
			        }
                break;
                
                
			// Selection
            case EStdKeyEnter:
			case EKeyEnter:
			case EStdKeyDevice3: //rocker selection key
                if (aType == EEventKey)
                    {
                    HandleDrawingModeSwitch(iDrawFunction);
                    }
                ResetZoomKeys();
                ResetDirKeyData();
			break;
			
			default:
				// Unknown key, it was not consumed
				ret = EKeyWasNotConsumed;
				ResetZoomKeys();
				ResetDirKeyData();
				break;
			}
		}
		
    DrawNow();
	
    return ret;
    }

void CImagicContainerBrowser::HandleDrawingModeSwitch(TDrawFunction& aDrawFunction)
    {
    //EGrid
    if(aDrawFunction == EGrid)
        {
        //Open one by one mode
        iDrawOneByOne->InitDrawOnebyOne(KInitDrawZoom/*drawZoom*/, inPictureX, inPictureY);
        
        //Set Grid view
        if(aDrawFunction == EFaceBrowser)
            SetDrawMode(EGrid);
            //aDrawFunction = EGrid;
        
        iView->SetFaceBrowsingMode(EFaceBrowserNone);
        }
    
    //EOneByOne
    else if(aDrawFunction == EOneByOne)
        {
        if(IS_NOT_IN_ZOOM_ONEBYONE)
            {
            //Return to previous mode
            iDrawGrid->InitDrawGrid();
            //aDrawFunction = EGrid;
            SetDrawMode(EGrid);
            iView->SetFaceBrowsingMode(EFaceBrowserNone);
            }
        else
            {
            iDrawOneByOne->SetDrawOneByOneTargetZoom(1);
            }
            
        }
    
    //EFaceBrowser
    else if(aDrawFunction == EFaceBrowser)
        {
        SetDrawMode(EOneByOne);
        //aDrawFunction = EOneByOne;
        iDrawOneByOne->InitDrawOnebyOne(/*KInitDrawZoom*/drawZoom, inPictureX, inPictureY);
        iView->SetFaceBrowsingMode(EFaceBrowserNone);
        }
    }

void CImagicContainerBrowser::SetFullScreen()
    {
    SetExtentToWholeScreen();
    }

void CImagicContainerBrowser::HandleRotation(float& aRotationAngle, float& aTargetRotationAngle)
    {
	// Force target to be in steps of 90
	aTargetRotationAngle=((int)(aTargetRotationAngle/90))*90;
	
	// Force both angles to be between 0-360
	while (aRotationAngle<0)			aRotationAngle+=360;
	while (aRotationAngle>360)			aRotationAngle-=360;
	while (aTargetRotationAngle<0)		aTargetRotationAngle+=360;
	while (aTargetRotationAngle>360)	aTargetRotationAngle-=360;
	
	// Calculate difference between angles
	float diff=aTargetRotationAngle-aRotationAngle;
	// Limit difference to be between [-180:180]
	while (diff<-180)		diff+=360;
	while (diff>180)		diff-=360;
	
	// Copy paste from Interpolate, just uses diff calculated above
	float aStep=0.26;
	float timediff = Min(0.1f, iTimeDiff); // so max value of timediff is 100tick (100ms)
	aRotationAngle += diff * aStep * timediff * 30;
	
	// Calculate new difference
	float newDiff=aTargetRotationAngle-aRotationAngle;
	while (newDiff<-180)	newDiff+=360;
	while (newDiff>180)		newDiff-=360;
	// If difference-angles have different signs, then we went past the target angle
	if (diff*newDiff < 0)
		aRotationAngle = aTargetRotationAngle;
	}

TBool CImagicContainerBrowser::IsOpenGLInit()
    {
    return iOpenGlInitialized;
    }

//Disables display drawing
void CImagicContainerBrowser::EnableDisplayDraw()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::EnableDisplayDraw++"));
    
    if(!iPeriodic->IsActive())
        iPeriodic->Start( 1, iDisplayDrawFreq, TCallBack( CImagicContainerBrowser::DrawCallBackL, this ) );
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::EnableDisplayDraw--"));
    }

//Enables display drawing
void CImagicContainerBrowser::DisableDisplayDraw()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DisableDisplayDraw++"));
    
    if(iPeriodic)
        if(iPeriodic->IsActive())
            {
            iPeriodic->Cancel();
            //Clear buffers
            //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::DisableDisplayDraw--"));
    }

void CImagicContainerBrowser::SetDrawMode(TDrawFunction aDrawFunction)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetDrawMode++"));
    
    ResetZoomKeys();
    ResetDirKeyData();
    
    if(/*aDrawFunction == EOneByOne && */!IS_NOT_IN_ZOOM_ONEBYONE)
        {
        iDrawOneByOne->SetDrawOneByOneTargetZoom(1);
        }
    else
        {
        iDrawFunction = aDrawFunction;
        }
    
    if(aDrawFunction == EGrid)
        {
        iPreferHighResLoading = EFalse;
#ifdef SUPERZOOM
        iIEngine->CancelFullSizeLoading();
        
        //Unload high res textures when going back to Grid
        CImageData* aGridData = iIEngine->GetImageData(iCurrentIndex); 
        iTextureLoader->ReleaseSuperHResTexture( aGridData );
#endif
        iDrawGrid->InitDrawGrid();
        }
    
    DrawNow();
    
    DP0_IMAGIC(_L("CImagicContainerBrowser::SetDrawMode--"));
    }

CImagicContainerBrowser::TDrawFunction CImagicContainerBrowser::GetDrawMode()
    {
    return iDrawFunction;
    }


void CImagicContainerBrowser::SetBGPSStatus(TBool aValue)
    {
    //False = BGPS is running
    //True = BGPS is completed
    iTNCreationComplete = aValue;
    }

CTextureLoader* CImagicContainerBrowser::GetTextureLoader()
    {
    return iTextureLoader;
    }

float CImagicContainerBrowser::GetAspectRatio(TInt aIndex)
    {
    return iIEngine->GetImageData(aIndex)->GetAspectRatio();
    }

void CImagicContainerBrowser::SetFaceCoords(RArray<TRect>& aCoordinates)
    {
    iDrawFaceBrowsing->SetFaceCoords(aCoordinates);
    }

void CImagicContainerBrowser::ClearFaceArray()
    {
    iDrawFaceBrowsing->ClearFaceArray();
    }

void CImagicContainerBrowser::SetTextIndex(GLuint aIndex)
    {
    iCurrentBindedIndex = aIndex;
    }

#ifdef _ACCELEROMETER_SUPPORTED_

TImagicDeviceOrientation CImagicContainerBrowser::GetDeviceOrientation()
    {
    return iDeviceOrientation;    
    }


void CImagicContainerBrowser::PhoneRotated(TImagicDeviceOrientation aDeviceOrientation)
    {
    /*if(iDeviceOrientation == aDeviceOrientation)
        {
        return;
        }*/
    
    iDeviceOrientation = aDeviceOrientation;
    
    //iDrawGridZoom = KZoomOutMaxGrid;//Set initial zooming value when draving Grid
#ifdef ENABLE_GRID_ZOOM
#ifdef ZOOM_WHILE_ROTATING
    if(iDrawFunction == EGrid)
        iDrawGridTargetZoom = KZoomOutMaxGrid;
#endif
#endif
    
    if(iDrawFunction == EOneByOne)
        {
        //Set currect rotation angle immeadetly to target, except for current image index
        TInt num = iIEngine->GetTotalNumOfImages();
        for(TInt i=0; i < num; i++ )
            {
            if(i != iCurrentIndex)
                {
                CImageData* data = iIEngine->GetImageData(i);
                data->iGridData.iRotationAngle = data->iGridData.iTargetRotationAngle;
                }  
            }
        }
    else if(iDrawFunction == EFaceBrowser)
        {
        //Do nothing
        }
    else if(iDrawFunction == EGrid)
        {
        //Do nothing
        }
    
    //DrawScreen();
    DrawNow();
    }

#endif

void CImagicContainerBrowser::IconTexturesLoaded(RArray<GLuint> aIconTextureIndexes)
    {
    for(TInt i=0; i<aIconTextureIndexes.Count(); i++)
        {
        iIconTextureIndexes.Append(aIconTextureIndexes[i]);        
        }
    }

void CImagicContainerBrowser::HandleSend2BackgroundEvent()
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleSend2BackgroundEvent++"));
    
    //Cancel SuperZoom image loading, if it was started
    iIEngine->CancelFullSizeLoading();
    /*
    //Delete OpenGL memory allocations
    TInt num = iIEngine->GetTotalNumOfImages();
    for(TInt i=0; i < num; i++ )
        {
        CImageData* data = iIEngine->GetImageData(i);
        
        if(data->iGridData.iGlHQ512TextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlHQ512TextIndex);
        if(data->iGridData.iGlSuperHQTextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlSuperHQTextIndex);
        
        data->iGridData.iGlHQ512TextIndex = 0;
        data->iGridData.iGlSuperHQTextIndex = 0;
        }
    */
    
    //Delete OpenGL memory allocations
    TInt num = iIEngine->GetTotalNumOfImages();
    for(TInt i=0; i < num; i++ )
        {
        CImageData* data = iIEngine->GetImageData(i);
        
        if(data->iGridData.iGlLQ128TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ128TextIndex );
        if(data->iGridData.iGlLQ32TextIndex)
            glDeleteTextures( 1, &data->iGridData.iGlLQ32TextIndex );
        if(data->iGridData.iGlHQ512TextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlHQ512TextIndex);
        if(data->iGridData.iGlSuperHQTextIndex != 0)
            glDeleteTextures(1, &data->iGridData.iGlSuperHQTextIndex);
        
        data->iGridData.iGlLQ32TextIndex = 0;
        data->iGridData.iGlLQ128TextIndex = 0;
        data->iGridData.iGlHQ512TextIndex = 0;
        data->iGridData.iGlSuperHQTextIndex = 0;
        }
        
        
    DP0_IMAGIC(_L("CImagicContainerBrowser::HandleSend2BackgroundEvent--"));
    
    }


/*
void CImagicContainerBrowser::MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct)
    {
    TBool pressValue=EFalse;
    
    switch(aOperationId)
        {
        case ERemConCoreApiVolumeDown: 
            {
            //do your own stuff
            iKeyData.iZoom-=1;
            iKeyData.iZoomOut=ETrue;
            break;
            }
        case ERemConCoreApiVolumeUp:
            {
            //do your own stuff
            iKeyData.iZoom+=1;
            iKeyData.iZoomIn=ETrue;
            break;
            }
        }

    }
    */


TBool CImagicContainerBrowser::GetSlideByDragValue()
    {
    return iOneByOneSlideByDrag;    
    }

TInt CImagicContainerBrowser::GetCurrentIndex()
    {
    return iCurrentIndex;    
    }

TInt CImagicContainerBrowser::GetPrevIndex()
    {
    return iPreviousIndex;    
    }

void CImagicContainerBrowser::SetCurrentIndex(TInt aIndex)
    {
    CheckIndexLimits(aIndex);
    iCurrentIndex = aIndex;    
    }

void CImagicContainerBrowser::CheckIndexLimits(TInt &aIndex)
    {
    //Check that current index is in grid area
    TInt num = iIEngine->GetTotalNumOfImages();
    
    if(!iJumpOver)
        {
        if(aIndex >= num)
            {
            aIndex = num-1;
            iIntheEndOfGrid=ETrue;
            }
        if(aIndex < 0)
            {
            aIndex = 0;
            iIntheEndOfGrid=ETrue;
            }
        }
    else//if(iJumpOver)
        {
        if (num)
            {
            aIndex %= num;
            if (aIndex < 0)
                aIndex = num + aIndex;
            }
        }
    }

void CImagicContainerBrowser::SetPrevIndex(TInt aIndex)
    {
    iPreviousIndex = aIndex;
    }


CKeyData& CImagicContainerBrowser::GetKeyData()
    {
    return iKeyData;
    }

void CImagicContainerBrowser::SetKeyData(CKeyData aData)
    {
    iKeyData = aData;
    }

void CImagicContainerBrowser::ResetKeyData()
    {
    iKeyData.iRotate=0;
    
    iKeyData.iX=0;
    iKeyData.iY=0;
    }

void CImagicContainerBrowser::ResetDirKeyData()
    {
    iKeyData.iUp=0;
    iKeyData.iDown=0;
    iKeyData.iLeft=0;
    iKeyData.iRight=0;
    }
    
CKeyData& CImagicContainerBrowser::GetTouchData()
    {
    return iTouchMoveData;
    }

void CImagicContainerBrowser::SetTouchData(CKeyData aData)
    {
    iTouchMoveData = aData;
    }

void CImagicContainerBrowser::ResetTouchData()
    {
    iTouchMoveData.iRotate=0;
    iTouchMoveData.iX=0;
    iTouchMoveData.iY=0;
    iTouchMoveData.iZoomInKey=0;
    iTouchMoveData.iZoomOutKey=0;
    iTouchMoveData.iRight = EFalse;
    iTouchMoveData.iLeft = EFalse;
    iTouchMoveData.iUp = EFalse;
    iTouchMoveData.iDown = EFalse;
    }


TInt CImagicContainerBrowser::GetGleMaxRes()
    {
    return iGLMaxRes;
    }

TBool CImagicContainerBrowser::IsUserInputGiven()
    {
    return iUserInputGiven;
    }


TSize CImagicContainerBrowser::GetScreenSize()
    {
    return iScreenSize;
    }


// End of File
