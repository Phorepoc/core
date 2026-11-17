// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phecamera.cpp -- OURS: Camera (P1, photo). A native Avkon app over ECam only, so it runs on any device with an ECam
// plug-in: a full-screen viewfinder, and under it a control bar with the last shot's thumbnail (left), the shutter
// (middle) and the camera switch (right, when there is more than one camera). Styled after the Etesian design language
// (https://language.etesian-design.com): black surface, white round shutter, quiet grey secondary controls.
// The still is the largest capture size, as Exif (else JPEG, else a bitmap the app encodes itself), saved to
// C:\Data\Images\IMG_<date>_<time>.jpg; a full-size still takes seconds, so the shutter shows a busy state until
// ImageReady. Without a camera (CCamera::CamerasAvailable() == 0) the screen says "No camera".
// Keys: Select/OK or Enter = shutter; Options menu = take photo / switch camera / exit.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikspane.h>
#include <avkon.hrh>
#include <ecam.h>
#include <fbs.h>
#include <bitdev.h>
#include <bitstd.h>
#include <f32file.h>
#include <imageconversion.h>
#include <phecamera.rsg>
#include "phecamera.hrh"

const TUid KUidPheCamera = { 0xE7016C60 };
_LIT(KImageDir, "C:\\Data\\Images\\");

// Etesian-style colours
const TRgb KSurface(0, 0, 0);
const TRgb KBar(0x14, 0x14, 0x14);
const TRgb KShutter(0xFF, 0xFF, 0xFF);
const TRgb KBusy(0x80, 0x80, 0x80);
const TRgb KQuiet(0xB0, 0xB0, 0xB0);
const TRgb KText(0xF6, 0xF8, 0xED);

// encodes a bitmap still to a JPEG file (only for a plug-in that offers no Exif/JPEG format)
class CPheBitmapSaver : public CActive
	{
public:
	CPheBitmapSaver() : CActive(EPriorityStandard) { CActiveScheduler::Add(this); }
	~CPheBitmapSaver() { Cancel(); delete iEncoder; delete iBitmap; }
	void StartL(RFs& aFs, const TDesC& aName, CFbsBitmap* aBitmap)
		{
		Cancel();
		delete iEncoder;
		iEncoder = NULL;
		delete iBitmap;
		iBitmap = aBitmap;
		iEncoder = CImageEncoder::FileNewL(aFs, aName, _L8("image/jpeg"));
		iEncoder->Convert(&iStatus, *iBitmap);
		SetActive();
		}
private:
	void RunL()
		{
		RDebug::Printf("PH: phecamera bitmap still encoded -> %d", iStatus.Int());
		delete iEncoder;
		iEncoder = NULL;
		delete iBitmap;
		iBitmap = NULL;
		}
	void DoCancel() { if (iEncoder) iEncoder->Cancel(); }
	CImageEncoder* iEncoder = NULL;
	CFbsBitmap* iBitmap = NULL;
	};

