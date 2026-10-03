#!/usr/bin/env python3
"""Self-test for scripts/migrate_compile_run_cases.py (TODO-5466)."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

VM_CASE = '''TEST_CASE("{name}") {{
  const std::string source = R"(
[return<int>]
main() {{
  return({code}i32)
}}
)";
  const std::string srcPath = writeTemp("vm_{tag}.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == {code});
}}
'''

NATIVE_CASE = '''TEST_CASE("{name}") {{
  const std::string source = R"(
[return<int>]
main() {{
  return({source_code}i32)
}}
)";
  const std::string srcPath = writeTemp("native_{tag}.prime", source);
  const std::string exePath = (testScratchPath("") / "native_{tag}_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == {code});
}}
'''

HEADER = '#include "../test_compile_run_helpers.h"\n\n'


def vm_file() -> str:
    cases = [VM_CASE.format(name="shared program", code=7, tag="shared"),
             VM_CASE.format(name="vm only program", code=1, tag="only"),
             VM_CASE.format(name="different exit code", code=3, tag="code")]
    return (HEADER + 'TEST_SUITE_BEGIN("primestruct.compile.run.vm.sample");\n\n' +
            "\n".join(cases) + "\nTEST_SUITE_END();\n")


def native_file(platform_guard: str = "#if defined(__linux__)") -> str:
    cases = [NATIVE_CASE.format(name="native shared program", source_code=7, code=7, tag="shared"),
             NATIVE_CASE.format(name="native other code", source_code=3, code=4, tag="code")]
    return (HEADER + platform_guard + '\nTEST_SUITE_BEGIN("primestruct.compile.run.native_backend.sample");\n\n' +
            "\n".join(cases) + "\nTEST_SUITE_END();\n#endif\n")


def run_script(script: Path, root: Path, *flags: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(script), "--root", str(root), *flags],
                          capture_output=True, text=True, check=False)


def make_tree(root: Path, native_text: str) -> None:
    for folder, name, text in (("vm", "test_vm_sample.cpp", vm_file()),
                               ("native_backend", "test_native_sample.cpp", native_text)):
        directory = root / "tests" / "unit" / "compile_run" / folder
        directory.mkdir(parents=True)
        (directory / name).write_text(text)
    (root / "tests" / "unit" / "program_matrix").mkdir(parents=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    script = repo / "scripts" / "migrate_compile_run_cases.py"
    failures: list[str] = []

    def check(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        make_tree(root, native_file())
        report = run_script(script, root, "--report")
        check(report.returncode == 0, f"report failed: {report.stderr}")
        check("merged pairs: 1" in report.stdout, f"expected one merged pair:\n{report.stdout}")
        check("reconciliation: 3 + 2 - 1 = 4 cases" in report.stdout, f"reconciliation line:\n{report.stdout}")

        applied = run_script(script, root, "--apply")
        check(applied.returncode == 0, f"apply failed: {applied.stderr}")
        matrix = root / "tests" / "unit" / "program_matrix" / "test_program_matrix_shared_programs.cpp"
        check(matrix.exists(), "matrix file not written")
        if matrix.exists():
            text = matrix.read_text()
            check('TEST_CASE("shared program")' in text, "merged case missing")
            check("return(7i32)" in text, "program source lost")
            check("program.exitCode = 7;" in text, "exit code lost")
            check('{"vm-O2", "native-O2"}' in text, "configs missing")
            check("primestruct.program_matrix.shared_programs" in text, "suite name missing")
        vm_text = (root / "tests/unit/compile_run/vm/test_vm_sample.cpp").read_text()
        native_text = (root / "tests/unit/compile_run/native_backend/test_native_sample.cpp").read_text()
        check('TEST_CASE("shared program")' not in vm_text, "vm copy not removed")
        check('TEST_CASE("vm only program")' in vm_text, "vm-only case lost")
        check('TEST_CASE("different exit code")' in vm_text, "different-code case lost")
        check('TEST_CASE("native shared program")' not in native_text, "native copy not removed")
        check('TEST_CASE("native other code")' in native_text, "native-only case lost")
        check(native_text.rstrip().endswith("#endif") and "TEST_SUITE_END();" in native_text,
              "native file trailer damaged")
        check(vm_text.count("TEST_CASE(") == 2 and native_text.count("TEST_CASE(") == 1, "case counts wrong")

    # Native files that only build on macOS arm64 are never merged: their cases would
    # start running on Linux.
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        make_tree(root, native_file("#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED"))
        report = run_script(script, root, "--report")
        check("merged pairs: 0" in report.stdout, f"gated native file must not pair:\n{report.stdout}")

    # --fix-registration lowers TOTAL_CASES and clamps ranges of suites that lost cases.
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "cmake").mkdir()
        cmake = root / "cmake" / "PrimeStructManagedSample.cmake"
        cmake.write_text(
            'addPrimeStructManagedDoctestSuite("s.total"\n    SHARD_PREFIX "a"\n    TOTAL_CASES 10\n    CASES_PER_SHARD 5)\n'
            'addPrimeStructManagedDoctestSuite("s.range"\n    SHARD_PREFIX "b"\n    SOURCE_FILE "*x.cpp"\n'
            "    RANGE_FIRST 1\n    RANGE_LAST 5)\n"
            'addPrimeStructManagedDoctestSuite("s.range"\n    SHARD_PREFIX "c"\n    SOURCE_FILE "*x.cpp"\n'
            "    RANGE_FIRST 6\n    RANGE_LAST 9)\n")
        output = root / "check.log"
        output.write_text(
            "  T --test-suite=s.total: shard 9..10 starts past the last case (7) and selects nothing\n"
            "  T --test-suite=s.range --source-file=*x.cpp: shard 6..9 starts past the last case (4) and selects nothing\n")
        fixed = run_script(script, root, "--fix-registration", str(output))
        check(fixed.returncode == 0, f"fix-registration failed: {fixed.stderr}")
        text = cmake.read_text()
        check("TOTAL_CASES 7" in text, "TOTAL_CASES not lowered")
        check("RANGE_LAST 4" in text, "RANGE_LAST not clamped")
        check('SHARD_PREFIX "c"' not in text, "empty shard block not removed")

    if failures:
        print("migrate_compile_run_cases self-test failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1
    print("migrate_compile_run_cases self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
