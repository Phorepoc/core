// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phkbd_board.cpp -- OURS (Phorepoc): the key area of the phkbd keyboard (see phkbd_board.h)
#include <coemain.h>
#include <w32std.h>
#include <bitstd.h>
#include <fbs.h>
#include "phkbd_board.h"
#include <peninputlayout.h>
#include <peninputlayoutctrlgroup.h>

namespace
    {
    const TInt KLongPressMs = 450;
    const TInt KSpaceLongMs = 650;
    const TInt KRepeatDelayMs = 450;
    const TInt KRepeatMs = 70;
    const TInt64 KDoubleTapUs = 450000;

    TInt Scale( TInt aValue, TInt aNum, TInt aDen ) { return ( aValue * aNum + aDen / 2 ) / aDen; }

    // the geometry (all derived from the board width so any screen size works)
    TInt KeyHeightPx( TInt aWidth, TInt aPct ) { return Scale( Scale( aWidth, 52, 360 ), aPct, 100 ); }
    TInt GapPx( TInt aWidth ) { return Max( 3, Scale( aWidth, 6, 360 ) ); }
    TInt SidePx( TInt aWidth ) { return Max( 2, Scale( aWidth, 3, 360 ) ); }
    TInt StripPx( TInt aKeyH ) { return aKeyH * 11 / 10; }
    TInt PadBottomPx( TInt aWidth ) { return Max( 3, Scale( aWidth, 6, 360 ) ); }

    TPhKbdColours Scheme( TInt aTheme )
        {
        TPhKbdColours c;
        switch ( aTheme )
            {
            case 1:   // light
                c.iPanel = TRgb( 212, 215, 221 ); c.iKey = TRgb( 255, 255, 255 ); c.iKeyFn = TRgb( 186, 191, 201 );
                c.iKeyDown = TRgb( 150, 172, 206 ); c.iAccent = TRgb( 20, 110, 200 ); c.iLabel = TRgb( 28, 28, 32 );
                c.iLabelDim = TRgb( 110, 114, 124 ); c.iPopup = TRgb( 255, 255, 255 ); c.iPopupLabel = TRgb( 20, 20, 24 );
                break;
            case 2:   // blue
                c.iPanel = TRgb( 10, 30, 54 ); c.iKey = TRgb( 30, 64, 104 ); c.iKeyFn = TRgb( 18, 44, 78 );
                c.iKeyDown = TRgb( 70, 124, 196 ); c.iAccent = TRgb( 255, 176, 32 ); c.iLabel = TRgb( 244, 247, 252 );
                c.iLabelDim = TRgb( 150, 172, 204 ); c.iPopup = TRgb( 70, 124, 196 ); c.iPopupLabel = TRgb( 255, 255, 255 );
                break;
            default:  // 0: dark, the N9 look
                c.iPanel = TRgb( 26, 27, 30 ); c.iKey = TRgb( 62, 64, 70 ); c.iKeyFn = TRgb( 42, 44, 49 );
                c.iKeyDown = TRgb( 104, 108, 118 ); c.iAccent = TRgb( 0, 150, 206 ); c.iLabel = TRgb( 242, 242, 244 );
                c.iLabelDim = TRgb( 140, 144, 152 ); c.iPopup = TRgb( 232, 234, 238 ); c.iPopupLabel = TRgb( 20, 21, 24 );
                break;
            }
        return c;
        }

    TBool IsLetter( TUint aChar ) { TChar c( aChar ); return c.IsAlpha(); }
    }

CPhKbdBoard* CPhKbdBoard::NewL( const TRect& aRect, CFepUiLayout* aLayout, TInt aControlId )
    {
    CPhKbdBoard* self = new ( ELeave ) CPhKbdBoard( aRect, aLayout, aControlId );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop( self );
    return self;
    }

CPhKbdBoard::CPhKbdBoard( const TRect& aRect, CFepUiLayout* aLayout, TInt aControlId )
    : CFepUiBaseCtrl( aRect, aLayout, aControlId ),
      iKeyHeightPct( 100 ), iTheme( 0 ), iPopups( ETrue ), iPage( 0 ), iShift( EPhLower ), iPressed( -1 ),
      iAccentSel( -1 ), iTimerPurpose( ENoTimer )
    {
    SetControlType( ECtrlBaseCtrl | EUserCtrlBase );
    }

void CPhKbdBoard::ConstructL()
    {
    BaseConstructL();
    iTimer = CPeriodic::NewL( CActive::EPriorityStandard );
    iCol = Scheme( iTheme );
    CreateFonts();
    BuildPage();
    }

