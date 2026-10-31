#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# build_qt_sis.sh -- Qt 4.8.7 for ARM as an INSTALLABLE SIS, from the public Qt source. Qt is LGPL, so it is never in the
# ROM image: it is built and packaged separately (package 0x2001E61C "Qt" 4.8.7, which is also what a Qt app's SIS depends on).
#
# Runs INSIDE the toolchain image with the pdk, core and the adaptations mounted as `ph` mounts them (PHOREPOC_PDK, PHOREPOC_CORE,
# PHOREPOC_OS_ROOT, PHOREPOC_SYMBIAN, PHOREPOC_ADAPTATIONS, PHOREPOC_BUILD) and the include tree exported (`ph source export -a`).
#
#   docker run --rm -v <pdk>:/pdk -v <core>:/core -v <core>/os:/os:ro -v <symbian>:/symbian:ro -v <build>:/build \
#       -v <soc>:/adapt/0:ro -v <board>:/adapt/1:ro -w /pdk \
#       -e PHOREPOC_PDK=/pdk -e PHOREPOC_CORE=/core -e PHOREPOC_OS_ROOT=/os -e PHOREPOC_SYMBIAN=/symbian -e PHOREPOC_BUILD=/build \
#       -e PHOREPOC_ADAPTATIONS=/adapt/0:/adapt/1 ghcr.io/phorepoc/web bash /os/tools/phorepoc-qt48/build_qt_sis.sh [stage ...]
#
# Stages (default: all, in order; each is skipped when its output exists):
#   fetch      the 4.8.7 tarball from download.qt.io (SHA-256 checked) -> <build>/qt48/qt-src
#   host       qmake/moc/rcc/uic for the build machine (a shadow build) -> <build>/qt48/qt-host      (~10 min)
#   configure  `configure -xplatform symbian-sbsv2` + qmake: a bld.inf and one .mmp per module -> <build>/qt48/qt-symbian   (~35 min)
#   mocables   moc/rcc/uic outputs the generated mmps compile                                         (~5 min)
#   stage      the mmps and our Qt patches (qt-patches/) into the source tree
#   build      the ARM images: qt48_core qt48_gui qt48_network qt48_svg qt48_xml qt48_sql (PHOREPOC_QT48=1 ARM_VFP=1)   (long)
#   sis        <build>/qt48/qt-4.8.7.sis: the DLLs as package 0x2001E61C "Qt"
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
PDK="${PHOREPOC_PDK:-/pdk}"
B="${PHOREPOC_BUILD:-/build}"
Q="$B/qt48"
SRC="$Q/qt-src"
HOST="$Q/qt-host"
SYM="$Q/qt-symbian"
TARBALL_URL="${QT_URL:-https://download.qt.io/archive/qt/4.8/4.8.7/qt-everywhere-opensource-src-4.8.7.tar.gz}"
TARBALL_SHA=e2882295097e47fe089f8ac741a95fef47e0a73a3f3cdf21b56990638f626ea0
MODULES="${QT_MODULES:-core gui network svg xml sql}"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$Q"

stage_fetch() {
  [ -f "$SRC/configure" ] && return 0
  local t="$Q/qt-everywhere-opensource-src-4.8.7.tar.gz"
  [ -f "$t" ] || curl -L --fail -o "$t" "$TARBALL_URL"
  echo "$TARBALL_SHA  $t" | sha256sum -c -
  mkdir -p "$SRC"
  tar -xzf "$t" -C "$SRC" --strip-components=1
}

