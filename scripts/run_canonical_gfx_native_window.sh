#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"

LAUNCHER_PREFIX="[native-window-launcher]"
USAGE_NAME="$(basename "$0")"
PRIME_SOURCE=""
ENTRYPOINT=""
HOST_SOURCE=""
PREFLIGHT_SCRIPT=""
OUT_SUBDIR=""
STREAM_BIN_NAME="gfx_stream"
HOST_BIN_NAME="gfx_window_host"
STREAM_DESCRIPTION="gfx stream binary"
HOST_DESCRIPTION="native window host"
STREAM_HEADER_FIELD_COUNT=26
FIRST_ANGLE_LINE_OFFSET=2
SECOND_ANGLE_LINE_OFFSET=6

PRIMEC_BIN=""
OUT_DIR=""
MAX_FRAMES=""
SIMULATION_SMOKE=0
SKIP_PREFLIGHT=0
VISUAL_SMOKE=0
AUTO_MAX_FRAMES=0

fail() {
  echo "${LAUNCHER_PREFIX} ERROR: $1" >&2
  exit 2
}

usage() {
  echo "usage: ${USAGE_NAME} [--primec <path>] [--out-dir <path>] [--max-frames <positive-int>] [--simulation-smoke] [--visual-smoke] [--skip-preflight]"
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
    --prime-source)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --prime-source"
      fi
      PRIME_SOURCE="$2"
      shift 2
      ;;
    --entry)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --entry"
      fi
      ENTRYPOINT="$2"
      shift 2
      ;;
    --host-source)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-source"
      fi
      HOST_SOURCE="$2"
      shift 2
      ;;
    --preflight-script)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --preflight-script"
      fi
      PREFLIGHT_SCRIPT="$2"
      shift 2
      ;;
    --out-subdir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --out-subdir"
      fi
      OUT_SUBDIR="$2"
      shift 2
      ;;
    --stream-bin-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --stream-bin-name"
      fi
      STREAM_BIN_NAME="$2"
      shift 2
      ;;
    --host-bin-name)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-bin-name"
      fi
      HOST_BIN_NAME="$2"
      shift 2
      ;;
    --stream-description)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --stream-description"
      fi
      STREAM_DESCRIPTION="$2"
      shift 2
      ;;
    --host-description)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --host-description"
      fi
      HOST_DESCRIPTION="$2"
      shift 2
      ;;
    --stream-header-field-count)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --stream-header-field-count"
      fi
      STREAM_HEADER_FIELD_COUNT="$2"
      shift 2
      ;;
    --first-angle-line-offset)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --first-angle-line-offset"
      fi
      FIRST_ANGLE_LINE_OFFSET="$2"
      shift 2
      ;;
    --second-angle-line-offset)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --second-angle-line-offset"
      fi
      SECOND_ANGLE_LINE_OFFSET="$2"
      shift 2
      ;;
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

[[ -n "$PRIME_SOURCE" ]] || fail "missing required internal arg: --prime-source"
[[ -n "$ENTRYPOINT" ]] || fail "missing required internal arg: --entry"
[[ -n "$HOST_SOURCE" ]] || fail "missing required internal arg: --host-source"
[[ -n "$PREFLIGHT_SCRIPT" ]] || fail "missing required internal arg: --preflight-script"
[[ -n "$OUT_SUBDIR" ]] || fail "missing required internal arg: --out-subdir"

if (( SIMULATION_SMOKE == 1 && VISUAL_SMOKE == 1 )); then
  fail "--simulation-smoke and --visual-smoke cannot be combined"
fi

if (( VISUAL_SMOKE == 1 )) && [[ -z "$MAX_FRAMES" ]]; then
  MAX_FRAMES="600"
fi

if [[ -z "$MAX_FRAMES" ]] && (( SIMULATION_SMOKE == 0 )); then
  MAX_FRAMES="600"
  AUTO_MAX_FRAMES=1
fi

if (( VISUAL_SMOKE == 1 )); then
  osName="$(uname -s 2>/dev/null || true)"
  if [[ "$osName" != "Darwin" ]]; then
    echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: SKIP non-macOS runner"
    exit 0
  fi

  if ! command -v launchctl >/dev/null 2>&1; then
    echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: SKIP launchctl unavailable"
    exit 0
  fi

  userId="$(id -u 2>/dev/null || true)"
  if [[ -z "$userId" ]]; then
    echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: SKIP unable to determine uid"
    exit 0
  fi

  if ! launchctl print "gui/$userId" >/dev/null 2>&1; then
    echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: SKIP GUI session unavailable"
    exit 0
  fi
fi

if [[ -z "$PRIMEC_BIN" ]]; then
  PRIMEC_BIN="$BUILD_DIR/primec"
fi
if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$BUILD_DIR/$OUT_SUBDIR"
fi

