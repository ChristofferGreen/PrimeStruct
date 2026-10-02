#!/usr/bin/env python3
"""Zero audit for string-tagged collection family comparisons (TODO-5386).

Collection family identity is compared through the typed `CollectionFamily`
API (`collection_helpers::isCollectionFamilyRoot` / `parseCollectionFamily`)
instead of `x == collection_helpers::kRootedVector` (`/map`, `/soa`, `/array`,
`/string`). This fails when such a comparison appears in src/ or include/
outside the enum owner, CollectionHelperNames.h.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

BASELINE_COMPARE_COUNT = 0
SCAN_DIRS = ("src", "include")
# The enum owner itself compares the spellings in its parse/format functions.
EXEMPT = {"include/primec/support/CollectionHelperNames.h"}
FAMILY = r"(?:collection_helpers::)?kRooted(?:Vector|Map|Soa|Array|String)"
COMPARE_RE = re.compile(rf"(?:[=!]=\s*{FAMILY}\b)|(?:\b{FAMILY}\s*[=!]=)")


def count_compares(text: str) -> int:
    return len(COMPARE_RE.findall(text))


def collect(root: Path) -> dict[str, int]:
    counts: dict[str, int] = {}
    for scan in SCAN_DIRS:
        for path in sorted((root / scan).rglob("*")):
            if path.suffix in {".cpp", ".h"} and path.is_file() and path.relative_to(root).as_posix() not in EXEMPT:
                n = count_compares(path.read_text(encoding="utf-8", errors="replace"))
                if n:
                    counts[path.relative_to(root).as_posix()] = n
    return counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--list", action="store_true", help="print per-file counts")
    args = parser.parse_args()
    counts = collect(args.root.resolve())
    total = sum(counts.values())
    if args.list:
        for path, n in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
            print(f"{n:4d} {path}")
    if total > BASELINE_COMPARE_COUNT:
        print(f"collection family compare audit failed: {total} string comparisons against rooted family "
              f"spellings (allowed: {BASELINE_COMPARE_COUNT}).")
        print("Use the typed family API (TODO-5374) instead of adding a new string comparison.")
        return 1
    print(f"collection family compare audit passed: {total} comparisons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
