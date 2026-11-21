// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// timageviewer -- OURS: an Avkon re-make of the imaging framework's TImageViewer test application (see timageviewer.mmp).
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <avkon.hrh>
#include <f32file.h>
#include <bautils.h>
#include <fbs.h>
#include <imageconversion.h>
#include <bitmaptransforms.h>
#include <timageviewer.rsg>
#include "timageviewer.hrh"

const TUid KUidTImageViewer = { 0x101F45EA };

// Decodes one picture (asynchronously) into a bitmap and draws it, scaled to fit.
class CImageView : public CCoeControl
	{
public:
	~CImageView() { delete iRunner; delete iRotator; delete iDecoder; delete iBitmap; }
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		iRunner = new (ELeave) CRunner(*this);
		ActivateL();
		}
	void ShowL(const TDesC& aFile)
		{
		StopDecode();
		iName = aFile;
		iStatusText = _L("Loading...");
		delete iBitmap; iBitmap = NULL;
		DrawNow();
		RDebug::Printf("[timageviewer] open %S", &aFile);
		// (read before the decoder opens the file: it holds it with a share mode a second open would conflict with)
		const TInt o = ExifOrientation(aFile);
		iDecoder = CImageDecoder::FileNewL(iEikonEnv->FsSession(), aFile);
		const TFrameInfo info = iDecoder->FrameInfo(0);
		// a decode at a reduction factor (1/2, 1/4, 1/8 of each side) for a picture beyond ~2.4 Mpixel: a 12 Mpixel JPEG
		// as a full 32-bit bitmap is 48 MB. The destination bitmap's size selects the reduced decode.
		TSize size = info.iOverallSizeInPixels;
		for (TInt factor = 1; factor <= 3 && (TInt64)size.iWidth * size.iHeight > 2400000; factor++)
			{
			TSize reduced;
			if (iDecoder->ReducedSize(info.iOverallSizeInPixels, factor, reduced) != KErrNone)
				break;
			size = reduced;
			}
		RDebug::Printf("[timageviewer] frame %dx%d, decode at %dx%d", info.iOverallSizeInPixels.iWidth, info.iOverallSizeInPixels.iHeight, size.iWidth, size.iHeight);
		iBitmap = new (ELeave) CFbsBitmap;
		User::LeaveIfError(iBitmap->Create(size, EColor16MU));
		// the EXIF orientation of a JPEG: the steps that turn the decoded bitmap upright
		iStepCount = 0; iStepAt = 0;
		static const TInt KSteps[9][2] = { {-1,-1}, {-1,-1}, {CBitmapRotator::EMirrorVerticalAxis,-1}, {CBitmapRotator::ERotation180DegreesClockwise,-1},
			{CBitmapRotator::EMirrorHorizontalAxis,-1}, {CBitmapRotator::ERotation90DegreesClockwise,CBitmapRotator::EMirrorVerticalAxis},
			{CBitmapRotator::ERotation90DegreesClockwise,-1}, {CBitmapRotator::ERotation270DegreesClockwise,CBitmapRotator::EMirrorVerticalAxis},
			{CBitmapRotator::ERotation270DegreesClockwise,-1} };
		if (o >= 2 && o <= 8)
			for (TInt k = 0; k < 2; k++)
				if (KSteps[o][k] >= 0) iSteps[iStepCount++] = KSteps[o][k];
		RDebug::Printf("[timageviewer] exif orientation %d, %d step(s)", o, iStepCount);
		iDecoder->Convert(&iRunner->iStatus, *iBitmap, 0);
		iRunner->Go();
		}
	// the orientation tag (0x0112) of a JPEG's EXIF block, 1 when there is none
	TInt ExifOrientation(const TDesC& aFile)
		{
		RFile f;
		if (f.Open(iEikonEnv->FsSession(), aFile, EFileRead | EFileShareAny) != KErrNone)
			return 1;
		HBufC8* buf = HBufC8::New(65536);
		TInt result = 1;
		if (buf)
			{
			TPtr8 p = buf->Des();
			if (f.Read(p) == KErrNone && p.Length() > 20 && p[0] == 0xFF && p[1] == 0xD8)
				{
				TInt i = 2;
				while (i + 4 < p.Length())
					{
					if (p[i] != 0xFF) break;
					const TInt marker = p[i + 1];
					const TInt len = (p[i + 2] << 8) | p[i + 3];
					if (marker == 0xE1 && i + 10 < p.Length() && p[i + 4] == 'E' && p[i + 5] == 'x' && p[i + 6] == 'i' && p[i + 7] == 'f')
						{
						const TInt t = i + 10;                      // the TIFF header
						if (t + 8 > p.Length()) break;
						const TBool le = p[t] == 'I';
						#define U16(o) (le ? (p[(o)] | (p[(o) + 1] << 8)) : ((p[(o)] << 8) | p[(o) + 1]))
						#define U32(o) (le ? (p[(o)] | (p[(o) + 1] << 8) | (p[(o) + 2] << 16) | (p[(o) + 3] << 24)) : ((p[(o)] << 24) | (p[(o) + 1] << 16) | (p[(o) + 2] << 8) | p[(o) + 3]))
						const TInt ifd = t + U32(t + 4);
						if (ifd + 2 <= p.Length())
							{
							const TInt n = U16(ifd);
							for (TInt e = 0; e < n && ifd + 2 + (e + 1) * 12 <= p.Length(); e++)
								{
								const TInt at = ifd + 2 + e * 12;
								if (U16(at) == 0x0112) { result = U16(at + 8); break; }
								}
							}
						#undef U16
						#undef U32
						break;
						}
					if (marker == 0xDA) break;
					i += 2 + len;
					}
				}
			delete buf;
			}
		f.Close();
		return (result >= 1 && result <= 8) ? result : 1;
		}
	// the decoder (or a transform step) completed
	void StepDone(TInt aErr)
		{
		if (aErr == KErrNone && iStepAt < iStepCount)
			{
			TRAPD(e, StartRotateL(iSteps[iStepAt++]));
			if (e == KErrNone) return;
			aErr = e;
			}
		Done(aErr);
		}
	void StartRotateL(TInt aAngle)
		{
		if (!iRotator) iRotator = CBitmapRotator::NewL();
		iRotator->Rotate(&iRunner->iStatus, *iBitmap, (CBitmapRotator::TRotationAngle)aAngle);
		iRunner->Go();
		}
	void Done(TInt aErr)
		{
		delete iDecoder; iDecoder = NULL;
		if (aErr != KErrNone)
			{
			iStatusText.Format(_L("Cannot decode (%d)"), aErr);
			delete iBitmap; iBitmap = NULL;
			RDebug::Printf("[timageviewer] decode failed %d", aErr);
			}
		else
			{
			iStatusText.Zero();
			RDebug::Printf("[timageviewer] decoded %dx%d", iBitmap->SizeInPixels().iWidth, iBitmap->SizeInPixels().iHeight);
			}
		DrawNow();
		}
	const TDesC& Name() const { return iName; }
	void StopDecode()
		{
		if (iRunner) iRunner->Cancel();
		delete iDecoder; iDecoder = NULL;
		}
