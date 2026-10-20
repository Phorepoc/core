#!/usr/bin/env bash
# census_web.sh -- IN-CONTAINER (phorepoc-jsc): the undefined-symbol census of the WebKit page stack (phwebtest = WebCore + PAL + JSC + WTF +
# bmalloc + Skia + the deps), the way package_jsc.sh does it for jsc: symbols no archive defines and Open C (libc/libm/libpthread .def)
# does not export. Out: $OUT/web-undefined.txt (mangled) + web-undefined-demangled.txt, grouped counts on stdout. This list is the
# remaining platform work (browser-plan.md section 9). CMake's thin archives are read in place.
set -u
V=/wv; BLD=$V/build-wc; OUT=${OUT:-/out/cxxrt}; P=$V/deps-x32/lib; N=/out/netsurf/lib; ICU=$V/icu-x32/inst/lib
cd $BLD || exit 1
OBJ=$(ninja -t inputs phwebtest 2>/dev/null | grep "phwebtest.dir/.*\.o$")
A="$(ls $BLD/lib/lib{WebCore,PAL,JavaScriptCore,WTF,bmalloc,Skia}.a) $P/libfreetype.a $P/libharfbuzz.a $P/libharfbuzz-icu.a $P/libxml2.a $P/libsqlite3.a $P/libwebp.a \
   $P/libwebpdemux.a $P/libsharpyuv.a $P/libjpeg.a $N/libpng.a $N/libnspsl.a $P/libcurl.a $P/libssl.a $P/libcrypto.a $P/libz.a $ICU/libicui18n.a $ICU/libicuuc.a \
   $ICU/libicudata.a $OUT/libphcompat.a $OUT/libstdc++.a"
nm -A --defined-only $OBJ $A 2>/dev/null | awk 'NF>=3 && $(NF-1)!="U" {print $NF}' | sort -u > /tmp/def.txt
nm -u $OBJ $A 2>/dev/null | grep -v ":$" | awk '{print $NF}' | sort -u > /tmp/und.txt
OE=/repo/baseline/symbian-2010/oss.FCL.sf.os.ossrv/genericopenlibs/openenvcore
cat $OE/libc/eabi/libcu.def $OE/libm/eabi/libmu.def $OE/libpthread/eabi/libpthreadu.def | awk '$1!="EXPORTS" && NF>=2 {print $1}' | sort -u > /tmp/oc.txt
comm -23 /tmp/und.txt /tmp/def.txt | comm -23 - /tmp/oc.txt > $OUT/web-undefined.txt
c++filt < $OUT/web-undefined.txt > $OUT/web-undefined-demangled.txt
wc -l /tmp/und.txt /tmp/def.txt $OUT/web-undefined.txt
# grouped: C symbols, then C++ by namespace::class
echo "== C (libc-level) symbols:"; grep -v "::" $OUT/web-undefined-demangled.txt | head -80 | tr '\n' ' '; echo
echo "== C++ by class:"; grep "::" $OUT/web-undefined-demangled.txt | sed 's/(.*//' | awk -F'::' '{ if (NF>=3) print $1"::"$2; else print $1 }' | sort | uniq -c | sort -rn | head -60
