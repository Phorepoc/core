#!/usr/bin/env bash
# drun2.sh <script-in-container> [args] -- like drun.sh but image phorepoc-jsc (cmake/ninja/ruby/ICU-dev/x32 libs) + the wkvol volume (/wv)
cd "$(dirname "$0")/../../.."; R="$(pwd -W)"
MSYS_NO_PATHCONV=1 docker.exe run --rm -e JOBS="${JOBS:-4}" -v "$R":/repo -v "$R/.phorepoc-x86-build":/out -v wkdata:/wk -v wkvol:/wv -e W=/wk/lsc phorepoc-jsc bash "$@"
