// Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description:
//

#include "screencaptureutil.h"
#include <e32std.h>
#include <e32base.h>
#include "panic.h"
#include <fbs.h>
#include <graphics/wsgraphicscontext.h>
#include <graphics/wsdisplaycontrol.h>
#include <graphics/wsscene.h>
#include <graphics/wsdisplaymapping.h>
//#include <graphics/testscreencapture.h>
//#include <graphics/wsscreendeviceutils.h>
#include <graphics/wsscreendevice.h>

CScreenCaptureUtil* CScreenCaptureUtil::NewL(CWsRenderStage* aOwner)
	{
	CScreenCaptureUtil * util = new(ELeave) CScreenCaptureUtil;
	CleanupStack::PushL(util);
	util->ConstructL(aOwner);
	
	CleanupStack::Pop(util);
	return util;
	}
	
void CScreenCaptureUtil::ConstructL(CWsRenderStage* aOwner)
	{
	// initialise the references to the interfaces of interest
	iScene = aOwner->ObjectInterface<MWsScene>();
	STD_ASSERT_ALWAYS(iScene, EPluginPanicSceneMissing);
	
	iScreenDevice = aOwner->ObjectInterface<MWsScreenDevice>();
	STD_ASSERT_ALWAYS(iScreenDevice, EPluginPanicSceneMissing);
	
	iDisplayMapping = aOwner->ObjectInterface<MWsDisplayMapping>();
	
	// create the local surface
	User::LeaveIfError(iSurfaceManager.Open());
	User::LeaveIfError(iSurfaceUpdateSession.Connect());
	}
	
CScreenCaptureUtil::CScreenCaptureUtil()
	{
	}
	
CScreenCaptureUtil::~CScreenCaptureUtil()
	{
	iScene->UnregisterSurface(iLocalSurface);
	iSurfaceManager.CloseSurface(iLocalSurface);
	iSurfaceManager.Close();
	iSurfaceUpdateSession.Close();
	}

void CScreenCaptureUtil::ComposeScreenL(TInt aHandle)
	{
	TInt ret = KErrNone;

	//Let's duplicate the bitmap
	CFbsBitmap* bitmap=new(ELeave) CFbsBitmap();
	CleanupStack::PushL(bitmap);
	
	TInt r = bitmap->Duplicate(aHandle);
	
	if (r == KErrNoMemory)
		{
		User::Leave(KErrNoMemory);
		}
	
	ret = KErrBadHandle;
	if (r == KErrNone)
		{
		CreateLocalSurfaceL();
		
		// check that the bitmap format is matching our requirements
		if (VerifyBitmapFormatL(*bitmap))
		    {
	        ret = iScene->RegisterSurface(iLocalSurface);
	        if (ret == KErrNone)
	            {
	            TRequestStatus cRs = KErrNone;
	            iScene->ComposePendingScene(iLocalSurface, &cRs);
	            User::WaitForRequest(cRs);
	            ret = CopySurfaceToBitmapL(*bitmap);
	            }
			}
		else
			{
			// we accept only matching bitmap formats
			ret = KErrArgument;
			}
		}
	
	CleanupStack::PopAndDestroy(bitmap);
	}
/*
void CScreenCaptureUtil::TranslateExtentL(const TWsScsComposeScreenCommand& aMessage)
	{
	TPoint srcTopLeft(aMessage.iParameter.iExtent.iTl.iX, aMessage.iParameter.iExtent.iTl.iY);
	TPoint srcBottomRight(aMessage.iParameter.iExtent.iBr.iX, aMessage.iParameter.iExtent.iBr.iY);
	TRect rectSrc(srcTopLeft, srcBottomRight);
	TRect rectRes;
	TInt ret = iDisplayMapping->MapCoordinates(EApplicationSpace, rectSrc, ECompositionSpace, rectRes);
	aUtils.ReplyBuf(&rectRes, sizeof(TRect));
	aUtils.SetReply(ret);
	}
*/
TSurfaceId CScreenCaptureUtil::CreateSurfaceL(const TSize& aSize, TUidPixelFormat aPixelFormat, TInt aStride)
	{
	RSurfaceManager::TSurfaceCreationAttributesBuf bf;
	RSurfaceManager::TSurfaceCreationAttributes& b = bf();
	b.iSize.iWidth = aSize.iWidth;
	b.iSize.iHeight = aSize.iHeight;
	b.iBuffers = 1;				// number of buffers in the surface
	b.iPixelFormat = aPixelFormat;
	b.iStride = aStride;		// Number of bytes between start of one line and start of next
	b.iOffsetToFirstBuffer = 0;	// way of reserving space before the surface pixel data
	b.iAlignment = 4;			// alignment, 1,2,4,8 byte aligned
	b.iContiguous=EFalse;
	b.iMappable = ETrue;

	TSurfaceId surface = TSurfaceId::CreateNullId();
	User::LeaveIfError(iSurfaceManager.CreateSurface(bf, surface));
	return surface;
	}

