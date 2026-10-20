#!/usr/bin/env bash
# drun-arm.sh <script-in-container> [args] -- drun2.sh for the PinePhone (ARMv7) web build: image phorepoc-web-arm
# (tools/phorepoc-arm/web/Dockerfile), its own volumes (wkdata-arm -> /wk, wkvol-arm -> /wv) and output dir .phorepoc-web-arm (/out),
# ARCH=arm for every build_*.sh. The x32 trees are never touched.
cd "$(dirname "$0")/../../.."; R="$(pwd -W)"; mkdir -p .phorepoc-web-arm
MSYS_NO_PATHCONV=1 docker.exe run --rm --cpus="${CPUS:-10}" -e JOBS="${JOBS:-10}" -e ARCH=arm -v "$R":/repo -v "$R/.phorepoc-web-arm":/out \
  -v wkdata-arm:/wk -v wkvol-arm:/wv -e W=/wk/lsc phorepoc-web-arm bash "$@"
