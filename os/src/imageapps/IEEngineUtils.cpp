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

#include <exifmodify.h>
#include <BAUTILS.H>
#include "IEEngineUtils.h" 
#include "ImagicConsts.h"
#include "debug.h"
#include <exifread.h>
#include <ExifUtility.h> 
#include <ICLExif.h> 
//#include <iclextjpegapi.h>  // For CExtJpegDecoder

#define KJpegDecIVAUidValue 0x10272C10
#define KJpegOptUidValue 0x101FF555

EXPORT_C CIEEngineUtils::CIEEngineUtils(RFs& aFs)
    : iFs(aFs)
    {
    } 

EXPORT_C CIEEngineUtils::~CIEEngineUtils()
    {
    }

/* Generating IETNFileName with complete path */
EXPORT_C void CIEEngineUtils::GenerateThumbnailFileName(
        TThumbSize aTNResolution,
        const TDesC& aSavedFileName, 
        TDes &aIETNFileName)
    {
    TFileName tmpName;
    TParse parser;
    switch(aTNResolution)
        {
        case ESize512x512:
            {
            parser.Set(aSavedFileName, NULL, NULL);
            tmpName = parser.DriveAndPath();//only path name
            tmpName.Append(K512x512TNFilePath);
            tmpName.Append(parser.NameAndExt());
            tmpName.Append(K512x512Ext);
            aIETNFileName.Copy(tmpName);
            break;
            }
        case ESize128x128:
           {
           parser.Set(aSavedFileName, NULL, NULL);
           tmpName = parser.DriveAndPath();//only path name
           tmpName.Append(K128x128TNFilePath);
           tmpName.Append(parser.NameAndExt());
           tmpName.Append(K128x128Ext);
           aIETNFileName.Copy(tmpName);
           break;
           }
        case ESize32x32:
           {
           parser.Set(aSavedFileName, NULL, NULL);
           tmpName = parser.DriveAndPath();//only path name
           tmpName.Append(K32x32TNFilePath);
           tmpName.Append(parser.NameAndExt());
           tmpName.Append(K32x32Ext);
           aIETNFileName.Copy(tmpName);
           break;
           }
        default:
           ASSERT(0);
           break;
        };
    }

EXPORT_C void CIEEngineUtils::DeleteThumbnails(TDesC& aFileName, RFs& aFs)
    {
    TThumbSize res[] = { ESize512x512, ESize128x128, ESize32x32 };
    for (TInt i = 0;i < sizeof(res)/ sizeof(TThumbSize);i++)
        {
        TFileName thumbFileName;
        GenerateThumbnailFileName(res[i], aFileName, thumbFileName);
        BaflUtils::DeleteFile(aFs, thumbFileName);
        }    
    }

/*Creating TN Folder */
EXPORT_C TInt CIEEngineUtils::CreateTNFolder(RFs aFs, const TDesC& aTNPath)
    {
    TInt error = KErrNone;
    if( !BaflUtils::PathExists( aFs, aTNPath ) )
        {
        error = aFs.MkDirAll( aTNPath );
        error = aFs.SetAtt( aTNPath, KEntryAttHidden, NULL );
        }
   
    return error;
    }

// Writes face coordinates to Exif data if faces was found
EXPORT_C TInt CIEEngineUtils::AddFaceCoordinate(const TFileName aFilename, RArray<TRect>& aCordArray)
    {
    DP0_IMAGIC(_L("CIEEngineUtils::AddFaceCoordinate++"));
    // Read first current maker note to new array from given file
    RArray<TRect> newCordArray;
    ReadFaceCoordinatesL(aFilename, newCordArray);
     
    // Append existing coords to new coords array
    for(TInt i=0; i<newCordArray.Count(); i++)
        {
        aCordArray.Append(newCordArray[i]);
        }
              
     // Write all coords to file exif data manufactorer note
    WriteFaceCoordinatesL(aFilename, aCordArray);
     
    newCordArray.Close();
    
    DP0_IMAGIC(_L("CIEEngineUtils::AddFaceCoordinate--"));
    return KErrNone;
    }

