/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// phkbd_board.h -- OURS (Phorepoc): the key area of the phkbd keyboard.
//
// One pen-input control (a CFepUiBaseCtrl) that owns the whole keyboard: it lays the keys out from data (phkbd_pages.h),
// draws them (rounded keys, flat dark panel, a single accent colour -- the N9 look), shows the key-press preview popup,
// the long-press accent bar, and turns pointer events into key events for the layout (phkbd_layout.cpp).
//
// Written from scratch against the pen input framework headers (EPL); the behaviour (nearest-key tap, press preview,
// long-press accents, shift / caps lock, symbol pages) follows the common Android and N9 practice.
#ifndef PHKBD_BOARD_H
#define PHKBD_BOARD_H

#include <e32base.h>
#include <gdi.h>
#include <touchfeedback.h>
#include <peninputlayoutbasecontrol.h>
#include "phkbd_pages.h"

// events the board reports to the layout (EEventControlUserBase +)
enum TPhKbdEvent
    {
    EPhEvtText = EEventControlUserBase + 1,   // data = the characters to type
    EPhEvtBackspace,                          // one backspace (auto-repeats while held)
    EPhEvtEnter,
    EPhEvtShift,                              // shift key tapped
    EPhEvtCapsLock,                           // shift key double-tapped
    EPhEvtSwitchKeyboard,                     // globe key, or a long press on space: the next keyboard
    EPhEvtClose                               // hide-keyboard chevron
    };

// the colours of one scheme (user setting KPhImKeyTheme)
struct TPhKbdColours
    {
    TRgb iPanel;        // keyboard background
    TRgb iKey;          // character key
    TRgb iKeyFn;        // function key (shift, backspace, ?123 ...)
    TRgb iKeyDown;      // pressed key
    TRgb iAccent;       // shift/caps lit, the popup highlight
    TRgb iLabel;
    TRgb iLabelDim;
    TRgb iPopup;
    TRgb iPopupLabel;
    };

enum TPhShiftState { EPhLower, EPhShift, EPhCaps };

class CPhKbdBoard : public CFepUiBaseCtrl
    {
public:
    // aRect = the board's rectangle in layout coordinates (the bottom of the screen)
    static CPhKbdBoard* NewL( const TRect& aRect, CFepUiLayout* aLayout, TInt aControlId );
    ~CPhKbdBoard();

    // user customisation (read from the registry's repository by the layout)
    void SetCustomisation( TInt aKeyHeightPercent, TInt aTheme, TBool aPopups );
    // height the board needs on a screen of aScreen (keyboard + preview strip), for the layout to place it
    static TInt HeightFor( const TSize& aScreen, TInt aKeyHeightPercent );
    // the shift state the FEP wants shown
    void SetShift( TPhShiftState aState );
    TPhShiftState Shift() const { return iShift; }
    // drop one-shot shift after a character was typed
    void ConsumeShift();
    void SetKeyboardName( const TDesC& aName );
    void SetEnterDimmed( TBool aDim );
    void ResetPage();

    // CFepUiBaseCtrl
    void Draw();
    void ReDrawRect( const TRect& aRect );
    CFepUiBaseCtrl* HandlePointerDownEventL( const TPoint& aPoint );
    CFepUiBaseCtrl* HandlePointerMoveEventL( const TPoint& aPoint );
    CFepUiBaseCtrl* HandlePointerUpEventL( const TPoint& aPoint );
    void CancelPointerDownL();
    void SetRect( const TRect& aRect );

private:
    struct TKey
        {
        const TPhKeyDef* iDef;
        TRect iRect;        // the visible key
        TRect iHit;         // the tap area (the rect plus half the gaps, so no dead zones)
        };

    CPhKbdBoard( const TRect& aRect, CFepUiLayout* aLayout, TInt aControlId );
    void ConstructL();
    void BuildPage();
    void ApplyColours();
    void CreateFonts();
    void ReleaseFonts();
    TInt KeyAt( const TPoint& aPoint ) const;
    TUint Display( const TPhKeyDef& aDef ) const;       // the character shown / typed for a key, with shift applied
    void DrawBoard( const TRect& aClip );
    void DrawKey( CBitmapContext& aGc, const TKey& aKey, TBool aPressed );
    void DrawPreview( CBitmapContext& aGc );
    void DrawAccents( CBitmapContext& aGc );
    void DrawStrip( CBitmapContext& aGc );
    void DrawGlyph( CBitmapContext& aGc, const TRect& aRect, const TDesC& aText, const CFont* aFont, TRgb aColour );
    void DrawIcon( CBitmapContext& aGc, const TRect& aRect, TInt aKind, TRgb aColour );
    void Refresh( const TRect& aRect );
    void PressKey( TInt aIndex );
    void ReleaseKey( TBool aCommit );
    void OpenAccents();
    TInt AccentAt( const TPoint& aPoint ) const;
    void CommitText( TUint aChar );
    void StartTimer( TInt aDelayMs, TInt aIntervalMs );
    void StopTimer();
    static TInt TimerTick( TAny* aThis );
    void OnTimer();

private:
    enum TTimerPurpose { ENoTimer, ELongPress, ERepeat };

    TPhKbdColours iCol;
    TInt iKeyHeightPct;
    TInt iTheme;
    TBool iPopups;
    TInt iPage;                         // 0 letters, 1 symbols, 2 more symbols
    TPhShiftState iShift;
    TBool iEnterDimmed;
    RArray<TKey> iKeys;
    TInt iStripHeight;
    TInt iPressed;                      // index of the key under the finger, -1 none
    TPoint iDownPoint;
    TBool iAccentOpen;
    RArray<TRect> iAccentRects;
    TBuf<12> iAccentChars;
    TInt iAccentSel;
    TRect iAccentBar;
    TTimerPurpose iTimerPurpose;
    TBool iLongHandled;
    CPeriodic* iTimer;
    TTime iLastShiftTap;
    TBuf<24> iName;
    CFont* iKeyFont;                    // letter keys
    CFont* iFnFont;                     // function-key words
    CFont* iPopupFont;                  // the preview glyph
    TRect iDirty;
    };

#endif
