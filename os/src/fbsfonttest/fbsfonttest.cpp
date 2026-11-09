// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
#include <e32base.h>
#include <e32debug.h>
#include <fbs.h>
#include <gdi.h>
#include <bitdev.h>
#include <bitstd.h>

// RFontTable (gdi.dll, publishedPartner gdiplatapi.h): declared here, as the header is not in the image build's include path
class RFontTable
	{
public:
	IMPORT_C RFontTable();
	IMPORT_C TInt Open(CFont& aFont, TUint32 aTag);
	IMPORT_C TInt TableLength() const;
	IMPORT_C const TUint8* TableContent() const;
	IMPORT_C void Close();
private:
	TAny* iTableContent;
	TInt iLength;
	const CFont* iFont;
	TUint32 iTag;
	};

_LIT(KFace, "Nokia Sans S60");

static void DrawWith(CFbsBitGc* aGc, CFont* aFont, const TDesC& aLabel)
	{
	aGc->UseFont(aFont);
	aGc->DrawText(_L("Hello Wxyz 0123 Ag"), TPoint(2, 20));
	TInt w = aFont->TextWidthInPixels(_L("Hello Wxyz 0123 Ag"));
	// the open-font glyph path (what Qt's S60 font engine uses): the glyph bitmaps, one rasterise request each
	CFbsFont* fbsFont = static_cast<CFbsFont*>(aFont);
	for (TUint code = 0x20; code < 0x7F; ++code)
		{
		TOpenFontCharMetrics m;
		const TUint8* bits = NULL;
		TSize bsize;
		fbsFont->GetCharacterData(code, m, bits, bsize);
		}
	TInt a = aFont->AscentInPixels();
	TInt h = aFont->HeightInPixels();
	RDebug::Printf("[fbsfonttest] %S: width %d ascent %d height %d", &aLabel, w, a, h);
	aGc->DiscardFont();
	}


static void ThreadBodyL(TInt aId)
	{
	User::LeaveIfError(RFbsSession::Connect());		// a font and bitmap server session is per thread
	CFbsTypefaceStore* ts = CFbsTypefaceStore::NewL(NULL);
	CleanupStack::PushL(ts);
	static const TUint32 tags[] = { 0x636d6170, 0x68656164, 0x68686561, 0x686d7478, 0x6d617870, 0x6e616d65, 0x4f532f32, 0x706f7374 };
	for (TInt round = 0; round < 60; ++round)
		for (TInt size = 1 + aId; size <= 80; size += 3)
			{
			TFontSpec spec(KFace, 0);
			spec.iHeight = size;
			CFont* f = NULL;
			if (ts->GetNearestFontInPixels(f, spec) != KErrNone)
				continue;
			CFbsFont* fbsFont = static_cast<CFbsFont*>(f);
			for (TUint code = 0x41 + aId; code < 0x5A; code += 4)
				{
				TOpenFontCharMetrics m;
				const TUint8* bits = NULL;
				TSize bsize;
				fbsFont->GetCharacterData(code, m, bits, bsize);
				}
			for (TUint k = 0; k < sizeof(tags) / sizeof(tags[0]); ++k)
				{
				RFontTable tbl;
				if (tbl.Open(*fbsFont, tags[k]) == KErrNone)
					{
					volatile TUint8 sum = 0;
					const TUint8* p = tbl.TableContent();
					for (TInt n = 0; p && n < 64; ++n)
						sum += p[n];
					tbl.Close();
					}
				}
			ts->ReleaseFont(f);
			}
	CleanupStack::PopAndDestroy(ts);
	RFbsSession::Disconnect();
	}

static TInt ThreadFn(TAny* aArg)
	{
	const TInt id = (TInt)aArg;
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, ThreadBodyL(id));
	delete cleanup;
	return err;
	}

