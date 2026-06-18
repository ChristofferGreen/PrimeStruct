# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md`.
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. Treat failing release-test cases as the top priority queue item: before
    starting new implementation work, update `docs/failing_tests.md`, fix the
    oldest reproducible failure first, and keep `docs/todo.md` aligned with the
    active test-fix work.
14. Every release test run must record any failing cases in
    `docs/failing_tests.md` before broader work continues.
15. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block to `docs/todo_finished.md`, and remove
    it from this file.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now

- TODO-4609: Reject escaping local array slices | track: array-slice-escape-diagnostics | surface: slice view lifetime diagnostics
- TODO-4610: Add forward cursor traversal API | track: cursor-forward-traversal | surface: forward cursor traversal

### Immediate Next 10

- TODO-4611: Add reverse cursor traversal API
- TODO-4612: Add safe extent and cursor code examples
- TODO-4637: Move `ir_pipeline` test shard into subdirectory

### Priority Lanes

- Scene graph renderer and UI presentation: TODO-4565 completed the data-only
  scene model and TODO-4566 completed the first BGRA8 2D primitive renderer;
  TODO-4567 completed the first globally lit 3D SDF widget primitive, and
  TODO-4595 completed deterministic shaped glyph runs. TODO-4596 completed
  deterministic text atlas/raster composition. TODO-4568 completed the first
  UI scene-record adapter, and TODO-4569 completed the software-surface UI
  presentation bridge.
- Map/vector compiler-independence: TODO-4570 retired the duplicate `map2`
  surface, TODO-4571 added the compiler-knowledge inventory categories that
  guide deletion scope, and TODO-4573 removed compiler-owned map literal
  lowering. TODO-4575 removed map helper/access classifiers, and vector path
  TODO-4572 and TODO-4574 completed the public helper classifier deletions.
  TODO-4576 and TODO-4577 removed map/vector backing classifiers. TODO-4578
  was split into TODO-4597 registry foundation plus TODO-4598, TODO-4599, and
  TODO-4600 subsystem migrations; TODO-4597 completed the generic registry
  IDs, TODO-4598 completed the semantics migration, TODO-4599 completed the
  emitter migration, TODO-4600 completed the IR-lowerer migration, and
  TODO-4601 removed the final map-helper classifier trace. TODO-4602 removed
  semantic vector-literal diagnostic traces, TODO-4603 completed the
  IR-lowerer vector-literal cleanup, and TODO-4579 wired the broad zero audit
  into release validation.
- Architecture hardening backlog: TODO-4586 completed parser diagnostic
  stability tiers. TODO-4587 completed the shared compile-time/runtime VM
  kernel boundary. TODO-4588 added the IR-preparation phase manifest.
  TODO-4589 added the architecture health dashboard. TODO-4594 completed the
  semantic unknown-call diagnostic stability slice. TODO-4616 made the
  semantic validation manifest executable. TODO-4619 completed the runtime
  reflection backend-profile capability gate. TODO-4620 completed indexed
  expanded-source diagnostic lookup.
- Architecture review hardening: TODO-4613 through TODO-4616 retired the
  temporary semantic/lowerer/emitter source locks and made the semantic
  validation manifest executable. TODO-4619 completed the second backend
  capability gate, and TODO-4620 completed deterministic indexed
  expanded-source diagnostic lookup. TODO-4617 completed preflight
  stale/missing diagnostics, TODO-4618 completed CT-eval
  requirement-predicate fail-closed coverage, and TODO-4621 completed one
  lowerer/backend variadic diagnostic stability-tier promotion.
- Safe array extents and capability views: TODO-4604 completed the requirement
  contract phase split, TODO-4622 implemented the first contract-form
  `require(...)` runtime slice (integer-parameter and `count(parameter)`
  comparisons lower to deterministic call-boundary checks), and TODO-4605
  completed the non-null safe pointer optionality model. TODO-4606 specified the capability-parameterized
  reference/slice view model in the normative docs. TODO-4607 published the
  initial semantic-product array extent facts, and TODO-4608 added the first
  checked read-only array slice construction surface. TODO-4609 through
  TODO-4612 remain from the agreed backlog in `docs/SafeArrayExtentViews.md`:
  conservative view escapes, cursor traversal with
  `limit(...)` / `reverse_limit(...)` boundaries, and style-aligned examples
  once the surface is specified.