CPhKbdBoard::~CPhKbdBoard()
    {
    if ( iTimer )
        {
        iTimer->Cancel();
        }
    delete iTimer;
    iKeys.Close();
    iAccentRects.Close();
    ReleaseFonts();
    }

TInt CPhKbdBoard::HeightFor( const TSize& aScreen, TInt aKeyHeightPercent )
    {
    const TInt keyH = KeyHeightPx( aScreen.iWidth, aKeyHeightPercent );
    return StripPx( keyH ) + 4 * ( keyH + GapPx( aScreen.iWidth ) ) + PadBottomPx( aScreen.iWidth );
    }

void CPhKbdBoard::SetCustomisation( TInt aKeyHeightPercent, TInt aTheme, TBool aPopups )
    {
    iKeyHeightPct = Min( 140, Max( 60, aKeyHeightPercent ) );
    iTheme = aTheme;
    iPopups = aPopups;
    iCol = Scheme( iTheme );
    ReleaseFonts();
    TRAP_IGNORE( CreateFonts() );
    }

void CPhKbdBoard::SetRect( const TRect& aRect )
    {
    CFepUiBaseCtrl::SetRect( aRect );
    BuildPage();
    }

void CPhKbdBoard::SetKeyboardName( const TDesC& aName )
    {
    iName.Copy( aName.Left( iName.MaxLength() ) );
    }

void CPhKbdBoard::SetEnterDimmed( TBool aDim )
    {
    iEnterDimmed = aDim;
    }

void CPhKbdBoard::ResetPage()
    {
    StopTimer();
    iPressed = -1;
    iAccentOpen = EFalse;
    iPage = 0;
    BuildPage();
    }

void CPhKbdBoard::SetShift( TPhShiftState aState )
    {
    if ( aState != iShift )
        {
        iShift = aState;
        if ( AbleToDraw() )
            {
            Refresh( iRect );
            }
        }
    }

void CPhKbdBoard::ConsumeShift()
    {
    if ( iShift == EPhShift )
        {
        iShift = EPhLower;
        TInt st = iShift;
        ReportEvent( EPhEvtShift, TPtrC( reinterpret_cast<const TUint16*>( &st ), sizeof( TInt ) / 2 ) );
        }
    }

// ---------------------------------------------------------------------------------------------------------------
// fonts

void CPhKbdBoard::CreateFonts()
    {
    CWsScreenDevice* dev = CCoeEnv::Static()->ScreenDevice();
    const TInt keyH = KeyHeightPx( iRect.Width() > 0 ? iRect.Width() : 360, iKeyHeightPct );
    TFontSpec spec = CCoeEnv::Static()->NormalFont()->FontSpecInTwips();
    spec.iHeight = Max( 12, keyH * 45 / 100 );
    User::LeaveIfError( dev->GetNearestFontInPixels( iKeyFont, spec ) );
    spec.iHeight = Max( 9, keyH * 30 / 100 );
    User::LeaveIfError( dev->GetNearestFontInPixels( iFnFont, spec ) );
    spec.iHeight = Max( 20, keyH * 85 / 100 );
    User::LeaveIfError( dev->GetNearestFontInPixels( iPopupFont, spec ) );
    }

void CPhKbdBoard::ReleaseFonts()
    {
    CWsScreenDevice* dev = CCoeEnv::Static()->ScreenDevice();
    if ( iKeyFont ) { dev->ReleaseFont( iKeyFont ); iKeyFont = NULL; }
    if ( iFnFont ) { dev->ReleaseFont( iFnFont ); iFnFont = NULL; }
    if ( iPopupFont ) { dev->ReleaseFont( iPopupFont ); iPopupFont = NULL; }
    }

// ---------------------------------------------------------------------------------------------------------------
// layout of the keys from the page data

void CPhKbdBoard::BuildPage()
    {
    iKeys.Reset();
    const TInt w = iRect.Width();
    if ( w <= 0 )
        {
        return;
        }
    const TInt keyH = KeyHeightPx( w, iKeyHeightPct );
    const TInt gap = GapPx( w );
    const TInt side = SidePx( w );
    iStripHeight = StripPx( keyH );
    const TPhPageDef& page = KPhPages[iPage];
    const TInt availW = w - 2 * side;
    for ( TInt r = 0; r < page.iRowCount; ++r )
        {
        const TPhRowDef& row = page.iRows[r];
        TInt total = 2 * row.iIndent;
        for ( TInt k = 0; k < row.iCount; ++k )
            {
            total += row.iKeys[k].iWidth;
            }
        const TInt rowTop = iRect.iTl.iY + iStripHeight + r * ( keyH + gap );
        TInt cum = row.iIndent;
        for ( TInt k = 0; k < row.iCount; ++k )
            {
            const TInt x0 = iRect.iTl.iX + side + availW * cum / total;
            cum += row.iKeys[k].iWidth;
            const TInt x1 = iRect.iTl.iX + side + availW * cum / total;
            TKey key;
            key.iDef = &row.iKeys[k];
            key.iRect = TRect( x0 + gap / 2, rowTop, x1 - ( gap - gap / 2 ), rowTop + keyH );
            // the tap area tiles the board: the gaps belong to the nearest key
            key.iHit = TRect( ( k == 0 && row.iIndent == 0 ) ? iRect.iTl.iX : x0,
                              r == 0 ? rowTop - gap : rowTop - gap / 2,
                              ( k == row.iCount - 1 && row.iIndent == 0 ) ? iRect.iBr.iX : x1,
                              rowTop + keyH + gap / 2 );
            iKeys.Append( key );
            }
        }
    }

