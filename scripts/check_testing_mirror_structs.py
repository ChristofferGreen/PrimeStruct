#!/usr/bin/env python3
"""Guard against header mirrors between src/ir_lowerer and the public include tree.

The lowerer helper headers used to exist twice (src/ir_lowerer/X.h and a hand-kept
copy under include/primec/testing/ir_lowerer_helpers/X.h) and an ODR bug came from the
copies drifting apart. The real headers now live once in include/primec/ir_lowerer/;
this check fails if a header basename ever appears again in two of the trees
src/ir_lowerer, include/primec/ir_lowerer and include/primec/testing.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

TREES = ("src/ir_lowerer", "include/primec/ir_lowerer", "include/primec/testing")


def find_duplicates(root: Path) -> list[str]:
    seen: dict[str, str] = {}
    problems: list[str] = []
    for tree in TREES:
        for path in sorted((root / tree).rglob("*.h")):
            name = path.name
            rel = path.relative_to(root).as_posix()
            if name in seen:
                problems.append(f"header basename {name} exists twice: {seen[name]} and {rel}")
            else:
                seen[name] = rel
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    problems = find_duplicates(parser.parse_args().root.resolve())
    if problems:
        print("duplicated lowerer headers:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("check_testing_mirror_structs: no duplicated lowerer header basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
