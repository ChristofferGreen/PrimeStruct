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

- TODO-4042

### Immediate Next 10 (After Ready Now)

- TODO-4043
- TODO-4044
- TODO-4045
- TODO-4046
- TODO-4047
- TODO-4048
- TODO-4049
- TODO-4050

### Priority Lanes (Current)

- Vector/map stdlib ownership cutover: TODO-4042 through TODO-4051
- Stdlib de-experimentalization: TODO-4052 through TODO-4059

### Execution Queue (Recommended)

1. TODO-4042
2. TODO-4043
3. TODO-4044
4. TODO-4045
5. TODO-4046
6. TODO-4047
7. TODO-4048
8. TODO-4049
9. TODO-4050
10. TODO-4052
11. TODO-4058
12. TODO-4053
13. TODO-4055
14. TODO-4054
15. TODO-4056
16. TODO-4057
17. TODO-4059

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4042, TODO-4043, TODO-4044, TODO-4045, TODO-4046, TODO-4047, TODO-4048, TODO-4049, TODO-4050, TODO-4051 |
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
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4048, TODO-4049, TODO-4050, TODO-4051 |
| De-experimentalization surface and namespace parity | TODO-4053, TODO-4054, TODO-4055, TODO-4056, TODO-4057, TODO-4058, TODO-4059 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

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
  - depends_on: TODO-4053, TODO-4050
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
  - depends_on: TODO-4052, TODO-4050
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

- [ ] TODO-4050: Retire duplicated vector/map compatibility and canonical-to-experimental tables
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4049
  - scope: Delete the remaining vector/map-specific compatibility tables, constructor-path mappings, and canonical-to-experimental translation helpers once the shared bridge and semantic/lowerer surface-ID flow fully cover the collection behavior they currently protect.
  - acceptance:
    - Vector/map-specific compatibility and canonical-to-experimental tables that are superseded by the bridge-backed flow are removed from production code.
    - Collection behavior continues to resolve through shared bridge metadata and semantic IDs rather than residual vector/map-only translation helpers.
    - Any residual edge-case table that cannot yet move is left behind with a narrowly scoped follow-up instead of preserving a broad duplicate compatibility layer.
  - stop_rule: Stop once the superseded vector/map path-translation tables are gone; split any stubborn residual edge cases into explicit follow-up leaves instead of keeping general duplicate tables alive.

- [ ] TODO-4049: Replace lowerer collection path dispatch with semantic surface IDs
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4048
  - scope: Refactor the collection-sensitive lowering paths for `vector` and `map` so they dispatch on semantic collection surface IDs rather than reconstructing collection meaning from `/std/collections/...` helper paths.
  - acceptance:
    - Production lowerer logic for vector/map-sensitive calls dispatches on semantic collection surface IDs.
    - Lowering parity coverage confirms that vector/map behavior is unchanged after the dispatch swap.
    - New vector/map lowering logic does not add fresh string-path matching outside narrowly documented temporary seams.
  - stop_rule: Stop once vector/map lowerer dispatch is ID-based; if a backend still needs a temporary path fallback, split that backend-specific fallback into a separate task.

- [ ] TODO-4048: Publish resolved vector/map surface IDs into semantic output
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4047
  - scope: Extend semantic resolution for `vector` and `map` so bridge-resolved imports, constructors, and helpers publish stable collection surface IDs that lowerer code can consume directly.
  - acceptance:
    - Semantic output carries stable surface identifiers for the resolved vector/map operations needed by lowering.
    - The vector/map IDs are emitted from bridge-backed semantic resolution rather than a second lowerer-specific classifier.
    - Source-lock or parity coverage documents the intended publication boundary for vector/map semantic IDs.
  - stop_rule: Stop once semantics publishes the vector/map IDs needed for lowering; split any broader semantic-product restructuring beyond that boundary into separate tasks.

- [ ] TODO-4047: Move collection helper rewrites onto bridge-backed semantic queries
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4046
  - scope: Refactor vector/map helper rewrite passes so they consume bridge-backed semantic queries instead of maintaining local path lists and canonical-to-experimental translation logic inside the rewrite code.
  - acceptance:
    - Vector/map helper rewrites use shared bridge-backed semantic classification instead of private string/path tables.
    - Rewrite behavior remains covered by focused parity tests after the migration.
    - The refactor reduces duplicated collection path knowledge instead of merely moving the same lists into another rewrite-local helper.
  - stop_rule: Stop once vector/map rewrites are bridge-backed and parity-tested; split any rewrite family that still needs a separate migration seam into its own follow-up item.

