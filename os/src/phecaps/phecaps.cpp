// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phecaps.cpp -- OURS: report the process's own capabilities on the serial log (a test tool for installed packages: a
// binary installed from a SIS runs on the capabilities of its E32 image header).
//   [phecaps] sid=... vid=... MultimediaDD=1 NetworkServices=1 ... (all -tcb: every one but TCB)
#include <e32base.h>
#include <e32debug.h>

static TInt Run()
	{
	RProcess me;
	const TSecureId sid = me.SecureId();
	const TVendorId vid = me.VendorId();
	TInt n = 0, ok = 0;
	for (TInt c = 0; c < ECapability_Limit; ++c)
		{
		++n;
		if (me.HasCapability(TCapability(c)))
			++ok;
		}
	RDebug::Printf("PH: [phecaps] sid=%08x vid=%08x capabilities %d of %d; TCB=%d MultimediaDD=%d NetworkServices=%d ReadUserData=%d",
		(TUint)sid, (TUint)vid, ok, n,
		me.HasCapability(ECapabilityTCB) ? 1 : 0, me.HasCapability(ECapabilityMultimediaDD) ? 1 : 0,
		me.HasCapability(ECapabilityNetworkServices) ? 1 : 0, me.HasCapability(ECapabilityReadUserData) ? 1 : 0);
	return KErrNone;
	}

GLDEF_C TInt E32Main()
	{
	return Run();
	}
