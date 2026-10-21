// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_theme.h -- the design tokens of the browser frontend, in ONE place: every colour the UI draws with. Plain 0xRRGGBB
// ints (no CONE types), so a host test or a later skin-derived theme can build one. Light, dark and the private-tab theme.
#ifndef PB_THEME_H
#define PB_THEME_H

struct TPbTokens
	{
	unsigned iBar;       // bar background
	unsigned iField;     // address field, cards, sheet rows' hover
	unsigned iInk;       // text and glyphs
	unsigned iDim;       // disabled glyphs, secondary text
	unsigned iAccent;    // progress line, selected, primary buttons
	unsigned iGood;      // secure connection
	unsigned iWarn;      // mixed / broken connection
	unsigned iPage;      // new-tab page, grid background
	unsigned iScrim;     // behind a sheet
	};

enum TPbThemeSetting { EPbThemeFollow = 0, EPbThemeLight = 1, EPbThemeDark = 2 };

inline TPbTokens PbTokens(int aDark, int aPrivate)
	{
	static const TPbTokens KLight = { 0xF4F5F7, 0xE3E6EB, 0x202226, 0xA0A4AA, 0x3D7BF0, 0x1E8E3E, 0xD93A25, 0xFFFFFF, 0x000000 };
	static const TPbTokens KDark = { 0x24262B, 0x3A3D44, 0xECEEF1, 0x7A7F88, 0x7AA7FF, 0x5BC47A, 0xFF7A66, 0x17181B, 0x000000 };
	static const TPbTokens KPrivate = { 0x2B2140, 0x3F3260, 0xECE6FA, 0x8F84AD, 0xB79CFF, 0x5BC47A, 0xFF7A66, 0x1E1730, 0x000000 };
	return aPrivate ? KPrivate : (aDark ? KDark : KLight);
	}

#endif
