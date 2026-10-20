// svgtest.cpp -- OURS (2026-10-12, the SVG-Tiny icon bug): decode every SVG entry of the .mif files given on the command line
// (default: z:\resource\apps\svgtest.mif) the way the icon server does (AknSvgFormatHandler.cpp: CSvgEngineInterfaceImpl::NewL with
// an un-created dummy bitmap + ESVGRendererSW, PrepareDom on the entry's bytes, ContentDimensions). Prints "PH:" lines: the kernel
// seam forwards them to PhoneLab on the phone; on QEMU every RDebug line is on the serial log.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <fbs.h>
#include <SVGEngineInterfaceImpl.h>
#include <SVGRequestObserver.h>

class TObserver : public MSvgRequestObserver
	{
public:
	void UpdateScreen() {}
	TBool ScriptCall(const TDesC&, CSvgElementImpl*) { return EFalse; }
	TInt FetchImage(const TDesC&, RFs&, RFile&) { return KErrNotSupported; }
	TInt FetchFont(const TDesC&, RFs&, RFile&) { return KErrNotSupported; }
	void UpdatePresentation(const TInt32&) {}
	};

static TUint32 U32(const TDesC8& aD, TInt aOff)
	{
	if (aOff < 0 || aOff + 4 > aD.Length())
		return 0;
	return aD[aOff] | (aD[aOff + 1] << 8) | (aD[aOff + 2] << 16) | ((TUint32)aD[aOff + 3] << 24);
	}

static void TestMifL(RFs& aFs, const TDesC& aName, TInt aMode)
	{
	RFile f;
	TInt r = f.Open(aFs, aName, EFileRead | EFileShareReadersOnly);
	TBuf8<128> name8;
	name8.Copy(aName.Left(120));
	if (r != KErrNone)
		{
		RDebug::Printf("PH: svgtest %S: open %d", &name8, r);
		return;
		}
	TInt size = 0;
	f.Size(size);
	HBufC8* buf = HBufC8::NewLC(size);
	TPtr8 d = buf->Des();
	f.Read(d);
	f.Close();
	// MIF: 'B##4', version, offset of the index, number of entries; index = (offset, length) pairs;
	// entry: 'C##4', version, data offset (from the entry), data length, type (1 = SVG, 2 = NVG ...)
	const TUint32 indexOff = U32(d, 8), count = U32(d, 12);
	RDebug::Printf("PH: svgtest %S: %d bytes, %d entries, renderer %d", &name8, size, count, aMode);
	TFontSpec spec;
	CFbsBitmap* dummy = new (ELeave) CFbsBitmap;
	CleanupStack::PushL(dummy);
	TObserver obs;
	CSvgEngineInterfaceImpl* engine = CSvgEngineInterfaceImpl::NewL(dummy, &obs, spec, (SVGRendererId)aMode);
	CleanupStack::PushL(engine);
	engine->SetBackgroundColor(aMode == ESVGRendererTLV ? 0xFFFFFFFF : 0);
	engine->SetDRMMode(EFalse);
	for (TUint i = 0; i < count && i < 8; ++i)
		{
		const TInt eo = (TInt)U32(d, indexOff + 8 * i), el = (TInt)U32(d, indexOff + 8 * i + 4);
		if (!eo || !el)
			continue;
		const TInt dataOff = eo + (TInt)U32(d, eo + 8), dataLen = (TInt)U32(d, eo + 12), type = (TInt)U32(d, eo + 16);
		if (dataOff + dataLen > d.Length())
			continue;
		TPtrC8 data = d.Mid(dataOff, dataLen);
		RDebug::Printf("PH: svgtest  entry %d type %d len %d magic %02x %02x %02x %02x", i, type, dataLen,
			dataLen > 0 ? data[0] : 0, dataLen > 1 ? data[1] : 0, dataLen > 2 ? data[2] : 0, dataLen > 3 ? data[3] : 0);
		if (type != 1)
			continue;
		TInt handle = 0;
		MSvgError* err = engine->PrepareDom(data, handle);
		if (err && err->HasError())
			{
			TBuf8<120> desc;
			desc.Copy(err->Description().Left(120));
			RDebug::Printf("PH: svgtest   PrepareDom: code %d sys %d warning %d '%S'", (TInt)err->ErrorCode(),
				err->SystemErrorCode(), (TInt)err->IsWarning(), &desc);
			}
		else
			RDebug::Printf("PH: svgtest   PrepareDom ok, handle %d", handle);
		if (handle)
			{
			err = engine->UseDom(handle, NULL, NULL, TSize(0, 0), ENone, ENone);
			const TSize s = engine->ContentDimensions();
			RDebug::Printf("PH: svgtest   UseDom err %d, ContentDimensions %dx%d", err && err->HasError() ? (TInt)err->ErrorCode() : 0,
				s.iWidth, s.iHeight);
			engine->DeleteDom(handle);
			}
		}
	CleanupStack::PopAndDestroy(3, buf);	// engine, dummy, buf
	}

static void MainL()
	{
	FbsStartup();
	User::LeaveIfError(RFbsSession::Connect());
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TBuf<256> cmd;
	User::CommandLine(cmd);
	if (cmd.Length() == 0)
		{
		// no arguments (SSM_TEST=svgtest on the phone): the installed apps' icons, c:\resource\apps\*.mif, once C: is up
		User::After(20000000);
		CDir* dir = NULL;
		_LIT(KApps, "c:\\resource\\apps\\");
		TInt r = fs.GetDir(_L("c:\\resource\\apps\\*.mif"), KEntryAttNormal, ESortByName, dir);
		RDebug::Printf("PH: svgtest c:\\resource\\apps\\*.mif: %d, %d files", r, dir ? dir->Count() : 0);
		for (TInt i = 0; dir && i < dir->Count() && i < 12; ++i)
			{
			TFileName name(KApps);
			name.Append((*dir)[i].iName);
			TRAP(r, TestMifL(fs, name, ESVGRendererSW));
			if (r == KErrNone)
				TRAP(r, TestMifL(fs, name, ESVGRendererTLV));	// the icon server's path (AknIcon _NGATESTING: SVG -> CAknNGAFormatHandler)
			if (r != KErrNone)
				RDebug::Printf("PH: svgtest  left %d", r);
			}
		delete dir;
		}
	TLex lex(cmd);
	for (TPtrC tok = lex.NextToken(); tok.Length(); tok.Set(lex.NextToken()))
		{
		TRAPD(r, TestMifL(fs, tok, ESVGRendererSW));
		if (r == KErrNone)
			TRAP(r, TestMifL(fs, tok, ESVGRendererTLV));
		if (r != KErrNone)
			RDebug::Printf("PH: svgtest  left %d", r);
		}
	CleanupStack::PopAndDestroy(&fs);
	RFbsSession::Disconnect();
	RDebug::Printf("PH: svgtest done");
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	CActiveScheduler* as = new CActiveScheduler;
	CActiveScheduler::Install(as);
	TRAPD(r, MainL());
	if (r != KErrNone)
		RDebug::Printf("PH: svgtest left %d", r);
	delete as;
	delete cleanup;
	return 0;
	}
