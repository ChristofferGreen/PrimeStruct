#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path


BASELINE_SCHEMA = "primestruct_type_graph_budget_baseline_v1"
REPORT_SCHEMA = "primestruct_type_graph_budget_report_v1"

THRESHOLD_TO_METRIC = {
    "max_prepare_ms": "prepare_ms",
    "max_build_ms": "build_ms",
    "max_invalidation_local_binding": "invalidation_local_binding",
    "max_invalidation_control_flow": "invalidation_control_flow",
    "max_invalidation_initializer_shape": "invalidation_initializer_shape",
    "max_invalidation_definition_signature": "invalidation_definition_signature",
    "max_invalidation_import_alias": "invalidation_import_alias",
    "max_invalidation_receiver_type": "invalidation_receiver_type",
}


def load_json(path: Path, label: str) -> dict:
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except Exception as exc:
    raise SystemExit(f"[check_graph_budget] failed to read {label} {path}: {exc}") from exc


def parse_metrics_line(stdout: str) -> dict[str, str]:
  for line in stdout.splitlines():
    stripped = line.strip()
    if not stripped.startswith("metrics "):
      continue
    metrics: dict[str, str] = {}
    for token in stripped[len("metrics "):].split():
      if "=" not in token:
        continue
      key, value = token.split("=", 1)
      metrics[key] = value
    return metrics
  return {}


def parse_int_metric(metrics: dict[str, str], key: str, entry_name: str) -> int:
  raw = metrics.get(key)
  if raw is None:
    raise ValueError(f"missing metric '{key}' for entry '{entry_name}'")
  try:
    return int(raw)
  except ValueError as exc:
    raise ValueError(f"metric '{key}' for entry '{entry_name}' is not an integer: {raw}") from exc


def run_type_graph(primec: Path, repo_root: Path, source: str, entry_name: str) -> tuple[dict[str, str], str]:
  source_path = repo_root / source
  if not source_path.is_file():
    raise ValueError(f"entry '{entry_name}' source file does not exist: {source_path}")

  command = [str(primec), str(source_path), "--dump-stage", "type-graph"]
  result = subprocess.run(
      command,
      cwd=repo_root,
      capture_output=True,
      text=True,
      check=False,
  )
  if result.returncode != 0:
    stderr = result.stderr.strip()
    stdout = result.stdout.strip()
    details = stderr if stderr else stdout
    raise RuntimeError(
        f"entry '{entry_name}' failed ({result.returncode}) while running {' '.join(command)}\n{details}")

  metrics = parse_metrics_line(result.stdout)
  if not metrics:
    raise RuntimeError(f"entry '{entry_name}' did not emit a metrics line in type-graph output")
  return metrics, result.stdout


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Check type-resolution graph metrics against baseline budget thresholds.")
  parser.add_argument("--primec", required=True, help="Path to primec executable.")
  parser.add_argument("--baseline", required=True, help="Path to graph-budget baseline JSON.")
  parser.add_argument("--repo-root", default=".", help="Repository root for resolving entry source paths.")
  parser.add_argument("--report-json", help="Optional path to write observed metric report JSON.")
  args = parser.parse_args()

  repo_root = Path(args.repo_root).resolve()
  primec = Path(args.primec)
  if not primec.is_absolute():
    primec = (repo_root / primec).resolve()
  baseline_path = Path(args.baseline)
  if not baseline_path.is_absolute():
    baseline_path = (repo_root / baseline_path).resolve()

  if not primec.is_file():
    print(f"[check_graph_budget] primec not found: {primec}", file=sys.stderr)
    return 2

  baseline = load_json(baseline_path, "baseline")
  if baseline.get("schema") != BASELINE_SCHEMA:
    print("[check_graph_budget] unsupported baseline schema", file=sys.stderr)
    return 2

  entries = baseline.get("entries")
  if not isinstance(entries, list) or not entries:
    print("[check_graph_budget] baseline entries must be a non-empty array", file=sys.stderr)
    return 2

  failures: list[str] = []
  report_entries: list[dict] = []

  for entry in entries:
    if not isinstance(entry, dict):
      failures.append("baseline entry is not an object")
      continue

    name = str(entry.get("name", "")).strip()
    source = str(entry.get("source", "")).strip()
    if not name or not source:
      failures.append("baseline entries require non-empty 'name' and 'source'")
      continue

    try:
      metrics, _ = run_type_graph(primec, repo_root, source, name)
    except Exception as exc:  # pylint: disable=broad-except
      failures.append(str(exc))
      continue

    observed: dict[str, int] = {}
    for threshold_key, metric_key in THRESHOLD_TO_METRIC.items():
      if threshold_key not in entry:
        continue
      try:
        observed_value = parse_int_metric(metrics, metric_key, name)
      except ValueError as exc:
        failures.append(str(exc))
        continue

      observed[metric_key] = observed_value
      allowed = int(entry[threshold_key])
      if observed_value > allowed:
        failures.append(
            f"{name}:{metric_key} regression observed={observed_value} allowed<={allowed}")

    report_entries.append({
        "name": name,
        "source": source,
        "metrics": observed,
    })

    metrics_preview = " ".join(f"{key}={value}" for key, value in sorted(observed.items()))
    print(f"[check_graph_budget] {name}: {metrics_preview}")

  if args.report_json:
    report_path = Path(args.report_json)
    if not report_path.is_absolute():
      report_path = (repo_root / report_path).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": REPORT_SCHEMA,
        "baseline": str(baseline_path),
        "entries": report_entries,
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[check_graph_budget] wrote report: {report_path}")

  if failures:
    print("[check_graph_budget] graph budget check failed:", file=sys.stderr)
    for failure in failures:
      print(f"  - {failure}", file=sys.stderr)
    return 1

  print(f"[check_graph_budget] baseline check passed ({len(report_entries)} entries)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
