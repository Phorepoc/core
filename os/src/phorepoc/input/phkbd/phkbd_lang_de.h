/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phkbd_lang_de.h -- OURS (Phorepoc): the German (QWERTZ) keyboard language of phkbd, as data (see phkbd_lang.h).
// The symbol pages are English's; the umlauts and the sharp s are on the long press of a, o, u and s.
#ifndef PHKBD_LANG_DE_H
#define PHKBD_LANG_DE_H

#include "phkbd_lang_en.h"

namespace PhLangDe
{
#define K PH_K
#define KF PH_KF

static const TPhKeyDef KRow1[] = { K('q'),K('w'),K('e'),K('r'),K('t'),K('z'),K('u'),K('i'),K('o'),K('p') };
static const TPhKeyDef KRow3[] = { KF(EKShift,14),K('y'),K('x'),K('c'),K('v'),K('b'),K('n'),K('m'),KF(EKBackspace,16) };
static const TPhRowDef KLetRows[] =
    { { KRow1, 10, 0 }, { PhLangEn::KLetRow2, 9, 5 }, { KRow3, 9, 0 }, { PhLangEn::KLetRow4, 6, 0 } };

static const TUint16 KA[] = { 0xE4,0xE0,0xE1,0xE2,0 };
static const TUint16 KO[] = { 0xF6,0xF2,0xF3,0xF4,0 };
static const TUint16 KU[] = { 0xFC,0xF9,0xFA,0xFB,0 };
static const TUint16 KS[] = { 0xDF,0x15B,0x161,0 };
static const TUint16 KE[] = { 0xE9,0xE8,0xEA,0xEB,0 };
static const TUint16 KQuote[] = { 0x201E,0x201C,0x201A,0x2018,0xBB,0xAB,0 };

static const TPhAccent KAccents[] =
    {
    { 'a', KA }, { 'o', KO }, { 'u', KU }, { 's', KS }, { 'e', KE }, { 'i', PhLangEn::KAccI }, { 'c', PhLangEn::KAccC },
    { 'n', PhLangEn::KAccN }, { '"', KQuote },
    { '.', PhLangEn::KAccDot }, { '-', PhLangEn::KAccDash }, { '0', PhLangEn::KAcc0 }, { '1', PhLangEn::KAcc1 },
    { '2', PhLangEn::KAcc2 }, { '3', PhLangEn::KAcc3 }, { '$', PhLangEn::KAccDollar }
    };
static const TInt KAccentCount = sizeof( KAccents ) / sizeof( KAccents[0] );

#undef K
#undef KF
}

static const TPhPageDef KPhLangDePages[] =
    { { PhLangDe::KLetRows, 4 }, { PhLangEn::KSymRows, 4 }, { PhLangEn::KSy2Rows, 4 } };

static const TPhLanguage KPhLangDe =
    { "de", "Deutsch", KPhLangDePages, 3, PhLangDe::KAccents, PhLangDe::KAccentCount };

#endif
