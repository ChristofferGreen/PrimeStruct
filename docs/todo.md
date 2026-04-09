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
  - ○ Refresh section 3 of `docs/ownership_runtime_soa_touchpoints.md` with concrete still-unhandled non-local receiver families (one entry per family with minimal repro + owning source files).
  - ○ Add one failing conformance case for the highest-priority unhandled non-local receiver family across native/C++/VM harnesses.
  - ○ Migrate that single highest-priority non-local receiver family onto the shared rewrite/lowering path and flip the new conformance case to passing.

**Group 14 - SoA bring-up and end-state cleanup**
- ◐ Retire remaining compiler-owned builtin `soa_vector` semantics/lowering/backend scaffolding as the stdlib `.prime` substrate becomes authoritative.
  - ◐ Migrate the next compiler-owned SoA fallback family onto shared stdlib helper/conversion paths.
    - ○ Refresh section 4 of `docs/ownership_runtime_soa_touchpoints.md` with concrete still-unhandled compiler-owned SoA fallback families (one entry per family with helper shape + owning files).
    - ○ Add one failing focused semantics/backend parity test for the highest-priority unhandled SoA fallback family.
    - ○ Migrate that single highest-priority SoA fallback family onto shared stdlib helper/conversion paths and flip the new parity test to passing.
  - ○ Remove one no-longer-needed compiler-owned SoA fallback branch for the current highest-priority migrated family.
  - ○ Add a guard test that fails if that removed branch behavior reappears.

**Group 15 - Semantic memory footprint and multithread compile substrate**
Order rule: execute leaves top-to-bottom. Each leaf below is scoped to one code-affecting commit.

P0 - Reproducible measurement and attribution
- ◐ [P0-01] Add a minimized checked-in reproducer fixture derived from the high-RSS `/std/math/*` case as the primary memory regression target.
- ◐ [P0-02] Add benchmark fixtures for no-import and `/std/math/vector` semantic runs (`ast-semantic` + `semantic-product`).
- ◐ [P0-03] Add benchmark fixtures for `/std/math/vector+matrix` and `/std/math/*` semantic runs (`ast-semantic` + `semantic-product`).
- ◐ [P0-04] Add a large non-math include fixture with comparable definition count to separate math-specific behavior from include-scaling behavior.
- ◐ [P0-05] Add paired fixtures that compile identical math bodies via inline source vs stdlib imports.
- ◐ [P0-06] Add `1x/2x/4x` scale-step fixtures and report RSS/time slope.
- ◐ [P0-07] Add a benchmark helper that captures wall time + peak RSS per fixture and emits a machine-readable report.
- ◐ [P0-08] Extend the benchmark report with key-cardinality metrics (distinct call/method target keys and max key length).
- ◐ [P0-09] Run each fixture 3 times and report median + worst-case RSS/time.
- ◐ [P0-10] Add benchmark-only per-fact-family collection toggles so each collector can be enabled/disabled independently.
- ◐ [P0-11] Add benchmark-only semantic-validation-without-fact-emission mode for validator-vs-fact A/B attribution.
- ◐ [P0-12] Add benchmark-only semantic-product force on/off override for gate-effectiveness A/B runs.
- ◐ [P0-13] Record and check in an initial baseline report (fixture + phase + RSS/time).
- ◐ [P0-14] Mark semantic memory benchmark target as expensive + serial when runtime or RSS exceeds expensive-test thresholds.

P1 - Immediate peak-RSS reductions in existing pipeline
- ◐ [P1-01] Add explicit compile-pipeline `needs semantic product` decision for pure `ast-semantic` paths.
- ◐ [P1-02] Add phase-level assertions/counters proving semantic-product builder is skipped on `ast-semantic` paths.
- ◐ [P1-03] Add focused tests that semantic-product generation occurs only for `semantic-product` dumps and emit paths that consume it.
- ◐ [P1-04] Migrate one non-consuming backend IR-preparation entry point to skip semantic-product requests.
- ◐ [P1-05] Migrate a second non-consuming backend IR-preparation entry point to skip semantic-product requests.
- ◐ [P1-06] Add/refresh backend-entrypoint inventory (`docs/semantic_product_backend_entrypoint_inventory.md`) and append explicit one-leaf follow-ups for each remaining non-consuming backend family.
- ◐ [P1-06a] Migrate `tests/unit/test_ir_pipeline_helpers.h::parseAndValidateThroughCompilePipeline` to skip semantic-product requests when semantic output is not requested.
- ◐ [P1-06b] Add explicit semantic-product intent plumbing to `include/primec/testing/CompilePipelineDumpHelpers.h` so non-consuming dump families cannot regress into implicit semantic-product allocation.
- ◐ [P1-07] Replace per-module full-entry copies for one high-volume semantic-product fact family with canonical vector references/indices.
- ◐ [P1-08] Add formatter/update path that resolves module references without re-copying entries for that migrated fact family.
- ◐ [P1-09] Add semantic-product text-parity regression test for the first dedup slice.
- ◐ [P1-10] Migrate a second high-volume copied fact family to canonical reference/index storage.
- ◐ [P1-11] Add semantic-product text-parity regression test for the second dedup slice.