if [[ ! -f "$PRIMEC_BIN" || ! -x "$PRIMEC_BIN" ]]; then
  fail "primec binary not found: $PRIMEC_BIN"
fi

if [[ -n "$MAX_FRAMES" ]]; then
  if ! [[ "$MAX_FRAMES" =~ ^[0-9]+$ ]] || (( MAX_FRAMES <= 0 )); then
    fail "--max-frames must be a positive integer: $MAX_FRAMES"
  fi
fi

if [[ ! -f "$PRIME_SOURCE" ]]; then
  fail "missing prime source: $PRIME_SOURCE"
fi
if [[ ! -f "$HOST_SOURCE" ]]; then
  fail "missing native window host source: $HOST_SOURCE"
fi
if [[ ! -f "$PREFLIGHT_SCRIPT" ]]; then
  fail "missing preflight script: $PREFLIGHT_SCRIPT"
fi

mkdir -p "$OUT_DIR"

GFX_STREAM_BIN="$OUT_DIR/$STREAM_BIN_NAME"
HOST_BIN="$OUT_DIR/$HOST_BIN_NAME"

if (( SKIP_PREFLIGHT == 0 )); then
  echo "${LAUNCHER_PREFIX} Running preflight checks"
  if ! "$PREFLIGHT_SCRIPT"; then
    fail "preflight checks failed"
  fi
else
  echo "${LAUNCHER_PREFIX} Skipping preflight checks (--skip-preflight)"
fi

echo "${LAUNCHER_PREFIX} Compiling ${STREAM_DESCRIPTION}"
if ! "$PRIMEC_BIN" --emit=native "$PRIME_SOURCE" -o "$GFX_STREAM_BIN" --entry "$ENTRYPOINT"; then
  fail "failed to compile ${STREAM_DESCRIPTION}"
fi

echo "${LAUNCHER_PREFIX} Compiling ${HOST_DESCRIPTION}"
if ! xcrun clang++ -std=c++17 -fobjc-arc "$HOST_SOURCE" \
    -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o "$HOST_BIN"; then
  fail "failed to compile ${HOST_DESCRIPTION}"
fi

runArgs=("$HOST_BIN" "--gfx" "$GFX_STREAM_BIN")
if [[ -n "$MAX_FRAMES" ]]; then
  runArgs+=("--max-frames" "$MAX_FRAMES")
fi
if (( SIMULATION_SMOKE == 1 )); then
  runArgs+=("--simulation-smoke")
fi

echo "${LAUNCHER_PREFIX} Launching ${HOST_DESCRIPTION}"
if (( AUTO_MAX_FRAMES == 1 )); then
  echo "${LAUNCHER_PREFIX} Defaulting to --max-frames ${MAX_FRAMES} (~10s at 60fps)"
fi
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
  echo "${LAUNCHER_PREFIX} ERROR: ${HOST_DESCRIPTION} exited with code $hostExit" >&2
  exit "$hostExit"
fi

if (( VISUAL_SMOKE == 1 )); then
  if ! grep -Fq "window_created=1" "$hostLogPath" || ! grep -Fq "startup_success=1" "$hostLogPath"; then
    fail "visual smoke criterion failed: window_shown"
  fi
  if ! grep -Fq "frame_rendered=1" "$hostLogPath" || ! grep -Fq "exit_reason=max_frames" "$hostLogPath"; then
    fail "visual smoke criterion failed: render_loop_alive"
  fi

  streamOutPath="$OUT_DIR/${STREAM_BIN_NAME}.visual_smoke.out.txt"
  if ! "$GFX_STREAM_BIN" >"$streamOutPath"; then
    fail "visual smoke criterion failed: rotation_changes_over_time (unable to execute stream binary)"
  fi

  firstAngleLine=$((STREAM_HEADER_FIELD_COUNT + FIRST_ANGLE_LINE_OFFSET))
  secondAngleLine=$((STREAM_HEADER_FIELD_COUNT + SECOND_ANGLE_LINE_OFFSET))
  firstAngle="$(sed -n "${firstAngleLine}p" "$streamOutPath")"
  secondAngle="$(sed -n "${secondAngleLine}p" "$streamOutPath")"
  if [[ -z "$firstAngle" || -z "$secondAngle" ]]; then
    fail "visual smoke criterion failed: rotation_changes_over_time (insufficient frame stream output)"
  fi
  if [[ "$firstAngle" == "$secondAngle" ]]; then
    fail "visual smoke criterion failed: rotation_changes_over_time (angle did not change)"
  fi

  echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: window_shown=PASS"
  echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: render_loop_alive=PASS"
  echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: rotation_changes_over_time=PASS"
  echo "${LAUNCHER_PREFIX} VISUAL-SMOKE: PASS"
fi

echo "${LAUNCHER_PREFIX} PASS: launch completed"
