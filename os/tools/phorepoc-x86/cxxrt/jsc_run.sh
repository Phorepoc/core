#!/usr/bin/env bash
# jsc_run.sh [target] -- host wrapper for build_jsc.sh (container with repo, build dir, the wkdata (/wk) and wkvol (/wv) volumes). CONFIGURE_ONLY=1 stops after cmake.
cd "$(dirname "$0")/../../.."; R="$(pwd -W)"
MSYS_NO_PATHCONV=1 docker.exe run --rm -e CONFIGURE_ONLY="${CONFIGURE_ONLY:-}" -e JOBS="${JOBS:-}" -v "$R":/repo -v "$R/.phorepoc-x86-build":/out -v wkdata:/wk -v wkvol:/wv phorepoc-x86 bash /repo/tools/phorepoc-x86/cxxrt/build_jsc.sh "$@"
