#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
PRIMEC_BIN=""
WORK_DIR=""
PORT_BASE=18870

while [[ $# -gt 0 ]]; do
  case "$1" in
    --primec)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        echo "[spinning-cube-demo] ERROR: missing value for --primec" >&2
        exit 2
      fi
      PRIMEC_BIN="$2"
      shift 2
      ;;
    --work-dir)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        echo "[spinning-cube-demo] ERROR: missing value for --work-dir" >&2
        exit 2
      fi
      WORK_DIR="$2"
      shift 2
      ;;
    --port-base)
      if [[ $# -lt 2 || "$2" == --* ]]; then
        echo "[spinning-cube-demo] ERROR: missing value for --port-base" >&2
        exit 2
      fi
      PORT_BASE="$2"
      shift 2
      ;;
    *)
      echo "[spinning-cube-demo] ERROR: unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$PRIMEC_BIN" ]]; then
  PRIMEC_BIN="$BUILD_DIR/primec"
fi
if [[ -z "$WORK_DIR" ]]; then
  WORK_DIR="$BUILD_DIR/spinning-cube-demo"
fi

if ! [[ "$PORT_BASE" =~ ^[0-9]+$ ]]; then
  echo "[spinning-cube-demo] ERROR: --port-base must be an integer: $PORT_BASE" >&2
  exit 2
fi

if [[ ! -x "$PRIMEC_BIN" ]]; then
  echo "[spinning-cube-demo] ERROR: primec binary not found: $PRIMEC_BIN" >&2
  exit 2
fi

WEB_STATUS="SKIP"
NATIVE_STATUS="SKIP"
METAL_STATUS="SKIP"
WEB_DETAIL="not-run"
NATIVE_DETAIL="not-run"
METAL_DETAIL="not-run"
OVERALL_EXIT=0

set_result() {
  local check="$1"
  local status="$2"
  local detail="$3"

  case "$check" in
    web)
      WEB_STATUS="$status"
      WEB_DETAIL="$detail"
      ;;
    native)
      NATIVE_STATUS="$status"
      NATIVE_DETAIL="$detail"
      ;;
    metal)
      METAL_STATUS="$status"
      METAL_DETAIL="$detail"
      ;;
    *)
      echo "[spinning-cube-demo] ERROR: unknown check: $check" >&2
      exit 2
      ;;
  esac

  if [[ "$status" == "FAIL" ]]; then
    OVERALL_EXIT=1
  fi
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

run_web_check() {
  local sample_dir="$ROOT_DIR/examples/web/spinning_cube"
  local web_dir="$WORK_DIR/web"
  local wasm_path="$web_dir/cube.wasm"
  local browser_dom="$web_dir/browser.dom.txt"
  local browser_err="$web_dir/browser.err.txt"
  local server_log="$web_dir/server.log"
  local browser_cmd=""
  local port=$((PORT_BASE + 0))

  if [[ ! -f "$sample_dir/cube.prime" || ! -f "$sample_dir/index.html" || ! -f "$sample_dir/main.js" || ! -f "$sample_dir/cube.wgsl" ]]; then
    set_result web FAIL "missing spinning cube web sample files"
    return
  fi

  if ! python3 --version >/dev/null 2>&1; then
    set_result web SKIP "python3 unavailable"
    return
  fi

  if ! browser_cmd="$(find_browser)"; then
    set_result web SKIP "headless browser unavailable"
    return
  fi

  if ! "$browser_cmd" --headless --disable-gpu --dump-dom about:blank >/dev/null 2>&1; then
    set_result web SKIP "browser headless mode unavailable"
    return
  fi

  rm -rf "$web_dir"
  mkdir -p "$web_dir"

  if ! "$PRIMEC_BIN" --emit=wasm --wasm-profile browser "$sample_dir/cube.prime" -o "$wasm_path" --entry /main; then
    set_result web FAIL "failed to compile cube.wasm"
    return
  fi

  cp "$sample_dir/index.html" "$web_dir/index.html"
  cp "$sample_dir/main.js" "$web_dir/main.js"
  cp "$sample_dir/cube.wgsl" "$web_dir/cube.wgsl"

  python3 -m http.server "$port" --bind 127.0.0.1 --directory "$web_dir" >"$server_log" 2>&1 &
  local server_pid=$!
  sleep 1

  if ! "$browser_cmd" --headless --disable-gpu --virtual-time-budget=6000 --dump-dom \
      "http://127.0.0.1:${port}/index.html" >"$browser_dom" 2>"$browser_err"; then
    kill "$server_pid" >/dev/null 2>&1 || true
    wait "$server_pid" 2>/dev/null || true
    set_result web FAIL "headless browser execution failed"
    return
  fi

  kill "$server_pid" >/dev/null 2>&1 || true
  wait "$server_pid" 2>/dev/null || true

  if ! grep -Fq "Host running with cube.wasm and cube.wgsl bootstrap." "$browser_dom"; then
    set_result web FAIL "missing wasm bootstrap status text"
    return
  fi
  if grep -Fq "Wasm load skipped" "$browser_dom"; then
    set_result web FAIL "found wasm fallback status text"
    return
  fi

  set_result web PASS "wasm bootstrap status verified"
}

