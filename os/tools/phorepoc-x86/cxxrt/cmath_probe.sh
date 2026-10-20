#!/usr/bin/env bash
# list what <cmath>/<cstdlib>/<cctype> name that Open C's headers lack (with the C99 config macros on)
W=${W:-/wk/lsc}
echo '#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cwchar>
int main(){return 0;}' > /tmp/p.cpp
$W/bin/ph-g++ -I$W/build/include/x86_64-unknown-elf -I$W/build/include -I$W/gcc-13.3.0/libstdc++-v3/libsupc++ -std=gnu++20 -fsyntax-only -fmax-errors=0 /tmp/p.cpp 2>&1 | grep "error:" | sed "s/.*error: //" | sort | uniq -c | sort -rn | head -${N:-80}
