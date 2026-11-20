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
// Include files
#include <e32debug.h>
#include <hal.h> 
#include "IEBitmapLoader.h"
#include "IEFileLoader.h"
#include "IEEngineUtils.h"

_LIT8(KMimeMbm, "image/x-epoc-mbm");
_LIT8(KMimeJpeg, "image/jpeg");

#define USE_EXIF_TN
#define USE_EXIF_TN_CROP
//#define USE_EXIF_TN_EXT_CROP

#ifdef USE_EXIF_TN
#include <iclextjpegapi.h>  // For CExtJpegDecoder
#endif

const TInt KMemoryThresholdForSuperZoom = 12*1024;


// ============================ MEMBER FUNCTIONS =========================== \\

CIEBitmapLoader* CIEBitmapLoader::NewL(RFs& aFileServer, MBitmapLoaderObserver& aBitmapLoaderObserver, RCriticalSection* aCritical)
{
 	CIEBitmapLoader* self = new (ELeave) CIEBitmapLoader(aFileServer, aBitmapLoaderObserver, aCritical);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
}

CIEBitmapLoader::~CIEBitmapLoader()
    {
    DP0_IMAGIC(_L("CIEBitmapLoader::~CIEBitmapLoader++"));
  	if(iImageDecoder)
  	    {
  	    iImageDecoder->Cancel();
		delete iImageDecoder;
		iImageDecoder = NULL;	
        }
  	
    if (iExifBitmap)
        {
        delete iExifBitmap;
        iExifBitmap = NULL;
        }
#ifdef DECODE_FROM_BUFFER    
    if(iSourceData) 
        {
        delete iSourceData;
        iSourceData = NULL;
        }
#endif
    DP0_IMAGIC(_L("CIEBitmapLoader::~CIEBitmapLoader--"));
  	}
 
CIEBitmapLoader::CIEBitmapLoader(RFs& aFileServer, MBitmapLoaderObserver& aBitmapLoaderObserver, RCriticalSection* aCritical)
    //:CActive(EPriorityLow),    
    :CActive(EPriorityStandard),
    //:CActive(EPriorityUserInput),
    //:CActive(CActive::EPriorityHigh),
    iFileServer(aFileServer), 
    iBitmapLoaderObserver(aBitmapLoaderObserver),
#ifdef DECODE_FROM_BUFFER    
    iSourceData(NULL), 
#endif        
    iCritical(aCritical)
    {
    }

void CIEBitmapLoader::ConstructL()
    {
	CActiveScheduler::Add(this);
	iExifTn = NULL;
	iImageDecoder = NULL;
	iImageArrayMode = EImages;
	iOutputBitmap = NULL;
	iExifBitmap = NULL;
#ifdef GET_DECODER_UID
	decoderUid = CIEEngineUtils::GetImageDecoderUid();	
#else
	decoderUid = KNullUid;
#endif
    }

void CIEBitmapLoader::CropImageL(CFbsBitmap* aOutput, CFbsBitmap* aInput) const
    {
    const TInt KMaxScanLine = 320 * 3;
    TSize inputSize = aInput->SizeInPixels();
    TSize outputSize = aOutput->SizeInPixels();
    TDisplayMode mode = aOutput->DisplayMode();
    
    // need to have same display mode and output cannot be bigger
    if (mode != aInput->DisplayMode() || 
        outputSize.iWidth > inputSize.iWidth ||
        outputSize.iHeight > inputSize.iHeight)
        User::Leave(KErrNotSupported);

    TInt len = CFbsBitmap::ScanLineLength(outputSize.iWidth, mode);
    TBuf8<KMaxScanLine> buf;
    if (len > KMaxScanLine)
        {
        User::Leave(KErrOverflow);
        }
    
    TPoint point((inputSize.iWidth - outputSize.iWidth + 1) / 2,
                 (inputSize.iHeight - outputSize.iHeight + 1) / 2);
    for (TInt i = 0;i < outputSize.iHeight;i++, point.iY++)
        {
        aInput->GetScanLine(buf, point, outputSize.iWidth, mode);
        aOutput->SetScanLine(buf, i);
        }
    }

