#!/usr/bin/env python3
"""Self-test for scripts/check_callback_alias_count.py."""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, write  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_callback_alias_count", repo / "scripts" / "check_callback_alias_count.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    checks = Checks("callback alias count")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write(root / "src/a.h", "using ExprPredicateFn = std::function<bool(const Expr &)>;\n"
                                "using OtherFn = std::function<void(int)>;\n")
        write(root / "include/b.h", "  using LocalFn = std::function<void(int)>;\n")
        write(root / "tests/c.cpp", "int x;\n")
        aliases = module.collect_aliases(root)
        checks.expect(set(aliases) == {"ExprPredicateFn", "OtherFn", "LocalFn"}, "aliases are collected from src, include, tests")
        checks.expect(module.find_problems(aliases, 3) == [], "the canonical alias and distinct signatures pass")
        checks.expect(module.find_problems(aliases, 2) != [], "growth past the baseline fails")
        aliases["IsThingFn"] = "bool(const Expr &)"
        problems = module.find_problems(aliases, 10)
        checks.expect(len(problems) == 1 and "IsThingFn duplicates canonical ExprPredicateFn" in problems[0],
                      "a bespoke alias for a canonical signature fails")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
