/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phkbd_pages.h -- OURS (Phorepoc): the key pages of phkbd as data (rows of keys with widths in tenths of a key unit).
// A page is rows; a row's keys are laid out left to right from the row's indent, scaled so a 100-tenth row fills the
// board width. Adding a language = another table of the same shape (the planned XML -> table generator emits these).
#ifndef PHKBD_PAGES_H
#define PHKBD_PAGES_H

#include <e32std.h>

enum TPhKeyKind
    {
    EKChar,         // a character key; iCode is the lower-case (or only) character
    EKShift,
    EKBackspace,
    EKSymbols,      // "?123": to the symbol page
    EKLetters,      // "ABC": back to the letters page
    EKMore,         // "1/2" <-> "2/2" between the two symbol pages
    EKGlobe,        // switch keyboard
    EKSpace,
    EKEnter
    };

struct TPhKeyDef
    {
    TUint16 iCode;
    TUint8 iKind;
    TUint8 iWidth;          // tenths of a key unit (10 = one ordinary key)
    };

struct TPhRowDef
    {
    const TPhKeyDef* iKeys;
    TInt iCount;
    TInt iIndent;           // tenths of a key unit of empty space before the first key
    };

struct TPhPageDef
    {
    const TPhRowDef* iRows;
    TInt iRowCount;
    };

#define K( c ) { c, EKChar, 10 }
#define KF( kind, w ) { 0, kind, w }

// page 0: letters (English QWERTY)
static const TPhKeyDef KLetRow1[] = { K('q'),K('w'),K('e'),K('r'),K('t'),K('y'),K('u'),K('i'),K('o'),K('p') };
static const TPhKeyDef KLetRow2[] = { K('a'),K('s'),K('d'),K('f'),K('g'),K('h'),K('j'),K('k'),K('l') };
static const TPhKeyDef KLetRow3[] = { KF(EKShift,14),K('z'),K('x'),K('c'),K('v'),K('b'),K('n'),K('m'),KF(EKBackspace,16) };
static const TPhKeyDef KLetRow4[] = { KF(EKSymbols,15),KF(EKGlobe,10),K(','),KF(EKSpace,40),K('.'),KF(EKEnter,15) };
static const TPhRowDef KLetRows[] =
    { { KLetRow1, 10, 0 }, { KLetRow2, 9, 5 }, { KLetRow3, 9, 0 }, { KLetRow4, 6, 0 } };

// page 1: digits and common symbols
static const TPhKeyDef KSymRow1[] = { K('1'),K('2'),K('3'),K('4'),K('5'),K('6'),K('7'),K('8'),K('9'),K('0') };
static const TPhKeyDef KSymRow2[] = { K('@'),K('#'),K('$'),K('%'),K('&'),K('-'),K('+'),K('('),K(')'),K('/') };
static const TPhKeyDef KSymRow3[] = { KF(EKMore,14),K('*'),K('"'),K('\''),K(':'),K(';'),K('!'),K('?'),KF(EKBackspace,16) };
static const TPhKeyDef KSymRow4[] = { KF(EKLetters,15),KF(EKGlobe,10),K(','),KF(EKSpace,40),K('.'),KF(EKEnter,15) };
static const TPhRowDef KSymRows[] =
    { { KSymRow1, 10, 0 }, { KSymRow2, 10, 0 }, { KSymRow3, 9, 0 }, { KSymRow4, 6, 0 } };

// page 2: more symbols
static const TPhKeyDef KSy2Row1[] = { K('~'),K('`'),K('|'),K('^'),K('='),K('{'),K('}'),K('\\'),K('['),K(']') };
static const TPhKeyDef KSy2Row2[] = { K(0x20AC),K(0xA3),K(0xA5),K(0xA2),K(0xB0),K(0xA9),K(0xAE),K(0x2122),K('<'),K('>') };
static const TPhKeyDef KSy2Row3[] = { KF(EKMore,14),K('_'),K(0xA1),K(0xBF),K(0xA7),K(0xB6),K(0xD7),K(0xF7),KF(EKBackspace,16) };
static const TPhRowDef KSy2Rows[] =
    { { KSy2Row1, 10, 0 }, { KSy2Row2, 10, 0 }, { KSy2Row3, 9, 0 }, { KSymRow4, 6, 0 } };

static const TPhPageDef KPhPages[] =
    { { KLetRows, 4 }, { KSymRows, 4 }, { KSy2Rows, 4 } };
static const TInt KPhPageCount = 3;

// long-press alternatives (iList is zero-terminated). Lower-case bases; shift upper-cases the choices.
struct TPhAccent
    {
    TUint16 iBase;
    const TUint16* iList;
    };

static const TUint16 KAccA[] = { 0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0 };
static const TUint16 KAccE[] = { 0xE8,0xE9,0xEA,0xEB,0x113,0x119,0 };
static const TUint16 KAccI[] = { 0xEC,0xED,0xEE,0xEF,0x12B,0 };
static const TUint16 KAccO[] = { 0xF2,0xF3,0xF4,0xF5,0xF6,0xF8,0x153,0 };
static const TUint16 KAccU[] = { 0xF9,0xFA,0xFB,0xFC,0x16B,0 };
static const TUint16 KAccC[] = { 0xE7,0x107,0x10D,0 };
static const TUint16 KAccN[] = { 0xF1,0x144,0 };
static const TUint16 KAccS[] = { 0xDF,0x15B,0x161,0 };
static const TUint16 KAccY[] = { 0xFD,0xFF,0 };
static const TUint16 KAccZ[] = { 0x17E,0x17A,0x17C,0 };
static const TUint16 KAccL[] = { 0x142,0 };
static const TUint16 KAccD[] = { 0xF0,0 };
static const TUint16 KAccT[] = { 0xFE,0 };
static const TUint16 KAccDot[] = { ',','?','!','\'','"',':',';',0 };
static const TUint16 KAccDash[] = { 0x2013,0x2014,'_',0 };
static const TUint16 KAcc0[] = { 0xB0,0x2205,0 };
static const TUint16 KAcc1[] = { 0xB9,0xBD,0x2153,0 };
static const TUint16 KAcc2[] = { 0xB2,0xBD,0 };
static const TUint16 KAcc3[] = { 0xB3,0xBE,0 };
static const TUint16 KAccDollar[] = { 0x20AC,0xA3,0xA5,0xA2,0 };
static const TUint16 KAccQuote[] = { 0x2018,0x2019,0x201C,0x201D,0xAB,0xBB,0 };

static const TPhAccent KPhAccents[] =
    {
    { 'a', KAccA }, { 'e', KAccE }, { 'i', KAccI }, { 'o', KAccO }, { 'u', KAccU }, { 'c', KAccC }, { 'n', KAccN },
    { 's', KAccS }, { 'y', KAccY }, { 'z', KAccZ }, { 'l', KAccL }, { 'd', KAccD }, { 't', KAccT },
    { '.', KAccDot }, { '-', KAccDash }, { '0', KAcc0 }, { '1', KAcc1 }, { '2', KAcc2 }, { '3', KAcc3 },
    { '$', KAccDollar }, { '"', KAccQuote }
    };
static const TInt KPhAccentCount = sizeof( KPhAccents ) / sizeof( KPhAccents[0] );

#undef K
#undef KF

#endif
