// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinreg.h -- the window registry (hildon-port.md C4): every application's GDK/Clutter backend registers its top-level
// windows here with the metadata a compositor needs (what libmatchbox2 reads from X window properties: type, title,
// transient-for, geometry, state) and publishes its front bitmap (an FBSERV CFbsBitmap handle, shareable between
// processes); the frame process lists them and waits for changes, then imports each bitmap as a texture. The registry
// owns no pixels: a window's content stays in its owner's bitmap. A session's windows go when the session closes.
//
// Client side: header only (RSessionBase), link euser.lib; the server is phewinreg.exe, started on the first Connect. The header
// lives in core (a launcher in core uses it); the server and the frames are in the layers above.
#ifndef PHEWINREG_H
#define PHEWINREG_H
#include <e32std.h>

_LIT(KPheWinRegServer, "phewinreg");
_LIT(KPheWinRegExe, "phewinreg.exe");

// the window types of the EWMH _NET_WM_WINDOW_TYPE set matchbox handles
enum TPheWinType { EPheWinNormal, EPheWinDialog, EPheWinMenu, EPheWinNotification, EPheWinDesktop, EPheWinInput, EPheWinSplash };
// state flags (_NET_WM_STATE): fullscreen, modal, urgent (demands attention), hidden (iconified)
enum { EPheWinFullscreen = 1, EPheWinModal = 2, EPheWinUrgent = 4, EPheWinHidden = 8,
	EPheWinFocused = 16 };	// registry-owned: set only by a frame's EPheWinActFocus (at most one window), owners cannot set or clear it

class TPheWinInfo
	{
public:
	TPheWinInfo() : iId(0), iOwner(0), iType(EPheWinNormal), iTransientFor(0), iState(0), iBitmap(0), iContentSerial(0), iGpuToken(0), iSubscribers(0), iComposited(0) {}
	TInt iId;					// the registry's id (0 before registering)
	TUint iOwner;				// the owner's process id (the registry fills it)
	TInt iType;					// TPheWinType
	TInt iTransientFor;			// another window's id, or 0
	TRect iGeometry;			// the requested position and size, in screen pixels
	TUint iState;				// EPheWin... flags
	TInt iBitmap;				// the front bitmap's FBSERV handle (CFbsBitmap::Handle()), 0 = no content yet
	TUint iContentSerial;		// counts content updates (the registry increments it)
	TRect iDamage;				// the area of the last content update, in window coordinates
	TInt iGpuToken;				// the front texture's SVGA share token (EGL_SYMBIAN_SVGA_SHARE_PH, hildon-port.md 7.47), 0 = bitmap only
	TInt iSubscribers;			// frame sessions that subscribed to this window's content (the registry fills it)
	TInt iComposited;			// ... of them, those that composite it: the owner keeps the window off the window server's screen
	TBuf<64> iTitle;
	};

// Subscribe() modes (hildon-port.md 7.48)
enum TPheWinSubscription { EPheWinUnsubscribe = 0, EPheWinWatch = 1, EPheWinComposite = 2 };
typedef TPckgBuf<TPheWinInfo> TPheWinInfoPckg;

enum TPheWinRegOp { EPheWinRegister, EPheWinUpdate, EPheWinContent, EPheWinRemove, EPheWinList, EPheWinNotify, EPheWinNotifyCancel, EPheWinWatchers, EPheWinWatchersCancel, EPheWinSubscribe, EPheWinContentNotify, EPheWinAction, EPheWinActionNotify, EPheWinActionCancel,
	EPheWinPointer, EPheWinPointerNotify, EPheWinPointerCancel,
	EPheWinSetHints, EPheWinHints, EPheWinResize, EPheWinPing, EPheWinPong,
	EPheWinSetAppUid, EPheWinAppUid, EPheWinRaiseApp, EPheWinApps, EPheWinOpen };

