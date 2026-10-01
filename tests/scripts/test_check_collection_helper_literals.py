#!/usr/bin/env python3
"""Self-test for scripts/check_collection_helper_literals.py.

Cases: the real checkout passes; a hard-coded helper literal in production code
fails; the same text in a comment, in the constants header, or as a harmless
string passes.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def run(repo_root: Path, scanned_root: Path) -> subprocess.CompletedProcess[str]:
    checker = repo_root / "scripts" / "check_collection_helper_literals.py"
    return subprocess.run(
        [sys.executable, str(checker), "--root", str(scanned_root)],
        text=True,
        capture_output=True,
        check=False,
    )


def write(root: Path, rel: str, text: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo_root = parser.parse_args().repo_root.resolve()

    real = run(repo_root, repo_root)
    if real.returncode != 0:
        print("real checkout should pass:\n" + real.stderr, file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write(root, "src/semantics/ok.cpp", '// "/vector/count" in a comment\nint a = 1; // "count_ref"\nconst char *s = "count";\n')
        write(root, "include/primec/support/CollectionHelperNames.h", 'inline constexpr char kCountRef[] = "count_ref";\n')
        write(root, "src/support/StdlibSurfaceRegistry.cpp", 'const char *x = "/std/collections/vector/count";\n')
        ok = run(repo_root, root)
        if ok.returncode != 0:
            print("allowed text should pass:\n" + ok.stderr, file=sys.stderr)
            return 1

        for rel, text in (
            ("src/semantics/bad_ref.cpp", 'bool f(std::string n) { return n == "get_ref"; }\n'),
            ("src/ir_lowerer/bad_path.cpp", 'auto p = std::string("/array/count");\n'),
            ("src/semantics/bad_std.cpp", 'auto p = "/std/collections/soa/get";\n'),
        ):
            write(root, rel, text)
            bad = run(repo_root, root)
            if bad.returncode == 0 or rel not in bad.stderr:
                print(f"{rel} should fail the check:\n{bad.stdout}{bad.stderr}", file=sys.stderr)
                return 1
            (root / rel).unlink()
    print("check_collection_helper_literals self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
