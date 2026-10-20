// rcse_null.cpp -- OURS (not Symbian source): a null VoIP profile registry (RCSE).
//
// The real rcse.dll keeps the VoIP/SIP profiles a phone's internet-telephony settings create; it was
// never released. This board has no VoIP service, so the honest answer to every lookup is "no
// profiles": Phonebook2 and the CMS server then report no SIP VoIP support for any contact.
// Header: toolchain/shims/unreleased_inc/crcseprofileregistry.h.
#include <crcseprofileregistry.h>

CRCSEProfileEntry::CRCSEProfileEntry() {}
EXPORT_C CRCSEProfileEntry::~CRCSEProfileEntry() {}
EXPORT_C CRCSEProfileEntry* CRCSEProfileEntry::NewL()
    {
    return new (ELeave) CRCSEProfileEntry;
    }

CRCSEProfileRegistry::CRCSEProfileRegistry() {}
EXPORT_C CRCSEProfileRegistry::~CRCSEProfileRegistry() {}

EXPORT_C CRCSEProfileRegistry* CRCSEProfileRegistry::NewLC()
    {
    CRCSEProfileRegistry* self = new (ELeave) CRCSEProfileRegistry;
    CleanupStack::PushL( self );
    return self;
    }

EXPORT_C CRCSEProfileRegistry* CRCSEProfileRegistry::NewL()
    {
    CRCSEProfileRegistry* self = NewLC();
    CleanupStack::Pop( self );
    return self;
    }

EXPORT_C void CRCSEProfileRegistry::FindByServiceIdL( TUint32 /*aServiceId*/,
    RPointerArray<CRCSEProfileEntry>& /*aFoundEntries*/ )
    {
    }
