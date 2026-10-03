#!/usr/bin/env python3
"""Run the benchmark programs through every execution backend and optimization level.

    python3 scripts/benchmark_backends.py --build-dir build-release [--runs 3]
        [--programs aggregate,json_scan,json_parse] [--configs vm-O0,vm-O2,...]
        [--json report.json] [--no-c]

For each program in benchmarks/ it measures wall time (median of --runs) of:

  vm-step-O0   primevm -O0 on the checked step kernel (PRIMEVM_KERNEL=step)
  vm-O0        primevm -O0 (flat loop)
  vm-O2        primevm -O2
  native-O0    primec --emit=native -O0, then the executable
  native-O2    primec --emit=native -O2, then the executable
  optexe-O2    primec --emit=optexe -O2 (host clang++ -O2), then the executable
  exe          primec --emit=exe (old C++ emitter at clang++ -O0), then the executable
  c            benchmarks/<name>.c with cc -O3 (the speed reference), unless --no-c

VM rows include the compile time (primevm compiles and runs); executable rows
list run time only, with the compile time in a separate column. Rows that cannot
run on this machine (native is Linux x86_64 and macOS arm64 only) or fail are
reported as `n/a`. Output is a markdown table on stdout and, with --json, a
machine-readable report. This script records numbers; it does not gate on them
(scripts/benchmark.sh and check_benchmark_report.py own the regression gates).
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

TIMEOUT_SECONDS = 600
DEFAULT_CONFIGS = ["vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe", "c"]


def timed(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> tuple[float, subprocess.CompletedProcess]:
    start = time.perf_counter()
    completed = subprocess.run(
        command, cwd=cwd, capture_output=True, timeout=TIMEOUT_SECONDS, env=env, check=False
    )
    return time.perf_counter() - start, completed


def median_run(command: list[str], cwd: Path, runs: int, env: dict[str, str] | None = None) -> tuple[float | None, bytes]:
    times: list[float] = []
    output = b""
    for _ in range(runs):
        seconds, completed = timed(command, cwd, env)
        if completed.returncode != 0:
            return None, completed.stderr
        times.append(seconds)
        output = completed.stdout
    return statistics.median(times), output


def native_supported() -> bool:
    machine = platform.machine().lower()
    system = platform.system()
    return (system == "Linux" and machine in ("x86_64", "amd64")) or (
        system == "Darwin" and machine in ("arm64", "aarch64")
    )


def measure(config: str, name: str, source: Path, build_dir: Path, work: Path, runs: int) -> dict:
    """Returns {seconds, compile_seconds, output, note}."""
    result: dict = {"seconds": None, "compile_seconds": None, "output": None, "note": ""}
    primevm = str(build_dir / "primevm")
    primec = str(build_dir / "primec")
    entry = ["--entry", "/main"]
    if config.startswith("vm"):
        env = dict(os.environ)
        level = "-O2" if config.endswith("O2") else "-O0"
        if config.startswith("vm-step"):
            env["PRIMEVM_KERNEL"] = "step"
        seconds, output = median_run([primevm, str(source), *entry, level], build_dir, runs, env)
        result.update(seconds=seconds, output=output)
        if seconds is None:
            result["note"] = output.decode(errors="replace")[:200]
        return result
    if config in ("native-O0", "native-O2", "optexe-O2", "exe"):
        if config.startswith("native") and not native_supported():
            result["note"] = "native is not supported on this machine"
            return result
        kind = {"native-O0": "native", "native-O2": "native", "optexe-O2": "optexe", "exe": "exe"}[config]
        flags = [f"-O{config[-1]}"] if config != "exe" else []
        binary = work / f"{name}_{config}"
        compile_seconds, compiled = timed(
            [primec, str(source), f"--emit={kind}", *flags, *entry, "-o", str(binary)], build_dir
        )
        if compiled.returncode != 0:
            result["note"] = compiled.stderr.decode(errors="replace")[:200]
            return result
        seconds, output = median_run([str(binary)], build_dir, runs)
        result.update(seconds=seconds, compile_seconds=compile_seconds, output=output)
        if seconds is None:
            result["note"] = output.decode(errors="replace")[:200]
        return result
    if config == "c":
        reference = source.with_suffix(".c")
        cc = shutil.which(os.environ.get("CC", "cc"))
        if not reference.exists() or cc is None:
            result["note"] = "no C reference or compiler"
            return result
        binary = work / f"{name}_c"
        compile_seconds, compiled = timed([cc, "-O3", "-DNDEBUG", "-std=c11", str(reference), "-o", str(binary)], build_dir)
        if compiled.returncode != 0:
            result["note"] = compiled.stderr.decode(errors="replace")[:200]
            return result
        seconds, output = median_run([str(binary)], build_dir, runs)
        result.update(seconds=seconds, compile_seconds=compile_seconds, output=output)
        return result
    raise SystemExit(f"unknown config: {config}")


def fmt(value: float | None) -> str:
    if value is None:
        return "n/a"
    if value < 0.01:
        return f"{value * 1000:.2f} ms"
    if value < 1:
        return f"{value * 1000:.0f} ms"
    return f"{value:.2f} s"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=Path("build-release"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--programs", default="")
    parser.add_argument("--configs", default=",".join(DEFAULT_CONFIGS))
    parser.add_argument("--json", type=Path, default=None)
    parser.add_argument("--no-c", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    for tool in ("primec", "primevm"):
        if not (build_dir / tool).exists():
            print(f"error: {build_dir / tool} not found; build first", file=sys.stderr)
            return 2
    configs = [config for config in args.configs.split(",") if config and not (args.no_c and config == "c")]
    bench_dir = args.root / "benchmarks"
    names = [n for n in args.programs.split(",") if n] or sorted(p.stem for p in bench_dir.glob("*.prime"))

    report: dict = {"machine": platform.machine(), "runs": args.runs, "programs": {}}
    with tempfile.TemporaryDirectory(prefix="primec_bench_") as tmp:
        work = Path(tmp)
        for name in names:
            source = bench_dir / f"{name}.prime"
            if not source.exists():
                print(f"error: {source} not found", file=sys.stderr)
                return 2
            rows = {}
            reference_output: bytes | None = None
            for config in configs:
                row = measure(config, name, source, build_dir, work, args.runs)
                if row["output"] is not None:
                    if reference_output is None:
                        reference_output = row["output"]
                    elif row["output"] != reference_output:
                        row["note"] = (row["note"] + " OUTPUT DIFFERS").strip()
                row.pop("output", None)
                rows[config] = row
            report["programs"][name] = rows

    # Markdown table: one row per config, one column per program (run seconds).
    header = "| config | " + " | ".join(names) + " |"
    print(header)
    print("| --- | " + " | ".join("---" for _ in names) + " |")
    for config in configs:
        cells = []
        for name in names:
            row = report["programs"][name][config]
            cell = fmt(row["seconds"])
            if row["compile_seconds"] is not None:
                cell += f" (+{fmt(row['compile_seconds'])} compile)"
            if "DIFFERS" in row["note"]:
                cell += " DIFFERS"
            cells.append(cell)
        print(f"| {config} | " + " | ".join(cells) + " |")
    notes = [
        f"{name}/{config}: {row['note']}"
        for name, rows in report["programs"].items()
        for config, row in rows.items()
        if row["note"]
    ]
    for note in notes:
        print(f"note: {note}", file=sys.stderr)
    if args.json:
        args.json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 1 if any("DIFFERS" in n for n in notes) else 0


if __name__ == "__main__":
    sys.exit(main())
