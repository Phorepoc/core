#!/usr/bin/env bash
# build_icu.sh -- ICU 74.2 for x32 over Open C (P1 of ). IN-CONTAINER (phorepoc-jsc), after build_libstdcxx.sh
# + mk_pure_wrappers.sh. 1) native build (host tools: icupkg/genrb/pkgdata, data built with icu_filter.json = en only, no translit/rbnf/dictionaries),
# 2) cross build for x32 with the pure-Open-C wrapper (link tests fail by design -> every function "absent"; ICU has portable fallbacks).
# Output: $V/icu-x32/{include,lib/libicu{uc,i18n,data}.a}. Source: third_party/icu/icu4c-74_2-src.tgz (gitignored).
set -u
ARCH=${ARCH:-x32}   # arm: the PinePhone browser (drun-arm.sh); output $V/icu-arm
if [ $ARCH = arm ]; then IHOST=arm-unknown-linux-gnueabi;   # mh-linux makefile rules only: the compiler is bare-metal, no __linux__
   LTRIP=arm-none-eabi; else IHOST=x86_64-pc-linux-gnu; LTRIP=x86_64-unknown-elf; fi
REPO=/repo; W=${W:-/wk/lsc}; V=/wv; D=$REPO/tools/phorepoc-x86/cxxrt; OUT=${OUT:-/out/cxxrt}; J=${JOBS:-12}
[ -d $V/icu/source ] || tar -C $V -xf $REPO/third_party/icu/icu4c-74_2-src.tgz || exit 1
S=$V/icu/source
if [ ! -f $V/icu-native/bin/icupkg ] && [ ! -f $V/icu-native/build/bin/icupkg ]; then
  mkdir -p $V/icu-native; cd $V/icu-native
  ICU_DATA_FILTER_FILE=$D/icu_filter.json $S/runConfigureICU Linux --disable-tests --disable-samples --disable-extras --disable-layoutex > $OUT/icu-native-configure.log 2>&1 || { tail $OUT/icu-native-configure.log; exit 1; }
  make -j$J > $OUT/icu-native-make.log 2>&1 || { tail -20 $OUT/icu-native-make.log; exit 1; }
fi
# data trim: the tarball ships only the PREBUILT icudt74l.dat (the data filter needs the separate data zip), so cut the items with the native icupkg
IP=$(ls $V/icu-native/bin/icupkg $V/icu-native/build/bin/icupkg 2>/dev/null | head -1)
if [ ! -f $S/data/in/.trimmed ]; then
  mkdir -p $S/data/orig
  [ -f $S/data/orig/icudt74l.dat ] || cp $S/data/in/icudt74l.dat $S/data/orig/icudt74l.dat
  LD_LIBRARY_PATH=$V/icu-native/lib $IP -l -o /tmp/icu-items.txt $S/data/orig/icudt74l.dat
  python3 $D/icu_trim.py /tmp/icu-items.txt > /tmp/icu-rm.txt; wc -l /tmp/icu-items.txt /tmp/icu-rm.txt
  LD_LIBRARY_PATH=$V/icu-native/lib $IP -r /tmp/icu-rm.txt $S/data/orig/icudt74l.dat $S/data/in/icudt74l.dat || exit 1
  ls -l $S/data/in/icudt74l.dat; touch $S/data/in/.trimmed
fi
echo native ok
mkdir -p $V/icu-$ARCH; cd $V/icu-$ARCH
if [ ! -f Makefile ]; then
  sed -i 's/^\( *\)as_fn_error \$? "Link tests are not allowed.*/\1:/' $S/configure 2>/dev/null
  CC=$W/bin/pp-gcc CXX=$W/bin/pp-g++ CFLAGS="-O2 -g0 -DU_HAVE_STD_STRING=1 -DU_HAVE_WCHAR_H=0 -DU_HAVE_WCSCPY=0" CXXFLAGS="-O2 -g0 -DU_HAVE_WCHAR_H=0 -DU_HAVE_WCSCPY=0 -std=gnu++17 -I$W/build/include/$LTRIP -I$W/build/include -I$W/gcc-13.3.0/libstdc++-v3/libsupc++ -fexceptions -frtti" \
  LDFLAGS="-nostdlib -nostartfiles -static" ICU_DATA_FILTER_FILE=$D/icu_filter.json \
  $S/configure --host=$IHOST --build=x86_64-linux-gnu --with-cross-build=$V/icu-native --prefix=$V/icu-$ARCH/inst \
     --enable-static --disable-shared --disable-tools --disable-tests --disable-samples --disable-extras --disable-layoutex --disable-dyload \
     --with-data-packaging=static > $OUT/icu-$ARCH-configure.log 2>&1 || { tail -30 $OUT/icu-$ARCH-configure.log; exit 1; }
fi
# Open C wchar.h typedefs mbstate_t, libstdc++ <cwchar> (no wchar support in our build) defines a struct of the same name: keep ICU off <wchar.h>
echo configured
python3 $D/icu_patch.py $S || exit 1   # Phorepoc edits (__SYMBIAN32__ only; write-on-change)
make -k -j$J > $OUT/icu-$ARCH-make.log 2>&1; echo "make exit $?"; make install > $OUT/icu-$ARCH-install.log 2>&1; grep -c "error:" $OUT/icu-$ARCH-make.log
