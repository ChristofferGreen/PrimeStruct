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
17. Keep the active queue short: no more than 8 live leaves at once.

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

### Ready Now (No Unmet TODO Dependencies)

1. TODO-0408
2. TODO-0401
3. TODO-0402
4. TODO-0405

### Immediate Next 10 (After Ready Now)

1. TODO-0409
2. TODO-0406
3. TODO-0403

### Priority Lanes (Current)

- P1 Collection stdlib ownership cutover (`vector`, `map`, `soa_vector`): TODO-0408, TODO-0409
- P2 SoA canonicalization + semantic memory/perf + multithread substrate + semantic-product boundary hardening: TODO-0401, TODO-0402, TODO-0405, TODO-0406
- P3 Queue/snapshot governance: TODO-0403

### Execution Queue (Recommended)

Wave A (collection stdlib ownership cutover):
1. TODO-0408
2. TODO-0409

Wave B (SoA completion):
1. TODO-0401

Wave C (semantic memory/perf):
1. TODO-0402
2. TODO-0405
3. TODO-0406

Wave D (queue hygiene):
1. TODO-0403

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Collection stdlib ownership cutover (`vector`, `map`, `soa_vector`) | TODO-0408, TODO-0409 |
| SoA bring-up and stdlib-authoritative `soa_vector` end-state cleanup | TODO-0401 |
| Semantic memory footprint and multithread compile substrate | TODO-0402 |
| Semantic-product contract/index boundary hardening | TODO-0405, TODO-0406 |
| TODO queue quality gates and dependency/coverage synchronization | TODO-0403 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Release gate (`./scripts/compile.sh --release`) discipline | TODO-0401, TODO-0402, TODO-0405, TODO-0406, TODO-0408, TODO-0409 |
| Collection conformance and alias-deletion checks (`vector`/`map`/`soa_vector`) | TODO-0408, TODO-0409 |
| Benchmark/runtime regression checks (`./scripts/benchmark.sh`) | TODO-0402 |
| Semantic-product contract/index and deterministic conformance checks | TODO-0405, TODO-0406 |
| TODO/open-vs-finished hygiene (`docs/todo.md` vs `docs/todo_finished.md`) | TODO-0403 |

### Task Blocks

- [ ] TODO-0409: Remove C++ name routing for `map` + `soa_vector`
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - depends_on: TODO-0408
  - scope: Complete stdlib-authoritative collection migration by routing `map` and `soa_vector` behavior through `.prime` definitions, then delete remaining production compiler/runtime C++ name/path alias logic for these collections in semantics/lowering (`src/` + `include/primec`, excluding tests/testing-only facades).
  - acceptance:
    - Production `src/` + `include/` collection routing no longer hardcodes `map`/`soa_vector` symbol/path aliases for normal call/access classification; dispatch is generic or intrinsic-driven.
    - Release-mode collection conformance/tests for `map` and `soa_vector` pass, and any changed diagnostics are updated via deterministic snapshots.
    - At least one real compatibility subsystem (legacy alias/canonicalization branch family) is deleted rather than renamed.
    - Focused release-mode suites pass for the migration surface: `./build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.ir.pipeline.validation`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.maps`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.native_backend.collections`, and `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.emitters.cpp`.
    - Production guardrail check confirms no hardcoded collection compatibility routing remains in semantics/lowerer/emitter code paths (for example via `rg` over `src/semantics`, `src/ir_lowerer`, and emitter sources), with any remaining mentions justified as canonical stdlib or test-only references.
    - Final release gate passes with `./scripts/compile.sh --release`.
  - stop_rule: If landing both collections together is too risky for one commit, split into TODO-0410 (`map`) and TODO-0411 (`soa_vector`) with independent acceptance before continuing.

