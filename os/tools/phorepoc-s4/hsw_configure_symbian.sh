#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# hsw_configure_symbian.sh -- the release's stand-alone homescreen widgets (oss.FCL.sf.app.profileswidgetplugin: the
# profile switcher, oss.FCL.sf.app.silentwidgetplugin: silent on/off), qmake-generated in-source like
# hs_configure_symbian.sh (whose staged qmake features they use: hb, mobility).
#
# Each .pro includes ../../common.pri (the platform tree it sat in): the trees go to <Q>/hsw/app/<package> next to an
# empty common.pri. Out: <Q>/hsw/app/<package> with its mmp, moc/rcc output and qmakepluginstubs.
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_epocroot
QMAKE=$Q/qt-symbian/bin/qmake
export QMAKEFEATURES=$Q/qmake-features
[ -f "$QMAKEFEATURES/hb.prf" ] || { echo "hsw_configure: run hsrv_configure_symbian.sh first (qmake features)"; exit 1; }
mkdir -p "$Q/hsw/app"
: > "$Q/hsw/common.pri"
echo "QT_BUILD_TREE = $Q/qt-symbian" > "$Q/hsw/.qmake.cache"

for pkg in profileswidgetplugin silentwidgetplugin; do
  s4_export oss.FCL.sf.app.$pkg hsw/app/$pkg $pkg.pro
  d=$Q/hsw/app/$pkg
  ( cd "$d" && "$QMAKE" -cache "$Q/hsw/.qmake.cache" -spec "$Q/qt-symbian/mkspecs/symbian-sbsv2" "$pkg.pro" > qmake.log 2>&1 ) \
    || { echo "$pkg: qmake FAILED"; tail -20 "$d/qmake.log"; exit 1; }
  grep -i "error" "$d/qmake.log" | head -5 || true
  # no C++ exceptions on this board (as Qt and Orbit)
  python3 "$HERE/hb_noexcept.py" "$d"
  mk=$d/Makefile
  sed -i -e 's#/epocroot//*epoc32//*bin/#'"$Q"'/qt-symbian/bin/#g' \
         -e 's#/epocroot/epoc32/include/Qt#'"$Q"'/qt-symbian/include/Qt#g' \
         -e 's#/epocroot/epoc32/mkspecs/#'"$Q"'/qt-src/mkspecs/#g' "$mk"
  sed -i -E 's#'"$Q"'/qt-symbian/include/Qt(ServiceFramework|PublishSubscribe|SystemInfo|Mobility)#'"$Q"'/qtm-src/include/Qt\1#g' "$mk"
  grep -q 'qt-symbian/include" ' "$mk" || sed -i 's#^INCPATH\t = #INCPATH\t =  -I"'"$Q"'/qt-symbian/include" #' "$mk"
  sed -i "s#/epocroot/epoc32/tools/docml2bin#LD_LIBRARY_PATH=$Q/qt-host/lib $Q/hb-host/docml2bin#g" "$mk"
  dm=; grep -q "^compiler_docml2bin_make_all:" "$mk" && dm=compiler_docml2bin_make_all
  ( cd "$d" && make -s mocables $dm compiler_rcc_make_all > mocables.log 2>&1 ) \
    || { echo "$pkg: mocables FAILED"; tail -3 "$d/mocables.log"; exit 1; }
  m=$(ls "$d"/*_dll.mmp)
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
  done
  [ -f "$d/qmakepluginstubs/$pkg.qtplugin" ] || { echo "$pkg: no qmakepluginstubs/$pkg.qtplugin"; exit 1; }
  echo "$pkg: $(basename "$m"), $(ls "$d" | grep -c '^moc_') moc, $(ls "$d" | grep -c '^qrc_') qrc"
done
