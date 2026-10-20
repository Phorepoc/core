// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// dscadd.cpp -- OURS (X4.78): register a program in S60's startup list (the DSC store), the way SWI's
// startuplistupdater does for an installed application whose package declares "start at boot"
// (appinstaller/AppinstUi/startuplistupdater: OpenL, CreateDscL if needed, CDscItem::NewLC(exe, args), AddItemL).
// At every normal boot SSM's r_cmd_amastart (noncriticalcmdlist_ext.rss) runs loadamastart.dll, which starts each
// registered item. Used to put the regression cycler (bootui with C:\bootui.txt) into an SSM boot without bootui
// being the system starter:
//
//     dscadd <executable> [arguments]        e.g.  dscadd z:\sys\bin\bootui.exe
//
// Prints "[dscadd] ..." on the serial log; exits with the error code. SWI does this for real apps once it is built.
#include <e32base.h>
#include <e32debug.h>
#include <dscstore.h>
#include <dscitem.h>

static void AddL(const TDesC& aExe, const TDesC& aArgs)
	{
	RDscStore store;
	store.OpenL();
	CleanupClosePushL(store);
	if (!store.DscExistsL())
		store.CreateDscL();
	CDscItem* item = CDscItem::NewLC(aExe, aArgs);
	if (store.ItemExistsL(*item))
		RDebug::Print(_L("[dscadd] %S already in the startup list"), &aExe);
	else
		{
		store.AddItemL(*item);
		RDebug::Print(_L("[dscadd] %S added to the startup list"), &aExe);
		}
	CleanupStack::PopAndDestroy(2, &store);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TBuf<256> cmd;
	User::CommandLine(cmd);
	cmd.Trim();
	TInt sp = cmd.Locate(' ');
	TPtrC exe = sp < 0 ? TPtrC(cmd) : cmd.Left(sp);
	TPtrC args = sp < 0 ? TPtrC(KNullDesC) : cmd.Mid(sp + 1);
	TInt err = exe.Length() ? KErrNone : KErrArgument;
	if (!err)
		TRAP(err, AddL(exe, args));
	if (err)
		RDebug::Print(_L("[dscadd] failed: %d"), err);
	delete cleanup;
	return err;
	}
