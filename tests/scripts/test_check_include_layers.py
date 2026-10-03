#!/usr/bin/env python3
"""Self-test for scripts/check_include_layers.py."""

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
    checks = Checks("include layers")
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        allow = write(root / "allow.txt", "# empty\n")
        write(root / "include/primec/Ok.h", '#include "primec/Other.h"\n')
        write(root / "src/a.cpp", '#include "primec/Ok.h"\n')
        write(root / "tests/t.cpp", '#include "primec/Ok.h"\n')

        def check():
            return run(repo, "check_include_layers.py", "--root", str(root), "--allowlist", str(allow))

        checks.expect(check().returncode == 0, "a clean tree passes")

        write(root / "include/primec/Bad.h", '#include "src/internal.h"\n')
        result = check()
        checks.expect(result.returncode == 1 and "public headers must not include private src" in result.stderr,
                      "public header -> src fails")
        (root / "include/primec/Bad.h").unlink()

        write(root / "src/b.cpp", '#include "tests/helper.h"\n')
        result = check()
        checks.expect(result.returncode == 1 and "production sources must not include test" in result.stderr,
                      "src -> tests fails")
        (root / "src/b.cpp").unlink()

        write(root / "src/ir_lowerer/L.cpp", '#include "semantics/Private.h"\n')
        result = check()
        checks.expect(result.returncode == 1 and "lowerer sources must not include private semantics" in result.stderr,
                      "lowerer -> semantics fails")
        write(allow, "src/ir_lowerer/L.cpp -> src/semantics/Private.h\n")
        checks.expect(check().returncode == 0, "an allowlisted lowerer dependency passes")
        (root / "src/ir_lowerer/L.cpp").unlink()
        stale = check()
        checks.expect(stale.returncode == 1 and "stale allowlist entry" in stale.stderr,
                      "an unused allowlist entry is stale")
        write(allow, "# empty\n")

        write(root / "tests/u.cpp", '#include "src/x/Y.h"\n')
        result = check()
        checks.expect(result.returncode == 1 and "tests -> src include is not allowlisted" in result.stderr,
                      "tests -> src fails")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
