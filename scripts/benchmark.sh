#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
RUNS="${BENCH_RUNS:-5}"
BENCH_DIR="${BENCH_DIR:-}"
COMPILE_RUNS="${BENCH_COMPILE_RUNS:-}"
COMPILE_LINES="${BENCH_COMPILE_LINES:-100000}"

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
if [[ -z "$COMPILE_RUNS" ]]; then
  COMPILE_RUNS="$RUNS"
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

COMPILE_C_SRC="$BENCH_DIR/compile_speed.c"
COMPILE_CPP_SRC="$BENCH_DIR/compile_speed.cpp"
COMPILE_RS_SRC="$BENCH_DIR/compile_speed.rs"
COMPILE_SRC="$BENCH_DIR/compile_speed.prime"
COMPILE_C_EXE="$BENCH_DIR/compile_speed_c"
COMPILE_CPP_EXE="$BENCH_DIR/compile_speed_cpp"
COMPILE_RS_EXE="$BENCH_DIR/compile_speed_rust"
COMPILE_CPP="$BENCH_DIR/compile_speed_primestruct.cpp"
COMPILE_NATIVE="$BENCH_DIR/compile_speed_primestruct_native"

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

"$PYTHON" - "$COMPILE_C_SRC" "$COMPILE_CPP_SRC" "$COMPILE_RS_SRC" \
  "$COMPILE_SRC" "$COMPILE_LINES" <<'PY'
import sys

paths = {
    "c": sys.argv[1],
    "cpp": sys.argv[2],
    "rs": sys.argv[3],
    "prime": sys.argv[4],
}
target_lines = int(sys.argv[5])

def emit_file(path: str, header: list[str], func_lines: list[str], footer: list[str]) -> None:
    lines = header[:]
    reserve = len(footer)
    remaining = target_lines - len(lines) - reserve
    if remaining < 0:
        raise SystemExit(f"compile benchmark requires at least {reserve + 1} lines")

    block = len(func_lines)
    num_funcs = remaining // block
    extra = remaining % block

    for i in range(num_funcs):
        for line in func_lines:
            lines.append(line.replace("{i}", str(i)))

    for _ in range(extra):
        lines.append("")

    lines.extend(footer)

    if len(lines) != target_lines:
        raise SystemExit(f"generated {len(lines)} lines, expected {target_lines}")

    with open(path, "w", newline="\n") as handle:
        handle.write("\n".join(lines))
        handle.write("\n")

emit_file(
    paths["c"],
    ["#include <stdint.h>", ""],
    [
        "static int32_t noop{i}(int32_t x) {",
        "  int32_t y = x + 1;",
        "  return y;",
        "}",
    ],
    [
        "int main(void) {",
        "  return 0;",
        "}",
    ],
)

emit_file(
    paths["cpp"],
    ["#include <cstdint>", ""],
    [
        "static int32_t noop{i}(int32_t x) {",
        "  int32_t y = x + 1;",
        "  return y;",
        "}",
    ],
    [
        "int main() {",
        "  return 0;",
        "}",
    ],
)

emit_file(
    paths["rs"],
    [],
    [
        "fn noop{i}(x: i32) -> i32 {",
        "  let y = x + 1;",
        "  y",
        "}",
    ],
    [
        "fn main() {",
        "}",
    ],
)

emit_file(
    paths["prime"],
    ["namespace bench {"],
    [
        "[void]",
        "noop{i}() {",
        "  return()",
        "}",
    ],
    [
        "[i32]",
        "main() {",
        "  return(0)",
        "}",
        "}",
    ],
)
PY

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
  "$PYTHON" - "$RUNS" "$RUN_NATIVE" "$COMPILE_RUNS" "$COMPILE_LINES" "$COMPILE_SRC" \
    "$COMPILE_C_SRC" "$COMPILE_CPP_SRC" "$COMPILE_RS_SRC" \
    "$CC" "$CXX" "$RUSTC" "$PRIMEC_BIN" \
    "$COMPILE_C_EXE" "$COMPILE_CPP_EXE" "$COMPILE_RS_EXE" \
    "$COMPILE_CPP" "$COMPILE_NATIVE" <<'PY'
import subprocess
import sys
import time
from pathlib import Path

runs = int(sys.argv[1])
run_native = int(sys.argv[2]) != 0
compile_runs = int(sys.argv[3])
compile_lines = int(sys.argv[4])
compile_src = sys.argv[5]
compile_c_src = sys.argv[6]
compile_cpp_src = sys.argv[7]
compile_rs_src = sys.argv[8]
cc = sys.argv[9]
cxx = sys.argv[10]
rustc = sys.argv[11]
primec_bin = sys.argv[12]
compile_c_exe = sys.argv[13]
compile_cpp_exe = sys.argv[14]
compile_rs_exe = sys.argv[15]
compile_cpp = sys.argv[16]
compile_native = sys.argv[17]

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
print("Compile benchmark lines:", compile_lines)
print("Compile benchmark runs:", compile_runs)

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

def run_compile_entry(label: str, cmd: list[str], outputs: list[str]) -> None:
    print(f"\n== {label} ==")
    check = subprocess.run(cmd, capture_output=True, text=True)
    if check.returncode != 0:
        print("compile failed")
        if check.stdout:
            print("stdout:", check.stdout.strip())
        if check.stderr:
            print("stderr:", check.stderr.strip())
        check.check_returncode()
    times = []
    for _ in range(compile_runs):
        for out in outputs:
            try:
                Path(out).unlink()
            except FileNotFoundError:
                pass
        start = time.perf_counter()
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        times.append(time.perf_counter() - start)
    mean = sum(times) / len(times)
    median = sorted(times)[len(times) // 2]
    print(f"mean:   {mean:.6f}s")
    print(f"median: {median:.6f}s")

for bench, entries in benchmarks:
    print(f"\n=== {bench} ===")
    for label, exe in entries:
        run_entry(label, exe)

compile_entries = [
    ("c", [
        cc,
        "-O3",
        "-DNDEBUG",
        "-std=c11",
        compile_c_src,
        "-o",
        compile_c_exe,
    ], [compile_c_exe]),
    ("cpp", [
        cxx,
        "-O3",
        "-DNDEBUG",
        "-std=c++23",
        compile_cpp_src,
        "-o",
        compile_cpp_exe,
    ], [compile_cpp_exe]),
    ("rust", [
        rustc,
        "-O",
        compile_rs_src,
        "-o",
        compile_rs_exe,
    ], [compile_rs_exe]),
    ("primestruct_cpp", [
        primec_bin,
        "--emit=cpp",
        compile_src,
        "-o",
        compile_cpp,
        "--entry",
        "/bench/main",
    ], [compile_cpp]),
]
if run_native:
    compile_entries.append((
        "primestruct_native",
        [
            primec_bin,
            "--emit=native",
            compile_src,
            "-o",
            compile_native,
            "--entry",
            "/bench/main",
        ],
        [compile_native],
    ))

print("\n=== compile_speed ===")
for label, cmd, outputs in compile_entries:
    run_compile_entry(label, cmd, outputs)
PY
)
