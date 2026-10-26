/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phedevmenu.h -- OURS: the developer-menu repository. Shared by the settings app, the
// Control Panel plug-in and (later) whatever reads a setting at boot: SSM's startup list, the kernel's boot guard.
#ifndef PHEDEVMENU_H
#define PHEDEVMENU_H

#include <e32std.h>

// the CenRep repository (initialised from Z:\private\10202be9\e7015a01.txt, tools/phorepoc-x86/x32_rom_args.py devmenu_cenrep)
const TUid KPheDevMenuRepoUid = { 0xE7015A01 };
// the settings application
const TUid KPheDevMenuAppUid = { 0xE7015A00 };

// key 1: the UI shell loaded at boot: 0 = Symbian^3 (homescreen + matrixmenu), 1 = Symbian^4 (Qt/Hb homescreen)
const TUint32 KPheDevKeyShell = 1;
const TInt KPheShellS3 = 0;
const TInt KPheShellS4 = 1;
// key 2: 1 = Control Panel lists a "Developer menu" entry
const TUint32 KPheDevKeyCpShortcut = 2;
// key 3: boot guard timeout, seconds (the kernel falls back to the golden image after this long without the UI-up ping;
// read by the HWRM power-state plug-in, which hands it to the kernel: power HAL function 0x5049)
const TUint32 KPheDevKeyGuardTimeout = 3;
// key 4: UI renderer: 0 = Classic (the non-Alfred WSERV path, default), 1 = Alfred/Hitchcock (GPU, uiacceltk); read at boot
const TUint32 KPheDevKeyRenderer = 4;
const TInt KPheRendererClassic = 0;
const TInt KPheRendererAlfred = 1;
const TInt KPheGuardMin = 30;
const TInt KPheGuardMax = 3600;
// key 5: the renderers THIS image carries (a bit mask, written by the ROM recipe, UI_RENDERER=): 1 = Classic, 2 = Alfred.
// The menu shows the "UI renderer" item only when both are present (3); absent = 3 (an older repository file)
const TUint32 KPheDevKeyRenderersAvail = 5;
const TInt KPheRenderersClassic = 1;
const TInt KPheRenderersAlfred = 2;
// key 6: the UI shells THIS image carries (a bit mask, written by the ROM recipe, SHELLS=): 1 = S^3, 2 = S^4. The menu shows the
// "UI shell" item only when both are present (3); absent = 1 (an older repository file: S^3 only). Read at boot by src/pheshellsel.
const TUint32 KPheDevKeyShellsAvail = 6;
const TInt KPheShellsS3 = 1;
const TInt KPheShellsS4 = 2;
// key 7: UI anti-aliasing of the software VG renderer (paths drawn by the compositor): 0 = Best (16 samples per pixel, the
// default), 1 = Fast (8), 2 = Off (1). Read once by the renderer (sfopenvg_board riRasterizer.cpp) when the compositor starts
const TUint32 KPheDevKeyAntialias = 7;
const TInt KPheAntialiasBest = 0;
// the guard's own default (src/phorepoc-arm a64_glue.cpp A64_BOOT_GUARD_S: seconds after the first watchdog feeding)
const TInt KPheGuardDefault = 180;

// The shells an image carries, from its ROM itself (2026-10-18): key 6 is only the build's record -- CenRep persists the whole
// repository to C: on the first save, so after a reflash it would describe the OLD image. Z: is the truth.
#include <f32file.h>
inline TInt PheShellsInRom()
	{
	RFs fs;
	if (fs.Connect() != KErrNone)
		return KPheShellsS3;
	TEntry e;
	TInt mask = 0;
	if (fs.Entry(_L("z:\\sys\\bin\\ailaunch.exe"), e) == KErrNone && fs.Entry(_L("z:\\sys\\bin\\matrixmenu.exe"), e) == KErrNone)
		mask |= KPheShellsS3;
	if (fs.Entry(_L("z:\\sys\\bin\\hsapplicationlauncher.exe"), e) == KErrNone && fs.Entry(_L("z:\\sys\\bin\\hsapplication.exe"), e) == KErrNone)
		mask |= KPheShellsS4;
	fs.Close();
	return mask ? mask : KPheShellsS3;
	}

#endif
