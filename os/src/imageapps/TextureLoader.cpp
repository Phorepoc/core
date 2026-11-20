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
#include "TextureLoader.h"
#include <e32debug.h>
#include <GLES\egl.h>
#include "ImagicContainerBrowser.h"
#include "ImagicViewBrowser.h"
#include <hal.h>

// INCLUDES
#include <coecntrl.h>
#include <aknnotewrappers.h>
#include "ImagicAppUi.h"

/*----------------------------------------------------------------------*/
// Constructor and destructor
//
CTextureLoader::CTextureLoader(CImagicAppUi* aImagicAppUi, CImagicContainerBrowser* aContainer, CImagicViewBrowser* aView, 
                               RCriticalSection* aDrawLock) :
        //CActive(CActive::EPriorityStandard),
        CActive(CActive::EPriorityLow),
        //CActive(CActive::EPriorityStandard),
        //CActive(CActive::EPriorityUserInput),
        //CActive(CActive::EPriorityHigh),
        iImagicAppUi(aImagicAppUi),
        iContainer(aContainer),
        iView(aView),
        iDrawLock(aDrawLock),
        iData(NULL)
        
    {
    DP0_IMAGIC(_L("CTextureLoader::CTextureLoader++"));
    
    // Add loader to active scheduler
    CActiveScheduler::Add(this);
    
    DP0_IMAGIC(_L("CTextureLoader::CTextureLoader--"));
    }

void CTextureLoader::ConstructL()
    {
    DP0_IMAGIC(_L("CTextureLoader::ConstructL++"));
    
    // Create small default bitmap
    iBitmap = new (ELeave) CFbsBitmap();
    iSmileBitmap = new (ELeave) CFbsBitmap();
    iZoomBitmap = new (ELeave) CFbsBitmap();
#ifdef USE_RGBA
    iBitmap->Create(TSize(10, 10), EColor16MU);
#else
    iBitmap->Create(TSize(10, 10), EColor16M);
#endif
    
    // Create bitmap scaler
    iBitmapScaler = CBitmapScaler::NewL();
    iBitmapScaler->UseLowMemoryAlgorithm(ETrue);
    
    iRGB2BGRDone = EFalse;

#ifdef ICONS_ENABLAD
    LoadIcons();
#endif
    
    DP0_IMAGIC(_L("CTextureLoader::ConstructL--"));
    }

CTextureLoader::~CTextureLoader()
    {
    DP0_IMAGIC(_L("CTextureLoader::~CTextureLoader++"));
    
    // Cancel ongoing process
    Cancel();
    
    // Free memory
    delete iBitmapScaler;
    delete iBitmap;
    delete iSmileBitmap;
    delete iZoomBitmap;
    
    iBitmapArray.Close();
    
    DP0_IMAGIC(_L("CTextureLoader::~CTextureLoader--"));
    }

void CTextureLoader::GetPngL(TFileName& afilepath, CFbsBitmap* aBitmap) 
    {
    DP0_IMAGIC(_L("CTextureLoader::GetPngL++"));
    
    CImageDecoder* idecoder = CImageDecoder::FileNewL(CEikonEnv::Static()->FsSession(), 
            afilepath, _L8("image/png")/*, CImageDecoder::EOptionAlwaysThread*/);
    
    TFrameInfo iFrameInfo = idecoder->FrameInfo(0);
    aBitmap->Create(iFrameInfo.iOverallSizeInPixels, EColor16MA );
    
    TRequestStatus aStatus = KRequestPending; 
    
    TRAPD(err1, idecoder->Convert( &aStatus, *aBitmap, 0 ));
    if(err1 == KErrNone)
        User::WaitForRequest( aStatus );   

    delete idecoder;
    
    DP0_IMAGIC(_L("CTextureLoader::GetPngL--"));
    }


void CTextureLoader::LoadIcons()
    {
    TInt err = KErrNone;

    TFileName filename;
    filename = KSmileFileName;
    TRAP(err, GetPngL(filename, iSmileBitmap));
    if(err == KErrNone)
        iBitmapArray.Append(iSmileBitmap);
    
    err = KErrNone;

    filename = KZoomFileName;
    TRAP(err, GetPngL(filename, iZoomBitmap));
    if(err == KErrNone)
        iBitmapArray.Append(iZoomBitmap);
    

    }


/*----------------------------------------------------------------------*/
// Returns loader loading status
//
TBool CTextureLoader::IsActiveAndRunning()
    {
    // Make sure that loader is not active
    if (IsRunning())
        return ETrue;
    else
        return EFalse;
    }
    

