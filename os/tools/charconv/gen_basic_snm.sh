#!/bin/bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
#
# Regenerates os/data/charconv/basic.snm -- the character-set standard-names / MIB-enum file that Symbian's charconv.iby
# ships as Z:\resource\charconv\basic.snm -- from the upstream charconv source (tools/basic.txt, tools/snmtool.pl).
# Without it Versit's charset lookups crash on UTF-8 vCards. Needs perl and python3; the 2010 tool uses defined(@array),
# which modern perl rejects, so the Perl module is patched in a temporary copy.
#   usage: gen_basic_snm.sh <path to charconv_fw/tools of the fetched upstream tree>
set -e
here="$(cd "$(dirname "$0")" && pwd)"
S="${1:?path to .../charconvfw/charconv_fw/tools}"
T=$(mktemp -d)
cp "$S/PARSER.PM" "$T/PARSER.pm"; cp "$S/WRITER.PM" "$T/WRITER.pm"; cp "$S/snmtool.pl" "$S/basic.txt" "$T/"
sed -i 's/defined(@PARSER::nextNonEmptyStrippedDownLine) && //' "$T/PARSER.pm"
cp "$here/uidcrc" "$here/uidcrc.py" "$T/"
(cd "$T" && PATH="$T:$PATH" perl -I. snmtool.pl basic.txt basic.snm)
cp "$T/basic.snm" "$here/../../data/charconv/basic.snm"
