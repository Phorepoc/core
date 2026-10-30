// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

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
#include <apgcli.h>
#include <apmrec.h>
#include <f32file.h>

static TInt Run()
	{
	TBuf<512> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	// a path with spaces comes quoted: "e:\data\some package.sis" e
	lex.SkipSpace();
	TPtrC file;
	if (lex.Peek() == '"')
		{
		lex.Inc();
		lex.Mark();
		while (!lex.Eos() && lex.Peek() != '"')
			lex.Inc();
		file.Set(lex.MarkedToken());
		if (!lex.Eos())
			lex.Inc();
		}
	else
		file.Set(lex.NextToken());
	TPtrC drv = lex.NextToken();
	if (file.Length() == 0)
		{
		// no arguments (a boot test: ssm-test=sisinst starts it bare): the package put on the disk as C:\sisinst.sis
		RDebug::Printf("PH: [sisinst] usage: sisinst <path.sis> [drive] | sisinst -u <package uid, hex>; none given: c:\\sisinst.sis");
		file.Set(_L("c:\\sisinst.sis"));
		}
	if (file == _L("-u"))
		{
		// uninstall: sisinst -u <uid>, silently (kills the running application, ignores dependencies)
		TUint32 uid = 0;
		TLex uidLex(drv);
		TInt perr = uidLex.Val(uid, EHex);
		if (perr != KErrNone)
			{
			RDebug::Printf("PH: [sisinst] -u: bad uid");
			return perr;
			}
		SwiUI::TUninstallOptions uopts;
		uopts.iKillApp = SwiUI::EPolicyAllowed;
		uopts.iBreakDependency = SwiUI::EPolicyAllowed;
		SwiUI::TUninstallOptionsPckg upckg(uopts);
		SwiUI::RSWInstSilentLauncher ulauncher;
		TInt uerr = ulauncher.Connect();
		if (uerr == KErrNone)
			{
			uerr = ulauncher.SilentUninstall(TUid::Uid(uid), upckg, _L8("x-epoc/x-sisx-app"));
			ulauncher.Close();
			}
		RDebug::Printf("PH: [sisinst] uninstall %08x: %s (%d)", uid, uerr == KErrNone ? "REMOVED" : "FAILED", uerr);
		return uerr;
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

	TBuf8<128> f8;
	f8.Copy(file.Right(128));
	// the installer picks its plug-in by the MIME type apparc's recognizers give the file: print it (a third-party
	// recognizer that claims the file makes the install fail with KSWInstErrPackageNotSupported, -30474)
	TBuf8<256> head;
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		RFile f;
		if (f.Open(fs, file, EFileRead | EFileShareReadersOnly) == KErrNone)
			{
			f.Read(head);
			f.Close();
			}
		fs.Close();
		}
	RApaLsSession ls;
	if (ls.Connect() == KErrNone)
		{
		TDataRecognitionResult rec;
		TInt rerr = ls.RecognizeData(file, head, rec);
		TPtrC8 mime = rec.iDataType.Des8();
		RDebug::Printf("PH: [sisinst] %S: recognized %d '%S' confidence %d", &f8, rerr, &mime, rec.iConfidence);
		ls.Close();
		}

	SwiUI::RSWInstSilentLauncher launcher;
	TInt err = launcher.Connect();
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