TInt CPhKbdBoard::KeyAt( const TPoint& aPoint ) const
    {
    for ( TInt i = 0; i < iKeys.Count(); ++i )
        {
        if ( iKeys[i].iHit.Contains( aPoint ) )
            {
            return i;
            }
        }
    // not inside any cell (the indent of the short row, the bottom padding): the nearest key centre
    if ( aPoint.iY < iRect.iTl.iY + iStripHeight )
        {
        return -1;
        }
    TInt best = -1;
    TInt bestD = KMaxTInt;
    for ( TInt i = 0; i < iKeys.Count(); ++i )
        {
        const TPoint c = iKeys[i].iRect.Center();
        const TInt dx = c.iX - aPoint.iX;
        const TInt dy = c.iY - aPoint.iY;
        const TInt d = dx * dx + dy * dy;
        if ( d < bestD )
            {
            bestD = d;
            best = i;
            }
        }
    return best;
    }

TUint CPhKbdBoard::Display( const TPhKeyDef& aDef ) const
    {
    TUint c = aDef.iCode;
    if ( iPage == 0 && iShift != EPhLower && IsLetter( c ) )
        {
        TChar ch( c );
        ch.UpperCase();
        c = ch;
        }
    return c;
    }

// ---------------------------------------------------------------------------------------------------------------
// drawing

void CPhKbdBoard::Draw()
    {
    if ( AbleToDraw() )
        {
        DrawBoard( iRect );
        }
    }

void CPhKbdBoard::ReDrawRect( const TRect& aRect )
    {
    if ( AbleToDraw() )
        {
        TRect r( aRect );
        r.Intersection( iRect );
        if ( !r.IsEmpty() )
            {
            DrawBoard( r );
            }
        }
    }

void CPhKbdBoard::Refresh( const TRect& aRect )
    {
    TRect r( aRect );
    r.Intersection( iRect );
    if ( r.IsEmpty() || !AbleToDraw() )
        {
        return;
        }
    DrawBoard( r );
    UpdateArea( r, EFalse );
    }

void CPhKbdBoard::DrawBoard( const TRect& aClip )
    {
    CFbsBitGc* gc = static_cast<CFbsBitGc*>( BitGc() );
    // the mask first (opaque: the keyboard covers whatever is below it); the framework leaves the gc on the MASK device
    // afterwards, so re-activate the colour bitmap before drawing the keys
    DrawOpaqueMaskBackground( aClip, EFalse );
    gc->Activate( BitmapDevice() );
    gc->SetClippingRect( aClip );
    gc->SetPenStyle( CGraphicsContext::ENullPen );
    gc->SetBrushStyle( CGraphicsContext::ESolidBrush );
    gc->SetBrushColor( iCol.iPanel );
    gc->DrawRect( iRect );
    DrawStrip( *gc );
    for ( TInt i = 0; i < iKeys.Count(); ++i )
        {
        if ( iKeys[i].iRect.Intersects( aClip ) )
            {
            DrawKey( *gc, iKeys[i], i == iPressed && !iAccentOpen );
            }
        }
    if ( iAccentOpen )
        {
        DrawAccents( *gc );
        }
    else if ( iPopups && iPressed >= 0 && iKeys[iPressed].iDef->iKind == EKChar )
        {
        DrawPreview( *gc );
        }
    gc->CancelClipping();
    }

void CPhKbdBoard::DrawGlyph( CBitmapContext& aGc, const TRect& aRect, const TDesC& aText, const CFont* aFont, TRgb aColour )
    {
    aGc.UseFont( aFont );
    aGc.SetPenStyle( CGraphicsContext::ESolidPen );
    aGc.SetPenColor( aColour );
    aGc.SetBrushStyle( CGraphicsContext::ENullBrush );
    const TInt base = ( aRect.Height() - aFont->HeightInPixels() ) / 2 + aFont->AscentInPixels();
    aGc.DrawText( aText, aRect, base, CGraphicsContext::ECenter );
    aGc.DiscardFont();
    }

