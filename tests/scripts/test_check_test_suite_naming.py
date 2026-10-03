#!/usr/bin/env python3
"""Self-test for scripts/check_test_suite_naming.py (TODO-5410)."""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run, write  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_test_suite_naming", repo / "scripts" / "check_test_suite_naming.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    checks = Checks("test suite naming")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for rel, suite in module.EXPECTED_FILE_SUITES.items():
            write(root / rel, f'TEST_SUITE_BEGIN("{suite}");\nTEST_CASE("x") {{}}\n')
        vm = root / "tests/unit/compile_run/vm"
        write(vm / "test_compile_run_vm_collections.cpp", "// helpers only\n")
        write(vm / "test_compile_run_vm_collections_basic.cpp",
              'TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");\nTEST_CASE("c") {}\n')

        def check():
            return run(repo, "check_test_suite_naming.py", "--root", str(root))

        checks.expect(check().returncode == 0, "a conforming tree passes")

        extra = write(root / "tests/unit/misc/test_thing_2.cpp", 'TEST_SUITE_BEGIN("a.b");\n')
        result = check()
        checks.expect(result.returncode == 1 and "numeric-only suffix" in result.stderr, "numeric suffix files fail")
        extra.unlink()

        dup = write(root / "tests/unit/misc/test_dup.cpp", 'TEST_SUITE_BEGIN("a.b");\nTEST_SUITE_BEGIN("a.b");\n')
        result = check()
        checks.expect(result.returncode == 1 and "duplicate TEST_SUITE_BEGIN" in result.stderr, "duplicate suites fail")
        dup.unlink()

        first = next(iter(module.EXPECTED_FILE_SUITES))
        original = (root / first).read_text(encoding="utf-8")
        write(root / first, 'TEST_SUITE_BEGIN("wrong.suite");\n')
        result = check()
        checks.expect(result.returncode == 1 and "expected TEST_SUITE_BEGIN" in result.stderr, "misaligned suites fail")
        write(root / first, original)

        (vm / "test_compile_run_vm_collections.cpp").unlink()
        result = check()
        checks.expect(result.returncode == 1 and "helper wrapper file is missing" in result.stderr,
                      "a missing collections wrapper fails")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
