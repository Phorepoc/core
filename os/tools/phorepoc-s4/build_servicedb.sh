#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
# build_servicedb.sh -- Symbian^4 S4.6 (docs/phorepoc/symbian4-build.md): the Qt service framework's ROM database.
# Qt Mobility's database server (qsfwdatabasemanagerserver) copies z:\private\2002ac7f\QtServiceFramework_4.7_system.db
# to C: at first use; the release's build made it with the HOST tool servicedbgen (qtmobility/tools/servicedbgen),
# which each `CONFIG += qtservice` component's bld.inf runs (the qt/qtservice extension: `servicedbgen -tarmv5
# [-i] add <descriptor>`, the descriptor exported to /epoc32/data/z/private/2002AC7F/des). Same here, over every
# descriptor our S^4 packages export. The database is SQLite: not ABI-dependent.
#
# The host Qt (build_qt_host.sh) was configured without SQLite: the tool is built here directly (its .pro's sources
# + Qt's SQLite driver and SQLite itself linked statically, as the .pro's CONFIG += static / QTPLUGIN += qsqlite),
# against the host QtCore + a host QtSql built now from the configured tree.
#
#   bash tools/phorepoc-s4/build_servicedb.sh        (host side; runs itself in Docker)
#
# Out: .phorepoc-s4-build/sfw/epoc32/data/z/private/2002AC7F/QtServiceFramework_4.7_system.db (s4_zdata.py adds it)
set -eu
. "$(dirname "${BASH_SOURCE[0]}")/s4env.sh"
S4=$Q
QH=$S4/qt-host
QS=$S4/qt-src
QTM=$S4/qtm-src
OUT=$S4/sfw
mkdir -p "$OUT/obj"
# the same guard flags as build_hb_host_tools.sh (Qt 4 vs modern GCC)
QFIX='CXX=g++ -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-strict-aliasing -fpermissive -DQT_QCHAR_CONSTRUCTOR -DQT_NO_INOTIFY'

# 1. host QtSql (its Makefile is configure's; no built-in driver)
if [ ! -f "$QH/lib/.built-sql" ]; then
  rm -rf "$QH/src/sql/.pch"
  for g in .obj .moc .uic .rcc; do mkdir -p "$QH/src/sql/$g/release-shared-emb-x86_64"; done
  make -C "$QH/src/sql" -j"$(nproc)" "$QFIX" > "$OUT/qt-sql.log" 2>&1 || { echo "host QtSql FAILED"; tail -20 "$OUT/qt-sql.log"; exit 1; }
  touch "$QH/lib/.built-sql"
fi

# 2. servicedbgen (+ the SQLite driver as a static plug-in, + SQLite)
CXXF="-O1 -w -std=gnu++98 -fno-delete-null-pointer-checks -fno-lifetime-dse -fno-strict-aliasing -fpermissive
      -DQT_QCHAR_CONSTRUCTOR -DQT_NO_DEBUG -DQT_SHARED -DQT_STATICPLUGIN
      -DSERVICE_XML_GENERATOR -DQT_SFW_SERVICEDATABASE_GENERATE -DQT_SFW_SERVICEDATABASE_USE_SECURITY_TOKEN
      -DQT_BUILD_INTERNAL -DQT_SYMBIAN_EMULATOR_SUPPORTS_PERPROCESS_WSD
      -I$QH/include -I$QH/include/QtCore -I$QH/include/QtSql -I$QH/src/corelib/global
      -I$QTM/src/global -I$QTM/src/serviceframework -I$QS/src/3rdparty/sqlite -I$QS/src/sql/drivers/sqlite"
cd "$OUT/obj"
"$QH/bin/moc" -DQT_BUILD_INTERNAL -DSERVICE_XML_GENERATOR -I"$QTM/src/global" -I"$QTM/src/serviceframework" -I"$QH/include" -I"$QH/include/QtCore" "$QTM/tools/servicedbgen/servicedbgen.cpp" -o servicedbgen.moc
# (servicedatabase.cpp includes its moc output inside the QtMobility namespace)
"$QH/bin/moc" -I"$QTM/src/global" -I"$QTM/src/serviceframework" -I"$QH/include" -I"$QH/include/QtCore" -I"$QH/include/QtSql" "$QTM/src/serviceframework/servicedatabase_p.h" -o moc_servicedatabase_p.cpp
"$QH/bin/moc" "$QS/src/sql/drivers/sqlite/qsql_sqlite.h" -o moc_qsql_sqlite.cpp
SRCS="$QTM/tools/servicedbgen/servicedbgen.cpp $QTM/src/serviceframework/servicemetadata.cpp
      $QTM/src/serviceframework/qserviceinterfacedescriptor.cpp $QTM/src/serviceframework/dberror.cpp
      $QTM/src/serviceframework/servicedatabase.cpp $QTM/src/serviceframework/qservicefilter.cpp
      $QS/src/sql/drivers/sqlite/qsql_sqlite.cpp moc_qsql_sqlite.cpp
      $QS/src/plugins/sqldrivers/sqlite/smain.cpp"
