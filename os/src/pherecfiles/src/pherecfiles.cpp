// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pherecfiles.cpp -- OURS: a data recogniser by file name extension (after RECTXT). See pherecfiles.mmp.
#include <apmrec.h>
#include <apmstd.h>
#include <ecom/implementationproxy.h>

const TUid KUidPheRecFiles = { 0xE7015C01 };
const TInt KPheRecFilesBuf = 64;

_LIT8(KTypeVCard, "text/X-vCard");
_LIT8(KTypeVCal, "text/x-vCalendar");
_LIT8(KTypeCert, "application/x-x509-ca-cert");
_LIT8(KTypePkcs12, "application/x-pkcs12");

_LIT(KExtVcf, ".vcf");
_LIT(KExtVcs, ".vcs");
_LIT(KExtCer, ".cer");
_LIT(KExtCrt, ".crt");
_LIT(KExtDer, ".der");
_LIT(KExtP12, ".p12");
_LIT(KExtPfx, ".pfx");

const TInt KPheRecFilesTypes = 4;

class CPheRecFiles : public CApaDataRecognizerType
    {
public:
    CPheRecFiles() : CApaDataRecognizerType(KUidPheRecFiles, CApaDataRecognizerType::ENormal)
        {
        iCountDataTypes = KPheRecFilesTypes;
        }
    TUint PreferredBufSize()
        {
        return KPheRecFilesBuf;
        }
    TDataType SupportedDataTypeL(TInt aIndex) const
        {
        switch (aIndex)
            {
            case 0: return TDataType(KTypeVCard);
            case 1: return TDataType(KTypeVCal);
            case 2: return TDataType(KTypeCert);
            default: return TDataType(KTypePkcs12);
            }
        }
    static CApaDataRecognizerType* CreateRecognizerL()
        {
        return new (ELeave) CPheRecFiles();
        }
private:
    static TBool HasExt(const TDesC& aName, const TDesC& aExt)
        {
        return aName.Length() > aExt.Length() && aName.Right(aExt.Length()).CompareF(aExt) == 0;
        }
    void DoRecognizeL(const TDesC& aName, const TDesC8& /*aBuffer*/)
        {
        if (HasExt(aName, KExtVcf))
            {
            iDataType = TDataType(KTypeVCard);
            }
        else if (HasExt(aName, KExtVcs))
            {
            iDataType = TDataType(KTypeVCal);
            }
        else if (HasExt(aName, KExtCer) || HasExt(aName, KExtCrt) || HasExt(aName, KExtDer))
            {
            iDataType = TDataType(KTypeCert);
            }
        else if (HasExt(aName, KExtP12) || HasExt(aName, KExtPfx))
            {
            iDataType = TDataType(KTypePkcs12);
            }
        else
            {
            return;   // not ours: iConfidence stays ENotRecognized
            }
        iConfidence = ECertain;
        }
    };

const TImplementationProxy ImplementationTable[] =
    {
    IMPLEMENTATION_PROXY_ENTRY(0xE7015C02, CPheRecFiles::CreateRecognizerL)
    };

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
    {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
    }
