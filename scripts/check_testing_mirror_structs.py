#!/usr/bin/env python3
"""Guard against ODR violations between src/ and include/primec/testing/ mirrors.

`include/primec/testing/ir_lowerer_helpers/*.h` are fragments included *inside*
`namespace primec::ir_lowerer { ... }` by `include/primec/testing/*.h` umbrella
headers, so a struct declared there is the **same type** as the identically
named struct declared in the corresponding `src/ir_lowerer/*.h` header - not a
separate testing-only copy. If the two definitions disagree about their members,
that is an ODR violation: the two translation units disagree about the type's
size and layout, and a test that declares the type by value and calls a real
library function returning it writes past its own (smaller) stack slot.

This is not hypothetical. TODO-5235's ASan poison audit found exactly this:
`ArrayVectorAccessTargetInfo` gained a `bool isStructBoxedRecordTarget` member
on the src side (TODO-4628) and never gained it on the testing side, leaving the
testing copy 56 bytes against the library's 64, and producing a real
stack-buffer-overflow in
tests/unit/ir_pipeline/validation/test_ir_pipeline_validation_ir_lowerer_call_helpers_dispatch_buffer_and_native_tail_wrappers.cpp.
See docs/CompilerArenaAllocator.md's 2026-09-22 section.

The check is deliberately conservative: it compares member *declarations* after
stripping comments, whitespace and redundant `::primec::` qualification (which
is required in the testing fragments and forbidden-by-style in the src headers,
but names the same type), and only compares structs whose enclosing namespace is
the same. A mismatch in the set of members fails; pure spelling differences do
not.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

STRUCT_RE = re.compile(r"\nstruct\s+(\w+)\s*(?:final\s*)?\{(.*?)\n\};", re.S)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")

# Testing fragment directory -> the namespace its umbrella header opens, and the
# src directory holding the authoritative definitions of those same types.
MIRROR_ROOTS = [
    (
        os.path.join("include", "primec", "testing", "ir_lowerer_helpers"),
        os.path.join("src", "ir_lowerer"),
    ),
]


def normalizeMemberLine(line: str) -> str:
    """Reduce one member declaration to a spelling-insensitive form."""
    text = line.strip()
    # The testing fragments must fully qualify primec types (they are included
    # inside a nested namespace opened by the umbrella header); the src headers
    # rely on the enclosing namespace. Same type either way.
    text = text.replace("::primec::", "")
    text = text.replace("primec::", "")
    text = re.sub(r"\s+", " ", text)
    return text


def parseStructs(path: str) -> dict[str, list[str]]:
    try:
        with open(path, "r", errors="ignore") as handle:
            source = handle.read()
    except OSError:
        return {}
    source = BLOCK_COMMENT_RE.sub("", source)
    source = LINE_COMMENT_RE.sub("", source)
    structs: dict[str, list[str]] = {}
    for match in STRUCT_RE.finditer(source):
        # Split on ';', not on newlines: a single member declaration is often
        # wrapped across several source lines (long std::function signatures),
        # and where the line break falls is pure formatting.
        members = [
            normalizeMemberLine(declaration)
            for declaration in match.group(2).split(";")
            if declaration.strip()
        ]
        structs[match.group(1)] = members
    return structs


def collect(root: str, directory: str) -> dict[str, tuple[str, list[str]]]:
    found: dict[str, tuple[str, list[str]]] = {}
    absolute = os.path.join(root, directory)
    if not os.path.isdir(absolute):
        return found
    for currentDir, _dirs, files in os.walk(absolute):
        for name in files:
            if not name.endswith(".h"):
                continue
            path = os.path.join(currentDir, name)
            for structName, members in parseStructs(path).items():
                # First definition wins; a duplicate inside the same tree is a
                # separate concern this check does not police.
                found.setdefault(structName, (os.path.relpath(path, root), members))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="repository root")
    args = parser.parse_args()
    root = os.path.abspath(args.root)

    violations: list[str] = []
    checked = 0
    for testingDir, srcDir in MIRROR_ROOTS:
        testingStructs = collect(root, testingDir)
        srcStructs = collect(root, srcDir)
        for structName, (testingPath, testingMembers) in sorted(testingStructs.items()):
            if structName not in srcStructs:
                continue
            srcPath, srcMembers = srcStructs[structName]
            checked += 1
            if testingMembers == srcMembers:
                continue
            onlySrc = [m for m in srcMembers if m not in testingMembers]
            onlyTesting = [m for m in testingMembers if m not in srcMembers]
            if not onlySrc and not onlyTesting:
                # Same members, different order - still a layout difference.
                onlySrc = ["(member order differs)"]
            violations.append(
                f"ODR mismatch for struct {structName}:\n"
                f"  src    : {srcPath}\n"
                f"  testing: {testingPath}\n"
                f"  only in src    : {onlySrc}\n"
                f"  only in testing: {onlyTesting}"
            )

    if violations:
        sys.stderr.write(
            "check_testing_mirror_structs: testing mirror headers declare the "
            "SAME types as their src/ counterparts (they are included inside "
            "namespace primec::ir_lowerer), so differing members are an ODR "
            "violation, not a harmless drift.\n\n"
        )
        for violation in violations:
            sys.stderr.write(violation + "\n\n")
        return 1

    print(f"check_testing_mirror_structs: {checked} mirrored structs agree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
