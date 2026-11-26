#!/usr/bin/env python3
"""Stamp semantic-memory report files with the sha256 of a budget policy (test helper)."""

import hashlib
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def main() -> int:
    digest = hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()
    for name in sys.argv[2:]:
        path = Path(name)
        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["policy_sha256"] = digest
        path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