void CPhKbdBoard::DrawStrip( CBitmapContext& aGc )
    {
    const TRect strip( iRect.iTl, TSize( iRect.Width(), iStripHeight ) );
    // the name of the keyboard on the left, a hide chevron on the right
    if ( iName.Length() )
        {
        TRect nameRect( strip.iTl.iX + 8, strip.iTl.iY, strip.iTl.iX + strip.Width() / 2, strip.iBr.iY - 4 );
        aGc.UseFont( iFnFont );
        aGc.SetPenStyle( CGraphicsContext::ESolidPen );
        aGc.SetPenColor( iCol.iLabelDim );
        aGc.SetBrushStyle( CGraphicsContext::ENullBrush );
        const TInt base = ( nameRect.Height() - iFnFont->HeightInPixels() ) / 2 + iFnFont->AscentInPixels();
        aGc.DrawText( iName, nameRect, base, CGraphicsContext::ELeft );
        aGc.DiscardFont();
        }
    const TInt cw = iStripHeight;
    TRect chev( strip.iBr.iX - cw * 2, strip.iTl.iY, strip.iBr.iX - 4, strip.iBr.iY - 4 );
    DrawIcon( aGc, chev, 100, iCol.iLabelDim );
    }

void CPhKbdBoard::DrawKey( CBitmapContext& aGc, const TKey& aKey, TBool aPressed )
    {
    const TPhKeyDef& def = *aKey.iDef;
    const TRect& r = aKey.iRect;
    const TInt radius = Max( 3, r.Height() / 5 );
    const TBool fn = def.iKind != EKChar && def.iKind != EKSpace;
    TRgb fill = aPressed ? iCol.iKeyDown : ( fn ? iCol.iKeyFn : iCol.iKey );
    if ( def.iKind == EKShift && iShift != EPhLower )
        {
        fill = iCol.iKeyFn;
        }
    if ( def.iKind == EKEnter )
        {
        fill = aPressed ? iCol.iKeyDown : iCol.iAccent;
        }
    // a darker bottom edge gives the key its height
    aGc.SetPenStyle( CGraphicsContext::ENullPen );
    aGc.SetBrushStyle( CGraphicsContext::ESolidBrush );
    aGc.SetBrushColor( TRgb( fill.Red() * 55 / 100, fill.Green() * 55 / 100, fill.Blue() * 55 / 100 ) );
    TRect edge( r );
    edge.Move( 0, 2 );
    aGc.DrawRoundRect( edge, TSize( radius, radius ) );
    aGc.SetBrushColor( fill );
    TRect face( r );
    if ( aPressed )
        {
        face.Move( 0, 1 );
        }
    aGc.DrawRoundRect( face, TSize( radius, radius ) );

    TBuf<4> text;
    switch ( def.iKind )
        {
        case EKChar:
            {
            text.Append( TChar( Display( def ) ) );
            DrawGlyph( aGc, face, text, iKeyFont, iCol.iLabel );
            break;
            }
        case EKSymbols:
            text.Copy( _L( "?123" ) );
            DrawGlyph( aGc, face, text, iFnFont, iCol.iLabel );
            break;
        case EKLetters:
            text.Copy( _L( "ABC" ) );
            DrawGlyph( aGc, face, text, iFnFont, iCol.iLabel );
            break;
        case EKMore:
            text.Copy( iPage == 1 ? _L( "1/2" ) : _L( "2/2" ) );
            DrawGlyph( aGc, face, text, iFnFont, iCol.iLabel );
            break;
        case EKSpace:
            if ( iName.Length() )
                {
                DrawGlyph( aGc, face, iName, iFnFont, iCol.iLabelDim );
                }
            break;
        case EKShift:
            DrawIcon( aGc, face, EKShift, iShift == EPhLower ? iCol.iLabel : iCol.iAccent );
            break;
        case EKBackspace:
        case EKGlobe:
            DrawIcon( aGc, face, def.iKind, iCol.iLabel );
            break;
        case EKEnter:
            DrawIcon( aGc, face, EKEnter, iEnterDimmed ? iCol.iLabelDim : TRgb( 255, 255, 255 ) );
            break;
        default:
            break;
        }
    }