// a frame's decoration actions, sent back to the window's owner (hildon-port.md 7.53)
enum TPheWinAction { EPheWinActClose = 1, EPheWinActMinimise = 2, EPheWinActMaximise = 3, EPheWinActFocus = 4, EPheWinActRaise = 5 };
// actions that only the registry queues, never Action(): EPheWinActResize (a frame's Resize; the owner reads the rect with Hints),
// EPheWinActPing (a frame's Ping; the owner answers with Pong)
enum { EPheWinActResize = 6, EPheWinActPing = 7 };
// List() returns the windows in stacking order, bottom first: top-level windows by their raise order, each followed by the windows
// transient for it (dialogs above their parent, recursively). EPheWinActRaise (a frame's Action) puts a window on top of its siblings
// ActionNotify completes with (window id << 4) | action
inline TInt PheWinActionId(TInt aEvent) { return aEvent >> 4; }
inline TInt PheWinActionCode(TInt aEvent) { return aEvent & 0xF; }

// a pointer event a frame sends to a window's owner (multi-window: a classic app's screen shown as a window): iType = a
// TRawEvent::TType (EButton1Down, EButton1Up, EPointerMove), iPos in the window's content pixels
class TPheWinPointer
	{
public:
	TPheWinPointer() : iId(0), iType(0) {}
	TInt iId;
	TInt iType;
	TPoint iPos;
	};
typedef TPckgBuf<TPheWinPointer> TPheWinPointerPckg;

// per-window data beside TPheWinInfo (hildon-port.md C5 items 4 and 5): the owner's size hints and icon, the frame's last
// geometry request, and the ping state. The owner sets the hints part (SetHints); the registry fills the rest
class TPheWinHints
	{
public:
	TPheWinHints() : iId(0), iIcon(0), iPingPending(0), iPingAgeMs(0), iIconMask(0), iFlags(0) {}
	TInt iId;
	TSize iMinSize;				// owner: the smallest size it can lay out in (0 x 0 = no limit)
	TSize iMaxSize;				// owner: the largest useful size (0 x 0 = no limit)
	TSize iPreferredSize;		// owner: its natural size (0 x 0 = no preference)
	TInt iIcon;					// owner: an FBSERV bitmap handle for the window's icon (task switcher, title bar), 0 = none
	TRect iRequested;			// registry: the frame's last Resize request (empty = none since the owner set its hints)
	TInt iPingPending;			// registry: pings the owner has not answered yet
	TInt iPingAgeMs;			// registry: ms since the oldest unanswered ping (0 when none): a frame shows "not responding" past its limit
	TInt iIconMask;				// owner: an FBSERV handle of the icon's mask (EGray256), 0 = none (hildon-port.md 7.51.23)
	TUint iFlags;				// owner: EPheWinHint... flags
	};
typedef TPckgBuf<TPheWinHints> TPheWinHintsPckg;
// TPheWinHints::iFlags: the window cannot be closed (screen 0, the phone screen: a close ends the app in front, the screen stays)
enum { EPheWinHintPermanent = 1 };

// one launchable classic app (RPheWinReg::Apps, hildon-port.md 7.51.23): AppArc's list minus hidden and non-classic apps
class TPheWinApp
	{
public:
	TPheWinApp() : iUid(0), iWindow(0), iIcon(0), iIconMask(0) {}
	TInt32 iUid;				// AppArc UID (= a window's app UID, RPheWinReg::AppUid)
	TInt iWindow;				// the window that shows it now, 0 = not open
	TInt iIcon;					// FBSERV handle of its 48x48 icon (EColor16MU), 0 = none; the registry owns it, a frame Duplicates
	TInt iIconMask;				// FBSERV handle of the icon's mask (EGray256), 0 = none
	TBuf<64> iCaption;			// AppArc caption
	};
typedef TPckgBuf<TPheWinApp> TPheWinAppPckg;

