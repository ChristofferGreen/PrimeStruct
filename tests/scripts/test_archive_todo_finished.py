#!/usr/bin/env python3
"""Self-test for scripts/archive_todo_finished.py (TODO-5366).

Covers: a legacy single file splits losslessly by month, rerunning is a no-op,
new blocks below the marker are archived verbatim and indexed, and --check
fails on unarchived blocks and on a stale index.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

LEGACY = """# PrimeStruct Finished TODOs

Legend:
  ✓ Finished

Finished items are periodically archived here from `docs/todo.md`; section headers record the archive date.

**Todo Completion (August 17, 2026)**
- [x] TODO-0001: First block
  - finished_at: 2026-08-17
  - scope: keep `|` pipes and 😀 intact
- ✓ TODO-0002: Legacy tick line

**Todo Completion (July 3, 2026) — TODO-0003**
- [x] TODO-0003: Older block
  - finished_at: 2026-07-03

Root cause paragraph without a bullet.
"""

NEW_BLOCK = """
- [x] TODO-0004: Brand new block
  - owner: ai
  - finished_at: 2026-10-02
  - result: done
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    script = repo / "scripts" / "archive_todo_finished.py"
    failures: list[str] = []

    def run(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([sys.executable, "-B", str(script), "--root", str(root), *args], text=True, capture_output=True)

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "docs").mkdir()
        finished = root / "docs" / "todo_finished.md"
        finished.write_text(LEGACY, encoding="utf-8")

        expect(run(root, "--check").returncode != 0, "legacy file with unarchived blocks fails --check")
        expect(run(root).returncode == 0, "legacy split succeeds")
        archive = root / "docs" / "todo_archive"
        aug = (archive / "2026-08.md").read_text(encoding="utf-8")
        jul = (archive / "2026-07.md").read_text(encoding="utf-8")
        expect("TODO-0001" in aug and "TODO-0002" in aug and "TODO-0003" not in aug, "August section archived by header date")
        expect("TODO-0003" in jul and "Root cause paragraph" in jul, "July section keeps its prose")
        expect("keep `|` pipes and 😀 intact" in aug, "block text is verbatim")
        original = Counter(l for l in LEGACY.split("\n")[6:] if l.strip())
        archived = Counter(l for f in archive.glob("*.md") for l in f.read_text(encoding="utf-8").split("\n")[2:] if l.strip())
        expect(original == archived, "no line lost or duplicated")
        index = finished.read_text(encoding="utf-8")
        for todo_id in ("TODO-0001", "TODO-0002", "TODO-0003"):
            expect(f"| {todo_id} |" in index, f"{todo_id} is in the index")
        expect(run(root, "--check").returncode == 0, "fresh index passes --check")

        before = {p.name: p.read_text(encoding="utf-8") for p in archive.glob("*.md")}
        expect(run(root).returncode == 0 and before == {p.name: p.read_text(encoding="utf-8") for p in archive.glob("*.md")},
               "rerun is idempotent")

        finished.write_text(finished.read_text(encoding="utf-8") + NEW_BLOCK, encoding="utf-8")
        expect(run(root, "--check").returncode != 0, "a block below the marker fails --check")
        expect(run(root).returncode == 0, "new block archives")
        expect("TODO-0004: Brand new block" in (archive / "2026-10.md").read_text(encoding="utf-8"), "new block dated by finished_at")
        expect("| TODO-0004 |" in finished.read_text(encoding="utf-8"), "new block indexed")
        expect(run(root, "--check").returncode == 0, "index current after archiving")

        finished.write_text(finished.read_text(encoding="utf-8").replace("| TODO-0004 |", "| TODO-9999 |"), encoding="utf-8")
        expect(run(root, "--check").returncode != 0, "a hand-edited index fails --check")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("archive_todo_finished self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
