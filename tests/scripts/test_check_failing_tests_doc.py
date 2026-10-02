#!/usr/bin/env python3
"""Self-test for scripts/check_failing_tests_doc.py (TODO-5367)."""

from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_failing_tests_doc", repo / "scripts" / "check_failing_tests_doc.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    def runner(results):
        return lambda name: results.get(name, (0, "No tests were found!!!"))

    base = "# Failing Tests\n\n## Open Failures\n\n{body}\n\n## Current Failures\n"
    failures = []

    def expect(condition, message):
        if not condition:
            failures.append(message)

    expect(module.check(base.format(body="None."), runner({})) == [], "no entries passes")
    still_failing = base.format(body="- `PrimeStruct_a`: still red")
    expect(module.check(still_failing, runner({"PrimeStruct_a": (8, "1 tests failed")})) == [], "a failing entry passes")
    expect(module.check(still_failing, runner({"PrimeStruct_a": (0, "100% tests passed")})) != [], "an entry that passes is stale")
    expect(module.check(still_failing, runner({})) != [], "an unknown test name is reported")
    expect(module.check("# Failing Tests\n\nNo section\n", runner({})) != [], "missing section is reported")
    expect(module.check(base.format(body="None.") + "x\n" * 200, runner({})) != [], "an oversize file is reported")
    expect(module.open_failures(base.format(body="- `A`: x\n- `B`: y")) == ["A", "B"], "bullets are parsed")
    real = (repo / "docs" / "failing_tests.md").read_text(encoding="utf-8")
    expect(len(real.split("\n")) <= module.MAX_LINES, "the real file is under the line limit")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_failing_tests_doc self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
