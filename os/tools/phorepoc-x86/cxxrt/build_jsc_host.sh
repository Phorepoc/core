#!/usr/bin/env bash
# build_jsc_host.sh -- host sanity build: the SAME JSC config (JSCOnly, CLoop, no JIT, static, system malloc) natively on x86_64 Linux
# in the container (x32 userland/kernel is not available there), to validate the CLoop build + hello.js before the Phorepoc port.
# IN-CONTAINER (image phorepoc-jsc). Output: /wv/build-host/bin/jsc
set -u
V=/wv; SRC=$V/wpewebkit-2.54.1; BLD=$V/build-host; OUT=${OUT:-/out/cxxrt}; mkdir -p $OUT
if [ ! -f $BLD/build.ninja ]; then
  mkdir -p $BLD; cd $BLD
  cmake -G Ninja $SRC -DPORT=JSCOnly -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DENABLE_JIT=OFF -DENABLE_C_LOOP=ON -DENABLE_DFG_JIT=OFF -DENABLE_FTL_JIT=OFF -DENABLE_WEBASSEMBLY=OFF -DENABLE_SAMPLING_PROFILER=OFF \
    -DENABLE_STATIC_JSC=ON -DUSE_SYSTEM_MALLOC=ON -DDEVELOPER_MODE=OFF -DENABLE_API_TESTS=OFF > $OUT/jsc-host-configure.log 2>&1; echo "configure $?"
fi
cd $BLD && ninja -j${JOBS:-8} jsc > $OUT/jsc-host-build.log 2>&1; echo "ninja exit $?"; grep -c "error:" $OUT/jsc-host-build.log
cat > /tmp/hello.js <<'J'
print("hello from jsc");
var s=0; for (var i=0;i<100000;i++) s+=i%7; print(s);
print([3,1,2].sort().join(","), JSON.stringify({a:[1,2,{b:"x"}]}), (0.1+0.2).toFixed(3), "héllo".toUpperCase());
J
find $BLD -name jsc -type f | head -2
$BLD/bin/jsc /tmp/hello.js
