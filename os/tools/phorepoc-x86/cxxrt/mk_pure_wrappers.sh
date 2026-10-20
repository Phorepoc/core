#!/usr/bin/env bash
# mk_pure_wrappers.sh -- IN-CONTAINER. Writes $W/bin/pp-gcc, pp-g++ : the "pure Open C" x32 compile line for engine code (WebKit/ICU).
# = openc_cflags.py --exe minus the Symbian kernel/euser include dirs and minus the three force-includes (variant .hrh, port_prelude.h,
# e32def.h); a stub e32def.h (overlay/pure) satisfies stdapis' own #include <e32def.h>. Removes the ASSERT clash class.
set -u
ARCH=${ARCH:-x32}   # arm: the PinePhone browser (ARMv7 Thumb-2 + VFP, soft-float ABI; drun-arm.sh)
if [ $ARCH = arm ]; then CROSS=arm-none-eabi-; CFL=--arm; MFL="-march=armv7-a -mthumb -mfloat-abi=softfp -mfpu=vfpv3-d16 -mword-relocations -include /repo/tools/phorepoc-x86/cxxrt/overlay/pure/ph_arm_prefix.h -D__IEEE_LITTLE_ENDIAN"; GINC=$(arm-none-eabi-gcc -print-file-name=include)
  POST="-fno-pic"   # after the caller's flags: WebKit/ICU/OpenSSL ask for -fPIC = GOT relocations, which E32 images cannot carry
else POST=""; CROSS=; CFL=; MFL=-mx32; GINC=/usr/lib/gcc/x86_64-linux-gnu/13/include; fi
REPO=/repo; W=${W:-/wk/lsc}; OVR=$REPO/tools/phorepoc-x86/cxxrt/overlay
mapfile -t FL < <(python3 $REPO/tools/phorepoc-x86/netsurf/openc_cflags.py --exe $CFL | grep -v -x -E -e '-x|c|-march=armv5te|-marm|-mthumb-interwork|-mfloat-abi=soft|-DSYMCC_C_DRIVER|-std=gnu11|-fno-exceptions|-fno-rtti|-fshort-wchar|-w|-ffreestanding|-fno-builtin|-masm=intel|-Dwchar_t=unsigned short|-D__SOFTFP=')
OUTA=(); skip=0; prev=""
for f in "${FL[@]}"; do
  if [ $skip = 1 ]; then skip=0; continue; fi
  case "$f" in
    -include) skip=1; continue;;
    -I) prev=-I; continue;;
  esac
  if [ "$prev" = -I ]; then prev=""; p="$f"; else p="${f#-I}"; [ "$p" = "$f" ] && { OUTA+=("$f"); continue; }; fi
  case "$p" in
    */toolchain/*|*/kernel/eka/*|*/halservices/*|*/tools/phorepoc-x86/port) continue;;
    # Open C's stdapis LAST (-idirafter), and .epoc32/include (zlib.h too): they bundle OpenSSL 0.9.8, an old sqlite3.h and zlib, and the engine's own (build_webdeps.sh)
    # come in as CMake -isystem dirs -- which GCC searches after every -I, and a -I naming an -isystem dir is ignored. Open C still
    # answers everything nobody else provides (and libstdc++'s #include_next <stdlib.h> & co. still reach it).
    */stdapis|*/stdapis/*|*/.epoc32/include) OUTA+=("-idirafter" "$p"); continue;;
  esac
  OUTA+=("-I$p")
done
mkdir -p $W/bin
for t in gcc g++; do
  # ARM: the bare-metal driver rejects -pthread (OpenSSL, CMake's Threads): dropped -- the threads are Open C's pthreads either way
  { echo "#!/bin/sh"; [ $ARCH = arm ] && echo 'for a; do shift; [ "$a" = -pthread ] || set -- "$@" "$a"; done'; echo -n "exec $CROSS$t $MFL -include $OVR/ph_cxx_compat.h "'"$@"'" $POST -I$OVR/pure "; printf "'%s' " "${OUTA[@]}" "-D__SOFTFP=" "-I$OVR" -include $OVR/ph_cxx_compat.h "-idirafter" $GINC; echo; } > $W/bin/pp-$t; chmod +x $W/bin/pp-$t
done
cat $W/bin/pp-g++
