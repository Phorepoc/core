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
#include <BAUTILS.H>

#include "IEBgpsController.h"
#include "debug.h"
#include "ImagicConsts.h"

// ====================== MEMBER FUNCTION ================================== //

CIEBgpsController* CIEBgpsController::NewL(RFs& aFileServer,
                                        MIEBgpsControllerObserver& aIEBgpsControllerObserver, 
                                        CIEEngineUtils& aEngineUtils,
                                        RCriticalSection* aCritical)
    {
    CIEBgpsController* self = new (ELeave) CIEBgpsController(aFileServer,
                                                            aIEBgpsControllerObserver, 
                                                            aEngineUtils,
                                                            aCritical);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
    }

CIEBgpsController::CIEBgpsController(RFs& aFileServer, MIEBgpsControllerObserver& aIEBgpsControllerObserver, 
                                     CIEEngineUtils& aEngineUtils, RCriticalSection* aCritical)
    :iFileServer(aFileServer),
    iIEBgpsControllerObserver(aIEBgpsControllerObserver),
    iFileLoader(NULL),
    iIEEngineUtils(aEngineUtils),
    iCritical(aCritical)
    {
    
    }

CIEBgpsController::~CIEBgpsController()
    {
    DP0_IMAGIC(_L("CIEBgpsController::~CIEBgpsController++"));
    
    if(iIEBgpsClient)
        {
        delete iIEBgpsClient;
        iIEBgpsClient = NULL;    
        }
    
    iFaceCropImageDataArray.Close();
    iFBCoordinateArray.Close();
    
    DP0_IMAGIC(_L("CIEBgpsController::~CIEBgpsController--"));
    }

void CIEBgpsController::ConstructL()
    {
    DP0_IMAGIC(_L("CIEBgpsController::ConstructL++"));
    
    iImageIndex = 0;
    iSingleTNGeneration = EFalse;
    iTnCreationCancelled = EFalse;
    iAllTNsDone = EFalse;
    
    //iIEBgpsClient = CIEImageProcessing::NewL(*this);
    
    DP0_IMAGIC(_L("CIEBgpsController::ConstructL--"));
    }


void CIEBgpsController::SetFileLoader(CIEFileLoader* aFileLoader)
    {
    DP0_IMAGIC(_L("CIEBgpsController::SetFileLoader"));
    
    iFileLoader = aFileLoader;
    }


void CIEBgpsController::CreateImageProcessing()
    {
    DP0_IMAGIC(_L("CIEBgpsController::CreateImageProcessing"));
    
    iIEBgpsClient = CIEImageProcessing::NewL(*this);
    
    }


