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

#include <e32debug.h>
#include <ImageConversion.h> //CImageDecoder
#include <FBS.H> //CFbsBitmap
#include <BAUTILS.H>
//#include <IEBGPSTrace.h>

#include "IETNGenerator.h"
#include "IEBgpServerSession.h" 
#include <hal.h> 
#include "ImagicConsts.h"
#include "ieengineutils.h" 

_LIT8(KMimeJpeg, "image/jpeg");
_LIT8(KMimePng, "image/png");

// PHOREPOC PATCH: the app relied on a device JPEG decoder that scales into any destination bitmap; the stock ICL JPEG decoder only reduces
// by 1/2, 1/4, 1/8 into the top-left of a larger bitmap, so the decode is scaled by CBitmapScaler as the app does without this macro.
//#define DO_NOT_USE_SCALING



CIETNGeneratorAO* CIETNGeneratorAO::NewL(RFs& aFileServer, MIEThumbNailObserver &aObserver)
{
	CIETNGeneratorAO* self = new (ELeave) CIETNGeneratorAO(aFileServer, aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
}

CIETNGeneratorAO::~CIETNGeneratorAO()
{
    DP0_IMAGIC((_L("CIETNGeneratorAO::~CIETNGeneratorAO ++")));
 	if(IsActive())
		Cancel();
 	
	 CancelOutStaningRequests();
	 DeleteObjects();
	 
	 DP0_IMAGIC((_L("CIETNGeneratorAO::~CIETNGeneratorAO --")));
}

//EPriorityIdle, EPriorityLow, EPriorityStandard, EPriorityUserInput, EPriorityHigh
CIETNGeneratorAO::CIETNGeneratorAO(RFs& aFileServer, MIEThumbNailObserver &aObserver)
: iFileServer(aFileServer),
  iThumbnailObserver(aObserver),
  CActive(EPriorityStandard)
{
	
}

void CIETNGeneratorAO::ConstructL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::ConstructL ++")));

	CActiveScheduler::Add(this);
	iError = KErrNone;
	decoderUid = CIEEngineUtils::GetImageDecoderUid();
	//i512x512TnBitmap = new (ELeave) CFbsBitmap();
	
	DP0_IMAGIC((_L("CIETNGeneratorAO::ConstructL --")));
    }

void CIETNGeneratorAO::RunL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::RunL ++")));
    iError = iStatus.Int();
    RDebug::Printf("[imagic-tn] RunL status %d state %d for %S", iStatus.Int(), (TInt)iConvertStatus, &iSourceFileName);
	
#ifdef _IMAGIC_DEBUG
    TInt mem = 0;
	TInt ret = HAL::Get(HALData::EMemoryRAMFree, mem);
	DP1_IMAGIC(_L("CIETNGeneratorAO::RunL - mem: %d"),mem);
