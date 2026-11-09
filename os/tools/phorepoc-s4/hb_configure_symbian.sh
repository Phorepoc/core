#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# hb_configure_symbian.sh -- Symbian^4 stage S4.4 (docs/phorepoc/symbian4-build.md): configure Orbit (the release's
# oss.FCL.sf.mw.hb) for Symbian the way its configure.py does for `--platform symbian`, then let qmake GENERATE the
# bld.inf + one .mmp per .pro (the same route as Qt, qt_configure_symbian.sh).
#
#   bash tools/phorepoc-s4/hb_configure_symbian.sh        (host side; runs itself in Docker)
#
# configure.py, bin/synchb.py are Python 2 and the container has no python2: this script does their work --
#   1. hb_install.prf + .qmake.cache (configure.py main(): the values below, symbian branch)
#   2. the wrapper headers in <build>/include (synchb.py; only `except E, e` is Python 2 -> fixed on a copy)
#   3. src/hbcore/resources/resources.qrc (resourcifier.py, Python-3 clean, with configure.py's excludes)
#   4. qmake -r -spec symbian-sbsv2 hb.pro with the Qt-for-Symbian qmake (qt-symbian/bin/qmake)
# configure.py's Symbian compile tests cannot run (no sbsv2); their answers for OUR board headers, by hand:
#   sgimagelite  no  (no VSW_GSW_SGIMAGELITE)
#   tchunkcreateinfo NO -- the header has TChunkCreateInfo::SetReadOnly, but on the 2010 kernel a read-only chunk is
#                    read-only for its CREATOR too: the theme server faulted zeroing its own cache chunk
#                    (HbSharedMemoryWrapper::create), and it is system-critical, so the whole OS went down
#   touchfeedback no (no ETouchFeedbackLongPress)       wsrenderorientation no (S^3 WSERV binary)
#   pointerregion no (no SetPointerAcceptanceRegion)    openvg no
#   qtmobility YES (2026-11-09): HB_HAVE_QT_MOBILITY = HbSystemInfo (the device dialog server publishes the network mode/status/strength,
#                    the status-bar indicators read it); the stock Qt Mobility 1.2.x (the "QtMobility" package: mobility.prf from
#                    <build>/qmake-features, the headers from qtm12-src)
#
# Out: .phorepoc-s4-build/hb-src (LF export; resources.qrc is written into it), .phorepoc-s4-build/hb-symbian (the
# build dir: .qmake.cache, hb_install.prf, include/, generated bld.inf + mmps).
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
s4_export oss.FCL.sf.mw.hb hb-src hb.pro
# (moc 4.8; the Qt 4.7 S^4 tree stays as it is). Unset = the original Qt 4.7 build in .phorepoc-s4-build.

SRC=$Q/hb-src
OUT=$Q/hb-symbian
QMAKE=$Q/qt-symbian/bin/qmake
mkdir -p /epocroot "$OUT"
ln -sfn $B/epoc32 /epocroot/epoc32
export EPOCROOT=/epocroot/
cd "$OUT"

# 1. hb_install.prf, then .qmake.cache = the same values + build/source/export dirs + CONFIG + the functions include
# (configure.py: ConfigFile.set_value -> "KEY = value", add_value -> "KEY += value"; prefix = $${EPOCROOT}epoc32)
P='$${EPOCROOT}epoc32'
cat > hb_install.prf <<EOF
HB_INSTALL_DIR = $P
HB_BIN_DIR = $P/tools
HB_LIB_DIR = $P/lib
HB_DOC_DIR = $P/doc
HB_INCLUDE_DIR = $P/include/mw/hb
HB_PLUGINS_DIR = \$\${EPOCROOT}resource/qt/plugins/hb
HB_FEATURES_DIR = $P/tools/qt/mkspecs/features/symbian
HB_RESOURCES_DIR = $P/resources
HB_TRANSLATIONS_DIR = $P/translations
HB_PLUGINS_EXPORT_DIR = $P/winscw/c/resource/qt/plugins/hb
DEFINES += HB_GESTURE_FW
DEFINES += HB_EFFECTS
DEFINES += HB_HAVE_QT_MOBILITY
QMAKE_RPATHDIR += \$\${HB_LIB_DIR}
EOF
{ cat hb_install.prf
  echo "HB_BUILD_DIR = $OUT"
  echo "HB_SOURCE_DIR = $SRC"
  echo "HB_EXPORT_DIR = $P/include/mw/hb/%1/%2"
  echo "HB_RESTRICTED_EXPORT_DIR = $P/include/mw/hb/%1/restricted/%2"
  for part in tests performance localization tutorials; do echo "HB_NOMAKE_PARTS += $part"; done
  echo "CONFIG += release"
  echo "DEFINES += QT_QCHAR_CONSTRUCTOR"
  echo "include($SRC/mkspecs/hb_functions.prf)"
} > .qmake.cache

