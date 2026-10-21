// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_layout.h -- the pure layout of the browser frontend: where the page and the bar go for a given screen size and bar
// position. ONE row, in portrait and landscape: [nav button] [address field] [overflow button]. The navigation buttons
// (back, forward, reload/stop, tabs) live in a popover that opens from the nav button (see NavRect). No CONE types
// (plain ints), so it can be unit-tested on the host.
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
	TPbRect iAddress;   // the bar row: nav button, field, overflow button
	TPbRect iProgress;  // the progress line (3 px), between the bar and the page
	};

// aRowH = 56 in portrait, 48 in landscape (the caller chooses).
inline TPbLayout PbLayout(int aW, int aH, TPbBarPos aPos, int aRowH, int aBarsVisible)
	{
	TPbLayout l;
	const int barH = aBarsVisible ? aRowH : 0;
	const int prog = 3;
	if (aPos == EPbBarTop)
		{
		TPbRect addr = { 0, 0, aW, barH };
		l.iAddress = addr;
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
		TPbRect addr = { 0, top, aW, barH };
		l.iAddress = addr;
		}
	return l;
	}

#endif
