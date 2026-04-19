#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"

LAUNCHER_PREFIX="[browser-launcher]"
USAGE_NAME="$(basename "$0")"
PRIME_SOURCE=""
ENTRYPOINT=""
SAMPLE_DIR=""
SHARED_RUNTIME=""
SAMPLE_SUBDIR=""
OUT_SUBDIR=""
EXPECTED_STATUS_TEXT=""

PRIMEC_BIN=""
OUT_DIR=""
PORT="8080"
HEADLESS_SMOKE=0

fail() {
  echo "${LAUNCHER_PREFIX} ERROR: $1" >&2
  exit 2
}

usage() {
  echo "usage: ${USAGE_NAME} [--primec <path>] [--out-dir <path>] [--port <int>] [--headless-smoke]"
}

find_browser() {
  local candidates=(chromium google-chrome chrome google-chrome-stable)
  local candidate=""
  for candidate in "${candidates[@]}"; do
    if "$candidate" --version >/dev/null 2>&1; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
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
    --sample-dir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --sample-dir"
      fi
      SAMPLE_DIR="$2"
      shift 2
      ;;
    --shared-runtime)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --shared-runtime"
      fi
      SHARED_RUNTIME="$2"
      shift 2
      ;;
    --sample-subdir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --sample-subdir"
      fi
      SAMPLE_SUBDIR="$2"
      shift 2
      ;;
    --out-subdir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --out-subdir"
      fi
      OUT_SUBDIR="$2"
      shift 2
      ;;
    --expected-status-text)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --expected-status-text"
      fi
      EXPECTED_STATUS_TEXT="$2"
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
    --port)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        fail "missing value for --port"
      fi
      PORT="$2"
      shift 2
      ;;
    --headless-smoke)
      HEADLESS_SMOKE=1
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
[[ -n "$SAMPLE_DIR" ]] || fail "missing required internal arg: --sample-dir"
[[ -n "$SHARED_RUNTIME" ]] || fail "missing required internal arg: --shared-runtime"
[[ -n "$SAMPLE_SUBDIR" ]] || fail "missing required internal arg: --sample-subdir"
[[ -n "$OUT_SUBDIR" ]] || fail "missing required internal arg: --out-subdir"
[[ -n "$EXPECTED_STATUS_TEXT" ]] || fail "missing required internal arg: --expected-status-text"

if [[ -z "$PRIMEC_BIN" ]]; then
  PRIMEC_BIN="$BUILD_DIR/primec"
fi
if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$BUILD_DIR/$OUT_SUBDIR"
fi

if [[ ! -f "$PRIMEC_BIN" || ! -x "$PRIMEC_BIN" ]]; then
  fail "primec binary not found: $PRIMEC_BIN"
fi
if ! [[ "$PORT" =~ ^[0-9]+$ ]]; then
  fail "--port must be an integer: $PORT"
fi
if (( PORT < 1 || PORT > 65535 )); then
  fail "--port out of range (1-65535): $PORT"
fi

SAMPLE_STAGE_DIR="$OUT_DIR/$SAMPLE_SUBDIR"
SHARED_STAGE_DIR="$OUT_DIR/shared"
WASM_PATH="$SAMPLE_STAGE_DIR/cube.wasm"
SERVER_LOG_PATH="$OUT_DIR/browser_server.log"
SERVER_DOM_PATH="$OUT_DIR/browser.dom.txt"
SERVER_ERR_PATH="$OUT_DIR/browser.err.txt"
INDEX_URL="http://127.0.0.1:${PORT}/${SAMPLE_SUBDIR}/index.html"

if [[ ! -f "$PRIME_SOURCE" ]]; then
  fail "missing sample prime source: $PRIME_SOURCE"
fi
if [[ ! -f "$SAMPLE_DIR/index.html" || ! -f "$SAMPLE_DIR/main.js" || ! -f "$SAMPLE_DIR/cube.wgsl" ]]; then
  fail "missing browser sample assets"
fi
if [[ ! -f "$SHARED_RUNTIME" ]]; then
  fail "missing shared browser runtime helper: $SHARED_RUNTIME"
fi

if (( HEADLESS_SMOKE == 1 )); then
  if ! python3 --version >/dev/null 2>&1; then
    echo "${LAUNCHER_PREFIX} SMOKE: SKIP python3 unavailable"
    exit 0
  fi

  browser_cmd=""
  if ! browser_cmd="$(find_browser)"; then
    echo "${LAUNCHER_PREFIX} SMOKE: SKIP headless browser unavailable"
    exit 0
  fi

  if ! "$browser_cmd" --headless --disable-gpu --dump-dom about:blank >/dev/null 2>&1; then
    echo "${LAUNCHER_PREFIX} SMOKE: SKIP browser headless mode unavailable"
    exit 0
  fi
fi

mkdir -p "$OUT_DIR"
rm -rf "$SAMPLE_STAGE_DIR" "$SHARED_STAGE_DIR"
rm -f "$SERVER_LOG_PATH" "$SERVER_DOM_PATH" "$SERVER_ERR_PATH"
mkdir -p "$SAMPLE_STAGE_DIR" "$SHARED_STAGE_DIR"

echo "${LAUNCHER_PREFIX} Compiling browser wasm"
if ! "$PRIMEC_BIN" --emit=wasm --wasm-profile browser "$PRIME_SOURCE" -o "$WASM_PATH" --entry "$ENTRYPOINT"; then
  fail "failed to compile cube.wasm"
fi

echo "${LAUNCHER_PREFIX} Staging browser assets"
cp "$SAMPLE_DIR/index.html" "$SAMPLE_STAGE_DIR/index.html"
cp "$SAMPLE_DIR/main.js" "$SAMPLE_STAGE_DIR/main.js"
cp "$SAMPLE_DIR/cube.wgsl" "$SAMPLE_STAGE_DIR/cube.wgsl"
cp "$SHARED_RUNTIME" "$SHARED_STAGE_DIR/$(basename "$SHARED_RUNTIME")"

if (( HEADLESS_SMOKE == 1 )); then
  python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$OUT_DIR" >"$SERVER_LOG_PATH" 2>&1 &
  server_pid=$!
  cleanup() {
    kill "$server_pid" >/dev/null 2>&1 || true
    wait "$server_pid" 2>/dev/null || true
  }
  trap cleanup EXIT
  sleep 1

  echo "${LAUNCHER_PREFIX} Running headless browser smoke"
  if ! "$browser_cmd" --headless --disable-gpu --virtual-time-budget=6000 --dump-dom \
      "$INDEX_URL" >"$SERVER_DOM_PATH" 2>"$SERVER_ERR_PATH"; then
    fail "headless browser execution failed"
  fi

  if ! grep -Fq "$EXPECTED_STATUS_TEXT" "$SERVER_DOM_PATH"; then
    fail "missing wasm bootstrap status text"
  fi
  if grep -Fq "Wasm load skipped" "$SERVER_DOM_PATH"; then
    fail "found wasm fallback status text"
  fi

  echo "${LAUNCHER_PREFIX} PASS: wasm bootstrap status verified"
  exit 0
fi

if ! python3 --version >/dev/null 2>&1; then
  fail "python3 unavailable"
fi

echo "${LAUNCHER_PREFIX} Serving ${INDEX_URL}"
exec python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$OUT_DIR"
