/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phkbd_lang.h -- OURS (Phorepoc): the keyboard languages of phkbd. A language is DATA: its pages (rows of keys), its
// long-press accents, a name. One header per language (phkbd_lang_en.h ...), each defining one TPhLanguage; adding a
// language = adding that header and one line to KPhLanguages in phkbd_langs.h. The globe key cycles the installed languages.
#ifndef PHKBD_LANG_H
#define PHKBD_LANG_H

#include <e32std.h>

enum TPhKeyKind
    {
    EKChar,         // a character key; iCode is the lower-case (or only) character
    EKShift,
    EKBackspace,
    EKSymbols,      // "?123": to the symbol page
    EKLetters,      // "ABC": back to the letters page
    EKMore,         // "1/2" <-> "2/2" between the two symbol pages
    EKGlobe,        // next language (or, with one language, the next keyboard)
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

// long-press alternatives (iList is zero-terminated). Lower-case bases; shift upper-cases the choices.
struct TPhAccent
    {
    TUint16 iBase;
    const TUint16* iList;
    };

struct TPhLanguage
    {
    const char* iCode;              // "en"
    const char* iName;              // "English" (shown on the space bar)
    const TPhPageDef* iPages;
    TInt iPageCount;
    const TPhAccent* iAccents;
    TInt iAccentCount;
    };

// a language name (UTF-8, 1- and 2-byte sequences) as text for display
inline void PhLangNameToDes( const char* aName, TDes& aOut )
    {
    aOut.Zero();
    for ( const TUint8* p = reinterpret_cast<const TUint8*>( aName ); *p && aOut.Length() < aOut.MaxLength(); ++p )
        {
        if ( *p >= 0xC0 && p[1] )
            {
            aOut.Append( TChar( ( ( *p & 0x1F ) << 6 ) | ( p[1] & 0x3F ) ) );
            ++p;
            }
        else
            {
            aOut.Append( TChar( *p ) );
            }
        }
    }

#define PH_K( c ) { c, EKChar, 10 }
#define PH_KF( kind, w ) { 0, kind, w }

#endif
