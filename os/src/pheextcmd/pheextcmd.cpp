// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheextcmd.cpp -- OURS: an SSM custom command that runs a System Startup Extension plug-in (ECOM interface 0x10205067,
// devicesrv_plat/system_startup_extension_plugin_api). S60's own startup ran these plug-ins from its command lists; that
// runner was never published, and the API header itself says "use ssmcustomcommand.h instead". This command is that
// bridge: the command list names the plug-in implementation and its parameter string, and the plug-in does the work.
//
// dll_data (SSM_START_CUSTOM_COMMAND) = "<implementation uid in hex> <parameter string>", 8-bit, for example
//   "1020506C enable global notes"  -> CAvkonStartupExtensionPlugIn publishes KUikGlobalNotesAllowed = 1
// The plug-in completes the request itself; its result is the command's result.
#include <e32base.h>
#include <e32debug.h>
#include <ecom/ecom.h>
#include <f32file.h>
#include <ssm/ssmcustomcommand.h>
#include <systemstartupextension.h>

NONSHARABLE_CLASS(CPheExtCmd) : public CBase, public MSsmCustomCommand
	{
public:
	static CPheExtCmd* NewL() { return new (ELeave) CPheExtCmd; }
	~CPheExtCmd() { delete iPlugin; iOwnFs.Close(); if (iDtorKey.iUid) REComSession::DestroyedImplementation(iDtorKey); REComSession::FinalClose(); }
private:
	TInt Initialize(CSsmCustomCommandEnv* aCmdEnv) { iEnv = aCmdEnv; return KErrNone; }
	void Execute(const TDesC8& aParams, TRequestStatus& aRequest);
	void ExecuteCancel() { if (iPlugin) iPlugin->Cancel(); }
	void Close() {}
	void Release() { delete this; }
	void ExecuteL(const TDesC8& aParams, TRequestStatus& aRequest);
private:
	CSsmCustomCommandEnv* iEnv;
	RFs iOwnFs;
	CSystemStartupExtension* iPlugin;
	TUid iDtorKey;
	};

void CPheExtCmd::Execute(const TDesC8& aParams, TRequestStatus& aRequest)
	{
	aRequest = KRequestPending;
	TRAPD(err, ExecuteL(aParams, aRequest));
	RDebug::Printf("PH: pheextcmd '%S' -> %d", &aParams, err);
	if (err != KErrNone)
		{
		TRequestStatus* s = &aRequest;
		User::RequestComplete(s, err);
		}
	}

void CPheExtCmd::ExecuteL(const TDesC8& aParams, TRequestStatus& aRequest)
	{
	TLex8 lex(aParams);
	TUint32 impl = 0;
	User::LeaveIfError(lex.Val(impl, EHex));
	lex.SkipSpace();
	HBufC* params = HBufC::NewLC(lex.Remainder().Length());
	params->Des().Copy(lex.Remainder());

	RFs* fs = &iOwnFs;
	if (iEnv)
		fs = const_cast<RFs*>(&iEnv->Rfs());
	else
		User::LeaveIfError(iOwnFs.Connect());
	iPlugin = static_cast<CSystemStartupExtension*>(
		REComSession::CreateImplementationL(TUid::Uid(impl), iDtorKey, fs));
	iPlugin->ExecuteL(aRequest, *params);		// the plug-in completes aRequest
	CleanupStack::PopAndDestroy(params);
	}

EXPORT_C MSsmCustomCommand* PheExtCmdNewL()
	{
	return CPheExtCmd::NewL();
	}
