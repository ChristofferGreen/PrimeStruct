#!/usr/bin/env python3
"""Self-test for scripts/check_testing_mirror_structs.py (no duplicated lowerer headers)."""

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
    checks = Checks("testing mirror headers")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write(root / "include/primec/ir_lowerer/A.h", "#pragma once\n")
        write(root / "src/ir_lowerer/B.h", "#pragma once\n")
        write(root / "include/primec/testing/Umbrella.h", "#pragma once\n")
        result = run(repo, "check_testing_mirror_structs.py", "--root", str(root))
        checks.expect(result.returncode == 0, f"distinct headers pass: {result.stdout}")
        write(root / "include/primec/testing/ir_lowerer_helpers/A.h", "#pragma once\n")
        result = run(repo, "check_testing_mirror_structs.py", "--root", str(root))
        checks.expect(result.returncode == 1 and "A.h exists twice" in result.stdout, "a duplicated basename fails")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
