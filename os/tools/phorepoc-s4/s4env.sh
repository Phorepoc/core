#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# s4env.sh -- the S^4 build's environment, sourced by every script here. They run INSIDE the toolchain image with the pdk, core
# and the exported Symbian sources mounted as `phe` mounts them (PHOREPOC_BUILD, PHOREPOC_SYMBIAN, PHOREPOC_OS_ROOT) and with
# core's Qt 4.8 recipe built first (qt-host, qt-symbian, qt-src under <build>/qt48: os/tools/phorepoc-qt48/build_qt_sis.sh).
#   Q    <build>/qt48   : the S^4 trees (hb-src, hb-symbian, qtm-src, qte-src, hsrv-src, hs-src, epoc32 exports, *-zdata.txt)
#   S10  the 2010 release sources (oss.FCL.sf.* checkouts)
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
B="${PHOREPOC_BUILD:?PHOREPOC_BUILD}"
Q="$B/qt48"
S10="${PHOREPOC_SYMBIAN:?PHOREPOC_SYMBIAN}/symbian-2010"
export PHOREPOC_QT48=1 PHOREPOC_S4=1
# the S^3 export tree the qmake runs see as /epocroot/epoc32
s4_epocroot() { mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32; export EPOCROOT=/epocroot/; }
# s4_export <repo> <dir under $Q> <marker file>: a plain copy of a release checkout (the checkout is never written)
s4_export() {
  [ -f "$Q/$2/$3" ] && return 0
  [ -d "$S10/$1" ] || { echo "s4env: $S10/$1 is missing (phe source export -a)"; exit 1; }
  mkdir -p "$Q/$2"; cp -r "$S10/$1/." "$Q/$2/"
}
[ -f "$Q/qt-symbian/bin/qmake" ] || { echo "s4env: Qt 4.8 is not built ($Q/qt-symbian): run os/tools/phorepoc-qt48/build_qt_sis.sh first"; exit 1; }
