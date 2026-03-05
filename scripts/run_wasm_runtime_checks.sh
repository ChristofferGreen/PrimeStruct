#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
PRIMEC_BIN=""
WASMTIME_BIN="${WASMTIME_BIN:-wasmtime}"
WORK_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --primec)
      PRIMEC_BIN="$2"
      shift 2
      ;;
    --work-dir)
      WORK_DIR="$2"
      shift 2
      ;;
    --wasmtime-bin)
      WASMTIME_BIN="$2"
      shift 2
      ;;
    *)
      echo "[wasm-runtime-checks] ERROR: unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$PRIMEC_BIN" ]]; then
  PRIMEC_BIN="$BUILD_DIR/primec"
fi
if [[ -z "$WORK_DIR" ]]; then
  WORK_DIR="$BUILD_DIR/wasm-runtime-checks"
fi

if [[ ! -x "$PRIMEC_BIN" ]]; then
  echo "[wasm-runtime-checks] ERROR: primec binary not found: $PRIMEC_BIN" >&2
  exit 2
fi

if ! "$WASMTIME_BIN" --version >/dev/null 2>&1; then
  echo "[wasm-runtime-checks] SKIP: wasmtime unavailable; wasm runtime checks not executed."
  exit 0
fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

WASI_SOURCE="$WORK_DIR/wasi_runtime_check.prime"
BROWSER_SOURCE="$WORK_DIR/browser_runtime_check.prime"
WASI_WASM="$WORK_DIR/wasi_runtime_check.wasm"
BROWSER_WASM="$WORK_DIR/browser_runtime_check.wasm"
WASI_OUT="$WORK_DIR/wasi_runtime_check.out"
BROWSER_OUT="$WORK_DIR/browser_runtime_check.out"

cat > "$WASI_SOURCE" <<'EOF'
[return<int> effects(io_out)]
main() {
  print_line("wasm-runtime-check"utf8)
  return(0i32)
}
EOF

cat > "$BROWSER_SOURCE" <<'EOF'
[return<int>]
main() {
  return(plus(3i32, 4i32))
}
EOF

"$PRIMEC_BIN" --emit=wasm --wasm-profile wasi "$WASI_SOURCE" -o "$WASI_WASM" --entry /main
"$PRIMEC_BIN" --emit=wasm --wasm-profile browser "$BROWSER_SOURCE" -o "$BROWSER_WASM" --entry /main

"$WASMTIME_BIN" --invoke main "$WASI_WASM" > "$WASI_OUT"
"$WASMTIME_BIN" --invoke main "$BROWSER_WASM" > "$BROWSER_OUT"

if [[ "$(cat "$WASI_OUT")" != "wasm-runtime-check"$'\n' ]]; then
  echo "[wasm-runtime-checks] ERROR: unexpected wasi runtime output" >&2
  exit 2
fi

if ! grep -q "7" "$BROWSER_OUT"; then
  echo "[wasm-runtime-checks] ERROR: unexpected browser runtime output" >&2
  exit 2
fi

echo "[wasm-runtime-checks] PASS: executed wasm runtime checks with wasmtime."
