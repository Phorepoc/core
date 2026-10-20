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
* Description:    CSvgtFbsRasterizer implementation.
*
*/

#include "SvgtFbsRasterizer.h"
#include "fbsrasterizerpanics.h"
#include "SvgtRegisteredBitmap.h"
#include "svgtgraphicsinterface.h"
#include <e32std.h>
#include <nvg.h>
#include <AknIconHeader.h>
#include <AknIconUtils.h>
#include <vgcontext.h>
#include <e32cmn.h>
#include <e32debug.h>
#include <bitstd.h>
#include <bitdev.h>

#include "SvgtRasterizerOOMPropertyMonitor.h"

TBool operator==(const CFbsRasterizer::TBitmapDesc& aBitmapDesc1, 
				 const CFbsRasterizer::TBitmapDesc& aBitmapDesc2)
	{
	return ((&aBitmapDesc1 == &aBitmapDesc2)
			|| (aBitmapDesc1.iSizeInPixels == aBitmapDesc2.iSizeInPixels
				&& aBitmapDesc1.iDispMode == aBitmapDesc2.iDispMode
				&& aBitmapDesc1.iDataType == aBitmapDesc2.iDataType
				&& aBitmapDesc1.iData == aBitmapDesc2.iData
				&& aBitmapDesc1.iDataSize == aBitmapDesc2.iDataSize));	
	}

void CleanupVGImage( TAny* aObj )
    {
    VGImage * imageToDestroy = reinterpret_cast<VGImage *>(aObj); 
    if (imageToDestroy && *imageToDestroy != VG_INVALID_HANDLE)
        {
        vgDestroyImage(*imageToDestroy);
        }
    }

/** Create a new rasterizer.
@return A pointer to a newly constructed CFbsRasterizer object if successful,
or NULL if no memory is available.
 */
EXPORT_C CFbsRasterizer* CSvgtFbsRasterizer::New()
	{
    if(!User::TrapHandler())
        {
        return NULL;
        }
	CSvgtFbsRasterizer* self = new CSvgtFbsRasterizer;
	if ( self )
	    {
	    self->InitializeRasterizer();
	    }
	return self;
	}

/** Constructor */
CSvgtFbsRasterizer::CSvgtFbsRasterizer()
:iRegisteredBmps(_FOFF(CSvgtRegisteredBitmap, iLink)), iRecentBmps(_FOFF(CSvgtRegisteredBitmap, iLink))
	            {

	            }

CSvgtFbsRasterizer::~CSvgtFbsRasterizer()
    {
    while (!iRegisteredBmps.IsEmpty())
        {
        delete iRegisteredBmps.First();
        }
    while (!iRecentBmps.IsEmpty())
        {
        delete iRecentBmps.First();
        }
    
    ClearAlphaCache();
    iAlpha.Close();
    iDiskIdx.Close();
    iDiskPend.Close();
    delete iDiskTimer;
    if (iFsOpen)
        {
        iFs.Close();
        }
    delete iNvgEngine;
    delete iGraphicsInterface;
    if ( iMatricesUpdated )
        {
        RestoreMatrices();
        }
    delete iMonitor;//OOM
    }

void CSvgtFbsRasterizer::ChangeCacheLimit( TBool aChangeCacheLimit )//OOM
    {
    iCacheLimit = KMaxRecentBmpCacheSize;

    if(aChangeCacheLimit)
        {
        RProcess currentProcess;
        iCacheLimit = GetCacheLimit(currentProcess.SecureId());
        }
    }

/** Register a bitmap with this rasterizer for rendering. In this example the extended 
bitmap data is just a buffer that is copied when BeginBitmap() is called and deleted when
EndBitmap() is called for the same bitmap. The only display modes supported by this example
raterizer are EColor16MU and EColor16MA
*/
void CSvgtFbsRasterizer::BeginBitmap(TInt64 aBitmapId, const TBitmapDesc& aBitmapDesc, const TRegion* aRegionOfInterest)
	{
	TRAPD(beginErr, DoBeginBitmapL(aBitmapId, aBitmapDesc, aRegionOfInterest));
	// If there is error in begin, the consecutive fulction calls should be invalid.
	iIsRasterizerValidState = ( beginErr == KErrNone); 
	}

void CSvgtFbsRasterizer::DoBeginBitmapL(TInt64 aBitmapId, const TBitmapDesc& aBitmapDesc,
        const TRegion* /* aRegionOfInterest */)
    {
    // Check that the passed extended bitmap description is valid
    if ((aBitmapDesc.iSizeInPixels.iWidth <= 0) || (aBitmapDesc.iSizeInPixels.iHeight <= 0)
            || (aBitmapDesc.iDataType != KUidNvgProprietaryFormat)
            || (!aBitmapDesc.iData )
            || (aBitmapDesc.iDataSize <= 0))
        {
        return;
        }
    // Check if the bitmap is already registered
    CSvgtRegisteredBitmap* foundRegBmp = RegisteredBitmap(aBitmapId);
    if (!foundRegBmp)
        {
        // Not registered: check if the bitmap is in the cache of recently used bitmaps
        foundRegBmp = RecentBitmap(aBitmapId);
        if (foundRegBmp)
            {
            // Take the bitmap out of the cache and put it in the list of registered bitmaps
            foundRegBmp->iLink.Deque();
            iTotalRecentBmpSize -= foundRegBmp->DataSize();
            iRegisteredBmps.AddLast(*foundRegBmp);
            }
        }
    if (foundRegBmp)
        {
        // This bitmap is already registered, just increment its reference count and return
        foundRegBmp->iRefCount++;
        return;
        }
    
    CSvgtRegisteredBitmap* regBmp = new CSvgtRegisteredBitmap(aBitmapId);
    if (!regBmp)
        {
        return;
        }
    // OURS (freeze diagnosis, bug-backlog "Applications grid"): the software OpenVG renders HERE, in the drawing
    // process (WSERV), on every cache miss -- log the slow ones / failures so a device run shows who rasterises what.
    const TUint32 renderStart = User::NTickCount();
    TRAPD(err, RenderL(aBitmapDesc, *regBmp));
    const TUint32 renderMs = User::NTickCount() - renderStart;
    if (err != KErrNone || renderMs >= 20)
        {
        RDebug::Printf("PH: [raster] sid=%08x miss %dx%d mode %d nvg=%d bytes ms=%u err=%d recent=%d/%d",
                       (TInt)RProcess().SecureId().iId, aBitmapDesc.iSizeInPixels.iWidth,
                       aBitmapDesc.iSizeInPixels.iHeight, (TInt)aBitmapDesc.iDispMode, aBitmapDesc.iDataSize,
                       (TUint)renderMs, err, iTotalRecentBmpSize, iCacheLimit);
        RDebug::Printf("PH: [raster] alpha cache hits=%d stores=%d entries=%d bytes=%d",
                       iAlphaHits, iAlphaStores, iAlpha.Count(), iAlphaBytes);
        LogDiskStats();
        }
    if (err != KErrNone)
        {
        delete regBmp;
        return;
        }
    // Success
    iRegisteredBmps.AddLast(*regBmp);
    regBmp->iRefCount = 1;
    }