run_native_check() {
  local web_sample_dir="$ROOT_DIR/examples/web/spinning_cube"
  local native_sample_dir="$ROOT_DIR/examples/native/spinning_cube"
  local native_dir="$WORK_DIR/native"
  local cube_native="$native_dir/cube_native"
  local host_bin="$native_dir/spinning_cube_host"
  local host_out="$native_dir/native_host.out.txt"
  local cxx=""

  if [[ ! -f "$web_sample_dir/cube.prime" || ! -f "$native_sample_dir/main.cpp" ]]; then
    set_result native FAIL "missing spinning cube native sample files"
    return
  fi

  if c++ --version >/dev/null 2>&1; then
    cxx="c++"
  elif clang++ --version >/dev/null 2>&1; then
    cxx="clang++"
  elif g++ --version >/dev/null 2>&1; then
    cxx="g++"
  else
    set_result native SKIP "no C++ compiler available"
    return
  fi

  rm -rf "$native_dir"
  mkdir -p "$native_dir"

  if ! "$PRIMEC_BIN" --emit=native "$web_sample_dir/cube.prime" -o "$cube_native" --entry /main; then
    set_result native FAIL "failed to compile cube native binary"
    return
  fi

  if ! "$cxx" -std=c++17 "$native_sample_dir/main.cpp" -o "$host_bin"; then
    set_result native FAIL "failed to compile native host"
    return
  fi

  if ! "$host_bin" "$cube_native" >"$host_out"; then
    set_result native FAIL "native host execution failed"
    return
  fi

  if ! grep -Fq "native host verified cube simulation output" "$host_out"; then
    set_result native FAIL "missing native success marker"
    return
  fi

  set_result native PASS "native success marker verified"
}

run_metal_check() {
  local metal_sample_dir="$ROOT_DIR/examples/metal/spinning_cube"
  local metal_dir="$WORK_DIR/metal"
  local air_path="$metal_dir/cube.air"
  local metallib_path="$metal_dir/cube.metallib"
  local host_bin="$metal_dir/metal_host"
  local host_out="$metal_dir/metal_host.out.txt"
  local host_err="$metal_dir/metal_host.err.txt"

  if [[ ! -f "$metal_sample_dir/cube.metal" || ! -f "$metal_sample_dir/metal_host.mm" ]]; then
    set_result metal FAIL "missing spinning cube metal sample files"
    return
  fi

  if ! xcrun --version >/dev/null 2>&1; then
    set_result metal SKIP "xcrun unavailable"
    return
  fi
  if ! xcrun --find metal >/dev/null 2>&1; then
    set_result metal SKIP "xcrun metal unavailable"
    return
  fi
  if ! xcrun --find metallib >/dev/null 2>&1; then
    set_result metal SKIP "xcrun metallib unavailable"
    return
  fi

  rm -rf "$metal_dir"
  mkdir -p "$metal_dir"

  if ! xcrun metal -std=metal3.0 -c "$metal_sample_dir/cube.metal" -o "$air_path"; then
    set_result metal FAIL "metal shader compile failed"
    return
  fi

  if ! xcrun metallib "$air_path" -o "$metallib_path"; then
    set_result metal FAIL "metallib link failed"
    return
  fi

  if ! xcrun clang++ -std=c++17 -fobjc-arc "$metal_sample_dir/metal_host.mm" \
      -framework Foundation -framework Metal -o "$host_bin"; then
    set_result metal FAIL "metal host compile failed"
    return
  fi

  if ! "$host_bin" "$metallib_path" >"$host_out" 2>"$host_err"; then
    set_result metal FAIL "metal host execution failed"
    return
  fi

  if ! grep -Fq "frame_rendered=1" "$host_out"; then
    set_result metal FAIL "missing metal frame marker"
    return
  fi

  set_result metal PASS "metal frame marker verified"
}

run_web_check
run_native_check
run_metal_check

echo "[spinning-cube-demo] SUMMARY"
echo "[spinning-cube-demo] WEB: ${WEB_STATUS} (${WEB_DETAIL})"
echo "[spinning-cube-demo] NATIVE: ${NATIVE_STATUS} (${NATIVE_DETAIL})"
echo "[spinning-cube-demo] METAL: ${METAL_STATUS} (${METAL_DETAIL})"
if [[ "$OVERALL_EXIT" -eq 0 ]]; then
  echo "[spinning-cube-demo] RESULT: PASS"
else
  echo "[spinning-cube-demo] RESULT: FAIL"
fi

exit "$OVERALL_EXIT"
