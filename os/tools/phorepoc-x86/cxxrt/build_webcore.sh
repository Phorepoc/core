#!/usr/bin/env bash
# build_webcore.sh [ninja target] -- P1 page layer: WebKit's `Symbian` port (OURS:
# webkit/port/) = JSC as in build_jsc.sh + PAL + WebCore (curl/mbedTLS, Skia CPU, HarfBuzz/FreeType, ICU) for x32 over Open C.
# IN-CONTAINER (phorepoc-jsc):  host: JOBS=4 bash tools/phorepoc-x86/cxxrt/drun2.sh /repo/tools/phorepoc-x86/cxxrt/build_webcore.sh [target]
# (log to a FILE, never pipe into head). Default target: WebCore. CONFIGURE_ONLY=1 stops after cmake. RECONF=1 re-runs cmake.
# Prereqs: build_jsc.sh once (toolchain file $TC, pure wrappers), build_icu.sh, build_webdeps.sh, the NetSurf port's
# curl/png/jpeg/mbedtls (.phorepoc-x86-build/netsurf), the sparse checkout /wv/wk-git (apply_patches.sh copies the curl backend).
# Build hygiene (browser-plan.md section 8): JOBS defaults to 4 (8 GB docker VM); patch scripts write only changed files.
set -u
source /repo/tools/phorepoc-x86/cxxrt/arch.sh
W=${W:-/wk/lsc}; V=/wv; SRC=$V/wpewebkit-2.54.1; BLD=$V/build-wc; OUT=${OUT:-/out/cxxrt}; D=/repo/tools/phorepoc-x86/cxxrt
[ -f $TC ] || { echo "no $TC: run build_jsc.sh first"; exit 1; }
bash $D/mk_pure_wrappers.sh >/dev/null
bash $D/webkit/apply_patches.sh $SRC $W || exit 1
if [ ! -f $BLD/build.ninja ] || [ -n "${RECONF:-}" ]; then
  mkdir -p $BLD; cd $BLD
  cmake -G Ninja $SRC -DPORT=Symbian -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_BUILD_TYPE=Release \
    -DDEVELOPER_MODE=OFF -DENABLE_API_TESTS=OFF -DPHOREPOC=1 \
    -DPH_DEPS=$DEPS -DPH_NETSURF=$NSO -DPH_MBEDTLS_SRC=/repo/shim/build/_deps/mbedtls-src -DPH_MBEDTLS_CFG=/repo/tools/phorepoc-x86/mbedtls \
    -DICU_INCLUDE_DIR=$ICU/include -DICU_DATA_LIBRARY_RELEASE=$ICU/lib/libicudata.a -DICU_I18N_LIBRARY_RELEASE=$ICU/lib/libicui18n.a -DICU_UC_LIBRARY_RELEASE=$ICU/lib/libicuuc.a \
    -DCMAKE_EXE_LINKER_FLAGS="-nostdlib -nostartfiles -static -Wl,--unresolved-symbols=ignore-all -Wl,--allow-multiple-definition -Wl,-e,main -Wl,--defsym,__dso_handle=0" \
    > $OUT/wc-configure.log 2>&1; rc=$?; echo "configure exit $rc"; grep -E "CMake Error|error" -A3 $OUT/wc-configure.log | head -40
  [ $rc = 0 ] || exit 1
fi
[ -n "${CONFIGURE_ONLY:-}" ] && exit 0
T=${1:-WebCore}
cd $BLD && ninja -k 0 -j${JOBS:-4} $T > $OUT/wc-build-$T.log 2>&1; rc=$?; echo "ninja exit $rc"
echo "$(grep -c "error:" $OUT/wc-build-$T.log) errors"
exit $rc   # ninja's status (so `build_webcore.sh && package_web.sh` chains)
