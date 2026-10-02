#!/usr/bin/env python3
"""Self-test for scripts/check_collection_family_compares.py (TODO-5374)."""

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
    spec = importlib.util.spec_from_file_location(
        "check_collection_family_compares", repo / "scripts" / "check_collection_family_compares.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    count = module.count_compares
    expect(count("if (x == collection_helpers::kRootedVector) {}") == 1, "== against a family root counts")
    expect(count("if (x != kRootedMap) {}") == 1, "!= against an unqualified family root counts")
    expect(count("if (kRootedSoa == x) {}") == 1, "a reversed comparison counts")
    expect(count("path == kRootedVectorCount") == 0, "a helper path constant is not a family root")
    expect(count("path.rfind(kRootedVectorPrefix, 0)") == 0, "prefix checks are not family compares")
    expect(count("a == kRootedArray || b == kRootedString") == 2, "several compares on a line all count")

    if failures:
        print("collection family compare self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("collection family compare self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