void CScreenCaptureUtil::CreateLocalSurfaceL()
	{
	TSize surfaceSize;
	GetCompositionAreaSize(surfaceSize);
	
	if (!iLocalSurface.IsNull())
		{
        RSurfaceManager::TInfoBuf infoBuf;
    	RSurfaceManager::TSurfaceInfoV01& info = infoBuf();
        if ((KErrNone != iSurfaceManager.SurfaceInfo(iLocalSurface, infoBuf)) || 
        	(info.iSize != surfaceSize))
            {
            iScene->UnregisterSurface(iLocalSurface);
    		iSurfaceManager.CloseSurface(iLocalSurface);
    		iLocalSurface = TSurfaceId::CreateNullId();
            }
		}
	
	if (iLocalSurface.IsNull())
		{
		iLocalSurface = CreateSurfaceL(surfaceSize, KSurfacePixelFormat, KSurfaceBytesPerPixel*surfaceSize.iWidth);
		}
	}

TBool CScreenCaptureUtil::VerifyBitmapFormatL(const CFbsBitmap& aBitmap)
	{
  RSurfaceManager::TInfoBuf infoBuf;
	RSurfaceManager::TSurfaceInfoV01& info = infoBuf();
  User::LeaveIfError(iSurfaceManager.SurfaceInfo(iLocalSurface, infoBuf));
    		
	// OURS (2026-11-02): also the 32-bit ALPHA modes. The task switcher's preview plug-in (tsfswpreviewprovider) makes its
	// bitmap in the screen's display mode, which this render stage reports as EColor16MAP (displayrenderstage.cpp
	// DisplayMode): with only EColor16MU accepted every capture failed silently and each preview stayed the new, white bitmap.
	// CopySurfaceToBitmapL writes the XRGB rows with an opaque alpha for these.
	const TDisplayMode mode = aBitmap.DisplayMode();
	return ((info.iSize == aBitmap.SizeInPixels()) && (EColor16MU == mode || EColor16MA == mode || EColor16MAP == mode));
	}

void CScreenCaptureUtil::GetCompositionAreaSize(TSize& aSize) const
	{
	TSize uiSize;
	uiSize = iScreenDevice->SizeInPixels();
	TRect uiRect(TPoint(0,0), uiSize);
	TRect compositionRect;
	iDisplayMapping->MapCoordinates(EFullScreenSpace, uiRect, ECompositionSpace, compositionRect);
	aSize = compositionRect.Size();
	}

TInt CScreenCaptureUtil::CopySurfaceToBitmapL(CFbsBitmap& aCopyToBitmap)
	{
	RSurfaceManager::TInfoBuf infoBuf;
	RSurfaceManager::TSurfaceInfoV01& info = infoBuf();

	User::LeaveIfError(iSurfaceManager.SurfaceInfo(iLocalSurface, infoBuf));
	
	TInt bytesPerPixel=0;
	TDisplayMode	bitmapMode = ENone;
	
	switch (info.iPixelFormat)
		{
		case EUidPixelFormatXRGB_8888:
			{
			bitmapMode = EColor16MU;
			bytesPerPixel = 4;
			break;
			}
		default:
			{
			return KErrCorrupt;
			}
		}
	
	// OURS (2026-11-02): an EColor16MA / EColor16MAP bitmap takes the same rows with the alpha byte set (see VerifyBitmapFormatL)
	const TDisplayMode mode = aCopyToBitmap.DisplayMode();
	const TBool setAlpha = bitmapMode == EColor16MU && (mode == EColor16MA || mode == EColor16MAP);
	if ((aCopyToBitmap.SizeInPixels() != info.iSize) || (mode != bitmapMode && !setAlpha))
		{
		return KErrCorrupt;
		}
	
	RChunk chunk;
	CleanupClosePushL(chunk);
	User::LeaveIfError(iSurfaceManager.MapSurface(iLocalSurface, chunk));
	TUint8* surfacePtr = chunk.Base();
	// OURS (2026-11-02): inside Begin/EndDataAccess. A bare DataAddress() marks the bitmap volatile for good
	// (CFbsBitmap::DataAddress), and Alf re-uploads a volatile bitmap on every frame it draws: the task switcher's
	// previews (captured straight into them) went back to the GPU ~15 times a second while sliding
	aCopyToBitmap.BeginDataAccess();
	TUint8* bitmapPtr = (TUint8*)aCopyToBitmap.DataAddress();
	TInt copyBytes=info.iSize.iWidth*bytesPerPixel;
	for (TInt y=0; y<info.iSize.iHeight; y++)
		{
		if (setAlpha)
			{
			const TUint32* from = reinterpret_cast<const TUint32*>(surfacePtr);
			TUint32* to = reinterpret_cast<TUint32*>(bitmapPtr);
			for (TInt x = 0; x < info.iSize.iWidth; x++)
				to[x] = from[x] | 0xFF000000;		// XRGB: the X byte is undefined
			}
		else
			Mem::Copy(bitmapPtr,surfacePtr,copyBytes);
		surfacePtr += info.iStride;
		bitmapPtr += aCopyToBitmap.DataStride();
		}
	aCopyToBitmap.EndDataAccess(EFalse);		// (changed: its touch count goes up, so caches take the new pixels once)
	CleanupStack::PopAndDestroy(&chunk);
	return KErrNone;
	}
