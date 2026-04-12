#!/usr/bin/env python3
"""Semantic-memory benchmark harness for PrimeStruct.

Measures wall time + peak RSS for semantic boundary phases on checked-in fixtures,
and emits a machine-readable JSON report.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import statistics
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"
DEFAULT_PHASES = ("ast-semantic", "semantic-product")
EXPENSIVE_RUNTIME_SECONDS = 3.0
EXPENSIVE_PEAK_RSS_BYTES = 500 * 1024 * 1024
SEMANTIC_COLLECTOR_FAMILIES = (
    "definitions",
    "executions",
    "direct_call_targets",
    "method_call_targets",
    "bridge_path_choices",
    "callable_summaries",
    "type_metadata",
    "struct_field_metadata",
    "binding_facts",
    "return_facts",
    "local_auto_facts",
    "query_facts",
    "try_facts",
    "on_error_facts",
)


@dataclass(frozen=True)
class FixtureSpec:
    name: str
    source: str
    group: str
    scale_factor: int | None = None


FIXTURES: tuple[FixtureSpec, ...] = (
    FixtureSpec("math_star_repro", "benchmarks/semantic_memory/fixtures/math_star_repro.prime", "primary"),
    FixtureSpec("no_import", "benchmarks/semantic_memory/fixtures/no_import.prime", "imports"),
    FixtureSpec("math_vector", "benchmarks/semantic_memory/fixtures/math_vector.prime", "imports"),
    FixtureSpec("math_vector_matrix", "benchmarks/semantic_memory/fixtures/math_vector_matrix.prime", "imports"),
    FixtureSpec(
        "non_math_large_include",
        "benchmarks/semantic_memory/fixtures/non_math_large_include.prime",
        "imports",
    ),
    FixtureSpec("inline_math_body", "benchmarks/semantic_memory/fixtures/inline_math_body.prime", "inline_vs_import"),
    FixtureSpec(
        "imported_math_body",
        "benchmarks/semantic_memory/fixtures/imported_math_body.prime",
        "inline_vs_import",
    ),
    FixtureSpec("scale_1x", "benchmarks/semantic_memory/fixtures/scale_1x.prime", "scale", scale_factor=1),
    FixtureSpec("scale_2x", "benchmarks/semantic_memory/fixtures/scale_2x.prime", "scale", scale_factor=2),
    FixtureSpec("scale_4x", "benchmarks/semantic_memory/fixtures/scale_4x.prime", "scale", scale_factor=4),
)

DIRECT_TARGET_RE = re.compile(r'direct_call_targets\[\d+\]:\s+scope_path="([^"]*)"\s+call_name="([^"]*)"')
METHOD_TARGET_RE = re.compile(r'method_call_targets\[\d+\]:\s+scope_path="([^"]*)"\s+method_name="([^"]*)"')
SEMANTIC_PHASE_COUNTERS_PREFIX = "[benchmark-semantic-phase-counters] "
SEMANTIC_REPEAT_LEAK_CHECK_PREFIX = "[benchmark-semantic-repeat-leak-check] "


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run PrimeStruct semantic-memory benchmarks.")
    parser.add_argument("--repo-root", default=".", help="Repository root for fixture paths.")
    parser.add_argument("--primec", default="build-release/primec", help="Path to primec executable.")
    parser.add_argument("--entry", default="/bench/main", help="Entry path used for fixture compilation.")
    parser.add_argument("--runs", type=int, default=3, help="Timed runs per fixture-phase (default: 3).")
    parser.add_argument(
        "--phases",
        default=",".join(DEFAULT_PHASES),
        help="Comma-separated dump stages (default: ast-semantic,semantic-product).",
    )
    parser.add_argument(
        "--fixtures",
        default="",
        help="Optional comma-separated fixture names to run. Empty = all fixtures.",
    )
    parser.add_argument(
        "--semantic-product-force",
        choices=("auto", "on", "off", "both"),
        default="auto",
        help="Benchmark semantic-product gate mode: auto, on, off, or both for A/B deltas.",
    )
    parser.add_argument(
        "--semantic-validation-without-fact-emission",
        choices=("off", "on", "both"),
        default="off",
        help="Benchmark mode for validation-only semantic runs: off, on, or both for A/B deltas.",
    )
    parser.add_argument(
        "--no-fact-emission",
        action="store_true",
        help="Deprecated alias for --semantic-validation-without-fact-emission=on.",
    )
    parser.add_argument(
        "--fact-families",
        default="auto",
        help="Benchmark-only collector allowlist CSV; use auto|all|none (default: auto).",
    )
    parser.add_argument(
        "--semantic-phase-counters",
        action="store_true",
        help="Benchmark-only mode: collect semantic phase counters and include them in the report.",
    )
    parser.add_argument(
        "--semantic-allocation-counters",
        action="store_true",
        help="Benchmark-only mode: collect semantic allocation counters and include them in the report.",
    )
    parser.add_argument(
        "--semantic-rss-checkpoints",
        action="store_true",
        help="Benchmark-only mode: collect per-phase semantic RSS checkpoints and include them in the report.",
    )
    parser.add_argument(
        "--repeat-compile-leak-check-runs",
        type=int,
        default=0,
        help="Benchmark-only mode: run repeated compile in one primec process and report RSS drift (0 disables).",
    )
    parser.add_argument(
        "--method-target-memoization",
        choices=("on", "off", "both"),
        default="on",
        help="Benchmark mode for method-target memoization: on, off, or both for A/B deltas.",
    )
    parser.add_argument(
        "--graph-local-auto-key-mode",
        choices=("compact", "legacy-shadow", "both"),
        default="compact",
        help="Graph-local-auto key mode: compact, legacy-shadow, or both for RSS deltas.",
    )
    parser.add_argument(
        "--graph-local-auto-side-channel-mode",
        choices=("flat", "legacy-shadow", "both"),
        default="flat",
        help="Graph-local-auto side-channel mode: flat, legacy-shadow, or both for RSS deltas.",
    )
    parser.add_argument(
        "--graph-local-auto-dependency-scratch-mode",
        choices=("pmr", "std", "both"),
        default="pmr",
        help="Graph-local-auto dependency scratch mode: pmr, std, or both for RSS deltas.",
    )
    parser.add_argument("--report-json", default="", help="Optional report output path.")
    return parser.parse_args()


def run_repeat_compile_leak_check(
    command: list[str],
    cwd: Path,
    fixture_name: str,
    phase: str,
    runs: int,
) -> dict:
    leak_command = list(command)
    leak_command.append(f"--benchmark-semantic-repeat-count={runs}")

    exit_code, stdout_text, stderr_text, _ = run_with_rusage(leak_command, cwd)
    if exit_code != 0:
        details = stderr_text.strip() or stdout_text.strip()
        raise RuntimeError(
            f"repeat compile leak check failed fixture={fixture_name} phase={phase} "
            f"runs={runs} exit={exit_code}: {details}"
        )

    for line in stderr_text.splitlines():
        if not line.startswith(SEMANTIC_REPEAT_LEAK_CHECK_PREFIX):
            continue
        payload = line[len(SEMANTIC_REPEAT_LEAK_CHECK_PREFIX):].strip()
        try:
            return json.loads(payload)
        except json.JSONDecodeError as exc:
            raise RuntimeError(
                f"failed to parse repeat compile leak check fixture={fixture_name} phase={phase}: {exc}"
            ) from exc

    raise RuntimeError(
        f"missing repeat compile leak check marker fixture={fixture_name} phase={phase} runs={runs}"
    )


def maxrss_bytes(raw_maxrss: int) -> int:
    if platform.system() == "Darwin":
        return int(raw_maxrss)
    return int(raw_maxrss) * 1024


def run_with_rusage(command: list[str], cwd: Path) -> tuple[int, str, str, int]:
    stdout_fd, stdout_path = tempfile.mkstemp(prefix="primestruct-semantic-memory-out-")
    stderr_fd, stderr_path = tempfile.mkstemp(prefix="primestruct-semantic-memory-err-")
    os.close(stdout_fd)
    os.close(stderr_fd)

    pid = os.fork()
    if pid == 0:
        try:
            os.chdir(cwd)
            out_fd = os.open(stdout_path, os.O_WRONLY | os.O_TRUNC)
            err_fd = os.open(stderr_path, os.O_WRONLY | os.O_TRUNC)
            os.dup2(out_fd, 1)
            os.dup2(err_fd, 2)
            os.close(out_fd)
            os.close(err_fd)
            os.execvp(command[0], command)
        except Exception as exc:  # pylint: disable=broad-except
            msg = f"semantic memory benchmark child launch failed: {exc}\n"
            try:
                os.write(2, msg.encode("utf-8", errors="replace"))
            except OSError:
                pass
            os._exit(127)

    _, status, rusage = os.wait4(pid, 0)
    try:
        stdout_text = Path(stdout_path).read_text(encoding="utf-8")
    except Exception:
        stdout_text = ""
    try:
        stderr_text = Path(stderr_path).read_text(encoding="utf-8")
    except Exception:
        stderr_text = ""

    for path in (stdout_path, stderr_path):
        try:
            os.unlink(path)
        except OSError:
            pass

    if os.WIFEXITED(status):
        exit_code = os.WEXITSTATUS(status)
    elif os.WIFSIGNALED(status):
        exit_code = 128 + os.WTERMSIG(status)
    else:
        exit_code = 1

    return exit_code, stdout_text, stderr_text, maxrss_bytes(rusage.ru_maxrss)


def median(values: Iterable[float]) -> float:
    data = list(values)
    if not data:
        return 0.0
    return float(statistics.median(data))


def measure_fixture_phase(primec: Path,
                         repo_root: Path,
                         fixture: FixtureSpec,
                         phase: str,
                         runs: int,
                         entry: str,
                         semantic_product_force: str,
                         no_fact_emission: bool,
                         fact_families: str,
                         semantic_phase_counters: bool,
                         semantic_allocation_counters: bool,
                         semantic_rss_checkpoints: bool,
                         repeat_compile_leak_check_runs: int,
                         method_target_memoization: str,
                         graph_local_auto_key_mode: str,
                         graph_local_auto_side_channel_mode: str,
                         graph_local_auto_dependency_scratch_mode: str) -> dict:
    fixture_path = (repo_root / fixture.source).resolve()
    if not fixture_path.is_file():
        raise FileNotFoundError(f"fixture not found: {fixture_path}")

    wall_seconds: list[float] = []
    peak_rss_bytes: list[int] = []
    captured_dump = ""

    command = [str(primec), str(fixture_path), "--entry", entry, "--dump-stage", phase]
    if semantic_product_force != "auto":
        command.append(f"--benchmark-force-semantic-product={semantic_product_force}")
    if no_fact_emission:
        command.append("--benchmark-semantic-no-fact-emission")
    if fact_families not in ("", "auto", "all"):
        command.append(f"--benchmark-semantic-fact-families={fact_families}")
    if semantic_phase_counters or semantic_allocation_counters:
        command.append("--benchmark-semantic-phase-counters")
    if semantic_allocation_counters:
        command.append("--benchmark-semantic-allocation-counters")
    if semantic_rss_checkpoints:
        command.append("--benchmark-semantic-rss-checkpoints")
    if method_target_memoization == "off":
        command.append("--benchmark-semantic-disable-method-target-memoization")
    if graph_local_auto_key_mode == "legacy-shadow":
        command.append("--benchmark-semantic-graph-local-auto-legacy-key-shadow")
    if graph_local_auto_side_channel_mode == "legacy-shadow":
        command.append("--benchmark-semantic-graph-local-auto-legacy-side-channel-shadow")
    if graph_local_auto_dependency_scratch_mode == "std":
        command.append("--benchmark-semantic-disable-graph-local-auto-dependency-scratch-pmr")

    semantic_phase_counter_runs: list[dict] = []

    for run_idx in range(runs):
        start = time.perf_counter()
        exit_code, stdout_text, stderr_text, rss_bytes = run_with_rusage(command, repo_root)
        elapsed = time.perf_counter() - start
        if exit_code != 0:
            details = stderr_text.strip() or stdout_text.strip()
            raise RuntimeError(
                f"benchmark run failed fixture={fixture.name} phase={phase} run={run_idx + 1} "
                f"exit={exit_code}: {details}"
            )
        wall_seconds.append(elapsed)
        peak_rss_bytes.append(rss_bytes)
        if captured_dump == "":
            captured_dump = stdout_text
        if semantic_phase_counters or semantic_allocation_counters or semantic_rss_checkpoints:
            parsed_counters = None
            for line in stderr_text.splitlines():
                if not line.startswith(SEMANTIC_PHASE_COUNTERS_PREFIX):
                    continue
                payload = line[len(SEMANTIC_PHASE_COUNTERS_PREFIX):].strip()
                try:
                    parsed_counters = json.loads(payload)
                except json.JSONDecodeError as exc:
                    raise RuntimeError(
                        f"failed to parse semantic phase counters fixture={fixture.name} "
                        f"phase={phase} run={run_idx + 1}: {exc}"
                    ) from exc
                break
            if parsed_counters is None:
                raise RuntimeError(
                    f"missing semantic phase counters fixture={fixture.name} "
                    f"phase={phase} run={run_idx + 1}"
                )
            semantic_phase_counter_runs.append(parsed_counters)

    result = {
        "fixture": fixture.name,
        "source": fixture.source,
        "group": fixture.group,
        "phase": phase,
        "runs": runs,
        "entry": entry,
        "wall_seconds": wall_seconds,
        "peak_rss_bytes": peak_rss_bytes,
        "median_wall_seconds": median(wall_seconds),
        "worst_wall_seconds": float(max(wall_seconds)),
        "median_peak_rss_bytes": int(median(peak_rss_bytes)),
        "worst_peak_rss_bytes": int(max(peak_rss_bytes)),
        "dump_size_bytes": len(captured_dump.encode("utf-8")),
        "semantic_product_force": semantic_product_force,
        "no_fact_emission": no_fact_emission,
        "fact_families": fact_families,
        "method_target_memoization": method_target_memoization,
        "graph_local_auto_key_mode": graph_local_auto_key_mode,
        "graph_local_auto_side_channel_mode": graph_local_auto_side_channel_mode,
        "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
    }
    if semantic_phase_counters or semantic_allocation_counters or semantic_rss_checkpoints:
        result["semantic_phase_counters_runs"] = semantic_phase_counter_runs
        result["semantic_phase_counters"] = semantic_phase_counter_runs[0]
    if repeat_compile_leak_check_runs > 0:
        result["repeat_compile_leak_check"] = run_repeat_compile_leak_check(
            command,
            repo_root,
            fixture.name,
            phase,
            repeat_compile_leak_check_runs,
        )
    exceeds_runtime_threshold = result["worst_wall_seconds"] > EXPENSIVE_RUNTIME_SECONDS
    exceeds_rss_threshold = result["worst_peak_rss_bytes"] > EXPENSIVE_PEAK_RSS_BYTES
    result["exceeds_expensive_runtime_threshold"] = exceeds_runtime_threshold
    result["exceeds_expensive_rss_threshold"] = exceeds_rss_threshold
    result["is_expensive_threshold_offender"] = exceeds_runtime_threshold or exceeds_rss_threshold

    if phase == "semantic-product":
        result["key_cardinality"] = collect_key_cardinality_metrics(captured_dump)

    return result


def collect_key_cardinality_metrics(dump_text: str) -> dict:
    direct_keys: set[str] = set()
    method_keys: set[str] = set()
    max_key_len = 0

    for match in DIRECT_TARGET_RE.finditer(dump_text):
        key = f"{match.group(1)}::{match.group(2)}"
        direct_keys.add(key)
        max_key_len = max(max_key_len, len(key))

    for match in METHOD_TARGET_RE.finditer(dump_text):
        key = f"{match.group(1)}::{match.group(2)}"
        method_keys.add(key)
        max_key_len = max(max_key_len, len(key))

    return {
        "distinct_direct_call_target_keys": len(direct_keys),
        "distinct_method_call_target_keys": len(method_keys),
        "max_target_key_length": max_key_len,
    }


def select_fixtures(selection: str) -> list[FixtureSpec]:
    if not selection:
        return list(FIXTURES)
    selected_names = {item.strip() for item in selection.split(",") if item.strip()}
    ordered = [fixture for fixture in FIXTURES if fixture.name in selected_names]
    missing = sorted(selected_names.difference({fixture.name for fixture in ordered}))
    if missing:
        raise ValueError("unknown fixtures: " + ", ".join(missing))
    return ordered


def parse_phases(text: str) -> list[str]:
    phases = [item.strip() for item in text.split(",") if item.strip()]
    if not phases:
        raise ValueError("--phases cannot be empty")
    for phase in phases:
        if phase not in DEFAULT_PHASES:
            raise ValueError(f"unsupported phase '{phase}' (expected ast-semantic or semantic-product)")
    return phases


def parse_fact_families(text: str) -> str:
    trimmed = text.strip()
    if trimmed in ("", "auto", "all", "none"):
        return "auto" if trimmed in ("", "auto", "all") else "none"
    families = [token.strip() for token in trimmed.split(",") if token.strip()]
    unknown = sorted(set(families).difference(SEMANTIC_COLLECTOR_FAMILIES))
    if unknown:
        raise ValueError("unknown semantic collector families: " + ", ".join(unknown))
    unique: list[str] = []
    for family in families:
        if family not in unique:
            unique.append(family)
    return ",".join(unique)


def path_for_report(path: Path, repo_root: Path) -> str:
    try:
        return str(path.resolve().relative_to(repo_root))
    except ValueError:
        return str(path.resolve())


def compute_scale_slopes(results: list[dict]) -> list[dict]:
    scale_rows: list[dict] = []
    by_phase: dict[str, list[tuple[int, dict]]] = {}

    for row in results:
        fixture_name = row.get("fixture", "")
        fixture = next((item for item in FIXTURES if item.name == fixture_name), None)
        if fixture is None or fixture.scale_factor is None:
            continue
        by_phase.setdefault(row["phase"], []).append((fixture.scale_factor, row))

    for phase, rows in sorted(by_phase.items()):
        if len(rows) < 2:
            continue
        rows.sort(key=lambda pair: pair[0])
        x0, first = rows[0]
        x1, last = rows[-1]
        span = float(x1 - x0)
        if span <= 0.0:
            continue
        rss_slope = (float(last["median_peak_rss_bytes"]) - float(first["median_peak_rss_bytes"])) / span
        wall_slope = (float(last["median_wall_seconds"]) - float(first["median_wall_seconds"])) / span
        scale_rows.append(
            {
                "phase": phase,
                "x_axis": [row[0] for row in rows],
                "fixtures": [row[1]["fixture"] for row in rows],
                "rss_bytes_per_scale_unit": rss_slope,
                "wall_seconds_per_scale_unit": wall_slope,
            }
        )

    return scale_rows


def collect_expensive_offenders(results: list[dict]) -> list[dict]:
    offenders: list[dict] = []
    for row in results:
        if not row.get("is_expensive_threshold_offender", False):
            continue
        offenders.append(
            {
                "fixture": row["fixture"],
                "phase": row["phase"],
                "worst_wall_seconds": row["worst_wall_seconds"],
                "worst_peak_rss_bytes": row["worst_peak_rss_bytes"],
                "exceeds_expensive_runtime_threshold": row["exceeds_expensive_runtime_threshold"],
                "exceeds_expensive_rss_threshold": row["exceeds_expensive_rss_threshold"],
            }
        )
    return offenders


def selected_semantic_validation_without_fact_emission_modes(
    selection: str, legacy_no_fact_emission: bool
) -> list[bool]:
    normalized = selection
    if legacy_no_fact_emission and normalized == "off":
        normalized = "on"
    if normalized == "both":
        return [False, True]
    return [normalized == "on"]


def selected_semantic_product_force_modes(selection: str) -> list[str]:
    if selection == "both":
        return ["on", "off"]
    return [selection]


def benchmark_row_semantic_product_force_mode(row: dict) -> str:
    value = row.get("semantic_product_force", "auto")
    if value is None:
        return "auto"
    normalized = str(value).strip().lower()
    if normalized in ("", "auto", "null", "none"):
        return "auto"
    if normalized in ("1", "true", "yes"):
        return "on"
    if normalized in ("0", "false", "no"):
        return "off"
    return normalized


def benchmark_row_fact_families_mode(row: dict) -> str:
    value = row.get("fact_families", "auto")
    if value is None:
        return "auto"
    normalized = str(value).strip().lower()
    if normalized in ("", "all", "auto", "null"):
        return "auto"
    if normalized == "none":
        return "none"
    families = [token.strip() for token in normalized.split(",") if token.strip()]
    if not families:
        return "auto"
    seen = set(families)
    known = [family for family in SEMANTIC_COLLECTOR_FAMILIES if family in seen]
    unknown = sorted(token for token in seen if token not in SEMANTIC_COLLECTOR_FAMILIES)
    canonical = known + unknown
    if not canonical:
        return "auto"
    return ",".join(canonical)


def benchmark_row_method_target_memoization_mode(row: dict) -> str:
    value = row.get("method_target_memoization", "on")
    if value is None:
        return "on"
    normalized = str(value).strip().lower()
    if normalized in ("", "on", "null", "none"):
        return "on"
    if normalized in ("1", "true", "yes"):
        return "on"
    if normalized in ("0", "false", "no"):
        return "off"
    return normalized


def benchmark_row_graph_local_auto_key_mode(row: dict) -> str:
    value = row.get("graph_local_auto_key_mode", "compact")
    if value is None:
        return "compact"
    normalized = str(value).strip().lower()
    normalized = normalized.replace("_", "-")
    if normalized in ("", "compact", "null", "none"):
        return "compact"
    return normalized


def benchmark_row_graph_local_auto_side_channel_mode(row: dict) -> str:
    value = row.get("graph_local_auto_side_channel_mode", "flat")
    if value is None:
        return "flat"
    normalized = str(value).strip().lower()
    normalized = normalized.replace("_", "-")
    if normalized in ("", "flat", "null", "none"):
        return "flat"
    return normalized


def benchmark_row_graph_local_auto_dependency_scratch_mode(row: dict) -> str:
    value = row.get("graph_local_auto_dependency_scratch_mode", "pmr")
    if value is None:
        return "pmr"
    normalized = str(value).strip().lower()
    normalized = normalized.replace("_", "-")
    if normalized in ("", "pmr", "null", "none"):
        return "pmr"
    return normalized


def benchmark_row_no_fact_emission_mode(row: dict) -> bool:
    value = row.get("no_fact_emission", False)
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    normalized = str(value).strip().lower()
    return normalized not in ("", "0", "false", "off", "no", "none", "null")


def compute_semantic_validation_without_fact_emission_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str, str, str, str, str, str], dict[bool, dict]] = {}
    for row in results:
        no_fact_emission = benchmark_row_no_fact_emission_mode(row)
        key = (
            row["fixture"],
            row["phase"],
            benchmark_row_semantic_product_force_mode(row),
            benchmark_row_fact_families_mode(row),
            benchmark_row_method_target_memoization_mode(row),
            benchmark_row_graph_local_auto_key_mode(row),
            benchmark_row_graph_local_auto_side_channel_mode(row),
            benchmark_row_graph_local_auto_dependency_scratch_mode(row),
        )
        grouped.setdefault(key, {})[no_fact_emission] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         semantic_product_force,
         fact_families,
         method_target_memoization,
         graph_local_auto_key_mode,
         graph_local_auto_side_channel_mode,
         graph_local_auto_dependency_scratch_mode), by_mode in sorted(grouped.items()):
        fact_emission_row = by_mode.get(False)
        no_fact_emission_row = by_mode.get(True)
        if fact_emission_row is None or no_fact_emission_row is None:
            continue

        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "semantic_product_force": semantic_product_force,
                "fact_families": fact_families,
                "method_target_memoization": method_target_memoization,
                "graph_local_auto_key_mode": graph_local_auto_key_mode,
                "graph_local_auto_side_channel_mode": graph_local_auto_side_channel_mode,
                "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
                "median_peak_rss_bytes_fact_emission": fact_emission_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_no_fact_emission": no_fact_emission_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_no_fact_emission_minus_fact_emission":
                    int(no_fact_emission_row["median_peak_rss_bytes"]) -
                    int(fact_emission_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_fact_emission": fact_emission_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_no_fact_emission": no_fact_emission_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_no_fact_emission_minus_fact_emission":
                    int(no_fact_emission_row["worst_peak_rss_bytes"]) -
                    int(fact_emission_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_fact_emission": fact_emission_row["median_wall_seconds"],
                "median_wall_seconds_no_fact_emission": no_fact_emission_row["median_wall_seconds"],
                "median_wall_seconds_no_fact_emission_minus_fact_emission":
                    float(no_fact_emission_row["median_wall_seconds"]) -
                    float(fact_emission_row["median_wall_seconds"]),
                "worst_wall_seconds_fact_emission": fact_emission_row["worst_wall_seconds"],
                "worst_wall_seconds_no_fact_emission": no_fact_emission_row["worst_wall_seconds"],
                "worst_wall_seconds_no_fact_emission_minus_fact_emission":
                    float(no_fact_emission_row["worst_wall_seconds"]) -
                    float(fact_emission_row["worst_wall_seconds"]),
            }
        )
    return deltas


def compute_semantic_product_force_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, bool, str, str, str, str, str], dict[str, dict]] = {}
    for row in results:
        mode = benchmark_row_semantic_product_force_mode(row)
        if mode not in ("on", "off"):
            continue
        key = (
            row["fixture"],
            row["phase"],
            benchmark_row_no_fact_emission_mode(row),
            benchmark_row_fact_families_mode(row),
            benchmark_row_method_target_memoization_mode(row),
            benchmark_row_graph_local_auto_key_mode(row),
            benchmark_row_graph_local_auto_side_channel_mode(row),
            benchmark_row_graph_local_auto_dependency_scratch_mode(row),
        )
        grouped.setdefault(key, {})[mode] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         no_fact_emission,
         fact_families,
         method_target_memoization,
         graph_local_auto_key_mode,
         graph_local_auto_side_channel_mode,
         graph_local_auto_dependency_scratch_mode), by_mode in sorted(grouped.items()):
        on_row = by_mode.get("on")
        off_row = by_mode.get("off")
        if on_row is None or off_row is None:
            continue
        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "no_fact_emission": no_fact_emission,
                "fact_families": fact_families,
                "method_target_memoization": method_target_memoization,
                "graph_local_auto_key_mode": graph_local_auto_key_mode,
                "graph_local_auto_side_channel_mode": graph_local_auto_side_channel_mode,
                "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
                "median_peak_rss_bytes_on": on_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_off": off_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_on_minus_off":
                    int(on_row["median_peak_rss_bytes"]) - int(off_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_on": on_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_off": off_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_on_minus_off":
                    int(on_row["worst_peak_rss_bytes"]) - int(off_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_on": on_row["median_wall_seconds"],
                "median_wall_seconds_off": off_row["median_wall_seconds"],
                "median_wall_seconds_on_minus_off":
                    float(on_row["median_wall_seconds"]) - float(off_row["median_wall_seconds"]),
                "worst_wall_seconds_on": on_row["worst_wall_seconds"],
                "worst_wall_seconds_off": off_row["worst_wall_seconds"],
                "worst_wall_seconds_on_minus_off":
                    float(on_row["worst_wall_seconds"]) - float(off_row["worst_wall_seconds"]),
            }
        )
    return deltas


def selected_method_target_memoization_modes(selection: str) -> list[str]:
    if selection == "both":
        return ["on", "off"]
    return [selection]


def compute_method_target_memoization_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str, bool, str, str, str, str], dict[str, dict]] = {}
    for row in results:
        mode = benchmark_row_method_target_memoization_mode(row)
        if mode not in ("on", "off"):
            continue
        grouped.setdefault(
            (
                row["fixture"],
                row["phase"],
                benchmark_row_semantic_product_force_mode(row),
                benchmark_row_no_fact_emission_mode(row),
                benchmark_row_fact_families_mode(row),
                benchmark_row_graph_local_auto_key_mode(row),
                benchmark_row_graph_local_auto_side_channel_mode(row),
                benchmark_row_graph_local_auto_dependency_scratch_mode(row),
            ),
            {},
        )[mode] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         semantic_product_force,
         no_fact_emission,
         fact_families,
         graph_local_auto_key_mode,
         graph_local_auto_side_channel_mode,
         graph_local_auto_dependency_scratch_mode), by_mode in sorted(grouped.items()):
        on_row = by_mode.get("on")
        off_row = by_mode.get("off")
        if on_row is None or off_row is None:
            continue

        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "semantic_product_force": semantic_product_force,
                "no_fact_emission": no_fact_emission,
                "fact_families": fact_families,
                "graph_local_auto_key_mode": graph_local_auto_key_mode,
                "graph_local_auto_side_channel_mode": graph_local_auto_side_channel_mode,
                "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
                "median_peak_rss_bytes_on": on_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_off": off_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_on_minus_off":
                    int(on_row["median_peak_rss_bytes"]) - int(off_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_on": on_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_off": off_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_on_minus_off":
                    int(on_row["worst_peak_rss_bytes"]) - int(off_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_on": on_row["median_wall_seconds"],
                "median_wall_seconds_off": off_row["median_wall_seconds"],
                "median_wall_seconds_on_minus_off":
                    float(on_row["median_wall_seconds"]) - float(off_row["median_wall_seconds"]),
                "worst_wall_seconds_on": on_row["worst_wall_seconds"],
                "worst_wall_seconds_off": off_row["worst_wall_seconds"],
                "worst_wall_seconds_on_minus_off":
                    float(on_row["worst_wall_seconds"]) - float(off_row["worst_wall_seconds"]),
            }
        )
    return deltas


def selected_graph_local_auto_key_modes(selection: str) -> list[str]:
    if selection == "both":
        return ["compact", "legacy-shadow"]
    return [selection]


def compute_graph_local_auto_key_mode_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str, str, bool, str, str, str], dict[str, dict]] = {}
    for row in results:
        mode = benchmark_row_graph_local_auto_key_mode(row)
        if mode not in ("compact", "legacy-shadow"):
            continue
        method_mode = benchmark_row_method_target_memoization_mode(row)
        grouped.setdefault(
            (
                row["fixture"],
                row["phase"],
                method_mode,
                benchmark_row_semantic_product_force_mode(row),
                benchmark_row_no_fact_emission_mode(row),
                benchmark_row_fact_families_mode(row),
                benchmark_row_graph_local_auto_side_channel_mode(row),
                benchmark_row_graph_local_auto_dependency_scratch_mode(row),
            ),
            {},
        )[mode] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         method_mode,
         semantic_product_force,
         no_fact_emission,
         fact_families,
         graph_local_auto_side_channel_mode,
         graph_local_auto_dependency_scratch_mode), by_mode in sorted(grouped.items()):
        compact_row = by_mode.get("compact")
        legacy_row = by_mode.get("legacy-shadow")
        if compact_row is None or legacy_row is None:
            continue
        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "method_target_memoization": method_mode,
                "semantic_product_force": semantic_product_force,
                "no_fact_emission": no_fact_emission,
                "fact_families": fact_families,
                "graph_local_auto_side_channel_mode": graph_local_auto_side_channel_mode,
                "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
                "median_peak_rss_bytes_compact": compact_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_legacy_shadow": legacy_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_legacy_shadow_minus_compact":
                    int(legacy_row["median_peak_rss_bytes"]) - int(compact_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_compact": compact_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_legacy_shadow": legacy_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_legacy_shadow_minus_compact":
                    int(legacy_row["worst_peak_rss_bytes"]) - int(compact_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_compact": compact_row["median_wall_seconds"],
                "median_wall_seconds_legacy_shadow": legacy_row["median_wall_seconds"],
                "median_wall_seconds_legacy_shadow_minus_compact":
                    float(legacy_row["median_wall_seconds"]) - float(compact_row["median_wall_seconds"]),
                "worst_wall_seconds_compact": compact_row["worst_wall_seconds"],
                "worst_wall_seconds_legacy_shadow": legacy_row["worst_wall_seconds"],
                "worst_wall_seconds_legacy_shadow_minus_compact":
                    float(legacy_row["worst_wall_seconds"]) - float(compact_row["worst_wall_seconds"]),
            }
        )
    return deltas


def selected_graph_local_auto_side_channel_modes(selection: str) -> list[str]:
    if selection == "both":
        return ["flat", "legacy-shadow"]
    return [selection]


def compute_graph_local_auto_side_channel_mode_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str, str, str, bool, str, str], dict[str, dict]] = {}
    for row in results:
        mode = benchmark_row_graph_local_auto_side_channel_mode(row)
        if mode not in ("flat", "legacy-shadow"):
            continue
        method_mode = benchmark_row_method_target_memoization_mode(row)
        key_mode = benchmark_row_graph_local_auto_key_mode(row)
        grouped.setdefault(
            (
                row["fixture"],
                row["phase"],
                method_mode,
                key_mode,
                benchmark_row_semantic_product_force_mode(row),
                benchmark_row_no_fact_emission_mode(row),
                benchmark_row_fact_families_mode(row),
                benchmark_row_graph_local_auto_dependency_scratch_mode(row),
            ),
            {},
        )[mode] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         method_mode,
         key_mode,
         semantic_product_force,
         no_fact_emission,
         fact_families,
         graph_local_auto_dependency_scratch_mode), by_mode in sorted(grouped.items()):
        flat_row = by_mode.get("flat")
        legacy_row = by_mode.get("legacy-shadow")
        if flat_row is None or legacy_row is None:
            continue
        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "method_target_memoization": method_mode,
                "graph_local_auto_key_mode": key_mode,
                "semantic_product_force": semantic_product_force,
                "no_fact_emission": no_fact_emission,
                "fact_families": fact_families,
                "graph_local_auto_dependency_scratch_mode": graph_local_auto_dependency_scratch_mode,
                "median_peak_rss_bytes_flat": flat_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_legacy_shadow": legacy_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_legacy_shadow_minus_flat":
                    int(legacy_row["median_peak_rss_bytes"]) - int(flat_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_flat": flat_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_legacy_shadow": legacy_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_legacy_shadow_minus_flat":
                    int(legacy_row["worst_peak_rss_bytes"]) - int(flat_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_flat": flat_row["median_wall_seconds"],
                "median_wall_seconds_legacy_shadow": legacy_row["median_wall_seconds"],
                "median_wall_seconds_legacy_shadow_minus_flat":
                    float(legacy_row["median_wall_seconds"]) - float(flat_row["median_wall_seconds"]),
                "worst_wall_seconds_flat": flat_row["worst_wall_seconds"],
                "worst_wall_seconds_legacy_shadow": legacy_row["worst_wall_seconds"],
                "worst_wall_seconds_legacy_shadow_minus_flat":
                    float(legacy_row["worst_wall_seconds"]) - float(flat_row["worst_wall_seconds"]),
            }
        )
    return deltas


def selected_graph_local_auto_dependency_scratch_modes(selection: str) -> list[str]:
    if selection == "both":
        return ["pmr", "std"]
    return [selection]


def compute_graph_local_auto_dependency_scratch_mode_deltas(results: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, str, str, str, str, str, bool, str], dict[str, dict]] = {}
    for row in results:
        mode = benchmark_row_graph_local_auto_dependency_scratch_mode(row)
        if mode not in ("pmr", "std"):
            continue
        method_mode = benchmark_row_method_target_memoization_mode(row)
        key_mode = benchmark_row_graph_local_auto_key_mode(row)
        side_channel_mode = benchmark_row_graph_local_auto_side_channel_mode(row)
        grouped.setdefault(
            (
                row["fixture"],
                row["phase"],
                method_mode,
                key_mode,
                side_channel_mode,
                benchmark_row_semantic_product_force_mode(row),
                benchmark_row_no_fact_emission_mode(row),
                benchmark_row_fact_families_mode(row),
            ),
            {},
        )[mode] = row

    deltas: list[dict] = []
    for (fixture,
         phase,
         method_mode,
         key_mode,
         side_channel_mode,
         semantic_product_force,
         no_fact_emission,
         fact_families), by_mode in sorted(grouped.items()):
        pmr_row = by_mode.get("pmr")
        std_row = by_mode.get("std")
        if pmr_row is None or std_row is None:
            continue
        deltas.append(
            {
                "fixture": fixture,
                "phase": phase,
                "method_target_memoization": method_mode,
                "graph_local_auto_key_mode": key_mode,
                "graph_local_auto_side_channel_mode": side_channel_mode,
                "semantic_product_force": semantic_product_force,
                "no_fact_emission": no_fact_emission,
                "fact_families": fact_families,
                "median_peak_rss_bytes_pmr": pmr_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_std": std_row["median_peak_rss_bytes"],
                "median_peak_rss_bytes_std_minus_pmr":
                    int(std_row["median_peak_rss_bytes"]) - int(pmr_row["median_peak_rss_bytes"]),
                "worst_peak_rss_bytes_pmr": pmr_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_std": std_row["worst_peak_rss_bytes"],
                "worst_peak_rss_bytes_std_minus_pmr":
                    int(std_row["worst_peak_rss_bytes"]) - int(pmr_row["worst_peak_rss_bytes"]),
                "median_wall_seconds_pmr": pmr_row["median_wall_seconds"],
                "median_wall_seconds_std": std_row["median_wall_seconds"],
                "median_wall_seconds_std_minus_pmr":
                    float(std_row["median_wall_seconds"]) - float(pmr_row["median_wall_seconds"]),
                "worst_wall_seconds_pmr": pmr_row["worst_wall_seconds"],
                "worst_wall_seconds_std": std_row["worst_wall_seconds"],
                "worst_wall_seconds_std_minus_pmr":
                    float(std_row["worst_wall_seconds"]) - float(pmr_row["worst_wall_seconds"]),
            }
        )
    return deltas


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    primec = Path(args.primec)
    if not primec.is_absolute():
        primec = (repo_root / primec).resolve()
    if not primec.is_file():
        print(f"[semantic_memory_benchmark] primec not found: {primec}", file=sys.stderr)
        return 2

    if args.runs < 1:
        print("[semantic_memory_benchmark] --runs must be >= 1", file=sys.stderr)
        return 2
    if args.repeat_compile_leak_check_runs < 0:
        print("[semantic_memory_benchmark] --repeat-compile-leak-check-runs must be >= 0", file=sys.stderr)
        return 2

    semantic_validation_without_fact_emission_mode = args.semantic_validation_without_fact_emission
    if args.no_fact_emission and semantic_validation_without_fact_emission_mode == "off":
        semantic_validation_without_fact_emission_mode = "on"

    try:
        phases = parse_phases(args.phases)
        fixtures = select_fixtures(args.fixtures)
        fact_families = parse_fact_families(args.fact_families)
    except ValueError as exc:
        print(f"[semantic_memory_benchmark] {exc}", file=sys.stderr)
        return 2
    if args.semantic_product_force == "both" and "semantic-product" in phases:
        print(
            "[semantic_memory_benchmark] --semantic-product-force both requires "
            "--phases without semantic-product (use ast-semantic for A/B gate attribution)",
            file=sys.stderr,
        )
        return 2

    print(f"[semantic_memory_benchmark] repo_root={repo_root}")
    print(f"[semantic_memory_benchmark] primec={primec}")
    print(f"[semantic_memory_benchmark] runs={args.runs} phases={','.join(phases)}")
    print(
        "[semantic_memory_benchmark] "
        f"semantic_product_force={args.semantic_product_force} "
        f"semantic_validation_without_fact_emission={semantic_validation_without_fact_emission_mode} "
        f"fact_families={fact_families} "
        f"semantic_phase_counters={args.semantic_phase_counters} "
        f"semantic_allocation_counters={args.semantic_allocation_counters} "
        f"semantic_rss_checkpoints={args.semantic_rss_checkpoints} "
        f"method_target_memoization={args.method_target_memoization} "
        f"graph_local_auto_key_mode={args.graph_local_auto_key_mode} "
        f"graph_local_auto_side_channel_mode={args.graph_local_auto_side_channel_mode} "
        f"graph_local_auto_dependency_scratch_mode={args.graph_local_auto_dependency_scratch_mode} "
        f"repeat_compile_leak_check_runs={args.repeat_compile_leak_check_runs}"
    )

    results: list[dict] = []
    no_fact_emission_modes = selected_semantic_validation_without_fact_emission_modes(
        semantic_validation_without_fact_emission_mode,
        args.no_fact_emission,
    )
    semantic_product_force_modes = selected_semantic_product_force_modes(args.semantic_product_force)
    method_target_memoization_modes = selected_method_target_memoization_modes(
        args.method_target_memoization
    )
    graph_local_auto_key_modes = selected_graph_local_auto_key_modes(
        args.graph_local_auto_key_mode
    )
    graph_local_auto_side_channel_modes = selected_graph_local_auto_side_channel_modes(
        args.graph_local_auto_side_channel_mode
    )
    graph_local_auto_dependency_scratch_modes = selected_graph_local_auto_dependency_scratch_modes(
        args.graph_local_auto_dependency_scratch_mode
    )

    for fixture in fixtures:
        for phase in phases:
            for no_fact_emission in no_fact_emission_modes:
                for semantic_product_force in semantic_product_force_modes:
                    for method_target_memoization in method_target_memoization_modes:
                        for graph_local_auto_key_mode in graph_local_auto_key_modes:
                            for graph_local_auto_side_channel_mode in graph_local_auto_side_channel_modes:
                                for graph_local_auto_dependency_scratch_mode in graph_local_auto_dependency_scratch_modes:
                                    row = measure_fixture_phase(
                                        primec,
                                        repo_root,
                                        fixture,
                                        phase,
                                        args.runs,
                                        args.entry,
                                        semantic_product_force,
                                        no_fact_emission,
                                        fact_families,
                                        args.semantic_phase_counters,
                                        args.semantic_allocation_counters,
                                        args.semantic_rss_checkpoints,
                                        args.repeat_compile_leak_check_runs,
                                        method_target_memoization,
                                        graph_local_auto_key_mode,
                                        graph_local_auto_side_channel_mode,
                                        graph_local_auto_dependency_scratch_mode,
                                    )
                                    results.append(row)
                                    print(
                                        "[semantic_memory_benchmark] "
                                        f"fixture={row['fixture']} phase={row['phase']} "
                                        f"semantic_product_force={row['semantic_product_force']} "
                                        f"no_fact_emission={row['no_fact_emission']} "
                                        f"method_target_memoization={row['method_target_memoization']} "
                                        f"graph_local_auto_key_mode={row['graph_local_auto_key_mode']} "
                                        f"graph_local_auto_side_channel_mode={row['graph_local_auto_side_channel_mode']} "
                                        f"graph_local_auto_dependency_scratch_mode={row['graph_local_auto_dependency_scratch_mode']} "
                                        f"median_wall={row['median_wall_seconds']:.6f}s "
                                        f"worst_wall={row['worst_wall_seconds']:.6f}s "
                                        f"median_rss={row['median_peak_rss_bytes']} "
                                        f"worst_rss={row['worst_peak_rss_bytes']}"
                                    )

    report = {
        "schema": REPORT_SCHEMA,
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
        },
        "primec": path_for_report(primec, repo_root),
        "repo_root": ".",
        "entry": args.entry,
        "runs": args.runs,
        "phases": phases,
        "benchmark_options": {
            "semantic_product_force": args.semantic_product_force,
            "semantic_validation_without_fact_emission": semantic_validation_without_fact_emission_mode,
            "no_fact_emission": semantic_validation_without_fact_emission_mode == "on",
            "fact_families": fact_families,
            "semantic_phase_counters": args.semantic_phase_counters,
            "semantic_allocation_counters": args.semantic_allocation_counters,
            "semantic_rss_checkpoints": args.semantic_rss_checkpoints,
            "method_target_memoization": args.method_target_memoization,
            "graph_local_auto_key_mode": args.graph_local_auto_key_mode,
            "graph_local_auto_side_channel_mode": args.graph_local_auto_side_channel_mode,
            "graph_local_auto_dependency_scratch_mode": args.graph_local_auto_dependency_scratch_mode,
            "repeat_compile_leak_check_runs": args.repeat_compile_leak_check_runs,
        },
        "expensive_thresholds": {
            "max_wall_seconds": EXPENSIVE_RUNTIME_SECONDS,
            "max_peak_rss_bytes": EXPENSIVE_PEAK_RSS_BYTES,
        },
        "fixtures": [
            {
                "name": fixture.name,
                "source": fixture.source,
                "group": fixture.group,
                "scale_factor": fixture.scale_factor,
            }
            for fixture in fixtures
        ],
        "results": results,
        "semantic_product_force_deltas": compute_semantic_product_force_deltas(results),
        "semantic_validation_without_fact_emission_deltas":
            compute_semantic_validation_without_fact_emission_deltas(results),
        "method_target_memoization_deltas": compute_method_target_memoization_deltas(results),
        "graph_local_auto_key_mode_deltas": compute_graph_local_auto_key_mode_deltas(results),
        "graph_local_auto_side_channel_mode_deltas": compute_graph_local_auto_side_channel_mode_deltas(results),
        "graph_local_auto_dependency_scratch_mode_deltas":
            compute_graph_local_auto_dependency_scratch_mode_deltas(results),
        "expensive_offenders": collect_expensive_offenders(results),
        "scale_slopes": compute_scale_slopes(results),
    }

    if args.report_json:
        report_path = Path(args.report_json)
        if not report_path.is_absolute():
            report_path = (repo_root / report_path).resolve()
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"[semantic_memory_benchmark] wrote report: {report_path}")
    else:
        print(json.dumps(report, indent=2, sort_keys=True))

    return 0


if __name__ == "__main__":
    sys.exit(main())
