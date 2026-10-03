#!/usr/bin/env python3
"""Self-test for scripts/finish_todo.py (TODO-5418)."""

from __future__ import annotations

import argparse
import importlib.util
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True

TODO = """### Queue Summary

| ID | Title | Status | Track |
| --- | --- | --- | --- |
| TODO-1 | First | ready | a |
| TODO-2 | Second | ready | a |

### Ready Now

- TODO-1 (track: a): First.
- TODO-2 (track: a): Second.

### Immediate Next 10

1. TODO-1 - First.
2. TODO-2 - Second.

### Priority Lanes

- Lane A: TODO-1 -> TODO-2

### Task Blocks

- [ ] TODO-1: First
  - owner: ai
  - status: ready
  - scope: s

- [ ] TODO-2: Second
  - owner: ai
  - status: ready
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("finish_todo", repo / "scripts" / "finish_todo.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "docs").mkdir()
        (root / "docs" / "todo.md").write_text(TODO, encoding="utf-8")
        (root / "docs" / "todo_finished.md").write_text("# Finished\n", encoding="utf-8")
        module.finish(root, "TODO-1", "done it", "2026-10-03", run_archive=False)
        todo = (root / "docs" / "todo.md").read_text(encoding="utf-8")
        finished = (root / "docs" / "todo_finished.md").read_text(encoding="utf-8")
        expect("- [ ] TODO-1:" not in todo, "the block leaves todo.md")
        expect("| TODO-1 |" not in todo, "the summary row is removed")
        expect("- TODO-1 (track" not in todo, "the Ready Now line is removed")
        expect("1. TODO-2 - Second." in todo, "Immediate Next 10 is renumbered")
        expect("TODO-1" not in todo.split("### Priority Lanes")[1].split("### Task Blocks")[0], "the lane drops the id")
        expect("TODO-2" in todo, "other tasks stay")
        expect("- [x] TODO-1: First" in finished and "status: done" in finished, "the finished block is marked done")
        expect("result: done it" in finished and "finished_at: 2026-10-03" in finished, "result and date are recorded")
        try:
            module.finish(root, "TODO-9", "x", "2026-10-03", run_archive=False)
            failures.append("an unknown id must fail")
        except SystemExit:
            pass

    if failures:
        print("finish_todo self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("finish_todo self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