class RPheWinReg : public RSessionBase
	{
public:
	inline TInt Connect();
	// register a window: aInfo's metadata in, its iId out
	inline TInt Register(TPheWinInfo& aInfo);
	// new metadata for window aInfo.iId (the bitmap and serial are kept)
	inline TInt Update(const TPheWinInfo& aInfo) { TPheWinInfoPckg p(aInfo); return SendReceive(EPheWinUpdate, TIpcArgs(&p)); }
	// the window's front bitmap, the area that changed and (0 = none) its GPU share token; returns the window's subscriber count, or an error
	inline TInt Content(TInt aId, TInt aBitmapHandle, const TRect& aDamage, TInt aGpuToken = 0)
		{ TPckgC<TRect> d(aDamage); return SendReceive(EPheWinContent, TIpcArgs(aId, aBitmapHandle, &d, aGpuToken)); }
	inline TInt Remove(TInt aId) { return SendReceive(EPheWinRemove, TIpcArgs(aId)); }
	// every window, in registration order: fills aBuf with TPheWinInfo records; returns the count (or the count needed, when > the buffer)
	inline TInt List(TDes8& aBuf) { return SendReceive(EPheWinList, TIpcArgs(&aBuf)); }
	// completes with the registry's change serial when anything changes after aSeen (0 = at the next change)
	inline void Notify(TUint aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinNotify, TIpcArgs((TInt)aSeen), aStatus); }
	inline void NotifyCancel() { SendReceive(EPheWinNotifyCancel, TIpcArgs()); }
	// for a compositor: completes with the content tick when a GPU window published its next frame (same token: nothing to re-list)
	// after aSeen (0 = at the next one). Notify still reports everything else (metadata, new tokens, bitmaps)
	inline void ContentNotify(TUint aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinContentNotify, TIpcArgs((TInt)aSeen), aStatus); }
	// for a window owner: completes with its subscription serial when it differs from aSeen (-1 = at once): the serial moves whenever a
	// subscription to one of the owner's windows changes; the owner then reads iSubscribers / iComposited of its windows (List).
	// A window without subscribers publishes no content; when one appears it publishes a full frame
	inline void Watchers(TInt aSeen, TRequestStatus& aStatus) { SendReceive(EPheWinWatchers, TIpcArgs(aSeen), aStatus); }
	// for a frame process: subscribe to one window's content (TPheWinSubscription; EPheWinUnsubscribe ends it). Only other sessions'
	// windows; a session's subscriptions end with it
	inline TInt Subscribe(TInt aId, TInt aMode) { return SendReceive(EPheWinSubscribe, TIpcArgs(aId, aMode)); }
	inline void WatchersCancel() { SendReceive(EPheWinWatchersCancel, TIpcArgs()); }
	// for a frame process: ask the owner of window aId to act (TPheWinAction: its close button, minimise, ...). Only a session that
	// subscribed to the window may send; the registry queues the action for the owner (in order) and returns at once
	inline TInt Action(TInt aId, TInt aAction) { return SendReceive(EPheWinAction, TIpcArgs(aId, aAction)); }
	// for a window owner: completes with the next queued action on one of its windows (PheWinActionId / PheWinActionCode)
	inline void ActionNotify(TRequestStatus& aStatus) { SendReceive(EPheWinActionNotify, TIpcArgs(), aStatus); }
	inline void ActionCancel() { SendReceive(EPheWinActionCancel, TIpcArgs()); }
	// for a frame process: a pointer event for window aId (content coordinates). As Action: only a subscriber may send, queued in
	// order for the owner, returns at once
	inline TInt Pointer(TInt aId, TInt aType, const TPoint& aPos)
		{ TPckgC<TPoint> p(aPos); return SendReceive(EPheWinPointer, TIpcArgs(aId, aType, &p)); }
	// for a window owner: completes when the next queued pointer event for one of its windows is in aEvent
	inline void PointerNotify(TPheWinPointerPckg& aEvent, TRequestStatus& aStatus) { SendReceive(EPheWinPointerNotify, TIpcArgs(&aEvent), aStatus); }
	inline void PointerCancel() { SendReceive(EPheWinPointerCancel, TIpcArgs()); }
	// for a window owner: its size hints and icon (iId names the window; the registry keeps iRequested / ping fields). Frames
	// hear it through Notify
	inline TInt SetHints(const TPheWinHints& aHints) { TPheWinHintsPckg p(aHints); return SendReceive(EPheWinSetHints, TIpcArgs(&p)); }
	// anyone: window aId's hints, the frame's last request and the ping state
	inline TInt Hints(TInt aId, TPheWinHints& aHints) { TPheWinHintsPckg p; const TInt r = SendReceive(EPheWinHints, TIpcArgs(aId, &p)); if (r == KErrNone) aHints = p(); return r; }
	// for a frame process: ask the owner of window aId to take geometry aRect (a subscriber only, as Action). The registry clamps
	// the size to the owner's min / max hints, records it as iRequested and queues EPheWinActResize for the owner, who then reads
	// it (Hints) and answers with Update (its new iGeometry), or keeps its size
	inline TInt Resize(TInt aId, const TRect& aRect) { TPckgC<TRect> p(aRect); return SendReceive(EPheWinResize, TIpcArgs(aId, &p)); }
	// for a frame process: queue EPheWinActPing for the owner (a subscriber only); the owner answers with Pong. Unanswered pings
	// show in Hints (iPingPending / iPingAgeMs); an answer is a registry change (Notify)
	inline TInt Ping(TInt aId) { return SendReceive(EPheWinPing, TIpcArgs(aId)); }
	inline TInt Pong(TInt aId) { return SendReceive(EPheWinPong, TIpcArgs(aId)); }
	// for a window owner: the application (AppArc UID) the window shows, e.g. phescreenpub for a classic app's screen; a frame
	// raises an app's existing window instead of starting it again. Kept beside TPheWinInfo (its layout is every client's ABI)
	inline TInt SetAppUid(TInt aId, TUid aUid) { return SendReceive(EPheWinSetAppUid, TIpcArgs(aId, aUid.iUid)); }
	// anyone: window aId's application UID (KNullUid = none set)
	inline TInt AppUid(TInt aId, TUid& aUid)
		{ TPckgBuf<TInt32> p; const TInt r = SendReceive(EPheWinAppUid, TIpcArgs(aId, &p)); aUid = TUid::Uid(r == KErrNone ? p() : 0); return r; }
	// anyone (a launcher): raise and focus the top window that shows application aUid, as a frame's EPheWinActRaise + EPheWinActFocus;
	// its owner gets EPheWinActFocus. Returns the window id, or KErrNotFound when no window shows that app (then start it)
	inline TInt RaiseApp(TUid aUid) { return SendReceive(EPheWinRaiseApp, TIpcArgs(aUid.iUid)); }
	// anyone (a launcher): the launchable apps, sorted by caption, as TPheWinApp records in aBuf; returns the count (or the count
	// needed, when > the buffer). Notify completes when the list changes (an app installed, a window opened or closed)
	inline TInt Apps(TDes8& aBuf) { return SendReceive(EPheWinApps, TIpcArgs(&aBuf)); }
	// anyone (a launcher): app aUid in a window. Already open: raised + focused, returns its window id (> 0). Otherwise started in a
	// new window (on a screen made on demand), returns 0 at once; the window appears through Notify with that app UID.
	// KErrNotFound = unknown or hidden app, KErrNoMemory = no screen could be made
	inline TInt Open(TUid aUid, const TDesC& aDocument = KNullDesC) { return SendReceive(EPheWinOpen, TIpcArgs(aUid.iUid, &aDocument)); }
	};

inline TInt RPheWinReg::Connect()
	{
	for (TInt tries = 0; tries < 2; ++tries)
		{
		TInt r = CreateSession(KPheWinRegServer, TVersion(1, 0, 0), 4);
		if (r != KErrNotFound && r != KErrServerTerminated)
			return r;
		RProcess p;
		if ((r = p.Create(KPheWinRegExe, KNullDesC)) != KErrNone)
			return r;
		TRequestStatus st;
		p.Rendezvous(st);
		p.Resume();
		User::WaitForRequest(st);
		p.Close();
		if (st.Int() != KErrNone && st.Int() != KErrAlreadyExists)
			return st.Int();
		}
	return CreateSession(KPheWinRegServer, TVersion(1, 0, 0), 4);
	}

inline TInt RPheWinReg::Register(TPheWinInfo& aInfo)
	{
	TPheWinInfoPckg p(aInfo);
	const TInt r = SendReceive(EPheWinRegister, TIpcArgs(&p));
	if (r > 0)
		aInfo.iId = r;
	return r < 0 ? r : KErrNone;
	}
#endif
