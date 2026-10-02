#!/usr/bin/env python3
"""Self-test for scripts/check_test_registration.py and generate_test_inventory.py (TODO-5354).

Negative cases prove the guards fire: a shard range with a wrong total, a shard
that selects nothing, a doctest case no CTest entry runs, a TEST_CASE file that
is not part of any target, and a stale generated inventory.
"""

from __future__ import annotations

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


sys.dont_write_bytecode = True


def load(repo_root: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, repo_root / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def doctest_test(first: int | None, last: int | None, suite: str = "s", extra: tuple[str, ...] = ()):
    command = ["/bin/bin_a", f"--test-suite={suite}", *extra, "--order-by=file"]
    if first is not None:
        command += [f"--first={first}", f"--last={last}"]
    return {"command": command}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo_root = parser.parse_args().repo_root.resolve()
    reg = load(repo_root, "check_test_registration")
    inv = load(repo_root, "generate_test_inventory")
    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    # --- shard ranges -----------------------------------------------------
    expect(reg.find_range_problems([(1, 5), (6, 10)], 10) == [], "exact tiling passes")
    expect(reg.find_range_problems([(1, 5), (6, 10)], 12) != [], "too few cases covered fails")
    expect(reg.find_range_problems([(1, 5), (7, 10)], 10) != [], "gap fails")
    expect(reg.find_range_problems([(1, 6), (6, 10)], 10) != [], "overlap fails")
    expect(reg.find_range_problems([(1, 5), (6, 12)], 9) == [], "partly filled last shard is tolerated")
    expect(reg.find_range_problems([(1, 5), (6, 8), (9, 10)], 7) != [], "shard past the end selects nothing")

    counts = {("/bin/bin_a", ("--test-suite=s",)): 10}
    counter = lambda binary, filters: counts[(binary, filters)]
    good = [doctest_test(1, 5), doctest_test(6, 10)]
    expect(reg.check_shards(good, counter) == [], "correct shard registration passes")
    expect(reg.check_shards([doctest_test(1, 5), doctest_test(6, 20)] + [doctest_test(21, 25)], counter) != [],
           "a wrong TOTAL_CASES (empty trailing shard) is reported")
    expect(reg.check_shards([doctest_test(1, 5)], counter) != [], "uncovered cases are reported")

    # --- exact coverage ---------------------------------------------------
    cases = [("s", "a.cpp", f"case {i}") for i in range(4)] + [("other", "b.cpp", "stray")]

    def lister(binary, args):
        selected = [c for c in cases if f"--test-suite={c[0]}" in args] if any(a.startswith("--test-suite=") for a in args) else list(cases)
        return selected

    expect(reg.check_coverage([doctest_test(None, None)], lister) != [], "a suite nobody registered is reported")
    covering = [doctest_test(None, None), doctest_test(None, None, suite="other")]
    expect(reg.check_coverage(covering, lister) == [], "everything registered passes")
    expect("stray" in " ".join(reg.check_coverage([doctest_test(None, None)], lister)), "the missing case is named")

    # --- unregistered files ----------------------------------------------
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        unit = root / "tests" / "unit" / "x"
        unit.mkdir(parents=True)
        (unit / "built.cpp").write_text('#include "helper.h"\nTEST_CASE("a") {}\n')
        (unit / "helper.h").write_text('TEST_CASE("from header") {}\n')
        (unit / "orphan.cpp").write_text('TEST_CASE("never built") {}\n')
        (unit / "no_cases.h").write_text("// utility only\n")
        findings = reg.check_unregistered_files(root, [unit / "built.cpp"], [])
        expect(len(findings) == 1 and "orphan.cpp" in findings[0], f"only the orphan is reported: {findings}")

        # --- inventory ----------------------------------------------------
        (unit / "orphan.cpp").unlink()
        script = repo_root / "scripts" / "generate_test_inventory.py"
        run = lambda *a: subprocess.run([sys.executable, str(script), "--root", str(root), *a], text=True, capture_output=True)
        expect(run().returncode == 0, "inventory generation succeeds")
        expect(run("--check").returncode == 0, "fresh inventory passes --check")
        (unit / "built.cpp").write_text('#include "helper.h"\nTEST_CASE("a") {}\nTEST_CASE("added later") {}\n')
        expect(run("--check").returncode != 0, "stale inventory fails --check")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("check_test_registration self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