- [ ] TODO-4046: Route vector/map helper compatibility resolution through the shared bridge
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4045
  - scope: Centralize vector/map helper-family matching, removed-helper diagnostics, and canonical-versus-compatibility spelling handling behind shared bridge queries so semantics stops carrying separate collection compatibility tables.
  - acceptance:
    - Vector/map helper compatibility resolution uses bridge metadata rather than duplicated collection descriptor arrays in production semantics code.
    - Removed-helper and compatibility diagnostics for vector/map remain stable under focused parity coverage.
    - Helper compatibility authority is centralized enough that later cleanup can delete the superseded vector/map tables.
  - stop_rule: Stop once vector/map helper compatibility resolution is bridge-backed; if one helper family still needs a temporary compatibility seam, split it into an explicit follow-up leaf.

- [ ] TODO-4045: Route collection constructor classification through the shared bridge
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4043
  - scope: Replace scattered canonical/experimental vector/map constructor path tables with bridge-backed constructor family classification so constructor handling no longer depends on hard-coded collection path helpers.
  - acceptance:
    - Vector/map constructor classification is driven by shared bridge metadata.
    - Constructor rewrite and resolution behavior remains stable under focused parity coverage.
    - Production collection constructor handling no longer requires duplicated local canonical-to-experimental path maps for the migrated constructor families.
  - stop_rule: Stop once vector/map constructor classification is bridge-backed and parity-tested; split any constructor family that still needs a temporary isolated mapping into a separate task.

- [ ] TODO-4044: Route vector/map import alias construction through collection bridge metadata
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4043
  - scope: Refactor vector/map import-time alias exposure so collection aliases come from the shared collection bridge metadata rather than exact-import and wildcard-import special cases scattered through semantics code.
  - acceptance:
    - Vector/map import alias construction is driven by shared collection bridge metadata.
    - Existing exact-import and wildcard-import collection alias behavior remains covered by focused parity tests.
    - The migrated collection import path no longer duplicates vector/map alias knowledge in multiple production files.
  - stop_rule: Stop once vector/map import alias construction is bridge-backed; split any broader packaging or stdlib-root-discovery work out of this item.

- [ ] TODO-4043: Add shared collection surface registry for vector and map
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4042, TODO-4036
  - scope: Extend the shared stdlib bridge with the canonical vector/map collection metadata needed for imports, constructors, helper families, compatibility spellings, removed spellings, and downstream semantic/lowering IDs.
  - acceptance:
    - The shared bridge contains explicit vector/map metadata rather than leaving collection specifics in scattered helper tables.
    - The registry is rich enough to back vector/map alias construction, constructor classification, helper compatibility resolution, and surface-ID publication in later slices.
    - Focused source-lock or parity coverage pins the intended vector/map bridge metadata surface before follow-on migrations consume it.
  - stop_rule: Stop once the shared bridge describes vector/map collection surfaces well enough for the follow-on migration tasks; do not widen this item into full semantics or lowering rewrites.

- [ ] TODO-4042: Define vector/map bridge scope and ownership boundary
  - owner: ai
  - created_at: 2026-04-19
  - phase: Vector/Map Stdlib Ownership Cutover
  - depends_on: TODO-4036
  - scope: Define exactly which vector/map imports, constructors, helper families, compatibility spellings, semantic operations, and lowering hooks belong to the vector/map ownership-cutover bridge contract, and call out what remains intentionally substrate-only or temporary.
  - acceptance:
    - The intended vector/map bridge contract is documented clearly enough that follow-on migration tasks can target the same surface area without re-litigating scope.
    - The documented boundary distinguishes public vector/map ownership behavior from temporary substrate or migration-only internals.
    - `docs/todo.md` reflects that boundary across the vector/map cutover tasks instead of leaving scope implicit.
  - stop_rule: Stop once the vector/map bridge boundary is explicit enough to guide implementation and validation slices; split any broader stdlib bridge scoping beyond vector/map into separate tasks.
