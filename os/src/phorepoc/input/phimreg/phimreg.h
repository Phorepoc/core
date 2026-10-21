/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// phimreg.h -- OURS (Phorepoc): the input-method registry.
//
// An Android-like "input method framework" on top of the 2010 pen input stack: several keyboards are installed side
// by side, one is active, the user switches between them (the layout's switch key) and chooses the default (Settings).
// A keyboard registers itself as an ECom implementation of PHIM_ENTRY_INTERFACE (resource: see phkbd.rss); the stock
// Nokia keyboards need no resource of their own -- built-in entries stand in for them.
//
// The state (the active keyboard, the user's customisation) lives in our own CenRep repository (KPhImRepository), so it
// survives reboots and is shared by the three processes that need it: the FEP side (the IME plug-in shim that picks a
// layout), the pen input server (the layout itself), and the Settings app.
#ifndef PHIMREG_H
#define PHIMREG_H

#include <e32std.h>
#include <e32base.h>

const TUint32 KPhImRepository         = 0xE7015830;   // CenRep repository (tools/kernel/cenrep/E7015830.txt)
const TUint32 KPhImKeyActive          = 1;            // entry UID of the active keyboard (0 = none chosen: priority order)
const TUint32 KPhImKeyKeyHeight       = 2;            // phkbd key-height scale, percent (60..140, default 100)
const TUint32 KPhImKeyTheme           = 3;            // phkbd colour scheme (0 dark, 1 light, 2 blue)
const TUint32 KPhImKeyPopups          = 4;            // phkbd key-press preview popup (1 on, 0 off)
const TUint32 KPhImKeyLanguage        = 5;            // phkbd active language (index into its language table)
const TUint32 KPhImKeyOverlay         = 6;            // phkbd: 1 = keyboard panel over the app (default), 0 = full screen with its own text box

const TInt32  PHIM_ENTRY_INTERFACE    = 0xE7015801;   // ECom interface UID of a registry entry
const TInt    KPhImNameMax            = 40;

// the pen-input modes the entries are for (the FEP's TPluginInputMode values)
const TInt    KPhImModeItut           = 0x0008;       // EPluginInputModeItut
const TInt    KPhImModePortraitQwerty = 0x0080;       // EPluginInputModePortraitFSQ

// built-in entries standing in for the stock keyboards
const TUint32 KPhImEntryStockItut     = 0xE7015812;
const TUint32 KPhImEntryStockQwerty   = 0xE7015811;
const TUint32 KPhImStockQwertyLayout  = 0x20026837;   // peninputgenericvkb's portrait QWERTY layout implementation

class TPhImEntry
    {
public:
    TUint32 iEntryUid;     // the registry entry's own UID (what is persisted)
    TInt    iMode;         // pen-input mode it serves (KPhImMode*)
    TUint32 iLayoutUid;    // pen server layout implementation UID (0 = the mode's stock layout, picked by the FEP side)
    TInt    iPriority;     // lower first: cycling order, and the pick when nothing is chosen
    TBuf<KPhImNameMax> iName;
    };

// All C linkage (no mangled names to keep in the .def). Every function returns a Symbian error code.
extern "C"
    {
    // the installed keyboards, ordered by priority
    IMPORT_C TInt PhImList( RArray<TPhImEntry>& aOut );
    // the active keyboard (KErrNotFound = none chosen yet)
    IMPORT_C TInt PhImActive( TUint32& aEntryUid );
    // make aEntryUid the active (and default) keyboard; persisted
    IMPORT_C TInt PhImSetActive( TUint32 aEntryUid );
    // the pen server layout UID to open for aMode: the active entry if it serves aMode, else the first entry that does
    // (which then becomes active). KErrNotFound = no layout of ours for that mode, the caller keeps its stock choice.
    IMPORT_C TInt PhImResolve( TInt aMode, TUint32& aLayoutUid );
    // the next keyboard after the active one (cyclic); it becomes active. aEntry = it.
    IMPORT_C TInt PhImNext( TPhImEntry& aEntry );
    // the mode a registered layout implementation serves (KErrNotFound = not registered)
    IMPORT_C TInt PhImModeOfLayout( TUint32 aLayoutUid, TInt& aMode );
    // integer settings (KPhImKey*); Get returns aDefault when the key is unset
    IMPORT_C TInt PhImGetSetting( TUint32 aKey, TInt aDefault, TInt& aValue );
    IMPORT_C TInt PhImSetSetting( TUint32 aKey, TInt aValue );
    }

#endif