# 2. wrapper headers (the generated Makefiles/mmps name them under the SOURCE dir: <src>/include -> <build>/include)
[ -e "$SRC/include" ] || ln -s "$OUT/include" "$SRC/include"
#    synchb.py (synchb.py; the only Python 2 construct is `except IOError, e:`)
sed 's/except \([A-Za-z]*\), e:/except \1 as e:/' "$SRC/bin/synchb.py" > /tmp/synchb.py
python3 /tmp/synchb.py -i "$SRC" -o "$OUT"
# 3. the Qt resource collection of hbcore's resources (configure.py's argument list, symbian platform)
( cd "$SRC/src/hbcore/resources" && python3 "$SRC/bin/resourcifier.py" -i . -o resources.qrc \
    --exclude "*distribution.policy.s60" --exclude "*readme.txt" --exclude "*.pr?" --exclude "*.qrc" --exclude "*~" \
    --exclude "variant/*" --exclude "*hbdefault.cssbin" --exclude "*macx*" --exclude "*unix*" --exclude "*win32*" )
echo "headers: $(find include -type f | wc -l)   resources.qrc: $(grep -c '<file>' "$SRC/src/hbcore/resources/resources.qrc") files"

# 4. qmake -> bld.inf + mmps
export QMAKEFEATURES=$Q/qmake-features
"$QMAKE" -spec $Q/qt-symbian/mkspecs/symbian-sbsv2 -cache "$OUT/.qmake.cache" -r "$SRC/hb.pro" > qmake.log 2>&1 || { tail -40 qmake.log; exit 1; }
tail -15 qmake.log
# 5. the generated sources the SBSv2 build makes before compiling (moc/rcc; qmake's shadow build writes them next to
# the mmp, whose SOURCEPATH is `.`), with the host tools of build_qt_host.sh (as qt_mocables.sh)
for mmp in $(find "$OUT/src" -name "*.mmp"); do
  d=$(dirname "$mmp")
  [ -f "$d/Makefile" ] || continue
  # the rules name the tools where a Qt INSTALL puts them ($EPOCROOT/epoc32/bin): ours are in the Qt build tree
  # and its Qt headers where the INSTALL puts them ($EPOCROOT/epoc32/include/QtXxx): ours are the build tree's
  sed -i -e "s#/epocroot//*epoc32//*bin/#$Q/qt-symbian/bin/#g" \
         -e "s#/epocroot/epoc32/include/Qt#$Q/qt-symbian/include/Qt#g" \
         -e "s#/epocroot/epoc32/mkspecs/#$Q/qt-src/mkspecs/#g" "$d/Makefile"
  # the Qt Mobility headers (QtSystemInfo / QtPublishSubscribe ...): the stock 1.2.x tree's, not the Qt build tree's
  sed -i -E "s#$Q/qt-symbian/include/Qt(ServiceFramework|PublishSubscribe|SystemInfo|Mobility)#$Q/qtm12-src/include/Qt\1#g" "$d/Makefile"
  # the mmp's SOURCEPATHs (and the moc/rcc outputs) are relative to the SOURCE dir of its .pro: stage it there
  cp -f "$mmp" "$SRC/${d#$OUT/}/"
  # ... with the application resources qmake generated beside it (<app>.rss, <app>_reg.rss)
  for r in "$d"/*.rss "$d"/*.loc; do [ -f "$r" ] && cp -f "$r" "$SRC/${d#$OUT/}/"; done
  # moc outputs of an earlier configure are NOT regenerated by make when only DEFINES changed (the header is not newer): a stale moc file lost
  # the Q_PRIVATE_SLOTs behind `#ifdef HB_HAVE_QT_MOBILITY` (HbSignalIndicator never got its mode)
  rm -f "$d"/moc_*.cpp "$d"/qrc_*.cpp   # (the copies in the source dir are overwritten below)
  ( cd "$d" && make -s mocables compiler_rcc_make_all > mocables.log 2>&1 ) || { echo "${d#$OUT/}: mocables FAILED"; tail -3 "$d/mocables.log"; }
  s="$SRC/${d#$OUT/}"
  # (Qt 4.8's qmake writes the moc/rcc outputs into the BUILD dir; the mmp's SOURCEPATH is the source dir -> copy them there)
  for g in "$d"/moc_*.cpp "$d"/*.moc "$d"/qrc_*.cpp; do [ -f "$g" ] && cp -f "$g" "$s/"; done
  # a moc_X.cpp a source #includes itself (Q_PRIVATE_SLOT needs the private class) must not be compiled again:
  # qmake's Symbian generator lists it anyway
  for inc in $(grep -rhoE '#include "moc_[A-Za-z0-9_]+\.cpp"' --include=*.cpp "$s" | grep -oE 'moc_[A-Za-z0-9_]+\.cpp' | sort -u); do
    sed -i "/^SOURCE[[:space:]]*$inc\$/d" "$s/$(basename "$mmp")"
  done
  echo "$(basename "$mmp"): $(ls "$s" | grep -c '^moc_') moc, $(ls "$s" | grep -c '^qrc_') qrc"
done
# 6. no C++ exceptions on this board (as Qt): Orbit's try/catch are out-of-memory guards
python3 $HERE/hb_noexcept.py "$SRC/src"
# 6b. Hb finds its typeface metrics on every drive, not only z: (they ship in the Catalogue package: hb_drives.py)
python3 $HERE/hb_drives.py "$SRC/src"
# 7. (was: the theme server's heap raised 10 -> 64 MB while it had no binary CSS. Since S4.4b the cssbin is made on
# the target (make_cssbin.sh, ./ph s4 cssbin) and the server keeps the release's 10 MB)
