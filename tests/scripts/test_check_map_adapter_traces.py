#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the retired map adapter trace checker."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="PrimeStruct repository root",
    )
    return parser.parse_args()


def run_checker(repo_root: Path, scanned_root: Path) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_map_adapter_traces.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root)],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    with tempfile.TemporaryDirectory(prefix="ps_map_adapter_clean_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "src" / "emitter").mkdir(parents=True)
        (scanned_root / "src" / "ir_lowerer").mkdir(parents=True)
        (scanned_root / "src" / "emitter" / "AllowedStdMap.cpp").write_text(
            "#include <map>\nstd::map<int, int> values;\n",
            encoding="utf-8",
        )
        clean = run_checker(repo_root, scanned_root)
        if clean.returncode != 0:
            print(clean.stdout, end="")
            print(clean.stderr, end="", file=sys.stderr)
            return 1
        if "0 retired lowerer/emitter traces observed" not in clean.stdout:
            print(f"Unexpected clean checker output: {clean.stdout!r}", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="ps_map_adapter_bad_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "src" / "ir_lowerer").mkdir(parents=True)
        (scanned_root / "src" / "ir_lowerer" / "BadTrace.cpp").write_text(
            'const char *helper = "mapTryAt";\n',
            encoding="utf-8",
        )
        failed = run_checker(repo_root, scanned_root)
        if failed.returncode == 0:
            print("Checker accepted a retired map adapter trace", file=sys.stderr)
            print(failed.stdout, end="")
            return 1
        if "Map adapter trace check failed" not in failed.stderr:
            print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/ir_lowerer/BadTrace.cpp: pattern map-wrapper-helper-symbol has 1 traces" not in failed.stderr:
            print(f"Unexpected trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
