"""Shared helpers for the subprocess-style audit self-tests (TODO-5410)."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True


class Checks:
    def __init__(self, name: str) -> None:
        self.name = name
        self.failures: list[str] = []

    def expect(self, condition: bool, message: str) -> None:
        if not condition:
            self.failures.append(message)

    def finish(self) -> int:
        if self.failures:
            print(f"{self.name} self-test failed:")
            for failure in self.failures:
                print(f"  {failure}")
            return 1
        print(f"{self.name} self-test passed")
        return 0


def run(repo: Path, script: str, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(repo / "scripts" / script), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def write(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def write_json(path: Path, value) -> Path:
    return write(path, json.dumps(value))