void CIEBgpsController::ThumbnailGenerationCompleted(TInt aErrorCode)
    {
    DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted++"));
    
    RDebug::Printf("[imagic-bgps] ThumbnailGenerationCompleted err %d, ops 512:%d 128:%d 32:%d, arrayCount %d", aErrorCode, i512x512TNCreationOn, i128x128TNCreationOn, i32x32TNCreationOn, iImageDataArray.Count());
    if (aErrorCode == KErrCancel)
        return;
    
    /*CImageData* imageData = iIEBgpsControllerObserver.GetImageData(iTnCreationIndex, EImages);
    TFileName filename;
    imageData->GetFileName(filename, ESize512x512);
    DP1_IMAGIC(_L("CFaceBrowser::BrowseFacesL 512x512TNFile = %S ++"), &filename);*/
        
    //Single TN generation ----------->
    if(aErrorCode != KErrNone)
        {
        DP1_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - Error in TN creation: %d"), aErrorCode);
        
        CImageData* imageData = iIEBgpsControllerObserver.GetImageData(iTnCreationIndex);
        //Mark image as corrupted so we do not try to generate TN or load it
        imageData->iGridData.iCorrupted = ETrue;
            
        //In Error case any way delete the iCorrupted Thumbnail + file we were trying to generate TN(if it is not original)
        if(!iSingleFaceDetectionOn)
            {
            DeleteCorruptedThumbNailFile(iTNFilename);
            if(iJpegFilename.Find(KPAlbTNFilePath) != KErrNotFound)
                {
                DeleteCorruptedThumbNailFile(iJpegFilename);
                }
            }
        }

    iSingleFaceDetectionOn = EFalse;
    
    //If no error mark that TN was created
    if(aErrorCode == KErrNone)
        {
        if(i512x512TNCreationOn)
            {
            DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - i512x512TNCreationOn"));
            iImageDataArray[iTnCreationIndex]->iGridData.iCorrupted = EFalse;
            iImageDataArray[iTnCreationIndex]->SetImageReady(ESize512x512, ETrue);
            iIEBgpsControllerObserver.SingleTNGenerationComplete(iTnCreationIndex, ESize512x512);

/*#ifdef FACE_DETECTION
            TInt count = iFBCoordinateArray.Count();
            for(TInt i=0; i<count; i++)
                iFBCoordinateArray.Remove(0);
                
            iSingleFBCoordinateArray = &iFBCoordinateArray;
            iImageData = iIEBgpsControllerObserver.GetImageData(iTnCreationIndex, EImages);
            
            iIEBgpsClient->StartSingleFaceDetection(iTnCreationIndex, *iSingleFBCoordinateArray, iImageData);
            iSingleFaceDetectionOn = ETrue;
#endif*/
            }
        if(i128x128TNCreationOn)
            {
            DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - i128x128TNCreationOn"));
            iImageDataArray[iTnCreationIndex]->SetImageReady(ESize128x128, ETrue);
            iIEBgpsControllerObserver.SingleTNGenerationComplete(iTnCreationIndex, ESize128x128);
            }
        if(i32x32TNCreationOn)
            {
            DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - i32x32TNCreationOn"));
            iImageDataArray[iTnCreationIndex]->SetImageReady(ESize32x32, ETrue);
            iIEBgpsControllerObserver.SingleTNGenerationComplete(iTnCreationIndex, ESize32x32);
            
#ifdef FACE_DETECTION
            TInt count = iFBCoordinateArray.Count();
            for(TInt i=0; i<count; i++)
                iFBCoordinateArray.Remove(0);
                
            iSingleFBCoordinateArray = &iFBCoordinateArray;
            iImageData = iIEBgpsControllerObserver.GetImageData(iTnCreationIndex);
            
            //iIEBgpsClient->StartSingleFaceDetection(iTnCreationIndex, *iSingleFBCoordinateArray, iImageData);
            iSingleFaceDetectionOn = ETrue;
#endif
            }
        }

    if(aErrorCode != KErrNone || !iSingleFaceDetectionOn)
        {
        TThumbSize res;
        iTnCreationIndex = FindMissingTN(res);
        RDebug::Printf("[imagic-bgps] next index %d res %d, selected image %d, count %d", iTnCreationIndex, (TInt)res, iIEBgpsControllerObserver.GetSelectedImageIndex(), iImageDataArray.Count());
        
        iAllTNsDone = ETrue;
        //Check if all TN images are done
        for(TInt i=0; i<iImageDataArray.Count(); i++)
            {
            if((!iImageDataArray[i]->IsImageReady(ESize512x512) || 
               !iImageDataArray[i]->IsImageReady(ESize128x128) ||
               !iImageDataArray[i]->IsImageReady(ESize32x32)) &&
               !iImageDataArray[i]->iGridData.iCorrupted )
                {
                iAllTNsDone = EFalse;
                DP1_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - Continue TN creation from index: %d"), i);
                break;
                }
            }
                        
        if(!iAllTNsDone && iTnCreationIndex >= 0)
            {
            //Start creating thumbnails by calling TN creator
            if(res == ESize512x512)
                {
                DP1_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - Res: ESize512x512, Index: %d"),iTnCreationIndex);
                i512x512TNCreationOn = ETrue;
                i128x128TNCreationOn = EFalse;
                i32x32TNCreationOn = EFalse;
                Generate512x512Thumbnails(iTnCreationIndex);
                }
            else if(res == ESize128x128)
                {
                DP1_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - Res: ESize128x128, Index: %d"),iTnCreationIndex);
                i128x128TNCreationOn = ETrue;
                i512x512TNCreationOn = EFalse;
                i32x32TNCreationOn = EFalse;
                Generate128x128Thumbnails(iTnCreationIndex);
                }
            else if(res == ESize32x32)
                {
                DP1_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - Res: ESize32x32, Index: %d"),iTnCreationIndex);
                i32x32TNCreationOn = ETrue;
                i512x512TNCreationOn = EFalse;
                i128x128TNCreationOn = EFalse;
                Generate32x32Thumbnails(iTnCreationIndex);
                }
            }
        
        // Callback after completion of all TNs 
        if(iAllTNsDone)
            {
            i32x32TNCreationOn = EFalse;
            i128x128TNCreationOn = EFalse;
            i512x512TNCreationOn = EFalse;
            DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted - All thumbnails are created, start face Detection"));
            //iIEBgpsClient->StartFaceDetection(iImageDataArray);
            //iBackGroundFaceDetectionOn = ETrue;
            iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
            }
        }

    DP0_IMAGIC(_L("CIEBgpsController::ThumbnailGenerationCompleted --"));    
    }


