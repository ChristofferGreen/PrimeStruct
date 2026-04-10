# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
Roadmap note: completed map/vector migration, compatibility-cleanup, graph queue, and semantics/lowering boundary slices now live in `docs/todo_finished.md`.
Sizing note: each leaf `○` item should fit in one code-affecting commit plus focused conformance updates for that slice. If a leaf needs multiple behavior changes, split it first.

**Group 13 - Ownership/runtime substrate**
- ◐ Route the remaining builtin canonical `map<K, V>` borrowed/non-local growth mutation surfaces through the shared grown-pointer write-back/repoint path.
  - ◐ Refresh section 3 of `docs/ownership_runtime_soa_touchpoints.md` with concrete still-unhandled non-local receiver families (one entry per family with minimal repro + owning source files).
  - ◐ Add one failing conformance case for the highest-priority unhandled non-local receiver family across native/C++/VM harnesses.
  - ◐ Migrate that single highest-priority non-local receiver family onto the shared rewrite/lowering path and flip the new conformance case to passing. Progress: direct + method helper-return map value receiver inserts and borrowed-holder field receiver inserts now rewrite to `/std/collections/map/insert_builtin` and run across VM/C++ emitter/native; section 3 currently reports no unhandled non-local receiver families in this inventory.

**Group 14 - SoA bring-up and end-state cleanup**
- ◐ Retire remaining compiler-owned builtin `soa_vector` semantics/lowering/backend scaffolding as the stdlib `.prime` substrate becomes authoritative.
  - ◐ Migrate the next compiler-owned SoA fallback family onto shared stdlib helper/conversion paths.
    - ◐ Refresh section 4 of `docs/ownership_runtime_soa_touchpoints.md` with concrete still-unhandled compiler-owned SoA fallback families (one entry per family with helper shape + owning files). Progress: section 4 now includes an explicit 2026-04 still-unhandled-family inventory (S1/S2/S3) covering struct-only non-empty `soa_vector` literal lowering, compiler-owned `to_aos` storage-layout bridge glue, and pending field-view diagnostic fallback paths with owning files.
    - ◐ Add one failing focused semantics/backend parity test for the highest-priority unhandled SoA fallback family. Progress: added a focused compile-run parity reject that locks root non-struct non-empty `soa_vector` literal behavior across semantic dump (`--dump-stage ast-semantic`) and emit (`--emit=exe`) paths with matching semantic-stage diagnostics.
    - ◐ Migrate that single highest-priority SoA fallback family onto shared stdlib helper/conversion paths and flip the new parity test to passing. Progress: canonical `to_aos_ref` now reuses the same SoA bridge-compat parameter matching as `to_aos` in inline parameter lowering (including specialized helper-path variants), and focused compile-run coverage now passes for root `soa_vector` receiver-location form `/std/collections/soa_vector/to_aos_ref<T>(location(values))` in the C++ emitter.
  - ◐ Remove one no-longer-needed compiler-owned SoA fallback branch for the current highest-priority migrated family. Progress: removed the inline-parameter `calleePath.empty()` acceptance branch from builtin `soa_vector` -> experimental `SoaVector__*` bridge matching so S2 bridge compatibility now requires canonical helper-path classification.
  - ◐ Add a guard test that fails if that removed branch behavior reappears. Progress: added a focused inline-parameter helper regression that asserts empty-callee-path calls reject builtin `soa_vector` -> `SoaVector__*` bridge matching while canonical `/std/collections/soa_vector/to_aos_ref` still succeeds.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution

P1 - Immediate peak-RSS reductions in existing pipeline

