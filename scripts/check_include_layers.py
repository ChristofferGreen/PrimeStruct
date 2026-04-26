#!/usr/bin/env python3

from __future__ import annotations

import argparse
import posixpath
import re
import sys
from pathlib import Path


INCLUDE_RE = re.compile(r'^\s*#include\s*[<"]([^">]+)[">]')
SCANNED_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check PrimeStruct include-layer guardrails.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1], help="repository root")
    parser.add_argument(
        "--allowlist",
        type=Path,
        default=Path(__file__).resolve().with_name("include_layer_allowlist.txt"),
        help="allowlist for private include-layer dependencies",
    )
    return parser.parse_args()


def normalize_path(path: Path) -> str:
    return path.as_posix()


def load_allowlist(path: Path) -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []
    for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if " -> " not in line:
            raise ValueError(f"{path}:{lineno}: expected '<source> -> <target>'")
        source, target = (part.strip() for part in line.split(" -> ", 1))
        entries.append((source, target))
    return entries


def iter_includes(root: Path):
    scan_roots = [root / "include", root / "src", root / "tests"]
    for scan_root in scan_roots:
        if not scan_root.exists():
            continue
        for path in sorted(scan_root.rglob("*")):
            if path.suffix not in SCANNED_SUFFIXES or not path.is_file():
                continue
            rel_path = normalize_path(path.relative_to(root))
            for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                match = INCLUDE_RE.match(line)
                if match:
                    yield rel_path, lineno, match.group(1)


def matches_allowlist(include_path: str, allow_target: str) -> bool:
    if allow_target.endswith("/"):
        return include_path.startswith(allow_target)
    return include_path == allow_target


def resolve_repo_include_path(rel_path: str, include_path: str) -> str:
    if include_path.startswith("."):
        return posixpath.normpath(posixpath.join(posixpath.dirname(rel_path), include_path))
    if include_path.startswith("semantics/"):
        return posixpath.normpath(posixpath.join("src", include_path))
    return include_path


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    allowlist_path = args.allowlist.resolve()
    allowlist = load_allowlist(allowlist_path)
    used_allowlist: set[tuple[str, str]] = set()
    violations: list[str] = []

    for rel_path, lineno, include_path in iter_includes(root):
        repo_include_path = resolve_repo_include_path(rel_path, include_path)
        if rel_path.startswith("include/primec/"):
            if repo_include_path.startswith("src/"):
                violations.append(
                    f"{rel_path}:{lineno}: public headers must not include private src headers: {include_path}"
                )
            if repo_include_path.startswith("tests/"):
                violations.append(
                    f"{rel_path}:{lineno}: production headers must not include test headers: {include_path}"
                )
            continue

        if rel_path.startswith("src/"):
            if repo_include_path.startswith("tests/"):
                violations.append(
                    f"{rel_path}:{lineno}: production sources must not include test headers: {include_path}"
                )
            if rel_path.startswith("src/ir_lowerer/") and repo_include_path.startswith("src/semantics/"):
                matched = False
                for entry in allowlist:
                    allow_source, allow_target = entry
                    if rel_path == allow_source and matches_allowlist(repo_include_path, allow_target):
                        used_allowlist.add(entry)
                        matched = True
                        break
                if not matched:
                    violations.append(
                        f"{rel_path}:{lineno}: lowerer sources must not include private semantics headers: "
                        f"{include_path}"
                    )
            continue

        if rel_path.startswith("tests/") and repo_include_path.startswith("src/"):
            matched = False
            for entry in allowlist:
                allow_source, allow_target = entry
                if rel_path == allow_source and matches_allowlist(repo_include_path, allow_target):
                    used_allowlist.add(entry)
                    matched = True
                    break
            if not matched:
                violations.append(
                    f"{rel_path}:{lineno}: direct tests -> src include is not allowlisted: {include_path}"
                )

    stale_entries = [entry for entry in allowlist if entry not in used_allowlist]
    for source, target in stale_entries:
        violations.append(
            f"{normalize_path(allowlist_path.relative_to(root))}: stale allowlist entry should be removed: "
            f"{source} -> {target}"
        )

    if violations:
        print("Include-layer check failed:", file=sys.stderr)
        for violation in violations:
            print(f"  - {violation}", file=sys.stderr)
        return 1

    print(
        "Include-layer check passed. "
        f"{len(used_allowlist)} allowlisted private include-layer dependencies remain."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