void CIEBgpsController::HandleError(TInt aError)
    {
    DP0_IMAGIC(_L("CIEBgpsController::HandleError"));
    //Handle BGPS errors here
    
    //Cancel UI frame blinking, we can not do anythign else if BGPS is jammed??
    //iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
    if(i32x32TNCreationOn || i128x128TNCreationOn || i512x512TNCreationOn)
        {
        DP0_IMAGIC(_L("CIEBgpsController::HandleError - Error on TN generation, try to continue"));
        ThumbnailGenerationCompleted(aError);
        }
    else
        {
        iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
        }
    }

void CIEBgpsController::ThumbnailGenerationCancelled(TInt /*aErrorCode*/)
    {
    //Start TN generation again if it was stopped
    if(iTnCreationCancelled)
        ;//StartSingleFaceDetection();
    }

void CIEBgpsController::GenerateThumbNailL(const TDes &aOrgFile, TThumbSize /*aTNResolution*/)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GenerateThumbNailL ++"));
    
    iSavedFileName.Copy(aOrgFile);
          
    /*Generating Thumbnail path for 320x320 */
    iIEEngineUtils.GenerateThumbnailFileName(ESize32x32,iSavedFileName,iTNFilename);
    
    /*Creating folder if not exists */
    TParse parser;
    parser.Set(iTNFilename, NULL, NULL );
    TFileName name = parser.NameAndExt();//image file name
    TFileName tnPath = parser.DriveAndPath();
       
    CIEEngineUtils::CreateTNFolder(iFileServer, tnPath);
    
    /*Generating TN file with absolute path */
    tnPath.Append(name);
     
    /* TN file name with absolute path*/
    iTNFilename.Copy(tnPath);
    
    /*Set Single Thumbnail generation flag on */
    iSingleTNGeneration = ETrue; 
    
    /*Generate TN for 320x320 resolution */
    iLatestCreatedTNSize = ESize32x32;
    iTmpImageData = CImageData::NewL();
    
    TSize originalSize;
    iIEEngineUtils.GetImageSizeL(iSavedFileName, originalSize);
    
    DP1_IMAGIC(_L("iTNFilename: %S"), &iTNFilename);
    
    TSize size(320, 320);
    if (originalSize.iWidth && originalSize.iHeight)
        {
        if(originalSize.iWidth > originalSize.iHeight)
            { // Landscape
            size.iHeight = 320 * originalSize.iHeight / originalSize.iWidth;
            if(size.iHeight%2 != 0)
                size.iHeight++;
            }
        else // Portrait
            {
            size.iWidth = 320 * originalSize.iWidth / originalSize.iHeight;
            if(size.iWidth%2 != 0)
                size.iWidth++;
            }
        }
    
    iTmpImageData->SetSize(size);
    iIEBgpsClient->GenerateTN(iSavedFileName, iTNFilename, size);

    
    DP0_IMAGIC(_L("CIEBgpsController::GenerateThumbNailL--"));
    }


