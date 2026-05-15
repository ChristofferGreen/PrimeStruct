#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-test the SoA surface trace inventory checker."
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
    inventory: Path,
) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_soa_surface_trace_inventory.py"
    return subprocess.run(
        [
            sys.executable,
            str(checker),
            "--root",
            str(scanned_root),
            "--inventory",
            str(inventory),
        ],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()

    with tempfile.TemporaryDirectory(prefix="ps_soa_surface_clean_") as temp:
        scanned_root = Path(temp)
        inventory = scanned_root / "inventory.txt"
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        inventory.write_text("", encoding="utf-8")
        (scanned_root / "include" / "AllowedSubstrate.h").write_text(
            "\n".join(
                [
                    "struct SoaColumn {};",
                    "struct SoaFieldView {};",
                    "struct SoaSchema {};",
                    "void borrowSoaFieldView();",
                    "void invalidateSoaFieldView();",
                ]
            ),
            encoding="utf-8",
        )
        clean = run_checker(repo_root, scanned_root, inventory)
        if clean.returncode != 0:
            print(clean.stdout, end="")
            print(clean.stderr, end="", file=sys.stderr)
            return 1
        if "0 production traces across 0 files" not in clean.stdout:
            print(f"Unexpected clean checker output: {clean.stdout!r}", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="ps_soa_surface_allowed_") as temp:
        scanned_root = Path(temp)
        inventory = scanned_root / "inventory.txt"
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        inventory.write_text(
            "src/AllowedTrace.cpp\tcanonical-soa-path\t1\n",
            encoding="utf-8",
        )
        (scanned_root / "src" / "AllowedTrace.cpp").write_text(
            'const char *path = "/std/collections/soa/count";\n',
            encoding="utf-8",
        )
        allowed = run_checker(repo_root, scanned_root, inventory)
        if allowed.returncode != 0:
            print(allowed.stdout, end="")
            print(allowed.stderr, end="", file=sys.stderr)
            return 1
        if "1 production traces across 1 files" not in allowed.stdout:
            print(
                f"Unexpected allowed checker output: {allowed.stdout!r}",
                file=sys.stderr,
            )
            return 1

    with tempfile.TemporaryDirectory(prefix="ps_soa_surface_bad_") as temp:
        scanned_root = Path(temp)
        inventory = scanned_root / "inventory.txt"
        (scanned_root / "include").mkdir()
        (scanned_root / "src").mkdir()
        inventory.write_text(
            "src/BadTrace.cpp\tcanonical-soa-path\t1\n",
            encoding="utf-8",
        )
        (scanned_root / "src" / "BadTrace.cpp").write_text(
            "\n".join(
                [
                    'const char *path = "/std/collections/soa/count";',
                    'const char *other = "/std/collections/soa/get";',
                    "const char *legacy = \"soaVectorCount\";",
                    "const char *convert = \"to_aos\";",
                    "using Old = SoaVector<int>;",
                ]
            ),
            encoding="utf-8",
        )
        failed = run_checker(repo_root, scanned_root, inventory)
        if failed.returncode == 0:
            print("Checker accepted increased SoA public surface traces", file=sys.stderr)
            print(failed.stdout, end="")
            return 1
        if "SoA surface trace inventory check failed" not in failed.stderr:
            print(f"Unexpected failing checker stderr: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern canonical-soa-path has 2 traces" not in failed.stderr:
            print(f"Unexpected canonical trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern soa-vector-helper-symbol has 1 traces" not in failed.stderr:
            print(f"Unexpected helper trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern soa-vector-type-symbol has 1 traces" not in failed.stderr:
            print(f"Unexpected type trace diagnostic: {failed.stderr!r}", file=sys.stderr)
            return 1
        if "src/BadTrace.cpp: pattern soa-conversion-helper has 1 traces" not in failed.stderr:
            print(
                f"Unexpected conversion trace diagnostic: {failed.stderr!r}",
                file=sys.stderr,
            )
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
