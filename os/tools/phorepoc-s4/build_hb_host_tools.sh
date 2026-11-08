#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# build_hb_host_tools.sh -- Symbian^4 S4.4 (docs/phorepoc/symbian4-build.md): Orbit's HOST tools, which the Symbian
# build runs to precompute theme data (configure.py's "Building host tools" step: --host-qmake-bin/--host-make-bin):
#   hbbincssmaker   the theme's CSS -> hbdefault.cssbin (the binary stylesheets + the GLOBAL LAYOUT PARAMETERS the theme
#                   server loads into its shared cache; without it Orbit's var(hb-param-...) sizes stay unresolved)
#   hbthemeindexer  the icon theme's index (the server can build it at run time too)
# They link a host QtCore/QtGui/QtSvg/QtXml: built here in the Qt/Embedded host tree of build_qt_host.sh.
#
#   bash tools/phorepoc-s4/build_hb_host_tools.sh        (host side; runs itself in Docker)
#
# Out: .phorepoc-s4-build/hb-host/{hbbincssmaker,hbthemeindexer,docml2bin}; .phorepoc-s4-build/hb-themes/hbdefault.cssbin
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
S4=$Q
QH=$S4/qt-host
HB=$S4/hb-src
OUT=$S4/hb-host
mkdir -p "$OUT" "$S4/hb-themes"
# the same guard flags as the host moc/rcc/uic (Qt 4 vs modern GCC: null-this checks, object lifetimes), added
# through CXX so each Makefile keeps its own CXXFLAGS (-fPIC ...). QT_QCHAR_CONSTRUCTOR: Orbit is built with it
# (configure.py), and Qt's qbase.pri adds it on Symbian only -- the host Qt must have the same QString API.
# QT_NO_INOTIFY: Qt 4 keeps glibc's inotify_event (a flexible array member) in a QMap, which modern GCC refuses
QFIX='CXX=g++ -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-strict-aliasing -fpermissive -DQT_QCHAR_CONSTRUCTOR -DQT_NO_INOTIFY'
[ -d "$S4/freetype-2.3.9" ] || tar -xjf "$S4/freetype-2.3.9.tar.bz2" -C "$S4"
FTINC="-idirafter $S4/freetype-2.3.9/include"
QFIX="$QFIX $FTINC"
QFIXC="CC=gcc $FTINC"
cd "$QH"
for m in corelib xml network gui svg; do
  [ -f "$QH/lib/.built-$m" ] && continue
  rm -rf "src/$m/.pch"				# a precompiled header made with other flags is refused
  for g in .obj .moc .uic .rcc; do mkdir -p "src/$m/$g/release-shared-emb-x86_64"; done	# qmake made these once
  make -C "src/$m" -j"$(nproc)" "$QFIX" "$QFIXC" > "$OUT/qt-$m.log" 2>&1 || { echo "host Qt $m FAILED"; tail -20 "$OUT/qt-$m.log"; exit 1; }
  touch "$QH/lib/.built-$m"
done
echo "host Qt: $(ls "$QH"/lib/libQt*.so.4 2>/dev/null | xargs -n1 basename | tr '\n' ' ')"

# the tools: their .pro files include hbcommon.pri and read HB_SOURCE_DIR etc. from a qmake cache -- a HOST one
# (the Symbian .qmake.cache names EPOCROOT paths and the symbian spec)
cat > "$OUT/.qmake.cache" <<EOF
HB_SOURCE_DIR = $HB
HB_BUILD_DIR = $OUT
HB_BIN_DIR = $OUT
HB_LIB_DIR = $OUT
HB_INCLUDE_DIR = $OUT/include
HB_FEATURES_DIR = $OUT
HB_PLUGINS_DIR = $OUT
HB_RESOURCES_DIR = $OUT
HB_TRANSLATIONS_DIR = $OUT
HB_DOC_DIR = $OUT
DEFINES += HB_GESTURE_FW HB_EFFECTS QT_QCHAR_CONSTRUCTOR
CONFIG += release host_tools
include($HB/mkspecs/hb_functions.prf)
EOF
[ -e "$OUT/include" ] || ln -s "$S4/hb-symbian/include" "$OUT/include"
for t in hbthemeindexer hbbincssmaker docml2bin; do
  mkdir -p "$OUT/build-$t"
  ( cd "$OUT/build-$t" && "$QH/bin/qmake" -cache "$OUT/.qmake.cache" "$HB/src/hbtools/$t/$t.pro" -after "QMAKE_CXXFLAGS-=-Werror" "QMAKE_CXXFLAGS+=-w -std=gnu++98" > qmake.log 2>&1 \
    && make -j"$(nproc)" "$QFIX" > make.log 2>&1 ) || { echo "$t FAILED"; tail -25 "$OUT/build-$t/make.log" 2>/dev/null; exit 1; }
  f=$(find "$OUT/build-$t" "$OUT" -maxdepth 2 -type f -name "$t" -perm -u+x | head -1)
  [ -n "$f" ] && cp -f "$f" "$OUT/$t" && echo "$t: built"
done

# hb.pro's cssbinmaker rule: the hbdefault theme's CSS -> the binary the theme server maps at start.
# NOT FOR THE ROM: the cssbin is a raw dump of the shared-memory structures (qptrdiff offsets, HbVector...), so
# it has the HOST's pointer width -- this LP64 one crashes an x32 HbSharedCache::offset. Real Orbit ran the tool on a
# 32-bit host. The theme server parses the CSS at run time without it, so it is written as
# .cssbin.lp64 and hb_zdata.py (which takes only hbdefault.cssbin) leaves it out. The ROM one: ./ph s4 cssbin (made on the target).
LD_LIBRARY_PATH="$QH/lib" "$OUT/hbbincssmaker" -i "$HB/src/hbcore/resources/themes/style/hbdefault" \
  -o "$S4/hb-themes/hbdefault.cssbin.lp64" > "$OUT/cssbin.log" 2>&1 || { echo "hbbincssmaker FAILED"; tail -10 "$OUT/cssbin.log"; exit 1; }
echo "hbdefault.cssbin.lp64: $(stat -c %s "$S4/hb-themes/hbdefault.cssbin.lp64") bytes (host-only, see above)"
