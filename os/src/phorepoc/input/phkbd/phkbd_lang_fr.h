/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phkbd_lang_fr.h -- OURS (Phorepoc): the French (AZERTY) keyboard language of phkbd, as data (see phkbd_lang.h).
#ifndef PHKBD_LANG_FR_H
#define PHKBD_LANG_FR_H

#include "phkbd_lang_en.h"

namespace PhLangFr
{
#define K PH_K
#define KF PH_KF

static const TPhKeyDef KRow1[] = { K('a'),K('z'),K('e'),K('r'),K('t'),K('y'),K('u'),K('i'),K('o'),K('p') };
static const TPhKeyDef KRow2[] = { K('q'),K('s'),K('d'),K('f'),K('g'),K('h'),K('j'),K('k'),K('l'),K('m') };
static const TPhKeyDef KRow3[] = { KF(EKShift,14),K('w'),K('x'),K('c'),K('v'),K('b'),K('n'),K('\''),KF(EKBackspace,16) };
static const TPhRowDef KLetRows[] =
    { { KRow1, 10, 0 }, { KRow2, 10, 0 }, { KRow3, 9, 0 }, { PhLangEn::KLetRow4, 6, 0 } };

static const TUint16 KA[] = { 0xE0,0xE2,0xE6,0xE1,0xE4,0 };
static const TUint16 KE[] = { 0xE9,0xE8,0xEA,0xEB,0x153,0 };
static const TUint16 KI[] = { 0xEE,0xEF,0xEC,0xED,0 };
static const TUint16 KO[] = { 0xF4,0x153,0xF2,0xF3,0xF6,0 };
static const TUint16 KU[] = { 0xF9,0xFB,0xFC,0xFA,0 };
static const TUint16 KC[] = { 0xE7,0x107,0x10D,0 };
static const TUint16 KY[] = { 0xFF,0xFD,0 };
static const TUint16 KQuote[] = { 0xAB,0xBB,0x201C,0x201D,0x2018,0x2019,0 };

static const TPhAccent KAccents[] =
    {
    { 'a', KA }, { 'e', KE }, { 'i', KI }, { 'o', KO }, { 'u', KU }, { 'c', KC }, { 'y', KY }, { '"', KQuote },
    { '.', PhLangEn::KAccDot }, { '-', PhLangEn::KAccDash }, { '0', PhLangEn::KAcc0 }, { '1', PhLangEn::KAcc1 },
    { '2', PhLangEn::KAcc2 }, { '3', PhLangEn::KAcc3 }, { '$', PhLangEn::KAccDollar }
    };
static const TInt KAccentCount = sizeof( KAccents ) / sizeof( KAccents[0] );

#undef K
#undef KF
}

static const TPhPageDef KPhLangFrPages[] =
    { { PhLangFr::KLetRows, 4 }, { PhLangEn::KSymRows, 4 }, { PhLangEn::KSy2Rows, 4 } };

static const TPhLanguage KPhLangFr =
    { "fr", "Fran\xC3\xA7" "ais", KPhLangFrPages, 3, PhLangFr::KAccents, PhLangFr::KAccentCount };

#endif