stage_host() {
  [ -f "$HOST/.qfix" ] && return 0
  mkdir -p "$HOST"; cd "$HOST"
  export CXXFLAGS="-std=gnu++98 -fpermissive -w" CFLAGS="-w"
  if [ ! -f "$HOST/bin/qmake" ]; then
    "$SRC/configure" -opensource -confirm-license -embedded x86_64 -prefix "$HOST" -fast -release \
      -nomake examples -nomake demos -nomake docs -nomake translations -no-webkit -no-qt3support -no-phonon \
      -no-multimedia -no-script -no-scripttools -no-declarative -no-xmlpatterns -no-dbus -no-openssl -no-opengl \
      -no-glib -no-cups -no-nis -no-iconv -no-gif -no-libtiff -no-libmng -qt-zlib -qt-libpng -qt-libjpeg \
      -no-accessibility -no-sql-sqlite -no-exceptions -silent -platform linux-g++ > configure.log 2>&1 \
      || { tail -30 configure.log; exit 1; }
  fi
  # Qt 4 relies on `this`-may-be-null checks and object lifetimes that GCC >= 6 optimises away: the guard flags
  local QFIX='CXXFLAGS=-pipe -O2 -w -std=gnu++98 -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-strict-aliasing $(DEFINES)'
  # configure already built the tools without the guard flags: a uic that drops custom widgets (QFileDialogTreeView) -> clean, rebuild
  make -C src/tools/bootstrap clean >/dev/null 2>&1 || true
  for t in moc rcc uic; do make -C "src/tools/$t" clean >/dev/null 2>&1 || true; rm -f "$HOST/bin/$t"; done
  rm -rf "$SRC"/src/*/.moc "$SRC"/src/*/.rcc "$SRC"/src/*/.uic "$SYM"/src/*/.moc "$SYM"/src/*/.rcc "$SYM"/src/*/.uic 2>/dev/null || true
  make -C src/tools/bootstrap -j"$JOBS" "$QFIX" > bootstrap.log 2>&1 || { tail -30 bootstrap.log; exit 1; }
  # the tools at -O2 with GCC 13 misbehave (uic loses widget names -- a custom widget "treeView" comes out as
  # "fileDialogTreeView"; moc reports "Undefined interface" on Q_INTERFACES): build them -O0
  for t in moc rcc uic; do
    make -C "src/tools/$t" -j"$JOBS" "${QFIX/-O2/-O0}" > "$t.log" 2>&1 || { tail -30 "$t.log"; exit 1; }
  done
  touch "$HOST/.qfix"
  ls -la "$HOST/bin"
}

epocroot() {   # mkspecs/common/symbian/symbian.conf probes EPOCROOT/epoc32 (the exported include tree)
  mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32; export EPOCROOT=/epocroot/
}

stage_configure() {
  [ -f "$SYM/src/corelib/QtCore_dll.mmp" ] && return 0
  epocroot; mkdir -p "$SYM"; cd "$SYM"
  "$SRC/configure" -opensource -confirm-license -xplatform symbian-sbsv2 -platform linux-g++ \
    -no-webkit -no-phonon -no-multimedia -no-audio-backend -no-openssl -no-openvg -no-opengl -graphicssystem raster \
    -no-qt3support -no-script -no-declarative -no-xmlpatterns -svg -nomake examples -nomake demos \
    -nomake tools -nomake docs -nomake translations -no-stl -fast -release > configure.log 2>&1 \
    || { tail -40 configure.log; exit 1; }
}

