# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

- `experimental-vector-negative-index-guard`: `/std/collections/experimental_vector/*`
  checked access now rejects negative indices via `vectorCheckIndex` (same
  bounds-error path as high indices) instead of falling through to invalid
  pointer arithmetic.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