private:
	class CRunner : public CActive
		{
	public:
		CRunner(CImageView& aView) : CActive(EPriorityStandard), iView(aView) { CActiveScheduler::Add(this); }
		~CRunner() { Cancel(); }
		void Go() { SetActive(); }
		void RunL() { iView.StepDone(iStatus.Int()); }
		void DoCancel() { if (iView.iDecoder) iView.iDecoder->Cancel(); if (iView.iRotator) iView.iRotator->Cancel(); }
		CImageView& iView;
		};
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(KRgbBlack);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.DrawRect(Rect());
		if (iBitmap && iStatusText.Length() == 0)
			{
			TSize s = iBitmap->SizeInPixels();
			TRect r = Rect();
			if (s.iWidth > 0 && s.iHeight > 0)
				{
				TInt w = r.Width(), h = (s.iHeight * w) / s.iWidth;
				if (h > r.Height()) { h = r.Height(); w = (s.iWidth * h) / s.iHeight; }
				TPoint tl(r.iTl.iX + (r.Width() - w) / 2, r.iTl.iY + (r.Height() - h) / 2);
				gc.DrawBitmap(TRect(tl, TSize(w, h)), iBitmap);
				}
			}
		else
			{
			gc.UseFont(iEikonEnv->NormalFont());
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenColor(KRgbWhite);
			if (iStatusText.Length())
				gc.DrawText(iStatusText, TPoint(10, 40));
			else
				gc.DrawText(_L("No picture"), TPoint(10, 40));
			gc.DiscardFont();
			}
		}
	CImageDecoder* iDecoder = NULL;
	CFbsBitmap* iBitmap = NULL;
	CRunner* iRunner = NULL;
	CBitmapRotator* iRotator = NULL;
	TInt iSteps[2];
	TInt iStepCount = 0;
	TInt iStepAt = 0;
	TFileName iName;
	TBuf<48> iStatusText;
	friend class CRunner;
	};

class CViewerAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		iView = new (ELeave) CImageView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		}
	~CViewerAppUi()
		{
		if (iView) RemoveFromStack(iView);
		delete iView;
		}
	void OpenFileL(const TDesC& aFileName) { iView->ShowL(aFileName); }
	TBool ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& aTail)
		{
		if (aDocumentName.Length())
			iView->ShowL(aDocumentName);
		return CAknAppUi::ProcessCommandParametersL(aCommand, aDocumentName, aTail);
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EImgCmdNext: StepL(1); break;
		case EImgCmdPrev: StepL(-1); break;
		case EAknSoftkeyExit:
		case EEikCmdExit: Exit(); break;
			}
		}
private:
	// the next picture of the folder of the current one (alphabetical, wrapping)
	void StepL(TInt aDir)
		{
		TParsePtrC p(iView->Name());
		TFileName dir = p.DriveAndPath();
		if (dir.Length() == 0) dir = _L("C:\\Data\\Images\\");
		CDir* list = NULL;
		User::LeaveIfError(iEikonEnv->FsSession().GetDir(dir, KEntryAttNormal, ESortByName, list));
		CleanupStack::PushL(list);
		RArray<TFileName> pics;
		CleanupClosePushL(pics);
		for (TInt i = 0; i < list->Count(); i++)
			{
			TParsePtrC e((*list)[i].iName);
			TFileName ext = e.Ext(); ext.LowerCase();
			if (ext == _L(".jpg") || ext == _L(".jpeg") || ext == _L(".png") || ext == _L(".gif") || ext == _L(".bmp"))
				{
				TFileName full = dir; full.Append((*list)[i].iName);
				pics.AppendL(full);
				}
			}
		if (pics.Count())
			{
			TInt at = -1;
			for (TInt i = 0; i < pics.Count(); i++) if (pics[i].CompareF(iView->Name()) == 0) at = i;
			at = (at + aDir + pics.Count()) % pics.Count();
			iView->ShowL(pics[at]);
			}
		CleanupStack::PopAndDestroy(2);
		}
	CImageView* iView = NULL;
	};

class CViewerDocument : public CAknDocument
	{
public:
	CViewerDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CViewerAppUi; }
	// a picture handed over as an open file (File Manager, the document handler)
	void OpenFileL(CFileStore*& aFileStore, RFile& aFile)
		{
		aFileStore = NULL;
		TFileName name;
		aFile.FullName(name);
		CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
		if (ui) static_cast<CViewerAppUi*>(ui)->OpenFileL(name);
		}
	};

class CViewerApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidTImageViewer; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CViewerDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CViewerApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