/*----------------------------------------------------------------------*/
// Loads picture and stores the OpenGL index into specified variable
//
void CTextureLoader::LoadL(CImageData* aData, TThumbSize aResolution)
    {
    DP1_IMAGIC(_L("CTextureLoader::LoadL++ - resolution: %d"), aResolution);

    // Make sure that loader is not active
    if (IsRunning())
        {
        DP0_IMAGIC(_L("CTextureLoader::LoadL - already running, KErrInUse"));
        User::Leave(KErrInUse);
        }
    // Check that image is not already loaded
    else if (aData->iGridData.iGlSuperHQTextIndex && aResolution == EFullSize)
        {
        DP0_IMAGIC(_L("CTextureLoader::LoadL - EFullRes KErrAlreadyExists"));
        iData = NULL;
        User::Leave(KErrAlreadyExists);
        }
    else if (aData->iGridData.iGlHQ512TextIndex && aResolution == ESize512x512)
        {
        DP0_IMAGIC(_L("CTextureLoader::LoadL - ESize512x512 KErrAlreadyExists"));
        iData = NULL;
        User::Leave(KErrAlreadyExists);
        }
    else if (aData->iGridData.iGlLQ128TextIndex && aResolution == ESize128x128)
        {
        DP0_IMAGIC(_L("CTextureLoader::LoadL -  ESize128x128 KErrAlreadyExists"));
        iData = NULL;
        User::Leave(KErrAlreadyExists);
        }
    else if (aData->iGridData.iGlLQ32TextIndex && aResolution == ESize32x32)
        {
        DP0_IMAGIC(_L("CTextureLoader::LoadL - ESize32x32 KErrAlreadyExists"));
        iData = NULL;
        User::Leave(KErrAlreadyExists);
        }
    
    iContainer->DynamicUnLoading();
    
    // Store image data
    iData = aData;
    iResolution = aResolution;
    iHighQuality = EFalse;
    if(aResolution == EFullSize || aResolution == ESize512x512)
        {
        iHighQuality = ETrue;
        //iContainer->DynamicUnLoading();
        }
    
    // Call engine to load picture
    TRAPD(err, iView->LoadBitmapsToBrowserL(iData, aResolution));
    if (err != KErrNone)
        {
        //if (err == KErrNotFound || err == KErrPathNotFound)
            iData->SetImageReady(aResolution, EFalse);
        iData = NULL;
        User::Leave(err);
        }

    DP0_IMAGIC(_L("CTextureLoader::LoadL--"));
    }

/*----------------------------------------------------------------------*/
// Releases 512x512 and Max resolution textures
//
void CTextureLoader::ReleaseSuperHResTexture(CImageData* aGridData)
    {
    DP0_IMAGIC(_L("CTextureLoader::ReleaseHResTextures++"));
    
    if(aGridData->iGridData.iGlSuperHQTextIndex != 0)
        {
        glDeleteTextures(1, &(aGridData->iGridData.iGlSuperHQTextIndex));
        aGridData->iGridData.iGlSuperHQTextIndex=0;
        DP0_IMAGIC(_L("CTextureLoader::ReleaseHResTextures - glSuperHQTextIndex released"));
        }
    
    DP0_IMAGIC(_L("CTextureLoader::ReleaseHResTextures--"));
    }

/*----------------------------------------------------------------------*/
// Unloads picture from specified index
//
void CTextureLoader::ReleaseHQ512Textures()
    {
    DP0_IMAGIC(_L("CTextureLoader::ReleaseQ512Textures++"));
    
    TInt mem = 0;
    TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CTextureLoader::ReleaseQ512Textures - Free RAM: %d"), mem);
    
    for(TInt i=0; i<iContainer->iIEngine->GetTotalNumOfImages(); i++)
        {
        if(i >= iContainer->GetCurrentIndex() + CImagicContainerBrowser::K512TNImageBuffer || 
           i <= iContainer->GetCurrentIndex() - CImagicContainerBrowser::K512TNImageBuffer)
            {
            CImageData* imageData = iImagicAppUi->GetEngine()->GetImageData(i);
            
            if(imageData->iGridData.iGlSuperHQTextIndex!=0)
                {
                glDeleteTextures(1, &(imageData->iGridData.iGlSuperHQTextIndex));
                imageData->iGridData.iGlSuperHQTextIndex=0;
                }
            
            if(i != iContainer->GetCurrentIndex())
                {
                if (imageData->iGridData.iGlHQ512TextIndex!=0)
                    {
                    glDeleteTextures(1, &(imageData->iGridData.iGlHQ512TextIndex));
                    imageData->iGridData.iGlHQ512TextIndex=0;
                    }
                }
            }
        }
    
    ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CTextureLoader::ReleaseQ512Textures - Free RAM: %d"), mem);
    
    DP0_IMAGIC(_L("CTextureLoader::ReleaseQ512Textures--"));
    }


