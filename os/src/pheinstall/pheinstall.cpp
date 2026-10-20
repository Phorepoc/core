// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// pheinstall.cpp -- install a SIS file silently through the S60 software installer: the device half of
// `ph package install` (the pdk fetches the package, checks its SHA-256, uploads it and runs this in a device shell).
//
//     pheinstall <file.sis> [drive]        e.g.  pheinstall c:\data\installs\hello.sis c
//
// RSWInstSilentLauncher::SilentInstall (SwiUI, the release's installer launcher API): no dialogs; upgrades,
// overwrites, packages that are not signed by a trusted root and the capabilities the package asks for are allowed
// (Phorepoc's install server decides what an untrusted package may do: swipolicy.ini). The target drive defaults
// to C:. Prints "[pheinstall] ..." lines on the debug log (the last: "installed" or "failed <code>") and exits with the
// installer's result (0 = installed).
#include <e32base.h>
#include <e32debug.h>
#include <swinstapi.h>
#include <swinstdefs.h>

using namespace SwiUI;

static TInt InstallL(const TDesC& aFile, TChar aDrive)
	{
	TInstallOptions opt;
	opt.iUpgrade = EPolicyAllowed;
	opt.iOptionalItems = EPolicyAllowed;
	opt.iOCSP = EPolicyNotAllowed;
	opt.iIgnoreOCSPWarnings = EPolicyAllowed;
	opt.iUntrusted = EPolicyAllowed;
	opt.iPackageInfo = EPolicyAllowed;
	opt.iCapabilities = EPolicyAllowed;
	opt.iKillApp = EPolicyAllowed;
	opt.iOverwrite = EPolicyAllowed;
	opt.iDownload = EPolicyNotAllowed;
	opt.iDrive = aDrive;
	opt.iUpgradeData = EPolicyAllowed;
	TInstallOptionsPckg pckg(opt);

	RSWInstSilentLauncher launcher;
	TInt r = launcher.Connect();
	RDebug::Printf("PH: [pheinstall] installer connect %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(launcher);
	r = launcher.SilentInstall(aFile, pckg);
	CleanupStack::PopAndDestroy(&launcher);
	return r;
	}

// the debug log carries 8-bit RDebug::Printf lines (a 16-bit RDebug::Print does not reach it); on the phone only user
// lines starting "PH:" pass its filter
static void Report(const TDesC& aFile, const char* aWhat, TInt aValue)
	{
	TBuf8<256> f;
	f.Copy(aFile.Left(255));
	RDebug::Printf("PH: [pheinstall] %s: %s %d", f.PtrZ(), aWhat, aValue);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TBuf<256> cmd;
	User::CommandLine(cmd);
	cmd.Trim();
	TInt sp = cmd.Locate(' ');
	TPtrC file = sp < 0 ? TPtrC(cmd) : cmd.Left(sp);
	TPtrC rest = sp < 0 ? TPtrC(KNullDesC) : cmd.Mid(sp + 1);
	TChar drive('C');
	if (rest.Length())
		{
		drive = rest[0];
		drive.UpperCase();
		}
	Report(cmd, "command line, length", cmd.Length());
	TInt err = KErrArgument;
	if (file.Length())
		{
		TInt r = KErrNone;
		TRAP(err, r = InstallL(file, drive));
		if (!err)
			err = r;
		}
	if (err)
		Report(file, "failed", err);
	else
		Report(file, "installed, drive", (TInt)(TUint)drive);
	delete cleanup;
	return err;
	}
