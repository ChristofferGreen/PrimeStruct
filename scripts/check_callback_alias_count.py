#!/usr/bin/env python3
"""Ratchet on distinct `using Name = std::function<...>` alias names.

Callback signatures are shared through the canonical aliases in
include/primec/support/CallbackTypes.h and src/ir_lowerer/IrLowererSharedTypes.h.
Adding a new bespoke alias for an already-named signature fails; the baseline
only ever shrinks.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

BASELINE_ALIAS_NAMES = 114
ALIAS_RE = re.compile(r"^\s*using\s+(\w+)\s*=\s*std::function<(.*)>\s*;\s*$")
CANONICAL = {
    "bool(const Expr &)": "ExprPredicateFn",
    "std::string(const Expr &)": "ExprStringFn",
    "int32_t()": "Int32ProviderFn",
    "bool(const Expr &, const LocalMap &)": "ExprLocalsPredicateFn",
    "void(const Expr &, LocalInfo &)": "ExprLocalInfoVisitorFn",
    "LocalInfo::ValueKind(const Expr &, const LocalMap &)": "ExprLocalsValueKindFn",
    "size_t()": "SizeProviderFn",
    "void()": "ActionFn",
    "void(IrOpcode, uint64_t)": "EmitInstructionFn",
    "void(size_t, uint64_t)": "PatchInstructionImmFn",
}


def collect_aliases(root: Path) -> dict[str, str]:
    aliases: dict[str, str] = {}
    for base in ("src", "include", "tests"):
        for path in sorted((root / base).rglob("*")):
            if path.suffix not in (".cpp", ".h"):
                continue
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                match = ALIAS_RE.match(line)
                if match:
                    aliases[match.group(1)] = match.group(2)
    return aliases


def find_problems(aliases: dict[str, str], baseline: int = BASELINE_ALIAS_NAMES) -> list[str]:
    problems = []
    canonical_names = set(CANONICAL.values())
    for name, signature in sorted(aliases.items()):
        canonical = CANONICAL.get(signature)
        if canonical and name != canonical and name not in canonical_names:
            problems.append(f"{name} duplicates canonical {canonical} (std::function<{signature}>)")
    if len(aliases) > baseline:
        problems.append(f"{len(aliases)} distinct std::function alias names exceed the baseline of {baseline}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    aliases = collect_aliases(parser.parse_args().root.resolve())
    problems = find_problems(aliases)
    if problems:
        print("Callback alias audit failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"Callback alias audit passed: {len(aliases)} alias names (baseline {BASELINE_ALIAS_NAMES}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