- Collections naming and surface-manifest retirement: remove the
  `experimental_*` and `internal_*` module-naming layers from
  `stdlib/std/collections` and retire `stdlib/std/collections/surfaces.psmeta`.
  The canonical `Vector`/`SoaVector` type identities still live in the
  `experimental_vector`/`experimental_soa_vector` namespaces, and roughly 45
  C++ files hardcode `experimental_` path literals plus 32 more for
  `internal_`, so the sequence is: TODO-4623 deleted the comment-only retired
  stubs, TODO-4624 added the shared `StdlibCollectionPaths.h` constants
  header with a pilot consumer, and TODO-4625 through TODO-4627 migrated the
  semantics, IR-lowerer, and emitter/pipeline literals so production C++ has
  no collection path literals outside the constants header; next, move the
  type identities to canonical
  namespaces and delete the experimental shims (TODO-4628 moved the Vector
  identity to `/std/collections/vector/Vector` and TODO-4629 moved the
  SoaVector identity to `/std/collections/soa/SoaVector`, and TODO-4630 deleted the
  deletable shims),
  collapse the `internal_*` modules into their public modules with visibility
  instead of naming as the boundary (TODO-4631 through TODO-4634 done), and finally
  derive the surface registry from stdlib declarations and delete the psmeta
  manifest (TODO-4635, TODO-4636).
- File layout restructuring: restructure the flat file layouts in
  `tests/unit/` (523 files), `include/primec/` (67 headers), and the
  top-level `src/` directory (~20 loose files). Phase 1 moves test shards
  into subdirectories mirroring source module structure (TODO-4637 through
  TODO-4640). Phase 2 groups headers by pipeline stage (TODO-4641). Phase 3
  consolidates loose src files (TODO-4642). Full design document at
  `docs/FileLayoutRestructuring.md`.
- Test name quality: improve test file and test case naming across the
  suite. Rename 63 opaque letter-suffixed shard files to topic-descriptive
  names (TODO-4647). Fix 8 duplicate test names (TODO-4643). Rewrite 53
  overlong names (TODO-4644). Drop ~740 redundant `compiles and runs`
  prefixes (TODO-4645). Tighten 12 vague short names (TODO-4646). Full
  analysis at `docs/FileLayoutRestructuring.md`.
- Oversized file refactoring: split files that are too large for
  maintainable development. Split `SemanticsValidate.cpp` (8,025 lines)
  into focused compilation units (TODO-4648). Convert IR lowerer include-only
  `.h` fragments to compileable `.h/.cpp` pairs (TODO-4649). Convert
  `TemplateMonomorph*.h` semantics fragments (TODO-4650). Split oversized
  test files (TODO-4651) and oversized single test case bodies (TODO-4652).
  Full analysis at `docs/FileLayoutRestructuring.md`.
- Test coverage and stdlib quality: add dedicated IrPrinter unit tests
  (TODO-4653). Add `[public]` annotations to style-aligned stdlib modules
  (TODO-4654). Add compile-run tests for all language level examples
  (TODO-4655). Full analysis at `docs/FileLayoutRestructuring.md`.
- Collection decoupling: move hardcoded collection knowledge from C++ to
  .prime files. ~75 production files have special-cased vector/map/soa
  logic. Phase 1 (manifest extension) complete: TODO-4656 through
  TODO-4661, TODO-4672 through TODO-4675 done. Phase 2 (type-category
  declarations) complete: TODO-4662 through TODO-4667 done. Phase 3
  (generic slot layout): TODO-4668 and TODO-4669 done. Remaining: TODO-4670
  (remove old alias branches when ready), TODO-4671 (cleanup dead helpers).
  Full design document at `docs/CollectionDecoupling.md`.

### Execution Queue

1. TODO-4609: Reject escaping local array slices
2. TODO-4610: Add forward cursor traversal API
3. TODO-4611: Add reverse cursor traversal API
4. TODO-4612: Add safe extent and cursor code examples
5. TODO-4637: Move `ir_pipeline` test shard into subdirectory
6. TODO-4638: Move `compile_run` test shard into subdirectory
7. TODO-4639: Move `semantics` test shard into subdirectory
8. TODO-4640: Move remaining test shards into subdirectories
9. TODO-4641: Group `include/primec/` headers by pipeline stage
10. TODO-4642: Consolidate loose top-level `src/` files into directories
11. TODO-4643: Fix 8 duplicate test names across files
12. TODO-4644: Rewrite 53 overlong test names (>120 chars)
13. TODO-4645: Drop `compiles and runs` prefix from ~740 test names
14. TODO-4646: Tighten 12 vague/short test names
15. TODO-4647: Rename 63 opaque shard files with topic suffixes
16. TODO-4648: Split `SemanticsValidate.cpp` into focused compilation units
17. TODO-4649: Convert IR lowerer include-only `.h` fragments to `.h/.cpp` pairs
18. TODO-4650: Convert `TemplateMonomorph*.h` semantics fragments to `.h/.cpp` pairs
19. TODO-4651: Split oversized test files (10K+ lines, 100+ tests)
20. TODO-4652: Split oversized single test case bodies (>1000 lines)
21. TODO-4653: Add dedicated IrPrinter unit tests
22. TODO-4654: Add [public] annotations to stdlib modules
23. TODO-4655: Add compile-run tests for language level examples
24. TODO-4670: Remove collection-specific slot layout helpers (old alias branches)
25. TODO-4671: Remove isVectorTypeName and isMapTypeName after migration

