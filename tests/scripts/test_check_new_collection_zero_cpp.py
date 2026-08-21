#!/usr/bin/env python3
"""Self-test for scripts/check_new_collection_zero_cpp.py.

Uses two real commit ranges from this repository's own history as fixed
proof cases (see the checker script's docstring for why the allowed
path set includes CMakeLists.txt/cmake/** alongside stdlib/tests/docs):

  - Positive case: the TODO-4702 commit range (2111c8b..0db6025), which
    added stdlib/std/collections/ring_buffer.prime as a second toy
    collection type with zero src/**/include/** changes. The gate must
    pass against it.
  - Negative case: the TODO-4701 commit range (7bf0886..2111c8b), which
    touched src/** and include/** (compiler source). The gate must
    reject it.

Per TODO-4703's stop_rule, this is a proof-case self-test, not a
general-purpose fuzz/property test of the checker.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


# Fixed commit range proving TODO-4702 stayed zero-C++ (stdlib/tests/docs
# plus the CMakeLists.txt/cmake/** build-registration files this repo's
# test setup requires for a new compile_run test source).
POSITIVE_BEFORE_REF = "2111c8b"
POSITIVE_AFTER_REF = "0db6025"

# Fixed commit range (TODO-4701) known to touch src/** and include/**,
# used to prove the gate actually rejects compiler-source changes.
NEGATIVE_BEFORE_REF = "7bf0886"
NEGATIVE_AFTER_REF = "2111c8b"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the new-collection zero-C++ gate checker."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="PrimeStruct repository root",
    )
    return parser.parse_args()


def run_checker(
    repo_root: Path,
    before_ref: str,
    after_ref: str,
) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_new_collection_zero_cpp.py"
    return subprocess.run(
        [
            sys.executable,
            str(checker),
            before_ref,
            after_ref,
            "--root",
            str(repo_root),
        ],
        text=True,
        capture_output=True,
        check=False,
    )


def ref_exists(repo_root: Path, ref: str) -> bool:
    result = subprocess.run(
        ["git", "cat-file", "-e", ref],
        cwd=repo_root,
        capture_output=True,
        check=False,
    )
    return result.returncode == 0


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    for ref in (
        POSITIVE_BEFORE_REF,
        POSITIVE_AFTER_REF,
        NEGATIVE_BEFORE_REF,
        NEGATIVE_AFTER_REF,
    ):
        if not ref_exists(repo_root, ref):
            print(
                f"Skipping self-test: ref {ref!r} not present in this "
                "checkout's git history.",
            )
            return 0

    passed = run_checker(repo_root, POSITIVE_BEFORE_REF, POSITIVE_AFTER_REF)
    if passed.returncode != 0:
        print(
            "Expected the TODO-4702 commit range to pass the zero-C++ gate",
            file=sys.stderr,
        )
        print(passed.stdout, end="")
        print(passed.stderr, end="", file=sys.stderr)
        return 1
    if "gate passed" not in passed.stdout:
        print(f"Unexpected passing checker output: {passed.stdout!r}", file=sys.stderr)
        return 1

    failed = run_checker(repo_root, NEGATIVE_BEFORE_REF, NEGATIVE_AFTER_REF)
    if failed.returncode == 0:
        print(
            "Expected the TODO-4701 commit range (touches src/**, "
            "include/**) to fail the zero-C++ gate",
            file=sys.stderr,
        )
        print(failed.stdout, end="")
        return 1
    if "gate failed" not in failed.stderr:
        print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
        return 1
    for expected_path in (
        "include/primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h",
        "src/ir_lowerer/IrLowererLegacyCollectionBranchCounters.cpp",
    ):
        if expected_path not in failed.stderr:
            print(
                f"Missing expected disallowed-path diagnostic {expected_path!r}: "
                f"{failed.stderr!r}",
                file=sys.stderr,
            )
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