void CIEBitmapLoader::RunL()
    {
    DP0_IMAGIC(_L("CIEBitmapLoader::RunL++"));
    
    SetPriority(EPriorityStandard);
    iThumbRes = ENotDefined;
	TInt error = iStatus.Int();
	
	DP1_IMAGIC(_L("CIEBitmapLoader::RunL - Error: %d"),error);
	
	if(iImageDecoder)
	    {
        delete iImageDecoder;
        iImageDecoder = NULL;   
	    }
	
#ifdef DECODE_FROM_BUFFER    
    if(iSourceData) 
        {
        delete iSourceData;
        iSourceData = NULL;
        }
#endif
	
#ifdef USE_EXIF_TN
    if (iExifTn)
        {
        delete iExifTn;
        iExifTn = NULL;
        }   
	
	if(iUseExifTn && iImageData && error == KErrNone) 
	    {
	    iImageData->SetImageReady(EExifThumb, ETrue);
	    
	    // Crop exif thumbnail
	    if (iExifBitmap && iOutputBitmap)
	        {
	        TRAP(error, CropImageL(iOutputBitmap, iExifBitmap));
	        }
	    }

	if (iExifBitmap)
	    {
	    delete iExifBitmap;
	    iExifBitmap = NULL;
	    }
	
    iOutputBitmap = NULL;
#endif
    
	iBitmapLoaderObserver.BitmapsLoadedL(error);
	
	DP0_IMAGIC(_L("CIEBitmapLoader::RunL--"));
    }

void CIEBitmapLoader::DoCancel()
    {
    }

TInt CIEBitmapLoader::RunError(TInt aError)
    {
    DP1_IMAGIC(_L("CIEBitmapLoader::RunError - Error: %d"),aError);
    
    return KErrNone;
    }

