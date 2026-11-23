#!/usr/bin/env python3
"""Fail when a tracked stdlib symbol manifest differs from the generator.

Every tracked `*.psmeta` file that carries the generator's `# Module:` and
`# Source:` header lines is regenerated with `generate_stdlib_manifest` into a
scratch directory and compared byte for byte with the tracked file. Hand
written manifests (no generator header) are skipped.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def tracked_manifests(root: Path):
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "*.psmeta"],
        check=True, capture_output=True, text=True).stdout
    return sorted(line for line in out.splitlines() if line)


def header_value(text: str, key: str):
    prefix = f"# {key}: "
    for line in text.splitlines():
        if line.startswith(prefix):
            return line[len(prefix):].strip()
        if line.startswith("["):
            break
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--scratch", default=None)
    args = parser.parse_args()
    root = Path(args.root).resolve()
    failures = []
    checked = 0
    with tempfile.TemporaryDirectory(dir=args.scratch) as scratch:
        for index, rel in enumerate(tracked_manifests(root)):
            tracked = (root / rel).read_text()
            module = header_value(tracked, "Module")
            source = header_value(tracked, "Source")
            if module is None or source is None:
                continue
            checked += 1
            out = Path(scratch) / f"{index}.psmeta"
            result = subprocess.run(
                [args.generator, source, module, str(out)],
                cwd=root, capture_output=True, text=True)
            if result.returncode != 0:
                failures.append(f"{rel}: generator failed: {result.stderr.strip()}")
            elif out.read_text() != tracked:
                failures.append(
                    f"{rel}: differs from `generate_stdlib_manifest {source} {module} {rel}`; "
                    "regenerate it with that command")
    if failures:
        print("stale stdlib symbol manifests:")
        for failure in failures:
            print("  " + failure)
        return 1
    if checked == 0:
        print("no generated stdlib manifests found")
        return 1
    print(f"stdlib manifests are fresh ({checked} checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
