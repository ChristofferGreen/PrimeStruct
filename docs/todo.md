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

- TODO-4026
- TODO-4036

### Immediate Next 10 (After Ready Now)

- TODO-4027
- TODO-4028
- TODO-4029
- TODO-4030
- TODO-4031
- TODO-4032
- TODO-4033
- TODO-4034
- TODO-4035
- TODO-4037

### Priority Lanes (Current)

- Stdlib surface-style alignment: TODO-4026 through TODO-4035
- Stdlib bridge consolidation: TODO-4036 through TODO-4041
- Vector/map stdlib ownership cutover: TODO-4042 through TODO-4051

### Execution Queue (Recommended)

1. TODO-4026
2. TODO-4036
3. TODO-4027
4. TODO-4028
5. TODO-4029
6. TODO-4030
7. TODO-4031
8. TODO-4032
9. TODO-4033
10. TODO-4034
11. TODO-4035
12. TODO-4037
13. TODO-4038
14. TODO-4039
15. TODO-4040
16. TODO-4041
17. TODO-4042
18. TODO-4051
19. TODO-4043
20. TODO-4044
21. TODO-4045
22. TODO-4046
23. TODO-4047
24. TODO-4048
25. TODO-4049
26. TODO-4050

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Stdlib surface-style alignment and public helper readability | TODO-4026, TODO-4027, TODO-4028, TODO-4029, TODO-4030, TODO-4031, TODO-4032, TODO-4033, TODO-4034, TODO-4035 |
| Stdlib bridge consolidation and collection/file/gfx surface authority | TODO-4036, TODO-4037, TODO-4038, TODO-4039, TODO-4040, TODO-4041 |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4042, TODO-4043, TODO-4044, TODO-4045, TODO-4046, TODO-4047, TODO-4048, TODO-4049, TODO-4050, TODO-4051 |
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
| CodeExamples-aligned stdlib surface syntax conformance | TODO-4026, TODO-4027, TODO-4028, TODO-4029, TODO-4030, TODO-4031, TODO-4032, TODO-4033, TODO-4034, TODO-4035 |
| Semantic-product publication parity and deterministic ordering | TODO-4039 |
| Lowerer/source-composition contract coverage | TODO-4040 |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4048, TODO-4049, TODO-4050, TODO-4051 |
| Focused backend rerun ergonomics and suite partitioning | none |
| Emitter map-helper canonicalization parity | TODO-4041 |
| VM debug-session argv lifetime coverage | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug numeric opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |

### Task Blocks

- [ ] TODO-4041: Retire duplicated collection helper compatibility tables after bridge parity lands
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - depends_on: TODO-4040
  - scope: Remove the remaining duplicated collection helper compatibility tables and canonical-to-experimental mapping lists once the shared stdlib bridge and semantic surface-ID flow cover the current vector/map behavior.
  - acceptance:
    - Duplicated collection helper compatibility tables that are superseded by the shared bridge are deleted from production code.
    - Collection helper compatibility behavior continues to resolve through the shared bridge/semantic ID path rather than scattered path tables.
    - Any residual collection compatibility rule that still cannot move onto the bridge is called out explicitly in source and left behind as a narrowly scoped follow-up.
  - stop_rule: Stop once the superseded collection compatibility tables are removed and the remaining collection path authority is centralized; split any stubborn edge-case migration into a separate leaf instead of keeping broad duplicate tables.

- [ ] TODO-4040: Replace lowerer stdlib string-path dispatch with semantic stdlib surface IDs
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - depends_on: TODO-4039
  - scope: Refactor lowering so stdlib-sensitive dispatch uses semantic stdlib surface IDs emitted by semantics rather than re-matching stdlib helper and method paths by string in the lowerer.
  - acceptance:
    - Production lowering paths that currently dispatch on stdlib helper strings consume semantic stdlib surface IDs instead.
    - Lowerer parity tests confirm that file/collection/gfx bridge-backed calls still lower to the same behavior after the dispatch swap.
    - No new production lowerer path matching against stdlib helper strings is introduced outside narrowly documented compatibility seams.
  - stop_rule: Stop once production lowerer stdlib dispatch is ID-based; if one backend family still needs temporary path fallback, split that residual fallback into an explicit follow-up item.

