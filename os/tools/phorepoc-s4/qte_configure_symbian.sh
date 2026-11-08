#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# qte_configure_symbian.sh -- Symbian^4 stage S4.5 (docs/phorepoc/symbian4-build.md): the Qt extensions (the release's
# oss.FCL.sf.mw.qtextensions) the S^4 homescreen and homescreensrv link -- qthighway (xqserviceutil, xqserviceipc,
# xqservice) and qtmobileextensions (xqsettingsmanager, xqsysinfo, xqutils, xqkeycapture, xqsystemtoneservice).
# In-source qmake as the package's own build (no configure step), then moc (as qtm_configure_symbian.sh).
#
#   bash tools/phorepoc-s4/qte_configure_symbian.sh        (host side; runs itself in Docker)
#
# Out: .phorepoc-s4-build/qte-src (LF export, in-source build: Makefiles, bld.inf, mmps, moc_*.cpp).
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_export oss.FCL.sf.mw.qtextensions qte-src qtextensions.pro

SRC=$Q/qte-src
QMAKE=$Q/qt-symbian/bin/qmake
mkdir -p /epocroot
ln -sfn $B/epoc32 /epocroot/epoc32
export EPOCROOT=/epocroot/

for pro in qthighway/qthighway.pro qtmobileextensions/qtmobileextensions.pro; do
  ( cd "$SRC/$(dirname "$pro")" && "$QMAKE" -spec $Q/qt-symbian/mkspecs/symbian-sbsv2 -r "$SRC/$pro" \
      > qmake.log 2>&1 ) || { echo "$pro: qmake FAILED"; tail -30 "$SRC/$(dirname "$pro")/qmake.log"; exit 1; }
  tail -5 "$SRC/$(dirname "$pro")/qmake.log"
done

# no C++ exceptions on this board (as Qt and Orbit): keycapture's one try/catch (around QT_TRAP_THROWING, which
# leaves instead of throwing here) -> hb_noexcept.py's rewrite; its handler logs e.what(), and the rewrite makes `e`
# an int in a never-run block, so that call goes too
python3 $HERE/hb_noexcept.py "$SRC/qtmobileextensions/src/keycapture"
sed -i 's/<< e\.what();/<< "(exception)";/' "$SRC/qtmobileextensions/src/keycapture/targetwrapper.cpp"

# the generated sources (moc) the SBSv2 build makes before compiling, with the host tools
for mk in $(find "$SRC/qthighway" "$SRC/qtmobileextensions/src" -name Makefile); do
  d=$(dirname "$mk")
  ls "$d"/*.mmp >/dev/null 2>&1 || continue
  sed -i -e 's#/epocroot//*epoc32//*bin/#'"$Q"'/qt-symbian/bin/#g' \
         -e 's#/epocroot/epoc32/include/Qt#'"$Q"'/qt-symbian/include/Qt#g' \
         -e 's#/epocroot/epoc32/mkspecs/#'"$Q"'/qt-src/mkspecs/#g' "$mk"
  ( cd "$d" && make -s mocables compiler_rcc_make_all > mocables.log 2>&1 ) || { echo "${d#$SRC/}: mocables FAILED"; tail -3 "$d/mocables.log"; }
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    for m in "$d"/*.mmp; do sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"; done
  done
  for m in "$d"/*.mmp; do echo "${m#$SRC/}: $(grep -c "moc" "$m") moc lines (moc output: under build/)"; done
done
