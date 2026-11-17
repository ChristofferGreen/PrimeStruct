#!/usr/bin/env python3
"""Self-test for scripts/check_vm_threads_corpus.py (TODO-5900).

Negative cases prove the guard fires: a concurrent test source missing from the list, a listed file that does not
exist, a matrix file that never runs the threads config, a case name that is not in its file, and a malformed line.
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True


def load(repo_root: Path):
    spec = importlib.util.spec_from_file_location("check_vm_threads_corpus", repo_root / "scripts" / "check_vm_threads_corpus.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules["check_vm_threads_corpus"] = module
    spec.loader.exec_module(module)
    return module


CMAKE = "set(Counts tasks=22 atomics=5)\n"


def write(root: Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo_root = parser.parse_args().repo_root.resolve()
    corpus = load(repo_root)
    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tasks = "tests/unit/program_matrix/test_tasks.cpp"
        helper = "tests/unit/compile_run/test_flags.cpp"
        write(root, tasks, 'TEST_SUITE_BEGIN("primestruct.program_matrix.tasks");\nvmThreadsConfig();\n')
        write(root, helper, 'TEST_SUITE_BEGIN("s");\nTEST_CASE("one case") {}\nvmTaskScheduleFlags();\n')
        good = (
            f"matrix|primestruct.program_matrix.tasks|{tasks}|2\n"
            f"cases|s|{helper}|one case\n"
            "# a comment\n"
        )
        expect(corpus.check(root, good, CMAKE) == [], "a complete list passes")
        expect(corpus.check(root, f"matrix|primestruct.program_matrix.tasks|{tasks}|2\n", CMAKE) != [],
               "a concurrent source missing from the list fails")
        expect(corpus.check(root, good + "repeated|s|tests/unit/none.cpp\n", CMAKE) != [], "a missing file fails")
        expect(corpus.check(root, good.replace("one case", "other case"), CMAKE) != [], "an unknown case name fails")
        expect(corpus.check(root, good, "set(Counts atomics=5)\n") != [], "a suite without a case count fails")
        write(root, tasks, 'TEST_SUITE_BEGIN("primestruct.program_matrix.tasks");\n--vm-threads\n')
        expect(corpus.check(root, good, CMAKE) != [], "a matrix file without the threads config fails")
        expect(corpus.check(root, good + "bogus|x\n", CMAKE) != [], "an unknown kind fails")
        expect(corpus.check(root, good + "exclude|only-one-field\n", CMAKE) != [], "a short line fails")
        write(root, tasks, 'TEST_SUITE_BEGIN("primestruct.program_matrix.tasks");\nvmThreadsConfig();\n')
        excluded = f"cases|s|{helper}|one case\nexclude|{tasks}|parses flags only\n"
        expect(corpus.check(root, excluded, CMAKE) == [], "an exclusion with a reason passes")

    live = corpus.check(repo_root, (repo_root / "tests" / "vm_threads_corpus.txt").read_text(),
                        (repo_root / "CMakeLists.txt").read_text())
    expect(live == [], f"the checked-in list is complete: {live}")

    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
