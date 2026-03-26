# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

- `experimental-vector-negative-index-guard`: `/std/collections/experimental_vector/*`
  checked access now rejects negative indices via `vectorCheckIndex` (same
  bounds-error path as high indices), and `vectorAtUnsafe` now also rejects
  negative indices explicitly, so negative indices no longer fall through to
  invalid pointer arithmetic/indirect-address traps.
- `experimental-vector-push-growth-overflow-contract`:
  `/std/collections/experimental_vector/vectorPush` now diverts capacity-
  doubling overflow into the shared runtime-contract path instead of letting
  the doubled capacity wrap or continue into unchecked growth.
- `map-bare-contains-canonical-helper-contract`: bare-root
  `contains(values, key)` on builtin `map<K, V>` now rejects with
  `unknown call target: /std/collections/map/contains` unless a canonical
  helper is imported or explicitly defined, matching the same-path contract
  already enforced for the surrounding bare-root map helpers.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
