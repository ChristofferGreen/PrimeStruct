#!/usr/bin/env python3
"""Every VM string lookup goes through resolveVmString (TODO-5364).

src/runtime must not index or size `stringTable` directly; only
src/runtime/VmStringHeap.cpp (the helper itself) may.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ALLOWED = {"src/runtime/VmStringHeap.cpp"}
PATTERN = re.compile(r"\bstringTable\b")


def find_violations(files: dict[str, str]) -> list[str]:
    problems = []
    for path, text in sorted(files.items()):
        if path in ALLOWED:
            continue
        for number, line in enumerate(text.splitlines(), 1):
            code = line.split("//", 1)[0]
            if PATTERN.search(code):
                problems.append(f"{path}:{number}: direct stringTable access; use resolveVmString")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    files = {
        p.relative_to(root).as_posix(): p.read_text(encoding="utf-8", errors="replace")
        for p in sorted((root / "src" / "runtime").rglob("*"))
        if p.suffix in {".cpp", ".h"}
    }
    problems = find_violations(files)
    if problems:
        print("VM string lookup check failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("VM string lookup check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
