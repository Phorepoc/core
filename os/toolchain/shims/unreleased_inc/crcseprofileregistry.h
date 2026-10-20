/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// crcseprofileregistry.h -- OURS (not Symbian source): the part of the S60 VoIP "RCSE" profile
// registry API that the release's clients use (rcse.dll, UID3 0x10202869; the real component and its API are in
// sf.app.iptelephony: richcallsettingsengine/rcse2, iptel_plat/richcallsettingsengine_api). Clients: Phonebook2's field analyzer (SIP VoIP
// support), the CMS server and the contact action service -- all only ask "which VoIP profiles
// belong to service id N?" (FindByServiceIdL) and ResetAndDestroy the answer.
// Our provider (toolchain/shims/rcse_null) has no VoIP profiles, so every lookup is empty.
// Searched LAST (components.py UNRELEASED_INC).
#ifndef CRCSEPROFILEREGISTRY_H
#define CRCSEPROFILEREGISTRY_H

#include <e32base.h>
#include <crcseprofileentry.h>

class CRCSEProfileRegistry : public CBase
    {
public:
    IMPORT_C static CRCSEProfileRegistry* NewL();
    IMPORT_C static CRCSEProfileRegistry* NewLC();
    IMPORT_C ~CRCSEProfileRegistry();

    // appends the profiles of VoIP service aServiceId (spsettings id) to aFoundEntries
    IMPORT_C void FindByServiceIdL( TUint32 aServiceId,
        RPointerArray<CRCSEProfileEntry>& aFoundEntries );

private:
    CRCSEProfileRegistry();
    };

#endif