EXPORT_C HBufC8* CIEEngineUtils::ReadExifMakerNoteL(const TDes &aFileName)
    {
    DP0_IMAGIC(_L("CIEEngineUtils::ReadExifMakerNoteL++"));
    
    HBufC8* exif = ReadExifHeaderL(iFs, aFileName);
    CleanupStack::PushL( exif );
    
    CExifRead* exifRead = CExifRead::NewL(exif->Des(), CExifRead::ENoJpeg);
    CleanupStack::PushL( exifRead );
        
    // Get required data from the Exif image...
    /*TUint32  xRes;
    TUint32  yRes;
    exifRead->GetPixelXDimension(xRes);
    exifRead->GetPixelYDimension(yRes);*/  
    HBufC8* makerNote = exifRead->GetMakerNoteL();
    CleanupStack::PushL( makerNote );
            
    CleanupStack::Pop( makerNote );
    CleanupStack::PopAndDestroy( exifRead );
    CleanupStack::PopAndDestroy( exif );

    DP0_IMAGIC(_L("CIEEngineUtils::ReadExifMakerNoteL--"));
    return makerNote;
    }

EXPORT_C TInt CIEEngineUtils::RemoveFaceCoordinate(const TFileName a128x128TNFileName, RArray<TRect>& aCordArray)
    {
    DP0_IMAGIC(_L("CIEEngineUtils::RemoveFaceCoordinate++"));
    
    //Read first current make note
    HBufC8* makerNote = ReadExifMakerNoteL(a128x128TNFileName);
     
    // Allocate buffer for coords to be removed
    HBufC8* heapComment = HBufC8::NewL(100);
    TPtr8 ptrCoords = heapComment->Des();
    //Copy coords to be removed to descriptor
    for(TInt i=0; i < aCordArray.Count(); i++)
        {
        ptrCoords.AppendNum(aCordArray[i].iTl.iX);
        ptrCoords.Append(' ');
        ptrCoords.AppendNum(aCordArray[i].iTl.iY);
        ptrCoords.Append(' ');
        ptrCoords.AppendNum(aCordArray[i].iBr.iX );
        ptrCoords.Append(' ');
        ptrCoords.AppendNum(aCordArray[i].iBr.iY);
        ptrCoords.Trim();
        }
     
    //Find coordinates from maker note
    TPtr8 tmpPtr = makerNote->Des();
    TInt res = tmpPtr.Find(ptrCoords);
     
    if(res == KErrNotFound)
        return res;
         
    //Remove coordinates from maker note
    TInt l = ptrCoords.Length();
    tmpPtr.Delete(res, ptrCoords.Length()+1);
     
    //Find number of faces from maker note and update it
    _LIT8(KNumberOfFace, "#");
    res = tmpPtr.Find(KNumberOfFace);
     
    TLex8 lex(makerNote->Ptr());
    lex.SkipAndMark(res+1);
    TInt faceCount = 0;
    lex.Val(faceCount);
     
    //Check lenght of number of faces string
    TInt length = 0;
    //TInt aFaceNumber = 1;
    if(faceCount < 10)
        length = 1;
    else
        length = 2;
          
    HBufC8* numberOfFaces = HBufC8::NewL(length);
    TPtr8 FaceNroPtr = numberOfFaces->Des();
    FaceNroPtr.AppendNum(faceCount-1);
              
    tmpPtr.Replace(res+1, length, FaceNroPtr);
    //TPtr8 numberOfFaces;
     
    delete numberOfFaces;
    //numberOfFaces.Copy();
    
    // 1. Read JPEG image from the file to a buffer...
    RFile file;
    User::LeaveIfError( file.Open( iFs, a128x128TNFileName, EFileWrite ) );
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
    CExifModify* modify = CExifModify::NewL( jpegImage->Des(), CExifModify::EModify );
    CleanupStack::PushL( modify );
     
    //3. Insert (Set) at least the mandatory Exif data.
    //TInt descSize = 300;
    //HBufC8* heapComment = HBufC8::NewL(descSize);
    TPtr8 ptr = makerNote->Des();
     
    modify->SetMakerNoteL(ptr);
    //modify->SetMakerNoteL(makerNote->Des());
     
    // 4. Get the new Exif image...
    // If zero length descriptor is given instead of jpeg->Des(), then only the
    // Exif meta data is returned.
    //HBufC8* newExif = modify->WriteDataL( jpegImage->Des() );
    HBufC8* newExif;
    TRAPD(err, newExif = modify->WriteDataL( jpegImage->Des() ));
     
    if(err != KErrNone)
        {
        TInt i=0;
        }
     
    //TPtr8 tmp = newExif->Des();
     
    User::LeaveIfError( file.Replace( iFs, a128x128TNFileName, EFileWrite ) );
    //Write Exif and jpeg image back to jpeg file
    User::LeaveIfError(file.Write(*newExif));
     
    // Process the new Exif data
    delete newExif;
    newExif = NULL;
     
    // 5. Delete the modifier instance...
    CleanupStack::PopAndDestroy( modify );
    CleanupStack::PopAndDestroy( jpegImage );
     
    file.Close();
    
    DP0_IMAGIC(_L("CIEEngineUtils::RemoveFaceCoordinate--"));
    return KErrNone;
    }



