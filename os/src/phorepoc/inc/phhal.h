// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
//
// phhal.h -- the HAL functions Phorepoc's own components call on a board, beyond Symbian's (the interface a SoC
// adaptation implements in its variant's HAL handler, docs/layering.md in the pdk). A board without the hardware
// returns KErrNotSupported, and the callers carry on without it.

#ifndef PHHAL_H
#define PHHAL_H

// EHalGroupPower functions
enum TPhHalPowerFunction
	{
	// the UI's liveness ping for the kernel's hang guard (the HWRM power-state plug-in sends it while WSERV and the
	// home screen answer); a1, a2 unused
	EPhHalPowerGuardPing = 0x5047,
	// the battery: a1 = a TDes8 for TInt[4] = { fuel gauge % (-1 = not known), charging, battery mV, external power };
	// KErrNotReady before the first reading
	EPhHalPowerBattery = 0x5048,
	// the boot hang guard's timeout: a1 = seconds (30..3600)
	EPhHalPowerGuardTimeout = 0x5049,
	// the vibration motor: a1 = on (non-zero) / off
	EPhHalPowerVibra = 0x5056,
	// the WLAN chip's MAC address: a1 = a TDes8 for 6 bytes; KErrNotReady before the chip is probed
	EPhHalPowerWlanMac = 0x574C
	};

#endif
