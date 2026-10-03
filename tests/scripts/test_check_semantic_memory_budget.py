#!/usr/bin/env python3
"""Self-test for scripts/check_semantic_memory_budget.py (TODO-5410)."""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write_json  # noqa: E402

POLICY_SCHEMA = "primestruct_semantic_memory_budget_policy_v1"
REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"


def report(rss: int) -> dict:
    return {"schema": REPORT_SCHEMA, "results": [{"fixture": "f", "phase": "p", "worst_peak_rss_bytes": rss}]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("semantic memory budget")
    policy = {
        "schema": POLICY_SCHEMA,
        "sustained_window": {"window_size": 2, "minimum_regressions": 2},
        "entries": [{"fixture": "f", "phase": "p", "soft_max_worst_peak_rss_bytes": 100,
                     "max_worst_peak_rss_bytes": 200}],
    }
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        pol = write_json(root / "policy.json", policy)

        def check(rss: int, *history: int):
            args = ["--policy", str(pol), "--report", str(write_json(root / "cur.json", report(rss)))]
            for index, value in enumerate(history):
                args += ["--history-report", str(write_json(root / f"h{index}.json", report(value)))]
            return run(repo, "check_semantic_memory_budget.py", *args)

        checks.expect(check(150).returncode == 0, "a report under the hard cap passes")
        result = check(250)
        checks.expect(result.returncode == 1 and "worst_peak_rss_bytes regression" in result.stderr,
                      "a report over the hard cap fails")
        result = check(150, 150)
        checks.expect(result.returncode == 1 and "sustained RSS regression" in result.stderr,
                      "repeated soft-cap breaches fail")
        checks.expect(check(150, 50).returncode == 0, "a single soft-cap breach in the window passes")
        stray = write_json(root / "stray.json", {"schema": REPORT_SCHEMA, "results": [
            {"fixture": "f", "phase": "p", "worst_peak_rss_bytes": 1},
            {"fixture": "g", "phase": "q", "worst_peak_rss_bytes": 1}]})
        result = run(repo, "check_semantic_memory_budget.py", "--policy", str(pol), "--report", str(stray))
        checks.expect(result.returncode == 1 and "missing policy entry" in result.stderr,
                      "a result without a policy entry fails")
        bad = write_json(root / "bad.json", {"schema": "wrong", "entries": [1]})
        checks.expect(run(repo, "check_semantic_memory_budget.py", "--policy", str(bad),
                          "--report", str(root / "cur.json")).returncode == 2, "an unknown policy schema is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
