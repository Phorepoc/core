// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_layout.h -- the pure layout of the browser frontend: where the page, the address row and the action row go for a
// given screen size and bar position. No CONE types (plain ints), so it can be unit-tested on the host.
#ifndef PB_LAYOUT_H
#define PB_LAYOUT_H

enum TPbBarPos { EPbBarTop = 0, EPbBarBottom = 1 };

struct TPbRect
	{
	int iX, iY, iW, iH;
	};

struct TPbLayout
	{
	TPbRect iPage;      // where the engine view is drawn
	TPbRect iAddress;   // address row: security icon, URL, clear/stop
	TPbRect iActions;   // action row: back, forward, reload, tabs, overflow (empty when folded into the address row)
	TPbRect iProgress;  // the progress line (3 px), adjacent to the address row on the page side
	int iActionCount;   // buttons in iActions (or in the folded address row)
	};

// aRowH = 56 in portrait, 48 in landscape (the caller chooses). Landscape (aW > aH) folds both rows into one.
inline TPbLayout PbLayout(int aW, int aH, TPbBarPos aPos, int aRowH, int aBarsVisible)
	{
	TPbLayout l;
	const int landscape = aW > aH;
	const int rows = landscape ? 1 : 2;
	const int barH = aBarsVisible ? rows * aRowH : 0;
	const int prog = 3;
	TPbRect none = { 0, 0, 0, 0 };
	l.iActions = none;
	l.iActionCount = 5;
	if (aPos == EPbBarTop)
		{
		TPbRect addr = { 0, 0, aW, aBarsVisible ? aRowH : 0 };
		l.iAddress = addr;
		if (!landscape && aBarsVisible)
			{
			TPbRect act = { 0, aRowH, aW, aRowH };      // top layout, portrait: address row first, actions under it
			l.iActions = act;
			}
		TPbRect pr = { 0, barH, aW, prog };
		l.iProgress = pr;
		TPbRect pg = { 0, barH, aW, aH - barH };
		l.iPage = pg;
		}
	else
		{
		const int top = aH - barH;
		TPbRect pg = { 0, 0, aW, top };
		l.iPage = pg;
		TPbRect pr = { 0, top - prog, aW, prog };
		l.iProgress = pr;
		TPbRect addr = { 0, top, aW, aBarsVisible ? aRowH : 0 };
		l.iAddress = addr;
		if (!landscape && aBarsVisible)
			{
			TPbRect act = { 0, top + aRowH, aW, aRowH };  // bottom layout: address row directly above the action row
			l.iActions = act;
			}
		}
	return l;
	}

#endif
