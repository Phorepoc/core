#!/usr/bin/env bash
# drun.sh <script-in-container> [args] -- run a cxxrt script in phorepoc-x86 with the repo, the build dir and the R: scratch (/wk)
cd "$(dirname "$0")/../../.."; R="$(pwd -W)"
MSYS_NO_PATHCONV=1 docker.exe run --rm -v "$R":/repo -v "$R/.phorepoc-x86-build":/out -v wkdata:/wk -e W=/wk/lsc phorepoc-x86 bash "$@"