EXPORT_C void CIEEngineUtils::WriteFaceCoordinatesL(const TFileName a512x512TNFileName, RArray<TRect>& aCordArray)
    {
    DP1_IMAGIC(_L("CIEEngineUtils::WriteFaceCoordinatesL a512x512TNFileName = %S ++"), &a512x512TNFileName);
    
    TInt error = KErrNone;
    RFile tnFile;
    TInt size = 0;
    
    //Check that coords got from IDL makes sense. Eg. not out of image area etc.
    /*TSize tnSize;
    iImageDecoder->GetImageSizeL(iCurrentImageData->iMGTN_320x320_Filename, tnSize);
    TInt brx;
    
    TInt count = aCordArray.Count();
    TBool removed = EFalse;
    for(TInt i=0; i<count; i++)
        {
        brx = aCordArray[i].iBr.iX;
        brx = aCordArray[i].iBr.iY;
        brx = aCordArray[i].iTl.iX;
        brx = aCordArray[i].iTl.iY;
        if(aCordArray[i].iTl.iX >= aCordArray[i].iBr.iX || aCordArray[i].iTl.iY >= aCordArray[i].iBr.iY)
            {
            aCordArray.Remove(i);
            removed = ETrue;
            }
        if(removed)
            count = aCordArray.Count();
        }
    */
   
    User::LeaveIfError(tnFile.Open(iFs, a512x512TNFileName, EFileRead));
    
    tnFile.Size(size);
    
    if(size <= 0) User::Leave(KErrUnderflow); // May be more meaningful error code shud be returned
    
    HBufC8* imageData = HBufC8::NewL(size);
    
    CleanupStack::PushL(imageData);
    
    TPtr8 imageDataPtr = imageData->Des();
    
    User::LeaveIfError(tnFile.Read(imageDataPtr));
    
    tnFile.Close();
    
    // Create the exifmodifier instance
    CExifModify* exifModifier = CExifModify::NewL(imageDataPtr, CExifModify::ECreate);
    
    CleanupStack::PushL(exifModifier);
    
    //3. Insert (Set) at least the mandatory Exif data...
    exifModifier->SetXResolutionL( 123, 1 ); 
    exifModifier->SetYResolutionL( 512, 1 ); 
    exifModifier->SetResolutionUnitL( 2 );
    exifModifier->SetYCbCrPositioningL( 1 );
    exifModifier->SetComponentsConfigurationL( 1, 2, 3, 0 );
    exifModifier->SetColorSpaceL( 1 );
    exifModifier->SetPixelXDimensionL( 512 );
    exifModifier->SetPixelYDimensionL( 512 );
    
    TInt descSize = aCordArray.Count()*4*4 + 32+10; // Be careful calculating like this!!!
    
    HBufC8* heapComment = HBufC8::NewL(descSize);
    
    CleanupStack::PushL(heapComment);
    
    TPtr8 ptr = heapComment->Des();
     
    ptr.Append(KFaceCoordsHeader);
    ptr.Append(KSpace);
    ptr.Append(KFaceCoordsImagicVersion);
    ptr.Append(KSpace);
    ptr.Append(KHash);
     
    //Set number of faces detected to Exif data
    if(aCordArray.Count() == 0)
        ptr.Append(KZero);
    else
        {
        ptr.AppendNum(aCordArray.Count());
        ptr.Append(KSpace);
        for(TInt i=0; i<aCordArray.Count(); i++)
            {
            //TInt size = ptr.Size();
            ptr.AppendNum(aCordArray[i].iTl.iX);
            ptr.Append(KSpace);
            ptr.AppendNum(aCordArray[i].iTl.iY);
            ptr.Append(KSpace);
            ptr.AppendNum(aCordArray[i].iBr.iX);
            ptr.Append(KSpace);
            ptr.AppendNum(aCordArray[i].iBr.iY);
            ptr.Append(KSpace);
            }    
        }
    
    exifModifier->SetMakerNoteL(ptr);
    
    HBufC8* newImageData = exifModifier->WriteDataL(imageDataPtr); // newImageData contains the image data with the modified exif data
    CleanupStack::PushL(newImageData);
    
    if(newImageData == NULL)
        User::Leave(KErrNotFound); // Better error code should be returned
    
    TPtr8 newImageDataPtr = newImageData->Des();
    
    // Create the new thumbnail image with modified exif data
    User::LeaveIfError(tnFile.Replace(iFs, a512x512TNFileName, EFileWrite));
    User::LeaveIfError(tnFile.Write(newImageDataPtr));
    
    tnFile.Flush();
    tnFile.Close();
    
    CleanupStack::PopAndDestroy(4);
    
    DP0_IMAGIC(_L("CIEEngineUtils::WriteFaceCoordinatesL --"));
    }

