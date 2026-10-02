#!/usr/bin/env python3
"""Self-test for scripts/check_source_file_sizes.py (TODO-5353)."""

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
    spec = importlib.util.spec_from_file_location("check_source_file_sizes", repo / "scripts" / "check_source_file_sizes.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures = []

    def expect(condition, message):
        if not condition:
            failures.append(message)

    big = module.MAX_LINES + 50
    expect(module.find_violations({"a.cpp": 10}, {}) == [], "small files pass")
    expect(module.find_violations({"a.cpp": big}, {}) != [], "an oversize file is reported")
    expect(module.find_violations({"a.cpp": big}, {"a.cpp": (big, "why")}) == [], "an allowlisted file at its limit passes")
    expect(module.find_violations({"a.cpp": big + 1}, {"a.cpp": (big, "why")}) != [], "growth past the limit is reported")
    expect(module.find_violations({"a.cpp": 10}, {"a.cpp": (big, "why")}) != [], "a stale entry is reported")
    expect(module.find_violations({}, {"gone.cpp": (big, "why")}) != [], "a missing file's entry is reported")
    expect(module.parse_allowlist("# c\n\nx.cpp 1300 because\n") == {"x.cpp": (1300, "because")}, "allowlist parses")
    try:
        module.parse_allowlist("x.cpp nope\n")
        failures.append("a malformed allowlist line must raise")
    except ValueError:
        pass
    real = module.find_violations(
        module.collect_sizes(repo), module.parse_allowlist((repo / module.ALLOWLIST).read_text(encoding="utf-8"))
    )
    expect(real == [], f"the repository passes its own check: {real}")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_source_file_sizes self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
