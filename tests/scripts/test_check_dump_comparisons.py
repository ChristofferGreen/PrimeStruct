#!/usr/bin/env python3
"""Self-test for scripts/check_dump_comparisons.py (TODO-5355).

Negative cases: a raw `CHECK(readFile(a) == readFile(b))` or `CHECK(dumpA == dumpB)`
in a dump-stage test is reported. Positive cases: normalized comparisons,
comparisons against literals, and files that never run `--dump-stage` pass.
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo_root = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_dump_comparisons", repo_root / "scripts" / "check_dump_comparisons.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    header = 'const std::string cmd = "./primec x --dump-stage type-graph";\n'
    cases = [
        ("raw readFile pair", header + "  CHECK(readFile(a) == readFile(b));\n", True),
        ("raw named dumps", header + "  CHECK(hyphenDump == underscoreDump);\n", True),
        ("multi-line raw pair", header + "  CHECK(readFile(primecOut) ==\n        readFile(primevmOut));\n", True),
        ("normalized pair", header + "  CHECK(stripDumpTimings(readFile(a)) == stripDumpTimings(readFile(b)));\n", False),
        ("normalized named", header + "  CHECK(stripDumpTimings(dumpA) == stripDumpTimings(dumpB));\n", False),
        ("literal comparison", header + '  CHECK(dump == "expected text");\n', False),
        ("no dump stage in file", "  CHECK(readFile(a) == readFile(b));\n", False),
        ("size comparison", header + "  CHECK(dump.size() == 3);\n", False),
    ]
    failures = []
    for name, text, expect_flagged in cases:
        flagged = bool(module.check_text(text))
        if flagged != expect_flagged:
            failures.append(f"{name}: expected flagged={expect_flagged}, got {flagged}")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_dump_comparisons self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
