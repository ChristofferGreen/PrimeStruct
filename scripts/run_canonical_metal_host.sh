#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"

LAUNCHER_PREFIX="[metal-launcher]"
USAGE_NAME="$(basename "$0")"
SHADER_SOURCE=""
HOST_SOURCE=""
OUT_SUBDIR=""
AIR_NAME="cube.air"
METALLIB_NAME="cube.metallib"
HOST_BIN_NAME="metal_host"
HOST_DESCRIPTION="metal host"

OUT_DIR=""
SOFTWARE_SURFACE_DEMO=0
SNAPSHOT_TICKS=""
PARITY_TICKS=""

fail() {
  echo "${LAUNCHER_PREFIX} ERROR: $1" >&2
  exit 2
}

usage() {
  echo "usage: ${USAGE_NAME} [--out-dir <path>] [--software-surface-demo] [--snapshot-code <ticks>] [--parity-check <ticks>]"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --launcher-prefix)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --launcher-prefix"
      fi
      LAUNCHER_PREFIX="$2"
      shift 2
      ;;
    --usage-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --usage-name"
      fi
      USAGE_NAME="$2"
      shift 2
      ;;
    --shader-source)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --shader-source"
      fi
      SHADER_SOURCE="$2"
      shift 2
      ;;
    --host-source)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-source"
      fi
      HOST_SOURCE="$2"
      shift 2
      ;;
    --out-subdir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --out-subdir"
      fi
      OUT_SUBDIR="$2"
      shift 2
      ;;
    --air-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --air-name"
      fi
      AIR_NAME="$2"
      shift 2
      ;;
    --metallib-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --metallib-name"
      fi
      METALLIB_NAME="$2"
      shift 2
      ;;
    --host-bin-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-bin-name"
      fi
      HOST_BIN_NAME="$2"
      shift 2
      ;;
    --host-description)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-description"
      fi
      HOST_DESCRIPTION="$2"
      shift 2
      ;;
    --out-dir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --out-dir"
      fi
      OUT_DIR="$2"
      shift 2
      ;;
    --software-surface-demo)
      SOFTWARE_SURFACE_DEMO=1
      shift
      ;;
    --snapshot-code)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --snapshot-code"
      fi
      SNAPSHOT_TICKS="$2"
      shift 2
      ;;
    --parity-check)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --parity-check"
      fi
      PARITY_TICKS="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      fail "unknown arg: $1"
      ;;
  esac
done

[[ -n "$SHADER_SOURCE" ]] || fail "missing required internal arg: --shader-source"
[[ -n "$HOST_SOURCE" ]] || fail "missing required internal arg: --host-source"
[[ -n "$OUT_SUBDIR" ]] || fail "missing required internal arg: --out-subdir"

mode_count=0
if (( SOFTWARE_SURFACE_DEMO == 1 )); then
  mode_count=$((mode_count + 1))
fi
if [[ -n "$SNAPSHOT_TICKS" ]]; then
  mode_count=$((mode_count + 1))
fi
if [[ -n "$PARITY_TICKS" ]]; then
  mode_count=$((mode_count + 1))
fi
if (( mode_count > 1 )); then
  fail "--software-surface-demo, --snapshot-code, and --parity-check are mutually exclusive"
fi

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$BUILD_DIR/$OUT_SUBDIR"
fi
mkdir -p "$OUT_DIR"

HOST_BIN_PATH="$OUT_DIR/$HOST_BIN_NAME"
HOST_STDOUT_PATH="$OUT_DIR/${HOST_BIN_NAME}.stdout.txt"
HOST_STDERR_PATH="$OUT_DIR/${HOST_BIN_NAME}.stderr.txt"
AIR_PATH="$OUT_DIR/$AIR_NAME"
METALLIB_PATH="$OUT_DIR/$METALLIB_NAME"

if ! xcrun --version >/dev/null 2>&1; then
  fail "xcrun unavailable; install Xcode Command Line Tools"
fi

if [[ -z "$SNAPSHOT_TICKS" && -z "$PARITY_TICKS" ]] && (( SOFTWARE_SURFACE_DEMO == 0 )); then
  if ! xcrun --find metal >/dev/null 2>&1; then
    fail "xcrun metal unavailable; run 'xcrun --find metal' after installing Command Line Tools"
  fi
  if ! xcrun --find metallib >/dev/null 2>&1; then
    fail "xcrun metallib unavailable; run 'xcrun --find metallib' after installing Command Line Tools"
  fi
fi

echo "${LAUNCHER_PREFIX} Compiling ${HOST_DESCRIPTION}"
if ! xcrun clang++ -std=c++17 -fobjc-arc "$HOST_SOURCE" -framework Foundation -framework Metal -o "$HOST_BIN_PATH"; then
  fail "${HOST_DESCRIPTION} compile failed"
fi

host_args=()
success_marker=""
success_message=""

if [[ -n "$SNAPSHOT_TICKS" ]]; then
  echo "${LAUNCHER_PREFIX} Running snapshot helper"
  host_args=(--snapshot-code "$SNAPSHOT_TICKS")
  success_marker="snapshot_code="
  success_message="snapshot helper completed"
elif [[ -n "$PARITY_TICKS" ]]; then
  echo "${LAUNCHER_PREFIX} Running parity helper"
  host_args=(--parity-check "$PARITY_TICKS")
  success_marker="parity_ok=1"
  success_message="parity helper completed"
elif (( SOFTWARE_SURFACE_DEMO == 1 )); then
  echo "${LAUNCHER_PREFIX} Launching ${HOST_DESCRIPTION} software-surface demo"
  host_args=(--software-surface-demo)
  success_marker="software_surface_presented=1"
  success_message="software-surface demo completed"
else
  echo "${LAUNCHER_PREFIX} Compiling Metal shader"
  if ! xcrun metal -std=metal3.0 -c "$SHADER_SOURCE" -o "$AIR_PATH"; then
    fail "metal shader compile failed"
  fi
  echo "${LAUNCHER_PREFIX} Linking metallib"
  if ! xcrun metallib "$AIR_PATH" -o "$METALLIB_PATH"; then
    fail "metallib link failed"
  fi
  echo "${LAUNCHER_PREFIX} Launching ${HOST_DESCRIPTION}"
  host_args=("$METALLIB_PATH")
  success_marker="frame_rendered=1"
  success_message="launch completed"
fi

if ! "$HOST_BIN_PATH" "${host_args[@]}" >"$HOST_STDOUT_PATH" 2>"$HOST_STDERR_PATH"; then
  cat "$HOST_STDOUT_PATH"
  cat "$HOST_STDERR_PATH" >&2
  fail "${HOST_DESCRIPTION} execution failed"
fi

cat "$HOST_STDOUT_PATH"
if [[ -s "$HOST_STDERR_PATH" ]]; then
  cat "$HOST_STDERR_PATH" >&2
fi

if [[ -n "$success_marker" ]] && ! grep -Fq "$success_marker" "$HOST_STDOUT_PATH"; then
  fail "missing success marker: $success_marker"
fi

echo "${LAUNCHER_PREFIX} PASS: ${success_message}"