//Filename array has been filled(completetely) when this function is called
void CIEBgpsController::AllFilesAddedToFilenameArrayL()
    {
    DP0_IMAGIC(_L("CIEBgpsController::AllFilesAddedToFilenameArrayL++"));
    
    //Get filename array
    iImageDataArray = iFileLoader->GetFileNameArray();
    
    // Start TN generation
    if(iImageDataArray.Count() > 0)
        {
        StartTNCreatorL();
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::AllFilesAddedToFilenameArrayL--"));
    }

TReal CIEBgpsController::GetAspectRatio(TInt aIndex)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GetAspectRatio"));
    
    iImageDataArray = iFileLoader->GetFileNameArray();
    
    if(aIndex < iImageDataArray.Count())
        {
        return iImageDataArray[aIndex]->GetAspectRatio();
        }
    else
        {
        return 0;
        }
    }

TReal CIEBgpsController::GetFacesAspectRatio(TInt aIndex)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GetAspectRatio"));
    
    iImageDataArray = iFileLoader->GetFacesFileNameArray();
    
    if(aIndex < iImageDataArray.Count())
        {
        return iImageDataArray[aIndex]->GetAspectRatio();
        }
    else
        return 0;
    }

void CIEBgpsController::GenerateTNForEditedImage(const TFileName aEditedFileName, const TReal aAspectRatio)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GenerateTNForEditedImage++"));

    iSavedFileName.Copy(aEditedFileName);
    
    //set iSingleTNGeneration mode on
    iSingleTNGeneration = ETrue; 
    iAspectRatio = aAspectRatio;
     
    /*Generating 128x128 Thumbnail path */
    iIEEngineUtils.GenerateThumbnailFileName(ESize128x128, aEditedFileName, iTNFilename);
 
    //Create new imagedata instance to be appended to filename array
    iTmpImageData = CImageData::NewL();
    iTmpImageData->SetSize(TSize(128 * iAspectRatio, 128)); // TODO no size info
    
    iLatestCreatedTNSize = ESize128x128;
    iIEBgpsClient->GenerateTN(aEditedFileName, iTNFilename, TSize(128,128));
    //iIEBgpsClient->GenerateTN(aEditedFileName, iTNFilename, TSize(32,32));//mikares32
    
    DP0_IMAGIC(_L("CIEBgpsController::GenerateTNForEditedImage--"));
    }

void CIEBgpsController::FilenameArrayCountChanged(const RArray<CImageData*>& aImageDataArray)
    {
    iImageDataArray = aImageDataArray;
    //iIEBgpsControllerObserver.TotalNumberOfImagesChanged(iImageDataArray.Count());
    }

CImageData* CIEBgpsController::GetImageData(const TInt aIndex)
    {
    DP1_IMAGIC(_L("CIEBgpsController::GetImageData %d"), aIndex);
    
    return (aIndex <= iImageDataArray.Count()) ? iImageDataArray[aIndex] : NULL;
    }

void CIEBgpsController::GetFaceCoordinates(const TFileName aTNFileName, RArray<TRect>& aFaceCoordinateArray)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GetFaceCoordinates"));
    aFaceCoordinateArray.Reset();
    //iIEBgpsClient->FindFaces(aTNFileName, aFaceCoordinateArray);
    }

