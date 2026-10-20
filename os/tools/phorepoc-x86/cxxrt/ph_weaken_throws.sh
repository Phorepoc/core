#!/usr/bin/env bash
# ph_weaken_throws.sh <libstdc++.a> -- IN-CONTAINER: make every std::__throw_* DEFINITION in the archive weak, so ph_throw.cpp's
# strong ones (linked as an object of the image) replace them (build_libstdcxx.sh runs it; idempotent). Only the defining members
# are rewritten: objcopy -W on the whole archive also turns the undefined references weak, which resolve to 0 in an image
# without ph_throw.o.
set -eu
A=$(readlink -f "$1"); T=$(mktemp -d); cd "$T"
${CROSS:-}nm -A "$A" 2>/dev/null | awk '$2 == "T" && $3 ~ /^_ZSt[0-9]+__throw_/ { split($1, p, ":"); print p[2], $3 }' | sort -u > defs.txt
[ -s defs.txt ] || { echo "ph_weaken_throws: no strong __throw_ definitions left in $A"; rm -rf "$T"; exit 0; }
for m in $(cut -d' ' -f1 defs.txt | sort -u); do
  ${CROSS:-}ar x "$A" "$m"
  ${CROSS:-}objcopy $(awk -v m="$m" '$1 == m { printf "-W %s ", $2 }' defs.txt) "$m"
  ${CROSS:-}ar r "$A" "$m"
done
echo "ph_weaken_throws: weakened $(wc -l < defs.txt) definitions in $(cut -d' ' -f1 defs.txt | sort -u | wc -l) members of $A"
rm -rf "$T"
