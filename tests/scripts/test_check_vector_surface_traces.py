#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the zero-tolerance vector surface trace checker."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="PrimeStruct repository root",
    )
    return parser.parse_args()


def run_checker(repo_root: Path, scanned_root: Path) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_vector_surface_traces.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root)],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    with tempfile.TemporaryDirectory(prefix="ps_vector_trace_clean_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        (scanned_root / "include" / "AllowedStdVector.h").write_text(
            "#include <vector>\nstd::vector<int> values;\n",
            encoding="utf-8",
        )
        clean = run_checker(repo_root, scanned_root)
        if clean.returncode != 0:
            print(clean.stdout, end="")
            print(clean.stderr, end="", file=sys.stderr)
            return 1
        if "0 production traces observed" not in clean.stdout:
            print(f"Unexpected clean checker output: {clean.stdout!r}", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="ps_vector_trace_bad_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        (scanned_root / "src" / "BadTrace.cpp").write_text(
            'const char *path = "/std/collections/vector/count";\n',
            encoding="utf-8",
        )
        failed = run_checker(repo_root, scanned_root)
        if failed.returncode == 0:
            print("Checker accepted a production vector trace", file=sys.stderr)
            print(failed.stdout, end="")
            return 1
        if "Vector surface trace check failed" not in failed.stderr:
            print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern canonical-vector-path has 1 traces" not in failed.stderr:
            print(f"Unexpected trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
