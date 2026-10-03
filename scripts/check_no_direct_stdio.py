#!/usr/bin/env python3
"""Fail when production sources write to std::cerr / std::cout (TODO-5407).

Instrumentation goes through primec::support::emitBenchmarkLine; src/bin
(the CLI front ends) may print. The IrToCpp emitters generate C++ text that
mentions std::cerr/std::cout, so they are allowlisted.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ALLOWLIST = {
    "src/backend/IrToCppEmitter.cpp",
    "src/backend/IrToCppEmitterInstructionEmitter.cpp",
    "src/backend/IrToCppEmitterPrintAndFileEmitter.cpp",
    "src/support/BenchmarkSink.cpp",
}
PATTERN = re.compile(r"\bstd::(cerr|cout)\b")


def find_violations(root: Path) -> list[str]:
    problems: list[str] = []
    for base in ("src", "include"):
        for path in sorted((root / base).rglob("*")):
            if path.suffix not in (".cpp", ".h"):
                continue
            rel = path.relative_to(root).as_posix()
            if rel.startswith("src/bin/") or rel in ALLOWLIST:
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if PATTERN.search(line):
                    problems.append(f"{rel}:{number}: direct {PATTERN.search(line).group(0)} use")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    problems = find_violations(parser.parse_args().root.resolve())
    if problems:
        print("Direct stdio audit failed (use primec::support::emitBenchmarkLine):")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("Direct stdio audit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
