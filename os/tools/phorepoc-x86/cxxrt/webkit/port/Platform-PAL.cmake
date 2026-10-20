# PlatformSymbian.cmake (PAL) -- OURS (Phorepoc, browser-plan.md section 9): PAL's generic sources + the digest on OpenSSL 3
# (upstream crypto/openssl/CryptoDigestOpenSSL.cpp; the mbedTLS one, webkit/webcore/CryptoDigestMbedTLS.cpp, is the unused alternative).
list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    text/KillRing.cpp
)

# OpenSSL 3 headers as plain -I: they must beat Open C's stdapis OpenSSL 0.9.8 on the compiler wrapper line (Platform-WebCore.cmake)
list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES "${PH_DEPS}/include")
list(APPEND PAL_LIBRARIES OpenSSL::Crypto)
