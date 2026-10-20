/*
 * CurlSSLHandleSymbian.cpp -- OURS (Phorepoc, browser-plan.md section 9): the platform half of CurlSSLHandle (the PlayStation
 * port takes its roots from a system certificate store). Ours is the PEM bundle the ROM already carries for SSL.DLL and
 * NetSurf (emitter/components.py `ssl`: \resource\ssl\ca-bundle.crt on Z:). Cipher/curve lists stay unset: curl's mbedTLS
 * backend then uses mbedTLS's defaults (TLS 1.3 + 1.2 AEAD suites, phorepoc_mbedtls_config.h).
 */

#include "config.h"
#include "CurlSSLHandle.h"

namespace WebCore {

void CurlSSLHandle::platformInitialize()
{
    setCACertPath("z:/resource/ssl/ca-bundle.crt"_s);
}

} // namespace WebCore
