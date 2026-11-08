#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# qtm_configure_symbian.sh -- Symbian^4 stage S4.5 (docs/phorepoc/symbian4-build.md): Qt Mobility 1.0.3 (the release's
# oss.FCL.sf.mw.qtmobility) configured for Symbian the way the MCL build does it: NO configure run -- the checked-in
# config.pri + staticconfig.pri, an in-source qmake (staticconfig.pri: "shadow builds are not supported for symbian").
# Then qmake GENERATES the bld.inf + one .mmp per .pro next to each .pro (the same route as Qt and Orbit).
#
#   bash tools/phorepoc-s4/qtm_configure_symbian.sh        (host side; runs itself in Docker)
#
# The modules: what the S^4 homescreen and homescreensrv use (their .pro files: MOBILITY = serviceframework,
# publishsubscribe, systeminfo) -- config.pri's mobility_modules cut to those three on our copy.
#
# Out: .phorepoc-s4-build/qtm-src (LF export, in-source build: Makefiles, mmps, moc_*.cpp beside the .pro files).
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_export oss.FCL.sf.mw.qtmobility qtm-src qtmobility.pro

SRC=$Q/qtm-src
QMAKE=$Q/qt-symbian/bin/qmake
mkdir -p /epocroot
ln -sfn $B/epoc32 /epocroot/epoc32
export EPOCROOT=/epocroot/
cd "$SRC"

# 1. the modules (config.pri is the MCL build's "configure output")
sed -i 's/^mobility_modules = .*/mobility_modules = serviceframework publishsubscribe systeminfo/' config.pri
grep '^mobility_modules' config.pri

# 2. qmake -> bld.inf + mmps (in-source). Qt's qconfig.pri (QT_VERSION... -- qtmobility.pro refuses "Qt 4.6 or higher"
# without it) is found through QT_BUILD_TREE in a .qmake.cache (qt_config.prf); an INSTALLED Qt would have it at
# $$[QT_INSTALL_DATA]/mkspecs
echo "QT_BUILD_TREE = $Q/qt-symbian" > .qmake.cache
"$QMAKE" -cache "$SRC/.qmake.cache" -spec $Q/qt-symbian/mkspecs/symbian-sbsv2 -r "$SRC/qtmobility.pro" > qmake.log 2>&1 || { tail -40 qmake.log; exit 1; }
tail -15 qmake.log

# 2b. the module header dirs clients include (<QtServiceFramework/...>, CONFIG += mobility): what the MCL build's
# qtmobilityheaders.flm does -- the package's own bin/syncheaders per module, into include/Qt<Module>
# (syncheaders writes only the class-name forwarders, `#include "qservicemanager.h"`: the module's public headers go
# beside them, as in the SDK's epoc32/include/mw/Qt<Module>)
for m in ServiceFramework:serviceframework PublishSubscribe:publishsubscribe SystemInfo:systeminfo Mobility:global; do
  perl bin/syncheaders "include/Qt${m%%:*}" "src/${m#*:}" > /dev/null 2>&1 || true
  mkdir -p "include/Qt${m%%:*}"
  for h in src/${m#*:}/*.h; do case "$h" in *_p.h) ;; *) cp -f "$h" "include/Qt${m%%:*}/" ;; esac; done
done
echo "module headers: $(find include -type f | wc -l)"

# 3. the generated sources (moc) the SBSv2 build makes before compiling, with the host tools (as hb_configure_symbian.sh)
# (a sub-project in the same directory as another gets Makefile.<name>; common.pri puts the moc output under
# build/Release/<target>/moc, which the mmp names)
for mk in $(find "$SRC/src" -name "Makefile*"); do
  d=$(dirname "$mk")
  ls "$d"/*.mmp >/dev/null 2>&1 || continue
  sed -i -e 's#/epocroot//*epoc32//*bin/#'"$Q"'/qt-symbian/bin/#g' \
         -e 's#/epocroot/epoc32/include/Qt#'"$Q"'/qt-symbian/include/Qt#g' \
         -e 's#/epocroot/epoc32/mkspecs/#'"$Q"'/qt-src/mkspecs/#g' "$mk"
  ( cd "$d" && make -s -f "$(basename "$mk")" mocables compiler_rcc_make_all > "mocables-$(basename "$mk").log" 2>&1 ) \
    || { echo "${mk#$SRC/}: mocables FAILED"; tail -3 "$d/mocables-$(basename "$mk").log"; }
done
for m in $(find "$SRC/src" -name "*.mmp"); do
  d=$(dirname "$m")
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
  done
  echo "${m#$SRC/}: $(grep -c 'moc_' "$m") moc sources"
done
