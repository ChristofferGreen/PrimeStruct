#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set

NON_EXPENSIVE_PARALLELISM = 8


def run_ctest_json(build_dir: Path) -> Dict:
    proc = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(proc.stdout)


def run_ctest(build_dir: Path, args: Sequence[str]) -> int:
    proc = subprocess.run(["ctest", "--test-dir", str(build_dir), *args])
    return int(proc.returncode)


def all_test_names(build_dir: Path) -> Set[str]:
    data = run_ctest_json(build_dir)
    return {test.get("name", "") for test in data.get("tests", []) if test.get("name")}


def tests_with_label(build_dir: Path, label: str) -> Set[str]:
    data = run_ctest_json(build_dir)
    result: Set[str] = set()
    for test in data.get("tests", []):
        name = test.get("name")
        if not name:
            continue
        for prop in test.get("properties", []):
            if prop.get("name") != "LABELS":
                continue
            value = prop.get("value", [])
            if isinstance(value, str):
                labels = [value]
            elif isinstance(value, list):
                labels = [str(v) for v in value]
            else:
                labels = []
            if label in labels:
                result.add(name)
                break
    return result


def read_line_set(path: Path) -> Set[str]:
    if not path.exists():
        return set()
    return {line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()}


def write_line_set(path: Path, values: Iterable[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sorted_values = sorted({v for v in values if v})
    path.write_text("".join(f"{v}\n" for v in sorted_values), encoding="utf-8")


def find_fast_cost_file(build_dir: Path) -> Path | None:
    local_file = build_dir / "Testing" / "Temporary" / "CTestCostData.txt"
    if local_file.exists():
        return local_file

    build_dir_name = build_dir.name
    root_dir = Path(__file__).resolve().parents[1]

    try:
        proc = subprocess.run(
            ["git", "-C", str(root_dir), "worktree", "list", "--porcelain"],
            check=True,
            capture_output=True,
            text=True,
        )
    except Exception:
        return None

    main_worktree = None
    for line in proc.stdout.splitlines():
        if line.startswith("worktree "):
            main_worktree = Path(line[len("worktree ") :].strip())
            break

    if main_worktree is None:
        return None

    fallback_file = main_worktree / build_dir_name / "Testing" / "Temporary" / "CTestCostData.txt"
    return fallback_file if fallback_file.exists() else None


def build_fast_exclude_file(build_dir: Path, threshold_seconds: float, output_file: Path) -> Path | None:
    cost_file = find_fast_cost_file(build_dir)
    if cost_file is None:
        write_line_set(output_file, [])
        return None

    offenders: Set[str] = set()
    for raw_line in cost_file.read_text(encoding="utf-8").splitlines():
        parts = raw_line.split()
        if len(parts) < 3:
            continue
        name = parts[0]
        try:
            runtime = float(parts[2])
        except ValueError:
            continue
        if runtime > threshold_seconds:
            offenders.add(name)

    write_line_set(output_file, offenders)
    return cost_file


def detect_runtime_offenders(build_dir: Path, threshold_seconds: float) -> Set[str]:
    cost_file = find_fast_cost_file(build_dir)
    if cost_file is None:
        return set()

    offenders: Set[str] = set()
    for raw_line in cost_file.read_text(encoding="utf-8").splitlines():
        parts = raw_line.split()
        if len(parts) < 3:
            continue
        name = parts[0]
        try:
            runtime = float(parts[2])
        except ValueError:
            continue
        if runtime > threshold_seconds:
            offenders.add(name)
    return offenders


def detect_memory_offenders(build_dir: Path, threshold_mb: int) -> Set[str]:
    offenders: Set[str] = set()

    metrics_file = build_dir / "Testing" / "Temporary" / "PrimeStructCommandMetrics.log"
    if metrics_file.exists():
        for raw_line in metrics_file.read_text(encoding="utf-8").splitlines():
            parts = raw_line.split("\t")
            if len(parts) < 3:
                continue
            test_name = parts[0].strip()
            if not test_name:
                continue
            try:
                rss_mb = int(parts[1])
            except ValueError:
                rss_mb = 0
            try:
                guard_triggered = int(parts[2])
            except ValueError:
                guard_triggered = 0
            if rss_mb > threshold_mb or guard_triggered > 0:
                offenders.add(test_name)

    last_test_log = build_dir / "Testing" / "Temporary" / "LastTest.log"
    if last_test_log.exists():
        current = ""
        pattern = re.compile(r"^\d+/\d+\s+Testing:\s+(.+)$")
        for raw_line in last_test_log.read_text(encoding="utf-8", errors="replace").splitlines():
            m = pattern.match(raw_line)
            if m:
                current = m.group(1).strip()
                continue
            if not current:
                continue
            if "PRIMESTRUCT_EXPENSIVE_MEMORY_OFFENDER" in raw_line or "PRIMESTRUCT_MEMORY_GUARD_TRIGGERED" in raw_line:
                offenders.add(current)

    return offenders


def refresh_marked(build_dir: Path, auto_file: Path, output_file: Path) -> None:
    valid_tests = all_test_names(build_dir)
    labeled = tests_with_label(build_dir, "expensive")
    auto = read_line_set(auto_file)
    merged = (labeled | auto) & valid_tests
    write_line_set(output_file, merged)


def update_auto(
    build_dir: Path,
    auto_file: Path,
    runtime_threshold: float,
    memory_threshold: int,
) -> List[str]:
    valid_tests = all_test_names(build_dir)
    runtime_offenders = detect_runtime_offenders(build_dir, runtime_threshold) & valid_tests
    memory_offenders = detect_memory_offenders(build_dir, memory_threshold) & valid_tests

    existing = read_line_set(auto_file)
    merged = existing | runtime_offenders | memory_offenders
    write_line_set(auto_file, merged)

    new_offenders = sorted(merged - existing)
    for name in new_offenders:
        reasons: List[str] = []
        if name in runtime_offenders:
            reasons.append(f"runtime>{runtime_threshold:g}s")
        if name in memory_offenders:
            reasons.append(f"memory>{memory_threshold}MB or memory-guard")
        reason_text = " and ".join(reasons) if reasons else "auto"
        print(f"Offender detected ({reason_text}): {name}")
    return new_offenders


def run_non_expensive_suite(build_dir: Path, marked_file: Path, fast_exclude_file: Path | None) -> int:
    combined: Set[str] = set()
    combined |= read_line_set(marked_file)
    if fast_exclude_file is not None:
        combined |= read_line_set(fast_exclude_file)

    if combined:
        with tempfile.NamedTemporaryFile(prefix="primestruct-ctest-exclude-", delete=False) as tmp:
            combined_file = Path(tmp.name)
        try:
            write_line_set(combined_file, combined)
            return run_ctest(
                build_dir,
                ["--output-on-failure", "--parallel", str(NON_EXPENSIVE_PARALLELISM), "--exclude-from-file", str(combined_file)],
            )
        finally:
            combined_file.unlink(missing_ok=True)

    return run_ctest(build_dir, ["--output-on-failure", "--parallel", str(NON_EXPENSIVE_PARALLELISM)])


def run_expensive_suite_serial(build_dir: Path, marked_file: Path) -> int:
    if not read_line_set(marked_file):
        print("No marked expensive tests to run.")
        return 0
    print("Running marked expensive tests serially (--include-expensive-tests).")
    return run_ctest(build_dir, ["--output-on-failure", "--parallel", "1", "--tests-from-file", str(marked_file)])


def cmd_refresh_marked(args: argparse.Namespace) -> int:
    refresh_marked(Path(args.build_dir), Path(args.auto_file), Path(args.output))
    return 0


def cmd_update_auto(args: argparse.Namespace) -> int:
    update_auto(
        Path(args.build_dir),
        Path(args.auto_file),
        float(args.runtime_threshold),
        int(args.memory_threshold),
    )
    return 0


def cmd_run_gate(args: argparse.Namespace) -> int:
    build_dir = Path(args.build_dir)
    include_expensive = int(args.include_expensive_tests) == 1
    runtime_threshold = float(args.runtime_threshold)
    memory_threshold = int(args.memory_threshold)

    auto_file = build_dir / "Testing" / "Temporary" / "PrimeStructAutoExpensiveTests.txt"
    metrics_file = build_dir / "Testing" / "Temporary" / "PrimeStructCommandMetrics.log"
    auto_file.parent.mkdir(parents=True, exist_ok=True)
    auto_file.touch()
    metrics_file.write_text("", encoding="utf-8")
    os.environ["PRIMESTRUCT_COMMAND_METRICS_FILE"] = str(metrics_file)
    print(
        f"Local safety guard: PRIMESTRUCT_TEST_MEMORY_GUARD_MB={os.environ.get('PRIMESTRUCT_TEST_MEMORY_GUARD_MB', '')}",
        flush=True,
    )

    with tempfile.NamedTemporaryFile(prefix="primestruct-marked-expensive-", delete=False) as tmp_marked:
        marked_file = Path(tmp_marked.name)

    fast_exclude_file: Path | None = None
    try:
        refresh_marked(build_dir, auto_file, marked_file)

        marked = read_line_set(marked_file)
        if marked:
            if include_expensive:
                print(f"Marked expensive tests: {len(marked)} (will run serially).", flush=True)
            else:
                print("Marked expensive tests: "
                      f"{len(marked)} (excluded by default; pass --include-expensive-tests to run).", flush=True)

        with tempfile.NamedTemporaryFile(prefix="primestruct-fast-tests-", delete=False) as tmp_fast:
            fast_exclude_file = Path(tmp_fast.name)
        cost_file = build_fast_exclude_file(build_dir, runtime_threshold, fast_exclude_file)
        if cost_file is None:
            print(
                "Default gate: no CTest timing data found locally or in the main worktree; running full suite.",
                file=sys.stderr,
            )
        else:
            excluded_count = len(read_line_set(fast_exclude_file))
            local_cost_file = build_dir / "Testing" / "Temporary" / "CTestCostData.txt"
            if cost_file != local_cost_file:
                print(f"Default gate: using fallback timing data from {cost_file}.", flush=True)
            if excluded_count > 0:
                print(
                    f"Default gate: excluding {excluded_count} tests with historical runtime > {runtime_threshold:g}s.",
                    flush=True,
                )
            else:
                print(f"Default gate: no historical tests exceed {runtime_threshold:g}s.", flush=True)

        non_expensive_status = run_non_expensive_suite(build_dir, marked_file, fast_exclude_file)
        update_auto(build_dir, auto_file, runtime_threshold, memory_threshold)

        if non_expensive_status != 0 and not include_expensive:
            refresh_marked(build_dir, auto_file, marked_file)
            if read_line_set(marked_file):
                print("Retrying non-expensive suite once after auto-marking offenders.", flush=True)
                non_expensive_status = run_non_expensive_suite(build_dir, marked_file, fast_exclude_file)
                update_auto(build_dir, auto_file, runtime_threshold, memory_threshold)

        expensive_status = 0
        if include_expensive:
            refresh_marked(build_dir, auto_file, marked_file)
            expensive_status = run_expensive_suite_serial(build_dir, marked_file)
            update_auto(build_dir, auto_file, runtime_threshold, memory_threshold)

        return 0 if (non_expensive_status == 0 and expensive_status == 0) else 1
    finally:
        marked_file.unlink(missing_ok=True)
        if fast_exclude_file is not None:
            fast_exclude_file.unlink(missing_ok=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Manage PrimeStruct expensive test lists.")
    sub = parser.add_subparsers(dest="command", required=True)

    refresh = sub.add_parser("refresh-marked", help="Merge expensive labels + auto offenders.")
    refresh.add_argument("--build-dir", required=True)
    refresh.add_argument("--auto-file", required=True)
    refresh.add_argument("--output", required=True)
    refresh.set_defaults(func=cmd_refresh_marked)

    update = sub.add_parser("update-auto", help="Update auto offenders from runtime/memory data.")
    update.add_argument("--build-dir", required=True)
    update.add_argument("--auto-file", required=True)
    update.add_argument("--runtime-threshold", required=True)
    update.add_argument("--memory-threshold", required=True)
    update.set_defaults(func=cmd_update_auto)

    run_gate = sub.add_parser("run-gate", help="Run ctest with expensive test management.")
    run_gate.add_argument("--build-dir", required=True)
    run_gate.add_argument("--include-expensive-tests", required=True)
    run_gate.add_argument("--runtime-threshold", required=True)
    run_gate.add_argument("--memory-threshold", required=True)
    run_gate.set_defaults(func=cmd_run_gate)

    return parser


def main(argv: List[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