// icons are drawn with lines and polygons so they scale with the key and need no bitmap files
void CPhKbdBoard::DrawIcon( CBitmapContext& aGc, const TRect& aRect, TInt aKind, TRgb aColour )
    {
    const TPoint c = aRect.Center();
    const TInt s = Min( aRect.Width(), aRect.Height() ) * 30 / 100;    // half size of the glyph
    aGc.SetPenColor( aColour );
    aGc.SetPenStyle( CGraphicsContext::ESolidPen );
    aGc.SetPenSize( TSize( Max( 2, s / 6 ), Max( 2, s / 6 ) ) );
    switch ( aKind )
        {
        case EKShift:
            {
            TPoint p[7] = { TPoint( c.iX, c.iY - s ), TPoint( c.iX + s, c.iY ), TPoint( c.iX + s / 2, c.iY ),
                            TPoint( c.iX + s / 2, c.iY + s * 7 / 10 ), TPoint( c.iX - s / 2, c.iY + s * 7 / 10 ),
                            TPoint( c.iX - s / 2, c.iY ), TPoint( c.iX - s, c.iY ) };
            aGc.SetBrushStyle( iShift == EPhLower ? CGraphicsContext::ENullBrush : CGraphicsContext::ESolidBrush );
            aGc.SetBrushColor( aColour );
            aGc.DrawPolygon( p, 7 );
            if ( iShift == EPhCaps )
                {
                aGc.SetBrushColor( iCol.iKeyFn );
                aGc.DrawLine( TPoint( c.iX - s / 2, c.iY + s ), TPoint( c.iX + s / 2, c.iY + s ) );
                }
            break;
            }
        case EKBackspace:
            {
            TPoint p[5] = { TPoint( c.iX - s * 13 / 10, c.iY ), TPoint( c.iX - s * 5 / 10, c.iY - s * 8 / 10 ),
                            TPoint( c.iX + s * 13 / 10, c.iY - s * 8 / 10 ), TPoint( c.iX + s * 13 / 10, c.iY + s * 8 / 10 ),
                            TPoint( c.iX - s * 5 / 10, c.iY + s * 8 / 10 ) };
            aGc.SetBrushStyle( CGraphicsContext::ENullBrush );
            aGc.DrawPolygon( p, 5 );
            aGc.DrawLine( TPoint( c.iX - s / 5, c.iY - s / 3 ), TPoint( c.iX + s * 6 / 10, c.iY + s / 3 ) );
            aGc.DrawLine( TPoint( c.iX + s * 6 / 10, c.iY - s / 3 ), TPoint( c.iX - s / 5, c.iY + s / 3 ) );
            break;
            }
        case EKEnter:
            {
            aGc.DrawLine( TPoint( c.iX + s, c.iY - s / 2 ), TPoint( c.iX + s, c.iY + s / 3 ) );
            aGc.DrawLine( TPoint( c.iX + s, c.iY + s / 3 ), TPoint( c.iX - s, c.iY + s / 3 ) );
            aGc.DrawLine( TPoint( c.iX - s, c.iY + s / 3 ), TPoint( c.iX - s / 2, c.iY - s / 6 ) );
            aGc.DrawLine( TPoint( c.iX - s, c.iY + s / 3 ), TPoint( c.iX - s / 2, c.iY + s * 5 / 6 ) );
            break;
            }
        case EKGlobe:
            {
            aGc.SetBrushStyle( CGraphicsContext::ENullBrush );
            aGc.DrawEllipse( TRect( c.iX - s, c.iY - s, c.iX + s, c.iY + s ) );
            aGc.DrawEllipse( TRect( c.iX - s / 2, c.iY - s, c.iX + s / 2, c.iY + s ) );
            aGc.DrawLine( TPoint( c.iX - s, c.iY ), TPoint( c.iX + s, c.iY ) );
            break;
            }
        case 100:       // the hide chevron
            {
            aGc.DrawLine( TPoint( c.iX - s / 2, c.iY - s / 4 ), TPoint( c.iX, c.iY + s / 4 ) );
            aGc.DrawLine( TPoint( c.iX, c.iY + s / 4 ), TPoint( c.iX + s / 2, c.iY - s / 4 ) );
            break;
            }
        default:
            break;
        }
    aGc.SetPenSize( TSize( 1, 1 ) );
    }