EXPORT_C void CIEEngineUtils::ReadFaceCoordinatesL(const TFileName a512x512TNFileName, RArray<TRect>& aCordArray)
    {
    DP1_IMAGIC(_L("CIEEngineUtils::ReadFaceCoordinatesL, a512x512TNFileName = %S ++"), &a512x512TNFileName);
    
    TInt count = aCordArray.Count();
    for(TInt i=0; i<count; i++)
        aCordArray.Remove(0);
    
    HBufC8* imageData = ReadExifHeaderL(iFs, a512x512TNFileName);
    CleanupStack::PushL(imageData);

    CExifRead* exifReader;
    exifReader = CExifRead::NewL(imageData->Des(), CExifRead::ENoJpeg);
    CleanupStack::PushL(exifReader);
    
    HBufC8* makerNoteData = exifReader->GetMakerNoteL();
    TPtr8 makerNoteDataPtr = makerNoteData->Des();

    // No valid face information
    if (makerNoteDataPtr.Find(KFaceCoordsHeader) != 0)
        User::Leave(KErrNotFound);
    
    // 31 is the length of the string KFaceCoordsHeader+KSpace+KFaceCoordsImagicVersion+KSpace+KHash
    makerNoteDataPtr.Delete(0, 31);

    TRect rect(0,0,0,0);
    TLex8 lex(makerNoteDataPtr.Ptr());
    TInt faceCount = 0;
    
    lex.Val(faceCount);
    
    if(faceCount > 0)
        {
        for(TInt i=0; i<faceCount; i++)
            {
            lex.SkipSpaceAndMark(); 
            lex.Val(rect.iTl.iX);
            lex.SkipSpaceAndMark(); 
            lex.Val(rect.iTl.iY);
            
            lex.SkipSpaceAndMark(); 
            lex.Val(rect.iBr.iX);
            lex.SkipSpaceAndMark(); 
            lex.Val(rect.iBr.iY);
            
            aCordArray.Append(rect);
            
            DP4_IMAGIC(_L("Rect(%d, %d, %d, %d)"), rect.iTl.iX, rect.iTl.iY, rect.iBr.iX, rect.iBr.iY);
            
            rect = TRect(0,0,0,0);
            }
        aCordArray.SortSigned();
        }
    
    CleanupStack::PopAndDestroy(2);
        
    DP0_IMAGIC(_L("CIEEngineUtils::ReadFaceCoordinatesL --"));
    }

EXPORT_C void CIEEngineUtils::GetModifiedTimeL(const TDes &aFileName, TTime& aTime)
    {
    DP0_IMAGIC(_L("CIEEngineUtils::GetFileModifiedTimeL++"));
   
    // Read file modified date
    RFile file;
    TInt error = file.Open(iFs, aFileName , EFileRead);
    DP1_IMAGIC(_L("CIEEngineUtils::GetFileModifiedTimeL - file open error = %d"), error);
    User::LeaveIfError(error);
    file.Modified(aTime);
    file.Close();
    DP0_IMAGIC(_L("CIEEngineUtils::GetFileModifiedTimeL--"));
    }

