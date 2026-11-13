#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# hs_configure_symbian.sh -- Symbian^4 stage S4.6 (docs/phorepoc/symbian4-build.md): the S^4 homescreen (the release's
# oss.FCL.sf.app.homescreen: hsapplication, the domain model, the state plug-ins, the clock/shortcut widgets, the
# app library), qmake-generated in-source as the package's build -- the same recipe as hsrv_configure_symbian.sh.
#
#   bash tools/phorepoc-s4/hs_configure_symbian.sh        (host side; runs itself in Docker)
#
# Needs ./ph s4 hsrv-setup first (its staged qmake features + the homescreensrv platform headers).
# Out: .phorepoc-s4-build/hs-src (LF export, in-source build: bld.infs, mmps, moc output under homescreenapp/bin/tmp).
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_export oss.FCL.sf.app.homescreen hs-src homescreen.pro

S4=$Q
SRC=$S4/hs-src
QMAKE=$S4/qt-symbian/bin/qmake
mkdir -p /epocroot
ln -sfn $B/epoc32 /epocroot/epoc32
export EPOCROOT=/epocroot/

F=$S4/qmake-features
cp -f "$S4/qtm-src/features/qtservice.prf" "$F/"
export QMAKEFEATURES=$F
echo "QT_BUILD_TREE = $S4/qt-symbian" > "$SRC/.qmake.cache"

for pro in homescreen_plat/homescreen_plat.pro homescreenapp/homescreenapp.pro; do
  ( cd "$SRC/$(dirname "$pro")" && "$QMAKE" -cache "$SRC/.qmake.cache" -spec "$S4/qt-symbian/mkspecs/symbian-sbsv2" \
      -r "$SRC/$pro" > qmake.log 2>&1 ) || { echo "$pro: qmake FAILED"; tail -30 "$SRC/$(dirname "$pro")/qmake.log"; exit 1; }
  grep -i "error\|warning" "$SRC/$(dirname "$pro")/qmake.log" | grep -v "backslashes" | head -5 || true
done

# no C++ exceptions on this board (as Qt and Orbit)
python3 $HERE/hb_noexcept.py "$SRC/homescreenapp"

# the generated sources (moc, rcc) the SBSv2 build makes before compiling, with the host tools
for mk in $(find "$SRC/homescreenapp" -name "Makefile*" -not -path "*/tsrc/*" -not -path "*/examples/*"); do
  d=$(dirname "$mk")
  ls "$d"/*.mmp >/dev/null 2>&1 && grep -q "^mocables:" "$mk" || continue
  sed -i -e 's#/epocroot//*epoc32//*bin/#'"$Q"'/qt-symbian/bin/#g' \
         -e 's#/epocroot/epoc32/include/Qt#'"$Q"'/qt-symbian/include/Qt#g' \
         -e 's#/epocroot/epoc32/mkspecs/#'"$Q"'/qt-src/mkspecs/#g' "$mk"
  sed -i -E 's#'"$Q"'/qt-symbian/include/Qt(ServiceFramework|PublishSubscribe|SystemInfo|Mobility)#'"$Q"'/qtm-src/include/Qt\1#g' "$mk"
  # moc follows includes to learn the platform macros: <QState> -> qstate.h -> <QtCore/qabstractstate.h> -> qglobal.h
  # (Q_OS_SYMBIAN). The SDK's epoc32/include held QtCore/; ours is qt-symbian/include -- without it moc skipped the
  # chain silently and dropped Q_OS_SYMBIAN-guarded slots (HsLoadSceneState::handleKeyChange: "No such slot")
  grep -q 'qt-symbian/include" ' "$mk" || sed -i 's#^INCPATH\t = #INCPATH\t =  -I"'"$Q"'/qt-symbian/include" #' "$mk"
  # Orbit's docml -> binary layout compiler (CONFIG += docml2bin; the rcc'd .qrc files name its output): the host
  # build of build_hb_host_tools.sh -- its output is a QDataStream, not ABI-dependent (unlike the cssbin)
  # docml2bin = the HOST converter built on the 4.7 host Qt (the 4.8 tree has no host QtGui; it writes Hb's binary docml, not Qt ABI)
  sed -i "s#/epocroot/epoc32/tools/docml2bin#LD_LIBRARY_PATH=$Q/qt-host/lib $Q/hb-host/docml2bin#g" "$mk"   # (expanded here: make would read $Q as its own)
  dm=; grep -q "^compiler_docml2bin_make_all:" "$mk" && dm=compiler_docml2bin_make_all
  ( cd "$d" && make -s -f "$(basename "$mk")" mocables $dm compiler_rcc_make_all > "mocables-$(basename "$mk").log" 2>&1 ) \
    || { echo "${mk#$SRC/}: mocables FAILED"; tail -3 "$d/mocables-$(basename "$mk").log"; }
done
for m in $(find "$SRC/homescreenapp" -name "*_dll.mmp" -not -path "*/tsrc/*" -o -name "*_exe.mmp" -not -path "*/tsrc/*"); do
  d=$(dirname "$m")
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
  done
  echo "${m#$SRC/}: $(grep -c 'moc' "$m") moc lines"
done
