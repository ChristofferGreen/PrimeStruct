#!/usr/bin/env python3
"""Self-test for scripts/check_todo_references.py (TODO-5396)."""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("check_todo_references", repo / "scripts" / "check_todo_references.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "docs" / "todo_archive").mkdir(parents=True)
        (root / "src").mkdir()
        (root / "include").mkdir()
        (root / "docs" / "todo.md").write_text("- [ ] TODO-1: open\n", encoding="utf-8")
        (root / "docs" / "todo_archive" / "2026-10.md").write_text("- [x] TODO-2: done\n", encoding="utf-8")
        (root / "src" / "a.cpp").write_text("// TODO-1 open\n// TODO-2 done\n// TODO-9 nowhere\n", encoding="utf-8")
        open_ids, archived = module.known_ids(root)
        expect(open_ids == {"1"} and archived == {"2"}, "ids are read from todo.md and the archive")
        dangling, closed = module.scan(root, open_ids, archived)
        expect(len(dangling) == 1 and "TODO-9" in dangling[0], "an unknown id is dangling")
        expect(closed == 1, "an archived id counts as a closed citation")

    expect(module.evaluate([], module.BASELINE_CLOSED_CITATIONS) == [], "the baseline count passes")
    expect(module.evaluate([], module.BASELINE_CLOSED_CITATIONS + 1) != [], "growth past the baseline fails")
    expect(module.evaluate([], module.BASELINE_CLOSED_CITATIONS - 1) != [], "shrinking requires lowering the baseline")
    expect(module.evaluate(["x"], module.BASELINE_CLOSED_CITATIONS) == ["x"], "dangling ids fail")

    if failures:
        print("todo reference self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("todo reference self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
