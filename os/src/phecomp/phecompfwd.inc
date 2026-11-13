// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause

// phecompfwd.inc -- the MWsGraphicsContext members of phecomp's TGcProxy: each forwards to the current target T()
// (one line per pure virtual of graphics/wsgraphicscontext.h)
	void BitBlt(const TPoint& aDestPos, const CFbsBitmap& aSourceBitmap) { T()->BitBlt(aDestPos, aSourceBitmap); }
	void BitBlt(const TPoint& aDestPos, const CFbsBitmap& aSourceBitmap, const TRect& aSourceRect) { T()->BitBlt(aDestPos, aSourceBitmap, aSourceRect); }
	void BitBltMasked(const TPoint& aDestPos, const CFbsBitmap& aSourceBitmap, const TRect& aSourceRect, const CFbsBitmap& aMaskBitmap, TBool aInvertMask) { T()->BitBltMasked(aDestPos, aSourceBitmap, aSourceRect, aMaskBitmap, aInvertMask); }
	void BitBltMasked(const TPoint& aDestPos, const CFbsBitmap& aSourceBitmap, const TRect& aSourceRect, const CFbsBitmap& aMaskBitmap, const TPoint& aMaskPos) { T()->BitBltMasked(aDestPos, aSourceBitmap, aSourceRect, aMaskBitmap, aMaskPos); }
	void ResetClippingRegion() { T()->ResetClippingRegion(); }
	void Clear() { T()->Clear(); }
	void Clear(const TRect& aRect) { T()->Clear(aRect); }
	void ResetBrushPattern() { T()->ResetBrushPattern(); }
	void ResetFont() { T()->ResetFont(); }
	void DrawArc(const TRect& aRect, const TPoint& aStart, const TPoint& aEnd) { T()->DrawArc(aRect, aStart, aEnd); }
	void DrawPie(const TRect& aRect, const TPoint& aStart, const TPoint& aEnd) { T()->DrawPie(aRect, aStart, aEnd); }
	void DrawBitmap(const TRect& aDestRect, const CFbsBitmap& aSourceBitmap) { T()->DrawBitmap(aDestRect, aSourceBitmap); }
	void DrawBitmap(const TRect& aDestRect, const CFbsBitmap& aSourceBitmap, const TRect& aSourceRect) { T()->DrawBitmap(aDestRect, aSourceBitmap, aSourceRect); }
	void DrawBitmapMasked(const TRect& aDestRect, const CFbsBitmap& aSourceBitmap, const TRect& aSourceRect, const CFbsBitmap& aMaskBitmap, TBool aInvertMask) { T()->DrawBitmapMasked(aDestRect, aSourceBitmap, aSourceRect, aMaskBitmap, aInvertMask); }
	void DrawRoundRect(const TRect& aRect, const TSize& aEllipse) { T()->DrawRoundRect(aRect, aEllipse); }
	void DrawPolyLine(const TArray<TPoint>& aPointList) { T()->DrawPolyLine(aPointList); }
	void DrawPolyLineNoEndPoint(const TArray<TPoint>& aPointList) { T()->DrawPolyLineNoEndPoint(aPointList); }
	void DrawPolygon(const TArray<TPoint>& aPointList, TFillRule aFillRule) { T()->DrawPolygon(aPointList, aFillRule); }
	void DrawEllipse(const TRect& aRect) { T()->DrawEllipse(aRect); }
	void DrawLine(const TPoint& aStart, const TPoint& aEnd) { T()->DrawLine(aStart, aEnd); }
	void DrawLineTo(const TPoint& aPoint) { T()->DrawLineTo(aPoint); }
	void DrawLineBy(const TPoint& aVector) { T()->DrawLineBy(aVector); }
	void DrawRect(const TRect& aRect) { T()->DrawRect(aRect); }
	void DrawText(const TDesC& aText, const TTextParameters* aParam) { T()->DrawText(aText, aParam); }
	void DrawText(const TDesC& aText, const TTextParameters* aParam, const TPoint& aPosition) { T()->DrawText(aText, aParam, aPosition); }
	void DrawText(const TDesC& aText, const TTextParameters* aParam, const TRect& aClipRect) { T()->DrawText(aText, aParam, aClipRect); }
	void DrawText(const TDesC& aText, const TTextParameters* aParam, const TRect& aClipFillRect, TInt aBaselineOffset, TTextAlign aHrz, TInt aMargin) { T()->DrawText(aText, aParam, aClipFillRect, aBaselineOffset, aHrz, aMargin); }
	void DrawTextVertical(const TDesC& aText, const TTextParameters* aParam, TBool aUp) { T()->DrawTextVertical(aText, aParam, aUp); }
	void DrawTextVertical(const TDesC& aText, const TTextParameters* aParam, const TPoint& aPosition, TBool aUp) { T()->DrawTextVertical(aText, aParam, aPosition, aUp); }
	void DrawTextVertical(const TDesC& aText, const TTextParameters* aParam, const TRect& aClipRect, TBool aUp) { T()->DrawTextVertical(aText, aParam, aClipRect, aUp); }
	void DrawTextVertical(const TDesC& aText, const TTextParameters* aParam, const TRect& aClipRect, TInt aBaselineOffset, TBool aUp, TTextAlign aVert, TInt aMargin) { T()->DrawTextVertical(aText, aParam, aClipRect, aBaselineOffset, aUp, aVert, aMargin); }
	void DrawTextVertical(const TDesC& aText, const TTextParameters* aParam, const TRect& aClipRect, TInt aBaselineOffset, TInt aTextWidth, TBool aUp, TTextAlign aVert, TInt aMargin) { T()->DrawTextVertical(aText, aParam, aClipRect, aBaselineOffset, aTextWidth, aUp, aVert, aMargin); }
	void MoveTo(const TPoint& aPoint) { T()->MoveTo(aPoint); }
	void MoveBy(const TPoint& aVector) { T()->MoveBy(aVector); }
	void Plot(const TPoint& aPoint) { T()->Plot(aPoint); }
	void Reset() { T()->Reset(); }
	void SetBrushColor(const TRgb& aColor) { T()->SetBrushColor(aColor); }
	void SetBrushOrigin(const TPoint& aOrigin) { T()->SetBrushOrigin(aOrigin); }
	void SetBrushStyle(TBrushStyle aBrushStyle) { T()->SetBrushStyle(aBrushStyle); }
	void SetClippingRegion(const TRegion& aRegion) { T()->SetClippingRegion(aRegion); }
	void SetDrawMode(TDrawMode aDrawMode) { T()->SetDrawMode(aDrawMode); }
	void SetOrigin(const TPoint& aPoint) { T()->SetOrigin(aPoint); }
	void SetPenColor(const TRgb& aColor) { T()->SetPenColor(aColor); }
	void SetPenStyle(TPenStyle aPenStyle) { T()->SetPenStyle(aPenStyle); }
	void SetPenSize(const TSize& aSize) { T()->SetPenSize(aSize); }
	void SetTextShadowColor(const TRgb& aColor) { T()->SetTextShadowColor(aColor); }
	void SetCharJustification(TInt aExcessWidth, TInt aNumChars) { T()->SetCharJustification(aExcessWidth, aNumChars); }
	void SetWordJustification(TInt aExcessWidth, TInt aNumGaps) { T()->SetWordJustification(aExcessWidth, aNumGaps); }
	void SetUnderlineStyle(TFontUnderline aUnderlineStyle) { T()->SetUnderlineStyle(aUnderlineStyle); }
	void SetStrikethroughStyle(TFontStrikethrough aStrikethroughStyle) { T()->SetStrikethroughStyle(aStrikethroughStyle); }
	void SetBrushPattern(const CFbsBitmap& aBitmap) { T()->SetBrushPattern(aBitmap); }
	void SetBrushPattern(TInt aFbsBitmapHandle) { T()->SetBrushPattern(aFbsBitmapHandle); }
	void SetFont(const CFont* aFont) { T()->SetFont(aFont); }
	void CopyRect(const TPoint& aOffset, const TRect& aRect) { T()->CopyRect(aOffset, aRect); }
	void UpdateJustification(const TDesC& aText, const TTextParameters* aParam) { T()->UpdateJustification(aText, aParam); }
	void UpdateJustificationVertical(const TDesC& aText, const TTextParameters* aParam, TBool aUp) { T()->UpdateJustificationVertical(aText, aParam, aUp); }
	void SetFontNoDuplicate(const CFont* aFont) { T()->SetFontNoDuplicate(aFont); }
	TBool HasBrushPattern() const { return T()->HasBrushPattern(); }
	TBool HasFont() const { return T()->HasFont(); }
	TRgb BrushColor() const { return T()->BrushColor(); }
	TRgb PenColor() const { return T()->PenColor(); }
	TRgb TextShadowColor() const { return T()->TextShadowColor(); }
	TInt GetError() { return T()->GetError(); }
	TPoint Origin() const { return T()->Origin(); }
	const TRegion& ClippingRegion() { return T()->ClippingRegion(); }
	TInt Push() { return T()->Push(); }
	void Pop() { T()->Pop(); }
