#!/usr/bin/env python3
"""Run semantic-memory benchmark/trend checks and always archive machine-readable artifacts."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run semantic-memory benchmark/trend checks and archive artifacts for CI.")
    parser.add_argument("--repo-root", default=".", help="Repository root.")
    parser.add_argument("--primec", default="build-release/primec", help="Path to primec executable.")
    parser.add_argument("--policy", default="benchmarks/semantic_memory_budget_policy.json",
                        help="Budget policy JSON path.")
    parser.add_argument("--runs", type=int, default=3, help="Benchmark runs per fixture/phase.")
    parser.add_argument("--history-dir", default="build-release/benchmarks/semantic_memory_history",
                        help="Directory with prior semantic-memory reports.")
    parser.add_argument("--history-limit", type=int, default=2,
                        help="History report count used for sustained checks.")
    parser.add_argument("--benchmark-report", default="build-release/benchmarks/semantic_memory_report.json",
                        help="Path for semantic-memory benchmark report.")
    parser.add_argument("--budget-report", default="build-release/benchmarks/semantic_memory_budget_check_report.json",
                        help="Path for budget-check report.")
    parser.add_argument("--trend-report", default="build-release/benchmarks/semantic_memory_trend_report.json",
                        help="Path for trend-check summary report.")
    parser.add_argument("--artifacts-dir", default="build-release/benchmarks/semantic_memory_artifacts",
                        help="Artifact bundle output directory.")
    parser.add_argument("--run-label", default="semantic_memory", help="Run label in manifest/artifact names.")
    parser.add_argument("--mode", choices=("benchmark", "trend", "full"), default="full",
                        help="Which steps to run.")
    parser.add_argument("--benchmark-cmd", default="",
                        help="Optional shell command override for benchmark step.")
    parser.add_argument("--trend-cmd", default="",
                        help="Optional shell command override for trend step.")
    parser.add_argument(
        "--skip-budget-check-in-benchmark",
        action="store_true",
        help=("When mode=benchmark, skip the policy/trend budget check stage and run benchmark only."),
    )
    return parser.parse_args()


def quote_shell_arg(text: str) -> str:
    return "'" + text.replace("'", "'\\''") + "'"


def timestamp_utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def run_shell_command(command: str, cwd: Path, stdout_path: Path, stderr_path: Path) -> int:
    result = subprocess.run(
        command,
        cwd=cwd,
        shell=True,
        text=True,
        capture_output=True,
        check=False,
    )
    stdout_path.write_text(result.stdout, encoding="utf-8")
    stderr_path.write_text(result.stderr, encoding="utf-8")
    return result.returncode


def copy_if_exists(path: Path, target_dir: Path) -> Optional[str]:
    if not path.is_file():
        return None
    destination = target_dir / path.name
    destination.write_bytes(path.read_bytes())
    return destination.name


def default_benchmark_command(repo_root: Path, primec: Path, runs: int, report: Path) -> str:
    script = repo_root / "scripts" / "semantic_memory_benchmark.py"
    return (
        f"{quote_shell_arg(sys.executable)} {quote_shell_arg(str(script))} "
        f"--repo-root {quote_shell_arg(str(repo_root))} "
        f"--primec {quote_shell_arg(str(primec))} "
        f"--runs {runs} "
        f"--report-json {quote_shell_arg(str(report))}"
    )


def default_trend_command(repo_root: Path,
                          policy: Path,
                          report: Path,
                          history_dir: Path,
                          history_limit: int,
                          budget_report: Path,
                          trend_report: Path) -> str:
    script = repo_root / "scripts" / "check_semantic_memory_trend.py"
    return (
        f"{quote_shell_arg(sys.executable)} {quote_shell_arg(str(script))} "
        f"--policy {quote_shell_arg(str(policy))} "
        f"--report {quote_shell_arg(str(report))} "
        f"--history-dir {quote_shell_arg(str(history_dir))} "
        f"--history-limit {history_limit} "
        f"--report-json {quote_shell_arg(str(budget_report))} "
        f"--trend-report-json {quote_shell_arg(str(trend_report))}"
    )


def main() -> int:
    args = parse_args()
    if args.runs < 1:
        print("[semantic_memory_ci_artifacts] --runs must be >= 1", file=sys.stderr)
        return 2
    if args.history_limit < 0:
        print("[semantic_memory_ci_artifacts] --history-limit must be >= 0", file=sys.stderr)
        return 2

    repo_root = Path(args.repo_root).resolve()
    primec = Path(args.primec)
    if not primec.is_absolute():
        primec = (repo_root / primec).resolve()
    policy = Path(args.policy)
    if not policy.is_absolute():
        policy = (repo_root / policy).resolve()
    benchmark_report = Path(args.benchmark_report)
    if not benchmark_report.is_absolute():
        benchmark_report = (repo_root / benchmark_report).resolve()
    budget_report = Path(args.budget_report)
    if not budget_report.is_absolute():
        budget_report = (repo_root / budget_report).resolve()
    trend_report = Path(args.trend_report)
    if not trend_report.is_absolute():
        trend_report = (repo_root / trend_report).resolve()
    artifacts_dir = Path(args.artifacts_dir)
    if not artifacts_dir.is_absolute():
        artifacts_dir = (repo_root / artifacts_dir).resolve()
    history_dir = Path(args.history_dir)
    if not history_dir.is_absolute():
        history_dir = (repo_root / history_dir).resolve()

    run_id = f"{args.run_label}_{timestamp_utc()}"
    run_dir = artifacts_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)

    benchmark_stdout = run_dir / "benchmark_stdout.txt"
    benchmark_stderr = run_dir / "benchmark_stderr.txt"
    trend_stdout = run_dir / "trend_stdout.txt"
    trend_stderr = run_dir / "trend_stderr.txt"
    trend_requested = (
        args.mode in ("trend", "full") or
        (args.mode == "benchmark" and not args.skip_budget_check_in_benchmark)
    )

    # Avoid stale-report carryover in modes that regenerate these files.
    if args.mode in ("benchmark", "full") and benchmark_report.exists():
        benchmark_report.unlink()
    if trend_requested and budget_report.exists():
        budget_report.unlink()
    if trend_requested and trend_report.exists():
        trend_report.unlink()

    benchmark_command = args.benchmark_cmd.strip() or default_benchmark_command(
        repo_root, primec, args.runs, benchmark_report)
    trend_command = args.trend_cmd.strip() or default_trend_command(
        repo_root, policy, benchmark_report, history_dir, args.history_limit, budget_report, trend_report)

    benchmark_exit_code: Optional[int] = None
    trend_exit_code: Optional[int] = None
    trend_skipped = False

    if args.mode in ("benchmark", "full"):
        benchmark_exit_code = run_shell_command(benchmark_command, repo_root, benchmark_stdout, benchmark_stderr)
    else:
        benchmark_stdout.write_text("", encoding="utf-8")
        benchmark_stderr.write_text("", encoding="utf-8")

    can_run_trend = trend_requested
    if args.mode in ("benchmark", "full") and benchmark_exit_code is not None and benchmark_exit_code != 0:
        can_run_trend = False
        trend_skipped = True

    if can_run_trend:
        trend_exit_code = run_shell_command(trend_command, repo_root, trend_stdout, trend_stderr)
    else:
        trend_stdout.write_text("", encoding="utf-8")
        trend_stderr.write_text("", encoding="utf-8")

    copied_benchmark_report: Optional[str] = None
    if args.mode == "trend":
        copied_benchmark_report = copy_if_exists(benchmark_report, run_dir)
    elif benchmark_exit_code == 0:
        copied_benchmark_report = copy_if_exists(benchmark_report, run_dir)

    copied_budget_report: Optional[str] = None
    if trend_exit_code == 0:
        copied_budget_report = copy_if_exists(budget_report, run_dir)
    copied_trend_report = copy_if_exists(trend_report, run_dir)

    copied_history_report: Optional[str] = None
    if benchmark_exit_code == 0 and benchmark_report.is_file():
        history_dir.mkdir(parents=True, exist_ok=True)
        history_file = history_dir / f"semantic_memory_report_{run_id}.json"
        history_file.write_bytes(benchmark_report.read_bytes())
        copied_history_report = str(history_file)

    failed = False
    if benchmark_exit_code is not None and benchmark_exit_code != 0:
        failed = True
    if trend_exit_code is not None and trend_exit_code != 0:
        failed = True

    manifest = {
        "schema": "primestruct_semantic_memory_ci_artifacts_v1",
        "run_id": run_id,
        "run_label": args.run_label,
        "mode": args.mode,
        "status": "failed" if failed else "passed",
        "benchmark_exit_code": benchmark_exit_code,
        "trend_exit_code": trend_exit_code,
        "trend_skipped": trend_skipped,
        "repo_root": str(repo_root),
        "primec": str(primec),
        "policy": str(policy),
        "benchmark_report": str(benchmark_report),
        "budget_report": str(budget_report),
        "trend_report": str(trend_report),
        "history_dir": str(history_dir),
        "copied_files": {
            "benchmark_report": copied_benchmark_report,
            "budget_report": copied_budget_report,
            "trend_report": copied_trend_report,
            "benchmark_stdout": benchmark_stdout.name,
            "benchmark_stderr": benchmark_stderr.name,
            "trend_stdout": trend_stdout.name,
            "trend_stderr": trend_stderr.name,
        },
        "history_report": copied_history_report,
        "commands": {
            "benchmark": benchmark_command if args.mode in ("benchmark", "full") else None,
            "trend": trend_command if trend_requested else None,
        },
    }
    manifest_path = run_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    latest_path = artifacts_dir / f"{args.run_label}_latest_manifest.json"
    latest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"[semantic_memory_ci_artifacts] wrote manifest: {manifest_path}")
    print(f"[semantic_memory_ci_artifacts] status={manifest['status']}")

    if benchmark_exit_code is not None and benchmark_exit_code != 0:
        return benchmark_exit_code
    if trend_exit_code is not None and trend_exit_code != 0:
        return trend_exit_code
    return 0


if __name__ == "__main__":
    sys.exit(main())
