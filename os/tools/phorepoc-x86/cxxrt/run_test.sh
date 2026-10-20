#!/usr/bin/env bash
# run_test.sh [secs] -- P0 spike: build the BOOT='!cxx17test' ROM (bootui runs just the test), boot it on QEMU, print the [cxxrt] lines.
# (cxx17test must be built: ./ph build cxx17test.) Waits for the docker host to be idle between steps: a boot container outlives
# `./ph boot`, and two QEMUs on one boot.img fail with "Failed to get write lock".
cd "$(dirname "$0")/../../.."
idle() { sleep 3; while docker ps -q | grep -q .; do sleep 3; done; }
idle; sleep 5
FAST=1 BOOT='!cxx17test' SECONDARY_IMAGE=efile.exe bash tools/make_phorepoc_rom.sh > .cxxrt-rom.log 2>&1 || { tr -d '\000' < .cxxrt-rom.log | tail -5; exit 1; }
idle; sleep 5
for try in 1 2 3 4; do sleep 25; ./ph boot "${1:-60}" > .cxxrt-boot.log 2>&1; idle
  grep -a -q "Failed to get" .phorepoc-x86-build/ekern/run.log || break   # a QEMU of the previous run still holds boot.img
done
grep -a "\[cxxrt\]\|cxx17test\|CRASH\|PANIC\|FAULT" .phorepoc-x86-build/ekern/run.log | head -${N:-20} | cut -c1-250