- [~] TODO-0408: Make `vector` stdlib-authoritative and delete vector-specific C++ classifiers
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Move `vector` constructor/method/member behavior to stdlib `.prime` definitions and remove vector-specific production compiler/runtime C++ path/name checks in semantics plus IR lowerer classification/mutation helpers (`src/` + `include/primec`, excluding tests/testing-only facades).
  - acceptance:
    - `vector` semantics/IR behavior is resolved through standard symbol/type flow without dedicated vector-name branches for normal production call/access classification.
    - Release-mode vector semantics + compile-run suites pass after deleting vector-specific compatibility branches.
    - Removed code includes at least one explicit vector alias/canonical-path branch from lowerer/semantics helpers.
    - Focused release-mode suites pass for vector behavior and routing: `./build-release/PrimeStruct_semantics_tests --test-suite=primestruct.semantics.calls_flow.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.ir.pipeline.validation`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.vm.collections`, `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.native_backend.collections`, and `./build-release/PrimeStruct_backend_tests --test-suite=primestruct.compile.run.emitters.cpp`.
    - Tests cover both canonical helper success paths and removed-alias rejection paths (unknown-target/unknown-method diagnostics) so deleted compatibility routing cannot silently regress.
    - Final release gate passes with `./scripts/compile.sh --release`.
  - stop_rule: If shared classifier rewrites destabilize `map`/`soa_vector`, isolate the generic mechanism in this leaf and defer remaining shared deletions to TODO-0409.
  - notes: This leaf should leave a reusable generic path that TODO-0409 can apply to `map` and `soa_vector`.
  - progress: Removed semantic method-resolution fallback from canonical stdlib vector helpers back to `/vector/*` alias paths in `SemanticsValidatorExprMethodResolution.cpp`, and added semantics coverage for canonical helper success plus explicit `/vector/count` method-call rejection.
  - progress: Removed method-call fallback that treated `/std/collections/vectorCount` and `/std/collections/vectorCapacity` alias helpers as valid providers for `values.count()` / `values.capacity()`, and added semantics coverage that alias-only helpers now reject with `unknown method` diagnostics.
  - progress: Extended semantics coverage to include canonical `values.capacity()` success and explicit `values./vector/capacity()` alias rejection when only canonical stdlib helpers exist.
  - progress: Extended semantics coverage to include canonical `values.at(...)` / `values.at_unsafe(...)` success and explicit `values./vector/at(...)` / `values./vector/at_unsafe(...)` alias rejection when only canonical stdlib helpers exist.
  - progress: Removed alias-dependent gating from vector method prevalidation so canonical `/std/collections/vector/at` and `/std/collections/vector/at_unsafe` integer-index diagnostics stay enforced even when `/vector/*` aliases exist, with coverage for alias-shadow scenarios.
  - progress: Removed explicit `values./vector/...` method-namespace compatibility for vector `count`/`capacity`/`at`/`at_unsafe` receivers and added coverage that those method forms now reject with `unknown method` even when `/vector/*` helpers are declared.
  - progress: Pruned `/vector/count` and `/vector/capacity` alias-path checks from explicit vector count/capacity method-path validation and added alias-declared rejection coverage for `values./vector/at(...)` and `values./vector/at_unsafe(...)`.
  - progress: Removed production wrapper-routing classifiers that treated `/std/collections/vectorCount` and `/std/collections/vectorCapacity` as direct vector builtins in semantics + lowerer helper-resolution/monomorph compatibility paths; direct canonical handling now stays on `/std/collections/vector/count` and `/std/collections/vector/capacity` (with experimental-vector canonical rewrites preserved).
  - progress: Removed the remaining `/std/collections/vectorXxx` wrapper-name routing for vector access/mutator helpers in production lowerer helper classification plus semantics compatibility/monomorph bridge tables, so canonical `/std/collections/vector/*` and experimental-vector helper paths are the only direct vector helper routes.
  - progress: Deleted the last semantics-only wrapper compatibility surface that accepted `/std/collections/vectorAt`/`/std/collections/vectorAtUnsafe`, and removed the monomorph exception that allowed templated `auto` parameters for `/std/collections/vectorXxx` wrapper definitions.
  - progress: Removed production `/vector/count` and `/vector/capacity` alias routing from lowerer vector helper alias resolution/receiver-probe classification and from semantics builtin count-capacity dispatch return-kind + method-call handling, including pre-dispatch canonical rewrite from `/vector/count` to `/std/collections/vector/count`.
  - progress: Removed remaining semantics dispatch/setup compatibility scaffolding that depended on `/vector/count` alias canonicalization, restricted late-call vector count/capacity compatibility checks to canonical `/std/collections/vector/*` paths, and switched unresolved `capacity()` builtin promotion to canonical `/std/collections/vector/capacity`.
  - progress: Removed remaining emitter + lowerer count/capacity alias hardcoding for `/vector/*` in direct-call type inference and removed-helper inline filtering; canonical `/std/collections/vector/count` and `/std/collections/vector/capacity` are now the only explicit vector count/capacity helper paths in those production classifiers.
  - progress: Pruned vector count/capacity `/vector/*` alias entries from template-monomorph compatibility mapping and semantic compatibility descriptor tables (including removed-helper descriptor list), and removed method-target compatibility guards that still treated `/vector/count` and `/vector/capacity` as distinct paths.
  - progress: Removed remaining `/vector/at` and `/vector/at_unsafe` alias routing from semantics direct-fallback/return-kind dispatch, emitter vector-access type inference/classification, lowerer explicit-vector-access classifiers, and template-compatibility alias tables so vector access helper routing is canonicalized on `/std/collections/vector/at` and `/std/collections/vector/at_unsafe`.
  - progress: Removed remaining `/vector/*` mutator alias mapping (`push`/`pop`/`reserve`/`clear`/`remove_at`/`remove_swap`) from experimental-vector helper canonicalization and vector compatibility descriptor aliases, and pruned emitter/lowerer direct-call alias filters so production mutator routing stays on canonical `/std/collections/vector/*` helper paths.
  - progress: Removed another cross-stage `/vector/*` compatibility subsystem by deleting alias candidate expansion/fallback from template-monomorph, pre-dispatch return-kind inference, struct-return path helpers, and emitter metadata candidate lookup; direct `/vector/*` paths now stay isolated instead of auto-bridging to canonical `/std/collections/vector/*`.
  - progress: Pruned additional `/vector/*` compatibility fallback from semantics method-target alias candidate expansion plus lowerer/emitter setup helper path preference (`preferVectorStdlibHelperPath`, setup collection candidate builders, and receiver-target method lookup), so canonical `/std/collections/vector/*` is preferred without back-routing to `/vector/*`.
  - progress: Removed another struct-return/type-inference compatibility family that cross-expanded `/vector/*` and `/std/collections/vector/*` in semantics + emitter collection helper candidate builders and return-inference method-path selection, keeping direct `/vector/*` lookup isolated while canonical stdlib vector paths remain authoritative.
  - progress: Canonicalized struct-return method candidate routing in semantics + lowerer and aligned emitter vector-access candidate pruning to `/std/collections/vector/*`, removing remaining fallback branches that still treated `/vector/*` as an automatic bridge in this struct-return inference path family.
  - progress: Removed `/vector/*` alias fallback from effect-free collection method candidate selection and helper-path preference/candidate expansion, so this effect-free semantics path now prefers canonical `/std/collections/vector/*` without back-routing through compatibility aliases.
  - progress: Removed initializer-time fallback that rewrote unresolved canonical `/std/collections/vector/count|capacity` calls back to `/vector/*` during binding validation and local-auto call inference, and updated preferred initializer helper resolution to no longer prefer `/vector/*` aliases over canonical stdlib vector helpers.
  - progress: Removed remaining template-monomorph `/vector/*` helper-path auto-bridge in `preferVectorStdlibHelperPath`/`preferVectorStdlibTemplatePath` and aligned pointer-like call candidate expansion to keep explicit `/vector/*` isolated while canonical `/std/collections/vector/*` paths no longer back-expand to `/vector/*`.
  - progress: Removed another cross-stage canonical-backfallback compatibility family by deleting `/std/collections/vector/* -> /array/*` fallback from semantics, template monomorph, emitter, and lowerer helper-path preference/candidate builders, so explicit canonical vector helper lookup now stays isolated instead of downshifting to `/array/*`.
  - progress: Removed another multi-stage canonical-backfallback family by deleting remaining `/std/collections/vector/* -> /array/*` fallback branches in semantics pre-dispatch + struct-return inference, emitter metadata/setup return inference + collection type helpers, and lowerer setup-type helper resolution/candidate builders; canonical stdlib vector helper lookup now stays isolated through these inference/classification paths.
  - progress: Removed another vector alias-compatibility subsystem in expression helper resolution by making `preferredBareVectorHelperTarget` canonical-first, dropping canonical stdlib helper fallback checks that still treated `/vector/*` helper declarations as equivalent for experimental-vector access (`at`/`at_unsafe`), and deleting late `/vector/* -> /std/collections/vector/*` fallback rewrite from `resolveExprVectorHelperCall`.
  - progress: Removed a broad dead-branch compatibility cleanup family by deleting no-op `/vector/*` and `/std/collections/vector/*` isolation stubs from semantics/emitter/lowerer/template helper candidate and preference builders now that those branches no longer perform fallback mutations.
  - progress: Removed `/vector/*` alias fallback from collection-access method targeting by deleting experimental-vector access fallback branches that rewrote to `/vector/<helper>` in `SemanticsValidatorExprCollectionAccess.cpp`, and tightened collection-access validation to require canonical `/std/collections/vector/<helper>` visibility instead of accepting `/vector/<helper>` declarations.
  - progress: Removed another lowerer + semantic-product compatibility subsystem by deleting `/vector/*` fallback rewrites from bare vector helper emission (`count`/`capacity`/`at`/`at_unsafe`) and by pruning `/vector/*` bridge-path classification in semantics/lowerer snapshot plumbing so bridge choices now treat canonical `/std/collections/vector/*` and experimental-vector paths as authoritative.
  - progress: Removed another inference/emission compatibility subsystem by disabling std-namespaced vector access fallback to `/vector/*` definitions in infer dispatch setup, canonicalizing infer late-fallback vector helper routing directly to `/std/collections/vector/*`, and pruning rooted `/vector/*` mutator alias recognition from lowerer statement helper resolution and vector-flow receiver normalization paths.
  - progress: Removed another rooted `/vector/*` compatibility family by deleting alias-only vector helper acceptance in method-target resolution (including explicit helper-path compatibility and vector-family classifier checks), pruning `/vector/*` acceptance in late unknown-target vector helper fallback, dropping `/vector/at*` from lowerer removed-helper filtering, narrowing setup-type direct-call guardrails to canonical `/std/collections/vector/*`, and removing named-argument/body-argument scaffolding that still treated `/vector/*` as a required compatibility surface.
  - progress: Removed another semantics+emitter vector-mutator compatibility family by deleting explicit `/vector/*` mutator call/method path handling and std-namespaced-to-alias fallback from statement helper validation, restricting emitter explicit-request filtering to canonical `/std/collections/vector/*` helper paths, tightening emitter method-call count/capacity compatibility checks to canonical vector helpers, and pruning pre-dispatch normalization branches that still treated bare `vector/*` helper tokens as compatibility candidates.
  - progress: Removed another vector helper rewrite/classification compatibility subsystem by pruning `/vector/*` visibility preference branches from semantics vector helper rewrite + builtin-name checks (`preferredBareVectorHelperTarget`, bare-vector helper rewrites, and vector helper visibility guards) and deleting emitter method-resolution parsing/classification branches that still treated rooted `vector/*` method names as a removed-collection alias family.
  - progress: Removed another infer+monomorph rooted-vector compatibility family by dropping `vector/*` removed-helper parsing in collection compatibility resolution (`getVectorStatementHelperName`/`getDirectVectorHelperCompatibilityPath`) and template-monomorph alias normalization (`isExplicitRemovedCollectionMethodAlias`, `preferVectorStdlibTemplatePath`, `isExplicitCollectionCompatibilityAliasPath`), while aligning statement helper user-target filtering to canonical `/std/collections/vector/*` paths.
  - progress: Removed another named-argument compatibility fallback by stopping unresolved explicit `/vector/at` and `/vector/at_unsafe` alias calls from being classified as builtin access helpers in semantic named-argument validation, so removed aliases now surface unknown-target diagnostics instead of builtin named-argument diagnostics; added semantics + C++ emitter coverage for both aliases.
  - progress: Completed child slice `TODO-0412` (archived in `docs/todo_finished.md`): shared semantics/lowerer access/helper recognizers no longer classify removed rooted `/vector/*` helper spellings as builtin vector helper metadata, while canonical `/std/collections/vector/*` spellings stay accepted.
  - progress: Completed child slice `TODO-0413` (archived in `docs/todo_finished.md`): shared collection constructor recognizers no longer classify removed rooted `/vector/vector` as a builtin vector constructor, while the lowerer still accepts canonical `/std/collections/vector/vector`.
  - progress: Completed child slice `TODO-0414` (archived in `docs/todo_finished.md`): shared semantics/lowerer/emitter simple-call helpers no longer treat removed rooted `/vector/vector` as a builtin `vector(...)` token, while bare `vector(...)` calls stay accepted.
  - progress: Completed child slice `TODO-0415` (archived in `docs/todo_finished.md`): emitter vector-mutator helper classification no longer treats removed rooted `/vector/*` mutator spellings as builtin helpers, while canonical `/std/collections/vector/*` mutator paths stay accepted.
  - progress: Completed child slice `TODO-0416` (archived in `docs/todo_finished.md`): semantics removed-alias helper classifiers no longer parse removed rooted `/vector/*` call/method spellings, while the remaining `/array/*` and canonical `/std/collections/vector/*` helper checks stay covered.
  - progress: Completed child slice `TODO-0417` (archived in `docs/todo_finished.md`): lowerer count-access classifiers/helpers no longer parse rooted `/vector/count` as a vector helper alias or compatibility name, while canonical `/std/collections/vector/count` handling stays covered through bundled classifier tests.
  - progress: Completed child slice `TODO-0418` (archived in `docs/todo_finished.md`): lowerer setup-type collection helper classification no longer parses removed rooted `/vector/count` through `resolveVectorHelperAliasName(...)`/`getNamespacedCollectionHelperName(...)`, while canonical `/std/collections/vector/count` setup-type helper detection stays covered.
  - progress: Completed child slice `TODO-0419` (archived in `docs/todo_finished.md`): lowerer setup-type removed-method alias classification no longer treats rooted `/vector/count` as an explicit removed vector method alias, while `/array/count` and canonical `/std/collections/vector/count` remain covered.
  - progress: Completed child slice `TODO-0420` (archived in `docs/todo_finished.md`): semantics named-argument builtin validation no longer treats rooted `/vector/push|reserve|remove_at|remove_swap` spellings as statement-only builtin helpers, while canonical `/std/collections/vector/*` and `/array/*` named-argument mutator diagnostics remain covered.
  - progress: Completed child slice `TODO-0421` (archived in `docs/todo_finished.md`): lowerer setup-type return-kind resolution dropped the dead explicit vector access removed-alias lambda that still carried rooted `/vector/at*` branches, and focused helper coverage now locks rooted plus canonical explicit slash-method vector access rejection even when the receiver is a `vector<i32>(...)` constructor call.
  - progress: Completed child slice `TODO-0422` (archived in `docs/todo_finished.md`): lowerer setup-type receiver-target method lookup no longer lets rooted `/vector/count` slash-method paths win by same-path lookup on vector receivers, while `/array/count` compatibility and canonical `/std/collections/vector/count` lookup remain covered.
  - progress: Completed child slice `TODO-0423` (archived in `docs/todo_finished.md`): semantics method-target resolution and infer fallback no longer let explicit rooted `/vector/count` or `/vector/capacity` slash-method requests resolve on vector-family receivers, and focused semantics coverage now locks rooted slash-method rejection for wrapper-vector chains and method-call sugar auto inference.
  - progress: Completed child slice `TODO-0424` (archived in `docs/todo_finished.md`): semantics method-target resolution and infer fallback no longer let explicit rooted `/vector/at` or `/vector/at_unsafe` slash-method requests resolve on vector-family receivers, and focused semantics coverage now locks rooted access-alias rejection even when same-path alias helpers are declared.
  - progress: Completed child slice `TODO-0425` (archived in `docs/todo_finished.md`): semantics late unknown-target fallback no longer replays explicit rooted `/vector/at` or `/vector/at_unsafe` slash-method requests as bare access helpers, and focused semantics coverage now locks exact `unknown method: /vector/at*` diagnostics both with canonical-only helpers and with same-path alias helpers declared.
  - progress: Completed child slice `TODO-0426` (archived in `docs/todo_finished.md`): semantics late direct-collection fallback no longer lets unresolved explicit `/vector/at` or `/vector/at_unsafe` direct calls continue into downstream compatibility rewrites, and focused wrapper-vector coverage now locks exact `unknown call target: /vector/at` diagnostics while canonical direct `/std/collections/vector/at*` paths remain covered.
  - progress: Completed child slice `TODO-0427` (archived in `docs/todo_finished.md`): semantics collection-access builtin validation dropped its dead rooted `/vector/at*` exemption after earlier direct-call guards took ownership of those diagnostics, and focused direct-call field/tag coverage now locks exact rooted `unknown call target: /vector/at` results instead of generic fallback errors.
  - progress: Completed child slice `TODO-0428` (archived in `docs/todo_finished.md`): semantics named-argument builtin validation dropped its dead rooted `/vector/at*` carveout after earlier direct-call guards took ownership of those diagnostics, and focused named-argument coverage now locks exact rooted `unknown call target: /vector/at` rejection even when canonical `/std/collections/vector/at` helpers are declared.
  - progress: Completed child slice `TODO-0429` (archived in `docs/todo_finished.md`): semantics method-target resolution no longer lets rooted `./vector/count()` slash-methods fall back to receiver-native `count` methods on local `string`, `array`, or `map` values when no same-path `/vector/count` helper exists, and focused semantics coverage now locks exact receiver-specific `unknown method` diagnostics for those local receivers.
  - progress: Completed child slice `TODO-0430` (archived in `docs/todo_finished.md`): semantics method-target resolution no longer lets wrapper-call rooted `./vector/count()` slash-methods fall back to temporary receiver-native `count` methods on `string`, `array`, or `map` return values when no same-path `/vector/count` helper exists, and focused wrapper-temporary semantics coverage now locks exact receiver-specific `unknown method` diagnostics while allowing a receiver-compatible same-path array helper.
  - progress: Completed child slice `TODO-0431` (archived in `docs/todo_finished.md`): semantics method-target resolution no longer lets wrapper-call rooted `./vector/capacity()` slash-methods fall back to temporary receiver-native `capacity` methods on `string`, `array`, or `map` return values when no same-path `/vector/capacity` helper exists, and focused wrapper-temporary semantics coverage now locks exact receiver-specific `unknown method` diagnostics while allowing a receiver-compatible same-path array helper.
  - progress: Completed child slice `TODO-0432` (archived in `docs/todo_finished.md`): semantics direct-call count/capacity resolution no longer lets rooted `/vector/count(...)` treat a wrapper-returned builtin vector as an implicit builtin alias when no same-path `/vector/count` helper exists, and focused wrapper-temporary semantics coverage now locks exact rooted `unknown call target: /vector/count` rejection while allowing a receiver-compatible same-path vector helper.
  - progress: Completed child slice `TODO-0433` (archived in `docs/todo_finished.md`): semantics direct-call count resolution no longer lets rooted `/vector/count(...)` treat a local builtin vector binding as an implicit builtin alias when no same-path `/vector/count` helper exists, and focused semantics coverage now locks exact rooted `unknown call target: /vector/count` rejection while existing same-path helper coverage on builtin vector targets remains intact.
  - progress: Completed child slice `TODO-0434` (archived in `docs/todo_finished.md`): semantics direct-call count/capacity resolution no longer lets rooted `/vector/capacity(...)` treat local or wrapper-returned builtin vectors as implicit builtin aliases when no same-path `/vector/capacity` helper exists, and focused semantics coverage now locks exact rooted `unknown call target: /vector/capacity` rejection while same-path helper coverage remains intact on builtin vector targets.
  - progress: Completed child slice `TODO-0435` (archived in `docs/todo_finished.md`): direct auto-inference no longer treats rooted `/vector/count(...)` or `/vector/capacity(...)` calls on builtin vector targets as inferable builtin aliases when no same-path helper exists, and focused auto-inference semantics coverage now locks exact rooted `unknown call target` diagnostics while same-path helper inference remains covered.
  - progress: Completed child slice `TODO-0436` (archived in `docs/todo_finished.md`): direct auto-inference no longer lets rooted `/vector/count(...)` piggyback on receiver-native `count` for non-vector builtin targets when no same-path `/vector/count` helper exists, and focused auto-inference semantics coverage now locks exact rooted `unknown call target: /vector/count` diagnostics for map, string, and array receivers while same-path helper inference remains covered.
  - progress: Completed child slice `TODO-0437` (archived in `docs/todo_finished.md`): semantics direct-call count validation no longer lets rooted `/vector/count(...)` wrapper calls fall through to non-vector receiver-native `count` targets when no same-path `/vector/count` helper exists, and focused wrapper semantics coverage now locks exact rooted `unknown call target: /vector/count` diagnostics for map, string, and array wrapper receivers.
  - progress: Completed child slice `TODO-0438` (archived in `docs/todo_finished.md`): template monomorph experimental constructor rewriting no longer treats rooted `/vector` as equivalent to canonical `/std/collections/vector/vector`, and focused semantics plus source-lock coverage now keep canonical constructor-helper rewriting intact while rooted `/vector/vector(...)` stays on exact `unknown call target` diagnostics.
  - progress: Completed child slice `TODO-0439` (archived in `docs/todo_finished.md`): initializer graph-binding inference no longer treats rooted `/vector/vector(...)` spellings as collection-constructor bypasses, and focused semantics plus source-lock coverage now keep canonical stdlib auto-binding constructor cases intact while rooted alias auto bindings stay on exact `unknown call target: /vector/vector` diagnostics.
  - progress: Completed child slice `TODO-0440` (archived in `docs/todo_finished.md`): statement binding validation no longer treats canonical `/std/collections/vector/vector(...)` initializers as mismatches for explicit builtin `vector<T>` bindings, and focused semantics plus source-lock coverage now keep positional and named-argument explicit builtin-vector bindings on canonical stdlib constructor success while the removed special-case helper stays deleted.
  - progress: Completed child slice `TODO-0441` (archived in `docs/todo_finished.md`): build-call resolution no longer rewrites canonical `/std/collections/vector/vector(...)` calls onto legacy arity helpers like `/std/collections/vectorPair`, and focused semantics plus source-lock coverage now keep explicit canonical constructor helper definitions working while missing canonical constructors stay on exact `unknown call target: /std/collections/vector/vector` diagnostics even when arity helpers are present.
  - progress: Completed child slice `TODO-0442` (archived in `docs/todo_finished.md`): template monomorph type resolution no longer rewrites canonical `/std/collections/vector/vector(...)` calls onto legacy arity helpers like `/std/collections/vectorPair`, and focused type-resolution parity plus source-lock coverage now keep missing canonical constructors on exact `unknown call target: /std/collections/vector/vector` diagnostics even when vector arity helpers are defined in source.
  - progress: Completed child slice `TODO-0443` (archived in `docs/todo_finished.md`): build-initializer collection binding inference no longer force-classifies canonical `/std/collections/vector/vector(...)` calls as builtin `vector<T>` ahead of generic helper-return analysis, and focused semantics plus source-lock coverage now keep `[auto]` bindings on same-path templated vector-constructor helpers aligned with the helper’s actual return type.
  - progress: Completed child slice `TODO-0444` (archived in `docs/todo_finished.md`): collection-call inference no longer force-classifies canonical `/std/collections/vector/vector(...)` calls as builtin vector receivers/template sources, and focused semantics plus source-lock coverage now keep same-path constructor helpers on their actual non-vector method diagnostics instead of vector helper routing.
  - progress: Completed child slice `TODO-0445` (archived in `docs/todo_finished.md`): expression vector-helper method routing no longer force-classifies canonical `/std/collections/vector/vector<T>(...)` calls as vector-family receivers ahead of generic helper-return analysis, and focused semantics plus source-lock coverage now keep same-path templated constructor helpers on their actual non-vector method diagnostics instead of vector helper routing.
  - progress: Completed child slice `TODO-0446` (archived in `docs/todo_finished.md`): statement vector-helper receiver probing no longer force-classifies canonical `/std/collections/vector/vector<T>(...)` calls as vector-family receivers ahead of normal statement validation, and focused semantics plus source-lock coverage now keep explicit same-path `push` helpers working while bare `push(...)` on templated constructor helpers stays on `push requires vector binding` diagnostics instead of vector-helper routing.
  - progress: Completed child slice `TODO-0447` (archived in `docs/todo_finished.md`): expression fallback diagnostics no longer treat canonical `/std/collections/vector/vector(...)` calls as exempt “real vector ctors” inside `SemanticsValidatorExpr.cpp`, and focused semantics plus source-lock coverage now keep same-path templated constructor helpers on receiver-specific non-vector `capacity()` diagnostics instead of vector-specific fallback handling.
  - progress: Completed child slice `TODO-0448` (archived in `docs/todo_finished.md`): template-monomorph experimental constructor rewrites no longer redirect canonical `/std/collections/vector/vector(...)` calls onto `/std/collections/experimental_vector/vector`, and focused semantics plus source-lock coverage now keep canonical auto-binding constructor calls on their same-path helper return type even when an experimental constructor helper is also defined.
  - progress: Completed child slice `TODO-0449` (archived in `docs/todo_finished.md`): template-monomorph type resolution no longer carries a vector-specific canonical constructor fast-path in `rewriteCanonicalCollectionConstructorPath(...)`, and focused semantics plus source-lock coverage now keep canonical auto-binding constructor calls on the correct same-path overload family without that vector-only branch.
  - progress: Completed child slice `TODO-0450` (archived in `docs/todo_finished.md`): expression vector-helper method routing no longer force-classifies `/std/collections/experimental_vector/vector<T>(...)` calls as experimental-vector receivers ahead of generic helper-return analysis, and focused semantics plus source-lock coverage now keep same-path templated experimental constructor helpers on their actual non-vector method diagnostics instead of vector helper routing.
  - progress: Completed child slice `TODO-0451` (archived in `docs/todo_finished.md`): statement vector-helper receiver probing no longer force-classifies `/std/collections/experimental_vector/vector<T>(...)` calls as experimental-vector receivers ahead of normal statement validation, and focused semantics plus source-lock coverage now keep explicit same-path `push` helpers working while bare `push(...)` on templated experimental constructor helpers stays on `push requires vector binding` diagnostics.
  - progress: Completed child slice `TODO-0452` (archived in `docs/todo_finished.md`): collection-call inference no longer lets same-path canonical or experimental vector constructor helpers override an already-inferred non-collection return type, and focused semantics plus source-lock coverage now keep `.count()` on those helpers on receiver-specific non-vector diagnostics while native/C++ vector constructor conformance expectations were aligned with the current canonical helper surface.
  - progress: Completed child slice `TODO-0453` (archived in `docs/todo_finished.md`): template-monomorph type resolution no longer rewrites exact `import /std/collections/vector` bindings onto `/std/collections/vector/vector`, and focused semantics plus source-lock coverage now keep explicit `vector(...)` calls on exact-import `unknown call target: /std/collections/vector` diagnostics instead of silently re-entering canonical constructor routing.
  - progress: Completed child slice `TODO-0454` (archived in `docs/todo_finished.md`): expression method resolution no longer short-circuits canonical `./std/collections/vector/count(...)` slash-method calls on builtin vector receivers through a dedicated builtin-mismatch shim, and focused semantics plus source-lock coverage now keep same-path helper auto inference on the helper return type while surrounding return mismatches stay generic.
  - progress: Completed child slice `TODO-0455` (archived in `docs/todo_finished.md`): expression method resolution no longer treats canonical `./std/collections/vector/capacity(...)` slash-method calls on builtin vector receivers as a missing-helper special case, and focused semantics plus source-lock coverage now keep same-path helper auto inference on the helper return type while surrounding return mismatches stay generic.
  - progress: Completed child slice `TODO-0456` (archived in `docs/todo_finished.md`): method-target resolution no longer promotes canonical `./std/collections/vector/count()` and `./std/collections/vector/capacity()` slash-methods on builtin vector receivers to builtin targets before same-path resolution, and focused semantics plus source-lock coverage now keep rooted `/vector/*` helpers from satisfying canonical slash-method calls while leaving canonical access helper routing intact.
  - progress: Completed child slice `TODO-0457` (archived in `docs/todo_finished.md`): method-target resolution no longer promotes canonical `./std/collections/vector/at()` and `./std/collections/vector/at_unsafe()` slash-methods on builtin vector receivers to builtin targets before same-path resolution, and focused semantics plus source-lock coverage now keep same-path helper inference on the helper return type while preserving the separate imported-helper fallback for unresolved canonical access routing.
  - progress: Completed child slice `TODO-0458` (archived in `docs/todo_finished.md`): method-target resolution no longer treats explicit canonical slash-method `./std/collections/vector/at()` and `./std/collections/vector/at_unsafe()` requests as builtin imports when only the imported stdlib helper is available, and focused semantics plus source-lock coverage now keep those explicit method forms on imported-helper validation while bare imported access routing remains intact.
  - progress: Completed child slice `TODO-0459` (archived in `docs/todo_finished.md`): method-target resolution no longer treats bare imported `values.at(...)` and `values.at_unsafe(...)` vector access methods as builtin fallbacks for `/std/collections/vector/at*`, and focused semantics plus source-lock coverage now keep imported vector method-call access on ordinary helper validation for both local and wrapper vector receivers.
  - progress: Completed child slice `TODO-0460` (archived in `docs/todo_finished.md`): method-target resolution no longer lets wrapper-returned non-vector receivers satisfy rooted `./vector/count()` slash-methods through same-path helper routing, and focused semantics plus C++ emitter source-lock coverage now keep wrapper `string`/`map`/`array` receivers on receiver-specific `unknown method` diagnostics even when `/vector/count(...)` helpers are declared.
  - progress: Completed child slice `TODO-0461` (archived in `docs/todo_finished.md`): method-target resolution no longer lets wrapper-returned non-vector receivers satisfy rooted `./vector/capacity()` slash-methods through same-path helper routing, and focused semantics plus C++ emitter source-lock coverage now keep wrapper `string`/`map`/`array` receivers on receiver-specific `unknown method` diagnostics even when `/vector/capacity(...)` helpers are declared.
  - progress: Completed child slice `TODO-0462` (archived in `docs/todo_finished.md`): method-target resolution no longer lets local non-vector receivers satisfy rooted `./vector/count()` slash-methods through same-path helper routing, and focused semantics plus C++ emitter source-lock coverage now keep local `string`/`map`/`array` receivers on receiver-specific `unknown method` diagnostics even when `/vector/count(...)` helpers are declared.
  - progress: Completed child slice `TODO-0463` (archived in `docs/todo_finished.md`): method-target resolution no longer lets local non-vector receivers satisfy canonical `./std/collections/vector/capacity()` slash-methods through same-path helper routing, and focused semantics plus C++ emitter source-lock coverage now keep local `string`/`map`/`array` receivers on receiver-specific `unknown method` diagnostics even when canonical `/std/collections/vector/capacity(...)` helpers are declared.
  - progress: Completed child slice `TODO-0464` (archived in `docs/todo_finished.md`): method-target resolution no longer lets local non-vector receivers satisfy canonical `./std/collections/vector/count()` slash-methods through same-path helper routing, and focused semantics plus C++ emitter source-lock coverage now keep local `string`/`map`/`array` receivers on receiver-specific `unknown method` diagnostics even when canonical `/std/collections/vector/count(...)` helpers are declared.

- [ ] TODO-0406: Split production semantics APIs from testing snapshots
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - depends_on: TODO-0405
  - scope: Move snapshot-for-testing surface area out of production semantics interfaces and into explicit `include/primec/testing/*` facades so runtime/compiler consumers only depend on production contracts.
  - acceptance:
    - Production-facing semantics headers stop exposing snapshot-only structs/methods used exclusively by tests.
    - Existing semantics/IR snapshot tests compile and pass through testing facades without adding new `tests -> src` include-layer allowlist entries.
    - At least one snapshot-only compatibility path in production semantics code is deleted rather than relocated.
  - stop_rule: If removal breaks unrelated runtime/backend callsites, split into two leaves (facade introduction first, production API pruning second) before continuing.

- [~] TODO-0405: Introduce unified semantic-product index builder
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - depends_on: TODO-0404
  - scope: Replace ad-hoc lowerer semantic-product lookup-map construction with one typed `SemanticProductIndex` builder and shared lookup API for direct/method/bridge/binding/local-auto/query/try/on_error families.
  - file_todos:
    - [x] `TODO-0405-F01` files: `/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererSemanticProductTargetAdapters.{h,cpp}`, `/Users/chrgre01/src/PrimeStruct/include/primec/testing/ir_lowerer_helpers/IrLowererSemanticProductTargetAdapters.h`; fix: introduce a typed `SemanticProductIndex` plus `SemanticProductIndexBuilder`, wire adapter construction through that builder for direct/method/bridge/binding/local-auto/query/try/on_error family indices, and route semantic lookup helpers through shared index-backed lookup paths.
    - [x] `TODO-0405-F02` files: `/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererSemanticProductTargetAdapters.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_graph_contexts.h`; fix: remove transitional duplicated legacy adapter-map population/fallback paths and migrate downstream checks to consume `SemanticProductIndex` directly.
    - [x] `TODO-0405-F03` files: `/Users/chrgre01/src/PrimeStruct/tests/unit/test_ir_pipeline_backends_registry.cpp`, `/Users/chrgre01/src/PrimeStruct/tests/unit/test_semantics_definition_partitioner.cpp`; fix: add/refresh deterministic parity coverage across worker counts (`1`, `2`, `4`) for the unified index path.
  - acceptance:
    - Lowerer semantic-product adapter paths consume one shared index builder instead of duplicating per-family map assembly logic.
    - Determinism parity coverage keeps semantic-product output and diagnostics stable across worker counts (`1`, `2`, `4`) on existing stress fixtures.
    - Semantic memory benchmark reports show non-regression for the semantic-product/lowering path, with measurable allocation or runtime improvement in at least one tracked fixture.
  - stop_rule: If measurable benchmark impact is not achieved after two attempts, archive this leaf as low-value per rule 16 and replace it with a different Group 15 hotspot.

- [ ] TODO-0403: Keep queue, lanes, and snapshots synchronized
  - owner: ai
  - created_at: 2026-04-13
  - phase: Cross-cutting
  - depends_on: TODO-0401, TODO-0402
  - scope: Keep this TODO log internally consistent as Group 14/15 leaf tasks are added, split, completed, and archived.
  - acceptance:
    - Every TODO ID listed in `Ready Now`, `Immediate Next 10`, `Priority Lanes`, and `Execution Queue` has a matching task block.
    - Archived IDs are removed from open sections in the same change that moves their task blocks to `docs/todo_finished.md`.
    - Coverage snapshot tables reflect only currently open IDs.
  - stop_rule: If synchronization touches more than one logical workstream, split updates into focused queue-only follow-up tasks.

- [ ] TODO-0402: Group 15 completion tracker (semantic memory + multithread substrate)
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 15
  - scope: Define and execute remaining measurable leaf slices after archived P0/P1/P2 work to ship memory/runtime wins with strict regression control.
  - acceptance:
    - Remaining Group 15 leaf tasks are listed with stable IDs and explicit metric baselines/targets.
    - Each new leaf includes concrete validation commands, including release build/test and focused benchmark or memory checks.
    - Completed Group 15 leaves are moved to `docs/todo_finished.md` with evidence notes.
  - stop_rule: If a leaf cannot demonstrate measurable impact within one code-affecting commit, split it into smaller measurable leaves before implementation.
  - notes: Prior Group 15 slices `[P0-17]` through `[P0-28]`, `[P1-01]` through `[P1-03]`, and `[P2-14]` through `[P2-42]` are already archived in `docs/todo_finished.md` (April 12-13, 2026).

- [ ] TODO-0401: Group 14 completion tracker (SoA end-state cleanup)
  - owner: ai
  - created_at: 2026-04-13
  - phase: Group 14
  - scope: Track and close Group 14 child leaves required to finish stdlib-authoritative collection behavior and remove compatibility scaffolding; implementation work lives in child TODOs.
  - acceptance:
    - Active Group 14 implementation leaves (currently TODO-0408 and TODO-0409) stay explicit with bounded scope, dependencies, and verification steps.
    - This tracker is only completed after all Group 14 child leaves are archived in `docs/todo_finished.md` with evidence notes.
    - At least one leaf is always `Ready Now` with no unmet TODO dependencies.
  - stop_rule: If a leaf is too large for one commit plus focused conformance updates, split it before implementation.
  - notes: Current active Group 14 child leaves are TODO-0408 and TODO-0409. Prior Group 14 slices `[S2-01]` through `[S2-05]`, `[S3-01]` through `[S3-132]`, and `[S4-01a1]` through `[S4-03]` are already archived in `docs/todo_finished.md` (April 12-13, 2026).
