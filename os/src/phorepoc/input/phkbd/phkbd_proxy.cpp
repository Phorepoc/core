// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phkbd_proxy.cpp -- OURS (Phorepoc): the ECom entry points of phkbd.dll
#include <e32base.h>
#include <implementationproxy.h>
#include "phkbd_layout.h"
#include "phimreg.h"

// the registry entry (PHIM_ENTRY_INTERFACE) is data only -- opaque_data in phkbd.rss; it is listed, never instantiated
static TAny* NewEntryL()
    {
    return new ( ELeave ) CBase;
    }

const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY( 0xE7015803, CPhKbdLayoutImp::NewL ),
    IMPLEMENTATION_PROXY_ENTRY( 0xE7015810, NewEntryL )
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy( TInt& aTableCount )
    {
    aTableCount = sizeof( ImplementationTable ) / sizeof( TImplementationProxy );
    return ImplementationTable;
    }
