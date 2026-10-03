"""Shared exemption lookup for the collection surface audits (TODO-5417).

A source file is exempt from a surface audit when it is listed in
`scripts/surface_audit_exemptions.txt` (one `<path> <marker>[,<marker>...]` row
per file, `#` starts a comment) or - legacy fixtures only - carries a
`// <marker>-surface-audit: exempt` comment in its first ten lines. Markers in
the data file are the bare audit names: `vector`, `soa`, `map`, `collection`,
`map-vector-compiler-knowledge`. `collection` exempts a file from every audit.
"""

from __future__ import annotations

from pathlib import Path

DATA_FILE = "scripts/surface_audit_exemptions.txt"
INLINE_SUFFIX = "-surface-audit: exempt"
INLINE_EXTRA = {"map-vector-compiler-knowledge": "map-vector-compiler-knowledge: exempt"}

_cache: dict[Path, dict[str, set[str]]] = {}


def load(root: Path) -> dict[str, set[str]]:
    root = root.resolve()
    if root not in _cache:
        entries: dict[str, set[str]] = {}
        path = root / DATA_FILE
        if path.is_file():
            for raw in path.read_text(encoding="utf-8").splitlines():
                line = raw.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = line.split()
                if len(parts) != 2:
                    raise SystemExit(f"{DATA_FILE}: bad row (need '<path> <markers>'): {raw}")
                entries[parts[0]] = set(parts[1].split(","))
        _cache[root] = entries
    return _cache[root]


def has_data_file(root: Path) -> bool:
    return (root / DATA_FILE).is_file()


def inline_markers(text: str) -> set[str]:
    found: set[str] = set()
    for line in text.splitlines()[:10]:
        if not line.lstrip().startswith("//"):
            continue
        for name in ("vector", "soa", "map", "collection"):
            if name + INLINE_SUFFIX in line:
                found.add(name)
        for name, spelled in INLINE_EXTRA.items():
            if spelled in line:
                found.add(name)
    return found


def is_exempt(root: Path, rel_path: str, text: str, names: set[str]) -> bool:
    """True when `rel_path` carries any of `names` (or `collection`)."""
    have = set(load(root).get(rel_path, set())) | inline_markers(text)
    return bool(have & (set(names) | {"collection"}))
