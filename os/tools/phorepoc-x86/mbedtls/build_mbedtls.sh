#!/usr/bin/env bash
# build_mbedtls.sh -- mbedTLS 3.6 for Path C (amd64, x32) on Open C: curl's TLS backend for NetSurf's https
# (docs/phorepoc/netsurf-port.md). Runs INSIDE phorepoc-x86, like build_curl.sh:
#
#   host:  R="$(pwd -W)"; MSYS_NO_PATHCONV=1 docker.exe run --rm -v "$R":/repo \
#              -v "$R/.phorepoc-x86-build":/out phorepoc-x86 bash /repo/tools/phorepoc-x86/mbedtls/build_mbedtls.sh
#
# Source: the mbedTLS the Goal U port uses (shim/: CMake FetchContent, 3.6.7 -> shim/build/_deps/mbedtls-src;
# MBEDTLS_SRC overrides). Config: phorepoc_mbedtls_config.h (MBEDTLS_CONFIG_FILE). Every library/*.c is compiled
# with the Open C client line (modules the config leaves out compile to nothing).
# Out: $OUT/lib/libmbedtls.a (tls + x509 + crypto in one archive); curl and NetSurf take the headers from the source.
set -u
REPO=${REPO:-/repo}
. "$REPO/tools/phorepoc-x86/netsurf/arch.sh"   # CC/AR/OBJCOPY + OUT for NS_ARCH (x32 or arm)
JOBS=${JOBS:-$(nproc)}
HERE=$REPO/tools/phorepoc-x86/mbedtls
SRC=${MBEDTLS_SRC:-$REPO/shim/build/_deps/mbedtls-src}
[ -f "$SRC/include/mbedtls/build_info.h" ] || { echo "no mbedTLS at $SRC (configure shim/ once: its CMake fetches it)"; exit 1; }
grep -q 'MBEDTLS_VERSION_STRING *"3\.6' "$SRC/include/mbedtls/build_info.h" || { echo "expected mbedTLS 3.6.x"; exit 1; }

mapfile -t XFLAGS < <(python3 "$REPO/tools/phorepoc-x86/netsurf/openc_cflags.py" --std=gnu11)
[ "${#XFLAGS[@]}" -gt 10 ] || { echo "openc_cflags.py failed"; exit 1; }
XF=()
for f in "${XFLAGS[@]}"; do [ "$f" = "-w" ] || XF+=("$f"); done
# (-iquote library/ and mbedTLS's include/ ahead of the Open C line: its stdapis/openssl headers share names)
XF+=(-Werror=implicit-function-declaration -Werror=implicit-int -Wno-attributes -fmax-errors=5
     "-DMBEDTLS_CONFIG_FILE=\"phorepoc_mbedtls_config.h\"" -iquote "$SRC/library" -I"$HERE" -I"$SRC/include")
# ARM (the real 701): platform_util.c knows no clock there -> entropy_symbian.cpp's mbedtls_ms_time (as ssl_arm)
[ "${NS_ARCH:-x32}" = arm ] && XF+=(-DMBEDTLS_PLATFORM_MS_TIME_ALT)

od=$OUT/obj/libmbedtls; ld=$OUT/logs/libmbedtls
rm -rf "$od" "$ld"; mkdir -p "$od" "$ld" "$OUT/lib"
printf '%s\n' "${XF[@]}" > "$od/.flags"
compile_one() {
    local s=$1 od=$2 ld=$3
    local base; base=$(basename "$s" .c)
    mapfile -t F < "$od/.flags"
    if "$CC" -I"$SRC/include" "${F[@]}" -c "$s" -o "$od/$base.o" > "$ld/$base.log" 2>&1; then
        echo "OK $s" >> "$od/.status"
    else
        echo "FAIL $s" >> "$od/.status"
    fi
}
export -f compile_one
ls "$SRC"/library/*.c | xargs -P "$JOBS" -I{} bash -c 'compile_one "$@"' _ {} "$od" "$ld"
n_ok=$(grep -c '^OK' "$od/.status"); n_fail=$(grep -c '^FAIL' "$od/.status")
echo "libmbedtls: $n_ok ok, $n_fail failed"
if [ "$n_fail" -gt 0 ]; then
    for f in $(grep '^FAIL' "$od/.status" | head -8 | awk '{print $2}'); do
        echo "--- $(basename "$f")"; grep -m3 "error" "$ld/$(basename "$f" .c).log"
    done
    exit 1
fi
rm -f "$OUT/lib/libmbedtls.a"; "$AR" rcs "$OUT/lib/libmbedtls.a" "$od"/*.o
echo "libmbedtls: $OUT/lib/libmbedtls.a"
