#!/usr/bin/env python3
"""Self-test for scripts/check_collection_audit_exemption_count.py.

Two cases:
  - Real repo state: running the checker against this checkout's actual
    include/ and src/ trees must pass (the recorded baseline is the real
    measured count as of introduction, so the live count must be <= it).
  - Artificial fixture: a temporary include/src tree seeded with one more
    exempt file than the checker's hardcoded baseline allows must fail.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the collection audit exemption-count ratchet."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="PrimeStruct repository root",
    )
    return parser.parse_args()


def run_checker(repo_root: Path, scanned_root: Path) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_collection_audit_exemption_count.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root)],
        text=True,
        capture_output=True,
        check=False,
    )


def read_baseline(repo_root: Path) -> int:
    checker = repo_root / "scripts" / "check_collection_audit_exemption_count.py"
    text = checker.read_text(encoding="utf-8")
    match = re.search(r"^BASELINE_EXEMPT_FILE_COUNT = (\d+)$", text, re.MULTILINE)
    if not match:
        raise SystemExit("Could not find BASELINE_EXEMPT_FILE_COUNT in checker script")
    return int(match.group(1))


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    # Case 1: the real repo state must currently pass.
    real = run_checker(repo_root, repo_root)
    if real.returncode != 0:
        print(
            "Expected the real repository state to pass the exemption-count "
            "ratchet (baseline may need updating if this is an intentional "
            "increase)",
            file=sys.stderr,
        )
        print(real.stdout, end="")
        print(real.stderr, end="", file=sys.stderr)
        return 1
    if "ratchet passed" not in real.stdout:
        print(f"Unexpected passing checker output: {real.stdout!r}", file=sys.stderr)
        return 1

    baseline = read_baseline(repo_root)

    # Case 2: an artificial fixture with baseline + 1 exempt files must fail.
    with tempfile.TemporaryDirectory(prefix="ps_collection_exempt_count_") as temp:
        scanned_root = Path(temp)
        include_dir = scanned_root / "include"
        src_dir = scanned_root / "src"
        include_dir.mkdir()
        src_dir.mkdir()

        for index in range(baseline + 1):
            (src_dir / f"Exempt{index}.cpp").write_text(
                "\n".join(
                    [
                        "// collection-surface-audit: exempt",
                        "// (fixture file for exemption-count ratchet self-test)",
                        "",
                        "void placeholder() {}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )

        failed = run_checker(repo_root, scanned_root)
        if failed.returncode == 0:
            print(
                f"Expected {baseline + 1} exempt fixture files (baseline "
                f"{baseline}) to fail the ratchet",
                file=sys.stderr,
            )
            print(failed.stdout, end="")
            return 1
        if "exemption-count ratchet failed" not in failed.stderr:
            print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
            return 1
        if str(baseline + 1) not in failed.stderr:
            print(
                f"Expected failing count {baseline + 1} in stderr: {failed.stderr!r}",
                file=sys.stderr,
            )
            return 1

    # Case 3: exactly baseline exempt files must still pass.
    with tempfile.TemporaryDirectory(prefix="ps_collection_exempt_count_ok_") as temp:
        scanned_root = Path(temp)
        include_dir = scanned_root / "include"
        src_dir = scanned_root / "src"
        include_dir.mkdir()
        src_dir.mkdir()

        for index in range(baseline):
            (src_dir / f"Exempt{index}.cpp").write_text(
                "\n".join(
                    [
                        "// vector-surface-audit: exempt",
                        "",
                        "void placeholder() {}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )

        at_baseline = run_checker(repo_root, scanned_root)
        if at_baseline.returncode != 0:
            print(
                f"Expected exactly {baseline} exempt fixture files to pass "
                "the ratchet",
                file=sys.stderr,
            )
            print(at_baseline.stdout, end="")
            print(at_baseline.stderr, end="", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