### Task Blocks

- [ ] TODO-4609: Reject escaping local array slices
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - parallel_track: array-slice-escape-diagnostics
  - depends_on: TODO-4608
  - scope: Add the first conservative view lifetime diagnostic by rejecting a
    slice or reference view that escapes a local array owner through return,
    stored field, or longer-lived binding.
  - implementation_notes: Start with semantic validation around slice
    construction, return validation, and binding lifetime/provenance facts. Use
    lexical scope rather than non-lexical lifetime inference.
  - acceptance:
    - Returning a slice of a local array is rejected with a deterministic
      diagnostic naming the view and owner.
    - Passing a slice to a callee that does not store or return it remains
      accepted.
    - Tests document that the first checker is conservative and lexical.
  - stop_rule: Stop once local-owner slice escape is rejected and covered; do
    not add disjoint mutable slice analysis in this leaf.

- [ ] TODO-4610: Add forward cursor traversal API
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - parallel_track: cursor-forward-traversal
  - depends_on: TODO-4608
  - scope: Add the first read-only forward cursor traversal API for arrays and
    vectors using `start(values)` as the first position and `limit(values)` as
    the one-past-final exclusive traversal boundary.
  - implementation_notes: Start with stdlib surface definitions or compiler
    intrinsics for `Cursor<T, Capability>`, `start`, `limit`, `read`, and
    `advance`; keep the first cursor category forward-only unless contiguous
    or random-access behavior is already needed by the tests.
  - acceptance:
    - A `while(it != limit(values))` loop over `vector<int>` compiles and runs
      without skipping the final element.
    - `read(limit(values))` is rejected or fails deterministically.
    - Cursor comparisons require compatible provenance and reject unrelated
      cursor comparisons.
  - stop_rule: Stop once read-only forward traversal works for arrays and
    vectors; leave reverse traversal and writable cursors to later leaves.

- [ ] TODO-4611: Add reverse cursor traversal API
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4610
  - scope: Add reverse read-only cursor traversal using
    `reverse_start(values)` as the last readable position or
    `reverse_limit(values)` when empty, and `reverse_limit(values)` as the
    one-before-first exclusive traversal boundary.
  - implementation_notes: Start with the cursor API from TODO-4610 and add
    `reverse_start`, `reverse_limit`, and `retreat` for arrays and vectors.
    Keep `last(values)` as an element-oriented helper returning
    `Maybe<Cursor<T, Capability>>` if it is exposed in this leaf.
  - acceptance:
    - A reverse `while(it != reverse_limit(values))` loop over `vector<int>`
      visits every element exactly once in reverse order.
    - Empty arrays/vectors produce `reverse_start(values) == reverse_limit(values)`
      and execute zero loop iterations.
    - `read(reverse_limit(values))` is rejected or fails deterministically.
  - stop_rule: Stop once read-only reverse traversal is implemented and
    covered for arrays and vectors; leave writable cursors and advanced
    iterator categories to later leaves.

- [ ] TODO-4612: Add safe extent and cursor code examples
  - owner: ai
  - created_at: 2026-05-27
  - phase: Safe array extents and views
  - depends_on: TODO-4604, TODO-4605, TODO-4606, TODO-4608, TODO-4610,
    TODO-4611
  - scope: Add runnable, style-aligned examples to `docs/CodeExamples.md` for
    the agreed safe-array extent and cursor surfaces after their normative
    spelling is specified.
  - implementation_notes: Cover the smallest useful set: a contract-form
    `require(...)` example that proves-or-checks an extent relationship,
    `Maybe<Pointer<T>>` optional-pointer handling, a
    `Reference<T, Capability>`/`Slice<T, Capability>` example, a checked slice
    loop, a forward cursor loop using `limit(values)`, and a reverse cursor
    loop using `reverse_limit(values)`. Keep examples minimal and runnable
    with the current compiler before treating them as style guidance.
  - acceptance:
    - `docs/CodeExamples.md` contains user-facing examples for safe extent
      contracts, optional pointers, capability views, checked slices, and
      forward/reverse cursor loops.
    - Examples use the readable surface form and naming guidance from
      `docs/CodeExamples.md`.
    - Source-lock coverage proves the examples stay aligned with
      `docs/SafeArrayExtentViews.md` and the relevant normative
      `docs/PrimeStruct.md` sections.
    - The new examples compile or are explicitly marked as proposed syntax
      until the corresponding implementation leaves land.
  - stop_rule: Stop once the example guide and source-lock coverage are
    updated; do not implement missing language features in this leaf.