void CPhKbdBoard::DrawPreview( CBitmapContext& aGc )
    {
    const TKey& key = iKeys[iPressed];
    const TInt keyW = key.iRect.Width();
    const TInt keyH = key.iRect.Height();
    TRect pop( 0, 0, keyW * 14 / 10, keyH * 115 / 100 );
    pop.Move( key.iRect.Center().iX - pop.Width() / 2, key.iRect.iTl.iY - pop.Height() + keyH / 8 );
    // keep inside the board
    if ( pop.iTl.iX < iRect.iTl.iX ) pop.Move( iRect.iTl.iX - pop.iTl.iX, 0 );
    if ( pop.iBr.iX > iRect.iBr.iX ) pop.Move( iRect.iBr.iX - pop.iBr.iX, 0 );
    if ( pop.iTl.iY < iRect.iTl.iY ) pop.Move( 0, iRect.iTl.iY - pop.iTl.iY );
    const TInt radius = Max( 4, keyH / 4 );
    aGc.SetPenStyle( CGraphicsContext::ENullPen );
    aGc.SetBrushStyle( CGraphicsContext::ESolidBrush );
    aGc.SetBrushColor( TRgb( 0, 0, 0 ) );
    TRect shadow( pop );
    shadow.Move( 0, 3 );
    aGc.DrawRoundRect( shadow, TSize( radius, radius ) );
    aGc.SetBrushColor( iCol.iPopup );
    aGc.DrawRoundRect( pop, TSize( radius, radius ) );
    TBuf<2> text;
    text.Append( TChar( Display( *key.iDef ) ) );
    DrawGlyph( aGc, pop, text, iPopupFont, iCol.iPopupLabel );
    }

void CPhKbdBoard::DrawAccents( CBitmapContext& aGc )
    {
    const TInt radius = Max( 4, iAccentBar.Height() / 5 );
    aGc.SetPenStyle( CGraphicsContext::ENullPen );
    aGc.SetBrushStyle( CGraphicsContext::ESolidBrush );
    aGc.SetBrushColor( TRgb( 0, 0, 0 ) );
    TRect shadow( iAccentBar );
    shadow.Move( 0, 3 );
    aGc.DrawRoundRect( shadow, TSize( radius, radius ) );
    aGc.SetBrushColor( iCol.iPopup );
    aGc.DrawRoundRect( iAccentBar, TSize( radius, radius ) );
    for ( TInt i = 0; i < iAccentRects.Count(); ++i )
        {
        TRect cell( iAccentRects[i] );
        cell.Shrink( 2, 3 );
        if ( i == iAccentSel )
            {
            aGc.SetBrushColor( iCol.iAccent );
            aGc.DrawRoundRect( cell, TSize( radius, radius ) );
            }
        TBuf<2> text;
        text.Append( TChar( iAccentChars[i] ) );
        DrawGlyph( aGc, cell, text, iKeyFont, i == iAccentSel ? TRgb( 255, 255, 255 ) : iCol.iPopupLabel );
        }
    }

// ---------------------------------------------------------------------------------------------------------------
// input

void CPhKbdBoard::PressKey( TInt aIndex )
    {
    iPressed = aIndex;
    iLongHandled = EFalse;
    StopTimer();
    if ( aIndex < 0 )
        {
        return;
        }
    const TPhKeyDef& def = *iKeys[aIndex].iDef;
    if ( def.iKind == EKChar )
        {
        for ( TInt i = 0; i < KPhAccentCount; ++i )
            {
            if ( KPhAccents[i].iBase == def.iCode )
                {
                iTimerPurpose = ELongPress;
                StartTimer( KLongPressMs, KLongPressMs );
                break;
                }
            }
        }
    else if ( def.iKind == EKSpace )
        {
        iTimerPurpose = ELongPress;
        StartTimer( KSpaceLongMs, KSpaceLongMs );
        }
    else if ( def.iKind == EKBackspace )
        {
        ReportEvent( EPhEvtBackspace );
        iLongHandled = ETrue;                       // the key has acted: its release does nothing
        iTimerPurpose = ERepeat;
        StartTimer( KRepeatDelayMs, KRepeatMs );
        }
    }

CFepUiBaseCtrl* CPhKbdBoard::HandlePointerDownEventL( const TPoint& aPoint )
    {
    CFepUiBaseCtrl::HandlePointerDownEventL( aPoint );
    iDownPoint = aPoint;
    iAccentOpen = EFalse;
    TRect dirty;
    const TRect strip( iRect.iTl, TSize( iRect.Width(), iStripHeight ) );
    if ( strip.Contains( aPoint ) )
        {
        iPressed = -1;
        return this;
        }
    const TInt idx = KeyAt( aPoint );
    PressKey( idx );
    if ( idx >= 0 )
        {
        dirty = iKeys[idx].iRect;
        dirty.Grow( iKeys[idx].iRect.Width(), iKeys[idx].iRect.Height() * 3 / 2 );
        Refresh( dirty );
        }
    return this;
    }

