#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# build_qtcomponents_sis.sh -- Qt Quick Components for Symbian 1.1 for ARM as a downloadable SIS (package "Qt Quick components for
# Symbian", uid 0x200346DE, 1.1.3: the uid/name a Qt Quick app's dependency declares, so the installer sees it as met). BSD-licensed
# sources (Nokia's qt-components, src/symbian, as mirrored in the nemomobile/qt-components history: GitHub master dropped the
# Symbian code, tree da3bea3c is the last that has it), built on the Qt 4.8.7 + Qt Mobility 1.2 that build_qt_sis.sh and
# build_qtmobility_sis.sh left in <build>/qt48 -- never in the ROM.
#
# Stages (default: all, in order; each is skipped when its output exists):
#   fetch      git clone nemomobile/qt-components, check out da3bea3c -> <build>/qt48/qtcomp-git
#   configure10 the same for the 1.0 plug-ins (the tree one release back) -> <build>/qt48/qtcomp10-src
#   configure  qt-patches/qtcomp/* + the host qmake -> a .mmp per plug-in DLL in <build>/qt48/qtcomp-src
#   build      the ARM images (PHOREPOC_QT48=1 ARM_VFP=1)
#   sis        <build>/qt48/qtcomponents-1.1.3.sis (package 0x200346DE)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
PDK="${PHOREPOC_PDK:-/pdk}"
B="${PHOREPOC_BUILD:-/build}"
Q="$B/qt48"
SRC="$Q/qtcomp-src"
SYM="$Q/qt-symbian"
QTM="$Q/qtm12-src"
REV="${QTCOMP_REV:-da3bea3c532acbb7e975fb49951b64d0270e4d5d}"
KEYS="${QTCOMP_KEYS:-qtcomponentsplugin symbianplugin_1_1_3 symbianextrasplugin_1_1_3 symbianplugin_1_0 symbianextrasplugin_1_0 qtcomponentsplugin_1_0}"
REV10="${QTCOMP_REV10:-ff4b5c76^}"   # the last tree before "switching to version 1.1": the Components 1.0 plug-ins (com.nokia.symbian 1.0)
SRC10="$Q/qtcomp10-src"

stage_fetch() {
  [ -d "$Q/qtcomp-git/src/symbian" ] && return 0
  [ -d "$Q/qtcomp-git/.git" ] || git clone -q --no-checkout https://github.com/nemomobile/qt-components.git "$Q/qtcomp-git"
  git -C "$Q/qtcomp-git" checkout -q "$REV"
}