- [x] TODO-4635: Derive the collection surface registry from stdlib declarations
  - owner: ai
  - created_at: 2026-06-10
  - phase: Collections naming and manifest retirement
  - depends_on: TODO-4631, TODO-4632, TODO-4633, TODO-4634
  - scope: Teach `StdlibSurfaceRegistry` to build collection surface metadata
    (member names, statement members, import-alias spellings, lowering
    spellings) from the parsed `[public]` stdlib declarations instead of
    `stdlib/std/collections/surfaces.psmeta`, keeping the manifest load behind
    a parity check during the transition.
  - implementation_notes: The manifest is loaded at runtime from
    `src/StdlibSurfaceRegistry.cpp:331`; the registry also carries parallel
    hardcoded C++ member arrays (for example the File helper tables) that
    should move to the same derivation. `stdlib/std/modules.psmeta` is a
    separate manifest and out of scope.
  - acceptance:
    - Registry contents derived from stdlib declarations are parity-checked
      identical to the manifest-derived contents.
    - Release tests pass with the derived registry active.
  - stop_rule: Stop once derivation plus parity check land; manifest deletion
    is TODO-4636.

- [x] TODO-4636: Delete surfaces.psmeta and its parity scaffolding
  - owner: ai
  - created_at: 2026-06-10
  - phase: Collections naming and manifest retirement
  - depends_on: TODO-4635
  - scope: Delete `stdlib/std/collections/surfaces.psmeta`, remove the
    manifest loader path and the TODO-4635 parity scaffolding from
    `StdlibSurfaceRegistry`, and update
    `scripts/check_soa_surface_trace_inventory.py` and its tests.
  - acceptance:
    - `stdlib/std/collections/surfaces.psmeta` is deleted and no code or
      script references it.
    - Release tests pass with the derived registry as the only source.
  - stop_rule: Stop once the manifest and loader are gone; do not extend the
    registry to non-collection surfaces in this leaf.

- [ ] TODO-4637: Move `ir_pipeline` test shard into subdirectory
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: test-layout-ir-pipeline
  - depends_on: (none)
  - scope: Move all `tests/unit/test_ir_pipeline*.cpp` and
    `tests/unit/test_ir_pipeline_helpers.h` files into
    `tests/unit/ir_pipeline/`, with subdirectories for `backends/`,
    `conversions/`, `serialization/`, `validation/`, `to_cpp/`, `to_glsl/`,
    and `wasm/`. Update the `PrimeStruct_backend_ir_tests` CMake source list.
  - implementation_notes: Use `git mv` for every file to preserve history.
    Do not rename test binaries. See `docs/FileLayoutRestructuring.md` for
    the full target layout.
  - acceptance:
    - All 157 `test_ir_pipeline*` files live under `tests/unit/ir_pipeline/`.
    - CMake source list reflects new paths.
    - `./scripts/compile.sh --release` passes.
    - `git log --follow` tracks renamed files correctly.
  - stop_rule: Stop once the ir_pipeline shard is moved and tests pass; do
    not touch other test shards in this leaf.

- [ ] TODO-4638: Move `compile_run` test shard into subdirectory
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: test-layout-compile-run
  - depends_on: (none)
  - scope: Move all `tests/unit/test_compile_run*.cpp` and helper headers
    into `tests/unit/compile_run/`, with subdirectories for `bindings/`,
    `emitters/`, `examples/`, `imports/`, `map_conformance/`,
    `native_backend/`, `smoke/`, `text_filters/`, `vector_conformance/`,
    and `vm/`. Update the `PrimeStruct_compile_run_tests` CMake source list.
  - implementation_notes: Use `git mv` for every file to preserve history.
    Do not rename test binaries.
  - acceptance:
    - All 208 `test_compile_run*` files live under `tests/unit/compile_run/`.
    - CMake source list reflects new paths.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the compile_run shard is moved and tests pass; do
    not touch other test shards in this leaf.

- [ ] TODO-4639: Move `semantics` test shard into subdirectory
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: test-layout-semantics
  - depends_on: (none)
  - scope: Move all `tests/unit/test_semantics*.cpp` and helper headers into
    `tests/unit/semantics/`, with subdirectories for `bindings/`,
    `calls_and_flow/`, `capabilities/`, `entry/`, `result_helpers/`,
    `type_resolution/`, and `manual/`. Update the
    `PrimeStruct_semantics_tests` CMake source list.
  - implementation_notes: Use `git mv` for every file to preserve history.
    Do not rename test binaries.
  - acceptance:
    - All ~100 `test_semantics*` files live under `tests/unit/semantics/`.
    - CMake source list reflects new paths.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the semantics shard is moved and tests pass; do
    not touch other test shards in this leaf.