#endif
	
	if(iError != KErrNone)
	    {
	    DeleteObjects();
        
        iConvertStatus = ENone;
        
        // inform to higher layer... and delete thumbnails we tried to create
        DP1_IMAGIC(_L("CIETNGeneratorAO::RunL - Something wrong %d"), iError);      
        iThumbnailObserver.ThumbNailGenerationCompleted(iError);
	    }
	
	switch (iConvertStatus)
	    {
	    case EDecoding:
	        {
	        DP0_IMAGIC((_L("CIETNGeneratorAO::RunL - EDecoding")));
	        TRAPD(err, DecodeL()); // TODO handle errors
	        if (err != KErrNone) 
	            {
	            DP1_IMAGIC((_L("CIETNGeneratorAO::RunL - EDecoding error: %d")), err);
	            iThumbnailObserver.ThumbNailGenerationCompleted(err); //ASSERT(0);
	            iConvertStatus = ENone;
	            }
	        else
	            {
#ifdef DO_NOT_USE_SCALING
	            iConvertStatus = EEncoding;
#else
	            iConvertStatus = EScaling;
#endif
                SetActive();
	            }
	        }
	        break;
		
	    case EScaling:
	        {
	        DP0_IMAGIC((_L("CIETNGeneratorAO::RunL - EScaling")));
	        TRAPD(err, ScaleL());
            if (err != KErrNone) 
                {
                DP1_IMAGIC((_L("CIETNGeneratorAO::RunL - EScaling error: %d")), err);
                iThumbnailObserver.ThumbNailGenerationCompleted(err); //ASSERT(0);
                iConvertStatus = ENone;
                }
            else
                {
                iConvertStatus = EEncoding;
                SetActive();
                }
	        }
	        break;
        	
	    case EEncoding:
	        {
	        DP0_IMAGIC((_L("CIETNGeneratorAO::RunL - EEncoding")));
	        TRAPD(err, EncodeL()); // TODO handle errors
	        if (err != KErrNone)
	            {
	            DP1_IMAGIC((_L("CIETNGeneratorAO::RunL - EEncoding error: %d")), err);
                iThumbnailObserver.ThumbNailGenerationCompleted(err); //ASSERT(0);
                iConvertStatus = ENone;
	            }
	        else
	            {
	            iConvertStatus = EReady;

#ifdef USE_BITMAPS_TNS
	            if(!IsJPEG(iThumbnailSize))
	                {
	                //When saving TN as bitmap we need to complete request immeadtely 
	                //because bitmap saving is handled by CFbsBitmap class instead of image encoder
	                SetActive();
	                TRequestStatus* status = &iStatus;
	                User::RequestComplete( status, KErrNone );
	                }
	            else
	                {
	                SetActive();
	                }
#else
	            SetActive();        
#endif
	            }
	        }
	        break;
        
	    case EReady:
	        {
	        DP0_IMAGIC((_L("CIETNGeneratorAO::RunL - EReady")));
   		
	        // use same image for generating smaller thumbnails
	        if (IsLargeThumbnail(iThumbnailSize))
	            {
                /*DP0_IMAGIC((_L("CIETNGeneratorAO::RunL - EScaling")));
                TRAPD(err, ScaleL());
                if (err != KErrNone)
                    ASSERT(0);// TODO handle errors   
                iConvertStatus = EEncoding;
                SetActive();*/ 
   		    }
   		
	        if( iImageEncoder )
	            {
	            delete iImageEncoder;
	            iImageEncoder = NULL;
	            }
        
	        if( iBitmap )
	            {
	            iBitmap->Reset();
	            delete iBitmap;
	            iBitmap = NULL;
	            }
	        
#ifdef DECODE_FROM_BUFFER    
	        if(iSourceData) 
	            {
	            delete iSourceData;
	            iSourceData = NULL;
	            }
#endif
	        
	        iConvertStatus = ENone;
        
#ifndef USE_BITMAPS_TNS
	        WriteExifData(iThumbnailFileName, iResolutionSize);
#else
	        if(IsJPEG(iThumbnailSize))
	            {
	            TRAPD(err, WriteExifDataL(iThumbnailFileName, iThumbnailSize));
	            }
#endif
            
            iFileServer.SetModified(iThumbnailFileName, iSourceTime);
            iThumbnailObserver.ThumbNailGenerationCompleted(KErrNone);
            }
	        break;
        }
	
    DP0_IMAGIC(_L("CIETNGeneratorAO::RunL --"));
    }

TBool CIETNGeneratorAO::IsJPEG(const TSize& aResolution) const
    {
#ifdef USE_64X64_BITMAP_TN
    return (aResolution.iWidth >= 512 || aResolution.iHeight >= 512);
#else
    return (aResolution.iWidth >= 128 || aResolution.iHeight >= 128);    
#endif
    }

TBool CIETNGeneratorAO::IsLargeThumbnail(const TSize& aResolution) const
    {
    return (aResolution.iHeight >= 512);
    }