- [ ] TODO-4039: Publish resolved stdlib surface IDs into semantic output for lowering consumers
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - depends_on: TODO-4038
  - scope: Extend semantic resolution so bridge-resolved stdlib imports, helpers, and methods publish stable stdlib surface IDs that downstream lowering code can consume without reconstructing path knowledge.
  - acceptance:
    - Semantic output carries stable stdlib surface identifiers for the bridge-backed surfaces needed by lowering.
    - Those IDs are produced from shared bridge resolution rather than parallel lowerer-specific classification logic.
    - Source-lock or parity coverage documents the intended semantic publication boundary for stdlib surface IDs.
  - stop_rule: Stop once the semantic product exposes the stdlib IDs needed for lowering; split any broader semantic-product reshaping beyond that boundary into separate tasks.

- [ ] TODO-4038: Route file helper and error-surface method resolution through shared bridge queries
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - depends_on: TODO-4037
  - scope: Move file helper resolution and file/error-surface method targeting onto shared stdlib bridge queries so those surfaces no longer rely on scattered direct stdlib path matching in semantics.
  - acceptance:
    - File helper and error-surface method resolution uses shared bridge metadata instead of local hard-coded stdlib path tables.
    - Existing file/error helper diagnostics and successful resolution behavior remain covered by parity tests after the migration.
    - The refactor leaves runtime/backing behavior unchanged and limits itself to centralizing resolution authority.
  - stop_rule: Stop once file/error-surface method resolution is bridge-driven; split any unrelated API cleanup or runtime behavior work into separate leaves.

- [ ] TODO-4037: Route stdlib import alias construction through shared bridge metadata
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - depends_on: TODO-4036
  - scope: Refactor stdlib import alias construction so import-time alias exposure for file, collections, and gfx comes from shared bridge metadata instead of hand-maintained special cases spread through parser/semantics code.
  - acceptance:
    - Stdlib import alias construction for the bridged surfaces is driven by the shared bridge metadata.
    - Existing exact-import and wildcard-import alias behavior remains covered by focused parity tests.
    - Import alias behavior no longer requires duplicating the same stdlib path knowledge in multiple production files for the migrated surfaces.
  - stop_rule: Stop once import alias construction for the targeted stdlib surfaces is bridge-backed; split any packaging/root-discovery follow-up out of this item.

- [ ] TODO-4036: Add shared stdlib surface registry for file, collections, and gfx helper metadata
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Bridge Consolidation
  - scope: Introduce one shared stdlib bridge/registry module that declares the canonical metadata for file, collections, and gfx helper surfaces so later semantics and lowering work can consume one source of truth instead of scattered path tables.
  - acceptance:
    - A shared production module exists for stdlib surface metadata covering the initial file, collection, and gfx bridge targets.
    - The registry defines the canonical metadata needed for import aliasing, helper classification, compatibility spellings, and downstream lowering identifiers for the migrated surfaces.
    - Focused parity coverage or source-lock checks pin the intended scope of the bridge before follow-on migrations start consuming it.
  - stop_rule: Stop once the shared registry exists and describes the initial stdlib bridge surfaces well enough for follow-on migration tasks; do not widen this item into semantics or lowering rewrites.

