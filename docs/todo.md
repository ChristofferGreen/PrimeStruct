# PrimeStruct TODO Log

## Operating Rules (Human + AI)

1. This file contains only open work (`[ ]` or `[~]`).
2. Use one task block per item with a stable ID: `TODO-XXXX`.
3. Keep newest items at the top of `## Open Tasks`.
4. Every task must include:
   - clear scope
   - acceptance criteria
   - owner (`human` or `ai`)
5. When a task is completed:
   - mark it `[x]`
   - add `finished_at` and short evidence note
   - move the full block to `docs/todo_finished.md`
   - remove it from this file (do not keep completed tasks here)
6. `docs/todo_finished.md` is append-only history. Do not rewrite old entries except to fix factual mistakes.
7. Each implementation task SHOULD include `phase` and `depends_on` to keep execution order explicit.
8. Prefer small, testable tasks over broad epics; split before starting if acceptance cannot be verified in one PR.
9. Keep the `Execution Queue` and coverage snapshots current when adding/removing tasks.
10. Keep an explicit `Ready Now` shortlist synced with dependencies; only items with no unmet TODO dependencies belong there.
11. When splitting broad tasks, update parent task scope to avoid duplicated acceptance criteria across child tasks.
12. Keep `Priority Lanes` aligned with queue order so critical-path tasks remain visible.
13. For phase-level tracking tasks, pair planning trackers with explicit acceptance-gate tasks before marking a phase complete.
14. Every active leaf must include a stop rule and deliver at least one of:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
15. Avoid standalone micro-cleanups (alias renames, trivial bool rewrites, local dedup) unless bundled into one value outcome above.
16. If a leaf misses its value target after 2 attempts, archive it as low-value and replace it with a different hotspot.
17. Keep the live execution queue short: no more than 8 leaf tasks in `Ready Now` at once. Additional dependency-blocked or deferred follow-up leaves may stay in the task blocks and `Immediate Next 10`, but only `Ready Now` counts as the live queue cap.
18. Treat disabled tests as debt: every retained `doctest::skip(true)` cluster must either map to an active TODO leaf with a clear re-enable-or-delete outcome, or be removed once proven stale.

Status legend:
- `[ ]` queued
- `[~]` in progress
- `[x]` completed (must be moved to `docs/todo_finished.md`)

Task template:

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional but recommended)
  - depends_on: TODO-XXXX, TODO-YYYY (optional but recommended)
  - scope: ...
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now (Live Leaves; No Unmet TODO Dependencies)

- TODO-4101

### Immediate Next 10 (After Ready Now)

- TODO-4051
- TODO-4052

### Priority Lanes (Current)

- Vector/map bridge rollout and ownership cutover: TODO-4101, TODO-4051
- Stdlib de-experimentalization: TODO-4052 through TODO-4059

### Execution Queue (Recommended)

