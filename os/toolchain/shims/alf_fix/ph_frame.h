/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// ph_frame.h -- OURS (ALF bring-up): a frame counter shared by the generated Hitchcock copies (HuiDisplay_ph.cpp bumps it once per Refresh after the dirty scan,
// HuiVisual_ph.cpp / HuiCanvasVisual_ph.cpp use it to log every dirty-region report of the sampled frames: "PH: alf: ReportChanged frame N ...").
#ifndef PH_FRAME_H
#define PH_FRAME_H
#include <e32std.h>
#include <e32debug.h>
inline TUint& PhFrame() { static TUint f; return f; }
// sampled frames: every 32nd frame from 8 on (a frame reports ~30 visuals)
inline TBool PhFrameSampled() { return (PhFrame() % 32) == 8; }
#endif