- [ ] TODO-4640: Move remaining test shards into subdirectories
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: test-layout-remaining
  - depends_on: TODO-4637, TODO-4638, TODO-4639
  - scope: Move remaining flat test files into subdirectories: `parser/`
    (~20 files), `text_filter/` (7 files), `vm_debug/` (3 files),
    `compile_time/` (3 files), `import_resolver/` (3 files), `ast/` (4
    files). Update all affected CMake source lists.
  - implementation_notes: Use `git mv` for every file to preserve history.
    These are smaller shards so they can be done in one commit.
  - acceptance:
    - No `test_*.cpp` or `test_*.h` files remain at the `tests/unit/` root
      except `main.cpp` and shared helpers.
    - All CMake source lists reflect new paths.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all remaining shards are moved and tests pass.

- [ ] TODO-4641: Group `include/primec/` headers by pipeline stage
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: include-layout
  - depends_on: TODO-4637, TODO-4638, TODO-4639, TODO-4640
  - scope: Move the 67 flat headers in `include/primec/` into subdirectories
    by pipeline stage: `ast/`, `frontend/`, `semantics/`, `ir/`, `backend/`,
    `runtime/`, `support/`, `pipeline/`. Update all `#include` paths in
    `src/` and `tests/`. Update `scripts/include_layer_allowlist.txt` and
    `scripts/check_include_layers.py` if needed.
  - implementation_notes: This phase has the widest blast radius. Move
    headers last so test files are already settled. Use `git mv` and update
    includes with a find-and-replace pass. Verify the include layer checker
    still passes.
  - acceptance:
    - No headers remain at the `include/primec/` root (except possibly a
      convenience umbrella header).
    - All `#include` paths in `src/` and `tests/` resolve correctly.
    - `scripts/check_include_layers.py` passes.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all headers are grouped and the include layer
    checker passes; do not restructure header contents in this leaf.

- [ ] TODO-4642: Consolidate loose top-level `src/` files into directories
  - owner: ai
  - created_at: 2026-06-11
  - phase: File layout restructuring
  - parallel_track: src-layout
  - depends_on: TODO-4641
  - scope: Move the ~20 loose `.cpp` and `.h` files at the `src/` root into
    focused directories: `src/runtime/` (VM files), `src/ir/` (IR printer,
    serializer, validation, inliner, vreg files), `src/pipeline/`
    (CompilePipeline, CliDriver), `src/frontend/` (ImportResolver files),
    `src/bin/` (main.cpp, primevm_main.cpp). Update the top-level CMake
    source lists.
  - implementation_notes: The `semantics/`, `ir_lowerer/`, `emitter/`,
    `parser/`, `text_filter/`, `native_emitter/`, `glsl_emitter/`, and
    `wasm_emitter/` directories stay as-is.
  - acceptance:
    - No `.cpp` or `.h` files remain at the `src/` root except possibly
      a thin forwarding `loc.sh`.
    - All CMake source lists reflect new paths.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once loose files are consolidated and tests pass; do
    not restructure existing subdirectories in this leaf.

- [ ] TODO-4643: Fix 8 duplicate test names across files
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test name quality
  - parallel_track: test-name-duplicates
  - depends_on: (none)
  - scope: Disambiguate 8 test names that appear in multiple files. Prefix
    each with its test module or rewrite to name the distinct behavior each
    test covers. See `docs/FileLayoutRestructuring.md` for the full list.
  - implementation_notes: The duplicates are:
    - `pointer plus accepts i64 offsets` (2 files)
    - `block expression requires a value` (2 files)
    - `runs vm with map at_unsafe helper` (2 files)
    - `vector stdlib namespaced capacity expression keeps canonical precedence` (2 files)
    - `vector stdlib namespaced capacity expression keeps return mismatch diagnostics` (2 files)
    - `C++ emitter rejects canonical vector mutator methods with alias-only helper before emission` (2 files)
    - `ir lowerer inline param helper aliases pure pointer soa_vector variadic forwarding` (2 files)
    - `rejects vm vector method alias access struct method chain with array receiver diagnostics` (2 files)
  - acceptance:
    - `rg -U 'TEST_CASE\(\s*"([^"]+)"' tests/unit/ -o --replace '$1' | cut -d: -f2 | sort | uniq -d` returns empty.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once no duplicate names remain and tests pass.

- [ ] TODO-4644: Rewrite 53 overlong test names (>120 chars)
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test name quality
  - parallel_track: test-name-overlong
  - depends_on: TODO-4643
  - scope: Rewrite all 53 test names longer than 120 characters to focus
    on the behavior being verified, not the internal mechanism. Target max
    80 characters per name. See `tests/TEST_INVENTORY.md` for the full
    list.
  - implementation_notes: The longest names embed entire diagnostic
    descriptions. Rewrite each to answer "what does this prove?" rather
    than describing internal paths.
  - acceptance:
    - `rg -U 'TEST_CASE\(\s*"([^"]{120,})"' tests/unit/` returns empty.
    - All rewritten names are specific enough to identify the failure
      without reading the test body.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all names are under 120 characters and tests pass.

