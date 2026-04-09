#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


CRITERIA_SCHEMA = "primestruct_semantic_memory_phase_one_success_criteria_v1"
REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"


def load_json(path: Path, label: str) -> dict:
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except Exception as exc:
    raise SystemExit(f"[check_semantic_memory_phase_one_success] failed to read {label} {path}: {exc}") from exc


def is_semantic_memory_report(path: Path) -> bool:
  try:
    payload = json.loads(path.read_text(encoding="utf-8"))
  except Exception:
    return False
  return payload.get("schema") == REPORT_SCHEMA


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


def as_float(value, context: str) -> float:
  try:
    return float(value)
  except Exception as exc:
    raise ValueError(f"{context} must be numeric") from exc


def as_metric_value(row: dict, key: str, context: str) -> float:
  if key not in row:
    raise ValueError(f"{context} missing metric '{key}'")
  return as_float(row[key], f"{context}:{key}")


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Check semantic-memory phase-one success criteria against current/history reports.")
  parser.add_argument("--criteria", required=True, help="Path to phase-one success criteria JSON.")
  parser.add_argument("--report", required=True, help="Path to current semantic memory report JSON.")
  parser.add_argument(
      "--history-report",
      action="append",
      default=[],
      help="Optional prior semantic memory report JSON (repeatable).")
  parser.add_argument(
      "--history-dir",
      default="",
      help="Optional directory with prior semantic memory report JSON files.")
  parser.add_argument(
      "--history-glob",
      default="semantic_memory_report*.json",
      help="Glob for history files inside --history-dir.")
  parser.add_argument(
      "--history-limit",
      type=int,
      default=2,
      help="Maximum number of auto-discovered history reports from --history-dir (default: 2).")
  parser.add_argument(
      "--require-full-window",
      action="store_true",
      help="Fail if a sustained window cannot be formed from history + current report.")
  parser.add_argument("--report-json", default="", help="Optional checker summary report path.")
  args = parser.parse_args()

  if args.history_limit < 0:
    print("[check_semantic_memory_phase_one_success] --history-limit must be >= 0", file=sys.stderr)
    return 2

  criteria_path = Path(args.criteria).resolve()
  current_report_path = Path(args.report).resolve()
  criteria = load_json(criteria_path, "criteria")
  current_report = load_json(current_report_path, "report")

  if criteria.get("schema") != CRITERIA_SCHEMA:
    print(f"[check_semantic_memory_phase_one_success] unsupported criteria schema: {criteria.get('schema')}",
          file=sys.stderr)
    return 2

  entries = criteria.get("criteria")
  if not isinstance(entries, list) or not entries:
    print("[check_semantic_memory_phase_one_success] criteria entries must be a non-empty array", file=sys.stderr)
    return 2

  sustained_gate = criteria.get("sustained_gate", {})
  try:
    window_size = int(sustained_gate.get("window_size", 3))
    minimum_passes = int(sustained_gate.get("minimum_passes", 2))
  except Exception:
    print("[check_semantic_memory_phase_one_success] sustained_gate values must be integers", file=sys.stderr)
    return 2
  if window_size < 1 or minimum_passes < 1 or minimum_passes > window_size:
    print("[check_semantic_memory_phase_one_success] sustained_gate values are invalid", file=sys.stderr)
    return 2

  history_paths: list[Path] = []
  seen: set[Path] = set()
  for raw in args.history_report:
    path = Path(raw).resolve()
    if path == current_report_path or path in seen:
      continue
    history_paths.append(path)
    seen.add(path)

  if args.history_dir:
    history_dir = Path(args.history_dir).resolve()
    if history_dir.is_dir():
      auto_candidates = sorted(history_dir.glob(args.history_glob), key=lambda path: path.name)
      auto_candidates = [path.resolve() for path in auto_candidates]
      auto_candidates = [
          path for path in auto_candidates if path != current_report_path and path not in seen and path.is_file()
      ]
      if args.history_limit > 0:
        auto_candidates = auto_candidates[-args.history_limit:]
      for path in auto_candidates:
        if not is_semantic_memory_report(path):
          continue
        history_paths.append(path)
        seen.add(path)

  history_reports = [load_json(path, f"history report {path}") for path in history_paths]
  try:
    current_map = to_result_map(current_report, "report")
    history_maps = [to_result_map(report, f"history report {history_paths[idx]}")
                    for idx, report in enumerate(history_reports)]
  except ValueError as exc:
    print(f"[check_semantic_memory_phase_one_success] {exc}", file=sys.stderr)
    return 2

  failures: list[str] = []
  summaries: list[dict] = []

  ordered_maps = history_maps + [current_map]
  for raw in entries:
    if not isinstance(raw, dict):
      failures.append("criteria entry is not an object")
      continue

    criterion_id = str(raw.get("id", "")).strip()
    fixture = str(raw.get("fixture", "")).strip()
    phase = str(raw.get("phase", "")).strip()
    metric = str(raw.get("metric", "")).strip()
    if not criterion_id or not fixture or not phase or not metric:
      failures.append("criteria entries require id, fixture, phase, and metric")
      continue

    key = (fixture, phase)
    context = f"{criterion_id}:{fixture}:{phase}:{metric}"
    try:
      baseline_value = as_float(raw.get("baseline_value"), f"{context}:baseline_value")
      target_reduction_ratio = as_float(raw.get("target_reduction_ratio", 0.0),
                                        f"{context}:target_reduction_ratio")
      absolute_cap_value = as_float(raw.get("absolute_cap_value"), f"{context}:absolute_cap_value")
    except ValueError as exc:
      failures.append(str(exc))
      continue

    if target_reduction_ratio < 0.0 or target_reduction_ratio > 1.0:
      failures.append(f"{context} target_reduction_ratio must be within [0,1]")
      continue

    ratio_target = baseline_value * (1.0 - target_reduction_ratio)
    effective_target = min(ratio_target, absolute_cap_value)

    if "effective_target_value" in raw:
      try:
        expected_effective_target = as_float(raw.get("effective_target_value"),
                                             f"{context}:effective_target_value")
      except ValueError as exc:
        failures.append(str(exc))
        continue
      if abs(expected_effective_target - effective_target) > 0.5:
        failures.append(
            f"{context} effective_target_value mismatch expected={expected_effective_target} "
            f"computed={effective_target}")

    current_row = current_map.get(key)
    if current_row is None:
      failures.append(f"missing report result for {fixture}:{phase}")
      continue

    try:
      observed_value = as_metric_value(current_row, metric, f"report:{fixture}:{phase}")
    except ValueError as exc:
      failures.append(str(exc))
      continue

    current_pass = observed_value <= effective_target
    if not current_pass:
      failures.append(
          f"{context} current criterion failed observed={observed_value} target<={effective_target}")

    recent_values: list[float] = []
    for mapped in ordered_maps:
      row = mapped.get(key)
      if row is None or metric not in row:
        continue
      try:
        recent_values.append(as_float(row[metric], f"window:{context}"))
      except ValueError:
        continue
    recent_window = recent_values[-window_size:] if recent_values else []

    sustained_pass_count = sum(1 for value in recent_window if value <= effective_target)
    sustained_pass = len(recent_window) == window_size and sustained_pass_count >= minimum_passes
    if len(recent_window) == window_size:
      if sustained_pass_count < minimum_passes:
        failures.append(
            f"{context} sustained window failed pass_count={sustained_pass_count} "
            f"window={window_size} minimum_passes={minimum_passes}")
    elif args.require_full_window:
      failures.append(
          f"{context} insufficient sustained window samples={len(recent_window)} required={window_size}")

    summaries.append({
        "id": criterion_id,
        "fixture": fixture,
        "phase": phase,
        "metric": metric,
        "baseline_value": baseline_value,
        "target_reduction_ratio": target_reduction_ratio,
        "absolute_cap_value": absolute_cap_value,
        "effective_target_value": effective_target,
        "observed_value": observed_value,
        "current_pass": current_pass,
        "sustained_window_size": window_size,
        "sustained_minimum_passes": minimum_passes,
        "sustained_window_samples": len(recent_window),
        "sustained_pass_count": sustained_pass_count,
        "sustained_pass": sustained_pass,
    })
    print(
        "[check_semantic_memory_phase_one_success] "
        f"{criterion_id} observed={observed_value} target<={effective_target} "
        f"sustained_passes={sustained_pass_count}/{len(recent_window)}")

  if args.report_json:
    report_path = Path(args.report_json).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema": "primestruct_semantic_memory_phase_one_check_report_v1",
        "criteria": str(criteria_path),
        "report": str(current_report_path),
        "history_reports": [str(path) for path in history_paths],
        "entries": summaries,
        "failure_count": len(failures),
    }
    report_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[check_semantic_memory_phase_one_success] wrote report: {report_path}")

  if failures:
    print("[check_semantic_memory_phase_one_success] phase-one check failed:", file=sys.stderr)
    for failure in failures:
      print(f"  - {failure}", file=sys.stderr)
    return 1

  print(f"[check_semantic_memory_phase_one_success] phase-one check passed ({len(summaries)} entries)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
