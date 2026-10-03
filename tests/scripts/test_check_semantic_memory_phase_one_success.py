#!/usr/bin/env python3
"""Self-test for scripts/check_semantic_memory_phase_one_success.py (TODO-5410)."""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write_json  # noqa: E402

CRITERIA_SCHEMA = "primestruct_semantic_memory_phase_one_success_criteria_v1"
REPORT_SCHEMA = "primestruct_semantic_memory_report_v1"


def report(value: int) -> dict:
    return {"schema": REPORT_SCHEMA, "results": [{"fixture": "f", "phase": "p", "rss": value}]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("semantic memory phase one")
    criteria = {
        "schema": CRITERIA_SCHEMA,
        "sustained_gate": {"window_size": 2, "minimum_passes": 2},
        "criteria": [{"id": "c1", "fixture": "f", "phase": "p", "metric": "rss", "baseline_value": 1000,
                      "target_reduction_ratio": 0.5, "absolute_cap_value": 800}],
    }
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        crit = write_json(root / "criteria.json", criteria)

        def check(value: int, *extra: str, history: tuple[int, ...] = ()):
            args = ["--criteria", str(crit), "--report", str(write_json(root / "cur.json", report(value)))]
            for index, old in enumerate(history):
                args += ["--history-report", str(write_json(root / f"h{index}.json", report(old)))]
            return run(repo, "check_semantic_memory_phase_one_success.py", *args, *extra)

        checks.expect(check(400).returncode == 0, "a value under the effective target passes")
        result = check(600)
        checks.expect(result.returncode == 1 and "current criterion failed" in result.stderr,
                      "a value over the effective target fails")
        result = check(400, history=(600,))
        checks.expect(result.returncode == 1 and "sustained window failed" in result.stderr,
                      "a failing history sample breaks the sustained window")
        checks.expect(check(400, history=(300,)).returncode == 0, "a passing sustained window passes")
        result = check(400, "--require-full-window")
        checks.expect(result.returncode == 1 and "insufficient sustained window" in result.stderr,
                      "--require-full-window rejects a short history")
        bad = write_json(root / "bad.json", {"schema": "wrong"})
        checks.expect(run(repo, "check_semantic_memory_phase_one_success.py", "--criteria", str(bad),
                          "--report", str(root / "cur.json")).returncode == 2, "an unknown criteria schema is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
