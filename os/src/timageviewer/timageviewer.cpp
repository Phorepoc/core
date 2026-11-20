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
#include <timageviewer.rsg>
#include "timageviewer.hrh"

const TUid KUidTImageViewer = { 0x101F45EA };

// Decodes one picture (asynchronously) into a bitmap and draws it, scaled to fit.
class CImageView : public CCoeControl
	{
public:
	~CImageView() { delete iRunner; delete iDecoder; delete iBitmap; }
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
		iDecoder->Convert(&iRunner->iStatus, *iBitmap, 0);
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
		void RunL() { iView.Done(iStatus.Int()); }
		void DoCancel() { if (iView.iDecoder) iView.iDecoder->Cancel(); }
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