P2 - Traversal and allocation churn reductions
- ◐ [P2-01] Inventory snapshot passes by traversal shape and pick the highest-overlap pair.
- ◐ [P2-02] Merge one high-overlap snapshot pair into a shared traversal collector.
- ◐ [P2-03] Add output-order parity test for the first snapshot merge.
- ◐ [P2-04] Merge a second high-overlap snapshot pair into the shared collector path.
- ◐ [P2-05] Add output-order parity test for the second snapshot merge.
- ◐ [P2-06] Introduce reusable per-definition scratch storage for call-target resolution transients. Progress: `resolveExprConcreteCallPath(...)` now reuses per-definition PMR scratch (`CallTargetResolutionScratch::concreteCallBaseCandidates` + `overloadFamilyPathCache`) instead of rebuilding transient overload-family/candidate structures per call.
- ◐ [P2-07] Extend reusable per-definition scratch storage to method-target resolution transients.
- ◐ [P2-08] Replace repeated call-target path normalization/concatenation with cached canonical fragments. Progress: call-target concrete-path resolution now caches overload/specialization canonical fragments (`overloadFamilyPrefixCache`, `specializationPrefixCache`, and `overloadCandidatePathCache`) in `CallTargetResolutionScratch`, reusing them across scoped-owner call resolution instead of rebuilding `path + "__ov..."`/`path + "__t..."` strings per call.
- ◐ [P2-09] Extend cached canonical fragment reuse to method-target path construction. Progress: method-target receiver alias candidate construction now routes through `joinMethodTarget(...)` via a shared `appendCanonicalReceiverResolutionCandidates(...)` helper (`/vector`↔`/std/collections/vector`, `/map`↔`/std/collections/map`) so canonical helper fragments are reused through the scoped method-path cache instead of ad-hoc string concatenation.
- ◐ [P2-10] Add method-target memoization keyed by semantic-node identity + receiver type + method name and report RSS/time delta. Progress: method-target resolution now supports benchmark-controlled memoization A/B via `--benchmark-semantic-disable-method-target-memoization`, threading through options/pipeline/validator to bypass `methodTargetMemoCache` lookup/store when disabled; `scripts/semantic_memory_benchmark.py` now supports `--method-target-memoization on|off|both` and emits per-fixture/phase `method_target_memoization_deltas` (RSS/time on-minus-off medians + worst-case deltas).
- ◐ [P2-11] Replace `graphLocalAuto*` string-concatenated map keys with compact structured keys and report RSS delta. Progress: `GraphLocalAutoKey` now stores interned `scopePathId` instead of copied `scopePath` text, validator graph-local key maps reuse a dedicated scope-path interner, and benchmark A/B reporting now supports compact-vs-legacy key-shadow runs via `--benchmark-semantic-graph-local-auto-legacy-key-shadow` + `scripts/semantic_memory_benchmark.py --graph-local-auto-key-mode compact|legacy-shadow|both` with per-fixture/phase `graph_local_auto_key_mode_deltas` RSS/time deltas.
- ◐ [P2-12] Convert one high-churn semantic map/set from string keys to a flatter cache-friendly container and report RSS delta. Progress: collapsed graph-local auto side-channel state from nine per-field keyed maps into one `graphLocalAutoFacts_` map so each key is hashed/stored once and related fields are co-located; benchmark A/B reporting now supports flat-vs-legacy side-channel shadow runs via `--benchmark-semantic-graph-local-auto-legacy-side-channel-shadow` + `scripts/semantic_memory_benchmark.py --graph-local-auto-side-channel-mode flat|legacy-shadow|both`, with per-fixture/phase `graph_local_auto_side_channel_mode_deltas` RSS/time deltas.
- ◐ [P2-13] Prototype per-definition arena/pmr allocation for transient semantic maps/vectors and report RSS/time delta. Progress: graph-local auto dependency-count transient map now uses a PMR `monotonic_buffer_resource`-backed container (`GraphLocalAutoDependencyScratch`) with benchmark A/B toggle `--benchmark-semantic-disable-graph-local-auto-dependency-scratch-pmr`; `scripts/semantic_memory_benchmark.py` now supports `--graph-local-auto-dependency-scratch-mode pmr|std|both` and emits per-fixture/phase `graph_local_auto_dependency_scratch_mode_deltas` RSS/time deltas.