/** Unregister an extended bitmap from this rasterizer. 
@see CFbsRasterizer::EndBitmap()
@see BeginBitmap()
 */
void CSvgtFbsRasterizer::EndBitmap(TInt64 aBitmapId)
	{
	CSvgtRegisteredBitmap* regBmp = RegisteredBitmap(aBitmapId);
	    if (regBmp)
	        {
	        if (--regBmp->iRefCount == 0)
	            {
            // Put unregistered bitmap in the cache of recently used bitmaps if wholly pre-rendered
            // and there is an active scheduler to add the idle-time clean-up active object to

            regBmp->iLink.Deque();
            if (regBmp->DataSize() <= iCacheLimit)
                {
                iRecentBmps.AddFirst(*regBmp);
                iTotalRecentBmpSize += regBmp->DataSize();
                }
            else
                {
                delete regBmp; // deleting the registered bitmap in a case when raster bitmap size is bigger than cache
                }	
                
            // Delete the least recently used bitmaps if the maximum size of the cache is exceeded
            while (iTotalRecentBmpSize > iCacheLimit)
                {
                regBmp = iRecentBmps.Last();
                iTotalRecentBmpSize -= regBmp->DataSize();
                delete regBmp;
                }
            }
        }
    }

/** Return a scanline from the passed extended bitmap given it's bitmap id. 
 */
const TUint32* CSvgtFbsRasterizer::ScanLine(TInt64 aBitmapId, const TPoint& aPixel, TInt aLength)
	{
	CSvgtRegisteredBitmap* regBmp = RegisteredBitmap(aBitmapId);
    if (!regBmp)
        {
        return NULL;
        }
    if (!TRect(regBmp->SizeInPixels()).Contains(aPixel)
        || (aLength > (regBmp->SizeInPixels().iWidth - aPixel.iX)))
        {
        return NULL;
        }

    return PtrAdd(regBmp->DataAddress(), aPixel.iY * regBmp->DataStride());
	}

/**  No extension interaces are available, KErrNotSupported for all aInterfaceId passed.
@see CFbsRasterizer::GetInterface()
 */
TInt CSvgtFbsRasterizer::GetInterface(TUid aInterfaceId, TAny*& aInterface)
    {
    aInterface = NULL;
    TInt err = KErrNone;

    switch (aInterfaceId.iUid)
        {
        case KUidFbsRasterizerClearCache:
            aInterface = static_cast<MFbsRasterizerClearCache*>(this);
            break;
        case KUidEnableSvgtRasterizer:
            aInterface = static_cast<MSvgtRasterizerCacheLimitHandler*>(this);
            break;
        default:
            err = KErrExtensionNotSupported;                        
            break;
        }

    return err;


    }


/** Gets a bitmap that has been registered with this rasterizer.

@param aBitmapId The ID of the bitmap to check for.

@return A pointer to the registered bitmap if found, NULL otherwise.
 */
CSvgtRegisteredBitmap* CSvgtFbsRasterizer::RegisteredBitmap(TInt64 aBitmapId)
    {
    TDblQueIter<CSvgtRegisteredBitmap> iter(iRegisteredBmps);
    while (CSvgtRegisteredBitmap* regBmp = iter++)
        {
        if (regBmp->iBitmapId == aBitmapId)
            {
            return regBmp;
            }
        }
    return NULL;
    }

/** Gets a bitmap that has been recently used with this rasterizer.

@param aBitmapId The ID of the bitmap to check for.

@return A pointer to the recently used bitmap if found, NULL otherwise.
 */
CSvgtRegisteredBitmap* CSvgtFbsRasterizer::RecentBitmap(TInt64 aBitmapId)
    {
    TDblQueIter<CSvgtRegisteredBitmap> iter(iRecentBmps);
    while (CSvgtRegisteredBitmap* regBmp = iter++)
        {
        if (regBmp->iBitmapId == aBitmapId)
            {
            return regBmp;
            }
        }
    return NULL;
    }

void CSvgtFbsRasterizer::InitializeRasterizer()
    {
    TBool cacheEnabled=TRUE;
    RProperty::Get(KSvgtPropertyCategory, ESvgtPropertyBool, cacheEnabled);
    iCacheLimit = KMaxRecentBmpCacheSize;
    if(cacheEnabled)
        {
        RProcess currentProcess;
        iCacheLimit = GetCacheLimit(currentProcess.SecureId());
        }
#ifdef PHOREPOC_RASTER_GPU
    // OURS (openvg-plan O2/O3): switch the GPU OpenVG engine (phvg) on for THIS process's VG work. The reference
    // implementation behind libopenvg_sw reads an int property KPhvgModeKey (0x50564731) in the process's own category
    // (here: WSERV, 0x10003B20) -- only that process may define it. A value somebody already set wins. Default OFF
    // (build with defs fbsrasterizer:PHOREPOC_RASTER_GPU to enable); phvg.dll missing -> the RI stays in charge.
        {
        const TUid cat = TUid::Uid((TInt)RProcess().SecureId().iId);
        TInt cur = 0;
        if (cat.iUid == 0x10003B20 && RProperty::Get(cat, 0x50564731, cur) == KErrNotFound)
            {
            TInt r = RProperty::Define(cat, 0x50564731, RProperty::EInt);
            if (r == KErrNone || r == KErrAlreadyExists)
                r = RProperty::Set(cat, 0x50564731, 1);
            RDebug::Printf("PH: [raster] phvg GPU mode requested for sid=%08x: %d", cat.iUid, r);
            }
        }
#endif
    TRAPD(err, iMonitor = CSvgtRasterizerOOMPropertyMonitor::NewL());
    iIsRasterizerValidState = ( err == KErrNone);
    }

