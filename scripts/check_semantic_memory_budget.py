#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


POLICY_SCHEMA = "primestruct_semantic_memory_budget_policy_v1"
REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"


def load_json(path: Path, label: str) -> dict:
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except Exception as exc:
    raise SystemExit(f"[check_semantic_memory_budget] failed to read {label} {path}: {exc}") from exc


def to_result_map(report: dict, label: str) -> dict[tuple[str, str], dict]:
  if report.get("schema") != REPORT_SCHEMA:
    raise ValueError(f"{label} schema must be {REPORT_SCHEMA}")
  rows = report.get("results")
  if not isinstance(rows, list):
    raise ValueError(f"{label} missing results array")

  mapped: dict[tuple[str, str], dict] = {}
  for row in rows:
    if not isinstance(row, dict):
      continue
    fixture = str(row.get("fixture", "")).strip()
    phase = str(row.get("phase", "")).strip()
    if not fixture or not phase:
      continue
    mapped[(fixture, phase)] = row
  return mapped


def as_float(row: dict, key: str, context: str) -> float:
  try:
    return float(row[key])
  except Exception as exc:
    raise ValueError(f"{context} missing/invalid {key}") from exc


def as_int(row: dict, key: str, context: str) -> int:
  try:
    return int(row[key])
  except Exception as exc:
    raise ValueError(f"{context} missing/invalid {key}") from exc


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Check semantic-memory report against per-fixture budget policy.")
  parser.add_argument("--policy", required=True, help="Path to semantic memory budget policy JSON.")
  parser.add_argument("--report", required=True, help="Path to current semantic memory report JSON.")
  parser.add_argument(
      "--history-report",
      action="append",
      default=[],
      help="Optional prior semantic memory report JSON (repeatable) for sustained-window checks.",
  )
  parser.add_argument("--report-json", help="Optional output path for observed checker summary JSON.")
  args = parser.parse_args()

  policy_path = Path(args.policy).resolve()
  current_report_path = Path(args.report).resolve()
  history_report_paths = [Path(path).resolve() for path in args.history_report]

  policy = load_json(policy_path, "policy")
  if policy.get("schema") != POLICY_SCHEMA:
    print(f"[check_semantic_memory_budget] unsupported policy schema: {policy.get('schema')}", file=sys.stderr)
    return 2

  entries = policy.get("entries")
  if not isinstance(entries, list) or not entries:
    print("[check_semantic_memory_budget] policy entries must be a non-empty array", file=sys.stderr)
    return 2

  sustained_window = policy.get("sustained_window", {})
  try:
    window_size = int(sustained_window.get("window_size", 3))
    minimum_regressions = int(sustained_window.get("minimum_regressions", 2))
  except Exception:
    print("[check_semantic_memory_budget] sustained_window values must be integers", file=sys.stderr)
    return 2
  if window_size < 1 or minimum_regressions < 1 or minimum_regressions > window_size:
    print("[check_semantic_memory_budget] sustained_window values are invalid", file=sys.stderr)
    return 2

  current_report = load_json(current_report_path, "report")
  history_reports = [load_json(path, f"history report {path}") for path in history_report_paths]

  try:
    current_map = to_result_map(current_report, "report")
    history_maps = [to_result_map(report, f"history report {history_report_paths[idx]}")
                    for idx, report in enumerate(history_reports)]
  except ValueError as exc:
    print(f"[check_semantic_memory_budget] {exc}", file=sys.stderr)
    return 2

  failures: list[str] = []
  summaries: list[dict] = []

  ordered_maps = history_maps + [current_map]
  for raw_entry in entries:
    if not isinstance(raw_entry, dict):
      failures.append("policy entry is not an object")
      continue

    fixture = str(raw_entry.get("fixture", "")).strip()
    phase = str(raw_entry.get("phase", "")).strip()
    if not fixture or not phase:
      failures.append("policy entries require non-empty fixture and phase")
      continue
    key = (fixture, phase)
    entry_name = f"{fixture}:{phase}"

    try:
      soft_rss = as_int(raw_entry, "soft_max_worst_peak_rss_bytes", entry_name)
      soft_wall = as_float(raw_entry, "soft_max_worst_wall_seconds", entry_name)
      max_rss = as_int(raw_entry, "max_worst_peak_rss_bytes", entry_name)
      max_wall = as_float(raw_entry, "max_worst_wall_seconds", entry_name)
    except ValueError as exc:
      failures.append(str(exc))
      continue

    current_row = current_map.get(key)
    if current_row is None:
      failures.append(f"missing report result for {entry_name}")
      continue

    try:
      observed_rss = as_int(current_row, "worst_peak_rss_bytes", f"report:{entry_name}")
      observed_wall = as_float(current_row, "worst_wall_seconds", f"report:{entry_name}")
    except ValueError as exc:
      failures.append(str(exc))
      continue

    if observed_rss > max_rss:
      failures.append(
          f"{entry_name} worst_peak_rss_bytes regression observed={observed_rss} allowed<={max_rss}")
    if observed_wall > max_wall:
      failures.append(
          f"{entry_name} worst_wall_seconds regression observed={observed_wall:.6f} allowed<={max_wall:.6f}")

    recent_rows = [mapped[key] for mapped in ordered_maps if key in mapped]
    recent_window = recent_rows[-window_size:] if recent_rows else []
    sustained_rss_regressions = 0
    sustained_wall_regressions = 0
    if len(recent_window) == window_size:
      for row in recent_window:
        rss_value = as_int(row, "worst_peak_rss_bytes", f"window:{entry_name}")
        wall_value = as_float(row, "worst_wall_seconds", f"window:{entry_name}")
        if rss_value > soft_rss:
          sustained_rss_regressions += 1
        if wall_value > soft_wall:
          sustained_wall_regressions += 1
      if sustained_rss_regressions >= minimum_regressions:
        failures.append(
            f"{entry_name} sustained RSS regression count={sustained_rss_regressions} "
            f"window={window_size} threshold={minimum_regressions}")
      if sustained_wall_regressions >= minimum_regressions:
        failures.append(
            f"{entry_name} sustained wall regression count={sustained_wall_regressions} "
            f"window={window_size} threshold={minimum_regressions}")

    summaries.append({
        "fixture": fixture,
        "phase": phase,
        "observed_worst_peak_rss_bytes": observed_rss,
        "observed_worst_wall_seconds": observed_wall,
        "soft_max_worst_peak_rss_bytes": soft_rss,
        "soft_max_worst_wall_seconds": soft_wall,
        "max_worst_peak_rss_bytes": max_rss,
        "max_worst_wall_seconds": max_wall,
        "sustained_window_size": window_size,
        "sustained_minimum_regressions": minimum_regressions,
        "sustained_window_samples": len(recent_window),
        "sustained_rss_regressions": sustained_rss_regressions,
        "sustained_wall_regressions": sustained_wall_regressions,
    })
    print(
        "[check_semantic_memory_budget] "
        f"{entry_name} rss={observed_rss}/{max_rss} wall={observed_wall:.6f}/{max_wall:.6f}")

  if args.report_json:
    report_path = Path(args.report_json).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "primestruct_semantic_memory_budget_check_report_v1",
        "policy": str(policy_path),
        "report": str(current_report_path),
        "history_reports": [str(path) for path in history_report_paths],
        "entries": summaries,
        "failure_count": len(failures),
    }
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[check_semantic_memory_budget] wrote report: {report_path}")

  if failures:
    print("[check_semantic_memory_budget] budget check failed:", file=sys.stderr)
    for failure in failures:
      print(f"  - {failure}", file=sys.stderr)
    return 1

  print(f"[check_semantic_memory_budget] policy check passed ({len(summaries)} entries)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