static void RunL()
	{
	User::LeaveIfError(RFbsSession::Connect());
	CFbsTypefaceStore* ts = CFbsTypefaceStore::NewL(NULL);
	CleanupStack::PushL(ts);
	CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
	CleanupStack::PushL(bmp);
	User::LeaveIfError(bmp->Create(TSize(200, 40), EGray256));
	CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(bmp);
	CleanupStack::PushL(dev);
	CFbsBitGc* gc = NULL;
	User::LeaveIfError(dev->CreateContext(gc));
	CleanupStack::PushL(gc);
	// what Orbit's Hb does when it has no typeface metrics (HbTypefaceInfoDatabase::autoGenerateMetrics): every size from 1 to
	// 100 of each family, measured through the font and bitmap server
	static const TText* const families[] = { _S("Nokia Sans"), _S("Nokia Sans Title SemiBold"), _S("OCR A Extended"),
		_S("Series 60 Sans"), _S("Series 60 Sans TitleSmBd"), _S("Series 60 ZDigi") };
	for (TInt fam = 0; fam < 6; ++fam)
		for (TInt size = 1; size <= 100; ++size)
			{
			TPtrC famName(families[fam]); if (famName.Length() > KMaxTypefaceNameLength) famName.Set(famName.Left(KMaxTypefaceNameLength));
			TFontSpec spec(famName, 0);
			spec.iHeight = size;
			CFont* f = NULL;
			TInt r = ts->GetNearestFontInPixels(f, spec);
			if (r == KErrNone)
				{
				CFbsFont* fbsFont = static_cast<CFbsFont*>(f);
				TOpenFontCharMetrics m;
				const TUint8* bits = NULL;
				TSize bsize;
				fbsFont->GetCharacterData('M', m, bits, bsize);
				fbsFont->GetCharacterData('g', m, bits, bsize);
				(void)f->HeightInPixels();
				(void)f->AscentInPixels();
				(void)f->TextWidthInPixels(_L("Mg"));
				ts->ReleaseFont(f);
				}
			}
	RDebug::Printf("[fbsfonttest] autogenerate-like sweep done");
	const TInt sizes[] = { 0, 0, 0, 1, 2, 0, 12, 0, 20, 0 };
	for (TInt round = 0; round < 40; ++round)
		for (TUint i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
			{
			TFontSpec spec(KFace, 0);
			CFont* f = NULL;
			TInt r;
			TBuf<40> label;
			spec.iHeight = sizes[i];
			r = ts->GetNearestFontInPixels(f, spec);
			label.Format(_L("InPixels h=%d r=%d"), sizes[i], r);
			if (r == KErrNone) { DrawWith(gc, f, label); ts->ReleaseFont(f); }
			else RDebug::Printf("[fbsfonttest] %S", &label);
			f = NULL;
			r = ts->GetNearestFontToDesignHeightInPixels(f, spec);
			label.Format(_L("Design h=%d r=%d"), sizes[i], r);
			if (r == KErrNone) { DrawWith(gc, f, label); ts->ReleaseFont(f); }
			else RDebug::Printf("[fbsfonttest] %S", &label);
			f = NULL;
			spec.iHeight = 14;
			r = ts->GetNearestFontToMaxHeightInPixels(f, spec, sizes[i]);
			label.Format(_L("MaxHeight m=%d r=%d"), sizes[i], r);
			if (r == KErrNone) { DrawWith(gc, f, label); ts->ReleaseFont(f); }
			else RDebug::Printf("[fbsfonttest] %S", &label);
			}
	// several client threads at once, font tables included (what Qt's font engine does while Hb's sweep runs)
	RDebug::Printf("[fbsfonttest] threads...");
	RThread th[4];
	TRequestStatus ths[4];
	for (TInt i = 0; i < 4; ++i)
		{
		TBuf<16> name;
		name.Format(_L("fbsthr%d"), i);
		User::LeaveIfError(th[i].Create(name, ThreadFn, 0x8000, NULL, (TAny*)i));
		th[i].Logon(ths[i]);
		th[i].Resume();
		}
	for (TInt i = 0; i < 4; ++i)
		{
		User::WaitForRequest(ths[i]);
		RDebug::Printf("[fbsfonttest] thread %d exit %d type %d", i, th[i].ExitReason(), (TInt)th[i].ExitType());
		th[i].Close();
		}
	RDebug::Printf("[fbsfonttest] done, no fault");
	CleanupStack::PopAndDestroy(4);			// gc, dev, bmp, ts
	RFbsSession::Disconnect();
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(r, RunL());
	RDebug::Printf("[fbsfonttest] exit %d", r);
	delete cleanup;
	return r;
	}
