// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// commondsy_null.cpp -- OURS (not Symbian source): a null DSY (Domestic OS licensee plug-in) with the
// REAL commondsyU.DEF ordinal table.
//
// The DOS server (dosserver.exe) RLibrary::Loads the DSY named by z:\private\101f6efa\dosserver.ini at
// start-up and cannot start without one. The release's reference DSY, CommonDSY, does not compile: its
// service-logic headers (CDSYSysUtilsDOSServerPlugin.h, the CDSY message ids) were never published.
// A DSY's only contract with the server is ordinal 1 (LibEntryL -> a CDosFactoryBase), whose single
// service is the licensee EXTENSION subsession. This board has no licensee device services, so that one
// leaves KErrNotSupported. Everything else the DOS server serves -- shared data / disk-space reservation
// (Virtual Phonebook's RSharedDataClient), event send/receive -- is the server's own and unaffected.
// Ordinals 2-16 (the CommonDSY controller + message classes) stay loud unimpl stubs.
#include <e32base.h>
#include <dosfactorybase.h>
#include <dosextensionbase.h>

class CNullDsyFactory : public CDosFactoryBase
    {
public:
    CDosExtensionBase* NewExtensionServiceL()
        {
        User::Leave( KErrNotSupported );
        return NULL;
        }
    };

EXPORT_C CDosFactoryBase* LibEntryL()
    {
    return new (ELeave) CNullDsyFactory;
    }
