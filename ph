#!/usr/bin/env bash
# ph -- the Phorepoc task runner: one place for the build commands (a Makefile without make).
#
#   ./ph help                   this list
#   ./ph <command> [args]       see below; environment options pass through (NET=1 FONTS=dejavu ... ./ph rom)
#
# Every command runs the real script (tools/...) in the toolchain image ghcr.io/phorepoc/base, with
#   core -> /core    PHOREPOC_OS_ROOT -> /os (read-only)    PHOREPOC_SYMBIAN -> /symbian (read-only)    PHOREPOC_BUILD -> /build
# mounted; inside a container already (/.dockerenv) it runs the script directly with the variables as they are.
set -u
CORE="$(cd "$(dirname "$0")" && pwd)"
IMAGE="${PHOREPOC_IMAGE:-ghcr.io/phorepoc/base:latest}"
OS_ROOT="${PHOREPOC_OS_ROOT:-$CORE/os}"
SYMBIAN="${PHOREPOC_SYMBIAN:-$CORE/symbian}"
BUILD="${PHOREPOC_BUILD:-$CORE/build}"

help() {
cat <<'EOF'
Phorepoc task runner -- ./ph <command> [args]

 SOURCE
  fetch                   the Symbian source: pinned commits of the Phorepoc forks -> symbian/ (./fetch.sh, on the host)
  export [pkg...]         the epoc32/include tree from the bld.inf exports (tools/epoc32_export.py; --all = every package)
  casefix                 case-insensitive include links for the case-sensitive file system (tools/casefix.py)

 BUILD
  build <key> [key...]    build components for x32 (tools/emitter/build_mmp_x32.py; keys = tools/emitter/components*.py)
  closure <key>           ... and everything it links against (--closure)
  all                     rebuild EVERY component, providers first (--all)
  arm <key> [key...]      build ARM images (tools/emitter/build_mmp.py)
  rsc <key> [key...]      compile a component's resources (tools/emitter/build_mmp.py --rsc)
  rom [ssm|@|@app]        build the ROM section (default ssm = the phone's real System State Manager boot; @ = bootui)
  stale                   images linked before a provider of theirs existed (need a relink)
  sis <key> [key...]      package components as an installable SIS (tools/make_sis.py)
  icons                   the icon files (.mif) of every registered component (tools/mif_all.py)
  zdata                   the Z: data the packages' bld.inf files export (tools/zdata_gen.py)

 TEST
  tests [args]            byte-compare the images built here with another build (tests/compare_images.py)
  shell                   a shell in the container, with the mounts above

 OPTIONS (environment)
  PHOREPOC_OS_ROOT   our overlay, components and board files (default: os/)
  PHOREPOC_SYMBIAN   the Symbian source: symbian-2010/ and symbian-rcl3/ (default: symbian/)
  PHOREPOC_BUILD     all generated files, a directory or a Docker volume name (default: build/)
  PHOREPOC_IMAGE     the toolchain image (default: ghcr.io/phorepoc/base:latest)
  NET=1 (comms stack)  FONTS=ubuntu|dejavu|gdr  UI_RENDERER=classic|alfred|both  SHELLS=s3|s4|both  FAST=1 (keep core images)
EOF
}

# in_container <command...>: run a command in the toolchain image with the mounts, or directly when already inside one
in_container() {
  if [ -f /.dockerenv ]; then
    export PHOREPOC_OS_ROOT="$OS_ROOT" PHOREPOC_SYMBIAN="$SYMBIAN" PHOREPOC_BUILD="$BUILD"
    (cd "$CORE" && "$@")
    return
  fi
  command -v docker >/dev/null 2>&1 || { echo "docker not found" >&2; exit 1; }
  local tty=(); [ -t 0 ] && tty=(-it)
  local args=(run --rm "${tty[@]}" -v "$CORE:/core" -w /core -e PHOREPOC_OS_ROOT=/os -e PHOREPOC_SYMBIAN=/symbian -e PHOREPOC_BUILD=/build)
  [ -d "$OS_ROOT" ] && args+=(-v "$OS_ROOT:/os:ro")
  [ -d "$SYMBIAN" ] && args+=(-v "$SYMBIAN:/symbian:ro")
  case "$BUILD" in
    /*|./*|../*) mkdir -p "$BUILD"; args+=(-v "$BUILD:/build") ;;
    *) args+=(-v "$BUILD:/build") ;;                       # a Docker volume name
  esac
  # options of the recipes pass through when set
  local v
  for v in NET NET_TEST NTP_SERVER FONTS UI_RENDERER SHELLS ALF NGA NGA_KEEP FAST LEAVE_TRACE SCHED_TRACE MIF_ALL IBY_DATA \
           ROM_ARCH ROM_TAIL ROM_EXCLUDE SSM_TEST SECONDARY_IMAGE PHOREPOC_S4 PHOREPOC_QT48 PHETTY CENREP_EXTRA ROM_DATA_EXTRA \
           UTC_OFFSET WEBFONTS THEMES X32_DEFS X32_INCLUDE X32_FULLERR PRUNE_UNRESOLVED ROM_HEAP_LIMITS; do
    [ -n "${!v+x}" ] && args+=(-e "$v=${!v}")
  done
  docker "${args[@]}" "$IMAGE" "$@"
}

cmd="${1:-help}"; shift || true
case "$cmd" in
  help|-h|--help) help ;;

  fetch)   exec "$CORE/fetch.sh" "$@" ;;
  export)  in_container python3 tools/epoc32_export.py "$@" ;;
  casefix) in_container python3 tools/casefix.py "$@" ;;

  build)   in_container python3 tools/emitter/build_mmp_x32.py "$@" ;;
  closure) in_container python3 tools/emitter/build_mmp_x32.py --closure "$@" ;;
  all)     in_container python3 tools/emitter/build_mmp_x32.py --all ;;
  arm)     for k in "$@"; do in_container python3 tools/emitter/build_mmp.py "$k" || exit 1; done ;;
  rsc)     for k in "$@"; do in_container python3 tools/emitter/build_mmp.py --rsc "$k" || exit 1; done ;;

  rom)
    boot="${1:-ssm}"
    # FAST=1 keeps the core images; LEAVE_TRACE=1 builds the traced core images (the [LEAVE]/SSM lines)
    in_container bash -c 'mkdir -p "$PHOREPOC_BUILD" && cd /core && \
      FAST="${FAST:-1}" LEAVE_TRACE="${LEAVE_TRACE:-1}" BOOT="$1" SECONDARY_IMAGE="${SECONDARY_IMAGE:-efile.exe}" \
      bash tools/make_phorepoc_rom.sh > "$PHOREPOC_BUILD/rom-$1.log" 2>&1 \
      && echo "ROM ok ($(grep -a -m1 images "$PHOREPOC_BUILD/rom-$1.log" | cut -c1-100))" \
      || { echo "ROM FAILED:"; tail -5 "$PHOREPOC_BUILD/rom-$1.log"; exit 1; }' _ "$boot" ;;

  stale)   in_container python3 tools/stale_stubs.py ;;
  sis)     in_container python3 tools/make_sis.py "$@" ;;
  icons)   in_container python3 tools/mif_all.py "$@" ;;
  zdata)   in_container python3 tools/zdata_gen.py "$@" ;;

  tests)   in_container python3 tests/compare_images.py "$@" ;;
  shell)   if [ -f /.dockerenv ]; then exec bash; else in_container bash; fi ;;

  *) echo "unknown command: $cmd"; echo; help; exit 1 ;;
esac