void CIEBitmapLoader::GetOneBitmapL(CImageData* aImageData, CFbsBitmap* aBitmap, TThumbSize aThumbRes)
    {
    DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL++"));
    DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d"), aThumbRes);

    ASSERT(iImageDecoder == NULL);    
    
    iThumbRes = aThumbRes;
    TBool thumbnailExists = ETrue;
    //TBool isFileJpg = ETrue;
    iUseExifTn = EFalse;
    iImageData = aImageData;
    iOutputBitmap = aBitmap;
    
#ifdef USE_EXIF_TN    
    CExtJpegDecoder* extImageDecoder = NULL;
#endif    
    
    TFileName fileName;
    if(aThumbRes == EFullSize)
        {
        if(iImageData->IsImageReady(EFullSize))
            {
            //SetPriority(EPriorityUserInput);
            iImageData->GetFileName(fileName, EFullSize);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
        else
            {
            //If TN is not ready we just return
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d, not found"), aThumbRes);
            thumbnailExists = EFalse;
            //iBitmapLoaderObserver.BitmapsLoadedL(KErrNotFound);
            User::Leave(KErrNotFound);
            return;
            }
        }
    else if(aThumbRes == ESize128x128)
        {
#ifdef USE_64X64_BITMAP_TN
        //isFileJpg = EFalse;
        iImageData->GetFileName(fileName, ESize128x128);
        DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
        TInt error = aBitmap->Load(fileName);
        if (error == KErrNone)
            {
            SetPriority(EPriorityUserInput);
            SetActive();
            TRequestStatus* status = &iStatus;
            User::RequestComplete(status, KErrNone);
            return;
            }
        
#ifdef USE_EXIF_TN
        else if(iImageData->IsImageReady(EFullSize))
            {
            iUseExifTn = ETrue;
            iImageData->GetFileName(fileName, EFullSize);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
#endif
        else
            {
            //If TN is not ready we just return
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d, not found"), aThumbRes);
            thumbnailExists = EFalse;     
            iImageData->SetImageReady(EExifThumb, EFalse);
            User::Leave(KErrNotFound);
            return;
            }
#else
        if(iImageData->IsImageReady(ESize128x128))
            {
            SetPriority(EPriorityUserInput);
            //isFileJpg = EFalse;
            iImageData->GetFileName(fileName, ESize128x128);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
#ifdef USE_EXIF_TN
        //else if(iImageData->IsImageReady((TThumbSize)(EFullSize|EExifThumb)))
        else if(iImageData->IsImageReady(EExifThumb))
            {
            SetPriority(EPriorityUserInput);
            iUseExifTn = ETrue;
            iImageData->GetFileName(fileName, EFullSize);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
#endif
        else
            {
            //If TN is not ready we just return
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d, not found"), aThumbRes);
            
            thumbnailExists = EFalse;
            iImageData->SetImageReady(EExifThumb, EFalse);
            //iBitmapLoaderObserver.BitmapsLoadedL(KErrNotFound);
            User::Leave(KErrNotFound);
            return;
            }
#endif
        }
    
    else if(aThumbRes == ESize512x512)
        {
        if(iImageData->IsImageReady(ESize512x512))
            {
            iImageData->GetFileName(fileName, ESize512x512);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
        else
            {
            //If TN is not ready we just return
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d, not found"), aThumbRes);
            thumbnailExists = EFalse;
            iImageData->SetImageReady(EExifThumb, EFalse);
            User::Leave(KErrNotFound);
            return;
            }
        }
    else if(aThumbRes == ESize32x32)
        {
        if(iImageData->IsImageReady(ESize32x32))
            {
            //isFileJpg = EFalse;
            iImageData->GetFileName(fileName, ESize32x32);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            TInt error = aBitmap->Load(fileName);
            if (error == KErrNone)
                {
                SetPriority(EPriorityUserInput);
                SetActive();
                TRequestStatus* status = &iStatus;
                User::RequestComplete(status, KErrNone);
                return;
                }
            }
#ifdef USE_EXIF_TN
        else if(iImageData->IsImageReady(EFullSize))
            {
            iUseExifTn = ETrue;
            iImageData->GetFileName(fileName, EFullSize);
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - filename: %S"), &fileName);
            }
#endif
        else
            {
            //If TN is not ready we just return
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - TN res: %d, not found"), aThumbRes);
            thumbnailExists = EFalse;     
            iImageData->SetImageReady(EExifThumb, EFalse);
            User::Leave(KErrNotFound);
            return;
            }
        }
            
    if(thumbnailExists)
        {
        TInt error = KErrNone;
        
#ifdef USE_EXIF_TN
        if (iUseExifTn)
            {
            ASSERT(iExifTn == NULL);
            TRAP(error, iExifTn = CIEEngineUtils::ReadExifThumbnailL(iFileServer, fileName));
        
            DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - create decoder"));
            if (error == KErrNone)
                {
                DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL data size: %d"), iExifTn->Length());
                TRAP(error, extImageDecoder = CExtJpegDecoder::DataNewL(iFileServer, *iExifTn, /*KMimeJpeg,*/ CImageDecoder::EOptionNone));
                iImageDecoder = extImageDecoder;
                }
            else
                {
                DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - no exif thumb"));
                iImageData->SetImageReady(EExifThumb, EFalse);
                }

            DP2_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - Image exif thumb decoder - TN res: %d, error: %d"), aThumbRes, error);
            }
        else
#endif
            {
            DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - create decoder"));
            /*CExtJpegDecoder * extImageDecoder;
            TRAP(error, iImageDecoder = extImageDecoder = CExtJpegDecoder::FileNewL(
                    CExtJpegDecoder::EHwImplementation,
                    iFileServer, 
                    fileName,
                    CImageDecoder::EPreferFastDecode));*/
#ifdef DECODE_FROM_BUFFER    
            if(iSourceData) 
                {
                delete iSourceData;
                iSourceData = NULL;
                }
            
            RFile jpgFile;
            TInt fileSize;
            
            User::LeaveIfError(jpgFile.Open(iFileServer, fileName, EFileRead));
            jpgFile.Size(fileSize);
            iSourceData = HBufC8::NewL(fileSize);
            
            TPtr8 buffer = iSourceData->Des();
            
            jpgFile.Read(buffer, fileSize);
            
            jpgFile.Close();
            
            iImageDecoder = CImageDecoder::DataNewL(
                    iFileServer, 
                    *iSourceData, 
                    //CImageDecoder::EOptionNone,
                    CImageDecoder::TOptions(CImageDecoder::EPreferFastDecode|CImageDecoder::EOptionAlwaysThread),
                    KNullUid, 
                    KNullUid, 
                    decoderUid);
#else
            TRAP(error, iImageDecoder = CImageDecoder::FileNewL(
                    iFileServer, 
                    fileName,
                    //CImageDecoder::EPreferFastDecode,
                    CImageDecoder::TOptions(CImageDecoder::EPreferFastDecode|CImageDecoder::EOptionAlwaysThread),
                    KNullUid,
                    KNullUid,
                    decoderUid));
#endif
            }

        if(error != KErrNone)
            {
            DP2_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - Image Decoder - TN res: %d, error: %d"), aThumbRes, error);
            delete iImageDecoder;
            iImageDecoder = NULL; 

#ifdef USE_EXIF_TN
            if (iExifTn)
                {
                delete iExifTn;
                iExifTn = NULL;
                }
#endif
//            iBitmapLoaderObserver.BitmapsLoadedL(error);
            User::Leave(error);
            return;
            }
        
        TFrameInfo frameInfo = iImageDecoder->FrameInfo();
        if(aThumbRes == ESize128x128)
            iImageDecoder->SetDecoderThreadPriority(/*EPriorityNormal*/EPriorityMuchMore);
        else
            iImageDecoder->SetDecoderThreadPriority(EPriorityLess);
        
        
#ifdef _IMAGIC_DEBUG
        TInt mem = 0;
        TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
#endif
        DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - #1"));        
        TSize frameSize = frameInfo.iFrameCoordsInPixels.Size();
        //If we are loading full resolution image, we have to create smaller target bitmap 
        //to save memory and to improve speed of further image processing
        if(aThumbRes == EFullSize)
            {
            TInt mem = 0;
            TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
            if(mem < KMemoryThresholdForSuperZoom*1024)
                {
                //User::Leave(KErrNoMemory);
#ifdef USE_OOM            	
                ROomMonitorSession oomMonitor;
                oomMonitor.Connect();
                TInt errorCode = oomMonitor.RequestFreeMemory( 1024*KMemoryThresholdForSuperZoom );
                
                if ( errorCode != KErrNone )
                    {
                    // try one more time 
                    errorCode = oomMonitor.RequestFreeMemory( 1024*KMemoryThresholdForSuperZoom );
                    }
                oomMonitor.Close();
#endif                
                }
                
            
            //TSize tgtSize(1024, 1024);
            //DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - #3"));
            //TargetDecodingSize(tgtSize, frameSizue);
            if(frameSize.iHeight >= 1024 || frameSize.iWidth >= 1024)
                {
                frameSize.iHeight = Min(1024, iBitmapLoaderObserver.GetGleMaxRes());
                frameSize.iWidth = Min(1024, iBitmapLoaderObserver.GetGleMaxRes());
                }
            else
                {
                frameSize.iHeight=512;
                frameSize.iWidth=512;
                }
            }   
        
        // Crop thumbnail if aspect ratio is not same as full size image
#ifdef USE_EXIF_TN
#ifdef USE_EXIF_TN_CROP
        if (iUseExifTn)
            {
#ifdef USE_EXIF_TN_EXT_CROP              
            TInt cap;
            TRAP(error, cap = extImageDecoder->CapabilitiesL());
            if (error == KErrNone && cap & CExtJpegDecoder::ECapCropping)
#endif                
                {
                TReal aspectRatio = aImageData->GetAspectRatio();
                TReal thumbAspectRatio = TReal(frameSize.iWidth) / frameSize.iHeight; 
            
                // calculate final frame size
                //if (frameSize.iWidth / frameSize.iHeight != size.iWidth / size.iHeight)
                if (thumbAspectRatio != aspectRatio)
                    {
                    TRect finalFrameRect;
                    finalFrameRect.SetSize(frameSize);
                    if (thumbAspectRatio < aspectRatio)
                        {
                        TInt h = frameSize.iWidth / aspectRatio;
                        h += h & 1;
                        finalFrameRect.SetHeight(h);
                        }
                    else
                        {
                        TInt w = frameSize.iHeight * aspectRatio;
                        w += w & 1;
                        finalFrameRect.SetWidth(w);
                        }
                    
                    finalFrameRect.Move(
                            (frameSize.iWidth - finalFrameRect.Width() + 1) / 2,
                            (frameSize.iHeight - finalFrameRect.Height() + 1) / 2);
#ifdef USE_EXIF_TN_EXT_CROP                    
                    TRAP(error, extImageDecoder->SetCroppingL(finalFrameRect));
#else         
                    // Create temporal bitmap
                    iExifBitmap = new CFbsBitmap;
                    if (iExifBitmap == NULL)
                        error = KErrGeneral;
                    else
                        error = iExifBitmap->Create(frameSize, frameInfo.iFrameDisplayMode);
                    aBitmap = iExifBitmap;                 
#endif
                    // Reduce final image size
                    if (error == KErrNone)
                        {
                        frameSize.iWidth = finalFrameRect.Width();
                        frameSize.iHeight = finalFrameRect.Height();
                        }                    
                    }
                }
            }
#endif        
#endif        
        
        if (error == KErrNone)
            {
#ifdef USE_RGBA
            error = iOutputBitmap->Create(frameSize, EColor16MU);
#else
            error = iOutputBitmap->Create(frameSize, frameInfo.iFrameDisplayMode);
#endif
            }
        
        if (error == KErrNone)
            {
#ifdef USE_EXIF_TN
            /*if(iUseExifTn)
                {
                TRAP(error, iExifDecoder->Convert(&iStatus, *aBitmap));
                }
            else*/
#endif
                {
                RDebug::Printf("[imagic-bl] decode TN res %d frame %dx%d into bitmap %dx%d (output %dx%d)", aThumbRes, frameInfo.iOverallSizeInPixels.iWidth, frameInfo.iOverallSizeInPixels.iHeight, aBitmap->SizeInPixels().iWidth, aBitmap->SizeInPixels().iHeight, iOutputBitmap->SizeInPixels().iWidth, iOutputBitmap->SizeInPixels().iHeight);
                TRAP(error, iImageDecoder->Convert(&iStatus, *aBitmap));
                }
            }
        
        if(error != KErrNone)
            {
            DP2_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - Image Decoder convert - TN res: %d, error: %d"), aThumbRes, error);
            delete iImageDecoder;
            iImageDecoder = NULL; 
            //aImageData->iGridData.iCorrupted = ETrue;//mika. added 03.06. maybe not good idea????????
            User::Leave(error);
            return;
            }
        else
            {
            DP1_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL - Decoding started TN res: %d"), aThumbRes);
            
            if(!IsActive())
                SetActive();
            }
        }
    
    DP0_IMAGIC(_L("CIEBitmapLoader::GetOneBitmapL--"));
    }

/**
Opens file and creates file pointer
@param aFileName The specified file to open
@return imageInMemoryPtr A Pointer to iImageInMemory
@leave System-wide error codes
*/  
/*TPtr8 CIEBitmapLoader::LoadImageIntoMemoryLC(const TDesC& aFileName)
    {
    RFile file;
    TInt fileSize = 0;

    // Open the file for decoding
    User::LeaveIfError(file.Open(iFileServer, aFileName, EFileRead));
    file.Size(fileSize);    

    //HBufC8* imageInMemory = HBufC8::NewMaxLC(fileSize);
    HBufC8* imageInMemory = HBufC8::NewMaxL(fileSize);
    TPtr8 imageInMemoryPtr = imageInMemory->Des();
    if(file.SubSessionHandle())
        {
        User::LeaveIfError(file.Read(imageInMemoryPtr));    
        }
        
    file.Close();

    return imageInMemoryPtr;
    }*/

void CIEBitmapLoader::TargetDecodingSize(TSize aTgtSize, TSize& aSrcSize)
    {
    DP0_IMAGIC(_L("CIEBitmapLoader::TargetDecodingSize++"));
    
    DP2_IMAGIC(_L("CIEBitmapLoader::TargetDecodingSize - Tgt size.iHeigth: %d, Tgt size.iWidth: %d"),aTgtSize.iHeight, aTgtSize.iWidth);
    DP2_IMAGIC(_L("CIEBitmapLoader::TargetDecodingSize - Src size.iHeigth: %d, Src size.iWidth: %d"),aSrcSize.iHeight, aSrcSize.iWidth);
    
    // up to 32 times downscale in scaler
    for (TInt i = 0;i < 5;i++)
        {
        if (aSrcSize.iWidth < aTgtSize.iWidth * 2 ||
            aSrcSize.iHeight < aTgtSize.iHeight * 2) 
            {
            break;
            }
            
            aSrcSize.iWidth >>= 1;
            aSrcSize.iHeight >>= 1;
        }
    
    // Check that we do not create odd resolution size thumbnail
    if(aSrcSize.iHeight & 1)
        aSrcSize.iHeight++;
    if(aSrcSize.iWidth & 1)
        aSrcSize.iWidth++;
    
    DP2_IMAGIC(_L("CIEBitmapLoader::TargetDecodingSize - Src size.iHeigth: %d, Src size.iWidth: %d"),aSrcSize.iHeight, aSrcSize.iWidth);
     
    DP0_IMAGIC((_L("CIEBitmapLoader::TargetDecodingSize--")));
    }


void CIEBitmapLoader::CancelFullSizeLoading()
    {
    //Cancel only if full resolution loading was on
    DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading++"));
    if(iThumbRes == EFullSize)
        {
        DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading 1"));        
        if(iImageDecoder)
            {
            iImageDecoder->Cancel();
            delete iImageDecoder;
            iImageDecoder = NULL;   
            }
        DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading 2"));
        if(IsActive())
            {
            Cancel();
            DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading 21"));
            iBitmapLoaderObserver.BitmapsLoadedL(KErrCancel);
            }
#ifdef DECODE_FROM_BUFFER    
        if(iSourceData) 
            {
            DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading 3"));
            delete iSourceData;
            iSourceData = NULL;
            }
#endif
        DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading 4"));
        }
        DP0_IMAGIC(_L("CIEBitmapLoader::CancelFullSizeLoading--"));
    }

void CIEBitmapLoader::SetImageDataMode(TImageArrayMode aMode)
    {
    iImageArrayMode = aMode;
    }


// EOF