EXPORT_C void CIEEngineUtils::GetImageSizeL(const TDes &aFileName, TSize& aSize)
    {
    DP0_IMAGIC(_L("CIEEngineUtils::GetImageSizeL++"));
    
    CImageDecoder* imageDecoder = CImageDecoder::FileNewL(iFs, aFileName);
    TFrameInfo frameInfo = imageDecoder->FrameInfo();
    aSize = frameInfo.iFrameCoordsInPixels.Size();

    delete imageDecoder;
    imageDecoder = NULL;   
    
    DP2_IMAGIC(_L("CIEEngineUtils::GetImageSizeL-- [%d x %d]"), aSize.iWidth, aSize.iHeight);
    }

EXPORT_C HBufC8* CIEEngineUtils::ReadExifHeaderL(RFs& aFs, const TDesC &aFileName) 
    {
    DP0_IMAGIC(_L("CIEEngineUtils::ReadExifHeaderL++"));
    RFile file;
    User::LeaveIfError(file.Open(aFs, aFileName, EFileRead|EFileShareReadersOnly));
    CleanupClosePushL(file);
    
    TInt size;
    file.Size(size);
    size = Min(size, 64 * 1024);    // TODO use exact exif size
        
    HBufC8* exif = HBufC8::NewL(size);
    CleanupStack::PushL(exif);
    TPtr8 bufferPtr(exif->Des()); 
    // PHOREPOC PATCH: read exactly the bytes asked for. Des() of a heap buffer as large as 64 KB is as long as the cell is (the allocator
    // rounds a large cell up to whole pages, here 0x10ffc), so a plain Read() filled the whole cell for any JPEG over 64 KB, and the
    // EXIF parse that followed ran off the end of the heap chunk (KERN-EXEC 3 in the finder thread).
    User::LeaveIfError(file.Read(bufferPtr, size));

    CleanupStack::Pop(exif); // exif
    CleanupStack::PopAndDestroy(); // file
    
    DP0_IMAGIC(_L("CIEEngineUtils::ReadExifHeaderL--"));
    return exif;
    }

EXPORT_C HBufC8* CIEEngineUtils::ReadExifThumbnailL(RFs& aFs, const TDesC& aFileName)
    {
    DP1_IMAGIC(_L("CIEEngineUtils::ReadExifThumbnailL++ %S"), &aFileName);
    
    HBufC8* exif = ReadExifHeaderL(aFs, aFileName);
    CleanupStack::PushL(exif);
    
    // Instantiate Exif reader
    CExifRead* exifRead = CExifRead::NewL(*exif, CExifRead::ENoJpeg);
    CleanupStack::PushL(exifRead);
    
    // Get required data from the Exif image
    HBufC8* exifThumb = exifRead->GetThumbnailL();
    CleanupStack::PushL(exifThumb);

    /*TUint32 w, w2, h, h2;
    exifRead->GetThumbnailXResolution(w, w2);
    exifRead->GetThumbnailYResolution(h, h2);*/
    
    CleanupStack::Pop(exifThumb);
    CleanupStack::PopAndDestroy(exifRead);
    CleanupStack::PopAndDestroy(exif);
    DP0_IMAGIC(_L("CIEEngineUtils::ReadExifThumbnailL--"));
    return exifThumb;
    }

