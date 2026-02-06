#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
RUNS="${BENCH_RUNS:-5}"
BENCH_DIR="${BENCH_DIR:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --runs)
      RUNS="$2"
      shift 2
      ;;
    --bench-dir)
      BENCH_DIR="$2"
      shift 2
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
 done

if [[ -z "$BENCH_DIR" ]]; then
  BENCH_DIR="$BUILD_DIR/benchmarks"
fi

PRIMEC_BIN="$BUILD_DIR/primec"
if [[ ! -x "$PRIMEC_BIN" ]]; then
  echo "[benchmark.sh] ERROR: primec not found: $PRIMEC_BIN" >&2
  exit 2
fi

RUN_NATIVE=0
if [[ "$(uname -s)" == "Darwin" ]]; then
  machine="$(uname -m)"
  if [[ "$machine" == "arm64" || "$machine" == "aarch64" ]]; then
    RUN_NATIVE=1
  fi
fi

CC="${CC:-cc}"
CXX="${CXX:-c++}"
RUSTC="${RUSTC:-rustc}"
PYTHON="${PYTHON:-python3}"

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "[benchmark.sh] ERROR: C compiler not found: $CC" >&2
  exit 2
fi
if ! command -v "$CXX" >/dev/null 2>&1; then
  echo "[benchmark.sh] ERROR: C++ compiler not found: $CXX" >&2
  exit 2
fi
if ! command -v "$RUSTC" >/dev/null 2>&1; then
  echo "[benchmark.sh] ERROR: Rust compiler not found: $RUSTC" >&2
  exit 2
fi
if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "[benchmark.sh] ERROR: Python not found: $PYTHON" >&2
  exit 2
fi

mkdir -p "$BENCH_DIR"

C_SRC="$ROOT_DIR/benchmarks/aggregate.c"
CPP_SRC="$ROOT_DIR/benchmarks/aggregate.cpp"
RS_SRC="$ROOT_DIR/benchmarks/aggregate.rs"
PRIME_SRC="$ROOT_DIR/benchmarks/aggregate.prime"
C_JSON_SRC="$ROOT_DIR/benchmarks/json_scan.c"
CPP_JSON_SRC="$ROOT_DIR/benchmarks/json_scan.cpp"
RS_JSON_SRC="$ROOT_DIR/benchmarks/json_scan.rs"
PRIME_JSON_SRC="$ROOT_DIR/benchmarks/json_scan.prime"
C_JSON_PARSE_SRC="$ROOT_DIR/benchmarks/json_parse.c"
CPP_JSON_PARSE_SRC="$ROOT_DIR/benchmarks/json_parse.cpp"
RS_JSON_PARSE_SRC="$ROOT_DIR/benchmarks/json_parse.rs"
PRIME_JSON_PARSE_SRC="$ROOT_DIR/benchmarks/json_parse.prime"

C_EXE="$BENCH_DIR/aggregate_c"
CPP_EXE="$BENCH_DIR/aggregate_cpp"
RS_EXE="$BENCH_DIR/aggregate_rust"
PRIME_CPP="$BENCH_DIR/aggregate_primestruct.cpp"
PRIME_CPP_EXE="$BENCH_DIR/aggregate_primestruct_cpp"
PRIME_NATIVE_EXE="$BENCH_DIR/aggregate_primestruct_native"
C_JSON_EXE="$BENCH_DIR/json_scan_c"
CPP_JSON_EXE="$BENCH_DIR/json_scan_cpp"
RS_JSON_EXE="$BENCH_DIR/json_scan_rust"
PRIME_JSON_CPP="$BENCH_DIR/json_scan_primestruct.cpp"
PRIME_JSON_CPP_EXE="$BENCH_DIR/json_scan_primestruct_cpp"
PRIME_JSON_NATIVE_EXE="$BENCH_DIR/json_scan_primestruct_native"
C_JSON_PARSE_EXE="$BENCH_DIR/json_parse_c"
CPP_JSON_PARSE_EXE="$BENCH_DIR/json_parse_cpp"
RS_JSON_PARSE_EXE="$BENCH_DIR/json_parse_rust"
PRIME_JSON_PARSE_CPP="$BENCH_DIR/json_parse_primestruct.cpp"
PRIME_JSON_PARSE_CPP_EXE="$BENCH_DIR/json_parse_primestruct_cpp"
PRIME_JSON_PARSE_NATIVE_EXE="$BENCH_DIR/json_parse_primestruct_native"

