// OURS: the REAL AlfRenderStage/src/alfrssendbuffer.cpp with the "PH:" lines the stage lacks around starting the alf server
// (its own error line, RDebug::Print without the PH: prefix, is dropped by the kernel log filter).
#include <alfdecoderserverclient.h>
#include <e32debug.h>
struct TPhAlfStarter
	{
	static void StartL(TRequestStatus& aStatus, TBool aCreateProcess)
		{
		RDebug::Printf("PH: alf: render stage -> AlfServerStarter::StartL (process=%d)", aCreateProcess);
		TRAPD(err, AlfServerStarter::StartL(aStatus, aCreateProcess));
		RDebug::Printf("PH: alf: AlfServerStarter::StartL returned %d (the alfredserver thread is created, running)", err);
		User::LeaveIfError(err);
		}
	};
#define AlfServerStarter TPhAlfStarter
#include "../../../baseline/symbian-2010/oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/AlfRenderStage/src/alfrssendbuffer.cpp"
