#!/usr/bin/env python3
"""Times the owned `String` against `std::string` (docs/spec/strings-and-views.md, Parity with `std::string`).

Each benchmarks/strings/<name>.prime has a <name>.cpp twin that does the same work and prints the same number.
The .prime file is compiled with `primec --emit=optexe`, the .cpp file with the host C++ compiler at -O2; both are
run RUNS times and the medians are compared. The outputs must match. With --budget the ratio of each workload
(optexe time / std::string time) must stay under its recorded limit. --vm also times the interpreter (slow).

  python3 scripts/benchmark_strings.py --build-dir build-release [--runs 5] [--budget benchmarks/strings/budget.json]
                                       [--report-json out.json] [--vm]
"""
import argparse
import json
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKLOADS = ["append_bytes", "append_chunks", "short_strings", "insert_front", "copy_text", "replace_middle", "request_heap",
             "request_arena"]


def run_timed(command, runs):
    # One untimed launch first: macOS scans a freshly built executable on its first run, which would swamp a
    # sub-100 ms workload.
    subprocess.run(command, capture_output=True, text=True, check=True)
    times, output = [], None
    for _ in range(runs):
        start = time.perf_counter()
        done = subprocess.run(command, capture_output=True, text=True, check=True)
        times.append(time.perf_counter() - start)
        if output is None:
            output = done.stdout.strip()
    return statistics.median(times), output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-release")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--budget")
    parser.add_argument("--report-json")
    parser.add_argument("--vm", action="store_true")
    parser.add_argument("--only", help="comma separated workload names")
    args = parser.parse_args()

    build = (ROOT / args.build_dir).resolve()
    primec = build / "primec"
    primevm = build / "primevm"
    cxx = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if not primec.exists() or cxx is None:
        print("benchmark_strings: needs a release build with primec and a host C++ compiler", file=sys.stderr)
        return 2
    budget = json.loads(Path(args.budget).read_text()) if args.budget else {}
    names = args.only.split(",") if args.only else WORKLOADS
    stdlib = str(ROOT / "stdlib")

    rows, failed = [], False
    with tempfile.TemporaryDirectory(prefix="benchmark_strings_") as scratch:
        for name in names:
            prime = ROOT / "benchmarks" / "strings" / f"{name}.prime"
            cpp = ROOT / "benchmarks" / "strings" / f"{name}.cpp"
            optexe = Path(scratch) / f"{name}_optexe"
            reference = Path(scratch) / f"{name}_std"
            subprocess.run([str(primec), "--emit=optexe", str(prime), "-o", str(optexe), "-I", stdlib], check=True,
                           capture_output=True)
            subprocess.run([cxx, "-O2", "-std=c++20", "-o", str(reference), str(cpp)], check=True, capture_output=True)
            std_time, std_out = run_timed([str(reference)], args.runs)
            opt_time, opt_out = run_timed([str(optexe)], args.runs)
            row = {"workload": name, "std_string_seconds": std_time, "optexe_seconds": opt_time,
                   "optexe_over_std": opt_time / std_time, "output_matches": std_out == opt_out}
            if args.vm and primevm.exists():
                vm_time, vm_out = run_timed([str(primevm), str(prime), "-I", stdlib], 1)
                row["vm_seconds"] = vm_time
                row["vm_over_std"] = vm_time / std_time
                row["output_matches"] = row["output_matches"] and vm_out == std_out
            limit = budget.get(name, {}).get("max_optexe_over_std")
            row["limit"] = limit
            row["within_budget"] = limit is None or row["optexe_over_std"] <= limit
            failed = failed or not row["output_matches"] or not row["within_budget"]
            rows.append(row)

    print(f"{'workload':16} {'std::string':>12} {'optexe':>9} {'ratio':>7} {'limit':>7}  status")
    for row in rows:
        status = "ok" if row["output_matches"] and row["within_budget"] else (
            "OUTPUT DIFFERS" if not row["output_matches"] else "OVER BUDGET")
        limit = "" if row["limit"] is None else f"{row['limit']:.1f}"
        print(f"{row['workload']:16} {row['std_string_seconds']:11.3f}s {row['optexe_seconds']:8.3f}s "
              f"{row['optexe_over_std']:6.2f}x {limit:>7}  {status}")
    if args.report_json:
        Path(args.report_json).write_text(json.dumps({"schema": "primestruct_string_benchmark_v1", "runs": args.runs,
                                                     "rows": rows}, indent=2) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