"$CC" -O3 -DNDEBUG -std=c11 "$C_SRC" -o "$C_EXE"
"$CXX" -O3 -DNDEBUG -std=c++23 "$CPP_SRC" -o "$CPP_EXE"
"$RUSTC" -O "$RS_SRC" -o "$RS_EXE"
"$PRIMEC_BIN" --emit=cpp "$PRIME_SRC" -o "$PRIME_CPP" --entry /main
"$CXX" -O3 -DNDEBUG -std=c++23 "$PRIME_CPP" -o "$PRIME_CPP_EXE"
if [[ $RUN_NATIVE -eq 1 ]]; then
  "$PRIMEC_BIN" --emit=native "$PRIME_SRC" -o "$PRIME_NATIVE_EXE" --entry /main
fi
"$CC" -O3 -DNDEBUG -std=c11 "$C_JSON_SRC" -o "$C_JSON_EXE"
"$CXX" -O3 -DNDEBUG -std=c++23 "$CPP_JSON_SRC" -o "$CPP_JSON_EXE"
"$RUSTC" -O "$RS_JSON_SRC" -o "$RS_JSON_EXE"
"$PRIMEC_BIN" --emit=cpp "$PRIME_JSON_SRC" -o "$PRIME_JSON_CPP" --entry /main
"$CXX" -O3 -DNDEBUG -std=c++23 "$PRIME_JSON_CPP" -o "$PRIME_JSON_CPP_EXE"
if [[ $RUN_NATIVE -eq 1 ]]; then
  "$PRIMEC_BIN" --emit=native "$PRIME_JSON_SRC" -o "$PRIME_JSON_NATIVE_EXE" --entry /main
fi
"$CC" -O3 -DNDEBUG -std=c11 "$C_JSON_PARSE_SRC" -o "$C_JSON_PARSE_EXE"
"$CXX" -O3 -DNDEBUG -std=c++23 "$CPP_JSON_PARSE_SRC" -o "$CPP_JSON_PARSE_EXE"
"$RUSTC" -O "$RS_JSON_PARSE_SRC" -o "$RS_JSON_PARSE_EXE"
"$PRIMEC_BIN" --emit=cpp "$PRIME_JSON_PARSE_SRC" -o "$PRIME_JSON_PARSE_CPP" --entry /main
"$CXX" -O3 -DNDEBUG -std=c++23 "$PRIME_JSON_PARSE_CPP" -o "$PRIME_JSON_PARSE_CPP_EXE"
if [[ $RUN_NATIVE -eq 1 ]]; then
  "$PRIMEC_BIN" --emit=native "$PRIME_JSON_PARSE_SRC" -o "$PRIME_JSON_PARSE_NATIVE_EXE" --entry /main
fi

(
  cd "$BENCH_DIR"
  "$PYTHON" - "$RUNS" "$RUN_NATIVE" <<'PY'
import subprocess
import sys
import time

runs = int(sys.argv[1])
run_native = int(sys.argv[2]) != 0

def primestruct_entries(name: str):
    entries = [
        ("primestruct_cpp", f"./{name}_primestruct_cpp"),
    ]
    if run_native:
        entries.append(("primestruct_native", f"./{name}_primestruct_native"))
    return entries

benchmarks = [
    ("aggregate", [
        ("c", "./aggregate_c"),
        ("cpp", "./aggregate_cpp"),
        ("rust", "./aggregate_rust"),
    ] + primestruct_entries("aggregate")),
    ("json_scan", [
        ("c", "./json_scan_c"),
        ("cpp", "./json_scan_cpp"),
        ("rust", "./json_scan_rust"),
    ] + primestruct_entries("json_scan")),
    ("json_parse", [
        ("c", "./json_parse_c"),
        ("cpp", "./json_parse_cpp"),
        ("rust", "./json_parse_rust"),
    ] + primestruct_entries("json_parse")),
]

print("Benchmark runs:", runs)
if not run_native:
    print("Note: PrimeStruct native backend not available; skipping.")

def run_entry(label: str, path: str) -> None:
    print(f"\n== {label} ==")
    check = subprocess.run([path], capture_output=True, text=True, check=True)
    output = check.stdout.strip()
    if output:
        print("output:", output)
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        subprocess.run([path], stdout=subprocess.DEVNULL, check=True)
        times.append(time.perf_counter() - start)
    mean = sum(times) / len(times)
    median = sorted(times)[len(times) // 2]
    print(f"mean:   {mean:.6f}s")
    print(f"median: {median:.6f}s")

for bench, entries in benchmarks:
    print(f"\n=== {bench} ===")
    for label, exe in entries:
        run_entry(label, exe)
PY
)
