#!/usr/bin/env python3
"""Self-test for scripts/check_vm_string_lookups.py (TODO-5364)."""

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
    spec = importlib.util.spec_from_file_location("check_vm_string_lookups", repo / "scripts" / "check_vm_string_lookups.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    failures = []

    def expect(condition, message):
        if not condition:
            failures.append(message)

    expect(module.find_violations({"src/runtime/A.cpp": "x = y;"}) == [], "clean file passes")
    expect(module.find_violations({"src/runtime/A.cpp": "m.stringTable[i]"}) != [], "direct access is reported")
    expect(module.find_violations({"src/runtime/A.cpp": "// stringTable is fine in comments"}) == [], "comments are ignored")
    expect(module.find_violations({"src/runtime/VmStringHeap.cpp": "module.stringTable[i]"}) == [], "the helper may index the table")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_vm_string_lookups self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
