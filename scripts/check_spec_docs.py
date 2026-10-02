#!/usr/bin/env python3
"""Check the split language specification (TODO-5368).

docs/PrimeStruct.md is the index of docs/spec/*.md. This checks that

  * every docs/spec/*.md part is listed in the index table with a known
    classification, and every table row points at an existing part;
  * every part carries its breadcrumb back to the index;
  * relative markdown links in the index and the parts resolve to existing
    files, and `#anchor` fragments match a heading of the target file.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

INDEX = "docs/PrimeStruct.md"
SPEC_DIR = "docs/spec"
CLASSIFICATIONS = {
    "normative",
    "normative (draft)",
    "design direction",
    "implementation note",
    "implementation note / roadmap",
    "examples",
    "roadmap / history",
}
LINK_RE = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
ROW_RE = re.compile(r"^\|\s*\[[^\]]+\]\(spec/([\w.-]+\.md)\)\s*\|\s*([^|]+?)\s*\|\s*\d+\s*\|\s*$")


def slug(heading: str) -> str:
    text = re.sub(r"[`*_]", "", heading.strip().lower())
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def anchors(text: str) -> set[str]:
    found: set[str] = set()
    counts: dict[str, int] = {}
    in_fence = False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        match = HEADING_RE.match(line)
        if match:
            base = slug(match.group(2))
            n = counts.get(base, 0)
            counts[base] = n + 1
            found.add(base if n == 0 else f"{base}-{n}")
    return found


def index_rows(index_text: str) -> dict[str, str]:
    rows: dict[str, str] = {}
    for line in index_text.splitlines():
        match = ROW_RE.match(line)
        if match:
            rows[match.group(1)] = match.group(2)
    return rows


def check_links(path: Path, text: str, root: Path, anchor_cache: dict[Path, set[str]]) -> list[str]:
    problems: list[str] = []
    in_fence = False
    for number, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for target in LINK_RE.findall(line):
            if re.match(r"^[a-z][a-z0-9+.-]*:", target):
                continue
            file_part, _, fragment = target.partition("#")
            resolved = path if not file_part else (path.parent / file_part).resolve()
            where = f"{path.relative_to(root).as_posix()}:{number}"
            if not resolved.exists():
                problems.append(f"{where}: link target does not exist: {target}")
                continue
            if fragment and resolved.suffix == ".md":
                if resolved not in anchor_cache:
                    anchor_cache[resolved] = anchors(resolved.read_text(encoding="utf-8"))
                if fragment not in anchor_cache[resolved]:
                    problems.append(f"{where}: no heading for anchor: {target}")
    return problems


def find_problems(root: Path) -> list[str]:
    problems: list[str] = []
    index_path = root / INDEX
    index_text = index_path.read_text(encoding="utf-8")
    rows = index_rows(index_text)
    parts = {p.name: p for p in sorted((root / SPEC_DIR).glob("*.md"))}
    for name in sorted(set(parts) - set(rows)):
        problems.append(f"{SPEC_DIR}/{name}: not listed in the {INDEX} index table")
    for name in sorted(set(rows) - set(parts)):
        problems.append(f"{INDEX}: index row points at missing part spec/{name}")
    for name, classification in sorted(rows.items()):
        if classification not in CLASSIFICATIONS:
            problems.append(f"{INDEX}: unknown classification '{classification}' for spec/{name}")
    anchor_cache: dict[Path, set[str]] = {}
    for name, path in parts.items():
        text = path.read_text(encoding="utf-8")
        if "(../PrimeStruct.md)" not in text.split("\n", 4)[2]:
            problems.append(f"{SPEC_DIR}/{name}: missing the breadcrumb line back to the index")
        problems += check_links(path, text, root, anchor_cache)
    problems += check_links(index_path, index_text, root, anchor_cache)
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    problems = find_problems(root)
    if problems:
        print("spec docs check failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"spec docs check passed ({len(list((root / SPEC_DIR).glob('*.md')))} parts)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
