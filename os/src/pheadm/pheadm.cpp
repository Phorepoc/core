// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheadm.cpp -- OURS: the application installer a Smart Installer wrapper hands its payload to.
//
// A Qt application of the S^3/Belle era often ships wrapped: the wrapper package (0x2002CCCF) installs a bootstrap
// (0x2002CCCD) and puts the real application SIS in \private\2002CCCE\import\. The bootstrap checks that package
// 0x2002CCCE (the installer that was downloaded from a server which no longer exists) is installed and starts its
// ADM.exe. This is that executable: it installs every SIS waiting in the import folder through the platform installer,
// whose own dependency check names any Qt package still missing, and removes each one installed. The bootstrap's
// command line (control flags, IAP, the SIS name) is not needed: the folder is the queue.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <swinstapi.h>
#include <swinstdefs.h>
#include <aknglobalnote.h>

_LIT(KImportDir, "?:\\private\\2002CCCE\\import\\");

static void Note(const TDesC& aName, TInt aErr)
	{
	TBuf<256> text;
	if (aErr == SwiUI::KSWInstErrMissingDependency)
		text.Format(_L("%S: a component it needs is not installed (Qt, Qt Mobility or Qt Quick Components)"), &aName);
	else
		text.Format(_L("%S: installation failed (%d)"), &aName, aErr);
	CAknGlobalNote* note = NULL;
	TRAPD(err, note = CAknGlobalNote::NewL());
	if (err == KErrNone)
		{
		TRAP_IGNORE(note->ShowNoteL(EAknGlobalErrorNote, text));
		delete note;
		}
	}

static TInt InstallOne(RFs& aFs, const TDesC& aPath, const TDesC& aName)
	{
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
	opts.iDownload = SwiUI::EPolicyNotAllowed;
	opts.iDrive = aPath[0];		// the drive the wrapper was installed to
	opts.iUsePhoneLang = ETrue;
	opts.iUpgradeData = SwiUI::EPolicyAllowed;
	SwiUI::TInstallOptionsPckg pckg(opts);

	// the folder is private to this SID: hand the installer an open file, not a name it cannot read
	RFile file;
	TInt err = file.Open(aFs, aPath, EFileRead | EFileShareReadersOnly);
	if (err == KErrNone)
		{
		SwiUI::RSWInstSilentLauncher launcher;
		err = launcher.Connect();
		if (err == KErrNone)
			{
			err = launcher.SilentInstall(file, pckg);
			launcher.Close();
			}
		file.Close();
		}
	TBuf8<128> n8;
	n8.Copy(aName.Left(128));
	RDebug::Printf("PH: [pheadm] %S: %s (%d)", &n8, err == KErrNone ? "INSTALLED" : "FAILED", err);
	if (err == KErrNone)
		aFs.Delete(aPath);
	else
		Note(aName, err);
	return err;
	}

static TInt Run()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	User::LeaveIfError(fs.ShareProtected());
	TInt installed = 0;
	TInt failed = 0;
	TDriveList drives;
	User::LeaveIfError(fs.DriveList(drives));
	for (TInt d = EDriveA; d <= EDriveZ; d++)
		{
		if (!drives[d] || d == EDriveZ)
			continue;
		TFileName dir(KImportDir);
		dir[0] = TChar('a' + d);
		CDir* entries = NULL;
		if (fs.GetDir(dir, KEntryAttNormal, ESortByName, entries) != KErrNone)
			continue;
		CleanupStack::PushL(entries);
		for (TInt i = 0; i < entries->Count(); i++)
			{
			const TDesC& name = (*entries)[i].iName;
			TParsePtrC parse(name);
			if (parse.Ext().CompareF(_L(".sis")) != 0 && parse.Ext().CompareF(_L(".sisx")) != 0)
				continue;
			TFileName path(dir);
			path.Append(name);
			if (InstallOne(fs, path, name) == KErrNone)
				installed++;
			else
				failed++;
			}
		CleanupStack::PopAndDestroy(entries);
		}
	RDebug::Printf("PH: [pheadm] done: %d installed, %d failed", installed, failed);
	CleanupStack::PopAndDestroy(&fs);
	return failed ? KErrGeneral : KErrNone;
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
			RDebug::Printf("PH: [pheadm] left %d", leave);
			err = leave;
			}
		delete cleanup;
		}
	return err;
	}
