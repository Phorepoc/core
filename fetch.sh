#!/bin/sh
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# fetch.sh -- fetch every package of sources.lock at its pinned commit from the Phorepoc forks into symbian/<tree>/<package>.
# Shallow (one commit each, fetched by sha); safe to re-run: a package already at its commit is skipped.
# SOURCES_URL=<base> uses another place than https://github.com/Phorepoc; PHOREPOC_SYMBIAN=<dir> another target than symbian/.
set -eu
cd "$(dirname "$0")"
BASE="${SOURCES_URL:-https://github.com/Phorepoc}"
DEST="${PHOREPOC_SYMBIAN:-symbian}"
mkdir -p "$DEST"
grep -v '^#' sources.lock | while IFS="$(printf '\t')" read -r tree pkg commit; do
	[ -n "$tree" ] || continue
	# the `kernelhwsrv` tree (package `.`) is the kernelhwsrv fork checked out at symbian/kernelhwsrv
	if [ "$pkg" = "." ]; then d="$DEST/$tree"; repo="oss.FCL.sf.os.kernelhwsrv"; else d="$DEST/$tree/$pkg"; repo="$pkg"; fi
	if [ -d "$d/.git" ] && [ "$(git -C "$d" rev-parse HEAD 2>/dev/null)" = "$commit" ]; then
		echo "have  $tree/$pkg"
		continue
	fi
	echo "fetch $tree/$pkg @ ${commit%"${commit#????????}"}"
	mkdir -p "$d"
	[ -d "$d/.git" ] || git -C "$d" init -q
	git -C "$d" fetch -q --depth 1 "$BASE/$repo.git" "$commit"
	git -C "$d" checkout -q --force FETCH_HEAD
done
