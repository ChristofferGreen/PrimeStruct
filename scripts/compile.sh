#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
RUN_TESTS=1
CONFIGURE_ONLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      shift
      ;;
    --configure)
      CONFIGURE_ONLY=1
      shift
      ;;
    --skip-tests)
      RUN_TESTS=0
      shift
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
 done

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR"

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
  exit 0
fi

if [[ $RUN_TESTS -eq 1 ]]; then
  (cd "$BUILD_DIR" && ctest --output-on-failure)
fi
