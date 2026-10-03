#!/usr/bin/env python3
"""Finish a TODO block (TODO-5418): implements the AGENTS.md block-move flow.

    python3 scripts/finish_todo.py TODO-1234 '<result note>' [--date YYYY-MM-DD] [--root ROOT]

Moves the open task block from docs/todo.md to docs/todo_finished.md (status
done, finished_at, result), removes its Queue Summary row, Ready Now and
Immediate Next 10 lines, drops its id from Priority Lanes text, renumbers the
Immediate Next 10 list, then runs scripts/archive_todo_finished.py.
"""

from __future__ import annotations

import argparse
import datetime
import re
import subprocess
import sys
from pathlib import Path


def split_block(lines: list[str], tid: str) -> tuple[list[str], list[str]]:
    out: list[str] = []
    block: list[str] = []
    state = "scan"
    for line in lines:
        if state == "scan" and re.match(rf"^- \[[ ~]\] {re.escape(tid)}:", line):
            state = "block"
            block = [re.sub(r"^- \[[ ~]\]", "- [x]", line)]
            continue
        if state == "block":
            if line.startswith("  "):
                block.append(line)
                continue
            state = "done"
            if not line.strip():
                continue  # drop the blank line that ended the block
        out.append(line)
    if not block:
        raise SystemExit(f"no open block for {tid} in docs/todo.md")
    return out, block


def clean_queues(lines: list[str], tid: str) -> list[str]:
    out: list[str] = []
    section = ""
    for line in lines:
        if line.startswith("### "):
            section = line[4:].strip()
        if line.startswith(f"| {tid} |"):
            continue
        if section in {"Ready Now", "Immediate Next 10"} and re.search(rf"\b{re.escape(tid)}\b", line) and (
                line.startswith("- ") or re.match(r"\d+\. ", line)):
            continue
        if section == "Priority Lanes" and line.startswith("- ") and tid in line:
            line = re.sub(rf"(, |\s*->\s*){re.escape(tid)}|{re.escape(tid)}(, |\s*->\s*)?", "", line)
            line = re.sub(r"^(- [^:]+):\s*$", "", line)
            if not line.strip():
                continue
        out.append(line)
    # renumber Immediate Next 10
    result: list[str] = []
    section = ""
    n = 0
    for line in out:
        if line.startswith("### "):
            section = line[4:].strip()
            n = 0
        if section == "Immediate Next 10" and re.match(r"\d+\. ", line):
            n += 1
            line = re.sub(r"^\d+\. ", f"{n}. ", line)
        result.append(line)
    return result


def finish(root: Path, tid: str, note: str, date: str, run_archive: bool = True) -> None:
    todo_path = root / "docs" / "todo.md"
    finished_path = root / "docs" / "todo_finished.md"
    lines = todo_path.read_text(encoding="utf-8").split("\n")
    lines, block = split_block(lines, tid)
    lines = clean_queues(lines, tid)
    block = [re.sub(r"^  - status: \w+", "  - status: done", l) for l in block]
    block += [f"  - finished_at: {date}", f"  - result: {note}"]
    todo_path.write_text("\n".join(lines), encoding="utf-8")
    finished = finished_path.read_text(encoding="utf-8").rstrip("\n")
    finished_path.write_text(finished + "\n\n" + "\n".join(block) + "\n", encoding="utf-8")
    if run_archive:
        subprocess.run([sys.executable, "-B", str(root / "scripts" / "archive_todo_finished.py")], check=True, cwd=root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("todo_id")
    parser.add_argument("result")
    parser.add_argument("--date", default=datetime.date.today().isoformat())
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    finish(args.root.resolve(), args.todo_id, args.result, args.date)
    return 0


if __name__ == "__main__":
    sys.exit(main())
