#!/usr/bin/env python3
"""Self-test for scripts/check_no_direct_stdio.py (TODO-5407)."""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_no_direct_stdio", repo / "scripts" / "check_no_direct_stdio.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for sub in ("src/bin", "src/backend", "src/semantics", "include"):
            (root / sub).mkdir(parents=True)
        (root / "src/bin/main.cpp").write_text("std::cerr << 1;\n", encoding="utf-8")
        (root / "src/backend/IrToCppEmitter.cpp").write_text('"std::cout"\n', encoding="utf-8")
        (root / "src/semantics/ok.cpp").write_text("int x;\n", encoding="utf-8")
        if module.find_violations(root):
            failures.append("bin, allowlisted and clean files pass")
        (root / "src/semantics/bad.cpp").write_text("void f() {\n  std::cerr << 1;\n}\n", encoding="utf-8")
        (root / "include/bad.h").write_text("auto &o = std::cout;\n", encoding="utf-8")
        problems = module.find_violations(root)
        if len(problems) != 2 or "bad.cpp:2" not in problems[0] and "bad.cpp:2" not in "".join(problems):
            failures.append(f"direct uses are reported with line numbers: {problems}")

    if failures:
        print("direct stdio self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("direct stdio self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
