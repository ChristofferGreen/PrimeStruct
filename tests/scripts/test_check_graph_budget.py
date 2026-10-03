#!/usr/bin/env python3
"""Self-test for scripts/check_graph_budget.py.

Uses a fake `primec` shell stand-in that prints a canned type-graph metrics line.
"""

from __future__ import annotations

import argparse
import json
import os
import stat
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write, write_json  # noqa: E402

SCHEMA = "primestruct_type_graph_budget_baseline_v1"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("graph budget")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write(root / "sample.prime", "[return<int>] main() { return(0i32) }\n")
        fake = write(root / "primec", "#!/bin/sh\necho 'metrics prepare_ms=5 build_ms=7 invalidation_local_binding=3'\n")
        fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
        failing = write(root / "primec_fail", "#!/bin/sh\necho boom >&2\nexit 3\n")
        failing.chmod(failing.stat().st_mode | stat.S_IXUSR)

        def baseline(**limits) -> Path:
            entry = {"name": "e", "source": "sample.prime", **limits}
            return write_json(root / "baseline.json", {"schema": SCHEMA, "entries": [entry]})

        def check(base: Path, primec: Path = fake, *extra: str):
            return run(repo, "check_graph_budget.py", "--primec", str(primec), "--baseline", str(base),
                       "--repo-root", str(root), *extra)

        report = root / "out" / "report.json"
        result = check(baseline(max_prepare_ms=10, max_build_ms=7, max_invalidation_local_binding=3), fake,
                       "--report-json", str(report))
        checks.expect(result.returncode == 0 and json.loads(report.read_text())["entries"][0]["metrics"]["build_ms"] == 7,
                      "metrics at the limits pass and are reported")
        result = check(baseline(max_prepare_ms=4))
        checks.expect(result.returncode == 1 and "prepare_ms regression" in result.stderr, "a metric over its limit fails")
        result = check(baseline(max_invalidation_import_alias=1))
        checks.expect(result.returncode == 1 and "missing metric" in result.stderr, "a missing metric fails")
        result = check(baseline(max_prepare_ms=10), failing)
        checks.expect(result.returncode == 1 and "boom" in result.stderr, "a failing primec run fails the check")
        checks.expect(check(baseline(), root / "nope").returncode == 2, "a missing primec is rejected")
        bad = write_json(root / "bad.json", {"schema": "wrong"})
        checks.expect(check(bad).returncode == 2, "an unknown baseline schema is rejected")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