- [ ] TODO-4035: Audit canonical /std/gfx wrappers for readable surface syntax without changing hybrid runtime boundaries
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Audit the canonical `/std/gfx` wrappers and convert their public-facing control flow, operators, and helper spellings to the example-style surface syntax without changing the hybrid runtime boundary or folding experimental/runtime internals into the same cleanup.
  - acceptance:
    - Canonical `/std/gfx` wrapper modules use the supported surface-form syntax for user-facing logic where that improves readability.
    - Runtime-boundary hooks, experimental gfx internals, and lowering/runtime ownership lines remain unchanged by this cleanup.
    - Any follow-up that would alter hybrid ownership or backend behavior is split into a separate TODO instead of being mixed into the style pass.
  - stop_rule: Stop once the canonical wrapper layer is style-aligned and any remaining backend-facing cleanup has been carved out into explicit follow-up work.

- [ ] TODO-4034: Audit /std/image for style-guide alignment and split style cleanup from codec-behavior changes
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Audit `/std/image` for places where public-facing library code still reads like canonical plumbing, then align that code to `docs/CodeExamples.md` style while keeping behavior-sensitive codec/parser changes isolated into separate follow-up work.
  - acceptance:
    - High-level `/std/image` library code that is user-facing adopts the supported surface-form syntax where it improves readability.
    - The style cleanup does not change codec behavior, file-format semantics, or error mapping as part of the same slice.
    - Any behavior-affecting codec or parser follow-up discovered during the audit is captured as separate TODO work rather than folded into the style cleanup.
  - stop_rule: Stop once the style-only `/std/image` cleanup boundary is clear and the public-facing layer is aligned without mixing in codec-behavior changes.

- [ ] TODO-4033: Audit /std/ui for example-style readability and convert internal collection workflows to preferred surface style
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Audit `/std/ui` for public and high-level internal code that still uses canonical helper-heavy spellings, then convert collection-heavy workflows to the preferred readable surface style shown in `docs/CodeExamples.md`.
  - acceptance:
    - `/std/ui` high-level flows use readable operators, control flow, and collection interactions where the language supports them.
    - Collection-heavy UI helpers prefer the intended surface syntax instead of canonical plumbing-style calls when readability improves.
    - Any UI cleanup that would require ownership, runtime, or behavior changes is split into a separate follow-up item.
  - stop_rule: Stop once the high-level `/std/ui` workflows read like standard library example code and any deeper architectural cleanup has been separated from the style pass.

- [ ] TODO-4032: Refactor stdlib error-helper modules to read like user-facing library code rather than canonical plumbing
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Rewrite public stdlib error-helper modules so they use the supported surface-form naming, control flow, and binding style expected of user-facing library code instead of canonical compiler-plumbing idioms.
  - acceptance:
    - Error-helper modules exposed through the stdlib read like ordinary library code rather than migration scaffolding or canonical lowering fixtures.
    - Helper naming, local bindings, and control flow in those modules align with `docs/CodeExamples.md` where the syntax is already supported.
    - The cleanup does not change public error domains or runtime behavior as part of the same slice.
  - stop_rule: Stop once the public error-helper layer is style-aligned; split any behavior or API changes into separate TODOs.

- [ ] TODO-4031: Normalize public stdlib helper naming to docs/CodeExamples conventions for free functions and member methods
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Align public stdlib helper names and call surfaces with the conventions demonstrated in `docs/CodeExamples.md`, prioritizing free functions and member methods that users encounter directly in high-level stdlib modules.
  - acceptance:
    - Public-facing stdlib helpers use naming that matches the example-style conventions instead of canonical or migration-oriented spellings.
    - Member-method versus free-function presentation is normalized where the existing language surface already supports the preferred form.
    - Compatibility shims or follow-up renames that need broader migration work are captured explicitly instead of being hidden in this cleanup.
  - stop_rule: Stop once public helper naming is normalized across the intended surface modules and any required compatibility migration has been split out.

