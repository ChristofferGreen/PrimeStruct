#!/usr/bin/env python3
import argparse
import datetime
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"
TREND_REPORT_SCHEMA = "primestruct_semantic_memory_trend_report_v1"


def is_semantic_memory_report(path: Path) -> bool:
  try:
    payload = json.loads(path.read_text(encoding="utf-8"))
  except Exception:
    return False
  return payload.get("schema") == REPORT_SCHEMA


def file_sha256(path: Path) -> str:
  hasher = hashlib.sha256()
  with path.open("rb") as handle:
    while True:
      chunk = handle.read(8192)
      if not chunk:
        break
      hasher.update(chunk)
  return hasher.hexdigest()


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Run semantic-memory sustained trend gate against policy and current report.")
  parser.add_argument("--policy", required=True, help="Path to semantic memory budget policy JSON.")
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
  parser.add_argument("--report-json", default="", help="Optional budget-check summary report path.")
  parser.add_argument(
      "--trend-report-json",
      default="",
      help="Optional trend-check summary report path (includes selected history and checker status).")
  parser.add_argument(
      "--checker",
      default="",
      help="Optional explicit path to check_semantic_memory_budget.py.")
  parser.add_argument(
      "--ignore-wall-seconds",
      action="store_true",
      help="Forward --ignore-wall-seconds to check_semantic_memory_budget.py.",
  )
  args = parser.parse_args()

  if args.history_limit < 0:
    print("[check_semantic_memory_trend] --history-limit must be >= 0", file=sys.stderr)
    return 2

  policy_path = Path(args.policy).resolve()
  current_report_path = Path(args.report).resolve()
  if args.checker:
    checker_path = Path(args.checker).resolve()
  else:
    checker_path = (Path(__file__).resolve().parent / "check_semantic_memory_budget.py").resolve()
  current_report_digest = file_sha256(current_report_path)

  history_paths: list[Path] = []
  seen: set[Path] = set()
  for raw in args.history_report:
    path = Path(raw).resolve()
    if path == current_report_path or path in seen:
      continue
    if file_sha256(path) == current_report_digest:
      continue
    history_paths.append(path)
    seen.add(path)

  if args.history_dir:
    history_dir = Path(args.history_dir).resolve()
    if history_dir.is_dir():
      auto_candidates = sorted(history_dir.glob(args.history_glob), key=lambda path: path.name)
      auto_candidates = [path.resolve() for path in auto_candidates]
      auto_candidates = [path for path in auto_candidates
                         if path != current_report_path and path not in seen and path.is_file()]
      if args.history_limit > 0:
        auto_candidates = auto_candidates[-args.history_limit:]
      for path in auto_candidates:
        if not is_semantic_memory_report(path):
          continue
        if file_sha256(path) == current_report_digest:
          continue
        history_paths.append(path)
        seen.add(path)

  checker_report_path: Path | None = None
  temp_checker_report: Path | None = None
  if args.report_json:
    checker_report_path = Path(args.report_json).resolve()
  elif args.trend_report_json:
    temp_file = tempfile.NamedTemporaryFile(
        prefix="primestruct-semantic-memory-trend-check-", suffix=".json", delete=False)
    temp_file.close()
    temp_checker_report = Path(temp_file.name)
    checker_report_path = temp_checker_report

  command = [
      sys.executable,
      str(checker_path),
      "--policy",
      str(policy_path),
      "--report",
      str(current_report_path),
  ]
  for history_path in history_paths:
    command.extend(["--history-report", str(history_path)])
  if checker_report_path is not None:
    command.extend(["--report-json", str(checker_report_path)])
  if args.ignore_wall_seconds:
    command.append("--ignore-wall-seconds")

  if history_paths:
    print("[check_semantic_memory_trend] history reports:", flush=True)
    for history_path in history_paths:
      print(f"  - {history_path}", flush=True)
  else:
    print("[check_semantic_memory_trend] no history reports discovered", flush=True)

  result = subprocess.run(command, check=False)

  budget_summary: dict | None = None
  if checker_report_path is not None and checker_report_path.is_file():
    try:
      budget_summary = json.loads(checker_report_path.read_text(encoding="utf-8"))
    except Exception:
      budget_summary = None

  if args.trend_report_json:
    trend_report_path = Path(args.trend_report_json).resolve()
    trend_report_path.parent.mkdir(parents=True, exist_ok=True)
    trend_payload = {
        "schema": TREND_REPORT_SCHEMA,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "policy": str(policy_path),
        "report": str(current_report_path),
        "history_reports": [str(path) for path in history_paths],
        "history_count": len(history_paths),
        "checker": str(checker_path),
        "checker_exit_code": int(result.returncode),
        "ignore_wall_seconds": bool(args.ignore_wall_seconds),
        "status": "passed" if result.returncode == 0 else "failed",
        "budget_report": str(checker_report_path) if checker_report_path is not None else None,
        "budget_summary": budget_summary,
    }
    trend_report_path.write_text(json.dumps(trend_payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[check_semantic_memory_trend] wrote trend report: {trend_report_path}", flush=True)

  if temp_checker_report is not None:
    try:
      temp_checker_report.unlink(missing_ok=True)
    except Exception:
      pass

  return result.returncode


if __name__ == "__main__":
  sys.exit(main())
