#!/usr/bin/env python3
"""Self-test for scripts/check_semantic_memory_trend.py (TODO-5410)."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write_json  # noqa: E402

REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"


def report(rss: int) -> dict:
    return {"schema": REPORT_SCHEMA, "results": [{"fixture": "f", "phase": "p", "worst_peak_rss_bytes": rss}]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("semantic memory trend")
    policy = {
        "schema": "primestruct_semantic_memory_budget_policy_v1",
        "sustained_window": {"window_size": 2, "minimum_regressions": 2},
        "entries": [{"fixture": "f", "phase": "p", "soft_max_worst_peak_rss_bytes": 100,
                     "max_worst_peak_rss_bytes": 200}],
    }
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        pol = write_json(root / "policy.json", policy)
        hist = root / "history"
        trend = root / "trend.json"

        def check(current: int, *old: int):
            for stale in hist.glob("*.json") if hist.exists() else []:
                stale.unlink()
            for index, value in enumerate(old):
                write_json(hist / f"semantic_memory_report_{index}.json", report(value))
            cur = write_json(root / "current.json", report(current))
            return run(repo, "check_semantic_memory_trend.py", "--policy", str(pol), "--report", str(cur),
                       "--history-dir", str(hist), "--trend-report-json", str(trend))

        result = check(50)
        payload = json.loads(trend.read_text(encoding="utf-8"))
        checks.expect(result.returncode == 0 and payload["status"] == "passed" and payload["history_count"] == 0,
                      "a clean run with no history passes")
        result = check(150, 160)
        payload = json.loads(trend.read_text(encoding="utf-8"))
        checks.expect(result.returncode == 1 and payload["status"] == "failed" and payload["history_count"] == 1,
                      "a sustained regression found through history fails the trend gate")
        result = check(50, 10, 20, 30)
        payload = json.loads(trend.read_text(encoding="utf-8"))
        checks.expect(payload["history_count"] == 2, "history discovery is limited to the newest two reports")
        cur_dup = write_json(root / "dup.json", report(150))
        write_json(hist / "semantic_memory_report_dup.json", report(150))
        result = run(repo, "check_semantic_memory_trend.py", "--policy", str(pol), "--report", str(cur_dup),
                     "--history-dir", str(hist), "--history-limit", "1", "--trend-report-json", str(trend))
        payload = json.loads(trend.read_text(encoding="utf-8"))
        checks.expect(all(Path(p).name != "semantic_memory_report_dup.json" for p in payload["history_reports"]),
                      "a history report identical to the current report is skipped")
        checks.expect(run(repo, "check_semantic_memory_trend.py", "--policy", str(pol),
                          "--report", str(cur_dup), "--history-limit", "-1").returncode == 2,
                      "a negative history limit is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
