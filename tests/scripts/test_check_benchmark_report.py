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

    platform_baseline = {
        "schema": "primestruct_benchmark_baseline_v1",
        "entries": [
            {"phase": "runtime", "benchmark": "n", "entry": "e", "platform": "Linux/x86_64",
             "max_artifact_size_bytes": 100},
            {"phase": "runtime", "benchmark": "n", "entry": "e", "platform": ["Darwin/arm64", "Darwin/x86_64"],
             "max_artifact_size_bytes": 1000},
            {"phase": "runtime", "benchmark": "only_linux", "entry": "e", "platform": "Linux/x86_64",
             "max_artifact_size_bytes": 100},
        ],
    }

    def platform_report(system: str, machine: str, size: int) -> dict:
        return {"schema": "primestruct_benchmark_report_v1",
                "platform": {"system": system, "machine": machine},
                "runtime_results": [{"benchmark": "n", "entry": "e", "mean_seconds": 0.0,
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

        pbase = write_json(root / "platform_baseline.json", platform_baseline)

        def platform_check(rep: dict, name: str):
            return run(repo, "check_benchmark_report.py", "--baseline", str(pbase),
                       "--report", str(write_json(root / f"{name}.json", rep)))

        mac = platform_check(platform_report("Darwin", "arm64", 900), "mac_ok")
        checks.expect(mac.returncode == 0 and "(1 entries)" in mac.stdout,
                      "an entry for another platform is not checked and a missing row of it is not an error")
        mac_big = platform_check(platform_report("Darwin", "arm64", 2000), "mac_big")
        checks.expect(mac_big.returncode == 1 and "artifact_size regression" in mac_big.stderr,
                      "the entry of the report's own platform still fails a large artifact")
        linux = platform_check(platform_report("Linux", "x86_64", 90), "linux_ok")
        checks.expect(linux.returncode == 1 and "only_linux" in linux.stderr
                      and "missing report entry" in linux.stderr,
                      "a Linux report is checked against the Linux entries only")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
