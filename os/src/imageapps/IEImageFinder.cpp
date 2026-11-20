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

#include <e32debug.h>
#include <eikenv.h>
#include <exifread.h>
#include <f32file.h>
#include "IEImageFinder.h"
#include "IEImageList.h"
#include "IEImageProcessing.h"
#include "IEImageData.h"
#include "IEEngineImp.h"
#include "IEEngineUtils.h"
#ifdef _S60_5x_ACCELEROMETER_
#include "IESensorMonitor.h"
#endif

#define LATETHUMBCHECK

// ================= MEMBER FUNCTIONS =======================

CIEImageFinder* CIEImageFinder::NewL(
        CIEFileLoader* aCallback, 
        RArray<CImageData*>& aFileNameData, 
        RArray<CImageData*>& aFaceFileNameData,
        RCriticalSection* aCritical)
    {
    CIEImageFinder* self = new(ELeave) CIEImageFinder(aCallback, aFileNameData, aFaceFileNameData, aCritical);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
    }

CIEImageFinder* CIEImageFinder::NewLC(
        CIEFileLoader* aCallback, 
        RArray<CImageData*>& aFileNameData, 
        RArray<CImageData*>& aFaceFileNameData, 
        RCriticalSection* aCritical)
    {
    CIEImageFinder* self = new(ELeave) CIEImageFinder(aCallback, aFileNameData, aFaceFileNameData, aCritical);
    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
    }

CIEImageFinder::CIEImageFinder(
        CIEFileLoader* aCallback, 
        RArray<CImageData*>& aFileNameData, 
        RArray<CImageData*>& aFaceFileNameData, 
        RCriticalSection* aCritical) :
        //iList(aFileNameData, aCallback),
        iCallback(aCallback),
        iFileNameData(aFileNameData),
        iFaceFileNameData(aFaceFileNameData),
        iCritical(aCritical),
        iIEEngineUtils(iFileServer)
    {
    DP0_IMAGIC(_L("CIEImageFinder::CIEImageFinder++ --"));
    }


void CIEImageFinder::ConstructL()
    {
    DP0_IMAGIC(_L("CIEImageFinder::ConstructL++"));
    
    User::LeaveIfError(iFileServer.Connect());
    
    iImageList = &iCallback->GetImageList();
    
    //iFileSystemMonitor = CFileSystemMonitorAO::NewL(iFileServer, this);
    
    // Creating Engine Utility class pointer */
        
    DP0_IMAGIC(_L("CIEImageFinder::ConstructL--"));
    }

CIEImageFinder::~CIEImageFinder()
    {
    DP0_IMAGIC(_L("CIEImageFinder::~CIEImageFinder++"));
  	
    iFileServer.Close();
  	
  	DP0_IMAGIC(_L("CIEImageFinder::~CIEImageFinder--"));
    }

TBool CIEImageFinder::IsSearching() const
    {
    return (iCallback->ImageFinderState() == CIEFileLoader::EImageFinderRunning);
    }

void CIEImageFinder::StartFinderL(const TDesC& aSearchName)
    {
    DP0_IMAGIC(_L("CIEImageFinder::StartFinderL++"));

#ifdef IMAGIC_DATABASE
    // Read initial list from database
    TRAP_IGNORE(iImageList->ReadDatabaseL());
#endif    
    if (IsSearching())
        SearchFilesL(aSearchName);
    
    DP0_IMAGIC(_L("CIEImageFinder::StartFinderL--"));
    }