# configure_tree <dir> <mmp that marks it done> <git rev> <apply our patches: 1|0>: the tree of one Components release -> mmps
configure_tree() {
  local SRC=$1 done_mmp=$2 gitrev=$3 patches=$4
  [ -f "$SRC/$done_mmp" ] && return 0
  [ -f "$SYM/bin/qmake" ] || { echo "run build_qt_sis.sh (fetch host configure mocables) first"; exit 1; }
  [ -d "$QTM/features" ] || { echo "run build_qtmobility_sis.sh first (the MOBILITY qmake feature)"; exit 1; }
  mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32; export EPOCROOT=/epocroot/
  rm -rf "$SRC"; mkdir -p "$SRC"; git -C "$Q/qtcomp-git" archive "$gitrev" | tar -x -C "$SRC"; cd "$SRC"
  # upstream blobs are CRLF: the patched files go LF first so the (LF) patches apply
  if [ "$patches" = 1 ]; then   # (the 1.1 tree's files; the 1.0 tree has neither)
    sed -i 's/\r$//' src/symbian/sdeclarativesharedstatusbar.cpp src/symbian/sdeclarativescreen_p.cpp
    for p in "$HERE"/qt-patches/qtcomp/*.patch; do patch -p1 -s < "$p"; done
  fi
  # what upstream's ./configure -symbian writes: symbian3 + mobility, NOT symbian_internal (the Avkon/CDL internals are not public)
  cp -f "$HERE/qt-patches/qtcomp/qt-components-config.prf" features/qt-components-config.prf
  { echo "QT_BUILD_TREE = $SYM"; echo "QT_CONFIG += declarative"; } > .qmake.cache
  export QMAKEFEATURES="$QTM/features"
  for p in src/symbian/symbian.pro src/symbian/extras/extras.pro src/components/components.pro; do
    # run in the .pro's own directory: qmake writes the mmps/bld.inf/Makefile into the cwd, where the registry looks for them
    ( cd "$SRC/$(dirname "$p")" && "$SYM/bin/qmake" -cache "$SRC/.qmake.cache" -spec "$SYM/mkspecs/symbian-sbsv2" "CONFIG+=mobility" "$SRC/$p" \
        > "$SRC/qmake-$(basename "$p").log" 2>&1 ) || { tail -30 "$SRC/qmake-$(basename "$p").log"; exit 1; }
  done
  grep -rl --include=*.cpp --include=*.h -E '#include <c(math|stdlib|stdio|string|time|assert)>' src 2>/dev/null | while read -r f; do
    sed -i -E 's/#include <c(math|stdlib|stdio|string|time|assert)>/#include <\1.h>/' "$f"
  done
  # moc/rcc outputs the mmps compile (host tools of the Qt build); qmake already wrote the real Makefile as Makefile.Makefile
  local qi="$Q/qt-src/include"; local minc=" -I$qi -I$qi/QtCore -I$qi/QtGui -I$qi/QtNetwork -I$qi/QtDeclarative -I$qi/QtScript -I$qi/QtSvg"
  for mk in $(find src/symbian src/components -name "Makefile*"); do
    local d; d=$(dirname "$mk"); ls "$d"/*.mmp >/dev/null 2>&1 || continue
    sed -i -e "s#/epocroot//*epoc32//*bin/#$SYM/bin/#g" -e "s#/epocroot/epoc32/include/Qt#$SYM/include/Qt#g" \
           -e "s#/epocroot/epoc32/mkspecs/#$Q/qt-src/mkspecs/#g" -e "s#^INCPATH	 =#INCPATH	 = $minc#" "$mk"
    ( cd "$d" && make -s -f "$(basename "$mk")" mocables compiler_rcc_make_all > "mocables-$(basename "$mk").log" 2>&1 ) \
      || echo "$mk: mocables skipped/failed (the bld.inf stub Makefile is expected to)"
  done
  for m in $(find src/symbian src/components -name "*.mmp"); do
    local d; d=$(dirname "$m")
    for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$d" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
      sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$m"
    done
  done
}

stage_configure() { configure_tree "$SRC" src/symbian/symbianplugin_1_1_3_dll.mmp "$REV" 1; }
stage_configure10() { configure_tree "$SRC10" src/symbian/symbianplugin_1_0_dll.mmp "$REV10" 0; }

stage_build() {
  export PHOREPOC_QT48=1 ARM_VFP=1
  cd "$PDK"
  for k in $KEYS; do python3 tools/emitter/build_mmp.py "$k"; done
}

stage_sis() {
  export PHOREPOC_QT48=1
  cd "$PDK"
  local s="$SRC/src" stubs="$Q/qtcomp-stubs" extra="--file $SRC/header.BSD=!:/resource/QtComponents/LICENSE.BSD"; mkdir -p "$stubs"
  # the files of upstream's s60installs/qtcomponents_1_1.pkg (1.1 parts): QML + qmldir + the .qtplugin stub per import dir
  put() {   # put <source file> <target path under !:/resource/qt>
    extra="$extra --file $1=!:/resource/qt/$2"
  }
  stub() {  # stub <target dir> <plug-in name>
    printf 'This file is a Qt plugin stub file. The real Qt plugin is located in /sys/bin.\n' > "$stubs/$2.qtplugin"
    put "$stubs/$2.qtplugin" "$1/$2.qtplugin"
  }
  dir() {   # dir <source dir> <target dir>: every QML/JS file and the qmldir
    for f in "$1"/*.qml "$1"/*.js "$1"/qmldir; do [ -f "$f" ] && put "$f" "$2/$(basename "$f")"; done
  }
  dir "$s/symbian" imports/com/nokia/symbian.1.1;   stub imports/com/nokia/symbian.1.1 symbianplugin_1_1_3
  dir "$s/symbian/extras" imports/com/nokia/extras.1.1; stub imports/com/nokia/extras.1.1 symbianextrasplugin_1_1_3
  dir "$s/components" imports/Qt/labs/components.1.1; stub imports/Qt/labs/components.1.1 qtcomponentsplugin
  # Components 1.0 (`import com.nokia.symbian 1.0`): qtcomponents_1_0.pkg's layout -- the unversioned import dirs. LGPL 2.1 sources (the
  # 1.0 release was not yet BSD): the licence text rides along.
  local s10="$SRC10/src"
  dir "$s10/symbian" imports/com/nokia/symbian;   stub imports/com/nokia/symbian symbianplugin_1_0
  dir "$s10/symbian/extras" imports/com/nokia/extras; stub imports/com/nokia/extras symbianextrasplugin_1_0
  dir "$s10/components" imports/Qt/labs/components; stub imports/Qt/labs/components qtcomponentsplugin_1_0
  extra="$extra --file $SRC10/LICENSE.LGPL=!:/resource/QtComponents/LICENSE.LGPL-1.0"
  # Qt.labs.components.native: the same QML (not the shared-status-bar pair) with its own qmldir, plus a stub of the symbian plug-in
  for f in "$s"/symbian/*.qml "$s"/symbian/*.js; do
    case "$(basename "$f")" in StatusBarDefault.qml|StatusBarShared.qml) continue;; esac
    put "$f" "imports/Qt/labs/components/native/$(basename "$f")"
  done
  put "$s/symbian/native/qmldir" imports/Qt/labs/components/native/qmldir
  put "$stubs/symbianplugin_1_1_3.qtplugin" imports/Qt/labs/components/native/symbianplugin_1_1_3.qtplugin
  # package identity = what Qt Quick apps declare: (0x200346DE), 1, 1, 0; needs Qt >= 4.7.4 (0x2001E61C) and QtMobility >= 1.2.0 (0x2002AC89)
  python3 tools/make_sis.py $KEYS --arm --unsigned --uid 0x200346DE --name "Qt Quick components for Symbian" --version 1.1.3 \
    --depends 0x2001E61C,4.7.4,Qt --depends 0x2002AC89,1.2.0,QtMobility \
    --vendor "Nokia (packaged by Phorepoc)" --type "${QT_SIS_TYPE:-sa}" $extra --out "$Q/qtcomponents-1.1.3.sis"
}

STAGES="${*:-fetch configure configure10 build sis}"
for s in $STAGES; do echo "=== qtcomponents: $s"; "stage_$s"; done
