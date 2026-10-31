#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# build_iap_sis.sh -- the In-App Purchase client stand-in (iapclientapi.dll, os/src/phorepoc/iapclient) as a downloadable SIS:
# package "IAP", uid 0x200345C8, version 3.23.7 (what an app declaring a dependency on IAP >= 3.23.7 expects), depending on
# the "Qt" package (0x2001E61C >= 4.7.3; make_sis.py adds the Symbian^3 platform dependency, 0x20022E6D). Built on the Qt 4.8.7 that build_qt_sis.sh left in
# <build>/qt48 (its host moc and qt-symbian headers); Qt itself is not rebuilt.
#
# Stages (default: all, in order):
#   moc     copy the sources to <build>/qt48/iap-src and run the host moc over iapclient.h
#   build   iapclientapi.dll (PHOREPOC_QT48=1 ARM_VFP=1)
#   sis     <build>/qt48/iap-3.23.7.sis
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
PDK="${PHOREPOC_PDK:-/pdk}"
CORE="${PHOREPOC_CORE:-$HERE/../../..}"
B="${PHOREPOC_BUILD:-/build}"
Q="$B/qt48"
SRC="$Q/iap-src"
SYM="$Q/qt-symbian"

stage_moc() {
  rm -rf "$SRC"; mkdir -p "$SRC"
  cp -r "$CORE/os/src/phorepoc/iapclient/." "$SRC/"
  local qi="$SYM/include"
  "$Q/qt-host/bin/moc" -DQT_SHARED -DUNICODE "-I$qi" "-I$qi/QtCore" "$SRC/iapclient.h" -o "$SRC/moc_iapclient.cpp"
  # the contract: sizeof == 12, so the QObject-sized allocation of the consumer holds
  grep -q 'restorableProductsReceived(int,QString,IAPClient::ProductDataList)' "$SRC/moc_iapclient.cpp"
  grep -q 'purchaseCompleted(int,QString,QString)' "$SRC/moc_iapclient.cpp"
}

stage_build() {
  mkdir -p /epocroot; ln -sfn "$B/epoc32" /epocroot/epoc32
  export PHOREPOC_QT48=1 ARM_VFP=1
  cd "$PDK"
  python3 tools/emitter/build_mmp.py iapclientapi
}

stage_sis() {
  export PHOREPOC_QT48=1
  cd "$PDK"
  python3 tools/make_sis.py iapclientapi --arm --unsigned --uid 0x200345C8 --name IAP --version 3.23.7 \
    --depends 0x2001E61C,4.7.3,Qt \
    --vendor "Phorepoc" --type "${QT_SIS_TYPE:-sa}" \
    --file "$CORE/os/src/phorepoc/iapclient/LICENSE.txt=!:/resource/IAP/LICENSE.txt" --out "$Q/iap-3.23.7.sis"
}

STAGES="${*:-moc build sis}"
for s in $STAGES; do echo "=== iap: $s"; "stage_$s"; done