void CIETNGeneratorAO::ScaleL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::Scale++")));
   
    if ( iImageDecoder )
        {
        delete iImageDecoder;
        iImageDecoder = NULL;
        }
    
    if (iBitmapScaler )
        {
        delete iImageDecoder;
        iImageDecoder = NULL;
        }
   
    iBitmapScaler = CBitmapScaler::NewL();
    iBitmapScaler->UseLowMemoryAlgorithm(ETrue);
   
    TSize tgtBitmapize;
   
    // If we want to create high resolution thumbnail, we use 512x512 resolution
    // for low resolution TN we use size set in iResolutionSize
    if(IsLargeThumbnail(iThumbnailSize))
        {
        tgtBitmapize.iWidth = 512;
        tgtBitmapize.iHeight = 512;
       
        if(iSourceSize.iWidth > iSourceSize.iHeight)
            {
            // If we have wide panorama, use higher resolution to width
            if((iSourceSize.iWidth / iSourceSize.iHeight) > 1.6)
                tgtBitmapize.iWidth = 1024;
            }
        else
            {
            // If we have wide panorama, use higher resolution to height
            if((iSourceSize.iHeight / iSourceSize.iWidth) > 1.6)
                tgtBitmapize.iHeight = 1024;
            }
        }
    else
        {
        tgtBitmapize = iThumbnailSize;
        }
   
    // Use max quality
    iBitmapScaler->SetQualityAlgorithm(CBitmapScaler::EMaximumQuality);
    //iBitmapScaler->SetQualityAlgorithm(CBitmapScaler::EMediumQuality);
    //iBitmapScaler->SetQualityAlgorithm(CBitmapScaler::EMinimumQuality);
   
    DP2_IMAGIC(_L("CIETNGeneratorAO::Scale to %dx%d"), tgtBitmapize.iWidth, tgtBitmapize.iHeight);
      
    RDebug::Printf("[imagic-tn] scale bitmap %dx%d to %dx%d", iBitmap->SizeInPixels().iWidth, iBitmap->SizeInPixels().iHeight, tgtBitmapize.iWidth, tgtBitmapize.iHeight);
    iBitmapScaler->Scale(&iStatus, *iBitmap, tgtBitmapize, EFalse);
       
    DP0_IMAGIC((_L("CIETNGeneratorAO::Scale--")));
    }

void CIETNGeneratorAO::EncodeL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::Encode++")));
   
    if (iImageDecoder )
        {
        delete iImageDecoder;
        iImageDecoder = NULL;
        }
    if( iBitmapScaler )
        {
        delete iBitmapScaler;
        iBitmapScaler = NULL;
        }
    if( iImageEncoder )
        {
        delete iImageEncoder;
        iImageEncoder = NULL;
        }

    TParse parser;
    parser.Set(iThumbnailFileName, NULL, NULL );
    TFileName tnPath = parser.DriveAndPath();
    CIEEngineUtils::CreateTNFolder(iFileServer, tnPath);
      
#ifdef USE_BITMAPS_TNS
    // Save Bitmap thumbnails if resolution is 128x128 or smaller
    if(!IsJPEG(iThumbnailSize))
        {
        iBitmap->Save(iThumbnailFileName);
        }
    else //if bigger than then save jpg thumbnails
#endif
        {
#if 0
        //If we had 512x512 resolution bitmap we make copy of it
        if(iBitmap->SizeInPixels().iWidth == 512 && iBitmap->SizeInPixels().iHeight == 512)
            {
            i512x512TnBitmap->Reset();
            TInt error = i512x512TnBitmap->Create(iBitmap->SizeInPixels(), EColor16M);
            
            TUint8* dstData = (TUint8 *)i512x512TnBitmap->DataAddress();
            TUint8* srcData = (TUint8 *)iBitmap->DataAddress();
            TInt dataSize = iBitmap->SizeInPixels().iWidth * iBitmap->SizeInPixels().iHeight;
            dataSize*=3;
            
            //Copy bitmap
            for(TInt i=0; i<dataSize; i++)
                {
                dstData[i] = srcData[i];
                }
            }
#endif       
        
        SetJpegImageDataL();
        DP1_IMAGIC(_L("CIETNGeneratorAO::Encode - iThumbnailFileName: %S"), &iThumbnailFileName);
        iMimeString = KMimeJpeg;
        iImageEncoder = CImageEncoder::FileNewL( iFileServer, iThumbnailFileName, iMimeString );
        iImageEncoder->Convert(&iStatus, *iBitmap, iFrameImageData);
        }
   
    TEntry entry;
    User::LeaveIfError(iFileServer.Entry(iThumbnailFileName, entry));
    // Remember the modified time of the source file
    iSourceTime = entry.iModified;
   
    DP0_IMAGIC((_L("CIETNGeneratorAO::Encode--")));
    }

