// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phecamera.cpp -- OURS: Camera (P1, photo). A native Avkon app over ECam only, so it runs on any device with an ECam
// plug-in. FULL SCREEN: no status pane, no softkey bar; the viewfinder fills the screen and the controls are transparent
// squircle buttons over it (Etesian, as the N9 camera): close (top left) and the menu (top right); along the bottom an
// icon bar of the last shot's thumbnail (left), the round shutter (middle) and the camera switch (right, when there is
// more than one camera). Every size follows the real screen (a fraction of its shorter side), portrait or landscape.
// Hardware keys keep working: the left softkey opens the menu, the right one exits.
// The still is the largest capture size, as Exif (else JPEG, else a bitmap the app encodes itself), saved to
// C:\Data\Images\IMG_<date>_<time>.jpg; a full-size still takes seconds, so the shutter shows a busy state until
// ImageReady. Without a camera (CCamera::CamerasAvailable() == 0) the screen says "No camera".
// Keys: Select/OK or Enter = shutter; Options menu = take photo / switch camera / exit.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <eikbtgpc.h>
#include <eikmenub.h>
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
const TRgb KShutter(0xFF, 0xFF, 0xFF);
const TRgb KBusy(0x80, 0x80, 0x80);
const TRgb KQuiet(0xB0, 0xB0, 0xB0);
const TRgb KGlyph(0xFF, 0xFF, 0xFF);
const TRgb KSquircleFill(0, 0, 0, 0x60);		// transparent: the viewfinder shows through
const TRgb KSquircleRim(0xFF, 0xFF, 0xFF, 0x70);
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
		// the softkey bar is hidden: its keys still work
		if (aType == EEventKey && aKeyEvent.iCode == EKeyDevice0)
			{
			iAvkonAppUi->ProcessCommandL(EAknSoftkeyOptions);
			return EKeyWasConsumed;
			}
		if (aType == EEventKey && aKeyEvent.iCode == EKeyDevice1)
			{
			iAvkonAppUi->ProcessCommandL(EAknSoftkeyExit);
			return EKeyWasConsumed;
			}
		return EKeyWasNotConsumed;
		}
	void HandlePointerEventL(const TPointerEvent& aEvent)
		{
		if (aEvent.iType != TPointerEvent::EButton1Up)
			return;
		if (Hit(CloseRect(), aEvent.iPosition))
			iAvkonAppUi->ProcessCommandL(EAknSoftkeyExit);
		else if (Hit(MenuRect(), aEvent.iPosition))
			iAvkonAppUi->ProcessCommandL(EAknSoftkeyOptions);
		else if (iState == ENoCamera)
			return;
		else if (Hit(ShutterRect(), aEvent.iPosition))
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

	// layout, all from the real screen: fractions of its shorter side
	TInt Unit() const { return Max(1, Min(Rect().Width(), Rect().Height())); }
	TInt Button() const { return Unit() * 15 / 100; }		// a squircle button (54 px on a 360 px wide screen)
	TInt Margin() const { return Unit() * 5 / 100; }
	TInt BarHeight() const { return Unit() * 30 / 100; }		// the bottom icon bar (over the viewfinder)
	TRect ViewfinderRect() const { return Rect(); }
	TRect BarRect() const { TRect r(Rect()); r.iTl.iY = r.iBr.iY - BarHeight(); return r; }
	TRect Square(TInt aCentreX, TInt aCentreY, TInt aSide) const
		{
		return TRect(TPoint(aCentreX - aSide / 2, aCentreY - aSide / 2), TSize(aSide, aSide));
		}
	TRect BarSquare(TInt aCentreX, TInt aSide) const { return Square(aCentreX, BarRect().Center().iY, aSide); }
	TRect ShutterRect() const { return BarSquare(BarRect().Center().iX, Unit() * 21 / 100); }
	TRect ThumbRect() const { return BarSquare(BarRect().iTl.iX + BarRect().Width() / 6, Button()); }
	TRect SwitchRect() const { return BarSquare(BarRect().iBr.iX - BarRect().Width() / 6, Button()); }
	TRect CloseRect() const { return TRect(Rect().iTl + TPoint(Margin(), Margin()), TSize(Button(), Button())); }
	TRect MenuRect() const
		{
		return TRect(TPoint(Rect().iBr.iX - Margin() - Button(), Rect().iTl.iY + Margin()), TSize(Button(), Button()));
		}

	// an Etesian squircle button: a transparent dark fill and a light rim, the viewfinder shows through
	void DrawSquircle(CWindowGc& aGc, const TRect& aRect) const
		{
		const TSize corner(aRect.Width() * 3 / 10, aRect.Height() * 3 / 10);
		aGc.SetDrawMode(CGraphicsContext::EDrawModePEN);
		aGc.SetPenStyle(CGraphicsContext::ESolidPen);
		aGc.SetPenSize(TSize(2, 2));
		aGc.SetPenColor(KSquircleRim);
		aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		aGc.SetBrushColor(KSquircleFill);
		aGc.DrawRoundRect(aRect, corner);
		aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
		}
	void Glyph(CWindowGc& aGc, TBool aLive) const
		{
		const TInt w = Max(2, Unit() / 120);
		aGc.SetPenStyle(CGraphicsContext::ESolidPen);
		aGc.SetPenSize(TSize(w, w));
		aGc.SetPenColor(aLive ? KGlyph : KBusy);
		aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
		}

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
		// the message over the viewfinder
		if (iState == ENoCamera)
			DrawText(gc, _L("No camera"), ViewfinderRect());
		else if (iState == EFailed)
			DrawText(gc, _L("Camera not available"), ViewfinderRect());
		else if (iState == EStarting && !iVf)
			DrawText(gc, _L("Starting camera"), ViewfinderRect());
		else if (iState == EBusy)
			{
			TRect r = BarRect();
			r.iBr.iY = r.iTl.iY;
			r.iTl.iY -= BarHeight() / 3;
			DrawText(gc, _L("Saving photo"), r);
			}
		// close: an X
		TRect close(CloseRect());
		DrawSquircle(gc, close);
		Glyph(gc, ETrue);
		close.Shrink(close.Width() * 3 / 10, close.Height() * 3 / 10);
		gc.DrawLine(close.iTl, close.iBr);
		gc.DrawLine(TPoint(close.iBr.iX, close.iTl.iY), TPoint(close.iTl.iX, close.iBr.iY));
		// the menu: three dots
		const TRect menu(MenuRect());
		DrawSquircle(gc, menu);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(KGlyph);
		const TInt dot = Max(3, menu.Width() / 10);
		for (TInt i = -1; i <= 1; ++i)
			{
			const TPoint c(menu.Center().iX + i * dot * 2, menu.Center().iY);
			gc.DrawEllipse(TRect(c - TPoint(dot / 2, dot / 2), TSize(dot, dot)));
			}
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		if (iState == ENoCamera)
			{
			gc.SetPenSize(TSize(1, 1));
			return;
			}
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
		// the last shot, inside a squircle
		const TRect thumb = ThumbRect();
		DrawSquircle(gc, thumb);
		if (iThumb)
			{
			TRect t(thumb);
			t.Shrink(thumb.Width() / 10, thumb.Height() / 10);
			gc.DrawBitmap(t, iThumb);
			}
		// the switch: a ring with two arrow heads (rear <-> front)
		if (iCount > 1)
			{
			const TRect swb(SwitchRect());
			DrawSquircle(gc, swb);
			TRect sw(swb);
			sw.Shrink(sw.Width() * 3 / 10, sw.Height() * 3 / 10);
			Glyph(gc, iState != EBusy);
			gc.DrawArc(sw, TPoint(sw.iBr.iX, sw.iTl.iY), TPoint(sw.iTl.iX, sw.iBr.iY));
			gc.DrawArc(sw, TPoint(sw.iTl.iX, sw.iBr.iY), TPoint(sw.iBr.iX, sw.iTl.iY));
			const TInt a = sw.Width() / 4;
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
		TRect inner(ThumbRect());
		inner.Shrink(inner.Width() / 10, inner.Height() / 10);
		const TSize size = inner.Size();
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
		// full screen: no status pane, no softkey bar (the view takes their keys)
		if (StatusPane())
			StatusPane()->MakeVisible(EFalse);
		if (Cba())
			Cba()->MakeVisible(EFalse);
		iView = new (ELeave) CPheCameraView;
		iView->ConstructL(ApplicationRect());
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
		case EAknSoftkeyOptions:
			if (iEikonEnv->AppUiFactory()->MenuBar())
				iEikonEnv->AppUiFactory()->MenuBar()->TryDisplayMenuBarL();
			break;
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
			iView->SetRect(ApplicationRect());
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
