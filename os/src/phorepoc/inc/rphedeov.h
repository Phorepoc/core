/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// rphedeov.h -- OURS: the interface of a display's video plane -- a plane the display hardware scans a YUV picture out of
// directly, with its own scaler and YUV->RGB conversion, blended over the UI. Video playback (src/phevidctl) uses it
// when the board's kernel has a logical device "PheDeOv"; a SoC whose display has such a plane implements it.
//   GetInfo  -> panel size, source limits, the overlay state, vblank / flip counters
//   Set      -> program the plane for a picture (source pointers, size, destination rectangle, alpha, colour standard)
//               and show it at the next vblank; also used to move / resize
//   Flip     -> (async) point the plane at the next picture of the same geometry; completes at the vblank that latched it
//   Disable  -> hide the plane (the buffers stay referenced until the next vblank passed)
//   ReadReg  -> one register of the display hardware, for tests (the offsets are the driver's own)
// The picture is NOT given as a physical address (never trusted from user mode): the caller passes the ADDRESS of the first
// byte of the Y plane and of the UV plane in a shared chunk it owns (physically contiguous); the driver resolves the
// chunk, checks the whole plane is contiguous, and holds a reference to it while it is shown.
#ifndef RPHEDEOV_H
#define RPHEDEOV_H

#include <e32cmn.h>
#ifndef __KERNEL_MODE__
#include <e32std.h>
#endif

_LIT(KPheDeOvLddName, "PheDeOv");

enum TPheDeOvFormat { EPheDeOvNV12 = 0, EPheDeOvNV21 = 1 };		// YUV 4:2:0 semi-planar (UV / VU interleaved)
enum TPheDeOvColor { EPheDeOvBT601 = 0, EPheDeOvBT709 = 1, EPheDeOvAuto = 2 };	// Auto: BT.709 from 1280 wide up, else BT.601
enum TPheDeOvRotation { EPheDeOvRot0 = 0 };		// other rotations: KErrNotSupported where the plane has no rotator

struct TPheDeOvInfo
	{
	TInt iPanelW, iPanelH;			// the panel's native pixels
	TInt iMaxSrcW, iMaxSrcH;		// the plane's source limits
	TInt iEnabled;					// the plane is shown
	TUint32 iVblanks;				// display frames since boot
	TUint32 iFlips;					// Set/Flip calls that reached the hardware
	TUint32 iFlipsDone;				// ... that completed on a vblank
	TInt iScaleMode;				// 0 = none/off, 1 = fine scaling only, 2 = vertical coarse, 3 = horizontal coarse, 4 = both
	};

// Set: the source Y/UV plane addresses are in the CALLER's address space (a shared chunk).
struct TPheDeOvSet
	{
	const TUint8* iY;				// first byte of the (cropped) source, Y plane
	const TUint8* iUV;				// first byte of the matching UV row/column (even x, even y of the Y origin)
	TInt iPitch;					// bytes per line of both planes
	TInt iSrcW, iSrcH;				// source size in pixels (even)
	TInt iDstX, iDstY, iDstW, iDstH;	// destination rectangle on the PANEL (native pixels)
	TInt iFormat;					// TPheDeOvFormat
	TInt iColor;					// TPheDeOvColor
	TInt iFullRange;				// 0 = limited (16..235), 1 = full
	TInt iAlpha;					// 0..255 plane alpha
	TInt iRotation;					// TPheDeOvRotation
	};

struct TPheDeOvFlip
	{
	const TUint8* iY;				// same geometry as the last Set, the next picture
	const TUint8* iUV;
	};

enum TPheDeOvControl { EPheDeOvCtrlGetInfo, EPheDeOvCtrlSet, EPheDeOvCtrlDisable, EPheDeOvCtrlReadReg };
enum TPheDeOvRequest { EPheDeOvReqFlip, EPheDeOvReqCount };
const TInt KPheDeOvMajorVersion = 1;

#ifndef __KERNEL_MODE__
class RPheDeOv : public RBusLogicalChannel
	{
public:
	inline static TVersion VersionRequired() { return TVersion(KPheDeOvMajorVersion, 0, 0); }
	inline TInt Open() { return DoCreate(KPheDeOvLddName, VersionRequired(), KNullUnit, NULL, NULL, EOwnerThread); }
	inline TInt GetInfo(TPheDeOvInfo& aInfo)
		{ TPckg<TPheDeOvInfo> p(aInfo); return DoControl(EPheDeOvCtrlGetInfo, &p); }
	inline TInt Set(const TPheDeOvSet& aSet)
		{ TPckgC<TPheDeOvSet> p(aSet); return DoControl(EPheDeOvCtrlSet, (TAny*)&p); }
	inline TInt Disable() { return DoControl(EPheDeOvCtrlDisable); }
	inline TInt ReadReg(TUint32 aOffset, TUint32& aVal)
		{ TPckg<TUint32> p(aVal); return DoControl(EPheDeOvCtrlReadReg, (TAny*)aOffset, &p); }
	inline void Flip(const TPheDeOvFlip& aFlip, TRequestStatus& aStatus)
		{ DoRequest(EPheDeOvReqFlip, aStatus, (TAny*)&aFlip); }
	inline void CancelFlip() { DoCancel(0xFFFFFFFF); }
	};
#endif

#endif