- [ ] TODO-4645: Drop `compiles and runs` prefix from ~740 test names
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test name quality
  - parallel_track: test-name-compile-run-prefix
  - depends_on: TODO-4643
  - scope: Remove the `compiles and runs` prefix from approximately 740
    test names. The prefix adds no information — the test file and module
    grouping already convey that this is a compile-run test.
  - implementation_notes: Do a bulk find-and-replace of
    `"compiles and runs "` → `""` in test name strings. Verify no
    semantic collision after the prefix is removed (i.e., no two tests in
    the same file end up with the same name).
  - acceptance:
    - `rg 'TEST_CASE\("compiles and runs' tests/unit/` returns empty.
    - No duplicate names within any single file after the change.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all prefixes are removed and tests pass.

- [ ] TODO-4646: Tighten 12 vague/short test names
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test name quality
  - parallel_track: test-name-vague
  - depends_on: TODO-4643
  - scope: Rewrite 12 test names under 20 characters to be specific enough
    to identify the failure without reading the test body. See
    `docs/FileLayoutRestructuring.md` for the full list.
  - implementation_notes: Examples: `parses loop form` → `parses loop form
    with body and condition`, `runs program in vm` → `vm runs hello world
    entry point`, `ir lowers clamp` → `ir lowers clamp with i32 operands`.
  - acceptance:
    - `rg -U 'TEST_CASE\(\s*"([^"]{1,19})"' tests/unit/` returns empty.
    - All rewritten names are specific enough to identify the behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all short names are tightened and tests pass.

- [ ] TODO-4647: Rename 63 opaque shard files with topic suffixes
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test name quality
  - parallel_track: test-shard-renames
  - depends_on: (none)
  - scope: Rename 63 test files that use opaque letter suffixes (`_a.cpp`,
    `_b.cpp`, ...) to use topic-descriptive suffixes instead. Each suffix
    should describe the content cluster in that shard. Update CMake source
    lists to match.
  - implementation_notes: There are 19 base names with shard splits. The
    largest is `test_compile_run_text_filters_diagnostics` with 22 shards.
    For each base name, read the test names in each shard, identify the
    topic cluster, and assign a short topic suffix (max 30 chars). Use
    `git mv` to preserve history. See `docs/FileLayoutRestructuring.md`
    for the full shard inventory.
  - acceptance:
    - No `_[a-z].cpp` or `_[a-z].h` files remain in `tests/unit/`.
    - All shard suffixes are topic-descriptive (e.g., `_argument_shape`,
      `_wrapper_methods`, `_png_read_filters`).
    - CMake source lists reflect new paths.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all shard files are renamed and tests pass; do
    not change test names or test logic in this leaf.

- [ ] TODO-4648: Split `SemanticsValidate.cpp` into focused compilation units
  - owner: ai
  - created_at: 2026-06-11
  - phase: Oversized file refactoring
  - parallel_track: split-semantics-validate
  - depends_on: (none)
  - scope: Split the 8,025-line `src/semantics/SemanticsValidate.cpp` into
    focused compilation units. Extract logical groups (validation passes,
    snapshot helpers, publication builders, benchmark orchestration, SoA
    helper metadata) into separate `.cpp` files with a shared context
    header.
  - implementation_notes: The file currently includes 13 Semantics-related
    headers and contains ~213 function definitions. Group functions by
    responsibility: validation entry points, snapshot/ID assignment
    helpers, experimental collection metadata validators, publication
    builders, and benchmark orchestration. Each extracted file gets a
    focused header if it is called from outside, or stays internal to the
    semantics module otherwise.
  - acceptance:
    - `SemanticsValidate.cpp` is under 2,000 lines.
    - No extracted file exceeds 1,500 lines.
    - `./scripts/compile.sh --release` passes.
    - `rg -c 'TEST_CASE' tests/` shows the same total count (no tests
      lost or duplicated).
  - stop_rule: Stop once the file is split and tests pass; do not change
    validation logic in this leaf.

