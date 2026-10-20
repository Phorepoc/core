#!/usr/bin/env bash
# build_web_arm_all.sh -- IN-CONTAINER via drun-arm.sh: the whole PinePhone web stack from clean, in order (libstdc++, wrappers, ICU,
# deps, OpenSSL-curl, phcompat, JSC, WebCore, phwebtest objects, package). Needed when the ARM compile line changes (wrapper edits do
# not retrigger make/ninja). JOBS=4 (WebCore's unified sources ~1.5 GB a compiler; 10 jobs ran the VM out of memory, 2026-10-14).
set -u
D=/repo/tools/phorepoc-x86/cxxrt; export ARCH=arm JOBS=${JOBS:-4}
s() { echo "== $1 $(date +%T)"; }
s libstdc++; rm -rf /wk/lsc/build; bash $D/build_libstdcxx.sh >/dev/null || exit 1
s wrappers; bash $D/mk_pure_wrappers.sh >/dev/null
s icu; rm -rf /wv/icu-arm; bash $D/build_icu.sh | tail -2
s deps; rm -rf /wv/deps-arm /wv/wd/b-* /wv/wd/b-sqlite /wv/wd/b-zlib; (cd /wv/wd/openssl-3.5.4 2>/dev/null && make clean >/dev/null 2>&1; rm -f Makefile)
bash $D/build_webdeps.sh | grep "^==\|FAILED" || true; bash $D/build_curl_ossl.sh | tail -1
s phcompat; bash $D/compat/build_phcompat.sh
s jsc; rm -rf /wv/build-jsc2; bash $D/build_jsc.sh JavaScriptCore
s webcore; rm -rf /wv/build-wc; bash $D/build_webcore.sh; bash $D/build_webcore.sh phwebtest
s package; bash $D/package_web.sh | tail -1
s done
