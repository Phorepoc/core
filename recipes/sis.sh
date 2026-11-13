#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
#
# sis.sh -- the pdk's package.yml runs it after the components (package.conf): the browser frontend as
# $PHOREPOC_BUILD/sis/phebrowser-<arch>.sis -- the app, its resources and registration, its icon. No engine (the engines are
# their own packages) and no Qt. Package UID = the app's UID3 (0xE7016B20), the same as the ROM stub the images carry
# (z:\system\install\phebrowser_stub.sis), and the ROM-upgrade flag: on Phorepoc it installs over the image's copy (uninstalling
# it brings that back); on a phone without one it is an ordinary install. The version is the release's.
set -eu
ARCH=${PHOREPOC_ARCH:-x32}
# ARM: self-signed (a stock Symbian^3 installer refuses an unsigned package; a self-signed one asks once); x32: unsigned (Phorepoc only)
if [ "$ARCH" = arm ]; then KEY=phebrowser_arm; AFL=--arm; else KEY=phebrowser; AFL=--unsigned; fi
REPO=$(cd "$(dirname "$0")/.." && pwd)
LIC=
for f in "$REPO/LICENSES/Apache-2.0.txt" "$REPO/NOTICE"; do
  LIC="$LIC --file $f=!:/resource/phebrowser/licences/$(basename "$f")"
done
mkdir -p "$PHOREPOC_BUILD/sis"
python3 "$PHOREPOC_PDK/tools/make_sis.py" $KEY $AFL --rom-upgrade --version "${PHOREPOC_PACKAGE_VERSION:-1.0.0}" \
  --name "Phorepoc Browser" --vendor Phorepoc $LIC \
  --out "$PHOREPOC_BUILD/sis/phebrowser-$ARCH.sis"