/*----------------------------------------------------------------------*/
// Unloads picture from specified index
//
void CTextureLoader::UnloadLQ512Tex(CImageData* aData) const
    {
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ512Tex++"));
    
    /*TInt mem = 0;
    TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CTextureLoader::UnloadLQ512Tex - Free RAM: %d"), mem);*/
                
    // Delete the picture
    if (aData->iGridData.iGlHQ512TextIndex!=0)
        {
        glDeleteTextures(1, &(aData->iGridData.iGlHQ512TextIndex));
        aData->iGridData.iGlHQ512TextIndex=0;
        }
    
    /*ret = HAL::Get(HALData::EMemoryRAMFree, mem);
    DP1_IMAGIC(_L("CTextureLoader::UnloadLQ512Tex - Free RAM: %d"), mem);*/
    
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ512Tex--"));
    }

/*----------------------------------------------------------------------*/
// Unloads picture from specified index
//
void CTextureLoader::UnloadLQ128Tex(CImageData* aData) const
    {
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ128Tex++"));
    
    // Delete the picture
    if (aData->iGridData.iGlLQ128TextIndex!=0)
        {
        glDeleteTextures(1, &(aData->iGridData.iGlLQ128TextIndex));
        aData->iGridData.iGlLQ128TextIndex=0;
        }
    
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ128Tex--"));
    }

/*----------------------------------------------------------------------*/
// Unloads picture from specified index
//
void CTextureLoader::UnloadLQ32Tex(CImageData* aData) const
    {
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ32Tex++"));
    
    if (aData->iGridData.iGlLQ32TextIndex!=0)
        {
        glDeleteTextures(1, &(aData->iGridData.iGlLQ32TextIndex));
        aData->iGridData.iGlLQ32TextIndex=0;
        }
    
    DP0_IMAGIC(_L("CTextureLoader::UnloadLQ32Tex--"));
    }


/*----------------------------------------------------------------------*/
// Image is loaded by engine, iScale it for OpenGL if needed
//
void CTextureLoader::ImageLoadedL(TInt aError, CFbsBitmap* aBitmap, TInt aGLMaxRes)
    {
    DP0_IMAGIC(_L("CTextureLoader::ImageLoadedL++"));
    
    if(aError == KErrNotFound || aError == KErrPathNotFound)
        {
        DP0_IMAGIC(_L("CTextureLoader::ImageLoadedL - Error: KErrNotFound/KErrPathNotFound"));
        iData->SetImageReady(iResolution, EFalse);
        iData = NULL;
        return;
        }
    else if(aError == KErrCorrupt)
        {
        DP0_IMAGIC(_L("CTextureLoader::ImageLoadedL - Error: KErrCorrupt"));
        iData->SetImageReady(iResolution, EFalse);
        iData->iGridData.iCorrupted = ETrue;
        iData = NULL;
        return;
        }
    else if(aError == KErrCancel)
        {
        //Image loading is cancelled only for superzoom iage loading
        DP1_IMAGIC(_L("CTextureLoader::ImageLoadedL - Error: %d"), aError);
        //iData->SetImageReady(iResolution, EFalse);
        iData = NULL;
        return;
        }
    else if(aError != KErrNone)
        {
        DP1_IMAGIC(_L("CTextureLoader::ImageLoadedL - Error: %d"), aError);
        iData->SetImageReady(iResolution, EFalse);
        iData = NULL;
        return;
        }
    
    // Set image ready to database
    //iData->SetImageReady(iResolution, ETrue);
    
    iGLMaxRes = aGLMaxRes;
    iImageSize = aBitmap->SizeInPixels();
    
    //Check loaded image real size and check if scaling is needed
    // PHOREPOC PATCH: upload the picture as it is (a non-power-of-two texture: the GL here takes it); the scaled copy came out
    // as a small picture in a corner of a larger bitmap (the whole grid showed at ~30 %). Sizes are logged to see which.
    RDebug::Printf("[imagic] ImageLoadedL %dx%d res %d, scaling would be %d, size when scaled %dx%d", iImageSize.iWidth, iImageSize.iHeight, iResolution, IsScalingNeeded(iImageSize), ScaleDown(iImageSize.iWidth), ScaleDown(iImageSize.iHeight));
    iScalingNeeded = EFalse;
        
    TSize size;
    // Check does image need to be scaled
    if(!iScalingNeeded)
        { //Image is already proper size, just create texture
        CreateThumbnailTexture(aBitmap);
        }
    //Calculate new image size if scaling is needed(in case size is not pow^2)
    else
        {
        size.iWidth = ScaleDown( iImageSize.iWidth );
        size.iHeight = ScaleDown( iImageSize.iHeight );

        // Setup target bitmap to be large enough
        iBitmap->Reset();
#ifdef USE_RGBA
        iBitmap->Create(size, EColor16MU);
#else
        iBitmap->Create(size, EColor16M);
#endif
    
        // Setup image quality
        CBitmapScaler::TQualityAlgorithm quality = CBitmapScaler::EMinimumQuality;
        /*if(iHighQuality)*/
            quality=CBitmapScaler::EMaximumQuality;
    
        iBitmapScaler->SetQualityAlgorithm(quality);
    
        // Start scaling the bitmap, RunL will be called when complete
        iBitmapScaler->Scale(&iStatus, *aBitmap, *iBitmap, EFalse);
        if(!IsActive())
            SetActive();
        }
    
    DP0_IMAGIC(_L("CTextureLoader::ImageLoadedL--"));
    }