void CIETNGeneratorAO::DecodeL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::Decode++")));
    
    if(iImageDecoder)
        {
        delete iImageDecoder;
        iImageDecoder = NULL;   
        }
    
    DP1_IMAGIC((_L("CIETNGeneratorAO::Decode - Creating decoder... with filename: %S")),&iSourceFileName);
#ifdef USE_EXT_JPEG_DEC
    iImageDecoder = CExtJpegDecoder::FileNewL(iFileServer, iSourceFileName);
#else
    
#ifdef DECODE_FROM_BUFFER    
    if(iSourceData) {
        delete iSourceData;
        iSourceData = NULL;
    }
    
    RFile jpgFile;
    TInt fileSize;
    
    User::LeaveIfError(jpgFile.Open(iFileServer, iSourceFileName, EFileRead));
    jpgFile.Size(fileSize);
    iSourceData = HBufC8::NewL(fileSize);
    
    TPtr8 buffer = iSourceData->Des();
    
    jpgFile.Read(buffer, fileSize);
    
    jpgFile.Close();
    
    iImageDecoder = CImageDecoder::DataNewL(
            iFileServer, 
            *iSourceData, 
            CImageDecoder::EOptionNone, 
            KNullUid, 
            KNullUid, 
            decoderUid);
    
    
#else    
    iImageDecoder = CImageDecoder::FileNewL(
                                        iFileServer, 
                                        iSourceFileName, 
                                        CImageDecoder::EOptionNone, 
                                        KNullUid, 
                                        KNullUid, 
                                        decoderUid);
#endif

#endif
    
    
    iMimeString = KMimeJpeg;
#ifdef DECODE_FROM_BUFFER
    CImageDecoder::GetMimeTypeDataL( *iSourceData, iMimeString );    
#else
    CImageDecoder::GetMimeTypeFileL( iFileServer, iSourceFileName, iMimeString );
#endif
    
    iSourceSize = iImageDecoder->FrameInfo().iOverallSizeInPixels;
    
    DP2_IMAGIC((_L("CIETNGeneratorAO::Decode - image %dx%d")), iSourceSize.iWidth, iSourceSize.iHeight );
        
    iBitmap = new (ELeave) CFbsBitmap();
    
    TargetDecodingSize(iThumbnailSize, iSourceSize);
   
#ifdef DO_NOT_USE_SCALING
    if(IsLargeThumbnail(iThumbnailSize))
        {
        iSourceSize.iHeight=512;
        iSourceSize.iWidth=512;
        }
#endif
  
    TInt error = iBitmap->Create(iSourceSize, EColor16M);
    
    // PHOREPOC PATCH (diagnostic)
    RDebug::Printf("[imagic-tn] decode frame %dx%d into bitmap %dx%d (reduction factor %d), thumbnail %dx%d", iImageDecoder->FrameInfo().iOverallSizeInPixels.iWidth, iImageDecoder->FrameInfo().iOverallSizeInPixels.iHeight, iSourceSize.iWidth, iSourceSize.iHeight, iImageDecoder->ReductionFactor(iImageDecoder->FrameInfo().iOverallSizeInPixels, iSourceSize), iThumbnailSize.iWidth, iThumbnailSize.iHeight);
    iImageDecoder->Convert(&iStatus, *iBitmap, 0);
    
    DP0_IMAGIC((_L("CIETNGeneratorAO::Decode - convert bitmap... OK")));
    DP0_IMAGIC((_L("CIETNGeneratorAO::Decode--")));
    }

