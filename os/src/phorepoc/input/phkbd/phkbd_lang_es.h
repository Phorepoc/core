/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phkbd_lang_es.h -- OURS (Phorepoc): the Spanish (QWERTY with n-tilde) keyboard language of phkbd, as data (see phkbd_lang.h).
#ifndef PHKBD_LANG_ES_H
#define PHKBD_LANG_ES_H

#include "phkbd_lang_en.h"

namespace PhLangEs
{
#define K PH_K
#define KF PH_KF

static const TPhKeyDef KRow2[] = { K('a'),K('s'),K('d'),K('f'),K('g'),K('h'),K('j'),K('k'),K('l'),K(0xF1) };
static const TPhRowDef KLetRows[] =
    { { PhLangEn::KLetRow1, 10, 0 }, { KRow2, 10, 0 }, { PhLangEn::KLetRow3, 9, 0 }, { PhLangEn::KLetRow4, 6, 0 } };

static const TUint16 KA[] = { 0xE1,0xE0,0xE4,0xE2,0 };
static const TUint16 KE[] = { 0xE9,0xE8,0xEB,0xEA,0 };
static const TUint16 KI[] = { 0xED,0xEC,0xEF,0xEE,0 };
static const TUint16 KO[] = { 0xF3,0xF2,0xF6,0xF4,0 };
static const TUint16 KU[] = { 0xFA,0xFC,0xF9,0xFB,0 };
static const TUint16 KN[] = { 0xF1,0x144,0 };
static const TUint16 KQm[] = { 0xBF,0xA1,0 };
static const TUint16 KQuote[] = { 0xAB,0xBB,0x201C,0x201D,0x2018,0x2019,0 };

static const TPhAccent KAccents[] =
    {
    { 'a', KA }, { 'e', KE }, { 'i', KI }, { 'o', KO }, { 'u', KU }, { 0xF1, KN }, { 'c', PhLangEn::KAccC },
    { '?', KQm }, { '"', KQuote },
    { '.', PhLangEn::KAccDot }, { '-', PhLangEn::KAccDash }, { '0', PhLangEn::KAcc0 }, { '1', PhLangEn::KAcc1 },
    { '2', PhLangEn::KAcc2 }, { '3', PhLangEn::KAcc3 }, { '$', PhLangEn::KAccDollar }
    };
static const TInt KAccentCount = sizeof( KAccents ) / sizeof( KAccents[0] );

#undef K
#undef KF
}

static const TPhPageDef KPhLangEsPages[] =
    { { PhLangEs::KLetRows, 4 }, { PhLangEn::KSymRows, 4 }, { PhLangEn::KSy2Rows, 4 } };

static const TPhLanguage KPhLangEs =
    { "es", "Espa\xC3\xB1" "ol", KPhLangEsPages, 3, PhLangEs::KAccents, PhLangEs::KAccentCount };

#endif
