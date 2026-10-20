#!/usr/bin/env bash
# in container: compile a C++ file with Open C headers + libstdc++ headers (probe)
set -u
cd /repo
mapfile -t FL < <(python3 tools/phorepoc-x86/netsurf/openc_cflags.py --exe | grep -v -x -E -e '-x|c|-std=gnu11|-fno-exceptions|-fno-rtti|-fshort-wchar')
printf '%s\n' "${FL[@]}" | tail -8
SRC=${1:-tools/phorepoc-x86/cxxrt/cxx17_test.cpp}
g++ -mx32 -x c++ -std=gnu++20 -fexceptions -frtti -O2 "${FL[@]}" -isystem /usr/include/c++/13 -isystem /usr/include/x86_64-linux-gnu/c++/13/x32 -c $SRC -o /tmp/t.o 2>&1 | head -${N:-40}