void CIETNGeneratorAO::TargetDecodingSize(const TSize aTgtSize, TSize& aSrcSize)
    {
    DP0_IMAGIC(_L("CIETNGeneratorAO::TargetDecodingSize++"));
    
    DP2_IMAGIC(_L("CIETNGeneratorAO::TargetDecodingSize - Tgt %dx%d"),aTgtSize.iHeight, aTgtSize.iWidth);
    DP2_IMAGIC(_L("CIETNGeneratorAO::TargetDecodingSize - Src %dx%d"),aSrcSize.iHeight, aSrcSize.iWidth);
    
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
    
    DP2_IMAGIC(_L("CIETNGeneratorAO::TargetDecodingSize - New Src %dx%d"), aSrcSize.iHeight, aSrcSize.iWidth);
    DP0_IMAGIC((_L("CIETNGeneratorAO::TargetDecodingSize--")));
    }

void CIETNGeneratorAO::WriteExifDataL(const TDes &aFilename, TSize aSize)
     {
     DP0_IMAGIC(_L("CIETNGeneratorAO::WriteExifData++"));
     
     // 1. Read JPEG image from the file to a buffer...
     RFile file;
     User::LeaveIfError( file.Open( iFileServer, aFilename, EFileWrite ) );
     CleanupClosePushL( file );
     TInt size = 0;
     file.Size(size);
     HBufC8* jpegImage = HBufC8::NewL( size );
     CleanupStack::PushL( jpegImage );
     TPtr8 bufferDes( jpegImage->Des() );
     User::LeaveIfError( file.Read( bufferDes ) );
     CleanupStack::Pop( jpegImage );
     CleanupStack::PopAndDestroy();
     CleanupStack::PushL( jpegImage );
     
     file.Close();
     
     // 2. Instantiate Exif modifier in ECreate mode...
     CExifModify* modify = CExifModify::NewL( jpegImage->Des(), CExifModify::ECreate );
     CleanupStack::PushL( modify );
     
     // 3. Insert (Set) at least the mandatory Exif data...
     modify->SetXResolutionL( aSize.iWidth, 1 ); 
     modify->SetYResolutionL( aSize.iHeight, 1 ); 
     modify->SetResolutionUnitL( 2 );
     modify->SetYCbCrPositioningL( 1 );
     modify->SetComponentsConfigurationL( 1, 2, 3, 0 );
     modify->SetColorSpaceL( 1 );
     modify->SetPixelXDimensionL( aSize.iWidth );
     modify->SetPixelYDimensionL( aSize.iHeight );
     
     // 4. Get the new Exif image...
     // If zero length descriptor is given instead of jpeg->Des(), then only the
     // Exif meta data is returned.
     HBufC8* newExif = modify->WriteDataL( jpegImage->Des() );
     TPtr8 tmp = newExif->Des();
     
     User::LeaveIfError( file.Replace( iFileServer, aFilename, EFileWrite ) );
     //Write Exif and jpeg image back to jpeg file
     User::LeaveIfError(file.Write(*newExif));
     
     /* Process the new Exif data */
     delete newExif;
     newExif = NULL;
     
     // 5. Delete the modifier instance...
     CleanupStack::PopAndDestroy( modify );
     CleanupStack::PopAndDestroy( jpegImage );
     
     file.Close();
     
     DP0_IMAGIC(_L("CIETNGeneratorAO::WriteExifData--"));
     }


//Set JPEG image quality for encoding
void CIETNGeneratorAO::SetJpegImageDataL()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetJpegImageDataL++")));
    
    TJpegImageData* jpegImageData;
    jpegImageData = new (ELeave) TJpegImageData;
    jpegImageData->iSampleScheme = TJpegImageData::EColor420;
    jpegImageData->iQualityFactor = 80;
    
    if (iFrameImageData)
        {
        delete iFrameImageData;
        iFrameImageData = NULL;
        }
    
    iFrameImageData = CFrameImageData::NewL();
    
    //Ownership of jpegImageData lies with CFrameImageData
    User::LeaveIfError(iFrameImageData->AppendImageData(jpegImageData));
    
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetJpegImageDataL--")));
    }