//------------------------------------------------------------------------------
// Read the JPEG EXIF creation timestamp and orientation
//------------------------------------------------------------------------------
EXPORT_C void CIEEngineUtils::GetExifDateTimeAndOrientationL(
        const TDesC& aFilename, 
        TTime& aExifDateTime,
        TUint16& aOrientation)
    {
    HBufC8* exifDateTime = NULL;
    
//#define USE_EXIF_DECODER
#ifdef USE_EXIF_DECODER
    // First create the decoder and attach it to the JPEG file. The
    // decoder implementation UID has to be specified or calling
    // ExifMetadata() will crash.
    CImageDecoder* imageDecoder = NULL;
    imageDecoder = CImageDecoder::FileNewL(
            iFs, 
            aFilename, 
            CImageDecoder::EOptionNone, 
            KImageTypeJPGUid, 
            KNullUid,
            TUid::Uid(KJPGDecoderImplementationUidValue));

    // The specific implementation UID makes the downcasting safe.
    // Besides, there is no other way to use the decoder.
    CJPEGExifDecoder* jpegDecoder = static_cast<CJPEGExifDecoder*>(imageDecoder);
    CleanupStack::PushL(jpegDecoder);
    // Read the EXIF timestamp, format "YYYY:MM:DD HH:MM:SS".
    MExifMetadata* exifData = jpegDecoder->ExifMetadata();
    if(!exifData)
        User::Leave(KErrNotSupported);
    
    TExifReaderUtility reader(exifData);
    exifDateTime = HBufC8::NewLC(KPMMExifDateTimeOriginalLength);
    TInt error = reader.GetDateTimeOriginal(exifDateTime);
    User::LeaveIfError(error);
#else
    HBufC8* exifData = ReadExifHeaderL(iFs, aFilename);
    CleanupStack::PushL(exifData);
    CExifRead* exifReader = CExifRead::NewL(*exifData, CExifRead::ENoJpeg);
    CleanupStack::PushL(exifReader);
    
    exifDateTime = exifReader->GetDateTimeOriginalL();
#endif    
    // Convert the descriptor to a TDateTime as it cannot be converted
    // directly to a TTime.
    TLex8 lexer(*exifDateTime);
    TInt timeValue;
    TDateTime intermediateDateTime;
    // Year
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetYear(timeValue);
    lexer.Inc(); // Skip the colon.
    // Month
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetMonth(TMonth(timeValue-1));
    lexer.Inc();
    // Day
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetDay(timeValue-1);
    lexer.Inc();
    // Hours
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetHour(timeValue);
    lexer.Inc();
    // Minutes
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetMinute(timeValue);
    lexer.Inc();
    // Seconds
    User::LeaveIfError(lexer.Val(timeValue));
    intermediateDateTime.SetSecond(timeValue);

    // Finally, convert the TDateTime to a TTime.
    aExifDateTime = intermediateDateTime;

    // Read orientation
    TUint16 exifOrientation;
#ifdef USE_EXIF_DECODER    
    if (reader.GetOrientation(exifOrientation) == KErrNone)
#else
    if (exifReader->GetOrientation(exifOrientation) == KErrNone)
#endif        
        {
        switch (exifOrientation)
            {
            case 1: case 2:
                aOrientation = 0;
                break;

            case 3: case 4:
                aOrientation = 180;
                break;
                
            case 5: case 8:
                aOrientation = 90;
                break;
                
            case 6: case 7:
                aOrientation = 270;
                break;
                
            default:
                DP0_IMAGIC(_L("CIEEngineUtils::GetExifDateTimeAndOrientationL: invalid orientation"));
            }
        
        DP1_IMAGIC(_L("CIEEngineUtils::GetExifDateTimeAndOrientationL: %d"), aOrientation);        
        }
    
#ifdef USE_EXIF_DECODER
    CleanupStack::PopAndDestroy(exifDateTime);    
    CleanupStack::PopAndDestroy(jpegDecoder);
#else
    CleanupStack::PopAndDestroy(exifReader);
    CleanupStack::PopAndDestroy(exifData);    
#endif    
    }

EXPORT_C TUid CIEEngineUtils::GetImageDecoderUid()
    {
    CImplementationInformationType* type;
    TInt error;

    TUid uid = TUid::Uid(KJpegDecIVAUidValue);
    TRAP(error, type = CImageDecoder::GetImplementationInformationL(uid));
    if (error == KErrNone)
        {
        DP0_IMAGIC(_L("CIEEngineUtils::GetImageDecoderUid: IVA decoder found"));
        return uid;
        }

    uid = TUid::Uid(KJpegOptUidValue);
    TRAP(error, type = CImageDecoder::GetImplementationInformationL(uid));
    if (error == KErrNone)
        {
        DP0_IMAGIC(_L("CIEEngineUtils::GetImageDecoderUid: Emuzed decoder found"));
        return uid;
        }
   
    /*CExtJpegDecoder* extDecoder;
    TRAP(error, extDecoder = CImageDecoder::DataNewL(CExtJpegDecoder::EHwImplementation));
    if (error == KErrNone)
        return extDecoder->ImplementationUid();*/
    
    /*TRAP(error, type = CImageDecoder::GetImplementationInformationL(CExtJpegDecoder::ESwImplementation));
    if (error == KErrNone)
        return type->ImplementationUid();*/ 
  
    DP0_IMAGIC(_L("CIEEngineUtils::GetImageDecoderUid: no specified decoder found"));    
    return KNullUid;
    }