- [ ] TODO-4030: Prefer member-style collection APIs over /std/collections helper calls in public stdlib modules
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Update public and high-level stdlib modules to prefer member-style collection calls over explicit `/std/collections/*` helper invocations where the language already supports the member-facing surface and the change improves readability.
  - acceptance:
    - Public/high-level stdlib code uses member-style collection APIs in place of direct helper-path calls where the semantics are already equivalent.
    - Remaining direct helper-path calls are limited to substrate, migration, or unsupported-language-surface cases and are documented as such in the touched modules.
    - The cleanup does not change collection semantics or ownership boundaries as part of the same slice.
  - stop_rule: Stop once user-facing collection usage is expressed in the preferred member style and only justified substrate-only helper calls remain.

- [ ] TODO-4029: Adopt concise inferred local bindings in high-level stdlib modules where initializers already show the type
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Replace verbose repeated-type local bindings in high-level stdlib modules with concise inferred bindings wherever the initializer already makes the type obvious and the style matches `docs/CodeExamples.md`.
  - acceptance:
    - High-level stdlib modules avoid redundant local type repetition when the initializer already establishes the type clearly.
    - The resulting bindings match the supported inferred-local style documented in `docs/CodeExamples.md`.
    - Any binding sites that need explicit types for readability, overload selection, or unsupported inference remain explicit with a short justification in code or commit context.
  - stop_rule: Stop once the obvious high-level inferred-binding opportunities are converted; leave substrate or behavior-sensitive cases explicit and separate.

- [ ] TODO-4028: Replace assign/plus/minus helper spelling in user-facing stdlib code with readable surface syntax
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Remove user-facing `assign(...)`, `plus(...)`, `minus(...)`, and similar helper-heavy spellings from public/high-level stdlib code in favor of the readable operator-based surface syntax already documented for users.
  - acceptance:
    - User-facing stdlib modules no longer rely on arithmetic or assignment helper spellings when equivalent readable surface syntax is supported.
    - Any remaining helper-form arithmetic or assignment is confined to substrate, canonical, or unsupported-syntax cases and is documented as intentional.
    - The cleanup does not mix in unrelated API or behavior changes.
  - stop_rule: Stop once public/high-level stdlib code uses readable surface arithmetic and assignment forms by default and only justified helper-form remnants remain.

- [ ] TODO-4027: Align public stdlib modules to surface-form operators and control flow
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - depends_on: TODO-4026
  - scope: Sweep the public-facing stdlib modules and convert their high-level logic to the supported surface-form operators and control-flow syntax described in `docs/CodeExamples.md`, without dragging substrate internals or unsupported language forms into the same change.
  - acceptance:
    - Public/high-level stdlib modules read like the documented language examples for operators, branching, and loop/control-flow spelling where the surface syntax is already supported.
    - Intentional canonical or substrate-only exceptions are limited to modules classified outside the public style-aligned surface.
    - The sweep leaves behavior unchanged and records any unsupported-syntax gaps as explicit follow-up work rather than silently keeping helper-heavy spellings.
  - stop_rule: Stop once the public/high-level stdlib surface follows the documented control-flow and operator style and any remaining unsupported cases are explicitly carved out.

- [ ] TODO-4026: Define and document which stdlib directories are style-aligned surface code versus intentionally canonical substrate code
  - owner: ai
  - created_at: 2026-04-19
  - phase: Stdlib Surface Style Alignment
  - scope: Establish and document the boundary between stdlib directories that should follow `docs/CodeExamples.md` surface-style conventions and directories that intentionally remain canonical/substrate-oriented so future cleanup work has an explicit target and stop line.
  - acceptance:
    - Repo docs clearly identify which `stdlib/std` directories are expected to read like user-facing surface code and which remain intentionally canonical or substrate-heavy.
    - The documented boundary is reflected in `docs/todo.md` task scoping so follow-on style cleanup items target the intended modules only.
    - Any ambiguous directories or modules are called out explicitly instead of being left to implicit judgment during later cleanup.
  - stop_rule: Stop once the stdlib surface-versus-substrate boundary is documented well enough that follow-on cleanup can proceed without re-litigating scope in each item.
