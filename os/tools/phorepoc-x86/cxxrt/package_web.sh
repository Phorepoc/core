#!/usr/bin/env bash
# package_web.sh -- IN-CONTAINER (phorepoc-jsc): gather the WebKit page stack's pieces into $OUT/*.a for the `phwebtest` registry
# component (x32_archives), the way package_jsc.sh does it for jsc. CMake's thin archives are re-packed into real ones; the deps
# (build_webdeps.sh, build_curl_ossl.sh, ICU, NetSurf's libpng/libnspsl) are copied. Census of what is still missing: census_web.sh.
set -eu
source /repo/tools/phorepoc-x86/cxxrt/arch.sh
V=/wv; BLD=$V/build-wc; OUT=${OUT:-/out/cxxrt}; W=$OUT/web; P=$DEPS/lib; ICUL=$ICU/lib
mkdir -p $W; cd $BLD
ninja -t inputs phwebtest 2>/dev/null | grep "phwebtest.dir/.*\.o$" > /tmp/webmain.txt; wc -l < /tmp/webmain.txt
rm -f $W/libphwebmain.a; ${CROSS}ar rcs $W/libphwebmain.a $(cat /tmp/webmain.txt)
# the BrCtl facade's engine (phebrctl -> browserengine.dll): PhEngine.cpp + the same Ph* support sources, as its own archive
if ninja -t inputs phengine 2>/dev/null | grep "phengine.dir/.*\.o$" > /tmp/webeng.txt && [ -s /tmp/webeng.txt ]; then rm -f $W/libphengine.a; ${CROSS}ar rcs $W/libphengine.a $(cat /tmp/webeng.txt); fi
for l in WebCore PAL JavaScriptCore WTF bmalloc Skia; do
  f=$BLD/lib/lib$l.a; [ $W/lib$l.a -nt $f ] && continue; rm -f $W/lib$l.a
  (cd $BLD && ${CROSS}ar t $f | xargs ${CROSS}ar rcs $W/lib$l.a)
done
for l in freetype harfbuzz harfbuzz-icu xml2 xslt sqlite3 webp webpdemux sharpyuv jpeg curl ssl crypto z; do cp -u $P/lib$l.a $W/; done
for l in icui18n icuuc icudata; do cp -u $ICUL/lib$l.a $W/; done
# native TLS cannot link into an E32 image: such sources need -femulated-tls (Platform-WebCore.cmake, at its end)
TLS=$(for a in $W/*.a; do ${CROSS}readelf -rW $a 2>/dev/null | awk -v A=$(basename $a) '/^File:/{f=$2} /TPOFF|TLSGD|TLSLD/{print f; exit}'; done)
[ -z "$TLS" ] || { echo "native TLS relocations (add -femulated-tls for these):"; echo "$TLS"; exit 1; }
ls -l $W/*.a | awk '{s+=$5; print $5, $9} END {print s, "total"}'
