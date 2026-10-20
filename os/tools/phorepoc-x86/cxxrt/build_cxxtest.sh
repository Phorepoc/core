#!/usr/bin/env bash
# build_cxxtest.sh -- IN-CONTAINER: compile the C++17 test + EH glue against the libstdc++ headers (build_libstdcxx.sh must have run:
# W=/wk/lsc) into /out/cxxrt/cxx17test.a (+ a copy of libgcc_eh.a), for the `cxx17test` registry component.
set -u
REPO=/repo; W=${W:-/wk/lsc}; OUT=${OUT:-/out/cxxrt}; D=$REPO/tools/phorepoc-x86/cxxrt
inc="-I$W/build/include/x86_64-unknown-elf -I$W/build/include -I$W/gcc-13.3.0/libstdc++-v3/libsupc++"
for src in cxx17_test eh_register; do
  $W/bin/ph-g++ $inc -std=gnu++20 -fexceptions -frtti -O2 -g0 ${EH:+-DPH_EH} -c $D/$src.cpp -o $OUT/$src.o 2>&1 | grep -v "SYMBIAN_ENABLE\|previous def\|^ *1864\|^ *|\|In file included from <command" | head -${N:-30}
done
rm -f $OUT/cxx17test.a; ar rcs $OUT/cxx17test.a $OUT/cxx17_test.o ${EH:+$OUT/eh_register.o}   # EH=1: + the .eh_frame registration (exceptions: see the notes in browser-plan.md; unfinished)
cp /usr/lib/gcc/x86_64-linux-gnu/13/x32/libgcc_eh.a $OUT/libgcc_eh.a; cp /usr/lib/gcc/x86_64-linux-gnu/13/x32/libgcc.a $OUT/libgcc.a
ls -l $OUT/*.a
if [ -n "${EH:-}" ]; then
# ld rewrites .eh_frame (it drops the FDEs of --gc-sections'd functions, merges CIEs) but --emit-relocs keeps the OLD relocation
# offsets -> the E32 emitter relocates the wrong words (seen: 182 corrupted words in the ROM's eh table). UNFINISHED.
for a in cxx17test libstdc++ libgcc_eh libgcc; do
  objcopy --rename-section .eh_frame=.ph_eh_frame,alloc,load,readonly,data,contents $OUT/$a.a
done
fi
