// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepowerkeypluginimplementationtable.cpp -- OURS: the ECOM proxy table (after GSAutoKeyguardPluginImplementationTable)
#include <e32std.h>
#include <ecom/implementationproxy.h>

#include "phepowerkeyplugin.h"

const TImplementationProxy KPhePowerKeyPluginImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY( 0xE70157B2, CPhePowerKeyPlugin::NewL )
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy( TInt& aTableCount )
    {
    aTableCount = sizeof( KPhePowerKeyPluginImplementationTable ) / sizeof( TImplementationProxy );
    return KPhePowerKeyPluginImplementationTable;
    }