- [ ] TODO-4649: Convert IR lowerer include-only `.h` fragments to `.h/.cpp` pairs
  - owner: ai
  - created_at: 2026-06-11
  - phase: Oversized file refactoring
  - parallel_track: split-ir-lowerer-headers
  - depends_on: (none)
  - scope: Convert the IR lowerer include-only `.h` fragments into
    compileable `.h/.cpp` pairs. The largest fragments:
    - `IrLowererLowerSumHelpers.h` (2,876 lines)
    - `IrLowererLowerStatementsExpr.h` (2,615 lines)
    - `IrLowererLowerEmitExprTailDispatch.h` (1,674 lines)
    - `IrLowererLowerEmitExprTryHelpers.h` (1,239 lines)
    - `IrLowererLowerInlineCalls.h` (1,021 lines)
    - `IrLowererLowerEmitExprCollectionHelpers.h` (818 lines)
  - implementation_notes: The IR lowerer already has a migration pattern
    (e.g., `IrLowererLowerEffects.{h,cpp}`). For each fragment: extract
    function declarations into the `.h`, move implementations to a new
    `.cpp`, update the CMake source list, and fix any textually-included
    dependencies.
  - acceptance:
    - Each fragment is a compileable `.h/.cpp` pair.
    - No `.h` file under `src/ir_lowerer/` contains function
      implementations.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all IR lowerer fragments are converted and tests
    pass; do not restructure the function logic in this leaf.

- [ ] TODO-4650: Convert `TemplateMonomorph*.h` semantics fragments to `.h/.cpp` pairs
  - owner: ai
  - created_at: 2026-06-11
  - phase: Oversized file refactoring
  - parallel_track: split-template-monomorph-headers
  - depends_on: TODO-4648
  - scope: Convert the `TemplateMonomorph*.h` include-only fragments into
    compileable `.h/.cpp` pairs. The largest fragments:
    - `TemplateMonomorphExpressionRewrite.h` (3,226 lines)
    - `TemplateMonomorphImplicitTemplateInference.h` (1,361 lines)
    - `TemplateMonomorphExperimentalCollectionReceiverResolution.h` (894 lines)
    - `TemplateMonomorphCoreUtilities.h` (820 lines)
    - `TemplateMonomorphDefinitionRewrites.h` (711 lines)
    - `TemplateMonomorphFallbackTypeInference.h` (708 lines)
    - `TemplateMonomorphMethodTargets.h` (651 lines)
  - implementation_notes: Depends on TODO-4648 splitting
    `SemanticsValidate.cpp` first, since these fragments are textually
    included into that translation unit. After the split, convert each
    fragment following the same pattern as the IR lowerer migration.
  - acceptance:
    - Each fragment is a compileable `.h/.cpp` pair.
    - No `TemplateMonomorph*.h` file contains function implementations.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all template monomorph fragments are converted
    and tests pass.

- [ ] TODO-4651: Split oversized test files (10K+ lines, 100+ tests)
  - owner: ai
  - created_at: 2026-06-11
  - phase: Oversized file refactoring
  - parallel_track: split-oversized-tests
  - depends_on: (none)
  - scope: Split test files that exceed 3,000 lines or contain 100+ test
    cases. The worst offenders:
    - `test_ir_pipeline_backends_registry.cpp` (10,003 lines, 156 tests)
    - `test_semantics_type_resolution_graph_snapshots.cpp` (8,645 lines)
    - `test_ir_pipeline_validation_ir_lowerer_statement_call_helper_validates_buffer_store_diagnostics.cpp` (7,004 lines)
    - `test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp` (5,806 lines)
    - `test_stdlib_map_ownership.cpp` (5,322 lines)
    - `test_compile_run_examples_docs_locks.cpp` (5,292 lines)
    - `test_compile_run_vm_collections_wrapper_temporaries_a.cpp` (3,519 lines, 189 tests)
  - implementation_notes: Apply the same doctest shard split pattern used
    elsewhere. Name shards by topic, not letters. Update CMake source
    lists. The doctest size guardrail says split at 10 SUBCASEs; apply
    the same principle to TEST_CASE count.
  - acceptance:
    - No test file exceeds 3,000 lines.
    - No test file contains more than 50 TEST_CASE macros.
    - All shard files use topic-descriptive suffixes.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all oversized test files are split and tests
    pass.

- [ ] TODO-4652: Split oversized single test case bodies (>1000 lines)
  - owner: ai
  - created_at: 2026-06-11
  - phase: Oversized file refactoring
  - parallel_track: split-oversized-test-bodies
  - depends_on: (none)
  - scope: Identify and split individual TEST_CASE or SUBCASE bodies that
    exceed 1,000 lines. These are usually large switch statements or
    sequential assertions that should be broken into focused subcases or
    helper functions.
  - implementation_notes: Search for TEST_CASE blocks with large line
    counts. Use `rg` to find `TEST_CASE` followed by many lines before
    the next `TEST_CASE`. Split into smaller focused subcases or extract
    repeated patterns into shared helper functions.
  - acceptance:
    - No single TEST_CASE body exceeds 1,000 lines.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once oversized test bodies are split and tests pass.

