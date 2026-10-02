#!/usr/bin/env python3
"""Self-test for scripts/check_spec_docs.py (TODO-5368)."""

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
    spec = importlib.util.spec_from_file_location("check_spec_docs", repo / "scripts" / "check_spec_docs.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    expect(module.slug("Move/Copy/Destroy") == "movecopydestroy", "slug drops slashes")
    expect(module.slug("Type System v1 (draft)") == "type-system-v1-draft", "slug joins words")
    expect(module.anchors("# A\n## A\n```\n# no\n```\n") == {"a", "a-1"}, "anchors dedupe and skip fences")
    expect(module.index_rows("| [T](spec/x.md) | normative | 3 |\n| h | h |\n") == {"x.md": "normative"}, "rows parse")

    def build(index_extra: str, part: str, rows: str = "| [X](spec/x.md) | normative | 3 |") -> list[str]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "docs" / "spec").mkdir(parents=True)
            (root / "docs" / "PrimeStruct.md").write_text(f"# Index\n\n{rows}\n{index_extra}\n", encoding="utf-8")
            (root / "docs" / "spec" / "x.md").write_text(part, encoding="utf-8")
            return module.find_problems(root.resolve())

    good = "# X\n\n> Part of the [index](../PrimeStruct.md). Classification: **normative**.\n\n## Heading\n"
    expect(build("", good) == [], "a consistent tree passes")
    expect(build("", good, rows="") != [], "an unlisted part is reported")
    expect(build("", good, rows="| [X](spec/x.md) | bogus | 3 |") != [], "an unknown classification is reported")
    expect(build("", good, rows="| [X](spec/x.md) | normative | 3 |\n| [Y](spec/y.md) | normative | 3 |") != [], "a row for a missing part is reported")
    expect(build("", "# X\n\nno breadcrumb\n\n") != [], "a missing breadcrumb is reported")
    expect(build("[bad](spec/nope.md)", good) != [], "a broken link is reported")
    expect(build("[bad](spec/x.md#missing)", good) != [], "a broken anchor is reported")
    expect(build("[ok](spec/x.md#heading)", good) == [], "a valid anchor passes")

    if failures:
        print("spec docs self-test failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("spec docs self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
