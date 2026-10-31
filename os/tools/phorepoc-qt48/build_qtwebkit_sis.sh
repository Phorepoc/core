#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# build_qtwebkit_sis.sh -- QtWebKit (the WebKit that ships inside Qt 4.8.7, src/3rdparty/webkit) for ARM as a downloadable
# SIS: package "QtWebKit", uid 0x200267C2, version 4.8.1 (what a Qt browser app's SIS declares; 4.8.1 satisfies >= 4.8.0),
# depending on the "Qt" package (0x2001E61C >= 4.7.3). WebKit is LGPL-2/BSD: the SIS carries LICENSE.LGPL, the Qt exception
# and a source offer. Built on the same <build>/qt48 tree as build_qt_sis.sh (its configure + mocables stages already ran
# qmake over the WebKit .pro files). Same container/mounts as build_qt_sis.sh.
#
# Stages (default: all, in order):
#   mocables   host moc/rcc outputs of JavaScriptCore, WebCore and WebKit/qt (the Makefiles' own `mocables` targets), mirrored
#              from the shadow tree beside the generated mmps
#   build      jscore.lib, webcore.lib (static), then qt48_webkit = QtWebKit.dll with the frozen
#              Source/WebKit/qt/symbian/eabi/QtWebKitu.def ordinals    (PHOREPOC_QT48=1 ARM_VFP=1)
#   sis        <build>/qt48/qtwebkit-4.8.1.sis
#
# First-step configuration (recorded in docs/phorepoc/qt48-build.md): JavaScriptCore runs as an interpreter (JIT, YARR JIT and
# the assembler are off); no exceptions (QT_NO_EXCEPTIONS, as for Qt itself); video/WebGL/web audio are off as in the mmp.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
PDK="${PHOREPOC_PDK:-/pdk}"
B="${PHOREPOC_BUILD:-/build}"
Q="$B/qt48"
SRC="$Q/qt-src"
SYM="$Q/qt-symbian"
W=3rdparty/webkit/Source

stage_mocables() {
  mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32; export EPOCROOT=/epocroot/
  for t in moc rcc uic; do cp -f "$Q/qt-host/bin/$t" "$SYM/bin/$t"; done
  [ -e "$SRC/include" ] || ln -s "$SYM/include" "$SRC/include"
  for m in JavaScriptCore WebCore WebKit/qt; do
    local d="$SYM/src/$W/$m" s="$SRC/src/$W/$m"
    ( cd "$d" && make -s mocables > mocables.log 2>&1 && make -s compiler_rcc_make_all >> mocables.log 2>&1 ) || { echo "$m: mocables FAILED"; tail -5 "$d/mocables.log"; exit 1; }
    for g in .moc .rcc .uic; do [ -d "$d/$g" ] && mkdir -p "$s/$g" && cp -ru "$d/$g/." "$s/$g/"; done
  done
}

stage_build() {
  export PHOREPOC_QT48=1 ARM_VFP=1
  cd "$PDK"
  for k in jscore webcore qt48_webkit; do python3 tools/emitter/build_mmp.py "$k"; done
}

stage_sis() {
  export PHOREPOC_QT48=1
  cd "$PDK"
  local extra="--file $HERE/QTWEBKIT-SOURCE-OFFER.txt=!:/resource/QtWebKit/SOURCE-OFFER.txt --file $SRC/LICENSE.LGPL=!:/resource/QtWebKit/LICENSE.LGPL"
  [ -f "$SRC/LGPL_EXCEPTION.txt" ] && extra="$extra --file $SRC/LGPL_EXCEPTION.txt=!:/resource/QtWebKit/LGPL_EXCEPTION.txt"
  python3 tools/make_sis.py qt48_webkit --arm --unsigned --uid 0x200267C2 --name QtWebKit --version 4.8.1 \
    --depends 0x2001E61C,4.7.3,Qt --vendor "The Qt Project (packaged by Phorepoc)" --type "${QT_SIS_TYPE:-sa}" $extra --out "$Q/qtwebkit-4.8.1.sis"
}

STAGES="${*:-mocables build sis}"
for s in $STAGES; do echo "=== qtwebkit: $s"; "stage_$s"; done
