#!/usr/bin/env python3
"""Assert a git commit range adding a new stdlib collection type stays
"zero C++" in the sense this repository can actually deliver today.

Usage:
    python3 scripts/check_new_collection_zero_cpp.py <before-ref> <after-ref>

Runs `git diff --name-only <before-ref> <after-ref>` and fails (non-zero
exit, disallowed paths listed on stderr) unless every changed path falls
under one of the allowed path prefixes below.

Why the allowed set is broader than "stdlib/**, tests/**, docs/**"
--------------------------------------------------------------------
TODO-4703's scope text names stdlib/**, tests/**, docs/** as the allowed
paths, on the premise that adding a purely-stdlib collection type (see
TODO-4702, "Add RingBuffer<T> as a second toy collection type, zero
C++") touches no compiler source. That premise holds for src/** and
include/** (TODO-4702's diff changed neither), but this codebase has no
test-file auto-discovery: registering a new compile_run test source
requires adding it to CMakeLists.txt, and the existing doctest smoke
shards in cmake/PrimeStructManagedCompileRunSmokeSuites.cmake match
source files by an explicit per-file SOURCE_FILE glob, so a new test
file needs a new shard block there too. TODO-4702's own diff touched
exactly those two build-config files alongside its stdlib/tests/docs
changes (see its progress_2026-08-21 note in docs/todo.md), and that is
a genuine, unavoidable requirement of "add a new compile_run test" in
the current build, not a violation of the zero-C++ property this gate
exists to protect.

So this gate's allowed set is:
    stdlib/**       - the new collection type's source
    tests/**        - its compile_run tests
    docs/**         - documentation / TODO bookkeeping
    CMakeLists.txt  - top-level test/target registration
    cmake/**        - shard/suite registration (e.g. the smoke-suite
                      SOURCE_FILE glob config)

It deliberately does NOT include src/** or include/** - those are the
actual compiler/VM source trees, and a diff touching either of them is
exactly what this gate is meant to catch and reject. Keeping the
allowed set no wider than "stdlib/tests/docs plus the specific build
registration files this repo's test setup requires" preserves the
spirit of "zero C++ implementation changes" while matching what a
passing real-world proof case (TODO-4702) actually looks like.

This script is a one-off proof-case gate per TODO-4703's stop_rule, not
a general-purpose always-on CI gate; it is invoked directly with an
explicit ref pair, not run automatically against arbitrary revisions.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ALLOWED_PATH_PREFIXES = (
    "stdlib/",
    "tests/",
    "docs/",
    "cmake/",
)

ALLOWED_EXACT_PATHS = (
    "CMakeLists.txt",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Assert that a git commit range adding a new stdlib collection "
            "type touches only stdlib/**, tests/**, docs/**, cmake/**, and "
            "CMakeLists.txt (never src/** or include/**)."
        )
    )
    parser.add_argument(
        "before_ref",
        help="git ref (commit/tag/branch) at the start of the range, exclusive",
    )
    parser.add_argument(
        "after_ref",
        help="git ref (commit/tag/branch) at the end of the range, inclusive",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    return parser.parse_args()


def changed_paths(root: Path, before_ref: str, after_ref: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", before_ref, after_ref],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(
            f"git diff failed (exit {result.returncode}): {result.stderr.strip()}"
        )
    return [line for line in result.stdout.splitlines() if line]


def is_allowed(path: str) -> bool:
    if path in ALLOWED_EXACT_PATHS:
        return True
    return any(path.startswith(prefix) for prefix in ALLOWED_PATH_PREFIXES)


def main() -> int:
    args = parse_args()
    root = args.root.resolve()

    paths = changed_paths(root, args.before_ref, args.after_ref)
    if not paths:
        print(
            f"No changed paths between {args.before_ref} and {args.after_ref}."
        )
        return 0

    disallowed = sorted(path for path in paths if not is_allowed(path))

    if disallowed:
        print(
            "New-collection zero-C++ gate failed: paths outside the allowed "
            f"set changed between {args.before_ref} and {args.after_ref}:",
            file=sys.stderr,
        )
        for path in disallowed:
            print(f"  - {path}", file=sys.stderr)
        return 1

    print(
        "New-collection zero-C++ gate passed: "
        f"{len(paths)} changed path(s) between {args.before_ref} and "
        f"{args.after_ref} all fall within the allowed set."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
