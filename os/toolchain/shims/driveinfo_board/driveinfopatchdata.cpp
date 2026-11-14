// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// driveinfopatchdata.cpp -- OURS (Phorepoc x86, X4.111): the board's patchable PlatformEnv drive defaults, replacing the 2010
// source's PlatformEnv/src/driveinfopatchdata.cpp (that file is a platform-variant "patch data" file: a product ROM patches
// the values at ROM-build time).
// Only the removable mass storage differs from the 2010 defaults: the memory card is F:, E: the internal mass memory
// (the layout the release's own Voice Recorder expects: its "insert the F: card" dialog). Everything else is the 2010
// source verbatim.
#include <f32file.h>

EXPORT_C extern const TInt KDriveInfoDefaultRomDrive = EDriveZ;
EXPORT_C extern const TInt KDriveInfoDefaultRamDrive = EDriveD;
EXPORT_C extern const TInt KDriveInfoDefaultSystemDrive = KErrNotFound; // To use RFs::GetSystemDrive()
EXPORT_C extern const TInt KDriveInfoDefaultPhoneMemory = EDriveC;
EXPORT_C extern const TInt KDriveInfoDefaultMassStorage = EDriveE;
EXPORT_C extern const TInt KDriveInfoDefaultRemovableMassStorage = EDriveF;

EXPORT_C extern const TInt KDriveInfoUserHiddenDrive1 = EDriveD;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive2 = EDriveZ;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive3 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive4 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive5 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive6 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive7 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoUserHiddenDrive8 = KErrNotFound;

EXPORT_C extern const TInt KDriveInfoUsbDrivesStart = EDriveG;
EXPORT_C extern const TInt KDriveInfoUsbDrivesMaxCount = 2;

EXPORT_C extern const TInt KDriveInfoLogicallyRemovableDrive1 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoLogicallyRemovableDrive2 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoLogicallyRemovableDrive3 = KErrNotFound;
EXPORT_C extern const TInt KDriveInfoLogicallyRemovableDrive4 = KErrNotFound;
