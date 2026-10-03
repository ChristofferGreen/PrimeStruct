#!/usr/bin/env python3
"""Self-test for scripts/check_format.py (TODO-5409)."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, write  # noqa: E402


def git(root: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)


def run(repo: Path, root: Path) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(repo / "scripts" / "check_format.py"), "--root", str(root), "--base", "master"],
                          capture_output=True, text=True, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("check_format")
    if shutil.which("clang-format") is None:
        print("check_format self-test skipped (clang-format not installed)")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        git(root, "init", "-q", "-b", "master")
        git(root, "config", "user.email", "t@example.com")
        git(root, "config", "user.name", "t")
        write(root / ".clang-format", (repo / ".clang-format").read_text(encoding="utf-8"))
        write(root / "a.cpp", "int  badly_formatted_old_line( int a ){return a;}\nint ok() {\n  return 1;\n}\n")
        git(root, "add", "-A")
        git(root, "commit", "-q", "-m", "base")
        result = run(repo, root)
        checks.expect(result.returncode == 0, f"an unchanged tree passes: {result.stdout}{result.stderr}")
        write(root / "a.cpp", "int  badly_formatted_old_line( int a ){return a;}\nint ok() {\n  return 2;\n}\n")
        result = run(repo, root)
        checks.expect(result.returncode == 0, "untouched badly formatted lines are not reported")
        write(root / "a.cpp", "int  badly_formatted_old_line( int a ){return a;}\nint ok() {\n  return    2;\n}\n")
        result = run(repo, root)
        checks.expect(result.returncode == 1 and "a.cpp:3" in result.stdout, f"a touched badly formatted line fails: {result.stdout}")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
