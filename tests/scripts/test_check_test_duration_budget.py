#!/usr/bin/env python3
"""Self-test for scripts/check_test_duration_budget.py."""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write_json  # noqa: E402


def shard(name: str, wall: float, timeout: float = 100.0) -> dict:
    cases = [{"name": f"c{i}", "duration_s": wall / 4} for i in range(4)]
    return {"ctest_name": name, "wall_time_s": wall, "ctest_timeout_s": timeout, "case_count": 4, "cases": cases}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("test duration budget")
    schema = "primestruct_test_durations_report_v1"
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)

        def check(shards: list, *extra: str):
            report = write_json(root / "report.json", {"schema": schema, "shards": shards})
            return run(repo, "check_test_duration_budget.py", "--report", str(report), *extra)

        result = check([shard("fast", 10.0)])
        checks.expect(result.returncode == 0 and "OK:" in result.stdout, "a shard inside the budget passes")
        result = check([shard("slow", 80.0)])
        checks.expect(result.returncode == 1 and "- slow" in result.stdout and "proposed resharding" in result.stdout,
                      "a shard over budget fails and proposes resharding")
        waivers = write_json(root / "waivers.json", {"^sl": "known"})
        result = check([shard("slow", 80.0)], "--waivers", str(waivers))
        checks.expect(result.returncode == 0 and "waived" in result.stdout, "a waived shard is reported, not failed")
        no_timeout = shard("untimed", 999.0)
        no_timeout["ctest_timeout_s"] = 0
        checks.expect(check([no_timeout]).returncode == 0, "shards without a timeout are skipped")
        bad = write_json(root / "bad.json", {"schema": "nope", "shards": []})
        checks.expect(run(repo, "check_test_duration_budget.py", "--report", str(bad)).returncode == 2,
                      "an unknown report schema is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
