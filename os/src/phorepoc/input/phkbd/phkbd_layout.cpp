// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phkbd_layout.cpp -- OURS (Phorepoc): the phkbd pen-input layout (see phkbd_layout.h)
#include <e32keys.h>
#include <e32debug.h>
#include <coemain.h>
#include <gdi.h>
#include <aknfeppeninputenums.h>        // ECmdPenInput*, ECase*
#include <AknFepGlobalEnums.h>          // EPluginInputMode*
#include <peninputcmd.h>                // ESignal*
#include <peninputlayoutmultilineicf.h>
#include <peninputlayoutctrlgroup.h>
#include <peninputlayoutrootctrl.h>
#include "phimreg.h"
#include "phkbd_board.h"
#include "phkbd_layout.h"

namespace
    {
    enum TCtrlIds { EIdIcf = 100, EIdBoard = 101 };
    const TInt KIcfMargin = 4;
    }

CPhKbdLayoutImp* CPhKbdLayoutImp::NewL( TAny* /*aInitParams*/ )
    {
    return new ( ELeave ) CPhKbdLayoutImp;
    }

MPenUiLayoutBase* CPhKbdLayoutImp::CreateFepUiLayoutL( MLayoutOwner* aLayoutOwner, const TAny* aData )
    {
    return CPhKbdLayout::NewL( aLayoutOwner, aData );
    }