void CSvgtFbsRasterizer::RenderBitmapL(CSvgtRegisteredBitmap& aPixMap, CFbsBitmap * aMask, 
                                        const TBitmapDesc& aBitmapDesc, TPtr8& aDataPtr8, TAknIconHeader& aIconHeader)
    {
    TSize newSize = aBitmapDesc.iSizeInPixels;
    UpdateMatrices();
    iMatricesUpdated = ETrue;

    TBool isMargin = aIconHeader.IsMarginCorrection();
    
    VGImage vgImage = VG_INVALID_HANDLE;
        
    if (isMargin)
        {
        vgImage = vgCreateImage((VGImageFormat)  VG_sRGBA_8888_PRE , aBitmapDesc.iSizeInPixels.iWidth,
                                    aBitmapDesc.iSizeInPixels.iHeight, VG_IMAGE_QUALITY_NONANTIALIASED);
        if (vgImage == VG_INVALID_HANDLE)
            {
            User::LeaveIfError(MapOpenVgErrorCodeToSymbian(vgGetError()));
            }
        CleanupStack::PushL( TCleanupItem( CleanupVGImage, &vgImage ) );
        iGraphicsInterface->BindClientBuffer(vgImage);
        }
    User::LeaveIfError(iNvgEngine->DrawNvg(aDataPtr8, newSize, &aPixMap, aMask));
   
    if(isMargin)
        {
        iGraphicsInterface->UnBindClientBuffer(); 
        newSize = ApplyMarginL(vgImage, aBitmapDesc.iSizeInPixels);
        if(newSize!=aBitmapDesc.iSizeInPixels)
        		{
        		User::LeaveIfError(iNvgEngine->DrawNvg(aDataPtr8, newSize, &aPixMap, aMask));
       		  }
       	else
       		  {
       		  vgDrawImage(vgImage);
       		  User::LeaveIfError(MapOpenVgErrorCodeToSymbian(vgGetError()));
            }
        	
        CleanupStack::PopAndDestroy();
        }
    // OURS (render-once): the alpha plane of this render is the mask the caller asks for next -- keep it
    TRAPD(alphaErr, ReadAlphaIntoCacheL(aBitmapDesc));
    (void)alphaErr; // a failed cache fill only costs the second rasterise
    iGraphicsInterface->CopyBitmapL(&aPixMap, aMask);
    
    TUint32 iconColor = aIconHeader.GetIconColor();
    
    if (iconColor & 0xFFFFFF)
        {
        CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL( &aPixMap );
        CleanupStack::PushL( dev );
        CFbsBitGc* gc = NULL;
        User::LeaveIfError( dev->CreateContext( gc ) );
        CleanupStack::PushL( gc );
        TRgb color(iconColor);          
        gc->SetBrushColor( color );
        gc->SetPenStyle( CGraphicsContext::ENullPen );
        gc->SetBrushStyle( CGraphicsContext::ESolidBrush );
        // Fill icon with the given color, mask defines the icon shape.
        TPoint origin(0, 0);
        if (newSize != aBitmapDesc.iSizeInPixels)
            {
            origin = TPoint((aBitmapDesc.iSizeInPixels.iWidth - newSize.iWidth)/2 , (aBitmapDesc.iSizeInPixels.iHeight - newSize.iHeight)/2  );
            }
#ifdef PHOREPOC_RASTER_TRACE
        // (diagnostic: X32_DEFS=fbsrasterizer:PHOREPOC_RASTER_TRACE -- the colour-icon fill, missing-pieces.md "Open")
        TRgb before;
        aPixMap.GetPixel(before, TPoint(newSize.iWidth / 2, newSize.iHeight / 2) + origin);
#endif
        gc->DrawRect( TRect( origin, newSize ) );
#ifdef PHOREPOC_RASTER_TRACE
        TRgb after;
        aPixMap.GetPixel(after, TPoint(newSize.iWidth / 2, newSize.iHeight / 2) + origin);
        RDebug::Printf("PH: [raster] colour icon %06x size %dx%d (bitmap %dx%d, mode %d) mask %08x: centre %08x -> %08x",
                       iconColor & 0xFFFFFF, newSize.iWidth, newSize.iHeight, aBitmapDesc.iSizeInPixels.iWidth,
                       aBitmapDesc.iSizeInPixels.iHeight, aPixMap.DisplayMode(), aMask, before.Internal(), after.Internal());
#endif
        CleanupStack::PopAndDestroy( 2 ); // dev, gc
        }
    }

void CSvgtFbsRasterizer::RenderMaskL(CSvgtRegisteredBitmap& aPixMap, CFbsBitmap * aMask, 
                                      const TBitmapDesc& aBitmapDesc, TPtr8& aDataPtr8, TAknIconHeader& aIconHeader)
    {
    UpdateMatrices();
    iMatricesUpdated = ETrue;
    TBool isMargin = aIconHeader.IsMarginCorrection();
    
    VGImage vgImage = VG_INVALID_HANDLE;
    if (isMargin)
        {
        vgImage = vgCreateImage((VGImageFormat)  VG_sRGBA_8888_PRE , aBitmapDesc.iSizeInPixels.iWidth, 
                                     aBitmapDesc.iSizeInPixels.iHeight, VG_IMAGE_QUALITY_NONANTIALIASED);
        if (vgImage == VG_INVALID_HANDLE)
            {
            User::LeaveIfError(MapOpenVgErrorCodeToSymbian(vgGetError()));
            }
        CleanupStack::PushL( TCleanupItem( CleanupVGImage, &vgImage ) );
        iGraphicsInterface->BindClientBuffer(vgImage);
        }
        
    VGfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    vgSeti(VG_SCISSORING, VG_FALSE);
    vgSetfv(VG_CLEAR_COLOR, 4, color);
    vgClear(0, 0, aBitmapDesc.iSizeInPixels.iWidth, aBitmapDesc.iSizeInPixels.iHeight);

    User::LeaveIfError(iNvgEngine->DrawNvg(aDataPtr8, aBitmapDesc.iSizeInPixels, &aPixMap, aMask));
    if (isMargin)
        {
        iGraphicsInterface->UnBindClientBuffer();
        TSize newSize = ApplyMarginL(vgImage, aBitmapDesc.iSizeInPixels);
        if(newSize!=aBitmapDesc.iSizeInPixels)
        		{
        		User::LeaveIfError(iNvgEngine->DrawNvg(aDataPtr8, newSize, &aPixMap, aMask));
       		  }
       	else
       		  {
       		  vgDrawImage(vgImage);
       		  User::LeaveIfError(MapOpenVgErrorCodeToSymbian(vgGetError()));
            }
        CleanupStack::PopAndDestroy();
        }
    
    TSize bitmapSize = aPixMap.SizeInPixels();

    const TInt KOriginalFilterMasks = vgGeti( VG_FILTER_CHANNEL_MASK );
    vgSeti( VG_FILTER_CHANNEL_MASK, VG_ALPHA );

    const TInt KStride = CFbsBitmap::ScanLineLength(bitmapSize.iWidth, aPixMap.DisplayMode());
    VGImageFormat format = ( aPixMap.DisplayMode() == EGray256 ) ? VG_A_8 : VG_BW_1;
    
    aPixMap.BeginDataAccess();
    TUint * data = (TUint*)((TUint)aPixMap.DataAddress() + ( KStride * ( aPixMap.SizeInPixels().iHeight - 1  ) ) );
    vgReadPixels( data, -KStride, format, 0, 0, bitmapSize.iWidth, bitmapSize.iHeight );
#ifdef PHOREPOC_RASTER_TRACE
    if (aIconHeader.GetIconColor() & 0xFFFFFF)
        {
        const TUint8* m = (const TUint8*)aPixMap.DataAddress();
        TInt set = 0, total = KStride * bitmapSize.iHeight;
        for (TInt i = 0; i < total; ++i)
            set += m[i] != 0;
        RDebug::Printf("PH: [raster] colour-icon mask %dx%d mode %d stride %d: %d/%d bytes set", bitmapSize.iWidth,
                       bitmapSize.iHeight, aPixMap.DisplayMode(), KStride, set, total);
        }
#endif
    if (aPixMap.DisplayMode() == EGray256 && vgGetError() == VG_NO_ERROR)
        {
        // a repeated mask request for the same icon (size-equal slots) is a hit too
        TRAP_IGNORE(StoreAlphaL(aBitmapDesc, iAlphaKeyLo, iAlphaKeyHi, (const TUint8*)aPixMap.DataAddress(), KStride));
        }
    aPixMap.EndDataAccess();

    vgSeti( VG_FILTER_CHANNEL_MASK, KOriginalFilterMasks );

    }

