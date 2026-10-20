// crcseprofileentry.h -- OURS (not Symbian source): the VoIP profile entry of the unreleased RCSE
// registry, reduced to what the release's clients need (a complete type to ResetAndDestroy).
// See crcseprofileregistry.h.
#ifndef CRCSEPROFILEENTRY_H
#define CRCSEPROFILEENTRY_H

#include <e32base.h>

class CRCSEProfileEntry : public CBase
    {
public:
    IMPORT_C static CRCSEProfileEntry* NewL();
    IMPORT_C ~CRCSEProfileEntry();

private:
    CRCSEProfileEntry();
    };

#endif