stage_mocables() {
  epocroot
  mkdir -p "$SYM/bin"
  for t in moc rcc uic; do cp -f "$HOST/bin/$t" "$SYM/bin/$t"; done
  [ -e "$SRC/include" ] || ln -s "$SYM/include" "$SRC/include"
  for m in corelib gui network svg sql xml s60main; do
    local d="$SYM/src/$m"
    [ -f "$d/Makefile" ] || { echo "$m: no Makefile"; continue; }
    mkdir -p "$SRC/src/$m/.moc/release-shared" "$SRC/src/$m/.rcc/release-shared" "$SRC/src/$m/.uic/release-shared"
    mkdir -p "$d/.moc/release-shared" "$d/.rcc/release-shared" "$d/.uic/release-shared"
    ( cd "$d" && make -s mocables > mocables.log 2>&1 ) || { echo "$m: mocables FAILED"; tail -5 "$d/mocables.log"; continue; }
    ( cd "$d" && make -s compiler_rcc_make_all >> mocables.log 2>&1 ) || true
    ( cd "$d" && make -s compiler_uic_make_all >> mocables.log 2>&1 ) || true
    for g in .moc .rcc .uic; do [ -d "$SRC/src/$m/$g" ] && mkdir -p "$d/$g" && cp -ru "$SRC/src/$m/$g/." "$d/$g/"; done
  done
  # qfiledialog.cpp includes ui_qfiledialog_embedded.h, for which the Symbian Makefile has no uic rule
  local u="$SYM/src/gui/.uic/release-shared"; mkdir -p "$u" "$SRC/src/gui/.uic/release-shared"
  "$HOST/bin/uic" "$SRC/src/gui/dialogs/qfiledialog_embedded.ui" -o "$u/ui_qfiledialog_embedded.h"
  cp -f "$u/ui_qfiledialog_embedded.h" "$SRC/src/gui/.uic/release-shared/"
}

stage_stage() {
  for f in $(cd "$SYM" && find src -name "*.mmp"); do mkdir -p "$SRC/$(dirname "$f")"; cp -f "$SYM/$f" "$SRC/$f"; done
  for p in "$HERE"/qt-patches/*.patch; do
    ( cd "$SRC" && patch -p1 -N -s -r - < "$p" >/dev/null ) && echo "patched: $(basename "$p")" || echo "(already in or failed: $(basename "$p"))"
  done
  # QtGui's application-framework resource (s60framework.pri) is a bld.inf extension in qmake's SBSv2 output, not in the mmp
  local G="$SRC/src/gui/QtGui_dll.mmp"
  grep -q "s60main.rss" "$G" || printf '\nSOURCEPATH s60framework\nSTART RESOURCE s60main.rss\nTARGET s60main\nHEADER\nTARGETPATH /resource/apps\nEND\n' >> "$G"
  # the generated mmps' SYSTEMINCLUDE ../../include is qt-src/include: use configure's own include/ tree and qconfig.h/.cpp
  cp -rf "$SYM/include/." "$SRC/include/"
  cp -f "$SYM/src/corelib/global/qconfig.h" "$SYM/src/corelib/global/qconfig.cpp" "$SRC/src/corelib/global/"
  for d in $(cd "$SYM" && find src -type d \( -name .moc -o -name .rcc -o -name .uic \)); do
    mkdir -p "$SRC/$d" && cp -ru "$SYM/$d/." "$SRC/$d/"
  done
}

stage_build() {
  export PHOREPOC_QT48=1 ARM_VFP=1
  cd "$PDK"
  for m in $MODULES; do python3 tools/emitter/build_mmp.py "qt48_$m"; done
}

stage_sis() {
  export PHOREPOC_QT48=1
  cd "$PDK"
  local keys=""; for m in $MODULES; do keys="$keys qt48_$m"; done
  # store-ready: package "Qt" 4.8.7 (the name/version a Qt app's SIS depends on), the LGPL text, its exception and the source offer inside
  local extra="--file $HERE/QT-SOURCE-OFFER.txt=!:/resource/Qt/SOURCE-OFFER.txt --file $SRC/LICENSE.LGPL=!:/resource/Qt/LICENSE.LGPL"
  [ -f "$SRC/LGPL_EXCEPTION.txt" ] && extra="$extra --file $SRC/LGPL_EXCEPTION.txt=!:/resource/Qt/LGPL_EXCEPTION.txt"
  python3 tools/make_sis.py $keys --arm --unsigned --uid 0x2001E61C --name Qt --version 4.8.7       --vendor "The Qt Project (packaged by Phorepoc)" --type "${QT_SIS_TYPE:-sa}" $extra --out "$Q/qt-4.8.7${QT_SIS_TYPE:+-$QT_SIS_TYPE}.sis"
}

STAGES="${*:-fetch host configure mocables stage build sis}"
for s in $STAGES; do echo "=== qt48: $s"; "stage_$s"; done
