#!/usr/bin/env python3
"""Fixture tests for scripts/refactor/split_file.py and register_split.py (TODO-5418)."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True

SOURCE = """// soa-surface-audit: exempt
#include "Thing.h"

namespace primec::semantics {
namespace {

bool helperOne(int x) {
  return x > 0;
}

} // namespace

bool Thing::first(int x) {
  return helperOne(x);
}

bool Thing::second(int x) {
  return !helperOne(x);
}

} // namespace primec::semantics
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "src").mkdir()
        (root / "scripts").mkdir()
        src = root / "src" / "Thing.cpp"
        src.write_text(SOURCE, encoding="utf-8")
        second_line = SOURCE.split("\n").index("bool Thing::second(int x) {") + 1
        run = subprocess.run(
            [sys.executable, str(repo / "scripts" / "refactor" / "split_file.py"), "src/Thing.cpp", "ThingHelpers", "thingHelpers",
             f"Second:{second_line}"],
            cwd=root, capture_output=True, text=True)
        expect(run.returncode == 0, f"split_file runs: {run.stderr}")
        header = (root / "src" / "ThingHelpers.h").read_text(encoding="utf-8")
        first = src.read_text(encoding="utf-8")
        second = (root / "src" / "ThingSecond.cpp").read_text(encoding="utf-8")
        expect("inline bool helperOne(int x)" in header and "namespace thingHelpers" in header, "helpers move to the header as inline")
        expect("Thing::first" in first and "Thing::second" not in first, "the first part keeps the first member")
        expect("Thing::second" in second and "Thing::first" not in second, "the second part gets the second member")
        expect('#include "ThingHelpers.h"' in first and "using namespace thingHelpers;" in second, "parts include and use the helpers")

        (root / "CMakeLists.txt").write_text("add_library(x\n  src/Thing.cpp\n)\n", encoding="utf-8")
        (root / "scripts" / "source_file_size_allowlist.txt").write_text("src/Thing.cpp 1300 big\nsrc/Other.cpp 1300 big\n", encoding="utf-8")
        for _ in range(2):  # idempotent
            subprocess.run([sys.executable, str(repo / "scripts" / "refactor" / "register_split.py"), "src/Thing.cpp", "src/ThingSecond.cpp"],
                           cwd=root, check=True)
        cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
        allow = (root / "scripts" / "source_file_size_allowlist.txt").read_text(encoding="utf-8")
        expect(cmake.count("src/ThingSecond.cpp") == 1, "the new part is registered once")
        expect("src/Thing.cpp " not in allow and "src/Other.cpp" in allow, "only the split file leaves the allowlist")

    if failures:
        print("refactor tools self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("refactor tools self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
