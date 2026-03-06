#!/usr/bin/env bash
set -euo pipefail

PREFIX="[native-window-launcher]"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
PRIMEC_BIN=""
OUT_DIR=""
MAX_FRAMES=""
SIMULATION_SMOKE=0
SKIP_PREFLIGHT=0

fail() {
  echo "${PREFIX} ERROR: $1" >&2
  exit 2
}

usage() {
  cat <<'EOF'
usage: run_native_spinning_cube_window.sh [--primec <path>] [--out-dir <path>] [--max-frames <positive-int>] [--simulation-smoke] [--skip-preflight]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --primec)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --primec"
      fi
      PRIMEC_BIN="$2"
      shift 2
      ;;
    --out-dir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --out-dir"
      fi
      OUT_DIR="$2"
      shift 2
      ;;
    --max-frames)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --max-frames"
      fi
      MAX_FRAMES="$2"
      shift 2
      ;;
    --simulation-smoke)
      SIMULATION_SMOKE=1
      shift
      ;;
    --skip-preflight)
      SKIP_PREFLIGHT=1
      shift
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

if [[ -z "$PRIMEC_BIN" ]]; then
  PRIMEC_BIN="$BUILD_DIR/primec"
fi
if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$BUILD_DIR/spinning-cube-native-window"
fi

if [[ ! -f "$PRIMEC_BIN" || ! -x "$PRIMEC_BIN" ]]; then
  fail "primec binary not found: $PRIMEC_BIN"
fi

if [[ -n "$MAX_FRAMES" ]]; then
  if ! [[ "$MAX_FRAMES" =~ ^[0-9]+$ ]] || (( MAX_FRAMES <= 0 )); then
    fail "--max-frames must be a positive integer: $MAX_FRAMES"
  fi
fi

CUBE_SOURCE="$ROOT_DIR/examples/web/spinning_cube/cube.prime"
HOST_SOURCE="$ROOT_DIR/examples/native/spinning_cube/window_host.mm"
PREFLIGHT_SCRIPT="$ROOT_DIR/scripts/preflight_native_spinning_cube_window.sh"

if [[ ! -f "$CUBE_SOURCE" ]]; then
  fail "missing cube source: $CUBE_SOURCE"
fi
if [[ ! -f "$HOST_SOURCE" ]]; then
  fail "missing native window host source: $HOST_SOURCE"
fi
if [[ ! -f "$PREFLIGHT_SCRIPT" ]]; then
  fail "missing preflight script: $PREFLIGHT_SCRIPT"
fi

mkdir -p "$OUT_DIR"

CUBE_SIM_BIN="$OUT_DIR/cube_native_frame_stream"
HOST_BIN="$OUT_DIR/spinning_cube_window_host"

if (( SKIP_PREFLIGHT == 0 )); then
  echo "${PREFIX} Running preflight checks"
  if ! "$PREFLIGHT_SCRIPT"; then
    fail "preflight checks failed"
  fi
else
  echo "${PREFIX} Skipping preflight checks (--skip-preflight)"
fi

echo "${PREFIX} Compiling cube simulation stream binary"
if ! "$PRIMEC_BIN" --emit=native "$CUBE_SOURCE" -o "$CUBE_SIM_BIN" --entry /cubeNativeAbiEmitFrameStream; then
  fail "failed to compile cube simulation stream binary"
fi

echo "${PREFIX} Compiling native window host"
if ! xcrun clang++ -std=c++17 -fobjc-arc "$HOST_SOURCE" \
    -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o "$HOST_BIN"; then
  fail "failed to compile native window host"
fi

runArgs=("$HOST_BIN" "--cube-sim" "$CUBE_SIM_BIN")
if [[ -n "$MAX_FRAMES" ]]; then
  runArgs+=("--max-frames" "$MAX_FRAMES")
fi
if (( SIMULATION_SMOKE == 1 )); then
  runArgs+=("--simulation-smoke")
fi

echo "${PREFIX} Launching native window host"
set +e
"${runArgs[@]}"
hostExit=$?
set -e
if (( hostExit != 0 )); then
  echo "${PREFIX} ERROR: native window host exited with code $hostExit" >&2
  exit "$hostExit"
fi

echo "${PREFIX} PASS: launch completed"
