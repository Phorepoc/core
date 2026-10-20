#!/usr/bin/env bash
# phweb_shard.sh <shard dir> -- one WPT shard boot, in the phorepoc-x86 container (phweb_iter.sh SHARDS=n, 2026-10-14).
# The shard dir (mounted at /shard) holds wpt.txt; /out is the shared build dir, read only here: the ISO make_ekern.sh
# built, and the SSM regression disk the boot copies. Each shard gets its own disk copy, monitor-less QEMU and run.log,
# so shards never share a file. Same QUIT_ON watcher as make_ekern.sh run.
set -e
S=$1
cp /out/disk/regress-ssm-template.img $S/boot.img
export MTOOLS_SKIP_CHECK=1
mcopy -o -i $S/boot.img@@1M /out/disk/boot-bootui.txt ::BOOTUI.TXT
mcopy -o -i $S/boot.img@@1M $S/wpt.txt ::wpt.txt
mcopy -s -o -i $S/boot.img@@1M /out/disk/boot-files/private ::
: > $S/run.log
( while sleep 1; do
    if tail -c 65536 $S/run.log 2>/dev/null | grep -a -q -F -- "$QUIT_ON"; then
      sleep "${QUIT_GRACE:-3}"; pkill -f "qemu-system-x86_64 -smp"; break
    fi
    pgrep -f "qemu-system-x86_64 -smp" >/dev/null || { [ -s $S/run.log ] && break; }
  done ) &
timeout ${SECS:-7200} qemu-system-x86_64 -smp ${CPUS:-2} -cdrom /out/ekern/phorepoc-ekern.iso -serial stdio -display none \
    -drive file=$S/boot.img,format=raw,if=ide,index=0 -audiodev none,id=snd0 -device AC97,audiodev=snd0 \
    -boot d -no-reboot -m ${MEM:-1024} 2>&1 | stdbuf -oL head -c ${LOGMAX:-4000000} > $S/run.log || true
wait
rm -f $S/boot.img