1. TODO-4101
2. TODO-4051
3. TODO-4052
4. TODO-4058
5. TODO-4053
6. TODO-4055
7. TODO-4054
8. TODO-4056
9. TODO-4057
10. TODO-4059

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4101, TODO-4051 |
| Stdlib de-experimentalization and public/internal namespace cleanup | TODO-4052, TODO-4053, TODO-4054, TODO-4055, TODO-4056, TODO-4057, TODO-4058, TODO-4059 |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | none |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debug trace replay robustness | none |
| VM/runtime debug numeric opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4101, TODO-4051 |
| De-experimentalization surface and namespace parity | TODO-4053, TODO-4054, TODO-4055, TODO-4056, TODO-4057, TODO-4058, TODO-4059 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/vector/*` and `/map/*` spellings,
  `vectorCount` / `mapCount`-style lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary until
  the later cutover TODOs retire them.
- Outside this lane: `array<T>` core ownership, `soa_vector<T>` maturity, and
  runtime/storage redesign remain separate boundaries and should not be folded
  into the vector/map bridge tasks below.

### Task Blocks

- [ ] TODO-4059: Remove completed experimental naming from docs and user-facing guidance
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4054, TODO-4056, TODO-4057, TODO-4058
  - scope: Remove stale experimental naming from public docs, examples, and user-facing guidance once the corresponding stdlib surfaces have been reclassified or renamed so the documented public contract no longer advertises transitional namespaces that are no longer meant for direct use.
  - acceptance:
    - Public docs and guidance no longer present completed collection/gfx/substrate de-experimentalization work as still experimental.
    - Any remaining intentionally incubating surface, especially SoA if still deferred, is documented explicitly rather than hidden behind blanket wording.
    - The resulting docs distinguish canonical public surfaces, compatibility shims, and internal substrate namespaces consistently.
  - stop_rule: Stop once the completed de-experimentalized surfaces are reflected in docs and any intentionally retained incubating surfaces are called out explicitly; split broader unrelated docs cleanup into separate tasks.

- [ ] TODO-4058: Decide and document the SoA maturity track separately from vector/map promotion
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4052
  - scope: Decide whether `soa_vector` remains an incubating surface with explicit experimental/internal backing or is ready for public promotion, and document the resulting boundary and rename policy separately from the vector/map cutover.
  - acceptance:
    - The repo explicitly states whether the SoA surface is still incubating or is on a promotion path independent of vector/map.
    - The decision identifies which SoA modules remain compatibility/internal-only versus which are public contract if any.
    - Follow-on rename or cleanup work for SoA is derived from that decision rather than assumed implicitly during collection de-experimentalization.
  - stop_rule: Stop once the SoA maturity decision and resulting namespace boundary are explicit; split any implementation-heavy promotion work into separate follow-up leaves.

- [ ] TODO-4057: Rename experimental substrate helpers to explicit internal namespaces instead of public-looking names
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4052, TODO-4054, TODO-4058
  - scope: Reclassify low-level substrate helpers such as buffer-checked/buffer-unchecked and SoA storage modules away from `experimental` naming toward explicit internal/substrate namespaces so they stop reading like candidate public stdlib APIs.
  - acceptance:
    - Low-level substrate modules no longer advertise themselves as public-facing experimental stdlib APIs purely by name.
    - The new naming makes their internal/substrate role explicit and leaves canonical public APIs routing through wrapper surfaces instead.
    - Any import, bridge, or docs updates needed by the rename stay limited to the targeted substrate helpers and do not fold in broader public API redesign.
  - stop_rule: Stop once the targeted substrate helper namespaces are clearly internal by name and usage; split any broader storage/runtime redesign into separate tasks.

- [ ] TODO-4056: Retire `/std/gfx/experimental/*` after compatibility-shim parity is proven
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4055
  - scope: Remove the public `/std/gfx/experimental/*` namespace once the canonical `/std/gfx/*` surface and bridge-backed lowering/method resolution paths cover the same behavior and any needed compatibility window has been exercised.
  - acceptance:
    - Canonical `/std/gfx/*` is the only remaining public gfx namespace for the retired experimental surface area.
    - Any temporary compatibility shim or import alias used during the transition is removed or narrowed to a clearly documented residual seam.
    - Gfx resolution/lowering parity coverage confirms that removing the experimental public namespace does not change supported behavior.
  - stop_rule: Stop once `/std/gfx/experimental/*` is no longer part of the public contract; split any backend-specific residual compatibility need into a separate follow-up item.

- [ ] TODO-4055: Collapse `/std/gfx/experimental/*` into a compatibility shim over canonical `/std/gfx/*`
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4052
  - scope: Refactor the gfx surface so `/std/gfx/*` is the only canonical public contract and `/std/gfx/experimental/*` becomes a temporary compatibility shim rather than a parallel first-class namespace.
  - acceptance:
    - Canonical `/std/gfx/*` is identified in code and docs as the authoritative public gfx surface.
    - The experimental gfx namespace, if retained temporarily, forwards through a compatibility layer instead of carrying a parallel independent public contract.
    - The migration leaves runtime and backend ownership boundaries unchanged while reducing duplicate public gfx namespace authority.
  - stop_rule: Stop once gfx has one canonical public namespace and any retained experimental path is clearly downgraded to compatibility-only status.

- [ ] TODO-4054: Convert `experimental_vector` and `experimental_map` into internal implementation modules
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4053, TODO-4101
  - scope: Reclassify the current experimental vector/map implementation namespaces as internal implementation modules or internal names once the canonical vector/map public contract and bridge-backed ownership cutover are complete.
  - acceptance:
    - `experimental_vector` and `experimental_map` no longer act as public transition namespaces for ordinary stdlib consumers.
    - Canonical `vector`/`map` continue to work through the same implementation behavior without requiring users or compiler paths to target the experimental namespaces directly.
    - Any rename or visibility change leaves internal implementation seams explicit instead of silently promoting them as new public API.
  - stop_rule: Stop once the experimental vector/map namespaces are no longer public contract; split any deeper implementation refactor beyond that visibility/name boundary into separate tasks.

- [ ] TODO-4053: Promote canonical `vector` and `map` as the sole public collection contracts
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4052, TODO-4101
  - scope: Make `/std/collections/vector/*` and `/std/collections/map/*` the only public collection contracts for the promoted surface area so users, docs, and compiler-facing authority no longer treat the experimental collection namespaces as peer public APIs.
  - acceptance:
    - Canonical vector/map namespaces are the only documented public collection contracts for the promoted collection surface.
    - Experimental collection namespaces are no longer required by user-facing docs, examples, or compiler path authority for ordinary vector/map behavior.
    - The promotion stays aligned with the completed bridge and ownership-cutover tasks instead of introducing a second compatibility layer.
  - stop_rule: Stop once canonical vector/map are the sole public collection contracts; split any remaining implementation visibility or rename work into separate follow-up tasks.

- [ ] TODO-4052: Define de-experimentalization policy and classify current stdlib experimental modules
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib De-Experimentalization
  - depends_on: TODO-4026, TODO-4036
  - scope: Define the repo-wide stdlib policy that distinguishes canonical public surfaces, compatibility shims, and internal substrate modules, then classify the current `experimental` stdlib modules against that policy so follow-on renames and removals have an explicit target.
  - acceptance:
    - The de-experimentalization policy explicitly separates public canonical API, temporary compatibility namespace, and internal substrate/helper namespace roles.
    - Current `stdlib/std` experimental modules are classified against that policy with enough precision to drive follow-on collection, gfx, SoA, and substrate cleanup tasks.
    - `docs/todo.md` and any touched design docs reflect that classification instead of relying on blanket “experimental” wording.
  - stop_rule: Stop once the policy and module classification are explicit enough to guide implementation; split any concrete rename/removal work into dedicated follow-up leaves.

- [ ] TODO-4051: Add vector/map bridge parity coverage for imports, rewrites, and lowering
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4042
  - scope: Add focused parity coverage that locks current vector/map import behavior, constructor/helper rewrites, compatibility diagnostics, and lowering behavior so the ownership-cutover work can centralize collection path authority without silent regressions.
  - acceptance:
    - Focused tests cover exact imports, wildcard imports, constructor/helper rewrite behavior, compatibility diagnostics, and representative lowering paths for `vector` and `map`.
    - The parity coverage is specific enough to detect behavior drift while the bridge-backed collection migration is in flight.
    - The new coverage is wired into the existing validation surfaces instead of remaining as ad hoc local-only checks.
  - stop_rule: Stop once vector/map bridge parity is pinned well enough that the migration tasks can safely delete duplicated path tables without guessing about current behavior.

- [ ] TODO-4101: Retire remaining semantics constructor-path tables after the lowerer bridge cutover
  - owner: ai
  - created_at: 2026-04-20
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: none
  - scope: Replace the remaining semantics/template-monomorph constructor-path switches and canonical-to-experimental constructor translation seams with shared bridge-backed constructor metadata now that the lowerer-side constructor tables have been centralized.
  - acceptance:
    - The remaining semantics/template constructor-path switches route through shared constructor metadata helpers instead of local vector/map argument-count ladders.
    - No broad vector/map-only canonical-to-experimental constructor translation tables remain outside the shared bridge helper layer.
    - Any residual constructor edge case that still cannot move is left behind with a narrowly scoped follow-up instead of preserving the old broad task.
  - stop_rule: Stop once the remaining semantics/template constructor-path tables are gone; split any stubborn residual edge case into an explicit follow-up rather than keeping another umbrella task alive.
