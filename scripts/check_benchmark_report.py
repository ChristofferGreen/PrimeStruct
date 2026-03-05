#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


def load_json(path: Path, label: str) -> dict:
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except Exception as exc:
    raise SystemExit(f"[check_benchmark_report] failed to read {label} {path}: {exc}") from exc


def make_report_map(report: dict) -> dict:
  if report.get("schema") != "primestruct_benchmark_report_v1":
    raise SystemExit("[check_benchmark_report] unsupported report schema")
  mapped = {}
  for entry in report.get("runtime_results", []):
    key = ("runtime", entry.get("benchmark", ""), entry.get("entry", ""))
    mapped[key] = entry
  for entry in report.get("compile_results", []):
    key = ("compile", entry.get("benchmark", ""), entry.get("entry", ""))
    mapped[key] = entry
  return mapped


def format_key(key) -> str:
  phase, bench, entry = key
  return f"{phase}:{bench}:{entry}"


def main() -> int:
  parser = argparse.ArgumentParser(description="Check benchmark report against stored baseline thresholds.")
  parser.add_argument("--baseline", required=True, help="Path to benchmark baseline JSON.")
  parser.add_argument("--report", required=True, help="Path to benchmark report JSON.")
  parser.add_argument("--max-regression-ratio", type=float, default=1.25,
                      help="Allowed runtime/compile mean-seconds regression ratio.")
  parser.add_argument("--size-regression-ratio", type=float, default=1.05,
                      help="Allowed artifact-size regression ratio.")
  args = parser.parse_args()

  baseline_path = Path(args.baseline)
  report_path = Path(args.report)
  baseline = load_json(baseline_path, "baseline")
  report = load_json(report_path, "report")

  if baseline.get("schema") != "primestruct_benchmark_baseline_v1":
    print("[check_benchmark_report] unsupported baseline schema", file=sys.stderr)
    return 2
  if args.max_regression_ratio <= 0.0:
    print("[check_benchmark_report] --max-regression-ratio must be > 0", file=sys.stderr)
    return 2
  if args.size_regression_ratio <= 0.0:
    print("[check_benchmark_report] --size-regression-ratio must be > 0", file=sys.stderr)
    return 2

  report_map = make_report_map(report)
  baseline_entries = baseline.get("entries", [])
  if not isinstance(baseline_entries, list):
    print("[check_benchmark_report] baseline entries must be an array", file=sys.stderr)
    return 2

  failures = []
  checked = 0

  for baseline_entry in sorted(baseline_entries,
                               key=lambda it: (
                                   it.get("phase", ""),
                                   it.get("benchmark", ""),
                                   it.get("entry", ""))):
    phase = baseline_entry.get("phase", "")
    benchmark = baseline_entry.get("benchmark", "")
    entry = baseline_entry.get("entry", "")
    if phase not in ("runtime", "compile"):
      failures.append(f"invalid baseline phase for {phase}:{benchmark}:{entry}")
      continue
    key = (phase, benchmark, entry)
    optional = bool(baseline_entry.get("optional", False))
    observed = report_map.get(key)
    if observed is None:
      if optional:
        continue
      failures.append(f"missing report entry {format_key(key)}")
      continue

    checked += 1
    observed_mean = float(observed.get("mean_seconds", 0.0))
    observed_size = int(observed.get("artifact_size_bytes", 0))

    baseline_mean = baseline_entry.get("max_mean_seconds")
    if baseline_mean is not None:
      allowed_mean = float(baseline_mean) * args.max_regression_ratio
      if observed_mean > allowed_mean:
        failures.append(
            f"{format_key(key)} mean_seconds regression: observed={observed_mean:.6f}s "
            f"allowed<={allowed_mean:.6f}s")

    baseline_size = baseline_entry.get("max_artifact_size_bytes")
    if baseline_size is not None:
      allowed_size = int(float(baseline_size) * args.size_regression_ratio)
      if observed_size > allowed_size:
        failures.append(
            f"{format_key(key)} artifact_size regression: observed={observed_size} "
            f"allowed<={allowed_size}")

  if failures:
    print("[check_benchmark_report] benchmark regression check failed:", file=sys.stderr)
    for line in failures:
      print(f"  - {line}", file=sys.stderr)
    return 1

  print(f"[check_benchmark_report] baseline check passed ({checked} entries)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
