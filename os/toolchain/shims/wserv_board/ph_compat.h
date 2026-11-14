// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// ph_compat.h -- OURS: the phecompat hook's profile lookup for WSERV.
//
// The repository KPhCompatRepository holds one integer per app: key = the app's secure id, value = a bit field:
//   bits 0-1   rotation offset in quarter turns: the rotation an app is told for every screen mode is the real one plus
//              this offset (see PhCompat::ReportedRotation). Only the per-mode getters use it.
//   bit 2      minimum touch hold on
//   bits 8-15  the hold, in 10 ms units: a pointer-up that arrives sooner after the pointer-down is delayed to that time
// An absent repository, an unreadable one or an app without a key gives 0, so every answer is the unmodified one.
// The table is read on first use and again whenever the repository changes (no restart needed).
#ifndef PH_COMPAT_H
#define PH_COMPAT_H

#include <e32std.h>
#include <w32std.h>

namespace PhCompat
    {
    // phe UID range: 0xE7016E00 = this repository; 0xE7016E01 and up = the phecompat package (app, importer, ...).
    const TUint32 KPhCompatRepositoryUid = 0xE7016E00;
    const TUint32 KRotationMask = 0x3;
    const TUint32 KHoldFlag = 0x4;
    const TInt KMaxProfiles = 64;

    // The profile value of an app (0 = none). Defined once, in scrdev_nga.cpp.
    TUint32 Profile(TUint32 aSid);

    // The rotation a client with this secure id is told, given the real rotation (a CFbsBitGc::TGraphicsOrientation
    // value: 0, 1, 2, 3 = 0, 90, 180, 270 degrees).
    inline CFbsBitGc::TGraphicsOrientation ReportedRotation(TUint32 aSid, CFbsBitGc::TGraphicsOrientation aReal)
        {
        const TInt offset = static_cast<TInt>(Profile(aSid) & KRotationMask);
        return static_cast<CFbsBitGc::TGraphicsOrientation>((static_cast<TInt>(aReal) + offset) & 3);
        }

    // The minimum time in ms a press must last for this app (0 = no hold).
    inline TInt HoldMs(TUint32 aSid)
        {
        const TUint32 v = Profile(aSid);
        return (v & KHoldFlag) ? static_cast<TInt>(((v >> 8) & 0xFF) * 10) : 0;
        }
    }

#endif
