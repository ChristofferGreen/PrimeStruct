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
VISUAL_SMOKE=0

fail() {
  echo "${PREFIX} ERROR: $1" >&2
  exit 2
}

usage() {
  cat <<'EOF'
usage: run_native_spinning_cube_window.sh [--primec <path>] [--out-dir <path>] [--max-frames <positive-int>] [--simulation-smoke] [--visual-smoke] [--skip-preflight]
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
    --visual-smoke)
      VISUAL_SMOKE=1
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

if (( SIMULATION_SMOKE == 1 && VISUAL_SMOKE == 1 )); then
  fail "--simulation-smoke and --visual-smoke cannot be combined"
fi

if (( VISUAL_SMOKE == 1 )) && [[ -z "$MAX_FRAMES" ]]; then
  MAX_FRAMES="600"
fi

if (( VISUAL_SMOKE == 1 )); then
  osName="$(uname -s 2>/dev/null || true)"
  if [[ "$osName" != "Darwin" ]]; then
    echo "${PREFIX} VISUAL-SMOKE: SKIP non-macOS runner"
    exit 0
  fi

  if ! command -v launchctl >/dev/null 2>&1; then
    echo "${PREFIX} VISUAL-SMOKE: SKIP launchctl unavailable"
    exit 0
  fi

  userId="$(id -u 2>/dev/null || true)"
  if [[ -z "$userId" ]]; then
    echo "${PREFIX} VISUAL-SMOKE: SKIP unable to determine uid"
    exit 0
  fi

  if ! launchctl print "gui/$userId" >/dev/null 2>&1; then
    echo "${PREFIX} VISUAL-SMOKE: SKIP GUI session unavailable"
    exit 0
  fi
fi

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
hostExit=0
hostLogPath="$OUT_DIR/native_window_host.log"
if (( VISUAL_SMOKE == 1 )); then
  set +e
  "${runArgs[@]}" >"$hostLogPath" 2>&1
  hostExit=$?
  set -e
  cat "$hostLogPath"
else
  set +e
  "${runArgs[@]}"
  hostExit=$?
  set -e
fi
if (( hostExit != 0 )); then
  echo "${PREFIX} ERROR: native window host exited with code $hostExit" >&2
  exit "$hostExit"
fi

if (( VISUAL_SMOKE == 1 )); then
  if ! grep -Fq "window_created=1" "$hostLogPath" || ! grep -Fq "startup_success=1" "$hostLogPath"; then
    fail "visual smoke criterion failed: window_shown"
  fi
  if ! grep -Fq "frame_rendered=1" "$hostLogPath" || ! grep -Fq "exit_reason=max_frames" "$hostLogPath"; then
    fail "visual smoke criterion failed: render_loop_alive"
  fi

  streamOutPath="$OUT_DIR/cube_native_frame_stream.visual_smoke.out.txt"
  if ! "$CUBE_SIM_BIN" >"$streamOutPath"; then
    fail "visual smoke criterion failed: rotation_changes_over_time (unable to execute stream binary)"
  fi

  firstAngle="$(sed -n '2p' "$streamOutPath")"
  secondAngle="$(sed -n '6p' "$streamOutPath")"
  if [[ -z "$firstAngle" || -z "$secondAngle" ]]; then
    fail "visual smoke criterion failed: rotation_changes_over_time (insufficient frame stream output)"
  fi
  if [[ "$firstAngle" == "$secondAngle" ]]; then
    fail "visual smoke criterion failed: rotation_changes_over_time (angle did not change)"
  fi

  echo "${PREFIX} VISUAL-SMOKE: window_shown=PASS"
  echo "${PREFIX} VISUAL-SMOKE: render_loop_alive=PASS"
  echo "${PREFIX} VISUAL-SMOKE: rotation_changes_over_time=PASS"
  echo "${PREFIX} VISUAL-SMOKE: PASS"
fi

echo "${PREFIX} PASS: launch completed"