CFepUiBaseCtrl* CPhKbdBoard::HandlePointerMoveEventL( const TPoint& aPoint )
    {
    CFepUiBaseCtrl::HandlePointerMoveEventL( aPoint );
    if ( iAccentOpen )
        {
        const TInt sel = AccentAt( aPoint );
        if ( sel != iAccentSel )
            {
            iAccentSel = sel;
            Refresh( iAccentBar );
            }
        return this;
        }
    if ( iPressed < 0 )
        {
        return this;
        }
    const TInt idx = KeyAt( aPoint );
    if ( idx != iPressed && idx >= 0 && iKeys[iPressed].iDef->iKind != EKBackspace )
        {
        // the finger slid to another key: that one is the target now (as Android does)
        TRect dirty( iKeys[iPressed].iRect );
        dirty.Grow( dirty.Width(), dirty.Height() * 3 / 2 );
        PressKey( idx );
        TRect d2( iKeys[idx].iRect );
        d2.Grow( d2.Width(), d2.Height() * 3 / 2 );
        dirty.BoundingRect( d2 );
        Refresh( dirty );
        }
    return this;
    }

CFepUiBaseCtrl* CPhKbdBoard::HandlePointerUpEventL( const TPoint& aPoint )
    {
    CFepUiBaseCtrl::HandlePointerUpEventL( aPoint );
    StopTimer();
    const TRect strip( iRect.iTl, TSize( iRect.Width(), iStripHeight ) );
    if ( iAccentOpen )
        {
        const TInt sel = AccentAt( aPoint );
        TRect dirty( iAccentBar );
        iAccentOpen = EFalse;
        TRect keyArea;
        if ( iPressed >= 0 )
            {
            keyArea = iKeys[iPressed].iRect;
            keyArea.Grow( keyArea.Width(), keyArea.Height() * 3 / 2 );
            dirty.BoundingRect( keyArea );
            }
        iPressed = -1;
        const TPhShiftState shiftBefore = iShift;
        if ( sel >= 0 )
            {
            CommitText( iAccentChars[sel] );
            }
        if ( iShift != shiftBefore )
            {
            dirty = iRect;
            }
        Refresh( dirty );
        return this;
        }
    if ( iPressed < 0 )
        {
        if ( strip.Contains( aPoint ) && strip.Contains( iDownPoint ) && aPoint.iX > strip.iBr.iX - strip.Height() * 2 )
            {
            ReportEvent( EPhEvtClose );
            }
        return this;
        }
    const TInt idx = iPressed;
    const TPhKeyDef& def = *iKeys[idx].iDef;
    TRect dirty( iKeys[idx].iRect );
    dirty.Grow( dirty.Width(), dirty.Height() * 3 / 2 );
    const TBool handled = iLongHandled;
    const TPhShiftState shiftBefore = iShift;
    iPressed = -1;
    TBool full = EFalse;
    if ( !handled )
        {
        switch ( def.iKind )
            {
            case EKChar:
                CommitText( Display( def ) );
                break;
            case EKSpace:
                CommitText( ' ' );
                break;
            case EKEnter:
                ReportEvent( EPhEvtEnter );
                break;
            case EKGlobe:
                ReportEvent( EPhEvtSwitchKeyboard );
                break;
            case EKShift:
                {
                TTime now;
                now.UniversalTime();
                if ( iShift == EPhLower )
                    {
                    TTimeIntervalMicroSeconds since = now.MicroSecondsFrom( iLastShiftTap );
                    iShift = since.Int64() < KDoubleTapUs ? EPhCaps : EPhShift;
                    }
                else
                    {
                    TTimeIntervalMicroSeconds since = now.MicroSecondsFrom( iLastShiftTap );
                    iShift = ( iShift == EPhShift && since.Int64() < KDoubleTapUs ) ? EPhCaps : EPhLower;
                    }
                iLastShiftTap = now;
                TInt st = iShift;
                ReportEvent( EPhEvtShift, TPtrC( reinterpret_cast<const TUint16*>( &st ), sizeof( TInt ) / 2 ) );
                full = ETrue;
                break;
                }
            case EKSymbols:
                iPage = 1;
                full = ETrue;
                break;
            case EKLetters:
                iPage = 0;
                full = ETrue;
                break;
            case EKMore:
                iPage = iPage == 1 ? 2 : 1;
                full = ETrue;
                break;
            default:
                break;
            }
        }
    if ( iShift != shiftBefore )
        {
        full = ETrue;           // the key labels change case
        }
    if ( full )
        {
        BuildPage();
        dirty = iRect;
        }
    Refresh( dirty );
    return this;
    }

void CPhKbdBoard::CancelPointerDownL()
    {
    StopTimer();
    iPressed = -1;
    iAccentOpen = EFalse;
    CFepUiBaseCtrl::CancelPointerDownL();
    Refresh( iRect );
    }

void CPhKbdBoard::CommitText( TUint aChar )
    {
    TBuf<2> text;
    text.Append( TChar( aChar ) );
    ReportEvent( EPhEvtText, text );
    if ( iShift == EPhShift && iPage == 0 )
        {
        ConsumeShift();
        }
    }

// ---------------------------------------------------------------------------------------------------------------
// long press: accents, and the switch on the space key

