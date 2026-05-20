#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the map surface strict audit checker."
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
    scanned_root: Path,
    *extra_args: str,
) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_map_surface_strict_audit.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root), *extra_args],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    with tempfile.TemporaryDirectory(prefix="ps_map_surface_clean_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        (scanned_root / "include" / "AllowedStdMap.h").write_text(
            "#include <map>\nstd::map<int, int> values;\n",
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

    with tempfile.TemporaryDirectory(prefix="ps_map_surface_bad_") as temp:
        scanned_root = Path(temp)
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        (scanned_root / "src" / "BadTrace.cpp").write_text(
            'const char *path = "/std/collections/map/count";\n',
            encoding="utf-8",
        )
        failed = run_checker(repo_root, scanned_root)
        if failed.returncode == 0:
            print("Checker accepted a production map surface trace", file=sys.stderr)
            print(failed.stdout, end="")
            return 1
        if "Map surface strict audit failed" not in failed.stderr:
            print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern canonical-map-path has 1 traces" not in failed.stderr:
            print(f"Unexpected trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="ps_map_surface_current_") as temp:
        scanned_root = Path(temp)
        current_path = scanned_root / "src" / "ir_lowerer"
        (scanned_root / "include").mkdir()
        current_path.mkdir(parents=True)
        (current_path / "IrLowererLowerStatementsExpr.h").write_text(
            'auto helperName = bareName == "at" ? "mapAt" : "mapAtUnsafe";\n',
            encoding="utf-8",
        )
        allowed = run_checker(repo_root, scanned_root)
        if allowed.returncode != 0:
            print(allowed.stdout, end="")
            print(allowed.stderr, end="", file=sys.stderr)
            return 1
        if "2 production traces observed" not in allowed.stdout:
            print(f"Unexpected current allowance output: {allowed.stdout!r}", file=sys.stderr)
            return 1

        zero = run_checker(repo_root, scanned_root, "--enforce-zero")
        if zero.returncode == 0:
            print("Checker accepted current map bridge residue in zero mode", file=sys.stderr)
            print(zero.stdout, end="")
            return 1
        if "pattern map-helper-symbol has 2 traces" not in zero.stderr:
            print(f"Unexpected zero-mode diagnostic: {zero.stderr!r}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
