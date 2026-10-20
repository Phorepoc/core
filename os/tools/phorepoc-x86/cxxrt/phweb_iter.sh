#!/usr/bin/env bash
# phweb_iter.sh -- one browser iteration, every step timed (2026-10-13: the loop took 25-35 min, 13.5 of them GNU ld):
#   WebCore/phwebtest (ninja, incremental) -> package_web -> link (lld) -> ROM (FAST) -> boot, quitting when phwebtest exits.
# Usage (repo root, Git Bash):
#   bash tools/phorepoc-x86/cxxrt/phweb_iter.sh https://example.com/          one page + paint
#   bash tools/phorepoc-x86/cxxrt/phweb_iter.sh --wpt <list file>            WPT list (tools/phorepoc-x86/wpt/mklist.py)
# NOBUILD=1 skips WebCore + package + link (only the ROM/boot changed: libpthread, a test list). SECS = boot timeout
# (default 2400 for --wpt, 420 for a page). MEM = guest RAM (default 1024 = what Symbian gets on the PinePhone; 512 left a 356 MB RAM bank
# after the ROM, and dom/nodes/name-validation.html ran JSC out of memory). Results: .phweb-iter/ (logs, timings.txt, wpt-results.txt, page.png).
set -u
cd "$(dirname "$0")/../../.."
D=tools/phorepoc-x86/cxxrt; O=.phweb-iter; mkdir -p $O; : > $O/timings.txt
step() {   # step <name> <command...>: run, log to $O/<name>.log, time it, stop the iteration on failure
  local name=$1; shift; local t0=$SECONDS
  "$@" > $O/$name.log 2>&1; local rc=$?
  printf '%-9s %4ds  rc=%d\n' "$name" $((SECONDS - t0)) $rc | tee -a $O/timings.txt
  [ $rc = 0 ] || { tail -n 15 $O/$name.log; exit $rc; }
}
if [ "${NOBUILD:-0}" != 1 ]; then
  step webcore bash $D/drun2.sh /repo/$D/build_webcore.sh phwebtest
  grep -q "^0 errors" $O/webcore.log || { grep -m5 -A4 "error:" .phorepoc-x86-build/cxxrt/wc-build-phwebtest.log; exit 1; }
  step package bash $D/drun2.sh /repo/$D/package_web.sh
  step link ./ph build phwebtest
  grep "stub:" $O/link.log && echo "(link stubs above: an inline without its *Inlines.h header?)"
fi
step rom env FAST=1 BOOT=ssm NET=1 SECONDARY_IMAGE=efile.exe bash tools/make_phorepoc_rom.sh
if [ "${1:-}" = --wpt ]; then
  # WPT's hostnames (ph-config.json browser_host web-platform.test + the subdomains get_host_info() uses): the guest's DNS
  # daemon reads C:\private\10000882\hosts (tcpiputils/dnd hosts.cpp) -- slirp's DNS proxy knows only real names, so every
  # cross-origin XHR/fetch test was a network error (2026-10-14). They map to wptserve's address as the QEMU container sees it.
  IP=$(MSYS_NO_PATHCONV=1 docker.exe run --rm phorepoc-x86 getent hosts host.docker.internal | awk '{print $1}')
  mkdir -p $O/disk/private/10000882
  N=""; for d in "" www. www1. www2. xn--n8j6ds53lwwkrqhv28a. xn--lve-6lad.; do N="$N ${d}web-platform.test ${d}not-web-platform.test"; done
  printf '%s %s\n' "${IP:-192.168.65.254}" "$N" > $O/disk/private/10000882/hosts
  QO=$'[js] exit(