P2 - Traversal and allocation churn reductions
- ◐ [P2-01] Inventory snapshot passes by traversal shape and pick the highest-overlap pair.
- ◐ [P2-02] Merge one high-overlap snapshot pair into a shared traversal collector.
- ◐ [P2-03] Add output-order parity test for the first snapshot merge.
- ◐ [P2-04] Merge a second high-overlap snapshot pair into the shared collector path.
- ◐ [P2-05] Add output-order parity test for the second snapshot merge.
- ◐ [P2-06] Introduce reusable per-definition scratch storage for call-target resolution transients.
- ◐ [P2-07] Extend reusable per-definition scratch storage to method-target resolution transients.
- ◐ [P2-08] Replace repeated call-target path normalization/concatenation with cached canonical fragments.
- ◐ [P2-09] Extend cached canonical fragment reuse to method-target path construction.
- ◐ [P2-10] Add method-target memoization keyed by semantic-node identity + receiver type + method name and report RSS/time delta.
- ◐ [P2-11] Replace `graphLocalAuto*` string-concatenated map keys with compact structured keys and report RSS delta.
- ◐ [P2-12] Convert one high-churn semantic map/set from string keys to a flatter cache-friendly container and report RSS delta.
- ◐ [P2-13] Prototype per-definition arena/pmr allocation for transient semantic maps/vectors and report RSS/time delta.

P3 - Interning/ID migration for hot paths
- ◐ [P3-01] Add deterministic compilation-local string interning (`SymbolId`) with explicit lifetime/ownership rules.
- ◐ [P3-02] Add repeat-run determinism tests that identical single-thread inputs produce identical `SymbolId` assignments.
- ◐ [P3-03] Add worker-local interner API boundaries (no shared mutex in hot paths) plus deterministic merge hooks.
- ◐ [P3-04] Migrate call-target path keys from `std::string` to `SymbolId` while keeping diagnostics text unchanged.
- ◐ [P3-05] Migrate method-target keys from `std::string` to `SymbolId` while keeping diagnostics text unchanged.
- ◐ [P3-06] Migrate direct/method call-target semantic-product facts to ID-backed internal storage with unchanged dump output.
- ◐ [P3-07] Migrate one additional semantic-product fact family to ID-backed internal storage with unchanged dump output.
- ◐ [P3-08] Remove one transitional string shadow field whose owning family is fully migrated and parity-tested.
- ◐ [P3-09] Update migration inventory with explicit one-leaf follow-ups for each remaining fact family not yet ID-backed.
- ◐ [P3-10] Migrate `bridge_path_choices` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ◐ [P3-11] Migrate `callable_summaries` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ○ [P3-12] Migrate `binding_facts` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ○ [P3-13] Migrate `return_facts` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ○ [P3-14] Migrate `local_auto_facts` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ○ [P3-15] Migrate `try_facts` semantic-product strings to ID-backed internal storage with unchanged dump output.
- ○ [P3-16] Migrate `on_error_facts` semantic-product strings to ID-backed internal storage with unchanged dump output.

P4 - Deterministic multithread compile substrate
- ○ [P4-01] Write a semantics multithread design note for phase boundaries, immutable shared state, and thread-local mutable state.
- ○ [P4-02] Write deterministic ordering rules for per-worker interner merge (tie-breaks, stable keys, collision handling).
- ○ [P4-03] Implement two-worker interner merge helper with deterministic ID assignment.
- ○ [P4-04] Add two-worker interner merge determinism regression tests.
- ○ [P4-05] Extend interner merge helper from 2 workers to N workers while preserving deterministic IDs.
- ○ [P4-06] Add N-worker interner merge determinism regression tests.
- ○ [P4-07] Extract read-only definition index/prepass (symbols + resolved declarations) with no validator-state mutation.
- ○ [P4-08] Route single-thread definition validation through the read-only prepass and add parity tests.
- ○ [P4-09] Implement deterministic N-way definition partitioner and standalone stability unit tests.
- ○ [P4-10] Run two deterministic definition-validation work chunks in parallel behind a feature flag.
- ○ [P4-11] Extend worker scheduling from two chunks to N chunks via deterministic partitioner.
- ○ [P4-12] Add focused ordering tests that lock diagnostic and semantic-product output order under N-chunk scheduling.
- ○ [P4-13] Add single-thread vs multithread equivalence tests for diagnostics ordering and semantic-product output.
- ○ [P4-14] Add explicit worker-count equivalence tests for `1`, `2`, and `4` workers.
- ○ [P4-15] Add concurrency stress tests on high-definition-count imports (including `/std/math/*`) with deterministic-output assertions.
- ○ [P4-16] Add ThreadSanitizer multithread semantics smoke target (feature-flag on small deterministic fixture) and gate it in optional CI.

P5 - CI observability, policy, and completion gates
- ○ [P5-01] Add optional phase-level semantic counters (calls visited, facts produced, peak local map sizes), off by default.
- ○ [P5-02] Add optional phase-level allocation counters (allocation count + allocated bytes) for semantic validation/fact collection, off by default.
- ○ [P5-03] Add optional per-phase RSS checkpoints around semantic validation and semantic-product build, emitted in benchmark reports.
- ○ [P5-04] Add repeated-compile leak check (same fixture N times in one process) and report RSS drift.
- ○ [P5-05] Define per-fixture RSS/time regression budgets (tolerance + sustained-window rule) in a checked-in benchmark policy note.
- ○ [P5-06] Define phase-one success criteria tied to the checked-in baseline report (target RSS reduction or absolute cap on primary `/std/math/*` fixture).
- ○ [P5-07] Add CI trend tracking for semantic benchmark RSS/time with sustained-regression failure policy.
- ○ [P5-08] Upload semantic benchmark machine-readable reports as CI artifacts for all runs (especially failures).