void CIEImageFinder::SearchFilesL(const TDesC& aSearchName)
    {
    DP0_IMAGIC(_L("CIEImageFinder::SearchFilesL++"));
     
    // Use phone memory, external memory card and internal memory card for file scanning 
    TFileName rootPathPhoneMemory = PathInfo::PhoneMemoryRootPath();
    rootPathPhoneMemory.Append(ImagePath);
     
    TFileName rootPathMCard = PathInfo::MemoryCardRootPath();
    rootPathMCard.Append(ImagePath);
     
    TFileName rootPathFDrive;
    rootPathFDrive.Copy(KRootPathFDrive);
    rootPathFDrive.Append(ImagePath);
     
    //Append drives to array
    RArray<TFileName> drives;
    drives.Append(rootPathPhoneMemory);
    drives.Append(rootPathMCard);
    drives.Append(rootPathFDrive);

    DP0_IMAGIC(_L("CIEImageFinder::SearchFilesL start"));    
    
    for(TInt i=0; i<drives.Count() && IsSearching(); i++)
        {
        if(BaflUtils::PathExists(iFileServer, drives[i]))
            {
            DP2_IMAGIC(_L("CIEImageFinder::SearchFilesL  aRootPath = %S, aSearchName = %S ++"), &drives[i], &aSearchName);
             
            CDirScan* dirScan = CDirScan::NewL(iFileServer);
             
            TRAPD(err, dirScan->SetScanDataL(
                    drives[i], 
                    KEntryAttDir|KEntryAttMatchExclusive, 
                    EDirDescending/*|EDescending*/|ESortByDate, 
                    CDirScan::EScanUpTree));

            if(err == KErrNone)
                {
                CDir* dir;
                while(IsSearching())
                    {
                    TRAP(err, dirScan->NextL(dir));
                    if(err != KErrNone || dir == NULL)
                        {
                        if (dir)
                            delete dir;
                        break;
                        }
                  
                    TRAP(err, ScanDirL(dir, dirScan->FullPath(), aSearchName));
                    delete dir;
                    if (err != KErrNone)
                        break;
                    }
                }
          
            delete dirScan;
            }
        }
    
#ifdef IMAGIC_DATABASE      
        // Remove files from deleted directories
    if (IsSearching())
        iImageList->RemoveNonExistImagesL(NULL, iFileServer);
#endif
    
#ifdef LATETHUMBCHECK    
    // Check all thumbnails
    for (TInt i = 0;i < iFileNameData.Count() && IsSearching();i++)
        {
        CheckCreatedThumbnails(*iFileNameData[i]);
        
        // Number of faces is unknown and thumbnail exists
        if (iFileNameData[i]->GetNumberOfFaces() < 0 &&
            iFileNameData[i]->IsImageReady(ESize512x512))
            {
            // Read number of faces from thumb's exif
            RArray<TRect> faceCoordinates;
            TFileName fileName;
            iFileNameData[i]->GetFileName(fileName, ESize512x512);
            TRAPD(error, iIEEngineUtils.ReadFaceCoordinatesL(fileName, faceCoordinates));
            if (error == KErrNone)
                {
                TInt count = faceCoordinates.Count();
                DP2_IMAGIC(_L("Read face# %d for %S"), count, &fileName);
                iFileNameData[i]->SetNumberOfFaces(count);
                iImageList->SetChanged(fileName);
                }
            }
        }
#endif    

    // Let engine know that all file has been added to filename array
    //iCallback->AllFilesAddedToFilenameArray();
         
    drives.Close();

#ifdef IMAGIC_DATABASE    
    // Write list to database
    TRAP_IGNORE(iImageList->WriteDatabaseL());
#endif    
    
    DP0_IMAGIC(_L("CIEImageFinder::SearchFilesL--"));
    }

