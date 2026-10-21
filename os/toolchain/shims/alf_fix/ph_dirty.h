/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// ph_dirty.h -- OURS (ALF bring-up): the dirty rectangles of the frame CHuiDisplay::Refresh just drew, handed to the swap (phvgwindow.h PhVgSwapBuffers) so that it
// copies and submits only those rectangles. Shared by the generated HuiDisplay_ph.cpp (writes the list) and HuiVg10RenderSurface_ph.cpp (writes the merged rectangle,
// reads both); inline functions with a function-local static, so every translation unit of hitchcock / huirenderpluginvg sees one copy per DLL.
#ifndef PH_DIRTY_H
#define PH_DIRTY_H
#include <e32std.h>
const TInt KPhDirtyMax = 8;
struct TPhDirty
	{
	TInt iRect[KPhDirtyMax][4];		// x0,y0,x1,y1
	TInt iN;						// valid rectangles of the list (0 = no list)
	TInt iMerged[4];
	TBool iMergedValid;
	TBool iListOpen;
	};
inline TPhDirty& PhDirty() { static TPhDirty d; return d; }
inline void PhDirtySetMerged(const TRect& aRect)
	{
	TPhDirty& d = PhDirty();
	d.iMerged[0] = aRect.iTl.iX; d.iMerged[1] = aRect.iTl.iY; d.iMerged[2] = aRect.iBr.iX; d.iMerged[3] = aRect.iBr.iY;
	d.iMergedValid = ETrue;
	}
// The list is handed over by CHuiDisplay::Refresh (hitchcock.dll) through the render surface (huirenderpluginvg.dll) -- two DLLs, so no shared static: SetDirtyRect(TRect(-7777,..))
// opens a list, every following SetDirtyRect(r) adds a rectangle, SetDirtyRect(TRect(-7778,..)) closes it. Anything else is the merged rectangle.
const TInt KPhDirtyBegin = -7777;
const TInt KPhDirtyEnd = -7778;
inline void PhDirtyRect(const TRect& aRect)
	{
	TPhDirty& d = PhDirty();
	if (aRect.iTl.iX == KPhDirtyBegin)
		{
		d.iN = 0;
		d.iListOpen = ETrue;
		return;
		}
	if (aRect.iTl.iX == KPhDirtyEnd)
		{
		d.iListOpen = EFalse;
		if (d.iN > KPhDirtyMax)
			d.iN = 0;					// too many rectangles: the merged one is used
		return;
		}
	if (d.iListOpen)
		{
		if (d.iN < KPhDirtyMax)
			{
			d.iRect[d.iN][0] = aRect.iTl.iX; d.iRect[d.iN][1] = aRect.iTl.iY; d.iRect[d.iN][2] = aRect.iBr.iX; d.iRect[d.iN][3] = aRect.iBr.iY;
			}
		++d.iN;
		return;
		}
	PhDirtySetMerged(aRect);
	}
#endif