objs=
for s in $SRCS; do
  o=$(basename "${s%.cpp}").o
  g++ $CXXF -I. -c "$s" -o "$o" >> build.log 2>&1 || { echo "servicedbgen: $s FAILED"; tail -15 build.log; exit 1; }
  objs="$objs $o"
done
gcc -O1 -w -DNDEBUG -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_COMPLETE -DSQLITE_THREADSAFE=0 -I"$QH/include/QtCore" -I"$QH/src/corelib/global" -c "$QS/src/3rdparty/sqlite/sqlite3.c" -o sqlite3.o
g++ -o "$OUT/servicedbgen" $objs sqlite3.o -L"$QH/lib" -lQtSql -lQtCore -lpthread -ldl >> build.log 2>&1 \
  || { echo "servicedbgen link FAILED"; tail -15 build.log; exit 1; }
echo "servicedbgen: built"

# 3. the descriptors (every qt/qtservice extension in the S^4 packages' generated bld.infs) -> the database, in
#    the order the build would run them (-i where the component asked for it)
STAGE=$OUT/epoc32/data/z/private/2002AC7F
rm -rf "$OUT/epoc32"
mkdir -p "$STAGE/des"
n=0
for inf in $(find "$S4/hs-src" "$S4/hsrv-src" "$S4/qte-src" "$S4/qtm-src" -name "bld.inf*" -not -path "*/tsrc/*" -not -path "*/examples/*" | sort); do
  d=$(dirname "$inf")
  awk '/START EXTENSION qt\/qtservice/{e=1} e&&/QTSERVICE_DESCRIPTOR/{printf "%s ", $3} e&&/QTSERVICE_INITIALIZE/{print ($3=="" ? "-" : $3); e=0}' "$inf" |
  while read -r des init; do
    src=$(find "$d" -name "$des" -not -path "*/tsrc/*" | head -1)
    [ -n "$src" ] || { echo "servicedb: $des (from ${inf#$S4/}) not found"; continue; }
    cp -f "$src" "$STAGE/des/"
    [ "$init" = "-" ] && init=
    EPOCROOT=$OUT/ LD_LIBRARY_PATH=$QH/lib "$OUT/servicedbgen" -tarmv5 $init add "$STAGE/des/$des" > "$OUT/add-$des.log" 2>&1
    grep -qi "error\|fail" "$OUT/add-$des.log" && { echo "servicedb: $des:"; head -5 "$OUT/add-$des.log"; }
    echo "  + $des"
  done
done
# 3b. qmake 4.8 drops the `START EXTENSION qt/qtservice` blocks from some generated bld.infs (the command handlers' caapphandlerplugin /
#     catapphandlerplugin / caurlhandlerplugin: no ICommandHandler in the database -> the app library's "Unable to open application
#     (error code 1)"). The release's own bld.infs declare them with an empty QTSERVICE_INITIALIZE: add the three by name
for des in caapphandlerplugin.xml catapphandlerplugin.xml caurlhandlerplugin.xml; do
  [ -f "$STAGE/des/$des" ] && continue
  src=$(find "$Q/hsrv-src" -name "$des" -not -path "*/tsrc/*" | head -1)
  [ -n "$src" ] || { echo "servicedb: $des not found in hsrv-src"; continue; }
  cp -f "$src" "$STAGE/des/"
  EPOCROOT=$OUT/ LD_LIBRARY_PATH=$QH/lib "$OUT/servicedbgen" -tarmv5 add "$STAGE/des/$des" > "$OUT/add-$des.log" 2>&1
  grep -qi "error\|fail" "$OUT/add-$des.log" && { echo "servicedb: $des:"; head -5 "$OUT/add-$des.log"; }
  echo "  + $des (declared by the release's cahandler bld.infs)"
done
ls -la "$STAGE"/*.db
