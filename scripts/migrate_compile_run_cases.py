#!/usr/bin/env python3
"""Merge duplicated vm/native compile-run cases into program-matrix cases (TODO-5466).

    python3 scripts/migrate_compile_run_cases.py --report
    python3 scripts/migrate_compile_run_cases.py --apply [--chunk 30]

Two shapes are converted, and nothing else is touched:

  vm:      source; writeTemp; `./primec --emit=vm <src> --entry /main`; CHECK(runCommand(..) == N)
  native:  source; writeTemp; exePath; `./primec --emit=native <src> -o <exe> --entry /main`;
           CHECK(compile == 0); CHECK(runCommand(exePath) == N)

A vm case and a native case with the same program source and the same exit code
become ONE program-matrix case that runs the program on vm-O2 and native-O2 (the
two configurations the originals ran at their default level), so the number of
program runs is unchanged while the duplicated source disappears. Cases in any
other shape (stdout/stderr files, diagnostics, helper functions, flags) stay, and so do
native files behind a PRIMESTRUCT_NATIVE_*_ENABLED guard (macOS arm64 only).

--report prints the reconciliation (old vm + old native - merged = new) and which
suites change. --apply writes tests/unit/program_matrix/test_program_matrix_shared_programs[_a..].cpp,
removes the merged cases from their source files. Afterwards rebuild, run
scripts/check_test_registration.py, and pass its output to --fix-registration to shrink the
managed shards of the suites that lost cases (the doctest registration guard checks them).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ENTRY_FLAGS = " --entry /main"
CASE_START = re.compile(r'^TEST_CASE\("((?:[^"\\]|\\.)*)"\) \{\n', re.M)

VM_TAIL = re.compile(
    r'\s*const std::string srcPath =\s*writeTemp\("([^"]+)", source\);\s*'
    r'const std::string runCmd = "\./primec --emit=vm " \+ srcPath \+ "( [^"]*)?";\s*'
    r'CHECK\(runCommand\(runCmd\) == (\d+)\);\s*\}\n'
)
NATIVE_TAIL = re.compile(
    r'\s*const std::string srcPath =\s*writeTemp\("([^"]+)", source\);\s*'
    r'const std::string exePath =\s*\(testScratchPath\(""\) /\s*"([^"]+)"\)\s*\.string\(\);\s*'
    r'const std::string compileCmd =\s*"\./primec --emit=native " \+ srcPath \+ " -o " \+ exePath \+ "( [^"]*)?";\s*'
    r'CHECK\(runCommand\(compileCmd\) == 0\);\s*CHECK\(runCommand\(exePath\) == (\d+)\);\s*\}\n'
)


SUITE_RE = re.compile(r'TEST_SUITE_BEGIN\("([^"]+)"\)')


class Case:
    def __init__(self, path: Path, name: str, start: int, end: int, source: str | None, tail: str):
        self.suite = ""
        self.path = path
        self.name = name
        self.start = start  # offset of TEST_CASE
        self.end = end  # offset just past the case's closing "}\n"
        self.source = source
        self.tail = tail


def split_cases(path: Path, text: str) -> list[Case]:
    cases = []
    for match in CASE_START.finditer(text):
        body = match.end()
        raw = text.find('R"(', body)
        next_case = text.find("\nTEST_CASE(", body)
        if next_case == -1:
            next_case = len(text)
        if raw == -1 or raw > next_case:
            cases.append(Case(path, match.group(1), match.start(), next_case + 1, None, ""))
            continue
        source_end = text.find('\n)";\n', raw)
        if source_end == -1 or source_end > next_case:
            cases.append(Case(path, match.group(1), match.start(), next_case + 1, None, ""))
            continue
        tail_start = source_end + 5
        cases.append(
            Case(path, match.group(1), match.start(), next_case + 1, text[raw + 3 : source_end + 1], text[tail_start:next_case])
        )
    return cases


def tail_exit(case: Case, pattern: re.Pattern, flags_index: int, code_index: int):
    if case.source is None or "\\" in case.name:
        return None
    match = pattern.match(case.tail)
    if match is None:
        return None
    flags = match.group(flags_index) or ""
    if flags != ENTRY_FLAGS:
        return None
    return int(match.group(code_index))


def exact_end(text: str, case: Case) -> int:
    """End offset of the case: just past the closing brace line (and one blank line)."""
    tail_start = case.end - len(case.tail) - 1
    brace = text.find("\n}\n", tail_start)
    end = brace + 3
    if text.startswith("\n", end):
        end += 1
    return end


def collect(root: Path):
    unit = root / "tests" / "unit" / "compile_run"
    result = {"vm": [], "native": []}
    totals = {"vm": 0, "native": 0}
    suite_cases: dict[str, int] = {}  # cases per suite that build on every platform
    for kind, folder, pattern, flags_index, code_index in (
        ("vm", "vm", VM_TAIL, 2, 3),
        ("native", "native_backend", NATIVE_TAIL, 3, 4),
    ):
        for path in sorted((unit / folder).glob("*.cpp")):
            text = path.read_text()
            # Files behind PRIMESTRUCT_NATIVE_*_ENABLED only build on macOS arm64; their
            # cases would newly run (and may fail) on Linux x86_64, so they stay put.
            platform_gated = kind == "native" and "#if PRIMESTRUCT_NATIVE_" in text
            suite_match = SUITE_RE.search(text)
            for case in split_cases(path, text):
                case.suite = suite_match.group(1) if suite_match else ""
                totals[kind] += 1
                suite_cases[case.suite] = suite_cases.get(case.suite, 0) + (0 if platform_gated else 1)
                if platform_gated:
                    continue
                code = tail_exit(case, pattern, flags_index, code_index)
                if code is not None:
                    result[kind].append((case, code))
    return result, totals, suite_cases


def keep_suites_populated(pairs, suite_cases):
    """Drops pairs that would leave a suite with no case that builds everywhere.

    A suite without cases is missing from the doctest binary, which the suite
    registration check reports as a mismatch.
    """
    remaining = dict(suite_cases)
    for vm, native, _code in pairs:
        remaining[vm.suite] = remaining.get(vm.suite, 0) - 1
        remaining[native.suite] = remaining.get(native.suite, 0) - 1
    kept = []
    for pair in pairs:
        vm, native, _code = pair
        if remaining.get(vm.suite, 0) <= 0 or remaining.get(native.suite, 0) <= 0:
            remaining[vm.suite] = remaining.get(vm.suite, 0) + 1
            remaining[native.suite] = remaining.get(native.suite, 0) + 1
            continue
        kept.append(pair)
    return kept


def pair_up(found):
    by_source = {}
    for case, code in found["native"]:
        by_source.setdefault((case.source, code), []).append(case)
    pairs = []
    used_native = set()
    for case, code in found["vm"]:
        candidates = by_source.get((case.source, code), [])
        for native in candidates:
            if id(native) not in used_native:
                used_native.add(id(native))
                pairs.append((case, native, code))
                break
    return pairs


def identifier(name: str, index: int) -> str:
    base = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()[:60] or "case"
    return f"{base}_{index}"


def render_case(vm: Case, code: int, index: int) -> str:
    return (
        f'TEST_CASE("{vm.name}") {{\n'
        "  program_matrix::ProgramCase program;\n"
        f'  program.name = "{identifier(vm.name, index)}";\n'
        f'  program.source = R"({vm.source})";\n'
        f"  program.exitCode = {code};\n"
        '  program.onlyConfigs = {"vm-O2", "native-O2"};\n'
        "  program_matrix::runProgramMatrix(program);\n"
        "}\n"
    )


def report(found, totals, pairs) -> None:
    print(f"vm cases: {totals['vm']} ({len(found['vm'])} in the plain shape)")
    print(f"native cases: {totals['native']} ({len(found['native'])} in the plain shape)")
    print(f"merged pairs: {len(pairs)}")
    new_total = totals["vm"] + totals["native"] - len(pairs)
    print(f"reconciliation: {totals['vm']} + {totals['native']} - {len(pairs)} = {new_total} cases"
          f" (vm {totals['vm'] - len(pairs)}, native {totals['native'] - len(pairs)}, matrix {len(pairs)})")


def apply(root: Path, pairs, chunk: int) -> list[Path]:
    # 1. Write the matrix files.
    matrix_dir = root / "tests" / "unit" / "program_matrix"
    written = []
    for chunk_index in range(0, len(pairs), chunk):
        number = chunk_index // chunk
        tag = "" if len(pairs) <= chunk else "_" + chr(ord("a") + number)
        body = [
            '#include "program_matrix.h"\n',
            f'TEST_SUITE_BEGIN("primestruct.program_matrix.shared_programs{tag}");\n',
            "// Programs that the vm and native compile-run suites used to carry twice (TODO-5466,\n"
            "// scripts/migrate_compile_run_cases.py): one case runs the program on vm-O2 and\n"
            "// native-O2 and checks the shared exit code.\n",
        ]
        for offset, (vm, _native, code) in enumerate(pairs[chunk_index : chunk_index + chunk]):
            body.append(render_case(vm, code, chunk_index + offset))
        body.append("TEST_SUITE_END();\n")
        path = matrix_dir / f"test_program_matrix_shared_programs{tag}.cpp"
        path.write_text("\n".join(body))
        written.append(path)
    # 2. Remove the merged cases from their files (last first so offsets stay valid).
    removals = {}
    for vm, native, _code in pairs:
        for case in (vm, native):
            removals.setdefault(case.path, []).append(case)
    for path, cases in removals.items():
        text = path.read_text()
        for case in sorted(cases, key=lambda c: c.start, reverse=True):
            text = text[: case.start] + text[exact_end(text, case) :]
        path.write_text(text)
    return written


SHARD_PAST_END = re.compile(
    r"--test-suite=(\S+)(?: --source-file=(\S+))?: shard \d+\.\.\d+ starts past the last case \((\d+)\)"
)


def fix_registration(root: Path, check_output: str) -> list[str]:
    """Shrinks the managed shards of suites that lost cases.

    `check_output` is the output of scripts/check_test_registration.py. A suite
    whose shards start past its real last case gets TOTAL_CASES lowered, its
    RANGE_LAST clamped, or the empty shard block removed. Returns the files changed.
    """
    counts = {}
    for line in check_output.splitlines():
        match = SHARD_PAST_END.search(line)
        if match:
            counts[(match.group(1), match.group(2))] = int(match.group(3))
    changed = []
    for path in sorted((root / "cmake").glob("PrimeStructManaged*.cmake")):
        text = path.read_text()
        edits = []
        for match in re.finditer(r'addPrimeStructManagedDoctestSuite\("([^"]+)"([^)]*)\)\n', text):
            source_file = re.search(r'SOURCE_FILE "([^"]+)"', match.group(2))
            key = (match.group(1), source_file.group(1) if source_file else None)
            if key not in counts:
                continue
            real = counts[key]
            body = match.group(2)
            first = re.search(r"RANGE_FIRST (\d+)", body)
            last = re.search(r"RANGE_LAST (\d+)", body)
            if first and last:
                if int(first.group(1)) > real:
                    edits.append((match.start(), match.end(), ""))
                elif int(last.group(1)) > real:
                    edits.append((match.start(2), match.end(2), re.sub(r"RANGE_LAST \d+", f"RANGE_LAST {real}", body)))
            elif re.search(r"TOTAL_CASES \d+", body):
                edits.append((match.start(2), match.end(2), re.sub(r"TOTAL_CASES \d+", f"TOTAL_CASES {real}", body)))
        for start, end, replacement in sorted(edits, reverse=True):
            text = text[:start] + replacement + text[end:]
        if edits:
            path.write_text(text)
            changed.append(str(path.relative_to(root)))
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--chunk", type=int, default=30)
    parser.add_argument("--fix-registration", type=Path, metavar="CHECK_OUTPUT",
                        help="file with the output of scripts/check_test_registration.py")
    args = parser.parse_args()
    if args.fix_registration:
        for name in fix_registration(args.root, args.fix_registration.read_text()):
            print("updated", name)
        return 0
    found, totals, suite_cases = collect(args.root)
    pairs = keep_suites_populated(pair_up(found), suite_cases)
    report(found, totals, pairs)
    if args.apply:
        written = apply(args.root, pairs, args.chunk)
        for path in written:
            print("wrote", path.relative_to(args.root))
    elif not args.report:
        parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