/*----------------------------------------------------------------------*/
// Creates OpenGL texture of given bitmap
//
TInt CTextureLoader::CreateTexture(CFbsBitmap* aBitmap, TBool aHighQuality)
    {
    DP0_IMAGIC(_L("CTextureLoader::CreateTexture++"));
    
    // Get image data size
    TInt width = aBitmap->SizeInPixels().iWidth;
    TInt height = aBitmap->SizeInPixels().iHeight;
    TInt dataSize = width * height;
    
    // Lock bitmap before modifying its data
    aBitmap->LockHeap( EFalse );
    
    // The data in the texture are in RGBA order but is read in BGRA order.
    // So we have to swap the 1st and 3rd bytes.
    TUint8* data = (TUint8 *)aBitmap->DataAddress();

#ifdef USE_RGBA
    dataSize*=4;
    for (TInt i=0; i<dataSize; i+=4)
        {
        TUint8 temp = data[i];
        data[i] = data[i+2];
        data[i+2] = temp;
        }
#else
    dataSize*=3;
    //RDebug::Print(_L("CTextureLoader::CreateTexture - Bitmap Data seize: %d"), dataSize);
    for(TInt i=0; i<dataSize; i+=3)
        {
        TUint8 temp = data[i];
        data[i] = data[i+2];
        data[i+2] = temp;
        }
#endif
    
    // Generate OpenGL texture index
    GLuint index;
    glGenTextures(1, &index);
    glBindTexture(GL_TEXTURE_2D, index);
    
//#define MIPMAPPING
#ifdef MIPMAPPING
       //Set mipmapping on
       //glHint( GL_GENERATE_MIPMAP_HINT, GL_NICEST );
       glHint( GL_GENERATE_MIPMAP_HINT, GL_FASTEST );
       glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
       //Select mipmapping filtering mode
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
       //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);//linear gives better image quality but slover rendering
#endif
       
    // Set texture parameters
    //GL_NEAREST is faster than GL_LINEAR but quality is better in linear
    if(!aHighQuality)
        {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    else
        {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    
#ifdef USE_RGBA
    // Load texture into OpenGL memory
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                  0, GL_RGBA, GL_UNSIGNED_BYTE, data );
#else
       glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
#endif
    
    // Unlock bitmap
    aBitmap->UnlockHeap( EFalse );
    
    return index;
    }
    
void CTextureLoader::CreateThumbnailTexture(CFbsBitmap* aBitmap)
    {
    DP0_IMAGIC(_L("CTextureLoader::CreateThumbnailTexture++"));
    
    TInt index = CreateTexture(aBitmap, iHighQuality);
    iContainer->SetTextIndex(index);
    
    //Image has been loaded, store index
    if(iResolution == ESize512x512)
        {
        //Check that we really have high resolution image
        if((aBitmap->SizeInPixels().iHeight == 512 && aBitmap->SizeInPixels().iWidth == 1024) || 
           (aBitmap->SizeInPixels().iHeight == 1024 && aBitmap->SizeInPixels().iWidth == 512) ||
           (aBitmap->SizeInPixels().iHeight == 512 && aBitmap->SizeInPixels().iWidth == 512))
            {
            iData->iGridData.iGlHQ512TextIndex = index;
            }
        else
            {//if no high resolution image, delete existing texture from 128x128 slot and bind newly created texture there
            if(aBitmap->SizeInPixels().iHeight == 128)
                {
                if(iData->iGridData.iGlLQ128TextIndex != 0)
                    glDeleteTextures(1, &(iData->iGridData.iGlLQ128TextIndex));
                iData->iGridData.iGlLQ128TextIndex = index;
                }
            if(aBitmap->SizeInPixels().iHeight == 32)
                {
                if(iData->iGridData.iGlLQ32TextIndex != 0)
                    glDeleteTextures(1, &(iData->iGridData.iGlLQ32TextIndex));
                iData->iGridData.iGlLQ32TextIndex = index;
                }
            }
        iPreviousData = iData;
        }
    else if(iResolution == ESize128x128)
        {
        //Check if we had exif tn already binded and delete it
        if(iData->iGridData.iGlLQ128TextIndex != 0)
            {
            glDeleteTextures(1, &(iData->iGridData.iGlLQ128TextIndex));
            }
        iData->iGridData.iGlLQ128TextIndex = index;
        }
    else if(iResolution == ESize32x32)
        {
        //Check if we had exif tn already binded and delete it
        if(iData->iGridData.iGlLQ32TextIndex != 0)
            {
            glDeleteTextures(1, &(iData->iGridData.iGlLQ32TextIndex));
            }
        iData->iGridData.iGlLQ32TextIndex = index;
        }
#ifdef SUPERZOOM
    else if(iResolution == EFullSize)
        {
        iData->iGridData.iGlSuperHQTextIndex = index;
        }
#endif

    //Order container to draw scaled image
    /*if(iContainer->GetDrawMode() == EOneByOne)
        {
        //Check if loaded image is current one and draw it
        CImageData* imageData = iImagicAppUi->GetEngineL()->GetImageData(iContainer->GetCurrentIndex(), iImagicAppUi->GetUIDrawMode());
        if(imageData->iGridData.iGlSuperHQTextIndex != 0)
            iContainer->DrawNow();
        else if(imageData->iGridData.iGlHQ512TextIndex == iData->iGridData.iGlHQ512TextIndex )
            iContainer->DrawNow();
        else if(imageData->iGridData.iGlLQ128TextIndex == iData->iGridData.iGlLQ128TextIndex )
            iContainer->DrawNow();
        }
    else*/
        {
        iContainer->DrawNow();
        DP0_IMAGIC(_L("<----------- CTextureLoader DrawNow ----------->"));
        }
    
    //Loader is finished
    iData = NULL;
    aBitmap->Reset();
    
    //Set loader back on
    //iContainer->SetLoadingOn(ETrue);
        
    DP0_IMAGIC(_L("CTextureLoader::CreateThumbnailTexture--"));
    }

/*----------------------------------------------------------------------*/
// Active object's RunL
//
void CTextureLoader::RunL()
    {
    ASSERT(iBitmap);
    CreateThumbnailTexture(iBitmap);
    }

/*----------------------------------------------------------------------*/
// Active object's DoCancel
//
void CTextureLoader::DoCancel()
    {
    DP0_IMAGIC(_L("CTextureLoader::DoCancel++"));
    
    // Reset bitmap and cancel scaler
    if (IsActive())
        {
        iBitmap->Reset();
        iBitmapScaler->Cancel();
        }
    
    DP0_IMAGIC(_L("CTextureLoader::DoCancel--"));
    }

/*----------------------------------------------------------------------*/
// Active object's RunError
//
void CTextureLoader::RunError()
    {
    // Nothing here
    }

/*----------------------------------------------------------------------*/
// Scales given value to power of two
//
TInt CTextureLoader::ScaleDown(TInt aSize)
    {
    DP0_IMAGIC(_L("CTextureLoader::ScaleDown++"));
    
    if(iResolution == ESize32x32)
        {
        return 32;
        }
    else if(iResolution == ESize128x128 || iResolution == ESize512x512)
        {
        return 128;
        }
        
    // Find new size for picture
    TInt newSize;
    for (newSize=1; newSize<aSize; newSize*=2)
        ;   // Just do nothing
    
    // Do not iScale up
    if(newSize > aSize)
        newSize/=2;
    
    // Limit size to some maximum value
    TInt maxSize=256;
    TInt minSize = 32;
    
    
    if(iHighQuality)    // High quality pictures are lot bigger
        {
/*#ifdef __WINS__
        maxSize=512;
#endif*/

#ifdef SUPERZOOM
        //maxSize=iGLMaxRes;
        if(iGLMaxRes >= 1024)
            maxSize=1024;
        else
            maxSize=iGLMaxRes;
        
#else
        maxSize=512;
#endif
    
        }
    // Check that size is below maximum and min size
    if(newSize > maxSize)
        newSize=maxSize;
    if(newSize < minSize)
        newSize=minSize;

    DP0_IMAGIC(_L("CTextureLoader::ScaleDown--"));
    return newSize;
    }

/*----------------------------------------------------------------------*/
// Creates OpenGL texture of given bitmap
//
void CTextureLoader::CreateIconTextures()
    {
    DP0_IMAGIC(_L("CTextureLoader::CreateIconTextures++"));
    
    for(TInt i=0; i<iBitmapArray.Count(); i++)
        {
        // Get image data size
        TInt width = iBitmapArray[i]->SizeInPixels().iWidth;
        TInt height = iBitmapArray[i]->SizeInPixels().iHeight;
        TInt dataSize = width * height;
        
        // Lock bitmap before modifying its data
        iBitmapArray[i]->LockHeap( EFalse );
        
        // The data in the texture are in RGBA order but is read in BGRA order.
        // So we have to swap the 1st and 3rd bytes.
        TUint8* data = (TUint8 *)iBitmapArray[i]->DataAddress();
    
        if(!iRGB2BGRDone)
            {
//#ifdef USE_RGBA
            dataSize*=4;
            for (TInt i=0; i<dataSize; i+=4)
                {
                TUint8 temp = data[i];
                data[i] = data[i+2];
                data[i+2] = temp;
                }
/*#else
            dataSize*=3;
            for(TInt i=0; i<dataSize; i+=3)
                {
                TUint8 temp = data[i];
                data[i] = data[i+2];
                data[i+2] = temp;
                }
#endif*/
            }
        
        // Generate OpenGL texture index
        //GLuint index;
        glGenTextures(1, &iSmileTexIndex);
        glBindTexture(GL_TEXTURE_2D, iSmileTexIndex);
        iIconTextureIndexes.Append(iSmileTexIndex);
        //iContainer->SetTextIndex(index);
        
        // Set texture parameters
        //GL_NEAREST is faster than GL_LINEAR but quality is better in linear
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
        
    //#ifdef USE_RGBA
        // Load texture into OpenGL memory
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
    /*#else
           glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    #endif*/
        
        // Unlock bitmap
       iBitmapArray[i]->UnlockHeap( EFalse );
       }
    
    iRGB2BGRDone = ETrue;
    
    iContainer->IconTexturesLoaded(iIconTextureIndexes);
    
    DP0_IMAGIC(_L("CTextureLoader::CreateIconTextures--"));
    }


/*----------------------------------------------------------------------*/
// Check if scaling is needed
//
TBool CTextureLoader::IsScalingNeeded(TSize aImageSize)
    {
//Check loaded image real size and over write high quality flag
    if((aImageSize.iHeight == 1024 && aImageSize.iWidth == 1024) ||
       (aImageSize.iHeight == 512 && aImageSize.iWidth == 1024) || 
       (aImageSize.iHeight == 1024 && aImageSize.iWidth == 512) ||
       (aImageSize.iHeight == 512 && aImageSize.iWidth == 512))
        {
        // Set image ready to database, only if resolution was maching with Imagic TN size
        iData->SetImageReady(ESize512x512, ETrue);
        return EFalse;
        }
    if((aImageSize.iHeight == 128 && aImageSize.iWidth == 128))
        {
        // Set image ready to database, only if resolution was maching with Imagic TN size
        iData->SetImageReady(ESize128x128, ETrue);
        return EFalse;
        }
    if((aImageSize.iHeight == 32 && aImageSize.iWidth == 32))
        {
        // Set image ready to database, only if resolution was maching with Imagic TN size
        iData->SetImageReady(ESize32x32, ETrue);
        return EFalse;
        }
    
    return ETrue;
    }
