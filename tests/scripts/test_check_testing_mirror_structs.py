#!/usr/bin/env python3
"""Self-test for scripts/check_testing_mirror_structs.py."""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("testing mirror structs")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        src = root / "src/ir_lowerer/Info.h"
        mirror = root / "include/primec/testing/ir_lowerer_helpers/Info.h"
        write(src, "\nstruct Info {\n  int a;\n  ::primec::Expr *b;\n};\n")
        write(mirror, "\nstruct Info {\n  int a;\n  primec::Expr *b;  // spelling differs only\n};\n")

        def check():
            return run(repo, "check_testing_mirror_structs.py", "--root", str(root))

        result = check()
        checks.expect(result.returncode == 0 and "1 mirrored structs agree" in result.stdout,
                      "spelling-only differences pass")
        write(mirror, "\nstruct Info {\n  int a;\n};\n")
        result = check()
        checks.expect(result.returncode == 1 and "ODR mismatch for struct Info" in result.stderr,
                      "a missing member fails")
        write(mirror, "\nstruct Info {\n  primec::Expr *b;\n  int a;\n};\n")
        result = check()
        checks.expect(result.returncode == 1 and "member order differs" in result.stderr,
                      "a reordered member fails")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