//If background face Detection is still continuing, we have to use GetSingleFaceCoordinates() function to get face coordinates
void CIEBgpsController::GetSingleFaceCoordinates(TInt aIndex, const TFileName /*aTNFileName*/, RArray<TRect>& aFaceCoordinateArray)
    {
    DP0_IMAGIC(_L("CIEBgpsController::GetSingleFaceCoordinates++"));
    
    if(i128x128TNCreationOn || i512x512TNCreationOn || i32x32TNCreationOn)
        {
        iTnCreationCancelled = ETrue;
        CancelTNGeneration();
        i128x128TNCreationOn = EFalse;
        i512x512TNCreationOn = EFalse;
        i32x32TNCreationOn = EFalse;
        }

    iSingleFBIndex = aIndex;
    iSingleFBCoordinateArray = &aFaceCoordinateArray;
    
    //If BGPS face Detection was on, we have to stop it first. Othervice we can contiue directly to StartSingleFaceDetection() 
    if(iBackGroundFaceDetectionOn)
        {//We just cancel background face Detection, when cancel is complete we get callback to StartSingleFaceDetection()
        //iIEBgpsClient->CancelFaceDetection();
        iBackGroundFaceDetectionOn = EFalse;
        }
    else
        {
        //StartSingleFaceDetection();
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::GetSingleFaceCoordinates--"));
    }

/* Creating Single Thumbnails for Editing. After editing the image we are 
 * genereating the thumbnails 128x128 & 640x480 resolution*/
void CIEBgpsController::CreateSingleTNL()
    {
    DP0_IMAGIC(_L("CIEBgpsController::CreateSingleTN++"));
    
    if(iLatestCreatedTNSize == ESize128x128)  
        {
        //Create and set data to image data item
        iTmpImageData->SetFileNameL(iSavedFileName);
        iImageDataArray[iImageIndex]->SetImageReady(EFullSize, ETrue);
        iImageDataArray[iImageIndex]->SetImageReady(ESize32x32, EFalse);
        iImageDataArray[iImageIndex]->SetImageReady(ESize512x512, EFalse);
        iImageDataArray[iImageIndex]->SetImageReady(ESize128x128, ETrue);
                
        //Insert new image data to filename array
        iFileLoader->AddNewImage(iTmpImageData, iImageIndex);
        iImageDataArray = iFileLoader->GetFileNameArray();
        //iIEBgpsControllerObserver.TotalNumberOfImagesChanged(iImageDataArray.Count());
        
        /*Generating 320x320 Tumbnail path */
        iIEEngineUtils.GenerateThumbnailFileName(ESize32x32,iSavedFileName,iTNFilename);
        iIEBgpsControllerObserver.TNGenerationComplete(ESize128x128);
        
        iLatestCreatedTNSize = ESize32x32;
        TSize size;

        TInt w, h;
        if(iImageDataArray[iImageIndex]->GetAspectRatio() > 1)
            {//Landscape
            w=320;
            h = 320/iImageDataArray[iImageIndex]->GetAspectRatio();
            if(h%2 != 0)
                h++;
            }
        else//Portrait
            {
            h = 320;
            w = 320 * iImageDataArray[iImageIndex]->GetAspectRatio();
            if(w%2 != 0)
                w++;
            }
        size.iHeight = h;
        size.iWidth = w;
        
        iTNSize = size;
        iIEBgpsClient->GenerateTN(iSavedFileName, iTNFilename, size);
        }
    
    else if(iLatestCreatedTNSize == ESize32x32)
        {
        iImageDataArray = iFileLoader->GetFileNameArray();
        iImageDataArray[iImageIndex]->SetImageReady(ESize32x32, ETrue);
        //iSingleTNGeneration = EFalse;
        
        iIEBgpsControllerObserver.TNGenerationComplete(ESize32x32);
        
        TSize size;
        size.iHeight = 512;
        size.iWidth = 512;
        
        iIEEngineUtils.GenerateThumbnailFileName(ESize512x512, iSavedFileName, iTNFilename);
        
        iTNSize = size;
        iIEBgpsClient->GenerateTN(iSavedFileName, iTNFilename, size);
        
        iLatestCreatedTNSize = ESize512x512;
        }
    
    //TN creation complete
    else
        {
        iSingleTNGeneration = EFalse;
        
        iImageDataArray[iImageIndex]->SetImageReady(ESize512x512, ETrue);
        
        //Insert new image data to filename array
        iFileLoader->ModifyImageData(iTmpImageData, iImageIndex);
                
        //Call observer/AppUI class about completing TN creation
        iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::CreateSingleTN--"));
    }

void CIEBgpsController::DeleteCorruptedThumbNailFile(TFileName aFileName)
    {
    DP0_IMAGIC(_L("CIEBgpsController::DeleteCorruptedThumbNailFile++"));
    
    //Checking for if we generated Imgaic TN already from iCorrupted file and delete it
    //if(aFileName.Find(KPAlbTNFilePath) != KErrNotFound)
        if(BaflUtils::FileExists(iFileServer, aFileName))
            {
            TInt err = BaflUtils::DeleteFile(iFileServer, aFileName);
            DP2_IMAGIC(_L("CIEBgpsController::DeleteCorruptedThumbNailFile - file found: %S, err:%d"), &aFileName, err);
            }
    
    DP0_IMAGIC(_L("CIEBgpsController::DeleteCorruptedThumbNailFile--"));
    }

//Finds optimal size for 128x128 TN creation
void CIEBgpsController::CheckOptimalFileFor128x128TnCreation(TInt aIndex, TFileName& aFileName)
    {
    DP0_IMAGIC(_L("CIEBgpsController::CheckOptimalFileForTnCreation++"));

    //TFileName tmpName;
    //iImageDataArray[aIndex]->GetFileName(tmpName, E512x512Thumbnail);
    if(iImageDataArray[aIndex]->IsImageReady(ESize512x512))
    //if(BaflUtils::FileExists(iFileServer, tmpName))
        {
        iImageDataArray[aIndex]->GetFileName(aFileName, ESize512x512);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename exits: %S"), &aFileName);
        }
    else//(iFileNameData[aIndex]->IsImageReady(EOriginalImage))
        {
        iImageDataArray[aIndex]->GetFileName(aFileName, EFullSize);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename exits: %S"), &aFileName);
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::CheckOptimalFileForTnCreation--")); 
    }

//Finds optimal size for 32x32 TN creation
void CIEBgpsController::CheckOptimalFileFor32x32TnCreation(TInt aIndex, TFileName& aFileName)
    {
    DP0_IMAGIC(_L("CIEBgpsController::CheckOptimalFileForTnCreation++"));
    if(iImageDataArray[aIndex]->IsImageReady(ESize128x128))
        {
        iImageDataArray[aIndex]->GetFileName(aFileName, ESize128x128);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename exits: %S"), &aFileName);
        }

/*    else if(iImageDataArray[aIndex]->IsImageReady(E128x96Thumbnail))
        {
        iImageDataArray[aIndex]->GetFileName(aFileName, E128x96Thumbnail);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename did not exits: %S"), &aFileName);
        }*/
    else if(iImageDataArray[aIndex]->IsImageReady(ESize512x512))

        {
        iImageDataArray[aIndex]->GetFileName(aFileName, ESize512x512);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename exits: %S"), &aFileName);
        }
    else//(iFileNameData[aIndex]->iMG_FileExist)
        {
        //DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename OK: %S"), &aFilename);
        iImageDataArray[aIndex]->GetFileName(aFileName, EFullSize);
        DP1_IMAGIC(_L("CIEBgpsController::CheckFileNmaesExitst - filename exits: %S"), &aFileName);
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::CheckOptimalFileForTnCreation--")); 
    }

void CIEBgpsController::Generate128x128Thumbnails(TInt aIndex)
    {
    DP0_IMAGIC(_L("CIEBgpsController::Generate128x128Thumbnails++"));
    //DP1_IMAGIC(_L("CIEBgpsController::Generate128x128Thumbnails - New TN to be created: %S"), &iImageDataArray[aIndex]->iIETN_128x128_Filename);
    iImageDataArray[aIndex]->GetFileName(iTNFilename, ESize128x128);
    iImageDataArray[aIndex]->GetFileName(iJpegFilename, EFullSize);
    CheckOptimalFileFor128x128TnCreation(aIndex,iJpegFilename);
#ifdef USE_64X64_BITMAP_TN
    TSize size(64,64);
#else
    TSize size(128,128);
#endif
    
    iIEBgpsClient->GenerateTN(iJpegFilename, iTNFilename, size);
    
    DP0_IMAGIC(_L("CIEBgpsController::Generate128x128Thumbnails--"));
    }

void CIEBgpsController::Generate32x32Thumbnails(TInt aIndex)
    {
    DP0_IMAGIC(_L("CIEBgpsController::Generate32x32Thumbnails++"));
//    DP1_IMAGIC(_L("CIEBgpsController::Generate320x320Thumbnails - New TN to be created: %S"), &iImageDataArray[aIndex]->iIETN_32x32_Filename);
    iImageDataArray[aIndex]->GetFileName(iTNFilename, ESize32x32);
    iImageDataArray[aIndex]->GetFileName(iJpegFilename, EFullSize);
    CheckOptimalFileFor32x32TnCreation(aIndex,iJpegFilename);
    TSize size(32,32);
    iIEBgpsClient->GenerateTN(iJpegFilename, iTNFilename, size);
    
    DP0_IMAGIC(_L("CIEBgpsController::Generate32x32Thumbnails--"));
    }

void CIEBgpsController::Generate512x512Thumbnails(TInt aIndex)
    {
    DP0_IMAGIC(_L("CIEBgpsController::Generate512x512Thumbnails++"));
    //DP1_IMAGIC(_L("CIEBgpsController::Generate640x480Thumbnails - New TN to be created: %S"), &iImageDataArray[aIndex]->iIETN_512x512_Filename);
    
    iImageDataArray[aIndex]->GetFileName(iTNFilename, ESize512x512); 
    iImageDataArray[aIndex]->GetFileName(iJpegFilename, EFullSize);
    TSize size(512,512);
    iIEBgpsClient->GenerateTN(iJpegFilename, iTNFilename, size);
    
    DP0_IMAGIC(_L("CIEBgpsController::Generate512x512Thumbnails--"));
    }


/*TReal CIEBgpsController::ReadAspectRatioL(TFileName& aFileName)
    {
    DP0_IMAGIC(_L("CIEBgpsController::ReadAspectRatio++"));

    CImageDecoder* imageDecoder = NULL;
    imageDecoder = CImageDecoder::FileNewL(iFileServer, aFileName);
 
    TFrameInfo frameInfo = imageDecoder->FrameInfo();
    TSize size = frameInfo.iFrameCoordsInPixels.Size();
    
    if(imageDecoder)
        {
        delete imageDecoder;
        imageDecoder = NULL;   
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::ReadAspectRatio--"));
    
    return (TReal)size.iWidth/(TReal)size.iHeight; 

    }*/

void CIEBgpsController::StartTNCreatorL()
    {
    DP0_IMAGIC(_L("CIEBgpsController::StartTNCreator++"));
    
    TThumbSize res;
    if((iTnCreationIndex = FindMissingTN(res)) < 0) {
        iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
        return;
    }
    
    //start creating thumbnails by calling TN creator
    //callback when one TN is created is done to CIEBgpsController::ThumbnailGenerationCompleted function
    if(res == ESize512x512)
        {
        i512x512TNCreationOn = ETrue;
        i128x128TNCreationOn = EFalse;
        i32x32TNCreationOn = EFalse;
        Generate512x512Thumbnails(iTnCreationIndex);
        }
    else if(res == ESize128x128)
        {
        i512x512TNCreationOn = EFalse;
        i128x128TNCreationOn = ETrue;
        i32x32TNCreationOn = EFalse;
        Generate128x128Thumbnails(iTnCreationIndex);
        }
    else if(res == ESize32x32)
        {
        i512x512TNCreationOn = EFalse;
        i128x128TNCreationOn = EFalse;
        i32x32TNCreationOn = ETrue;
        Generate32x32Thumbnails(iTnCreationIndex);
        }
    else
        {
        //All TNs were already done
        iIEBgpsControllerObserver.TNGenerationComplete(ENotDefined);
        //We complete face Detection here also because if there was no new images added -> no need to start face Detection in BGPS
        iIEBgpsControllerObserver.FaceDetectionComplete();
        }
    
    DP0_IMAGIC(_L("CIEBgpsController::StartTNCreator--"));
    }

//Returns index of first missing TN from any TN size
TInt CIEBgpsController::FindMissingTN(TThumbSize& aRes)
    {
    DP0_IMAGIC(_L("CIEBgpsController::FindMissingTN++"));
    
    TInt currentIndex = iIEBgpsControllerObserver.GetSelectedImageIndex();
    CImageData* gridData = iIEBgpsControllerObserver.GetImageData(currentIndex);
    
    TInt tnIndex = -1;
    
    for(TInt i = 0; i<iImageDataArray.Count(); i++)
        {
        // Check to positive and negative direction from current picture
        for (TInt j=0; j<2; j++)
            {
            // Calculate image index
            tnIndex = currentIndex + (j ? i : -i);
            
            // Check that index is valid
            if (tnIndex < 0 || tnIndex >= iImageDataArray.Count())
                continue;
            
        CImageData* imageData = iIEBgpsControllerObserver.GetImageData(tnIndex);
        
        //Mark image as corrupted so we do not try to generate TN or load it
        if(imageData->iGridData.iCorrupted)
            {
            tnIndex = -20/*KErrCorrupt*/;
            continue;
            }
    
        if(!iImageDataArray[tnIndex]->IsImageReady(ESize512x512))
            {
            TFileName filename;
            iImageDataArray[tnIndex]->GetFileName(filename,ESize512x512);
            DP1_IMAGIC(_L("CIEBgpsController::FindMissingTN - Filename to be crated: %S"), &filename);
            aRes = ESize512x512;
            //break;
            return tnIndex;
            }
        if(!iImageDataArray[tnIndex]->IsImageReady(ESize128x128))
            {
            TFileName filename;
            iImageDataArray[tnIndex]->GetFileName(filename,ESize128x128);
            DP1_IMAGIC(_L("CIEBgpsController::FindMissingTN - Filename to be crated: %S"), &filename);
            aRes = ESize128x128;
            //break;
            return tnIndex;
            }
        if(!iImageDataArray[tnIndex]->IsImageReady(ESize32x32))
            {
            TFileName filename;
            iImageDataArray[tnIndex]->GetFileName(filename,ESize32x32);
            DP1_IMAGIC(_L("CIEBgpsController::FindMissingTN - Filename to be crated: %S"), &filename);
            aRes = ESize32x32;
            //break;
            return tnIndex;
            }
        }
    }
    DP0_IMAGIC(_L("CIEBgpsController::FindMissingTN--"));
    return -1;    
    }

void CIEBgpsController::CancelTNGeneration()
    {
    DP0_IMAGIC(_L("CIEBgpsController::CancelTNGeneration"));
    
    iIEBgpsClient->CancelTNGeneration();    
    }


// EOF
