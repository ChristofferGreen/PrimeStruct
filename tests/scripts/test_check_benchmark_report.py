#!/usr/bin/env python3
"""Self-test for scripts/check_benchmark_report.py."""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write_json  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("benchmark report")
    baseline = {
        "schema": "primestruct_benchmark_baseline_v1",
        "entries": [
            {"phase": "runtime", "benchmark": "b", "entry": "e", "max_mean_seconds": 1.0,
             "max_artifact_size_bytes": 1000},
            {"phase": "compile", "benchmark": "opt", "entry": "e", "optional": True, "max_mean_seconds": 1.0},
        ],
    }

    def report(mean: float, size: int) -> dict:
        return {"schema": "primestruct_benchmark_report_v1",
                "runtime_results": [{"benchmark": "b", "entry": "e", "mean_seconds": mean,
                                     "artifact_size_bytes": size}]}

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        base = write_json(root / "baseline.json", baseline)

        def check(rep: dict, name: str, *extra: str):
            return run(repo, "check_benchmark_report.py", "--baseline", str(base),
                       "--report", str(write_json(root / f"{name}.json", rep)), *extra)

        checks.expect(check(report(1.0, 1000), "ok").returncode == 0, "report within thresholds passes")
        slow = check(report(2.0, 1000), "slow")
        checks.expect(slow.returncode == 1 and "mean_seconds regression" in slow.stderr, "a slow mean fails")
        big = check(report(1.0, 2000), "big")
        checks.expect(big.returncode == 1 and "artifact_size regression" in big.stderr, "a large artifact fails")
        checks.expect(check(report(1.2, 1000), "ratio", "--max-regression-ratio", "1.1").returncode == 1,
                      "a tighter ratio fails a borderline run")
        missing = check({"schema": "primestruct_benchmark_report_v1"}, "missing")
        checks.expect(missing.returncode == 1 and "missing report entry" in missing.stderr,
                      "a missing required entry fails")
        bad = write_json(root / "bad_baseline.json", {"schema": "wrong"})
        bad_run = run(repo, "check_benchmark_report.py", "--baseline", str(bad),
                      "--report", str(write_json(root / "r.json", report(1.0, 1000))))
        checks.expect(bad_run.returncode == 2, "an unknown baseline schema is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