[js] abort()
phwebtest.exe::Main Panic'
  LOGS=.phorepoc-x86-build/ekern/run.log
  [ "${SHARDS:-1}" -gt 5 ] && { echo "SHARDS <= 5: the budget is 10 cores, 8 GB (2 cores + 1.5 GB a shard)"; exit 1; }
  if [ "${SHARDS:-1}" -gt 1 ]; then
    # SHARDS=n (2026-10-14): n QEMU boots at once, the list dealt round-robin (each shard gets a slice of every directory).
    # The ISO is built once (make_ekern.sh without run); every shard boots it from its own disk copy (phweb_shard.sh).
    B=.phorepoc-x86-build; R2=$(pwd -W)
    printf '%s\n' '!phwebtest|--wpt c:\wpt.txt' > $B/disk/boot-bootui.txt
    rm -rf $B/disk/boot-files; mkdir -p $B/disk/boot-files; cp -r $O/disk/private $B/disk/boot-files/private
    step iso env MSYS_NO_PATHCONV=1 docker.exe run --rm -v "$R2:/repo" -v "$R2/$B:/out" -e SECONDARY=rom -e SCREEN=360x640 \
      phorepoc-x86 bash /repo/tools/phorepoc-x86/ekern/make_ekern.sh
    rm -rf $O/shards; LOGS=""; t0=$SECONDS
    for i in $(seq 1 $SHARDS); do
      mkdir -p $O/shards/$i; awk -v n=$SHARDS -v i=$i 'NF && (NR - 1) % n == i - 1' "$2" > $O/shards/$i/wpt.txt
      MSYS_NO_PATHCONV=1 docker.exe run --rm --name phweb-shard$i --cpus=2 --memory=1536m -v "$R2:/repo" -v "$R2/$B:/out:ro" -v "$R2/$O/shards/$i:/shard" \
        -e QUIT_ON="$QO" -e QUIT_GRACE="${QUIT_GRACE:-30}" -e SECS="${SECS:-7200}" -e MEM="${MEM:-1024}" \
        phorepoc-x86 bash /repo/$D/phweb_shard.sh /shard > $O/shards/$i/boot.log 2>&1 &
      LOGS="$LOGS $O/shards/$i/run.log"
    done
    wait
    printf '%-9s %4ds  rc=0  (%s shards)\n' boot $((SECONDS - t0)) $SHARDS | tee -a $O/timings.txt
  else
    step boot env MEM=${MEM:-1024} QUIT_ON="$QO" DISKFILES="$2=wpt.txt $O/disk/private=private" \
      BOOTLIST='!phwebtest|--wpt c:\wpt.txt' ./ph boot "${SECS:-2400}"
  fi
  R=$O/wpt-results-$(basename "$2" .txt).txt   # per list: a probe run must not overwrite a directory's results
  # results come straight to serial as "[js] > <line>", long lines continued by "[js] >+ <rest>" (phsym.cpp ph_serial): join them
  for l in $LOGS; do
  awk '/^.:\[js\] >\+ /{sub(/^.:\[js\] >\+ /, ""); last = last $0; next}
       {if (have) print last; have = 0}
       /^.:\[js\] > /{sub(/^.:\[js\] > /, ""); last = $0; have = 1; next}
       {print}
       END{if (have) print last}' $l \
    | grep -a "^wpt[:|]\|Panic\|^.:eip\|\[cxx\]\|\[pthread\]\|abort()"
  done > $R
  cp $R $O/wpt-results.txt
  grep "wpt: done\|Panic\|\[cxx\]\|\[pthread\]\|abort" $R
  awk '/^wpt: http/{o[$3]++} /COUNT/{p+=$4; f+=$6} END{printf "subtests: pass %d, not-pass %d; tests:", p, f; for (k in o) printf " %s %d", k, o[k]; print ""}' $R
  # against upstream WebKit 2.54.1 (tools/phorepoc-x86/wpt/compare.py); the regressing tests -> $O/regressions-<list>.txt
  py tools/phorepoc-x86/wpt/compare.py $R --list $O/regressions-$(basename "$2" .txt).txt | tail -n 3
else
  step boot env MEM=${MEM:-1024} QUIT_ON=$'[js] exit(
[js] abort()
phwebtest.exe::Main Panic' BOOTLIST="!phwebtest${1:+|$1}" ./ph boot "${SECS:-420}"
  grep -a "phwebtest: \(WebKit\|painted\|done\)\|Panic\|\[cxx\]\|\[pthread\]\|abort()" .phorepoc-x86-build/ekern/run.log | head
  MSYS_NO_PATHCONV=1 docker.exe run --rm -v "$(pwd -W)/.phorepoc-x86-build:/out" phorepoc-x86 bash -c \
    'MTOOLS_SKIP_CHECK=1 mcopy -o -i /out/disk/boot.img@@1M ::/data/page.ppm /out/page.ppm' 2>/dev/null \
    && py $D/ppm2png.py .phorepoc-x86-build/page.ppm $O/page.png
fi
cat $O/timings.txt