- [ ] TODO-4653: Add dedicated IrPrinter unit tests
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test coverage gaps
  - parallel_track: ir-printer-tests
  - depends_on: (none)
  - scope: Add dedicated unit tests for `IrPrinter` that directly verify
    output format, edge cases, and error handling. Currently IrPrinter is
    only tested indirectly through `test_ast_ir_dump*.cpp` files.
  - implementation_notes: Create `tests/unit/test_ir_printer.cpp` with
    focused test cases for IrPrinter output. Test empty programs, single
    definitions, nested expressions, and error cases.
  - acceptance:
    - `tests/unit/test_ir_printer.cpp` exists with at least 5 TEST_CASEs.
    - Tests cover empty program, basic definition, expression printing,
      and struct printing.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once basic IrPrinter coverage exists; do not aim for
    exhaustive coverage in this leaf.

- [ ] TODO-4654: Add `[public]` annotations to stdlib modules
  - owner: ai
  - created_at: 2026-06-11
  - phase: Stdlib quality
  - parallel_track: stdlib-public-annotations
  - depends_on: (none)
  - scope: Add `[public]` annotations to user-facing definitions in stdlib
    `.prime` files. Currently none of the 31 stdlib files use `[public]`,
    so all definitions are implicitly visible regardless of whether they
    are intended as public API.
  - implementation_notes: Start with the style-aligned surface modules
    listed in `docs/CodeExamples.md`: `math/*`, `maybe/*`, `file/*`,
    `image/*`, `ui/*`, `scene/*`, `collections/vector.prime`,
    `collections/map.prime`, `collections/errors.prime`,
    `collections/soa.prime`, `tuple/tuple.prime`, `gfx/gfx.prime`. Mark
    internal helper definitions as non-public.
  - acceptance:
    - At least the style-aligned stdlib modules have `[public]` on
      user-facing definitions.
    - Internal helpers are not marked `[public]`.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the style-aligned modules have visibility
    annotations; do not annotate all 31 files in this leaf.

- [ ] TODO-4655: Add compile-run tests for language level examples
  - owner: ai
  - created_at: 2026-06-11
  - phase: Test coverage gaps
  - parallel_track: example-compile-run-tests
  - depends_on: (none)
  - scope: Add compile-run test cases that exercise the language level
    examples under `examples/`. Currently only 8 test references to
    `examples/` exist, so most examples can silently drift from the
    compiler's actual behavior.
  - implementation_notes: For each `.prime` file under
    `examples/0.Concrete/`, `examples/1.Template/`,
    `examples/2.Inference/`, `examples/3.Surface/`, and
    `examples/4.Transforms/`, add a test case that compiles and runs the
    example (or at minimum parses and lowers it). Group tests by language
    level.
  - acceptance:
    - Every `.prime` file under `examples/` has at least one test case
      that compiles and runs it.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once all examples are covered; do not add new examples
    in this leaf.

- [ ] TODO-4670: Remove old vector/soa-vector alias early-exit branches
  - owner: ai
  - created_at: 2026-06-13
  - phase: Collection decoupling - Phase 3
  - parallel_track: collection-decoupling
  - depends_on: TODO-4623, TODO-4624 (alias removal from TODO-4623..4636)
  - scope: Remove `isBuiltinVectorTypeName` and `isBuiltinSoaVectorTypeName`
    early-exit branches from `resolveStructSlotLayoutFromDefinitionFields`
    in `IrLowererStructSlotLayoutHelpers.cpp`. These 3-slot hardcoded layouts
    handle old aliases (`vector`, `/vector`, `soa_vector`, `/soa_vector`).
    They can only be removed once TODO-4623..4636 retires these aliases so
    no code paths reach these branches.
  - acceptance:
    - Both branches deleted
    - `isBuiltinVectorTypeName` and `isBuiltinSoaVectorTypeName` helpers
      have no remaining callers in this file
    - All tests pass
  - stop_rule: branches deleted and tests pass

- [ ] TODO-4671: Remove isVectorTypeName and isMapTypeName after migration
  - owner: ai
  - created_at: 2026-06-13
  - phase: Collection decoupling - Cleanup
  - parallel_track: collection-decoupling
  - depends_on: TODO-4665, TODO-4666, TODO-4670
  - scope: After TODO-4666 (IR lowerer slot layout) and TODO-4670
    (old alias branches) are complete, verify that `isVectorTypeName()`,
    `isMapTypeName()`, `isBuiltinVectorTypeName`, `isBuiltinSoaVectorTypeName`,
    `isInternalSoaColumnTypeName`, and `isExperimentalSoaVectorTypeName` have
    no remaining callers. Delete dead helpers.
  - acceptance:
    - Dead helper functions removed from `IrLowererStructSlotLayoutHelpers.cpp`
    - No unused collection helper functions remain
    - Full test suite passes
  - stop_rule: dead code removed and tests pass