void CPhKbdBoard::OpenAccents()
    {
    if ( iPressed < 0 )
        {
        return;
        }
    const TPhKeyDef& def = *iKeys[iPressed].iDef;
    for ( TInt i = 0; i < KPhAccentCount; ++i )
        {
        if ( KPhAccents[i].iBase != def.iCode )
            {
            continue;
            }
        iAccentChars.Zero();
        iAccentRects.Reset();
        for ( const TUint16* p = KPhAccents[i].iList; *p && iAccentChars.Length() < iAccentChars.MaxLength(); ++p )
            {
            TChar ch( *p );
            if ( iPage == 0 && iShift != EPhLower )
                {
                ch.UpperCase();
                }
            iAccentChars.Append( ch );
            }
        const TRect& kr = iKeys[iPressed].iRect;
        const TInt cellW = Max( kr.Width() * 9 / 10, kr.Height() * 8 / 10 );
        const TInt cellH = kr.Height() * 11 / 10;
        const TInt n = iAccentChars.Length();
        TRect bar( 0, 0, cellW * n + 8, cellH + 6 );
        bar.Move( kr.Center().iX - bar.Width() / 2, kr.iTl.iY - bar.Height() - 2 );
        if ( bar.iTl.iX < iRect.iTl.iX + 2 ) bar.Move( iRect.iTl.iX + 2 - bar.iTl.iX, 0 );
        if ( bar.iBr.iX > iRect.iBr.iX - 2 ) bar.Move( iRect.iBr.iX - 2 - bar.iBr.iX, 0 );
        if ( bar.iTl.iY < iRect.iTl.iY ) bar.Move( 0, iRect.iTl.iY - bar.iTl.iY );
        iAccentBar = bar;
        for ( TInt k = 0; k < n; ++k )
            {
            iAccentRects.Append( TRect( bar.iTl.iX + 4 + k * cellW, bar.iTl.iY + 3, bar.iTl.iX + 4 + ( k + 1 ) * cellW, bar.iBr.iY - 3 ) );
            }
        iAccentSel = 0;
        // the accent nearest the finger starts selected
        const TInt s = AccentAt( iDownPoint );
        if ( s >= 0 )
            {
            iAccentSel = s;
            }
        iAccentOpen = ETrue;
        iLongHandled = ETrue;
        TRect dirty( iAccentBar );
        dirty.BoundingRect( kr );
        Refresh( dirty );
        return;
        }
    }

TInt CPhKbdBoard::AccentAt( const TPoint& aPoint ) const
    {
    if ( !iAccentOpen || iAccentRects.Count() == 0 )
        {
        return -1;
        }
    // a generous vertical window: the finger is below the bar, on the key
    if ( aPoint.iX < iAccentBar.iTl.iX - 6 || aPoint.iX > iAccentBar.iBr.iX + 6 )
        {
        return -1;
        }
    for ( TInt i = 0; i < iAccentRects.Count(); ++i )
        {
        if ( aPoint.iX >= iAccentRects[i].iTl.iX && aPoint.iX < iAccentRects[i].iBr.iX )
            {
            return i;
            }
        }
    return aPoint.iX < iAccentRects[0].iTl.iX ? 0 : iAccentRects.Count() - 1;
    }

void CPhKbdBoard::StartTimer( TInt aDelayMs, TInt aIntervalMs )
    {
    iTimer->Cancel();
    iTimer->Start( TTimeIntervalMicroSeconds32( aDelayMs * 1000 ), TTimeIntervalMicroSeconds32( aIntervalMs * 1000 ),
                   TCallBack( &CPhKbdBoard::TimerTick, this ) );
    }

void CPhKbdBoard::StopTimer()
    {
    if ( iTimer )
        {
        iTimer->Cancel();
        }
    iTimerPurpose = ENoTimer;
    }

TInt CPhKbdBoard::TimerTick( TAny* aThis )
    {
    static_cast<CPhKbdBoard*>( aThis )->OnTimer();
    return 0;
    }

void CPhKbdBoard::OnTimer()
    {
    const TTimerPurpose purpose = iTimerPurpose;
    if ( purpose == ERepeat )
        {
        ReportEvent( EPhEvtBackspace );
        return;
        }
    StopTimer();
    if ( purpose == ELongPress && iPressed >= 0 )
        {
        if ( iKeys[iPressed].iDef->iKind == EKSpace )
            {
            iLongHandled = ETrue;
            TRect dirty( iKeys[iPressed].iRect );
            iPressed = -1;
            Refresh( dirty );
            ReportEvent( EPhEvtSwitchKeyboard );
            }
        else
            {
            OpenAccents();
            }
        }
    }
