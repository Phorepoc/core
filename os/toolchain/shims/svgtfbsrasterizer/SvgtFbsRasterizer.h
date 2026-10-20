/*
* Copyright (c) 2002 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:   CSvgtFbsRasterizer class.
*
*/			

#ifndef SVGTFBSRASTERIZER_H
#define SVGTFBSRASTERIZER_H

#include "SvgtRegisteredBitmap.h"
#include "svgtgraphicsinterface.h"
#include <graphics/fbsrasterizer.h>
#include <graphics/fbsrasterizerclearcache.h>
#include "SvgtRasterizerCacheLimitHandler.h"

#include <e32base.h>
#include <f32file.h>
#include <VG/openvg.h>

const TUid KUidNvgProprietaryFormat = { 968435518 };
const TInt KMaxRecentBmpCacheSize = 0x40000;

class CRasterizerCacheItem;
class CRasterizedBitmapCache;
class MVGImageBinder;
class CSvgtRegisteredBitmap;
class CNvgEngine;
class TAknIconHeader;

TBool operator==(const CFbsRasterizer::TBitmapDesc& aBitmapDesc1, const CFbsRasterizer::TBitmapDesc& aBitmapDesc2);

class CSvgtRasterizerOOMPropertyMonitor;

/** Example implementation of a rasterizer that is used to generate pixel
data for extended bitmaps of example type KUidExtendedBitmapExample. 
 */
