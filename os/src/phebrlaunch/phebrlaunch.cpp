// phebrlaunch.cpp -- OURS: launch BrowserNG on a URL at boot (the phone's equivalent of the x86 BOOTLIST '0x10008D39=<url>').
// The URL is KDefaultUrl (edit + rebuild; one-line change).
#include <e32base.h>
#include <e32debug.h>
#include <apgcli.h>
#include <apacmdln.h>

_LIT(KDefaultUrl, "https://example.com/");
const TUid KBrowserUid = {0x10008D39};

static TInt Launch(const TDesC& aUrl)
	{
	RApaLsSession ls;
	TInt r = ls.Connect();
	if (r)
		return r;
	TApaAppInfo info;
	r = ls.GetAppInfo(info, KBrowserUid);
	if (!r)
		{
		CApaCommandLine* cmd = NULL;
		TRAP(r, cmd = CApaCommandLine::NewL());
		if (!r)
			{
			TRAP(r, cmd->SetExecutableNameL(info.iFullName));
			if (!r)
				{
				TRAP(r, cmd->SetDocumentNameL(aUrl));
				cmd->SetCommandL(EApaCommandOpen);
				}
			if (!r)
				r = ls.StartApp(*cmd);
			delete cmd;
			}
		}
	ls.Close();
	return r;
	}

TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	// join the WLAN first (wlanjoin.exe, the default SSM_TEST of the other images): without a network every BrowserNG load
	// fails and the connection-error notes flood the screen (boot 639)
	RProcess join;
	TInt jr = join.Create(_L("wlanjoin.exe"), KNullDesC);
	if (jr == KErrNone)
		{
		TRequestStatus st;
		join.Logon(st);
		join.Resume();
		User::WaitForRequest(st);
		jr = st.Int();
		join.Close();
		}
	RDebug::Printf("PH: [phebrlaunch] wlanjoin: %d", jr);
	// the WLAN IAPs exist now (wlanjoin / the stored "ASUS"): cmsetup puts them into the "Internet" destination, which
	// BrowserNG's own connection start selects (OCC: KBrowserOccNGDefaultSnapId 0 -> the Internet-purpose SNAP). Without
	// it NetworkConnectionNeededL leaves -1 and phebrctl falls back to starting the IAP itself
	RProcess cms;
	TInt cr = cms.Create(_L("cmsetup.exe"), KNullDesC);
	if (cr == KErrNone)
		{
		TRequestStatus cs;
		cms.Logon(cs);
		cms.Resume();
		User::WaitForRequest(cs);
		cr = cs.Int();
		cms.Close();
		}
	RDebug::Printf("PH: [phebrlaunch] cmsetup: %d", cr);
	// SSM starts the non-critical list early: retry until the app list is populated (GetAppInfo -> KErrNotFound before that)
	TInt r = KErrNotFound;
	for (TInt i = 0; i < 90 && r; ++i)
		{
		User::After(2000000);
		r = Launch(KDefaultUrl);
		RDebug::Printf("PH: [phebrlaunch] try %d: %d", i, r);
		}
	delete tc;
	return r;
	}
