#!/usr/bin/env python3
"""Check that include/primec/ir/IrOpcodeTable.h has one row per IrOpcode (TODO-5361).

Fails when an enumerator of `enum class IrOpcode` in Ir.h has no row, a row has
no enumerator, or the rows are not in enum order.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def enum_opcodes(ir_header: str) -> list[str]:
    match = re.search(r"enum class IrOpcode : uint8_t \{(.*?)\n\};", ir_header, re.S)
    if not match:
        raise ValueError("enum class IrOpcode not found")
    return re.findall(r"^\s+(\w+)(?: = \d+)?,", match.group(1), re.M)


def table_opcodes(table_header: str) -> list[str]:
    return re.findall(r"^\s+X\((\w+), [01], [01], [01]\)", table_header, re.M)


def check(enum_names: list[str], table_names: list[str]) -> list[str]:
    problems = []
    for name in enum_names:
        if name not in table_names:
            problems.append(f"IrOpcode::{name} has no row in IrOpcodeTable.h")
    for name in table_names:
        if name not in enum_names:
            problems.append(f"table row {name} has no IrOpcode enumerator")
    if not problems and enum_names != table_names:
        problems.append("table rows are not in IrOpcode enum order")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    problems = check(
        enum_opcodes((root / "include/primec/ir/Ir.h").read_text(encoding="utf-8")),
        table_opcodes((root / "include/primec/ir/IrOpcodeTable.h").read_text(encoding="utf-8")),
    )
    if problems:
        print("IR opcode table check failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("IR opcode table check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