NONSHARABLE_CLASS (CSvgtFbsRasterizer) : public CFbsRasterizer, 
                                            public MFbsRasterizerClearCache,
                                            public MSvgtRasterizerCacheLimitHandler
    {
public:
    IMPORT_C static CFbsRasterizer* New();
    ~CSvgtFbsRasterizer();
    
    // From CFbsRasterizer
    void BeginBitmap(TInt64 aBitmapId, const TBitmapDesc& aBitmapDesc, const TRegion* aRegionOfInterest);
    void EndBitmap(TInt64 aBitmapId);
    const TUint32* ScanLine(TInt64 aBitmapId, const TPoint& aPixel, TInt aLength);
    TInt GetInterface(TUid aInterfaceId, TAny*& aInterface);

private:
    CSvgtFbsRasterizer();
    CSvgtRegisteredBitmap* RegisteredBitmap(TInt64 aBitmapId);
    CSvgtRegisteredBitmap* RecentBitmap(TInt64 aBitmapId);
    
    void DoBeginBitmapL(TInt64 aBitmapId, const TBitmapDesc& aBitmapDesc,
                        const TRegion* aRegionOfInterest);
    CSvgtRegisteredBitmap* DoCreateAndRegisterBitmapL(TInt64 aBitmapId, const TBitmapDesc& aBitmapDesc, CFbsBitmap * aPixmap);    
    void InitializeRasterizer();
    CRasterizerCacheItem* FetchFromCache( TInt64 aBitmapId );
    
    /*
     * RenderL retrieves data from the Extended Bitmap and passes the data to nvgdecoder  
     */
    void RenderL( const TBitmapDesc& aBitmapDesc, CSvgtRegisteredBitmap& aBitmap  );
    /*
     * Rendering of Bitmap
     */
    void RenderBitmapL(CSvgtRegisteredBitmap& aPixMap, CFbsBitmap * aMask, const TBitmapDesc& aBitmapDesc, TPtr8& aDataPtr8, TAknIconHeader& aIconHeader);
    /* 
     * Rendering of mask
     */
    void RenderMaskL(CSvgtRegisteredBitmap& aPixMap, CFbsBitmap * aMask, const TBitmapDesc& aBitmapDesc, TPtr8& aDataPtr8, TAknIconHeader& aIconHeader);

    /**
     * UpdateMatrices() 
     * locally stores the current environment parameters like paint,path handles,position matrices etc., 
     * so as to revert back (after operations done by nvgdecoder) so that the original position of the icons are intact
     */
    void UpdateMatrices();
   
    /**RestoreMatrices() reverts back the position matrices,path and paint handles which are locally stored using UpdateCurrentMatrices 
    so that the original positions of the icons are intact
    */            
    void RestoreMatrices();
    /**
     * ApplyMarginL applies MarginCorrection to icons which donot comply to the S60 width-height/aspect ratio restrictions
    */   
    TSize ApplyMarginL(VGImage aVgImage, TSize aSize);
        
    /**
     * Map OpenVg Error codes to symbian error codes
     */
    TInt MapOpenVgErrorCodeToSymbian(TInt aErrorCode);
    /*
     * Returns cache limit (if) specific to process
     */
    TInt GetCacheLimit(TUid aProcessUID) const;

    //From MFbsRasterizerClearCache
    virtual void ClearCache();
    
    //from MSvgtRasterizerCacheLimitHandler
    virtual void ChangeCacheLimit( TBool aChangeCacheLimit );

private: //Data members
    /** List of currently registered extended bitmaps, the key is the bitmap id.
     */
    TDblQue<CSvgtRegisteredBitmap> iRegisteredBmps;
    /** List of recently used extended bitmaps, the key is the bitmap id.
     */
    TDblQue<CSvgtRegisteredBitmap> iRecentBmps;
    /** Total size in bytes of the pre-rendered buffers for all the recently used extended bitmaps.
     */
    TInt iTotalRecentBmpSize;
    /** An idle-time active object to clean up the recently used extended bitmaps.
     */
    
    CSvgtGraphicsInterface * iGraphicsInterface;
    CSvgtRasterizerOOMPropertyMonitor* iMonitor;
         
    TBool iIsRasterizerValidState;
    TBool iMatricesUpdated;
    CNvgEngine* iNvgEngine;
    
    VGPaint iUserStrokePaint;
    VGPaint iUserFillPaint;
    VGint iMatrixMode;
    TReal32 iImageMatrix[9];
    TReal32 iPathMatrix[9];
    TReal32 iFillPaintMatrix[9];
    TReal32 iStrokePaintMatrix[9];
    TInt   iCacheLimit;

    // OURS (render-once): the colour icon and its mask are two extended bitmaps carrying the same NVG data; the
    // alpha plane of the colour render IS the mask. Keyed by a 64-bit hash of (NVG data, size, scale, rotation, margin).
    struct TAlphaEntry
        {
        TUint32 iKeyLo;
        TUint32 iKeyHi;
        TInt    iWidth;
        TInt    iHeight;
        HBufC8* iData;      // EGray256 bitmap rows, top to bottom, ScanLineLength(width, EGray256) stride
        };
    void ComputeAlphaKey(const TBitmapDesc& aDesc, TAknIconHeader& aHeader, TUint32& aLo, TUint32& aHi) const;
    TBool FillMaskFromCache(CSvgtRegisteredBitmap& aPixMap, const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi);
    void StoreAlphaL(const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi, const TUint8* aRows, TInt aStride);
    void ReadAlphaIntoCacheL(const TBitmapDesc& aDesc);
    void ClearAlphaCache();
    RPointerArray<TAlphaEntry> iAlpha;
    TInt   iAlphaBytes;
    TInt   iAlphaHits;
    TInt   iAlphaStores;
    TUint32 iAlphaKeyLo;    // key of the render in progress (set by RenderL)
    TUint32 iAlphaKeyHi;

    // OURS (persistent icon cache): rasterised icons survive on C:\private\<sid>\iconcache\, one file per icon
    // (header + checksum + pixels + alpha), LRU-evicted at 4 MB. A miss in RAM looks here before the software VG runs.
    struct TDiskEnt
        {
        TUint32 iLo;
        TUint32 iHi;
        TInt    iSize;
        };
    TBool DiskEnsure();
    void  DiskScan();
    void  DiskName(TDes& aName, TUint32 aLo, TUint32 aHi) const;
    TInt  DiskFind(TUint32 aLo, TUint32 aHi) const;
    void  DiskDrop(TInt aIndex, TBool aDeleteFile);
    TBool DiskLoad(CSvgtRegisteredBitmap& aBmp, const TBitmapDesc& aDesc);
    void  DiskStore(CSvgtRegisteredBitmap& aBmp, const TBitmapDesc& aDesc);
    TAlphaEntry* FindAlpha(const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi);
    void  LogDiskStats();
    void  DiskQueue(TInt aSize);
    void  DiskMaintain();
    static TInt DiskTimerCb(TAny* aSelf);
    RArray<TDiskEnt> iDiskPend;
    CPeriodic* iDiskTimer;
    RFs iFs;
    TBool iFsOpen;
    TBool iDiskDead;
    TBool iDiskScanned;
    RArray<TDiskEnt> iDiskIdx;  // oldest first
    TInt iDiskBytes;
    TInt iDiskHits;
    TInt iDiskMiss;
    TInt iDiskStores;
    TInt iDiskEvict;
    TInt iDiskBad;
    TUint32 iDiskHitMs;
    TUint32 iDiskStoreMs;
    TUint32 iDiskLo;        // disk key of the render in progress (0/0 = not cacheable)
    TUint32 iDiskHi;
    };

#endif // SVGTFBSRASTERIZER_H
