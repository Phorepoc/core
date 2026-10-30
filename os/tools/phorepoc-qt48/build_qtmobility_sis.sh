#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# build_qtmobility_sis.sh -- Qt Mobility 1.2 for ARM as a downloadable SIS (package "QtMobility", uid 0x2002AC89, the uid/name
# a Qt Mobility app's dependency declares, so the installer sees the dependency as met). LGPL 2.1 sources (upstream tag v1.2.0),
# built on the Qt 4.8.7 that build_qt_sis.sh produced (same <build>/qt48 tree) and packaged separately -- never in the ROM.
# Runs inside the same toolchain image / mounts as build_qt_sis.sh.
#
# Stages (default: all, in order; each is skipped when its output exists):
#   fetch      git clone --branch v1.2.0 github.com/qtproject/qt-mobility -> <build>/qt48/qtm12-git
#   configure  features/config.pri.symbian3 + the host qmake -> a .mmp per DLL (modules AND plug-ins) in <build>/qt48/qtm12-src
#   build      the ARM images (PHOREPOC_QT48=1 ARM_VFP=1); QTM_KEYS overrides the list
#   sis        <build>/qt48/qtmobility-1.2.1.sis (package 0x2002AC89 "QtMobility" 1.2.1)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
PDK="${PHOREPOC_PDK:-/pdk}"
B="${PHOREPOC_BUILD:-/build}"
Q="$B/qt48"
SRC="$Q/qtm12-src"
SYM="$Q/qt-symbian"
TAG="${QTM_TAG:-v1.2.0}"
# module DLLs first (link order), then the plug-ins. Not built yet: qtmessaging (needs the email API headers), qtsensors_sym,
# qtcontacts_symbian, qtlandmarks_symbian (host moc: "Undefined interface" on Q_INTERFACES), qtmultimediakit (+ecam/mmf/m3u engines).
KEYS="${QTM_KEYS:-qtsensors qtbearer qtserviceframework qtpublishsubscribe qtsysteminfo qtlocation qtcontacts qtversit qtorganizer qtversitorganizer qtfeedback qtgallery qtsensors_generic qtfeedback_mmk qtfeedback_symbian qtgeoservices_nokia qtorganizer_symbian qtcontacts_serviceactionmanager qtversit_backuphandler qtversit_vcardpreserver}"

stage_fetch() {
  [ -d "$Q/qtm12-git/src" ] && return 0
  git clone -q --depth 1 --branch "$TAG" https://github.com/qtproject/qt-mobility "$Q/qtm12-git"
}

stage_configure() {
  [ -f "$SRC/src/bearer/QtBearer_dll.mmp" ] && return 0
  [ -f "$SYM/bin/qmake" ] || { echo "run build_qt_sis.sh (fetch host configure mocables) first"; exit 1; }
  mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32; export EPOCROOT=/epocroot/
  rm -rf "$SRC"; cp -r "$Q/qtm12-git" "$SRC"; cd "$SRC"
  cp -f features/config.pri.symbian3 config.pri            # the Symbian^3 MCL build's own configure output
  { echo "QT_BUILD_TREE = $SYM"; echo "QT_CONFIG += declarative"; } > .qmake.cache   # the QML plug-ins (plugins/declarative)
  "$SYM/bin/qmake" -cache "$SRC/.qmake.cache" -spec "$SYM/mkspecs/symbian-sbsv2" -r "$SRC/qtmobility.pro" > qmake.log 2>&1 \
    || { tail -40 qmake.log; exit 1; }
  # module header dirs (<QtSensors/...>, <qsensorplugin.h>): the package's own syncheaders, as the MCL build does
  for m in $(ls src | grep -v -E "^(3rdparty|global|s60installs|harmattaninstalls|meegoinstalls|mobilitysimulator)$"); do
    perl bin/syncheaders "include/Qt$m" "src/$m" > /dev/null 2>&1 || true
  done
  mkdir -p include/QtMobility; cp -f src/global/*.h include/QtMobility/ 2>/dev/null || true
  # <cmath>/<cstdlib> would reach the host toolchain's newlib headers: the C headers (the ROM's stdapis) instead
  grep -rl --include=*.cpp --include=*.h -E '#include <c(math|stdlib|stdio|string|time|assert)>' src plugins 2>/dev/null | while read -r f; do
    sed -i -E 's/#include <c(math|stdlib|stdio|string|time|assert)>/#include <\1.h>/' "$f"
  done
  # moc/rcc outputs the mmps compile (host tools of the Qt build)
  local minc=""; for d in "$SRC"/include/Qt*; do minc="$minc -I\"$d\""; done
  for mk in $(find src plugins -name "Makefile*"); do
    local d; d=$(dirname "$mk"); ls "$d"/*.mmp >/dev/null 2>&1 || continue
    sed -i -e "s#/epocroot//*epoc32//*bin/#$SYM/bin/#g" -e "s#/epocroot/epoc32/include/Qt#$SYM/include/Qt#g" \
           -e "s#/epocroot/epoc32/mkspecs/#$Q/qt-src/mkspecs/#g" -e "s#^INCPATH	 =#INCPATH	 = $minc#" "$mk"
    ( cd "$d" && make -s -f "$(basename "$mk")" mocables compiler_rcc_make_all > "mocables-$(basename "$mk").log" 2>&1 ) \
      || echo "$d: mocables FAILED (that DLL is not built)"
  done
  # Mobility's cpp files #include their own moc outputs: those are not separate sources
  for m in $(find src plugins -name "*.mmp"); do
    local d; d=$(dirname "$m")
    for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
      sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
    done
  done
}

stage_build() {
  export PHOREPOC_QT48=1 ARM_VFP=1
  cd "$PDK"
  for k in $KEYS; do python3 tools/emitter/build_mmp.py "$k"; done
}

stage_sis() {
  export PHOREPOC_QT48=1
  cd "$PDK"
  # the LGPL text, its exception and a source offer travel inside; plug-in stubs (\resource\qt\plugins\<type>\<name>.qtplugin) are added
  # per plug-in by the --file list below (TODO verify the stub format against a real package before the first catalogue release)
  local extra="--file $HERE/QT-SOURCE-OFFER.txt=!:/resource/QtMobility/SOURCE-OFFER.txt --file $SRC/LICENSE.LGPL=!:/resource/QtMobility/LICENSE.LGPL"
  [ -f "$SRC/LGPL_EXCEPTION.txt" ] && extra="$extra --file $SRC/LGPL_EXCEPTION.txt=!:/resource/QtMobility/LGPL_EXCEPTION.txt"
  # package identity = what Qt Mobility apps declare: (0x2002AC89), 1, 2, 0, {"QtMobility"}; 1.2.1 satisfies a >= 1.2.0 dependency
  python3 tools/make_sis.py $KEYS --arm --unsigned --uid 0x2002AC89 --name QtMobility --version 1.2.1 \
    --vendor "The Qt Project (packaged by Phorepoc)" --type "${QT_SIS_TYPE:-sa}" $extra --out "$Q/qtmobility-1.2.1.sis"
}

STAGES="${*:-fetch configure build sis}"
for s in $STAGES; do echo "=== qtmobility: $s"; "stage_$s"; done