class CPheCameraView : public CCoeControl, public MCameraObserver
	{
public:
	enum TState { ENoCamera, EStarting, EReady, EBusy, EFailed };

	~CPheCameraView()
		{
		Stop();
		delete iSwitchCb;
		delete iThumb;
		delete iSaver;
		}
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		iSwitchCb = new (ELeave) CAsyncCallBack(TCallBack(SwitchCb, this), CActive::EPriorityStandard);
		ActivateL();
		Start();
		}

	// camera life cycle
	void Start()
		{
		if (iCam)
			return;
		iCount = CCamera::CamerasAvailable();
		RDebug::Printf("PH: phecamera cameras available %d", iCount);
		if (iCount <= 0)
			{
			SetState(ENoCamera);
			return;
			}
		if (iIndex >= iCount)
			iIndex = 0;
		TRAPD(e, iCam = CCamera::NewL(*this, iIndex));
		if (e != KErrNone)
			{
			RDebug::Printf("PH: phecamera camera %d -> %d", iIndex, e);
			SetState(EFailed);
			return;
			}
		SetState(EStarting);
		iCam->Reserve();
		}
	void Stop()
		{
		iVf = NULL;
		if (!iCam)
			return;
		iCam->StopViewFinder();
		iCam->PowerOff();
		iCam->Release();
		delete iCam;
		iCam = NULL;
		}
	void Capture()
		{
		if (iState != EReady || !iCam)
			return;
		KeepThumbnail();
		SetState(EBusy);
		iT0 = User::NTickCount();
		iCam->CaptureImage();
		}
	void Switch()
		{
		if (iCount > 1 && iState != EBusy)
			iSwitchCb->CallBack();	// not inside a camera callback: the camera object is deleted
		}

	// MCameraObserver
	void ReserveComplete(TInt aError)
		{
		RDebug::Printf("PH: phecamera reserve %d", aError);
		if (aError == KErrNone)
			iCam->PowerOn();
		else
			SetState(EFailed);
		}
	void PowerOnComplete(TInt aError)
		{
		RDebug::Printf("PH: phecamera power on %d", aError);
		if (aError != KErrNone)
			{
			SetState(EFailed);
			return;
			}
		TCameraInfo info;
		iCam->CameraInfo(info);
		// a camera that faces the user shows a mirror image, as every phone camera does
		iMirror = info.iOrientation == TCameraInfo::EOrientationInwards;
		if (iMirror)
			{
			TRAPD(m, iCam->SetViewFinderMirrorL(ETrue));
			if (m != KErrNone)
				RDebug::Printf("PH: phecamera mirror -> %d", m);
			}
		// the still: Exif, else JPEG, else a bitmap we encode; the largest size of that format
		const TUint32 formats = info.iImageFormatsSupported;
		iFormat = formats & CCamera::EFormatExif ? CCamera::EFormatExif : formats & CCamera::EFormatJpeg ? CCamera::EFormatJpeg :
			formats & CCamera::EFormatFbsBitmapColor16MU ? CCamera::EFormatFbsBitmapColor16MU : CCamera::EFormatFbsBitmapColor16M;
		TInt best = -1, area = 0;
		for (TInt i = 0; i < info.iNumImageSizesSupported; ++i)
			{
			TSize s;
			iCam->EnumerateCaptureSizes(s, i, iFormat);
			if (s.iWidth * s.iHeight > area)
				{
				area = s.iWidth * s.iHeight;
				best = i;
				iStillSize = s;
				}
			}
		TInt e = best < 0 ? KErrNotSupported : KErrNone;
		if (e == KErrNone)
			TRAP(e, iCam->PrepareImageCaptureL(iFormat, best));
		RDebug::Printf("PH: phecamera camera %d: still %dx%d format %x -> %d", iIndex, iStillSize.iWidth, iStillSize.iHeight,
			(TUint)iFormat, e);
		TSize vf = ViewfinderRect().Size();
		TRAPD(v, iCam->StartViewFinderBitmapsL(vf));
		RDebug::Printf("PH: phecamera viewfinder %dx%d -> %d", vf.iWidth, vf.iHeight, v);
		SetState(v == KErrNone ? (e == KErrNone ? EReady : EFailed) : EFailed);
		}
	void ViewFinderFrameReady(CFbsBitmap& aFrame)
		{
		// a deferred redraw: the window server coalesces it with input (a DrawNow per frame can starve the input)
		iVf = &aFrame;
		Window().Invalidate(ViewfinderRect());
		}
	void ImageReady(CFbsBitmap* aBitmap, HBufC8* aData, TInt aError)
		{
		TFileName name;
		NewName(name);
		TInt r = aError;
		if (r == KErrNone && aData)
			{
			RFs& fs = iCoeEnv->FsSession();
			fs.MkDirAll(KImageDir);
			RFile f;
			r = f.Replace(fs, name, EFileWrite);
			if (r == KErrNone)
				{
				r = f.Write(*aData);
				f.Close();
				}
			}
		else if (r == KErrNone && aBitmap)
			{
			iCoeEnv->FsSession().MkDirAll(KImageDir);
			if (!iSaver)
				iSaver = new CPheBitmapSaver;
			r = iSaver ? KErrNone : KErrNoMemory;
			if (r == KErrNone)
				TRAP(r, iSaver->StartL(iCoeEnv->FsSession(), name, aBitmap));
			if (r == KErrNone)
				aBitmap = NULL;		// the saver owns it now
			}
		TBuf8<KMaxFileName> name8;
		name8.Copy(name);
		RDebug::Printf("PH: phecamera still %S: %d (%d bytes, %u ms)", &name8, r, aData ? aData->Length() : 0,
			(TUint)(User::NTickCount() - iT0));
		delete aData;
		delete aBitmap;
		SetState(EReady);	// a failed still is logged; the camera stays usable
		}
	void FrameBufferReady(MFrameBuffer* /*aFrameBuffer*/, TInt /*aError*/) {}

	// CCoeControl
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
		{
		if (aType == EEventKey && (aKeyEvent.iCode == EKeyDevice3 || aKeyEvent.iCode == EKeyEnter || aKeyEvent.iCode == EKeyOK))
			{
			Capture();
			return EKeyWasConsumed;
			}
		return EKeyWasNotConsumed;
		}
	void HandlePointerEventL(const TPointerEvent& aEvent)
		{
		if (aEvent.iType != TPointerEvent::EButton1Up)
			return;
		if (Hit(ShutterRect(), aEvent.iPosition))
			Capture();
		else if (iCount > 1 && Hit(SwitchRect(), aEvent.iPosition))
			Switch();
		}
	void SizeChanged()
		{
		delete iThumb;		// its size follows the bar
		iThumb = NULL;
		}

