#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# build_translations.sh -- Symbian^4 S4.6 (docs/phorepoc/symbian4-build.md): the S^4 UI's English texts.
# Orbit apps show logical text ids (hbTrId("txt_homescreen_title_...")) that a QTranslator turns into text; the
# release ships every S^4 translation as Qt Linguist sources (oss.FCL.sf.mw.qttranslations: data/tsfiles.zip,
# 5814 .ts, 114 per language) which its build compiled with lrelease into z:\resource\qt\translations.
# Here: Qt's own lrelease built on the host tree, over the English (_en) set; -idbased: Orbit looks texts up by id
# (hbTrId = qtTrId), the .ts messages carry id= attributes.
#
#   bash tools/phorepoc-s4/build_translations.sh        (host side; runs itself in Docker)
#
# Out: .phorepoc-s4-build/qm/<name>_en.qm (s4_zdata.py adds them)
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
S4=$Q
QH=$S4/qt-host
OUT=$S4/qm
# lrelease (tools/linguist/lrelease: QtCore + QtXml), the same modern-GCC guard flags as the other host tools
QFIX='CXX=g++ -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-strict-aliasing -fpermissive -DQT_QCHAR_CONSTRUCTOR -DQT_NO_INOTIFY'
# the English (_en) set of the release's translations (qttranslations data/tsfiles.zip), then the texts the code gained
# after that snapshot (ts_extras.py: <name>_extra.ts with only the ids <name>_en.ts lacks)
mkdir -p "$OUT/ts"
python3 - "$S10/oss.FCL.sf.mw.qttranslations/data/tsfiles.zip" "$OUT/ts" <<'PY'
import os, sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
for n in z.namelist():
    if n.endswith("_en.ts"):
        open(os.path.join(sys.argv[2], os.path.basename(n)), "wb").write(z.read(n))
PY
python3 "$HERE/ts_extras.py"
LR=$QH/tools/linguist/lrelease     # (inside the host tree: its .qmake.cache names the source/build trees)
if [ ! -f "$OUT/lrelease" ]; then
  mkdir -p "$LR"
  ( cd "$LR" && "$QH/bin/qmake" "$S4/qt-src/tools/linguist/lrelease/lrelease.pro" -after "QMAKE_CXXFLAGS+=-w -std=gnu++98" \
      "DESTDIR=$OUT" > qmake.log 2>&1 && make -j"$(nproc)" "$QFIX" > make.log 2>&1 ) \
    || { echo "lrelease FAILED"; tail -20 "$LR/make.log" 2>/dev/null; exit 1; }
  echo "lrelease: built"
fi
n=0
for ts in "$OUT"/ts/*_en.ts; do
  extra="${ts%_en.ts}_extra.ts"; [ -f "$extra" ] || extra=
  LD_LIBRARY_PATH=$QH/lib "$OUT/lrelease" -silent -idbased "$ts" $extra -qm "$OUT/$(basename "${ts%.ts}").qm" >> "$OUT/lrelease.log" 2>&1 \
    || { echo "lrelease: $(basename "$ts") FAILED"; continue; }
  n=$((n + 1))
done
echo "translations: $n .qm -> ${OUT#/repo/}"
