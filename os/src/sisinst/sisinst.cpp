// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// sisinst.cpp -- OURS (X4.86): install a SIS package silently through the REAL S60 installer (SWInstSvrUI ->
// SWI's install server) and print the result on the serial log -- the software-install regression test.
//
//   sisinst <path.sis> [drive letter]       e.g. from the cycler list: !sisinst c:\data\test.sis
//
// Every policy is "allowed" (a silent install cannot ask): self-signed packages, user-grantable capabilities,
// upgrades, overwrites. The install drive defaults to C:.
#include <e32base.h>
#include <e32debug.h>
#include <swinstapi.h>
#include <swinstdefs.h>

static TInt Run()
	{
	TBuf<256> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TPtrC file = lex.NextToken();
	TPtrC drv = lex.NextToken();
	if (file.Length() == 0)
		{
		RDebug::Printf("PH: [sisinst] usage: sisinst <path.sis> [drive]");
		return KErrArgument;
		}
	SwiUI::TInstallOptions opts;
	opts.iUpgrade = SwiUI::EPolicyAllowed;
	opts.iOptionalItems = SwiUI::EPolicyAllowed;
	opts.iOCSP = SwiUI::EPolicyNotAllowed;
	opts.iIgnoreOCSPWarnings = SwiUI::EPolicyAllowed;
	opts.iUntrusted = SwiUI::EPolicyAllowed;
	opts.iPackageInfo = SwiUI::EPolicyAllowed;
	opts.iCapabilities = SwiUI::EPolicyAllowed;
	opts.iKillApp = SwiUI::EPolicyAllowed;
	opts.iOverwrite = SwiUI::EPolicyAllowed;
	opts.iDownload = SwiUI::EPolicyAllowed;
	opts.iDrive = drv.Length() ? TChar(drv[0]) : TChar('c');
	opts.iUsePhoneLang = ETrue;
	opts.iUpgradeData = SwiUI::EPolicyAllowed;
	SwiUI::TInstallOptionsPckg pckg(opts);

	SwiUI::RSWInstSilentLauncher launcher;
	TInt err = launcher.Connect();
	TBuf8<128> f8;
	f8.Copy(file.Left(128));
	RDebug::Printf("PH: [sisinst] %S: connect %d", &f8, err);
	if (err == KErrNone)
		{
		err = launcher.SilentInstall(file, pckg);
		launcher.Close();
		}
	RDebug::Printf("PH: [sisinst] %S: %s (%d)", &f8, err == KErrNone ? "INSTALLED" : "FAILED", err);
	return err;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt err = KErrNoMemory;
	if (cleanup)
		{
		TRAPD(leave, err = Run());
		if (leave != KErrNone)
			{
			RDebug::Printf("PH: [sisinst] left %d", leave);
			err = leave;
			}
		delete cleanup;
		}
	return err;
	}
