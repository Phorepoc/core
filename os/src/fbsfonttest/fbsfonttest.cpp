// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
#include <e32base.h>
#include <e32debug.h>
#include <fbs.h>
#include <gdi.h>
#include <bitdev.h>
#include <bitstd.h>

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
			TFontSpec spec(TPtrC(families[fam]), 0);
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
