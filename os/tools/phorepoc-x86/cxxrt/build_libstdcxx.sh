#!/usr/bin/env bash
# build_libstdcxx.sh -- P0 spike: libstdc++ (GCC 13.3 source) for Path C x32 over Open C / P.I.P.S. headers. IN-CONTAINER.
#   host: R="$(pwd -W)"; MSYS_NO_PATHCONV=1 docker.exe run --rm -v "$R":/repo -v "$R/.phorepoc-x86-build":/out phorepoc-x86 \
#             bash /repo/tools/phorepoc-x86/cxxrt/build_libstdcxx.sh  > .cxxrt-build.log 2>&1
# Source: third_party/gcc/gcc-13.3.0.tar.xz (gitignored). Same GCC version as the container's compiler (libsupc++ ABI).
set -u
# ARCH=arm (2026-10-14, the PinePhone browser): the same library for ARMv7 Thumb-2 + VFP, soft-float ABI (links with the ROM's
# armv5te userland, ARM_VFP=1 shape), in the phorepoc-arm container (arm-none-eabi-gcc 13.2). OUT=/out/cxxrt-arm there.
ARCH=${ARCH:-x32}
REPO=/repo; W=${W:-/tmp/lsc-$ARCH}; OUT=${OUT:-/out/cxxrt}; mkdir -p $W $OUT
if [ $ARCH = arm ]; then
  CROSS=arm-none-eabi-; HOST=arm-none-eabi; CFL=--arm; MFL="-march=armv7-a -mthumb -mfloat-abi=softfp -mfpu=vfpv3-d16 -mword-relocations"   # E32 relocates 32-bit words only: no MOVW/MOVT
  GINC=$(arm-none-eabi-gcc -print-file-name=include)
else
  CROSS=; HOST=x86_64-unknown-elf; CFL=; MFL=-mx32; GINC=/usr/lib/gcc/x86_64-linux-gnu/13/include
fi
TB=$REPO/third_party/gcc/gcc-13.3.0.tar.xz
[ -d $W/gcc-13.3.0/libstdc++-v3 ] || tar -C $W --exclude='gcc-13.3.0/gcc' --exclude='gcc-13.3.0/gcc-*' -xf $TB || exit 1
S=$W/gcc-13.3.0
[ -f $S/gcc/DATESTAMP ] || tar -C $W -xf $TB gcc-13.3.0/gcc/DATESTAMP gcc-13.3.0/gcc/BASE-VER gcc-13.3.0/gcc/DEV-PHASE
OVR=$REPO/tools/phorepoc-x86/cxxrt/overlay
mapfile -t FL < <(python3 $REPO/tools/phorepoc-x86/netsurf/openc_cflags.py --exe $CFL | grep -v -x -E -e '-x|c|-march=armv5te|-marm|-mfloat-abi=soft|-std=gnu11|-fno-exceptions|-fno-rtti|-fshort-wchar|-w|-ffreestanding|-fno-builtin|-masm=intel|-Dwchar_t=unsigned short|-D__SOFTFP=')
mkdir -p $W/bin
for t in gcc g++; do   # libstdc++ own -I first (its cstdlib include_next must find stdapis AFTER it), the Open C line after "$@"
  { echo "#!/bin/sh"; echo -n "exec $CROSS$t $MFL "'"$@"'" "; printf "'%s' " "${FL[@]}" "-I$OVR" -include $OVR/ph_cxx_compat.h -idirafter $GINC; echo; } > $W/bin/ph-$t; chmod +x $W/bin/ph-$t
done
# os/newlib assumes newlib ctype tables; Open C ctype is functions -> take os/generic (self-built table) for the OS dir
cp $S/libstdc++-v3/config/os/generic/* $S/libstdc++-v3/config/os/newlib/
cp $S/libgcc/gthr-posix.h $S/libgcc/gthr-default.h   # what gcc/configure generates in-tree; configure probes gthr.h with it
B=$W/build; mkdir -p $B; cd $B
if [ ! -f Makefile ]; then
  sed -i 's/^\( *\)as_fn_error \$? "Link tests are not allowed.*/\1:/' $S/libstdc++-v3/configure   # link tests then RUN and FAIL (-nostdlib): every function is "absent"
  # arm-none-eabi-gcc reports "Thread model: single" (bare metal); our threads are Open C pthreads, as on x32
  sed -i 's/^\( *\)target_thread_file=`\$CXX -v 2>&1 .*/\1target_thread_file=posix/' $S/libstdc++-v3/configure
  CC=$W/bin/ph-gcc CXX=$W/bin/ph-g++ LDFLAGS="-nostdlib -nostartfiles -static" CFLAGS="-O2 -g0" CXXFLAGS="-O2 -g0 -fexceptions -frtti" \
  $S/libstdc++-v3/configure --host=$HOST --build=x86_64-linux-gnu --with-newlib --disable-shared --enable-static \
     --disable-multilib --disable-libstdcxx-pch --disable-nls --disable-libstdcxx-dual-abi --disable-libstdcxx-filesystem-ts \
     --enable-libstdcxx-threads --disable-libstdcxx-backtrace --with-default-libstdcxx-abi=new > $OUT/configure.log 2>&1 || { tail -30 $OUT/configure.log; exit 1; }
fi
echo configured
cd $B
[ -f include/x86_64-unknown-elf/bits/gthr-default.h ] || true
make -C include > $OUT/make-inc.log 2>&1 || tail -5 $OUT/make-inc.log
make -k -j${JOBS:-$(nproc)} > $OUT/make.log 2>&1
echo "make exit $?"; grep -c "error:" $OUT/make.log
# the archive is assembled by hand: -k leaves a few TUs out (std::filesystem dir ops, <chrono> tzdb, long-double math stubs:
# Open C lacks d_type / has C long double overloads) -- none of which the browser needs
rm -f $OUT/libstdc++.a; find $B/src $B/libsupc++ -name '*.o' ! -path '*/.libs/*.dll*' | sort > $OUT/objs.txt
${CROSS}ar rcs $OUT/libstdc++.a $(cat $OUT/objs.txt); ls -l $OUT/libstdc++.a; wc -l < $OUT/objs.txt
# libstdc++'s own throws (std::__throw_*) become weak: ph_throw.cpp (an object of each image) defines them -- log + panic
# CXX-THROW instead of a throw our images cannot unwind (C++ exceptions: browser-plan.md). ph_weaken_throws.sh = this step alone.
CROSS=$CROSS bash $REPO/tools/phorepoc-x86/cxxrt/ph_weaken_throws.sh $OUT/libstdc++.a
