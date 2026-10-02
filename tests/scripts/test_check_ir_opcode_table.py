#!/usr/bin/env python3
"""Self-test for scripts/check_ir_opcode_table.py (TODO-5361)."""

from __future__ import annotations

import argparse
import importlib.util
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_ir_opcode_table", repo / "scripts" / "check_ir_opcode_table.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures = []

    def expect(condition, message):
        if not condition:
            failures.append(message)

    expect(module.check(["A", "B"], ["A", "B"]) == [], "matching lists pass")
    expect(module.check(["A", "B", "C"], ["A", "B"]) != [], "an opcode without a row fails")
    expect(module.check(["A"], ["A", "Z"]) != [], "a row without an opcode fails")
    expect(module.check(["A", "B"], ["B", "A"]) != [], "a reordered table fails")
    ir = (repo / "include/primec/ir/Ir.h").read_text(encoding="utf-8")
    table = (repo / "include/primec/ir/IrOpcodeTable.h").read_text(encoding="utf-8")
    expect(module.check(module.enum_opcodes(ir), module.table_opcodes(table)) == [], "the repository table matches the enum")
    # Negative test on the real header: add an enumerator without a row.
    extended = re.sub(r"(enum class IrOpcode : uint8_t \{.*?)\n\};", r"\1\n  BrandNewOpcode,\n};", ir, count=1, flags=re.S)
    expect(extended != ir, "test setup: enumerator inserted")
    expect(module.check(module.enum_opcodes(extended), module.table_opcodes(table)) != [], "a new opcode without a row fails")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_ir_opcode_table self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