void CSvgtFbsRasterizer::RenderL( const TBitmapDesc& aBitmapDesc, CSvgtRegisteredBitmap& aBitmap  )
    {
    
    iIsRasterizerValidState = EFalse;

    User::LeaveIfError(aBitmap.Create(aBitmapDesc.iSizeInPixels, aBitmapDesc.iDispMode));

    // OURS (render-once): parse the header first; a mask whose colour twin was just rasterised is a copy of its alpha
    iAlphaKeyLo = iAlphaKeyHi = 0;
    iDiskLo = iDiskHi = 0;
    if (aBitmapDesc.iDataSize > KIconHeaderLength)
        {
        TPtr8 hdrPtr((TUint8*)aBitmapDesc.iData, KIconHeaderLength, KIconHeaderLength);
        TAknIconHeader hdr(hdrPtr);
        ComputeAlphaKey(aBitmapDesc, hdr, iAlphaKeyLo, iAlphaKeyHi);
        if (hdr.IsMask() && aBitmapDesc.iDispMode == EGray256
            && FillMaskFromCache(aBitmap, aBitmapDesc, iAlphaKeyLo, iAlphaKeyHi))
            {
            iAlphaHits++;
            iIsRasterizerValidState = ETrue;
            return;
            }
        // OURS (persistent cache): the disk key = the alpha key + what else changes the final pixels (display mode,
        // mask-or-colour, the colour the icon is filled with). The NVG payload hash makes a changed skin/theme miss.
        const TUint32 extra[3] = { (TUint32)aBitmapDesc.iDispMode, (TUint32)hdr.IsMask(), (TUint32)hdr.GetIconColor() };
        TUint32 a = iAlphaKeyLo ^ 0x9E3779B9u, b = iAlphaKeyHi + 0x7F4A7C15u;
        for (TInt i = 0; i < 3; ++i)
            {
            a = (a ^ extra[i]) * 0x01000193u;
            b = (b ^ extra[i]) * 0x85EBCA6Bu;
            b ^= b >> 13;
            }
        iDiskLo = a;
        iDiskHi = b;
        if ((a || b) && DiskLoad(aBitmap, aBitmapDesc))
            {
            iIsRasterizerValidState = ETrue;
            return;
            }
        }
    
    if (!iGraphicsInterface)
        {
        iGraphicsInterface = CSvgtGraphicsInterface::CreateImplementationL();
        }
    
    iGraphicsInterface->InitializeL(aBitmapDesc.iSizeInPixels);
    iIsRasterizerValidState  = ETrue;

    TPtr8 bmpDataPtr8((TUint8*)aBitmapDesc.iData, aBitmapDesc.iDataSize);
    
    if (!iNvgEngine)
        {
        iNvgEngine = CNvgEngine::NewL();
        }
        
    iNvgEngine->SetVGImageBinder(iGraphicsInterface);
    
    TUint8 *temp = (TUint8 *)bmpDataPtr8.Ptr();
    TPtr8 IconHeaderPtr(temp, KIconHeaderLength, KIconHeaderLength);

    TAknIconHeader iconheader(IconHeaderPtr);
#ifdef PHOREPOC_RASTER_TRACE
    RDebug::Printf("PH: [raster] render %dx%d mode %d colour %08x ismask %d scale %d rot %d",
                   aBitmapDesc.iSizeInPixels.iWidth, aBitmapDesc.iSizeInPixels.iHeight, aBitmapDesc.iDispMode,
                   iconheader.GetIconColor(), iconheader.IsMask(), iconheader.GetScaleMode(), iconheader.GetRotation());
#endif
    //skipping header bytes
    temp = temp + KIconHeaderLength;
    TPtr8 newDataPtr8(temp, (bmpDataPtr8.MaxLength() - KIconHeaderLength), (bmpDataPtr8.MaxLength() - KIconHeaderLength));

    CFbsBitmap* mask = NULL;
    
    VGfloat color[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    vgSeti(VG_SCISSORING, VG_FALSE);
    vgSetfv(VG_CLEAR_COLOR, 4, color);
    vgClear(0, 0, aBitmapDesc.iSizeInPixels.iWidth, aBitmapDesc.iSizeInPixels.iHeight);

    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    vgLoadIdentity();

    vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    vgLoadIdentity();

    TInt rotAngle = iconheader.GetRotation();
    // setting the rotation angle
    iNvgEngine->Rotate(-rotAngle,  aBitmapDesc.iSizeInPixels.iWidth >> 1,aBitmapDesc.iSizeInPixels.iHeight >>1);
    
    //setting preserve aspect ratio
    TNvgAlignStatusType alignTypeValue = ENvgPreserveAspectRatio_XmidYmid;
    TNvgMeetOrSliceType meetOrSliceTypeValue = ENvgMeet;
    
    switch ( iconheader.GetScaleMode() )
        {
        case EAspectRatioPreserved: // fall through
            {
            // use default
            break;
            }
            // Ensures NVG content fully covers the area of the icon whilst preserving aspect ratio.
        case EAspectRatioPreservedSlice:
            {
            // alignTypeValue use default
            meetOrSliceTypeValue = ENvgSlice;
            break;
            } 
            /* EAspectRatioPreservedAndUnusedSpaceRemoved is mapped to the same values as EAspectRatioNotPreserved
             * because we already have a frame buffer with the dimensions that preserves the aspect ratio.
             * This mapping ensures that NVG engine does not calculate aspect ratio twice and potentially resulting in precision loss.*/
        case EAspectRatioPreservedAndUnusedSpaceRemoved:                        
        case EAspectRatioNotPreserved:
            {            
            alignTypeValue = ENvgPreserveAspectRatio_None;
            // meetOrSliceTypeValue use default
            break;
            }
        default:
            {
            User::Leave(KErrCorrupt);
            }
        }    
    iNvgEngine->SetPreserveAspectRatio(alignTypeValue, meetOrSliceTypeValue);

    iMatricesUpdated = EFalse;
    if (iconheader.IsMask())
        {
        RenderMaskL(aBitmap, mask, aBitmapDesc, newDataPtr8, iconheader);
        }
    else
        {
        RenderBitmapL(aBitmap, mask, aBitmapDesc, newDataPtr8, iconheader);
        }
    
    if ( iMatricesUpdated )
        {
        RestoreMatrices();
        iMatricesUpdated = EFalse;
        }
       iNvgEngine->ResetNvgState();
    if (iDiskLo || iDiskHi)
        {
        DiskStore(aBitmap, aBitmapDesc);
        }
    }

void CSvgtFbsRasterizer::UpdateMatrices()
    {
    iUserStrokePaint = vgGetPaint(VG_STROKE_PATH);
    
    iUserFillPaint = vgGetPaint(VG_FILL_PATH);
    
    iMatrixMode = vgGeti(VG_MATRIX_MODE);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    
    vgGetMatrix(iPathMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    
    vgGetMatrix(iImageMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
    
    vgGetMatrix(iFillPaintMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_STROKE_PAINT_TO_USER);
    
    vgGetMatrix(iStrokePaintMatrix);
    
    vgSeti(VG_MATRIX_MODE, iMatrixMode);
    
    }

void CSvgtFbsRasterizer::RestoreMatrices()
    {
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    
    vgLoadMatrix(iPathMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    
    vgLoadMatrix(iImageMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
    
    vgLoadMatrix(iFillPaintMatrix);
    
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_STROKE_PAINT_TO_USER);
    
    vgLoadMatrix(iStrokePaintMatrix);
    
    vgSeti(VG_MATRIX_MODE, iMatrixMode);
    
    vgSetPaint(iUserStrokePaint, VG_STROKE_PATH);
    
    vgSetPaint(iUserFillPaint, VG_FILL_PATH);
    }


TSize CSvgtFbsRasterizer::ApplyMarginL(VGImage aVgImage, TSize aSize)
    {
    HBufC8* buf = HBufC8::NewL(aSize.iWidth* sizeof(TUint32));
    TUint32* ptr = (TUint32*)(buf->Des()).Ptr();
    
    const TInt validMargin = aSize.iHeight * 12 / 100;
    
    const TInt Ha = aSize.iHeight;
    TInt hTa = 0;
    TInt hNT = 0;
    TInt C = 0;
    TInt hNTN = Ha - 2.0 * 0.12 * Ha;   
    TReal R = 1.0;
    TInt HaN = Ha;
    
    const TInt lastColumn = aSize.iHeight - 1;
    for ( TInt curRow = 0; curRow < validMargin; curRow++ )
        {
        const TInt y = (aSize.iHeight - 1) - curRow; // h - 1 is the last line
        
        vgGetImageSubData(aVgImage, ptr, sizeof(TUint)*aSize.iWidth, VG_sRGBA_8888_PRE, 0, y, aSize.iWidth, 1);
                                
        for ( TInt s = lastColumn; s >= 0; --s )
            {
            if ( ptr[s] & 0x000000FF ) 
                {
                hTa = curRow;
                hNT = Ha - 2 * hTa;
                C = 2 * hTa;
                R = ( ( (TReal)hNTN / (TReal)hNT ) > 1.0 ) ? 1 : (TReal)hNTN / (TReal)hNT;
                HaN = Ha * R - C * R + C;
                curRow = validMargin; // to exit the outer loop
                break; // to exit the inner
                }
            
            }
        }
    
    delete buf;
        
    if(aSize.iHeight > HaN)
        {
        
        VGfloat color[4] = { 1.0f, 1.0f, 1.0f, 0.0f }; 
        
        vgSetfv(VG_CLEAR_COLOR, 4, color);
        vgClear(0, 0, aSize.iWidth,aSize.iHeight);
        
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);   
        VGfloat Hr = (VGfloat)HaN/(aSize.iHeight);
        
        TInt WaN = aSize.iWidth*Hr;
        
        VGfloat Tx = (aSize.iHeight-HaN)/2;
        VGfloat Ty = (aSize.iWidth-WaN)/2;
        vgTranslate(Tx,Ty);
        
        return( TSize(HaN,WaN));    
        }
    return aSize;
    
    }



TInt CSvgtFbsRasterizer::MapOpenVgErrorCodeToSymbian(TInt aErrorCode)
    {
    switch( aErrorCode )
        {
        case VGI_OK:
            return KErrNone;
            
        case VGI_ERROR_OUT_OF_MEMORY:
            return KErrNoMemory;
            
        case VGI_ERROR_INVALID_ARGUMENTS:
            return KErrArgument;
            
        case VGI_ERROR_ALREADY_EXISTS:
            return KErrAlreadyExists;
            
        case VGI_ERROR_COLORSPACE_NOT_SUPPORTED:
            return KErrNotSupported;
            
        case VGI_ERROR_NOT_SUPPORTED:
            return KErrNotSupported;

        case VGI_ERROR_ILLEGAL_IMAGE_HANDLE:
            return KErrBadHandle;

        case VGI_ERROR_IMAGE_IN_USE:
            return KErrInUse;

        case VGI_ERROR_ILLEGAL_OPERATION:
            return KErrPermissionDenied;

        default:
            return KErrUnknown;
        }

    }

TInt CSvgtFbsRasterizer::GetCacheLimit(TUid aProcessUID) const
{
    TInt cacheLimit = KMaxRecentBmpCacheSize;
    
    RProcess currentProcess;
    
    if(aProcessUID == TUid::Uid(0x10003B20)) // Alf 
        {
        cacheLimit = 0x600000; 
        }
    else if(aProcessUID == TUid::Uid(0x10281855)) // Peninputserver
        {
        cacheLimit = 0x133333;
        }
    
    return cacheLimit;
}
void CSvgtFbsRasterizer::ClearCache()//OOM 
    {
    ClearAlphaCache();
    CSvgtRegisteredBitmap* regBmp=NULL;
    if(iCacheLimit>KMaxRecentBmpCacheSize)
        { 
        while (!iRecentBmps.IsEmpty()&&(iTotalRecentBmpSize > KMaxRecentBmpCacheSize))
            {
            regBmp = iRecentBmps.Last();
            iTotalRecentBmpSize -= regBmp->DataSize();
            delete regBmp;
            }
        }
    }


// ---- OURS: render-once alpha cache ------------------------------------------------------------------------------
// The icon server hands the rasteriser TWO extended bitmaps per NVG icon (colour + mask), same NVG data, same size.
// The software VG renders the icon at ~30-170 ms each (WSERV's single thread), so the second render is pure waste: the mask
// is the alpha plane of the colour render (RenderMaskL clears to alpha 0 and draws the same NVG; RenderBitmapL clears to
// (1,1,1,0)). So: after a colour render read the alpha plane (VG_A_8, exactly what RenderMaskL reads) into this cache,
// and answer the mask request from it. Only EGray256 masks are served (the BW_1 case still renders).
static const TInt KAlphaCacheMaxBytes = 256 * 1024;
static const TInt KAlphaCacheMaxEntries = 48;

void CSvgtFbsRasterizer::ComputeAlphaKey(const TBitmapDesc& aDesc, TAknIconHeader& aHeader, TUint32& aLo, TUint32& aHi) const
    {
    // two 32-bit hash lanes over the NVG payload (past the icon header, which differs between the colour and the
    // mask request) + everything that changes the pixels
    TUint32 a = 0x811C9DC5u, b = 0x01000193u ^ 0xA5A5A5A5u;
    const TUint8* p = (const TUint8*)aDesc.iData + KIconHeaderLength;
    const TInt n = aDesc.iDataSize - KIconHeaderLength;
    for (TInt i = 0; i < n; ++i)
        {
        a = (a ^ p[i]) * 0x01000193u;
        b = (b ^ p[i]) * 0x85EBCA6Bu;
        b ^= b >> 15;
        }
    const TInt extra[6] = { aDesc.iSizeInPixels.iWidth, aDesc.iSizeInPixels.iHeight, n,
                            (TInt)aHeader.GetScaleMode(), (TInt)aHeader.GetRotation(), (TInt)aHeader.IsMarginCorrection() };
    for (TInt i = 0; i < 6; ++i)
        {
        a = (a ^ (TUint32)extra[i]) * 0x01000193u;
        b = (b ^ (TUint32)extra[i]) * 0x85EBCA6Bu;
        b ^= b >> 15;
        }
    aLo = a;
    aHi = b;
    }

TBool CSvgtFbsRasterizer::FillMaskFromCache(CSvgtRegisteredBitmap& aPixMap, const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi)
    {
    for (TInt i = iAlpha.Count() - 1; i >= 0; --i)
        {
        TAlphaEntry* e = iAlpha[i];
        if (e->iKeyLo != aLo || e->iKeyHi != aHi || e->iWidth != aDesc.iSizeInPixels.iWidth
            || e->iHeight != aDesc.iSizeInPixels.iHeight)
            continue;
        const TInt stride = CFbsBitmap::ScanLineLength(e->iWidth, EGray256);
        if (e->iData->Length() != stride * e->iHeight || aPixMap.SizeInPixels() != aDesc.iSizeInPixels)
            return EFalse;
        aPixMap.BeginDataAccess();
        Mem::Copy(aPixMap.DataAddress(), e->iData->Ptr(), e->iData->Length());
        aPixMap.EndDataAccess();
        // most recently used last
        iAlpha.Remove(i);
        iAlpha.Append(e); // cannot fail: the slot was just freed
        return ETrue;
        }
    return EFalse;
    }

void CSvgtFbsRasterizer::StoreAlphaL(const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi, const TUint8* aRows, TInt aStride)
    {
    const TInt bytes = aStride * aDesc.iSizeInPixels.iHeight;
    if (bytes <= 0 || bytes > KAlphaCacheMaxBytes / 2 || (aLo == 0 && aHi == 0))
        return;
    for (TInt i = 0; i < iAlpha.Count(); ++i)
        {
        if (iAlpha[i]->iKeyLo == aLo && iAlpha[i]->iKeyHi == aHi && iAlpha[i]->iWidth == aDesc.iSizeInPixels.iWidth
            && iAlpha[i]->iHeight == aDesc.iSizeInPixels.iHeight)
            return;
        }
    while (iAlpha.Count() && (iAlphaBytes + bytes > KAlphaCacheMaxBytes || iAlpha.Count() >= KAlphaCacheMaxEntries))
        {
        TAlphaEntry* old = iAlpha[0];
        iAlpha.Remove(0);
        iAlphaBytes -= old->iData->Length();
        delete old->iData;
        delete old;
        }
    HBufC8* data = HBufC8::NewL(bytes);
    TAlphaEntry* e = new TAlphaEntry;
    if (!e)
        {
        delete data;
        User::Leave(KErrNoMemory);
        }
    data->Des().Copy(aRows, bytes);
    e->iKeyLo = aLo;
    e->iKeyHi = aHi;
    e->iWidth = aDesc.iSizeInPixels.iWidth;
    e->iHeight = aDesc.iSizeInPixels.iHeight;
    e->iData = data;
    if (iAlpha.Append(e) != KErrNone)
        {
        delete data;
        delete e;
        User::Leave(KErrNoMemory);
        }
    iAlphaBytes += bytes;
    iAlphaStores++;
    }

// read the alpha plane of the current VG surface (the render just done) into the cache, in EGray256 bitmap layout
void CSvgtFbsRasterizer::ReadAlphaIntoCacheL(const TBitmapDesc& aDesc)
    {
    if (aDesc.iSizeInPixels.iWidth <= 0 || aDesc.iSizeInPixels.iHeight <= 0 || (iAlphaKeyLo == 0 && iAlphaKeyHi == 0))
        return;
    const TInt w = aDesc.iSizeInPixels.iWidth, h = aDesc.iSizeInPixels.iHeight;
    const TInt stride = CFbsBitmap::ScanLineLength(w, EGray256);
    if (stride * h > KAlphaCacheMaxBytes / 2)
        return;
    HBufC8* tmp = HBufC8::NewL(stride * h);
    CleanupStack::PushL(tmp);
    TUint8* base = (TUint8*)tmp->Des().Ptr();
    Mem::FillZ(base, stride * h);
    (void)vgGetError(); // clear any stale error so ours is the one we read
    vgReadPixels(base + stride * (h - 1), -stride, VG_A_8, 0, 0, w, h);
    if (vgGetError() == VG_NO_ERROR)
        StoreAlphaL(aDesc, iAlphaKeyLo, iAlphaKeyHi, base, stride);
    CleanupStack::PopAndDestroy(tmp);
    }

void CSvgtFbsRasterizer::ClearAlphaCache()
    {
    for (TInt i = 0; i < iAlpha.Count(); ++i)
        {
        delete iAlpha[i]->iData;
        delete iAlpha[i];
        }
    iAlpha.Reset();
    iAlphaBytes = 0;
    }

// ---- OURS: persistent rasterised-icon cache ---------------------------------------------------------------------
// One file per icon in C:\private\<sid>\iconcache\<hi><lo>.ic : 12 words of header, then the bitmap rows, then (colour
// icons) the EGray256 alpha plane that answers the mask request. The key (RenderL) hashes the NVG payload, size, scale
// mode, rotation, margin flag, display mode, mask flag and fill colour, so a new skin/theme simply misses. Every file is
// checksummed (header and payload); a mismatch deletes it. Total bounded at KDiskMaxBytes, oldest (by use) evicted first.
// C: has FileCacheWrite ON, so the create+write costs a memcpy; the real flush happens in the background.
static const TUint32 KDiskMagic = 0x43494850u;      // 'PHIC'
static const TUint32 KDiskVersion = 1;              // bump when the pixel pipeline / file layout changes
static const TInt KDiskMaxBytes = 4 * 1024 * 1024;
static const TInt KDiskMaxFile = 256 * 1024;
static const TInt KDiskHdrWords = 12;
_LIT(KDiskDirFmt, "C:\\private\\%08x\\iconcache\\");
_LIT(KDiskNameFmt, "%08x%08x.ic");

static TUint32 DiskSum(const TUint8* aPtr, TInt aLen, TUint32 aSeed)
    {
    TUint32 h = aSeed;
    for (TInt i = 0; i < aLen; ++i)
        {
        h = (h ^ aPtr[i]) * 0x01000193u;
        }
    return h;
    }

void CSvgtFbsRasterizer::DiskName(TDes& aName, TUint32 aLo, TUint32 aHi) const
    {
    aName.Format(KDiskDirFmt, (TUint)RProcess().SecureId().iId);
    aName.AppendFormat(KDiskNameFmt, aHi, aLo);
    }

TBool CSvgtFbsRasterizer::DiskEnsure()
    {
    if (iDiskDead)
        {
        return EFalse;
        }
    if (!iFsOpen)
        {
        if (iFs.Connect() != KErrNone)
            {
            iDiskDead = ETrue;
            return EFalse;
            }
        iFsOpen = ETrue;
        TBuf<64> dir;
        dir.Format(KDiskDirFmt, (TUint)RProcess().SecureId().iId);
        TInt r = iFs.MkDirAll(dir);
        if (r != KErrNone && r != KErrAlreadyExists)
            {
            RDebug::Printf("PH: [raster] disk cache: no dir (%d), disabled", r);
            iDiskDead = ETrue;
            return EFalse;
            }
        }
    return ETrue;
    }

// index the existing files once (oldest first); strays are deleted
void CSvgtFbsRasterizer::DiskScan()
    {
    iDiskScanned = ETrue;
    TBuf<64> dir;
    dir.Format(KDiskDirFmt, (TUint)RProcess().SecureId().iId);
    CDir* list = NULL;
    const TUint32 t0 = User::NTickCount();
    if (iFs.GetDir(dir, KEntryAttNormal | KEntryAttHidden | KEntryAttSystem | KEntryAttArchive, ESortByDate, list) != KErrNone || !list)
        {
        return;
        }
    TInt strays = 0;
    for (TInt i = 0; i < list->Count(); ++i)
        {
        const TEntry& e = (*list)[i];
        TDiskEnt ent;
        // "<hi 8 hex><lo 8 hex>.ic" = 19 chars. This said 11 once: every file looked like a stray and was deleted at the
        // first scan, so the cache never survived a reboot (2026-10-16)
        const TBool okLen = (e.iName.Length() == 19 && e.iName.Right(3).CompareF(_L(".ic")) == 0);
        TLex lexHi(okLen ? e.iName.Left(8) : TPtrC()), lexLo(okLen ? e.iName.Mid(8, 8) : TPtrC());
        if (!okLen || lexHi.Val(ent.iHi, EHex) != KErrNone || lexLo.Val(ent.iLo, EHex) != KErrNone
            || iDiskIdx.Append(ent) != KErrNone)
            {
            TFileName path(dir);
            path.Append(e.iName);
            iFs.Delete(path);
            ++strays;
            continue;
            }
        iDiskIdx[iDiskIdx.Count() - 1].iSize = e.iSize;
        iDiskBytes += e.iSize;
        }
    delete list;
    RDebug::Printf("PH: [raster] disk cache: sid=%08x %d files, %d bytes (limit %d), strays deleted %d, scan %u ms", (TUint)RProcess().SecureId().iId, iDiskIdx.Count(), iDiskBytes,
                   KDiskMaxBytes, strays, (TUint)(User::NTickCount() - t0));
    }

TInt CSvgtFbsRasterizer::DiskFind(TUint32 aLo, TUint32 aHi) const
    {
    for (TInt i = iDiskIdx.Count() - 1; i >= 0; --i)
        {
        if (iDiskIdx[i].iLo == aLo && iDiskIdx[i].iHi == aHi)
            {
            return i;
            }
        }
    return KErrNotFound;
    }

void CSvgtFbsRasterizer::DiskDrop(TInt aIndex, TBool aDeleteFile)
    {
    if (aIndex < 0 || aIndex >= iDiskIdx.Count())
        {
        return;
        }
    if (aDeleteFile)
        {
        TBuf<96> name;
        DiskName(name, iDiskIdx[aIndex].iLo, iDiskIdx[aIndex].iHi);
        iFs.Delete(name);
        }
    iDiskBytes -= iDiskIdx[aIndex].iSize;
    iDiskIdx.Remove(aIndex);
    }

CSvgtFbsRasterizer::TAlphaEntry* CSvgtFbsRasterizer::FindAlpha(const TBitmapDesc& aDesc, TUint32 aLo, TUint32 aHi)
    {
    for (TInt i = iAlpha.Count() - 1; i >= 0; --i)
        {
        TAlphaEntry* e = iAlpha[i];
        if (e->iKeyLo == aLo && e->iKeyHi == aHi && e->iWidth == aDesc.iSizeInPixels.iWidth
            && e->iHeight == aDesc.iSizeInPixels.iHeight)
            {
            return e;
            }
        }
    return NULL;
    }

void CSvgtFbsRasterizer::LogDiskStats()
    {
    RDebug::Printf("PH: [raster] disk cache hits=%d miss=%d stores=%d evict=%d bad=%d files=%d bytes=%d hit_ms=%u store_ms=%u",
                   iDiskHits, iDiskMiss, iDiskStores, iDiskEvict, iDiskBad, iDiskIdx.Count(), iDiskBytes,
                   (TUint)iDiskHitMs, (TUint)iDiskStoreMs);
    }

TBool CSvgtFbsRasterizer::DiskLoad(CSvgtRegisteredBitmap& aBmp, const TBitmapDesc& aDesc)
    {
    if (!DiskEnsure())
        {
        return EFalse;
        }
    const TUint32 t0 = User::NTickCount();
    TBuf<96> name;
    DiskName(name, iDiskLo, iDiskHi);
    RFile f;
    if (f.Open(iFs, name, EFileRead | EFileShareReadersOnly) != KErrNone)
        {
        iDiskMiss++;
        if (iDiskMiss < 200)
            RDebug::Printf("PH: [raster] dk miss sid=%08x key=%08x%08x %dx%d m%d nvg=%d", (TUint)RProcess().SecureId().iId, iDiskHi, iDiskLo, aDesc.iSizeInPixels.iWidth, aDesc.iSizeInPixels.iHeight, (TInt)aDesc.iDispMode, aDesc.iDataSize);
        return EFalse;
        }
    const TInt w = aDesc.iSizeInPixels.iWidth, h = aDesc.iSizeInPixels.iHeight;
    const TInt pixLen = CFbsBitmap::ScanLineLength(w, aDesc.iDispMode) * h;
    const TInt alphaLen = CFbsBitmap::ScanLineLength(w, EGray256) * h;
    TUint32 hd[KDiskHdrWords];
    TPtr8 hp((TUint8*)hd, 0, KDiskHdrWords * 4);
    TBool ok = (f.Read(hp) == KErrNone && hp.Length() == KDiskHdrWords * 4
                && hd[0] == KDiskMagic && hd[1] == KDiskVersion && hd[2] == iDiskLo && hd[3] == iDiskHi
                && hd[4] == (TUint32)w && hd[5] == (TUint32)h && hd[6] == (TUint32)aDesc.iDispMode
                && hd[7] == (TUint32)pixLen && (hd[8] == 0 || hd[8] == (TUint32)alphaLen)
                && hd[10] == DiskSum((const TUint8*)hd, 40, 0x811C9DC5u)
                && aBmp.SizeInPixels() == aDesc.iSizeInPixels && aBmp.DisplayMode() == aDesc.iDispMode);
    HBufC8* alpha = NULL;
    if (ok)
        {
        aBmp.BeginDataAccess();
        TPtr8 pp((TUint8*)aBmp.DataAddress(), 0, pixLen);
        ok = (f.Read(pp) == KErrNone && pp.Length() == pixLen);
        TUint32 sum = ok ? DiskSum(pp.Ptr(), pixLen, 0x811C9DC5u) : 0;
        aBmp.EndDataAccess();
        if (ok && hd[8])
            {
            alpha = HBufC8::New(alphaLen);
            if (alpha)
                {
                TPtr8 ap = alpha->Des();
                ok = (f.Read(ap) == KErrNone && ap.Length() == alphaLen);
                if (ok)
                    {
                    sum = DiskSum(ap.Ptr(), alphaLen, sum);
                    }
                }
            else
                {
                hd[8] = 0; // no memory for the alpha: use the pixels, the mask request renders (checksum then differs)
                ok = EFalse;
                }
            }
        ok = ok && (sum == hd[9]);
        }
    f.Close();
    if (!ok)
        {
        delete alpha;
        iDiskBad++;
        DiskDrop(DiskFind(iDiskLo, iDiskHi), ETrue);
        if (DiskFind(iDiskLo, iDiskHi) == KErrNotFound)
            {
            iFs.Delete(name); // not indexed (scan not run yet): still remove the bad file
            }
        iDiskMiss++;
        return EFalse;
        }
    if (alpha)
        {
        CleanupStack::PushL(alpha);
        TRAP_IGNORE(StoreAlphaL(aDesc, iAlphaKeyLo, iAlphaKeyHi, alpha->Ptr(), CFbsBitmap::ScanLineLength(w, EGray256)));
        CleanupStack::PopAndDestroy(alpha);
        }
    // LRU touch is deferred to the idle-time maintenance (no directory scan / SetModified on the hit path)
    DiskQueue(0);
    iDiskHits++;
    iDiskHitMs += User::NTickCount() - t0;
    if ((iDiskHits & 15) == 1)
        {
        LogDiskStats();
        }
    return ETrue;
    }

void CSvgtFbsRasterizer::DiskStore(CSvgtRegisteredBitmap& aBmp, const TBitmapDesc& aDesc)
    {
    if (!DiskEnsure())
        {
        return;
        }
    const TInt w = aDesc.iSizeInPixels.iWidth, h = aDesc.iSizeInPixels.iHeight;
    if (aBmp.SizeInPixels() != aDesc.iSizeInPixels || aBmp.DisplayMode() != aDesc.iDispMode)
        {
        return;
        }
    const TInt pixLen = CFbsBitmap::ScanLineLength(w, aDesc.iDispMode) * h;
    TAlphaEntry* ae = FindAlpha(aDesc, iAlphaKeyLo, iAlphaKeyHi);
    const TInt alphaLen = (ae && ae->iData->Length() == CFbsBitmap::ScanLineLength(w, EGray256) * h) ? ae->iData->Length() : 0;
    const TInt total = KDiskHdrWords * 4 + pixLen + alphaLen;
    if (pixLen <= 0 || total > KDiskMaxFile)
        {
        return;
        }
    const TUint32 t0 = User::NTickCount();
    TBuf<96> name;
    DiskName(name, iDiskLo, iDiskHi);
    aBmp.BeginDataAccess();
    const TUint8* pix = (const TUint8*)aBmp.DataAddress();
    TUint32 sum = DiskSum(pix, pixLen, 0x811C9DC5u);
    if (alphaLen)
        {
        sum = DiskSum(ae->iData->Ptr(), alphaLen, sum);
        }
    TUint32 hd[KDiskHdrWords];
    Mem::FillZ(hd, sizeof(hd));
    hd[0] = KDiskMagic; hd[1] = KDiskVersion; hd[2] = iDiskLo; hd[3] = iDiskHi;
    hd[4] = w; hd[5] = h; hd[6] = (TUint32)aDesc.iDispMode; hd[7] = pixLen; hd[8] = alphaLen; hd[9] = sum;
    hd[10] = DiskSum((const TUint8*)hd, 40, 0x811C9DC5u);
    RFile f;
    TInt r = f.Replace(iFs, name, EFileWrite | EFileShareExclusive);
    if (r == KErrNone)
        {
        r = f.Write(TPtrC8((const TUint8*)hd, KDiskHdrWords * 4));
        if (r == KErrNone)
            {
            r = f.Write(TPtrC8(pix, pixLen));
            }
        if (r == KErrNone && alphaLen)
            {
            r = f.Write(*ae->iData);
            }
        f.Close();
        }
    aBmp.EndDataAccess();
    if (r != KErrNone)
        {
        iFs.Delete(name);
        return;
        }
    DiskQueue(total);
    iDiskStores++;
    iDiskStoreMs += User::NTickCount() - t0;
    }

// queue the file just stored (aSize > 0) or hit (0) for the idle-time maintenance
void CSvgtFbsRasterizer::DiskQueue(TInt aSize)
    {
    TDiskEnt p;
    p.iLo = iDiskLo;
    p.iHi = iDiskHi;
    p.iSize = aSize;
    if (iDiskPend.Count() < 512)
        {
        iDiskPend.Append(p);
        }
    if (!iDiskTimer)
        {
        iDiskTimer = CPeriodic::New(CActive::EPriorityIdle);
        }
    if (iDiskTimer)
        {
        // (re)arm: run once, 2 s after the last rasterise, i.e. when WSERV has been quiet
        iDiskTimer->Cancel();
        iDiskTimer->Start(2000000, 2000000, TCallBack(&CSvgtFbsRasterizer::DiskTimerCb, this));
        }
    }

TInt CSvgtFbsRasterizer::DiskTimerCb(TAny* aSelf)
    {
    CSvgtFbsRasterizer* self = static_cast<CSvgtFbsRasterizer*>(aSelf);
    self->iDiskTimer->Cancel();
    self->DiskMaintain();
    return 0;
    }

// idle time: index the directory once, flush the files stored since the last run (a reset right after a write-back
// cache fill must not lose them), refresh the LRU order, evict down to the limit
void CSvgtFbsRasterizer::DiskMaintain()
    {
    if (!DiskEnsure())
        {
        iDiskPend.Reset();
        return;
        }
    const TUint32 t0 = User::NTickCount();
    if (!iDiskScanned)
        {
        DiskScan();
        }
    TTime now;
    now.UniversalTime();
    TInt flushed = 0;
    for (TInt i = 0; i < iDiskPend.Count(); ++i)
        {
        const TDiskEnt p = iDiskPend[i];
        TBuf<96> name;
        DiskName(name, p.iLo, p.iHi);
        TInt idx = DiskFind(p.iLo, p.iHi);
        TDiskEnt ent = p;
        if (idx != KErrNotFound)
            {
            if (!p.iSize)
                {
                ent.iSize = iDiskIdx[idx].iSize;
                }
            iDiskBytes -= iDiskIdx[idx].iSize;
            iDiskIdx.Remove(idx);
            }
        else if (!p.iSize)
            {
            continue; // touched a file that is not there any more
            }
        if (iDiskIdx.Append(ent) == KErrNone)
            {
            iDiskBytes += ent.iSize;
            }
        if (p.iSize)
            {
            RFile f;
            if (f.Open(iFs, name, EFileWrite | EFileShareAny) == KErrNone)
                {
                f.Flush();
                f.Close();
                flushed++;
                }
            }
        else
            {
            iFs.SetModified(name, now);
            }
        }
    iDiskPend.Reset();
    while (iDiskIdx.Count() > 1 && iDiskBytes > KDiskMaxBytes)
        {
        DiskDrop(0, ETrue);
        iDiskEvict++;
        }
    RDebug::Printf("PH: [raster] disk cache maintain: flushed %d, files=%d bytes=%d evict=%d, %u ms", flushed,
                   iDiskIdx.Count(), iDiskBytes, iDiskEvict, (TUint)(User::NTickCount() - t0));
    }
