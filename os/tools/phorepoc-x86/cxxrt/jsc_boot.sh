#!/usr/bin/env bash
# jsc_boot.sh [script.js] [seconds] -- run jsc.exe on Phorepoc x86 (QEMU): package the built engine (package_jsc.sh), link the
# jsc component, a bootui ROM whose only item is `jsc z:\run.js` (the script staged as Z:\run.js), boot, print the [js] lines.
# Prereq: build_jsc.sh jsc (drun2.sh; JOBS=4 -- see the 2026-10-11 notes in that script). Private build dir .phorepoc-jsc-build.
# jsc's stdout goes to C:\jsout.txt (phsym.cpp); the [js] lines (ctor, exit, abort + stack) go to the serial log.
set -e
cd "$(dirname "$0")/../../.."
JS=${1:-tools/phorepoc-x86/cxxrt/hello.js}; SECS=${2:-240}
export PHOREPOC_B=.phorepoc-jsc-build
bash tools/phorepoc-x86/cxxrt/drun2.sh /repo/tools/phorepoc-x86/cxxrt/package_jsc.sh > .jsc-pkg.log 2>&1
./ph build jsc > .jsc-ph.log 2>&1
grep -E "ACCEPTED|FAIL" .jsc-ph.log
# (not ./ph rom: it names its log after the boot list)
FAST=1 LEAVE_TRACE=1 BOOT='!jsc|z:\run.js' SECONDARY_IMAGE=efile.exe ROM_DATA_EXTRA="run.js=$JS" \
  bash tools/make_phorepoc_rom.sh > .jsc-rom.log 2>&1 || { tail -5 .jsc-rom.log; exit 1; }
tail -1 .jsc-rom.log
./ph boot "$SECS" > .jsc-boot.log 2>&1 || true
grep -a "\[js\]" $PHOREPOC_B/ekern/run.log | tr -cd '\11\12\15\40-\176' | head -60
