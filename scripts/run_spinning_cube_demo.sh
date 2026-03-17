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
if (( PORT_BASE < 1 || PORT_BASE > 65535 )); then
  echo "[spinning-cube-demo] ERROR: --port-base out of range (1-65535): $PORT_BASE" >&2
  exit 2
fi

if [[ ! -f "$PRIMEC_BIN" || ! -x "$PRIMEC_BIN" ]]; then
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

run_web_check() {
  local web_dir="$WORK_DIR/web"
  local launcher_script="$ROOT_DIR/scripts/run_browser_spinning_cube.sh"
  local launcher_out="$web_dir/browser_launcher.out.txt"
  local launcher_err="$web_dir/browser_launcher.err.txt"
  local port=$((PORT_BASE + 0))

  if [[ ! -f "$launcher_script" ]]; then
    set_result web FAIL "missing spinning cube browser launcher"
    return
  fi

  rm -rf "$web_dir"
  mkdir -p "$web_dir"

  if ! "$launcher_script" --primec "$PRIMEC_BIN" --out-dir "$web_dir" --port "$port" --headless-smoke \
      >"$launcher_out" 2>"$launcher_err"; then
    if grep -Fq "failed to compile cube.wasm" "$launcher_err"; then
      set_result web FAIL "failed to compile cube.wasm"
      return
    fi
    set_result web FAIL "browser launcher execution failed"
    return
  fi

  if grep -Fq "SMOKE: SKIP python3 unavailable" "$launcher_out"; then
    set_result web SKIP "python3 unavailable"
    return
  fi
  if grep -Fq "SMOKE: SKIP headless browser unavailable" "$launcher_out"; then
    set_result web SKIP "headless browser unavailable"
    return
  fi
  if grep -Fq "SMOKE: SKIP browser headless mode unavailable" "$launcher_out"; then
    set_result web SKIP "browser headless mode unavailable"
    return
  fi
  if grep -Fq "PASS: wasm bootstrap status verified" "$launcher_out"; then
    set_result web PASS "wasm bootstrap status verified"
    return
  fi

  set_result web FAIL "missing browser launcher success marker"
}

run_native_check() {
  local web_sample_dir="$ROOT_DIR/examples/web/spinning_cube"
  local native_sample_dir="$ROOT_DIR/examples/native/spinning_cube"
  local native_dir="$WORK_DIR/native"
  local cube_native="$native_dir/cube_native"
  local native_compile_err="$native_dir/native_compile.err.txt"
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

  if ! "$PRIMEC_BIN" --emit=native "$web_sample_dir/cube.prime" -o "$cube_native" --entry /mainNative 2>"$native_compile_err"; then
    if grep -Fq "backend does not support return type" "$native_compile_err"; then
      set_result native SKIP "native backend limitation: backend does not support return type"
      return
    fi
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
  local metal_dir="$WORK_DIR/metal"
  local host_out="$metal_dir/metal_host.out.txt"
  local host_err="$metal_dir/metal_host.err.txt"
  local launcher_script="$ROOT_DIR/scripts/run_metal_spinning_cube.sh"

  if [[ ! -f "$launcher_script" ]]; then
    set_result metal FAIL "missing spinning cube metal launcher"
    return
  fi

  rm -rf "$metal_dir"
  mkdir -p "$metal_dir"

  if ! "$launcher_script" --out-dir "$metal_dir" >"$host_out" 2>"$host_err"; then
    if grep -Fq "xcrun unavailable" "$host_err"; then
      set_result metal SKIP "xcrun unavailable"
      return
    fi
    if grep -Fq "xcrun metal unavailable" "$host_err"; then
      set_result metal SKIP "xcrun metal unavailable"
      return
    fi
    if grep -Fq "xcrun metallib unavailable" "$host_err"; then
      set_result metal SKIP "xcrun metallib unavailable"
      return
    fi
    set_result metal FAIL "metal launcher execution failed"
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
