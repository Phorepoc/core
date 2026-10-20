#!/usr/bin/env bash
# build_phcompat.sh -- IN-CONTAINER: libphcompat.a (OURS) with the pure Open C line. -> $OUT/libphcompat.a
set -u
source /repo/tools/phorepoc-x86/cxxrt/arch.sh
W=${W:-/wk/lsc}; OUT=${OUT:-/out/cxxrt}; D=/repo/tools/phorepoc-x86/cxxrt/compat; T=/tmp/phc; rm -rf $T; mkdir -p $T
for f in $D/*.c; do $W/bin/pp-gcc -O2 -g0 -Wall -Wno-unused -c $f -o $T/$(basename $f .c).o 2>&1 | grep -v "Wmissing-field\|PTHREAD_\|^ *[0-9]* |\|^ *|\|In file included\|from " ; done
rm -f $OUT/libphcompat.a; ${CROSS}ar rcs $OUT/libphcompat.a $T/*.o; ls -l $OUT/libphcompat.a
# ARM: the compiler's own libgcc for this line (ARMv7 Thumb-2, soft-float ABI): the 64-bit division/shift helpers the engine calls
[ $ARCH = arm ] && cp $(arm-none-eabi-gcc -march=armv7-a -mthumb -mfloat-abi=softfp -mfpu=vfpv3-d16 -print-libgcc-file-name) $OUT/libgcc.a