void CIETNGeneratorAO::DoCancel()
    {	
    }

TInt CIETNGeneratorAO::RunError(TInt /*aError*/)
    {
	return KErrNone;
    }
 
/* Set Image arrary received from Client.*/ 
void CIETNGeneratorAO::SetImageArray(RArray<CImageData*> aImageArray)
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetImageArray ++")));
    
    iImageArray = aImageArray;
    
    DP0_IMAGIC((_L("CIETNGeneratorAO::SetImageArray --")));
    }
 
void CIETNGeneratorAO::CreateThumbnailL(
        const TDes& aSourceFile, 
        const TDes& aThumbnailFile, 
        const TSize &aSize)
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::CreateThumbnailL ++")));

	iSourceFileName.Copy(aSourceFile);
	iThumbnailFileName.Copy(aThumbnailFile);
	iThumbnailSize = aSize;
 
 	iConvertStatus = EDecoding;
 	if(!IsActive())
 	    {
 	    SetActive();
 	    TRequestStatus* status = &iStatus;
 	    User::RequestComplete( status, KErrNone );
 	    }
 	
 	DP0_IMAGIC((_L("CIETNGeneratorAO::CreateThumbnailL --")));
    }
	

void CIETNGeneratorAO::CreateThumbnailL(
        const TDes& aSourceFile, 
        const TDes& aThumbnailFile, 
        const TSize &aSize,
        CFbsBitmap* /*a512x512TnBitmap*/)
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::CreateThumbnailL ++")));

    iSourceFileName.Copy(aSourceFile);
    iThumbnailFileName.Copy(aThumbnailFile);
    iThumbnailSize = aSize;
 
    iConvertStatus = EDecoding;
    if(!IsActive())
        {
        SetActive();
        TRequestStatus* status = &iStatus;
        User::RequestComplete( status, KErrNone );
        }
    
    DP0_IMAGIC((_L("CIETNGeneratorAO::CreateThumbnailL --")));
    }

void CIETNGeneratorAO::CancelOutStaningRequests()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::CancelOutStaningRequests ++")));
    
    if(iError != KErrNone)
        {
        if(BaflUtils::FileExists(iFileServer, iThumbnailFileName))
            {
            TInt err = BaflUtils::DeleteFile(iFileServer, iThumbnailFileName);
            DP2_IMAGIC(_L("CIETNGeneratorAO::CancelOutStaningRequests - DeleteCorruptedThumbNailFile - file found: %S, err:%d"), &iThumbnailFileName, err);
            }
        }
        
    DeleteObjects();
    
    DP0_IMAGIC((_L("CIETNGeneratorAO::CancelOutStaningRequests --")));
    }
 
void CIETNGeneratorAO::DeleteObjects()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::DeleteObjects ++")));
	
    if(iImageDecoder)
		{
		delete iImageDecoder;
		iImageDecoder = NULL;	
		}

	if( iBitmapScaler )
		{
		delete iBitmapScaler;
		iBitmapScaler = NULL;
		}
	
	if( iBitmap )
		{
		delete iBitmap;
		iBitmap = NULL;
		}
	
	if( iImageEncoder )
		{
		delete iImageEncoder;
		iImageEncoder = NULL;
		}	
	
    if (iFrameImageData)
        {
        delete iFrameImageData;
        iFrameImageData = NULL;
        }
	
#ifdef DECODE_FROM_BUFFER    
    if(iSourceData) 
        {
        delete iSourceData;
        iSourceData = NULL;
        }
#endif
    //delete i512x512TnBitmap;
    
	DP0_IMAGIC((_L("CIETNGeneratorAO::DeleteObjects --")));
    }

void CIETNGeneratorAO::CancelRequestsAndDeleteObjects()
    {
    DP0_IMAGIC((_L("CIETNGeneratorAO::CancelRequestsAndDeleteObjects ++")));
    
	CancelOutStaningRequests();
	DeleteObjects();
	
	DP0_IMAGIC((_L("CIETNGeneratorAO::CancelRequestsAndDeleteObjects --")));
    }
