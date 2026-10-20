#!/usr/bin/env bash
# web_boot.sh [seconds] -- run phwebtest.exe (the WebKit page test, browser-plan.md section 9) on Phorepoc x86 (QEMU), like
# jsc_boot.sh: package the built engine (package_web.sh), link the phwebtest component, a bootui ROM whose only item is
# phwebtest, boot, print the [js] lines (phsym.cpp replays stdout -- the render tree + pixel stats -- to serial at exit).
# Prereq: build_webcore.sh phwebtest (drun2.sh). Private build dir .phorepoc-jsc-build (shared with jsc_boot.sh: run one at a time).
set -e
cd "$(dirname "$0")/../../.."
SECS=${1:-300}; export MEM=${MEM:-512}   # QEMU RAM (MB): a 65 MB image + the page's heap
export PHOREPOC_B=.phorepoc-jsc-build
bash tools/phorepoc-x86/cxxrt/drun2.sh /repo/tools/phorepoc-x86/cxxrt/package_web.sh > .wc-pkg.log 2>&1 || { tail -5 .wc-pkg.log; exit 1; }
./ph build phwebtest > .wc-e32.log 2>&1
grep -E "ACCEPTED|FAIL" .wc-e32.log
FAST=1 LEAVE_TRACE=1 NET=1 BOOT='!phwebtest' SECONDARY_IMAGE=efile.exe \
  bash tools/make_phorepoc_rom.sh > .wc-rom.log 2>&1 || { tail -5 .wc-rom.log; exit 1; }
tail -1 .wc-rom.log
./ph boot "$SECS" > .wc-boot.log 2>&1 || true
grep -a "\[js\]" $PHOREPOC_B/ekern/run.log | tr -cd '\11\12\15\40-\176' | head -80