void CIEImageFinder::ScanDirL(CDir* aDir, const TDesC& aDirPath, const TDesC& aWild)
    {
    DP1_IMAGIC(_L("CIEImageFinder::ScanDirL++ %S"), &aDirPath);
    
    TParse parse;
    parse.Set(aWild, &aDirPath, NULL);
    TPtrC spec(parse.FullName());
    
    /*if(aDirPath.Find(KCRootBgroundImages) != KErrNotFound)
        return;*/
     
    TFindFile findFile(iFileServer);
     
    if (findFile.FindWildByPath(parse.FullName(), NULL, aDir) == KErrNone)
        {
        // Sort in time order
        aDir->Sort(EDescending|ESortByDate);

        // Go through dir in inverted order
        for(TInt i = 0;i < aDir->Count() && IsSearching();i++)
            {
            parse.Set((*aDir)[i].iName, &spec, NULL);
               
            // Full filename with path of original image
            TFileName imageFileName;
            imageFileName = parse.FullName();
               
            RDebug::Printf("[imagic-find] file %S", &imageFileName);
            // Check if file exist and not be hidden
            TBool visible = EFalse;
            TRAPD(err, visible = iImageList->IsImageViewableL(imageFileName, iFileServer));
            if (err != KErrNone || !visible)
                {
                DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - not found or hidden file"));
                continue;
                }
            
            CImageData* imageData = iImageList->GetImageData(imageFileName);
            
            // Read file time
            TTime fileTime(0);
            TRAP(err, iIEEngineUtils.GetModifiedTimeL(imageFileName, fileTime));
            if (err != KErrNone)
                {
                DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - get file time failed"));

                // File is locked for writing, use old file time if possible
                if (imageData == NULL)
                    continue;
                else
                    fileTime = imageData->GetFileTime();
                }
            
            // Image already exist in list
            if (imageData != NULL)
                {
                DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - image exist"));
                // File has been changed
                if (imageData->GetFileTime() != fileTime)
                    {
                    DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - file time is different"));
                    // Delete thumbnails and remove image from the list
                    iIEEngineUtils.DeleteThumbnails(imageFileName, iFileServer);
                    iImageList->Remove(iImageList->GetImageIndex(imageData), iFileServer);
                    imageData = NULL;
                    }
                }

            // Image already exist
            if (imageData == NULL)
                {
                // Read aspect ratio
                TSize size;
                TRAP(err, iIEEngineUtils.GetImageSizeL(imageFileName, size));
                if (err != KErrNone)
                    {
                    RDebug::Printf("[imagic-find] size failed %d", err);
                    DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - get size failed"));
                    continue;
                    }

                // Read EXIF created time (use file time if EXIF fails)
                TTime createdTime;
                TUint16 orientation = 0;
                TRAP(err, iIEEngineUtils.GetExifDateTimeAndOrientationL(
                        imageFileName, 
                        createdTime, 
                        orientation));
                
                if (err != KErrNone) 
                    {
                    DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - No EXIF date"));
                    createdTime = fileTime;
                    }
                  
                // Add image to list
                DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - Call AddImageL"));
                TRAP(err, imageData = iImageList->CreateImageDataL(imageFileName, createdTime, orientation));
                RDebug::Printf("[imagic-find] CreateImageDataL err %d size %dx%d orientation %d", err, size.iWidth, size.iHeight, orientation);
                DP1_IMAGIC(_L("CIEImageFinder::ScanDirL - AddImageL returned: err:% d"), err);
                
                if (err != KErrNone)
                    {
                    imageData = NULL;
                    }
                else if (imageData) 
				    {
                    imageData->SetFileTime(fileTime);
                    imageData->SetSize(size);
                    iImageList->AddImage(imageData);
                    }
                iImageList->SetChanged(imageFileName);
                }
            
            DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - 03"));
            // Mark original image as ready
            if (imageData)
                {
                DP0_IMAGIC(_L("CIEImageFinder::ScanDirL - 04"));
                imageData->SetImageReady(EFullSize, ETrue);
                }
            }
        
            delete aDir;
        }
    
#ifdef IMAGIC_DATABASE    
    //RemoveDeletedImagesL(*array, &aDirPath);
#endif    
    
    DP0_IMAGIC(_L("CIEImageFinder::ScanDirL--"));
    }

void CIEImageFinder::CheckCreatedThumbnails(CImageData& aImageData) const
    {
    TThumbSize sizes[] = { /*EFullSize,*/ ESize512x512, ESize128x128, ESize32x32 };
    
    TInt numSizes = sizeof(sizes) / sizeof(TThumbSize);
    for (TInt i = 0;i < numSizes; i++)
        {
        TFileName fileName;
        aImageData.GetFileName(fileName, sizes[i]);
        if (BaflUtils::FileExists(iFileServer, fileName))
            aImageData.SetImageReady(sizes[i], ETrue);
        else
            // Mark all smaller sizes non-exist
            //while (i < numSizes)
                {
                aImageData.SetImageReady(sizes[i], EFalse);
                //i++;
                }
        }    
    }

void CIEImageFinder::FileSystemChanged()
    {
    //Scan filesystem for new images
    //SearchForNewFilesL(KRootImagePath, KFileString);
    
#ifdef __WINS__
    SearchFilesL(KFileString);
#else
    TFileName rootPath = PathInfo::MemoryCardRootPath();
    rootPath.Append(ImagePath);
    SearchFilesL(KFileString);
#endif
    
    }
