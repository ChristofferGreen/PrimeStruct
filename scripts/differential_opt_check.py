#!/usr/bin/env python3
"""Differential check of the IR optimizer on real programs.

Extracts every PrimeStruct program embedded as a raw string in the VM
compile-run tests (plus benchmarks/*.prime and the checked-in examples that have
a `main`), runs each with `primevm` at -O0 and at each requested optimization
level with --opt-verify-each, and reports any program whose exit code, stdout or
stderr differs. A program that behaves differently between two -O0 runs is
reported as unstable and skipped.

    python3 scripts/differential_opt_check.py --build-dir build-release [--levels 1,2,3]
                                              [--jobs 4] [--limit N] [--keep-dir DIR]
                                              [--optexe [--optexe-levels 0,2]] [--native [--native-levels 0,2]]
                                              [--baseline-kernel step]

With --optexe each program is also compiled with `primec --emit=optexe` (at each
--optexe-levels host optimization level, via `-O<n>`) and the resulting binary is
compared against the -O0 VM run; --native does the same for `primec --emit=native`
executables (Linux x86_64 and macOS arm64). Programs a backend rejects are counted
separately and skipped.

Run from anywhere; `primevm` is executed with the build directory as its working
directory, as the compile-run tests do. Exit status 0 when every program
matches, 1 when any differs.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

RAW_STRING_RE = re.compile(r'R"\((.*?)\)"', re.DOTALL)
MAIN_RE = re.compile(r"\bmain\s*\(")
TIMEOUT_SECONDS = 30


def collect_sources(root: Path) -> list[tuple[str, str]]:
    """Returns (label, source) pairs, de-duplicated by content, in a stable order."""
    seen: set[str] = set()
    sources: list[tuple[str, str]] = []

    def add(label: str, text: str) -> None:
        if not MAIN_RE.search(text):
            return
        digest = hashlib.sha1(text.encode("utf-8")).hexdigest()
        if digest in seen:
            return
        seen.add(digest)
        sources.append((label, text))

    for path in sorted((root / "tests" / "unit" / "compile_run" / "vm").glob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for index, match in enumerate(RAW_STRING_RE.finditer(text)):
            add(f"{path.name}#{index}", match.group(1))
    for path in sorted((root / "benchmarks").glob("*.prime")):
        add(path.name, path.read_text(encoding="utf-8", errors="replace"))
    for path in sorted((root / "examples").rglob("*.prime")):
        add(str(path.relative_to(root)), path.read_text(encoding="utf-8", errors="replace"))
    return sources


def run_vm(build_dir: Path, source_path: Path, flags: list[str], kernel: str = "") -> tuple[int, str, str]:
    env = dict(os.environ)
    if kernel:
        env["PRIMEVM_KERNEL"] = kernel
    try:
        completed = subprocess.run(
            [str(build_dir / "primevm"), str(source_path), "--entry", "/main", *flags],
            cwd=build_dir,
            capture_output=True,
            timeout=TIMEOUT_SECONDS,
            env=env,
        )
    except subprocess.TimeoutExpired:
        return (-999, "", "timeout")
    return (
        completed.returncode,
        completed.stdout.decode("utf-8", errors="replace"),
        completed.stderr.decode("utf-8", errors="replace"),
    )


def run_emitted(build_dir: Path, source_path: Path, kind: str, level: int) -> tuple[int, str, str] | str:
    """Compiles with the given emit kind (optexe or native) and runs the binary.
    Returns the run result, or a string naming why it could not be compared
    (`unsupported` when the backend rejects the program, `frontend` for any other
    compile failure)."""
    binary = source_path.with_suffix(f".{kind}{level}")
    try:
        compiled = subprocess.run(
            [str(build_dir / "primec"), str(source_path), f"--emit={kind}", f"-O{level}", "--entry", "/main",
             "-o", str(binary)],
            cwd=build_dir,
            capture_output=True,
            timeout=TIMEOUT_SECONDS * 4,
        )
    except subprocess.TimeoutExpired:
        return "frontend"
    if compiled.returncode != 0:
        message = compiled.stderr.decode("utf-8", errors="replace")
        rejected = "optexe does not support" in message or "native backend" in message or "IR lowering error" in message
        return "unsupported" if rejected else "frontend"
    try:
        completed = subprocess.run([str(binary)], cwd=build_dir, capture_output=True, timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        binary.unlink(missing_ok=True)
        return (-999, "", "timeout")
    binary.unlink(missing_ok=True)
    return (
        completed.returncode,
        completed.stdout.decode("utf-8", errors="replace"),
        completed.stderr.decode("utf-8", errors="replace"),
    )


def check_one(args: tuple[str, str, Path, Path, list[int], list[int], list[int], str]) -> tuple[str, str, str]:
    """Returns (label, status, detail); status is ok, unstable, unsupported, or DIFF."""
    label, text, build_dir, work_dir, levels, optexe_levels, native_levels, baseline_kernel = args
    digest = hashlib.sha1(text.encode("utf-8")).hexdigest()[:12]
    source_path = work_dir / f"{digest}.prime"
    # The tests substitute a scratch path for this placeholder; give each program
    # its own file so parallel jobs do not race on a shared one.
    text = text.replace("__PATH__", str(work_dir / f"{digest}.data"))
    source_path.write_text(text, encoding="utf-8")

    baseline = run_vm(build_dir, source_path, [], baseline_kernel)
    again = run_vm(build_dir, source_path, [], baseline_kernel)
    if baseline != again:
        return (label, "unstable", "two -O0 runs differ")
    if baseline[0] == -999:
        return (label, "unstable", "timeout at -O0")

    for level in levels:
        result = run_vm(build_dir, source_path, [f"-O{level}", "--opt-verify-each"])
        if result != baseline:
            detail = [f"-O{level} differs from -O0 (source kept as {source_path.name})"]
            if result[0] != baseline[0]:
                detail.append(f"  exit code: {baseline[0]} -> {result[0]}")
            if result[1] != baseline[1]:
                detail.append(f"  stdout: {baseline[1][:200]!r} -> {result[1][:200]!r}")
            if result[2] != baseline[2]:
                detail.append(f"  stderr: {baseline[2][:300]!r} -> {result[2][:300]!r}")
            return (label, "DIFF", "\n".join(detail))
    unsupported = False
    emitted = [("optexe", level) for level in optexe_levels] + [("native", level) for level in native_levels]
    first_by_kind: dict[str, tuple[int, tuple[int, str, str]]] = {}
    for kind, level in emitted:
        result = run_emitted(build_dir, source_path, kind, level)
        if not isinstance(result, str):
            # An executable must behave the same at every level even where it
            # differs from the VM (known backend gaps): compare against the
            # first level of the same kind before comparing against the VM.
            if kind in first_by_kind and result != first_by_kind[kind][1]:
                return (label, "DIFF", f"{kind} -O{level} differs from {kind} -O{first_by_kind[kind][0]} "
                                       f"(source kept as {source_path.name})")
            first_by_kind.setdefault(kind, (level, result))
        if isinstance(result, str):
            if result == "frontend" and baseline[0] == 0:
                return (label, "DIFF", f"{kind} -O{level} failed to compile a program the VM runs "
                                       f"(source kept as {source_path.name})")
            unsupported = True
            continue
        if result != baseline:
            detail = [f"{kind} -O{level} differs from the -O0 VM (source kept as {source_path.name})"]
            if result[0] != baseline[0]:
                detail.append(f"  exit code: {baseline[0]} -> {result[0]}")
            if result[1] != baseline[1]:
                detail.append(f"  stdout: {baseline[1][:200]!r} -> {result[1][:200]!r}")
            if result[2] != baseline[2]:
                detail.append(f"  stderr: {baseline[2][:300]!r} -> {result[2][:300]!r}")
            return (label, "DIFF", "\n".join(detail))
    source_path.unlink()
    return (label, "unsupported" if unsupported else "ok", "")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=Path("build-release"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--levels", default="1", help="comma-separated -O levels to compare against -O0")
    parser.add_argument("--optexe", action="store_true", help="also compare optexe binaries against the -O0 VM")
    parser.add_argument("--optexe-levels", default="2", help="comma-separated host -O levels for --optexe")
    parser.add_argument("--native", action="store_true", help="also compare native executables against the -O0 VM")
    parser.add_argument("--native-levels", default="0,2", help="comma-separated -O levels for --native")
    parser.add_argument("--baseline-kernel", default="", choices=["", "step"],
                        help="run the -O0 baseline on the step kernel (PRIMEVM_KERNEL=step) to compare it with the fast kernel")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--limit", type=int, default=0, help="check only the first N programs")
    parser.add_argument("--keep-dir", type=Path, default=None, help="directory for programs that differ")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if not (build_dir / "primevm").exists():
        print(f"error: {build_dir / 'primevm'} not found; build first", file=sys.stderr)
        return 2
    levels = [int(part) for part in args.levels.split(",") if part]
    optexe_levels = [int(part) for part in args.optexe_levels.split(",") if part] if args.optexe else []
    native_levels = [int(part) for part in args.native_levels.split(",") if part] if args.native else []
    sources = collect_sources(args.root)
    if args.limit > 0:
        sources = sources[: args.limit]

    work_root = args.keep_dir if args.keep_dir else Path(tempfile.mkdtemp(prefix="primec_diff_"))
    work_root.mkdir(parents=True, exist_ok=True)
    jobs = [(label, text, build_dir, work_root, levels, optexe_levels, native_levels, args.baseline_kernel) for label, text in sources]

    counts = {"ok": 0, "unstable": 0, "unsupported": 0, "DIFF": 0}
    failures: list[str] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for label, status, detail in pool.map(check_one, jobs):
            counts[status] += 1
            if status == "DIFF":
                failures.append(f"{label}: {detail}")

    print(f"programs checked: {len(sources)}  ok: {counts['ok']}  unstable (skipped): {counts['unstable']}  "
          f"emit-unsupported: {counts['unsupported']}  differing: {counts['DIFF']}  levels: {levels}")
    for failure in failures:
        print(failure)
    if failures:
        print(f"differing programs are kept in {work_root}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