CPhKbdLayout* CPhKbdLayout::NewL( MLayoutOwner* aLayoutOwner, const TAny* /*aInitData*/ )
    {
    CPhKbdLayout* self = new ( ELeave ) CPhKbdLayout( aLayoutOwner );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

CPhKbdLayout::CPhKbdLayout( MLayoutOwner* aLayoutOwner )
    : CFepUiLayout( aLayoutOwner ), iKeyHeightPct( 100 ), iTheme( 0 ), iPopups( ETrue )
    {
    }

CPhKbdLayout::~CPhKbdLayout()
    {
    // iBoard and iIcf belong to the root control
    }

void CPhKbdLayout::ReloadSettings()
    {
    PhImGetSetting( KPhImKeyKeyHeight, 100, iKeyHeightPct );
    PhImGetSetting( KPhImKeyTheme, 0, iTheme );
    TInt popups = 1;
    PhImGetSetting( KPhImKeyPopups, 1, popups );
    iPopups = popups != 0;
    }

void CPhKbdLayout::ConstructL()
    {
    BaseConstructL();
    ReloadSettings();
    const TSize screen = ScreenSize();
    const TInt boardH = CPhKbdBoard::HeightFor( screen, iKeyHeightPct );
    const TRect boardRect( 0, screen.iHeight - boardH, screen.iWidth, screen.iHeight );
    const TRect icfRect( 0, 0, screen.iWidth, screen.iHeight - boardH );

    // the input context field: the text being edited, shown above the keys (this is a full-screen layout)
    const CFont* font = CCoeEnv::Static()->NormalFont();
    iIcf = CFepLayoutMultiLineIcf::NewL( icfRect, this, EIdIcf, font->HeightInPixels(), font->FontMaxHeight(), font );
    iIcf->SetTextMargin( KIcfMargin * 2, KIcfMargin * 2, KIcfMargin * 2, KIcfMargin );
    iIcf->SizeChangedL( icfRect, font->HeightInPixels(), font->FontMaxHeight(), font );
    iIcf->SetBkColor( KRgbWhite );
    iIcf->SetTextColorL( KRgbBlack );
    iIcf->SetFocus( ETrue );
    AddControlL( iIcf, ETrue );

    iBoard = CPhKbdBoard::NewL( boardRect, this, EIdBoard );
    iBoard->SetCustomisation( iKeyHeightPct, iTheme, iPopups );
    iBoard->SetKeyboardName( _L( "phkbd" ) );
    AddControlL( iBoard, ETrue );

    SetRect( TRect( TPoint( 0, 0 ), screen ) );
    }

// the ICF fills what the keys leave; recomputed when the keyboard height setting changed
void CPhKbdLayout::Arrange()
    {
    const TSize screen = ScreenSize();
    const TInt boardH = CPhKbdBoard::HeightFor( screen, iKeyHeightPct );
    const TRect boardRect( 0, screen.iHeight - boardH, screen.iWidth, screen.iHeight );
    const TRect icfRect( 0, 0, screen.iWidth, screen.iHeight - boardH );
    const CFont* font = CCoeEnv::Static()->NormalFont();
    TRAP_IGNORE( iIcf->SizeChangedL( icfRect, font->HeightInPixels(), font->FontMaxHeight(), font ) );
    iBoard->SetCustomisation( iKeyHeightPct, iTheme, iPopups );
    iBoard->SetRect( boardRect );
    }

TInt CPhKbdLayout::PenInputType()
    {
    return EPluginInputModePortraitFSQ;
    }

void CPhKbdLayout::OnActivate()
    {
    // a fresh open: the user's settings may have changed (Settings app), start on the letters page
    ReloadSettings();
    Arrange();
    iBoard->ResetPage();
    CFepUiLayout::OnActivate();
    RDebug::Printf( "[phkbd] activate" );
    }

TInt CPhKbdLayout::SizeChanged( const TAny* /*aData*/ )
    {
    SetRect( TRect( TPoint( 0, 0 ), ScreenSize() ) );
    Arrange();
    return KErrNone;
    }

TInt CPhKbdLayout::OnAppEditorTextComing( const TFepInputContextFieldData& aData )
    {
    TRAPD( err, iIcf->SetTextL( aData ) );
    return err;
    }

TInt CPhKbdLayout::HandleCommand( TInt aCmd, TUint8* aData )
    {
    if ( CFepUiLayout::HandleCommand( aCmd, aData ) == KErrNone )
        {
        return KErrNone;
        }
    switch ( aCmd )
        {
        case ECmdPenInputWindowOpen:
            LayoutOwner()->Hide( EFalse );
            RootControl()->UpdateValidRegion( NULL, EFalse );
            return KErrNone;
        case ECmdPenInputWindowClose:
            LayoutOwner()->Hide( ETrue );
            return KErrNone;
        case ECmdPenInputCase:
            {
            // the FEP's case (auto-capital after a full stop, its own shift handling): show it on the shift key
            const TInt c = *reinterpret_cast<TInt*>( aData );
            iBoard->SetShift( c == ECaseUpper ? EPhCaps : ( c == ECaseText ? EPhShift : EPhLower ) );
            return KErrNone;
            }
        case ECmdPenInputIsSecretText:
            iIcf->SetTextIsSecret( *aData );
            return KErrNone;
        case ECmdPenInputDimEnterKey:
            iBoard->SetEnterDimmed( *reinterpret_cast<TUint16*>( aData ) );
            return KErrNone;
        default:
            break;
        }
    return KErrNone;
    }

// ---------------------------------------------------------------------------------------------------------------
// events from the controls -> the FEP

void CPhKbdLayout::SendKeys( const TDesC& aText )
    {
    SignalOwner( ESignalKeyEvent, aText );
    }

void CPhKbdLayout::SendCase( TInt aPenCase )
    {
    TBuf<2> buf;
    buf.Append( reinterpret_cast<const TUint16*>( &aPenCase ), sizeof( TInt ) / 2 );
    SignalOwner( ESignalCaseMode, buf );
    }

// the globe key / long press on space: the next keyboard of the registry. When it is for another mode (the ITU-T keypad)
// the FEP reopens the pen input in that mode; when it serves the same mode (the stock QWERTY) it reopens the layout
// and the shim in the IME plug-in resolves the registry's new active entry.
void CPhKbdLayout::SwitchKeyboard()
    {
    TPhImEntry next;
    const TInt err = PhImNext( next );
    RDebug::Printf( "[phkbd] switch -> %d: entry %08x mode %d layout %08x", err, next.iEntryUid, next.iMode,
                    next.iLayoutUid );
    if ( err != KErrNone )
        {
        return;
        }
    TInt v[2] = { ETrue, next.iMode };
    TBuf<4> buf;
    buf.Append( reinterpret_cast<const TUint16*>( v ), 4 );
    SignalOwner( ESignalLayoutUIChanged, buf );
    }

void CPhKbdLayout::HandleControlEvent( TInt aEventType, CFepUiBaseCtrl* /*aCtrl*/, const TDesC& aEventData )
    {
    // one serial-log line per keyboard event: the test scripts (tools/phorepoc-x86/kbd_taps.py --sync) pace on these
    if ( aEventType >= EPhEvtText && aEventType <= EPhEvtClose )
        {
        RDebug::Printf( "[phkbd] event %d len %d", aEventType - EEventControlUserBase, aEventData.Length() );
        }
    switch ( aEventType )
        {
        case EPhEvtText:
            SendKeys( aEventData );
            break;
        case EPhEvtBackspace:
            {
            TBuf<1> b;
            b.Append( TChar( EKeyBackspace ) );
            SendKeys( b );
            break;
            }
        case EPhEvtEnter:
            {
            TBuf<1> b;
            b.Append( TChar( EKeyEnter ) );
            SendKeys( b );
            break;
            }
        case EPhEvtShift:
            {
            const TInt st = *reinterpret_cast<const TInt*>( aEventData.Ptr() );
            // the stock layouts' mapping (peninputdataconverter.cpp): shift = ECaseText, caps lock = ECaseUpper.
            // ECaseInverseText makes the FEP invert what the board already capitalised ("X" typed as "x").
            SendCase( st == EPhCaps ? ECaseUpper : ( st == EPhShift ? ECaseText : ECaseLower ) );
            break;
            }
        case EPhEvtSwitchKeyboard:
            SwitchKeyboard();
            break;
        case EPhEvtClose:
            SignalOwner( ESignalLayoutClosed );
            break;
        default:
            break;
        }
    }
