// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phecaps.cpp -- OURS: report the process's own capabilities on the serial log (a test tool for installed packages: a
// binary installed from a SIS runs on the capabilities of its E32 image header).
//   [phecaps] sid=... vid=... MultimediaDD=1 NetworkServices=1 ... (all -tcb: every one but TCB)
// `phecaps <file>` also reports RLibrary::GetInfo for that file (what ECOM's plug-in security check reads) and the
// attributes of its drive.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

static void GetInfo(const TDesC& aPath)
	{
	RLibrary::TInfoBuf info;
	const TInt r = RLibrary::GetInfo(aPath, info);
	TBuf8<128> p;
	p.Copy(aPath.Left(127));
	RDebug::Printf("PH: [phecaps] getinfo %s -> %d uids %08x %08x %08x sid %08x vid %08x caps %08x %08x", p.PtrZ(), r,
		info().iUids[0].iUid, info().iUids[1].iUid, info().iUids[2].iUid, (TUint)info().iSecurityInfo.iSecureId,
		(TUint)info().iSecurityInfo.iVendorId, info().iSecurityInfo.iCaps[0], info().iSecurityInfo.iCaps[1]);
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	TInt drive;
	if (aPath.Length() > 1 && RFs::CharToDrive(aPath[0], drive) == KErrNone)
		{
		TDriveInfo di;
		const TInt dr = fs.Drive(di, drive);
		RDebug::Printf("PH: [phecaps] drive %c: -> %d type %d drive att %08x media att %08x", (char)aPath[0], dr,
			di.iType, di.iDriveAtt, di.iMediaAtt);
		}
	fs.Close();
	}

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
	TBuf<256> cmd;
	if (User::CommandLineLength() <= cmd.MaxLength())
		{
		User::CommandLine(cmd);
		cmd.Trim();
		if (cmd.Length())
			GetInfo(cmd);
		}
	return Run();
	}