private:
	static TInt SwitchCb(TAny* aSelf)
		{
		CPheCameraView* self = static_cast<CPheCameraView*>(aSelf);
		self->Stop();
		self->iIndex = (self->iIndex + 1) % self->iCount;
		self->DrawNow();
		self->Start();
		return KErrNone;
		}
	void SetState(TState aState)
		{
		iState = aState;
		DrawDeferred();
		}
	void NewName(TFileName& aName) const
		{
		TTime now;
		now.HomeTime();
		const TDateTime d = now.DateTime();
		aName.Format(_L("%SIMG_%04d%02d%02d_%02d%02d%02d.jpg"), &KImageDir, d.Year(), (TInt)d.Month() + 1, d.Day() + 1, d.Hour(),
			d.Minute(), d.Second());
		}
	static TBool Hit(const TRect& aRect, const TPoint& aPoint)
		{
		TRect r(aRect);
		r.Grow(8, 8);		// a finger is bigger than the drawing
		return r.Contains(aPoint);
		}

	// layout: the viewfinder above a control bar of a sixth of the height (at least 96 px)
	TInt BarHeight() const { return Max(96, Rect().Height() / 6); }
	TRect ViewfinderRect() const { TRect r(Rect()); r.iBr.iY -= BarHeight(); return r; }
	TRect BarRect() const { TRect r(Rect()); r.iTl.iY = r.iBr.iY - BarHeight(); return r; }
	TRect Square(TInt aCentreX, TInt aSide) const
		{
		const TRect bar = BarRect();
		const TPoint c(aCentreX, bar.Center().iY);
		return TRect(c - TPoint(aSide / 2, aSide / 2), TSize(aSide, aSide));
		}
	TRect ShutterRect() const { return Square(BarRect().Center().iX, BarHeight() * 64 / 100); }
	TRect ThumbRect() const { return Square(BarRect().iTl.iX + BarRect().Width() / 6, BarHeight() * 46 / 100); }
	TRect SwitchRect() const { return Square(BarRect().iBr.iX - BarRect().Width() / 6, BarHeight() * 46 / 100); }

	// the viewfinder frame, centred and cropped to fill its area
	void DrawViewfinder(CWindowGc& aGc) const
		{
		const TRect area = ViewfinderRect();
		aGc.SetBrushColor(KSurface);
		aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		aGc.SetPenStyle(CGraphicsContext::ENullPen);
		if (!iVf || iState == ENoCamera)
			{
			aGc.DrawRect(area);
			return;
			}
		const TSize src = iVf->SizeInPixels();
		if (src.iWidth <= 0 || src.iHeight <= 0)
			return;
		// crop the source to the area's aspect ratio
		TRect s(TPoint(0, 0), src);
		if (src.iWidth * area.Height() > src.iHeight * area.Width())
			{
			const TInt w = src.iHeight * area.Width() / area.Height();
			s.iTl.iX = (src.iWidth - w) / 2;
			s.iBr.iX = s.iTl.iX + w;
			}
		else
			{
			const TInt h = src.iWidth * area.Height() / area.Width();
			s.iTl.iY = (src.iHeight - h) / 2;
			s.iBr.iY = s.iTl.iY + h;
			}
		aGc.DrawBitmap(area, iVf, s);
		}
	void DrawText(CWindowGc& aGc, const TDesC& aText, const TRect& aRect) const
		{
		const CFont* font = iEikonEnv->LegendFont();
		aGc.UseFont(font);
		aGc.SetPenStyle(CGraphicsContext::ESolidPen);
		aGc.SetPenColor(KText);
		aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
		const TInt baseline = aRect.Height() / 2 + font->AscentInPixels() / 2;
		aGc.DrawText(aText, aRect, baseline, CGraphicsContext::ECenter);
		aGc.DiscardFont();
		}
	void Draw(const TRect& /*aRect*/) const
		{
		CWindowGc& gc = SystemGc();
		DrawViewfinder(gc);
		// the bar
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(KBar);
		gc.DrawRect(BarRect());
		// the message over the viewfinder
		if (iState == ENoCamera)
			DrawText(gc, _L("No camera"), ViewfinderRect());
		else if (iState == EFailed)
			DrawText(gc, _L("Camera not available"), ViewfinderRect());
		else if (iState == EStarting && !iVf)
			DrawText(gc, _L("Starting camera"), ViewfinderRect());
		else if (iState == EBusy)
			{
			TRect r = ViewfinderRect();
			r.iTl.iY = r.iBr.iY - BarHeight() / 2;
			DrawText(gc, _L("Saving photo"), r);
			}
		if (iState == ENoCamera)
			return;
		// the shutter: a white ring around a white disc; grey while a still is on its way
		const TRect shutter = ShutterRect();
		const TBool live = iState == EReady;
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(4, 4));
		gc.SetPenColor(live ? KShutter : KBusy);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.DrawEllipse(shutter);
		TRect disc(shutter);
		disc.Shrink(shutter.Width() / 8, shutter.Height() / 8);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(live ? KShutter : KBusy);
		gc.DrawEllipse(disc);
		// the last shot
		const TRect thumb = ThumbRect();
		if (iThumb)
			gc.DrawBitmap(thumb, iThumb);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(2, 2));
		gc.SetPenColor(KQuiet);
		gc.DrawRoundRect(thumb, TSize(6, 6));
		// the switch: a ring with two arrow heads (rear <-> front)
		if (iCount > 1)
			{
			TRect sw(SwitchRect());
			sw.Shrink(sw.Width() / 6, sw.Height() / 6);
			gc.SetPenColor(iState == EBusy ? KBusy : KQuiet);
			gc.DrawArc(sw, TPoint(sw.iBr.iX, sw.iTl.iY), TPoint(sw.iTl.iX, sw.iBr.iY));
			gc.DrawArc(sw, TPoint(sw.iTl.iX, sw.iBr.iY), TPoint(sw.iBr.iX, sw.iTl.iY));
			const TInt a = sw.Width() / 5;
			const TPoint top(sw.Center().iX, sw.iTl.iY), bottom(sw.Center().iX, sw.iBr.iY);
			gc.DrawLine(top, top + TPoint(-a, -a));
			gc.DrawLine(top, top + TPoint(-a, a));
			gc.DrawLine(bottom, bottom + TPoint(a, -a));
			gc.DrawLine(bottom, bottom + TPoint(a, a));
			}
		gc.SetPenSize(TSize(1, 1));
		}
	// the thumbnail is the viewfinder frame at the shutter press (no JPEG decode needed)
	void KeepThumbnail()
		{
		if (!iVf)
			return;
		const TSize size = ThumbRect().Size();
		if (!iThumb)
			{
			iThumb = new CFbsBitmap;
			if (!iThumb || iThumb->Create(size, EColor16MU) != KErrNone)
				{
				delete iThumb;
				iThumb = NULL;
				return;
				}
			}
		CFbsBitmapDevice* dev = NULL;
		TRAPD(e, dev = CFbsBitmapDevice::NewL(iThumb));
		CFbsBitGc* gc = NULL;
		if (e == KErrNone && dev->CreateContext(gc) == KErrNone)
			{
			const TSize src = iVf->SizeInPixels();
			const TInt side = Min(src.iWidth, src.iHeight);
			const TRect s(TPoint((src.iWidth - side) / 2, (src.iHeight - side) / 2), TSize(side, side));
			gc->DrawBitmap(TRect(TPoint(0, 0), size), iVf, s);
			}
		delete gc;
		delete dev;
		}

	CCamera* iCam = NULL;
	TInt iIndex = 0;
	TInt iCount = 0;
	TState iState = EStarting;
	CFbsBitmap* iVf = NULL;		// the plug-in's, valid until its next frame
	CFbsBitmap* iThumb = NULL;
	CPheBitmapSaver* iSaver = NULL;
	CAsyncCallBack* iSwitchCb = NULL;
	CCamera::TFormat iFormat = CCamera::EFormatExif;
	TSize iStillSize;
	TBool iMirror = EFalse;
	TUint32 iT0 = 0;
	};

class CPheCameraAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		if (StatusPane())
			StatusPane()->MakeVisible(EFalse);
		iView = new (ELeave) CPheCameraView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		}
	~CPheCameraAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheCamCmdCapture: iView->Capture(); break;
		case EPheCamCmdSwitch: iView->Switch(); break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
		default:
			break;
			}
		}
	// the camera is shared: give it back while another app is in front
	void HandleForegroundEventL(TBool aForeground)
		{
		CAknAppUi::HandleForegroundEventL(aForeground);
		if (!iView)
			return;
		if (aForeground)
			iView->Start();
		else
			iView->Stop();
		}
	void HandleResourceChangeL(TInt aType)
		{
		CAknAppUi::HandleResourceChangeL(aType);
		if (iView && aType == KEikDynamicLayoutVariantSwitch)
			iView->SetRect(ClientRect());
		}
private:
	CPheCameraView* iView = NULL;
	};

class CPheCameraDocument : public CAknDocument
	{
public:
	CPheCameraDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPheCameraAppUi; }
	};

class CPheCameraApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidPheCamera; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPheCameraDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPheCameraApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
