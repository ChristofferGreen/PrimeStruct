#!/usr/bin/env bash
# Builds a native executable from a PrimeStruct native UI program: the program is compiled to C++ by
# the optimized emitter (primec --emit=optexe) and linked with a UI backend, so the result needs no
# VM and no compiler at run time.
#
# Usage: scripts/build_native_app.sh <app.prime> <output> [--backend appkit|headless] [--build-dir DIR]
#                                    [--direct] [--link-arg ARG]...
#
# --direct (appkit only) skips the C++ compiler: primec's own native backend (primec --emit=native)
# writes the executable, which loads libps_ui_appkit.dylib (copied next to the output) and binds the
# program's host functions to it at launch. Build the dylib first:
#   cmake --build build-release --target primec primec_ui_appkit_dylib
#
# --backend appkit (the default on macOS) links the AppKit backend; headless links the scripted
# test backend (src/ui/headless). Extra --link-arg values go to the C++ compiler after the libraries
# (a source that drives the headless backend, for instance). The build directory holds primec and
# the backend libraries (default build-release); build them first:
#   cmake --build build-release --target primec primec_ui_appkit_lib primec_ui_headless_lib
set -euo pipefail

usage() {
  echo "Usage: $0 <app.prime> <output> [--backend appkit|headless] [--build-dir DIR] [--direct] [--link-arg ARG]..." >&2
  exit 64
}

if [[ $# -lt 2 ]]; then
  usage
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROGRAM="$1"
OUTPUT="$2"
shift 2

BUILD_DIR="$ROOT_DIR/build-release"
BACKEND="appkit"
if [[ "$(uname -s)" != "Darwin" ]]; then
  BACKEND="headless"
fi
EXTRA=()
DIRECT=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --direct) DIRECT=1; shift ;;
    --backend) [[ $# -ge 2 ]] || usage; BACKEND="$2"; shift 2 ;;
    --build-dir) [[ $# -ge 2 ]] || usage; BUILD_DIR="$2"; shift 2 ;;
    --link-arg) [[ $# -ge 2 ]] || usage; EXTRA+=("--link-arg" "$2"); shift 2 ;;
    *) usage ;;
  esac
done

require() {
  if [[ ! -e "$1" ]]; then
    echo "missing $1; build it first (cmake --build $BUILD_DIR --target $2)" >&2
    exit 1
  fi
}

require "$BUILD_DIR/primec" primec

if [[ $DIRECT -eq 1 ]]; then
  [[ "$BACKEND" == "appkit" ]] || usage
  require "$BUILD_DIR/libps_ui_appkit.dylib" primec_ui_appkit_dylib
  mkdir -p "$(dirname "$OUTPUT")"
  # A fresh file, not a copy over an old one: the kernel caches the signature of the old file's pages.
  rm -f "$(dirname "$OUTPUT")/libps_ui_appkit.dylib"
  cp "$BUILD_DIR/libps_ui_appkit.dylib" "$(dirname "$OUTPUT")/libps_ui_appkit.dylib"
  "$BUILD_DIR/primec" --emit=native "$PROGRAM" -o "$OUTPUT" \
    --link-arg "@executable_path/libps_ui_appkit.dylib" "${EXTRA[@]+"${EXTRA[@]}"}"
  echo "$OUTPUT"
  exit 0
fi

require "$BUILD_DIR/libprimec_ui_highlight_lib.a" primec_ui_highlight_lib

LINK=()
case "$BACKEND" in
  appkit)
    require "$BUILD_DIR/libprimec_ui_appkit_lib.a" primec_ui_appkit_lib
    LINK+=("--link-arg" "$BUILD_DIR/libprimec_ui_appkit_lib.a")
    LINK+=("--link-arg" "$BUILD_DIR/libprimec_ui_highlight_lib.a")
    LINK+=("--link-arg" "-framework" "--link-arg" "AppKit")
    LINK+=("--link-arg" "-framework" "--link-arg" "Foundation")
    LINK+=("--link-arg" "-framework" "--link-arg" "UniformTypeIdentifiers")
    ;;
  headless)
    require "$BUILD_DIR/libprimec_ui_headless_lib.a" primec_ui_headless_lib
    LINK+=("--link-arg" "-I$ROOT_DIR/include")
    LINK+=("--link-arg" "$BUILD_DIR/libprimec_ui_headless_lib.a")
    LINK+=("--link-arg" "$BUILD_DIR/libprimec_ui_highlight_lib.a")
    ;;
  *) usage ;;
esac

mkdir -p "$(dirname "$OUTPUT")"
"$BUILD_DIR/primec" --emit=optexe "$PROGRAM" -o "$OUTPUT" "${EXTRA[@]+"${EXTRA[@]}"}" "${LINK[@]}"
# primec leaves the generated C++ next to the executable; the executable is the result.
rm -f "$OUTPUT.cpp"
echo "$OUTPUT"
