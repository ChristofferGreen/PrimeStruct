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
- TODO-4685: Directory-scan discovery of collection .prime files | track: collection-decoupling-registry | surface: StdlibSurfaceRegistry file discovery
- TODO-4690: Wire borrowedVariants/findBorrowedVariant, migrate first site | track: collection-decoupling-borrowed-variants | surface: StdlibSurfaceRegistry + method target resolution
- TODO-4694: Introduce shared collection/key-value trait wrapper helpers | track: collection-decoupling-trait-wrappers | surface: semantics type-classification helpers
- TODO-4707: Fix cross-test-case pollution in whole-process doctest suites | track: test-runtime-pollution-fix | surface: doctest suite process/case isolation
- TODO-4714: Fix named-argument call-form receiver dispatch for vector/map mutator helpers | track: hidden-test-failures-collections | surface: SemanticsValidatorExprCollectionAccess.cpp / SemanticsValidatorExprNamedArgumentBuiltins.cpp

### Immediate Next 10

- TODO-4611: Add reverse cursor traversal API
- TODO-4612: Add safe extent and cursor code examples
- TODO-4637: Move `ir_pipeline` test shard into subdirectory
- TODO-4708: Measure per-shard doctest binary startup/registration overhead
- TODO-4709: Audit compile_run pass/fail-only cases for downgrade candidates
- TODO-4710: Cache stdlib .prime parse results across compile-pipeline test runs
- TODO-4711: Tighten CTest TIMEOUT values toward the 30s ceiling
- TODO-4712: Grow CTest shard size once cross-test-case pollution is fixed
- TODO-4713: Diagnose and reduce SoaColumnsN monomorphization's non-linear cost
- TODO-4715: Triage remaining calls_flow.collections hidden failures into clusters
- TODO-4719: Fix remaining type_resolution_graph SoA-cluster compatibility failures
- TODO-4723: Fix imported-helper diagnostics, nested-call "unknown call target", and rooted-helper-fallback rejection bugs (15 cases)
- TODO-4724: Decompose the 2800+ line resolveMethodTarget function into smaller, traceable pieces
- TODO-4725: Triage and fix newly-exposed non-semantics test failures from TODO-4720's shard-config fix
- TODO-4726: Fix remaining namespaced/rooted builtin-helper matching gaps (5 functions, 4 cases)
- TODO-4727: Fix soa canonical-path (get/ref/reserve/to_aos) method routing through the full compile pipeline
- TODO-4728: Fix ir_lowerer effects-unit test fixtures missing semantic-product callable summaries
- TODO-4731: Close the modern soa surface gaps (bare get template args, method mutators, canonical to_aos lowering, call-receiver method chains, legacy-path diagnostics)
- TODO-4739: Fix vector/at direct-call override precedence - multiple redundant, inconsistent native-fastpath classification sites

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
- Test runtime optimization: get the test suite fast and hang-proof (no
  test should ever exceed 30s; most should run under 5s). Triggered by
  discovering an unsharded `calls_flow.collections` invocation left
  running for 2h13m undetected. TODO-4706 (done) root-caused the
  `calls_flow_collections` `181_190`-family shard timeouts to `SoaColumnsN`
  stdlib templates with up to 16 type parameters (measured at 426s for a
  single 16-column case, 1762s for the worst full shard) and shipped a
  CTest `TIMEOUT` override (300s -> 2400s) as the near-term fix, all 3
  previously-timing-out shards now pass. TODO-4707 fixes the cross-test-case
  pollution that currently forces small 10-case shards, TODO-4708 measures
  fixed per-shard binary startup cost, TODO-4709 audits `compile_run`
  cases that only check pass/fail (candidates for downgrading off the full
  compile-and-execute path), TODO-4710 caches redundant stdlib `.prime`
  re-parsing across compile-pipeline test helpers, TODO-4711 tightens
  CTest `TIMEOUT` values once real per-shard costs are known, TODO-4712
  grows shard size once TODO-4707 proves pollution-free (so hundreds of
  tiny shards stop each paying fixed binary-launch/registration cost), and
  TODO-4713 tracks the actual algorithmic investigation into why
  `SoaColumnsN` monomorphization cost grows so sharply, profiled to
  implicate the same fragmented compat-path resolution helpers documented
  in `docs/CompatPathResolutionConsolidation.md`. Full findings log at
  `docs/TestRuntimeOptimization.md`.
- Hidden test failure remediation: 13 of 27 `primestruct.semantics` CTest
  suites had a stale `TOTAL_CASES` in
  `cmake/PrimeStructManagedSemanticsSuites.cmake` that silently capped
  `--first`/`--last` sharding below the real case count, so roughly 900
  test cases (including all of the known SoA-cluster failures) were never
  once executed by the CTest gate despite `docs/failing_tests.md` claiming
  a green 1548/1548 run. The stale counts are now fixed; running the
  corrected gate end to end surfaced 46 failing shards / 122 individual
  failing test cases, documented in `docs/failing_tests.md`'s 2026-07-15
  entry. TODO-4714 fixes the single worst cluster (named-argument
  call-form receiver dispatch for vector/map mutator helpers, ~10 cases,
  root-cause partially traced already). TODO-4715 triaged the remaining
  92-case `calls_flow.collections` cluster and confirmed two dominant
  root causes without yet fixing them: same-path shadow precedence for
  explicit namespaced method calls (23 cases, one file) and the same
  generic-fallback-instead-of-specific-diagnostic pattern TODO-4714
  already started tracing (most of the rest, across ~9 more files).
  TODO-4716 (done) fixed 4 newly-exposed `effects`
  shards - two batches of stale test content (a rooted-path naming
  convention change, a text-transform-only `==` operator used on the raw
  no-transform parse path, and a struct-definition typo) plus a genuine
  ~605s reflected-SoaSchema case needing a `TIMEOUT` bump, same pattern as
  TODO-4706. TODO-4717 (done) re-investigated
  an `imports` case whose "always passes in isolation" documented finding
  just got contradicted by a genuine single-case CTest failure - it
  turned out to be stale test syntax (`mapPair<i32,i32>` no longer
  resolves for primitive keys; `map<i32,i32>` is the current constructor),
  not flakiness. TODO-4718 (done) fixed a `maybe.cpp` nullptr failure that
  turned out to be a test
  helper searching the wrong semantic-product fact table (method-call vs.
  direct-call targets) for a templated type's monomorphized method calls
  - not a compiler bug. TODO-4719 fixes the pre-existing 10-case
  `type_resolution_graph` SoA-cluster (already deeply investigated in an
  earlier session; blocked on a further `/soa/push` stdlib-syntax
  question for at least one case). TODO-4720 audits the other
  (non-semantics) suite-definition files for the same drift pattern, not
  yet checked.

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
26. TODO-4684: Spike a minimal zero-C++ collection type (done, see docs/todo_finished.md)
27. TODO-4685: Directory-scan discovery of collection .prime files
28. TODO-4686: Generic `[collection_type]`/`[key_value_type]` struct detection
29. TODO-4687: Generic canonicalPath/bridgeKey/prefix derivation + override syntax
30. TODO-4688: Fold `deriveCollectionsSurfaces()`'s 3 blocks into one loop
31. TODO-4689: Dynamically-sized registry storage, enum resolution by path
32. TODO-4690: Wire borrowedVariants/findBorrowedVariant, migrate first site
33. TODO-4691: Migrate remaining borrowed-variant chains in MethodTargetResolution
34. TODO-4692: Migrate soaVector* literal families to registry lookup
35. TODO-4693: Clean up residual ContainerError string comparisons
36. TODO-4694: Introduce shared trait wrapper helpers, behavior-preserving
37. TODO-4695: Migrate semantics/ call sites to wrappers
38. TODO-4696: Migrate ir_lowerer/ call sites to wrappers
39. TODO-4697: Migrate emitter/ call sites to wrappers
40. TODO-4698: Swap wrapper internals to generic registry/trait queries, delete old helpers
41. TODO-4699: Add legacy-collection-branch reachability instrumentation
42. TODO-4700: Delete 3-slot branches + duplicate definition, evidence-based
43. TODO-4701: Evidence-based resolution of isCollectionVectorOwnerPath branches
44. TODO-4702: Add second toy collection type, zero C++
45. TODO-4703: Add diff-based zero-C++ gate script
46. TODO-4704: Add audit-exemption-count ratchet script
47. TODO-4705: Correct stale Collection decoupling documentation
48. TODO-4707: Fix cross-test-case pollution in whole-process doctest suites
49. TODO-4708: Measure per-shard doctest binary startup/registration overhead
50. TODO-4709: Audit compile_run pass/fail-only cases for downgrade candidates
51. TODO-4710: Cache stdlib .prime parse results across compile-pipeline test runs
52. TODO-4711: Tighten CTest TIMEOUT values toward the 30s ceiling
53. TODO-4712: Grow CTest shard size once cross-test-case pollution is fixed
54. TODO-4713: Diagnose and reduce SoaColumnsN monomorphization's non-linear cost
55. TODO-4714: Fix named-argument call-form receiver dispatch for vector/map mutator helpers
56. TODO-4715: Triage remaining calls_flow.collections hidden failures into clusters
57. TODO-4719: Fix remaining type_resolution_graph SoA-cluster compatibility failures
61. TODO-4723: Fix imported-helper diagnostics, nested-call "unknown call target", and rooted-helper-fallback rejection bugs (15 cases)
62. TODO-4724: Decompose the 2800+ line resolveMethodTarget function into smaller, traceable pieces
63. TODO-4725: Triage and fix newly-exposed non-semantics test failures from TODO-4720's shard-config fix
64. TODO-4726: Fix remaining namespaced/rooted builtin-helper matching gaps (5 functions, 4 cases)
65. TODO-4727: Fix soa canonical-path (get/ref/reserve/to_aos) method routing through the full compile pipeline
66. TODO-4728: Fix ir_lowerer effects-unit test fixtures missing semantic-product callable summaries
67. TODO-4731: Close the modern soa surface gaps (bare get template args, method mutators, canonical to_aos lowering, call-receiver method chains, legacy-path diagnostics)

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

- [ ] TODO-4681: Delete unreachable collection dispatch branches
  - owner: ai
  - created_at: 2026-07-02
  - phase: Collection dispatch retirement
  - parallel_track: collection-decoupling
  - depends_on: TODO-4680
  - scope: With ordinary resolution primary, measure which
    count/capacity and compatibility interceptor branches no longer fire
    (coverage or trace evidence, not pattern matching) and delete them.
    Revisit blocked TODO-4670/TODO-4671 cleanup in the same pass.
  - acceptance:
    - Dead interceptor branches removed with evidence they were unreachable
    - Full release test suite passes
  - stop_rule: evidenced dead code removed and tests pass

- [ ] TODO-4683: Rewrite pair constructor calls to entries at monomorph time and delete the pair ladder
  - owner: ai
  - created_at: 2026-07-02
  - phase: Collection dispatch retirement
  - parallel_track: collection-decoupling
  - depends_on: TODO-4682
  - scope: ATTEMPTED 2026-07-03 AND REVERTED. The monomorph rewrite +
    signature deletion worked for direct compile-run programs but broke 121
    positive semantics programs plus ~90 pinned diagnostics: the pair-shape
    assumption is load-bearing across implicit template inference
    ("implicit template arguments conflict"), key/value target resolution
    (resolveMapTarget pairwise arg peeking in
    SemanticsValidatorExprArgumentValidation.cpp), user-defined same-path
    map helpers without stdlib imports, and several unknown-call-target
    diagnostic families. Current landed state: the 8 pair signatures remain
    as one-line forwards through the variadic entries constructor (single
    insertion implementation). A real deletion needs a phased sub-project:
    first teach resolveMapTarget/implicit-inference to understand
    entry-pack calls, then gate the rewrite on resolution outcomes (not
    path shape), then delete signatures and flip the eighthKey locks.
    ROUND 2 (2026-07-03): with existence-gating + env-gate A/B measurement
    the true regression delta is only 8 test cases (argument/field/
    assignment/receiver shapes), all rooted in pairwise K/V type-text
    inference fabricating map<EntryPath, EntryPath> specializations for
    rewritten entries-shaped calls. A first producer-fix attempt
    (deriveKeyValueTypesFromEntryPackCall, helper retained unwired in
    SemanticsValidatorCollectionHelperRewrites.cpp) surfaced additional
    unidentified producers; next attempt should instrument ALL key/value
    type-text producers at once (playbook in the
    map-pair-ladder-deletion-goal memory) before wiring fixes.
    ROUND 3 (2026-07-04): LANDED the monomorph pair-to-entry rewrite
    (TemplateMonomorphExpressionRewrite.h) gated on
    no-matching-pair-arity-overload (fires only if the pair signatures are
    absent, so it is provably inert today), plus entry-pack gates in the
    four pairwise K/V producers (BuildInitializerInference,
    InferCollectionCallResolution, ExprMutationBorrows,
    TemplateMonomorphExperimentalCollectionReceiverResolution) and named-arg
    ordering for rewritten pair calls. With the ladder DELETED all
    direct/receiver/assign/argument shapes passed, but 37 additional
    inference-layer cases regressed (auto returns, struct storage,
    helper-wrapped Result.ok payloads) because pre-rewrite inference needs
    the pair overloads; the ladder therefore stays as 8 one-line forwards.
    Discriminator evidence: suite at exact 115-case baseline with ladder
    present (all landed C++ inert), 152 with ladder deleted. Remaining
    deletion work = teach TemplateMonomorphFallbackTypeInference and the
    auto/storage return-type layers to type pair-shaped constructor calls
    without pair overloads (failing-case list preserved in the goal memory
    notes).
  - acceptance:
    - map.prime exposes exactly two map constructors: zero-arg and the
      variadic entries form
    - map(k, v, ...) surface calls still work in VM and exe for 1..8 pairs
    - Invalid pair calls keep their current diagnostics
    - Release map/conformance/lock suites green
  - stop_rule: pair signatures deleted and suites green

- [ ] TODO-4685: Generalize collection .prime file discovery to a directory scan
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 1
  - parallel_track: collection-decoupling-registry
  - depends_on: TODO-4684 (done, see docs/todo_finished.md)
  - scope: Replace the 3 hardcoded findStdlibCollectionFilePath("vector.prime"
    | "map.prime" | "soa.prime") call sites in src/StdlibSurfaceRegistry.cpp
    with a directory scan over the resolved stdlib/std/collections/
    directory, returning the list of *.prime files to consider. Keep
    deriveCollectionsSurfaces()'s 3 blocks temporarily filtering that list
    down to today's 3 names (no behavior change yet).
  - acceptance:
    - No hardcoded filename string literals remain for locating collection
      .prime files; the file list is produced by directory enumeration.
    - Registry contents are byte-identical to before the change.
    - Release tests pass.
  - stop_rule: Stop once directory-scan discovery lands with identical
    output; do not yet change struct-declaration detection or derivation.

- [ ] TODO-4686: Detect [collection_type]/[key_value_type] struct declarations generically
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 1
  - parallel_track: collection-decoupling-registry
  - depends_on: TODO-4685
  - scope: For each file found by TODO-4685, text-scan for a
    [collection_type] or [key_value_type] struct annotation (same scanning
    style as the existing [public] function scan in
    scanStdlibPublicFunctions) and extract the annotated struct's declared
    type name from the following line. Do not yet derive canonicalPath/
    bridgeKey/prefix from this — just prove detection matches today's 3
    known types (Vector, MapValue, SoaVector) plus discovers SoaColumn's
    [collection_type] in soa_storage.prime (currently unused by the
    registry) without misclassifying it.
  - acceptance:
    - Detection correctly identifies all 4 currently-annotated structs
      across vector.prime/map.prime/soa.prime/soa_storage.prime.
    - Unit test covers detection against a fixture .prime file.
  - stop_rule: Stop once detection is proven correct; derivation of
    canonicalPath/bridgeKey/prefix is TODO-4687.

- [ ] TODO-4687: Derive canonical path, bridge key, and member prefix generically, with annotation override
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 1
  - parallel_track: collection-decoupling-registry
  - depends_on: TODO-4686
  - scope: Derive memberPrefix (default: lowercase-first-letter of type name
    minus known suffixes Value/Vector/Column), canonicalPath, and bridgeKey
    from the detected type name and file location. Extend the
    [collection_type]/[key_value_type] annotation grammar with an optional
    named parameter (e.g. member_prefix="map") for cases the convention
    gets wrong, consumed by the same annotation-parsing code already used
    for [public] scanning. Verify the convention reproduces today's
    vector->"vector", MapValue->"map" (via explicit override), SoaVector->
    "soaVector" mappings exactly.
  - acceptance:
    - Derived values for vector/map/soa match today's hardcoded values
      exactly (parity-asserted in a unit test).
    - map.prime uses the new override parameter to get "map" instead of the
      convention-derived "mapValue".
  - stop_rule: Stop once derivation is proven equivalent for existing types;
    folding deriveCollectionsSurfaces() into one loop is TODO-4688.

- [ ] TODO-4688: Fold deriveCollectionsSurfaces()'s 3 hand-written blocks into one generic loop
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 1
  - parallel_track: collection-decoupling-registry
  - depends_on: TODO-4687
  - scope: Replace the 3 near-identical --- Vector --- / --- Map --- /
    --- Soa --- blocks in deriveCollectionsSurfaces() (StdlibSurfaceRegistry.cpp
    ~545-686) with one loop over TODO-4685/4686/4687's generic discovery,
    keeping the vectorHelpers/vectorConstructors/... named fields as
    lookups into the loop's output for now (no consumer-visible change).
  - acceptance:
    - deriveCollectionsSurfaces() output is byte-identical to before
      (parity check).
    - The 3 hand-written blocks are deleted; one generic loop remains.
    - Release tests pass.
  - stop_rule: Stop once the loop replaces the 3 blocks with identical
    output; changing storage shape/enum resolution is TODO-4689.

- [ ] TODO-4689: Make collection registry storage dynamically sized; resolve enum members by canonical path
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 1
  - parallel_track: collection-decoupling-registry
  - depends_on: TODO-4688
  - scope: Change Registry from a fixed std::array<StdlibSurfaceMetadata, 11>
    to a container built once at startup from (a) the fixed non-collection
    entries (File, Gfx, ContainerError) and (b) the dynamically-discovered
    collection entries from TODO-4688. Keep StdlibSurfaceId's 6 collection
    enum members, resolved once at startup by looking up "the discovered
    entry whose canonicalPath matches today's known path" (fail loudly if
    not found). No existing call site of StdlibSurfaceId::CollectionsManifestSurface0
    etc. changes.
  - acceptance:
    - A newly-added [collection_type]-annotated .prime file (e.g. a
      TODO-4684-style spike type, re-added temporarily for this test)
      appears in stdlibSurfaceRegistry() with domain==Collections and no
      enum member, with zero further edits to StdlibSurfaceRegistry.cpp/.h.
    - All existing StdlibSurfaceId-based call sites compile and behave
      unchanged; full suite passes.
  - stop_rule: Stop once new-type discovery requires zero C++ edits beyond
    the .prime file; do not migrate any of the ~40 existing enum call sites.

- [ ] TODO-4690: Wire borrowedVariants/findBorrowedVariant and migrate first call site
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 2
  - parallel_track: collection-decoupling-borrowed-variants
  - depends_on: (none — independent of Phase 1)
  - scope: Populate borrowedVariants for vector/map/soa registry entries
    (count->count_ref, at->at_ref, to_aos->to_aos_ref, etc.) from the pairs
    currently hardcoded across SemanticsValidatorExprMethodTargetResolution.cpp
    (lines 497-508, 986-993, 2038-2056) and TemplateMonomorphExpressionRewrite.h
    (1371-1439). Migrate exactly the `count`->`count_ref` call site to use
    findBorrowedVariant instead of its hardcoded branch.
  - acceptance:
    - borrowedVariants is non-empty for vector/map/soa surfaces.
    - findBorrowedVariant has at least one production caller.
    - The count/count_ref hardcoded branch at the migrated call site is
      removed; behavior unchanged; semantics tests pass.
  - stop_rule: Stop once one call site is migrated and passing; migrating
    the rest is TODO-4691/4692.

- [ ] TODO-4691: Migrate remaining borrowed-variant chains in MethodTargetResolution
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 2
  - parallel_track: collection-decoupling-borrowed-variants
  - depends_on: TODO-4690
  - scope: Migrate the remaining hardcoded helperName chains at
    SemanticsValidatorExprMethodTargetResolution.cpp:908-913, 2421-2424,
    2707-2803, 2865-2996, 3360-3555 to findBorrowedVariant/
    isStdlibSurfaceMemberName queries. Split into 2-3 batches by logical
    chain group rather than one commit.
  - acceptance:
    - Hardcoded helperName=="..." chains for borrowed-variant routing at
      the listed line ranges are removed in favor of registry queries.
    - Semantics tests pass after each batch.
  - stop_rule: Stop once all listed chains are migrated; do not touch
    unrelated helperName checks in the same file that aren't borrowed-
    variant routing.

- [ ] TODO-4692: Migrate soaVector* literal families to registry-driven lookup
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 2
  - parallel_track: collection-decoupling-borrowed-variants
  - depends_on: TODO-4691
  - scope: Replace the hardcoded soaVectorCount/soaVectorCountRef/
    soaVectorGetRef/soaVectorRefRef/soaVectorPush/soaVectorReserve/
    soaVectorNew/soaVectorSingle/soaVectorFromAos string literals in
    SemanticsBuiltinPathHelpers.cpp:927-1068 and SemanticsValidate.cpp:
    1386-1391 with borrowedVariants/isStdlibSurfaceMemberName queries
    against the soa registry entry.
  - acceptance:
    - Listed literal families are removed from the two files.
    - SOA-specific tests (tests/unit/semantics, tests/unit/compile_run soa
      shards) pass.
  - stop_rule: Stop once the listed literal families are migrated; do not
    touch soa literals outside these two files in this leaf.

- [ ] TODO-4693: Clean up residual bare ContainerError string comparisons
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 2
  - parallel_track: collection-decoupling-borrowed-variants
  - depends_on: (none)
  - scope: Remove the 2 remaining bare "ContainerError" string comparisons
    left over from TODO-4675, replacing with findStdlibSurfaceMetadata
    queries consistent with the rest of that migration.
  - acceptance:
    - Zero bare "ContainerError" string literal comparisons remain outside
      StdlibSurfaceRegistry.cpp itself.
    - Tests pass.
  - stop_rule: Stop once both comparisons are migrated.

- [ ] TODO-4694: Introduce shared collection/key-value trait wrapper helpers (behavior-preserving)
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 3
  - parallel_track: collection-decoupling-trait-wrappers
  - depends_on: (none — independent of Phase 1/2)
  - scope: Introduce 2-4 new, generically-named helper functions (e.g.
    isCollectionSurfaceTypeName, isKeyValueSurfaceTypeName) implemented as a
    behavior-preserving union of today's isKeyValueCollectionTypeName,
    isExperimentalCollectionBackingTypeName (literal-arg cases),
    isCollectionVectorValue, isKeyValueStorageValue, isArrayValue. Do not
    migrate any call sites yet.
  - acceptance:
    - New wrapper helpers exist with unit tests proving equivalence to the
      union of the old helpers' current behavior for all currently-known
      inputs.
    - No existing call site changed.
  - stop_rule: Stop once wrappers exist and are proven equivalent; call
    site migration is TODO-4695/4696/4697.

- [ ] TODO-4695: Migrate semantics/ call sites to the shared trait wrapper helpers
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 3
  - parallel_track: collection-decoupling-trait-wrappers
  - depends_on: TODO-4694
  - scope: Migrate the ~45 isKeyValueCollectionTypeName call sites and the
    isExperimentalCollectionBackingTypeName call sites under src/semantics/
    to call the TODO-4694 wrappers instead.
  - acceptance:
    - Old helpers' call-site count in src/semantics/ drops to 0.
    - Full semantics test suite passes.
  - stop_rule: Stop once src/semantics/ call sites are migrated; ir_lowerer/
    and emitter/ are separate leaves.

- [ ] TODO-4696: Migrate ir_lowerer/ call sites to the shared trait wrapper helpers
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 3
  - parallel_track: collection-decoupling-trait-wrappers
  - depends_on: TODO-4695
  - scope: Migrate the remaining isKeyValueCollectionTypeName/
    isExperimentalCollectionBackingTypeName call sites under
    src/ir_lowerer/ to the TODO-4694 wrappers.
  - acceptance:
    - Old helpers' call-site count in src/ir_lowerer/ drops to 0.
    - Full ir_pipeline test suite passes.
  - stop_rule: Stop once src/ir_lowerer/ call sites are migrated.

- [ ] TODO-4697: Migrate emitter/ call sites (isCollectionVectorValue/isKeyValueStorageValue/isArrayValue) to the shared wrapper helpers
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 3
  - parallel_track: collection-decoupling-trait-wrappers
  - depends_on: TODO-4696
  - scope: Migrate the 7 emitter files' isCollectionVectorValue/
    isKeyValueStorageValue/isArrayValue call sites to the TODO-4694
    wrappers.
  - acceptance:
    - Old helpers' call-site count in src/emitter/ drops to 0.
    - Compile-run tests pass.
  - stop_rule: Stop once emitter call sites are migrated.

- [ ] TODO-4698: Swap wrapper internals to query the generic registry/has_trait; delete old type-specific helpers
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 3
  - parallel_track: collection-decoupling-trait-wrappers
  - depends_on: TODO-4697, TODO-4689
  - scope: Now that all call sites go through the TODO-4694 wrappers and the
    Phase 1 registry genericization has landed, swap the wrappers' internal
    implementation to genuinely query stdlibSurfaceRegistry()'s domain/shape
    iteration (or has_trait where a resolved type is available), and delete
    isKeyValueCollectionTypeName, isExperimentalCollectionBackingTypeName,
    isCollectionVectorValue, isKeyValueStorageValue, isArrayValue entirely.
  - acceptance:
    - The 5 named old helpers no longer exist anywhere in the codebase.
    - A TODO-4684-style spike type (re-verified) is recognized correctly
      by the wrappers with zero code changes beyond its .prime declaration.
    - Full suite passes.
  - stop_rule: Stop once old helpers are deleted and wrappers are generic;
    do not begin new trait categories in this leaf.

- [ ] TODO-4699: Add legacy-collection-branch reachability instrumentation
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 4 (evidence gathering)
  - parallel_track: collection-decoupling-evidence
  - depends_on: (none)
  - scope: Add a benchmark-flag-gated counter family (following the
    --benchmark-semantic-* convention in src/OptionsParser.cpp), e.g.
    --benchmark-ir-lowerer-legacy-collection-branch-counters, that counts
    each time isBuiltinVectorTypeName/isBuiltinSoaVectorTypeName's 3-slot
    branches fire (IrLowererStructSlotLayoutHelpers.cpp:258,268), each time
    the duplicate in IrLowererUninitializedStructInference.cpp:178 fires,
    and each time isCollectionVectorOwnerPath/
    isCollectionVectorMetadataMethodPath (IrLowererSetupTypeMethodCallResolution.cpp)
    cause the caller to take the legacy path. Add a dual-computation
    equivalence check (log-only, non-fatal) comparing the 3-slot hardcoded
    layout against the generic field-based layout for TODO-4670's targets.
  - acceptance:
    - Full unit + compile_run suite runs with the new flag enabled and
      produces counter output plus zero divergence log lines from the
      dual-computation check, or a clear list of divergences if any exist.
  - stop_rule: Stop once counters and the equivalence check are landed and
    one full suite run's results are recorded; deletions are separate leaves.

- [ ] TODO-4700: Delete isBuiltinVectorTypeName/isBuiltinSoaVectorTypeName 3-slot branches and duplicate definition
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 4
  - parallel_track: collection-decoupling-evidence
  - depends_on: TODO-4699
  - scope: Given zero divergence from TODO-4699's equivalence check across
    the full suite, delete the 3-slot early-exit branches in
    IrLowererStructSlotLayoutHelpers.cpp (:258-267, :268-277) and their
    isBuiltinVectorTypeName/isBuiltinSoaVectorTypeName helper definitions
    (:21-28), and the independent duplicate definition of
    isBuiltinVectorTypeName in IrLowererUninitializedStructInference.cpp
    (:53-55, call site :178), replacing both with the generic field-based
    slot layout path. This supersedes TODO-4670's original narrower scope
    (which never mentioned the duplicate definition).
  - acceptance:
    - Both hardcoded 3-slot branches and the duplicate definition are gone.
    - Full suite passes with identical slot layouts for vector/soa as
      before (verified by the surviving generic path).
  - stop_rule: Stop once both deletions land and tests pass; do not touch
    isExperimentalSoaVectorTypeName/isInternalSoaColumnTypeName in this leaf.

- [ ] TODO-4701: Evidence-based resolution of isCollectionVectorOwnerPath/isCollectionVectorMetadataMethodPath branches
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 4
  - parallel_track: collection-decoupling-evidence
  - depends_on: TODO-4699
  - scope: Using TODO-4699's counters plus manual reachability review of
    every remaining caller of isCollectionVectorOwnerPath and
    isCollectionVectorMetadataMethodPath (IrLowererSetupTypeMethodCallResolution.cpp
    :411,547-597,636-676,721,740-771,1220), delete only the sub-parts shown
    both counter-zero and manually unreachable. Explicitly preserve the
    tryResolvedPath(targetPath) fallback added ahead of the owner-path
    branch's `return nullptr` (landed 2026-07-04/05 to fix a real method-
    call-resolution bug) if that branch shows nonzero reachability; do not
    delete it in this leaf if so. This is the evidence-based deletion TODO-
    4681 called for.
  - acceptance:
    - Every deleted branch has a recorded zero-counter result across the
      full suite AND a written manual-reachability justification.
    - Any branch not deleted has its continued necessity documented in
      this TODO's follow-up notes rather than silently left unaddressed.
  - stop_rule: Stop once evidence-supported deletions land; file a follow-up
    TODO for any branch whose evidence is inconclusive rather than forcing
    a decision.

- [ ] TODO-4702: Add a second toy collection type (Deque or RingBuffer) purely in stdlib, zero C++
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 5 (proof)
  - parallel_track: collection-decoupling-proof
  - depends_on: TODO-4689
  - scope: Add stdlib/std/collections/deque.prime (or ring_buffer.prime)
    implementing a minimal Deque<T>/RingBuffer<T> using [collection_type]
    and ordinary [public] methods (construct, push, count, at-or-iterate),
    with zero changes to any file under src/ or include/. Add compile_run
    tests exercising it end to end.
  - acceptance:
    - The new type compiles and its compile_run tests pass.
    - git diff for this change touches no path under src/** or include/**.
  - stop_rule: Stop once the type works end to end with a zero-C++ diff;
    do not add advanced operations beyond the minimal proof set.

- [ ] TODO-4703: Add a diff-based zero-C++ gate script for new collection types
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 5 (proof)
  - parallel_track: collection-decoupling-proof
  - depends_on: TODO-4702
  - scope: Add scripts/check_new_collection_zero_cpp.py, taking a before/
    after git ref pair, asserting the diff between them touches only
    stdlib/**, tests/**, docs/**. Wire it into CI/test workflow using the
    TODO-4702 commit range as its first proof case.
  - acceptance:
    - The script exists, is documented, and passes against the TODO-4702
      commit range.
  - stop_rule: Stop once the script exists and passes for one proof case;
    do not attempt to make it a general-purpose ongoing CI gate beyond
    this one recorded case yet.

- [ ] TODO-4704: Add an exemption-count ratchet for check_vector_surface_traces.py
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 5 (proof)
  - parallel_track: collection-decoupling-proof
  - depends_on: (none)
  - scope: Add scripts/check_collection_audit_exemption_count.py that counts
    files under src/ and include/ carrying a "*-surface-audit: exempt"
    marker and fails if the count exceeds the current baseline recorded at
    introduction time (115 files as of 2026-07-06). Wire into ctest/CI
    alongside check_vector_surface_traces.py.
  - acceptance:
    - Script exists, records today's baseline count, and fails on an
      artificially increased count in a test fixture.
  - stop_rule: Stop once the ratchet exists; shrinking the baseline is
    handled per-leaf in Phases 2-4 by removing markers as files are
    migrated, not in this leaf.

- [ ] TODO-4705: Correct stale Collection decoupling documentation
  - owner: ai
  - created_at: 2026-07-06
  - phase: Collection decoupling — Phase 6 (doc hygiene)
  - parallel_track: collection-decoupling-docs
  - depends_on: TODO-4693
  - scope: Fix docs/todo.md's summary line (currently claims TODO-4656
    through TODO-4661 are all done; only 4656/4658 are). Check off
    TODO-4658, TODO-4672, TODO-4675 in docs/CollectionDecoupling.md. Add the
    duplicate-definition note to TODO-4670's scope (superseded in practice
    by TODO-4700 above). Record the TODO-4703/TODO-4704 acceptance criteria
    as the effort's top-level completion definition.
  - acceptance:
    - docs/todo.md and docs/CollectionDecoupling.md accurately reflect
      source-verified completion status for TODO-4656 through TODO-4675.
  - stop_rule: Stop once both docs are corrected; do not re-scope any
    open TODO's acceptance criteria in this leaf.

- [ ] TODO-4707: Fix cross-test-case pollution in whole-process doctest suites
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-pollution-fix
  - depends_on: (none)
  - scope: `docs/failing_tests.md` documents two known cases of test
    results changing based on whether a suite runs as one continuous
    process vs. sharded into single-case processes: (1) running all of
    `primestruct.semantics.calls_flow.collections` unsharded shows ~114
    spurious failures not present under CTest's sharded invocation, and (2)
    `primestruct.semantics.imports`'s "import resolves std collections
    experimental map wildcard surface" case deterministically fails only
    when run as part of the full unsharded suite. Find the shared state (a
    cache, static table, or similar) that isn't reset between `TEST_CASE`
    invocations in-process and fix it so both suites produce identical
    results whether run as one process or sharded.
  - implementation_notes: Likely a static/thread-local cache in the
    semantics or stdlib-resolution layer that isn't cleared by doctest's
    per-case teardown. Diff the exact failing test names between an
    unsharded run and a fixed baseline (per the existing "Methodology
    note" in `docs/failing_tests.md`) to confirm the fix closes the gap
    without changing sharded-run results.
  - acceptance:
    - `primestruct.semantics.imports` run in one unsharded process produces
      the same pass/fail set as its CTest-sharded run.
    - `primestruct.semantics.calls_flow.collections` run in one unsharded
      process produces the same pass/fail set as its CTest-sharded run (no
      ~114-case spurious-failure artifact).
  - stop_rule: Stop once these two documented cases are eliminated; do not
    increase CTest shard sizes in this leaf even though it becomes safe to
    do so — that's a follow-up once pollution-freedom is proven broadly,
    not just for these two known cases.

- [ ] TODO-4708: Measure per-shard doctest binary startup/registration overhead
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-startup-cost
  - depends_on: (none)
  - scope: Measure the fixed cost of launching `PrimeStruct_semantics_tests`
    (or another large managed-suite binary) and reaching the point where
    doctest has registered all `TEST_CASE`s, before any selected case
    actually executes. Multiply by the number of CTest shards that launch
    this binary to estimate total suite-wide fixed cost from process
    startup and static registration alone.
  - implementation_notes: A `--list-test-cases` invocation (or a case
    selector matching zero cases) isolates registration/startup time from
    execution time. Compare against a much smaller single-suite test binary
    to see how registration cost scales with total `TEST_CASE` count in
    the binary.
  - acceptance:
    - A reproducible measured number (milliseconds) for fixed
      startup/registration cost is recorded in
      `docs/TestRuntimeOptimization.md`, along with the shard count for
      at least one large suite and the resulting suite-wide estimate.
  - stop_rule: Stop once the measurement is taken and documented; do not
    implement any startup-cost optimization in this leaf — file a
    follow-up if the measured cost is a significant fraction of total
    suite runtime.

- [ ] TODO-4709: Audit compile_run pass/fail-only cases for downgrade candidates
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-pyramid-audit
  - depends_on: (none)
  - scope: Scan `tests/unit/compile_run/` for `TEST_CASE` bodies that only
    assert on `ok`/`error` (compile success or failure) via
    `validateProgramThroughCompilePipeline`/`runCompilePipeline`, without
    inspecting actual emitted/executed program output. These are candidates
    for downgrading to a plain `Semantics::validate()` check, which skips
    codegen and process-spawn entirely.
  - implementation_notes: ~3,935 `TEST_CASE`s live under `compile_run`
    (~42% of the ~9,443 total in `tests/unit/`); this leaf is the audit
    only, not the migration. Produce a concrete `file:line` list, split into
    "safe to downgrade" (pass/fail only, no output/behavior assertions) vs.
    "needs the full pipeline" (checks actual runtime output, cross-backend
    parity, or emitted-code shape).
  - acceptance:
    - A list of candidate `file:line` entries with a total count is
      recorded in `docs/TestRuntimeOptimization.md`.
  - stop_rule: Stop once the audit list exists; do not perform any of the
    downgrades in this leaf — each migration is its own follow-up leaf so
    correctness can be verified per-file.

- [ ] TODO-4710: Cache stdlib .prime parse results across compile-pipeline test runs
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-stdlib-cache
  - depends_on: (none)
  - scope: Determine whether `validateProgramThroughCompilePipeline`-style
    test helpers (and the underlying `ImportResolver`/`runCompilePipeline`
    machinery) re-read and re-parse the same unchanging stdlib `.prime`
    files from disk for every single test case that imports them. If so,
    add a process-local cache keyed on file path + mtime so repeated
    imports of the same stdlib module within one test binary process reuse
    already-parsed content.
  - implementation_notes: Confirm with a read syscall count or simple
    instrumentation before assuming this is real; don't add caching
    speculatively. Any cache must not change behavior for tests that
    intentionally write and import a modified stdlib file mid-run, if any
    exist.
  - acceptance:
    - Before/after wall-clock timing for one representative `compile_run`
      CTest shard is recorded in `docs/TestRuntimeOptimization.md`.
    - No test behavior changes (full affected suite still passes
      identically before and after).
  - stop_rule: Stop once caching is implemented and measured for one
    representative shard; broader rollout or cache-invalidation edge cases
    are follow-up work if the measured win is significant.

- [ ] TODO-4711: Tighten CTest TIMEOUT values toward the 30s ceiling
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-timeout-tightening
  - depends_on: TODO-4707, TODO-4708, TODO-4709, TODO-4710
  - scope: Once real per-shard/per-suite runtimes are known from the
    groundwork leaves, lower the managed doctest suite `TIMEOUT` (currently
    300s via `addPrimeStructManagedDoctestSuite`, 600s via the older
    `PrimeStructSuite_TIMEOUT` default) suite-by-suite toward the 30s hard
    ceiling from `docs/TestRuntimeOptimization.md`, with headroom sized to
    each suite's actual observed runtime rather than a single global cut.
  - implementation_notes: Suites that can't yet meet 30s (because their
    root-cause slowness leaf hasn't landed) should get an explicit,
    commented interim timeout and a linked follow-up TODO, not be silently
    left at the old default.
  - acceptance:
    - Every managed doctest suite's CTest `TIMEOUT` is at or under 60s,
      with suites already fast enough tightened to 30s or less.
    - Any suite still exceeding 30s has a documented interim timeout and a
      linked open TODO explaining why.
  - stop_rule: Stop once every managed suite has an intentional,
    documented timeout at or below 60s; do not force every suite to exactly
    30s in this leaf if its own root-cause fix hasn't landed yet.

- [ ] TODO-4712: Grow CTest shard size once cross-test-case pollution is fixed
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-shard-consolidation
  - depends_on: TODO-4707, TODO-4708
  - scope: Managed doctest suites are currently sharded into small 10-case
    `add_test` chunks (`addPrimeStructManagedDoctestSuite`,
    `cmake/PrimeStructManagedSemanticsSuites.cmake`), which was necessary to
    dodge cross-test-case pollution (see TODO-4707) but means every one of
    the resulting hundreds of shards separately pays fixed binary-launch
    and doctest-registration overhead (see TODO-4708's measurement). Once
    TODO-4707 proves a suite pollution-free running as one process, raise
    that suite's `CASES_PER_SHARD` (or equivalent) toward the largest chunk
    size that still finishes comfortably under the 30s ceiling from
    `docs/TestRuntimeOptimization.md`, so the fixed per-shard cost stops
    being paid hundreds of times over for the same total case count.
  - implementation_notes: Shard size is a tradeoff, not a monotonic win:
    bigger shards amortize fixed overhead better but increase blast radius
    (one bad case can no longer be isolated as easily) and reduce
    parallelism granularity under `ctest --parallel N`. Pick a size using
    TODO-4708's measured overhead number and real per-case runtime, not a
    round number. Start with `calls_flow.collections` (the suite already
    under investigation) before generalizing to other managed suites.
  - acceptance:
    - `calls_flow.collections`'s shard count is reduced (larger
      `CASES_PER_SHARD`) with total wall-clock time for the full suite
      measurably lower than the current 10-case-shard baseline, and no
      shard exceeds the 30s ceiling.
    - The change is proven safe by confirming pass/fail results are
      identical to the pre-change baseline (no reintroduced pollution).
  - stop_rule: Stop once `calls_flow.collections` is re-sharded and
    verified; rolling the same change out to every other managed suite is
    follow-up work, not part of this leaf.

- [ ] TODO-4713: Diagnose and reduce SoaColumnsN monomorphization's non-linear cost
  - owner: ai
  - created_at: 2026-07-15
  - phase: Test runtime optimization
  - parallel_track: test-runtime-monomorph-perf
  - depends_on: (none)
  - scope: TODO-4706 root-caused (and pragmatically timeout-mitigated) the
    `calls_flow_collections` shard timeout cluster: monomorphizing
    `stdlib/std/collections/soa_storage.prime`'s `SoaColumnsN` struct/helper
    family (N = 2 through 16 type parameters, exercised by
    `test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp`)
    has wall time that grows far faster than the column count itself:
    measured standalone/serial, 2-col ~12s, 4-col ~13s, 8-col ~15s, 12-col
    ~41s, 16-col **426s** (a 33% column-count increase from 12 to 16
    produced a ~10x time increase). This leaf is the actual algorithmic
    investigation and fix TODO-4706 explicitly deferred.
  - implementation_notes: Live gdb sampling (`gdb -p <pid> -batch -ex "bt
    12"`, repeated every few seconds against a running 16-column case) on
    2026-07-15 found: (1) deep self-recursion through
    `rewriteExpr` (`TemplateMonomorphExpressionRewrite.h:2099`) walking
    chained receiver expressions in the monomorphized helper bodies —
    `mapping`/`locals`/`params` are already passed by const-reference, not
    copied, so the recursion depth itself isn't the direct cost; (2) a wide,
    diffuse set of small string-heavy compat/alias-resolution helpers
    invoked on every expression node during that walk -
    `resolveCalleePath` (`TemplateMonomorphTypeResolution.h:643`),
    `resolveStdlibSurfaceCompatibilityAlias`'s `findVisibleSpelling` inner
    loop, `matchesAny`, `stdlibSurfaceMatchesSpelling`,
    `canonicalizeLegacySoaToAosHelperPath`, `resolveStdlibSurfaceMemberName`
    - live-inspected `ctx.sourceDefs.size() == 531`,
    `ctx.templateDefs.size() == 432` at one sample point (roughly constant
    across the whole shard since `import .../soa_storage/*` always pulls in
    all N=2..16 struct/helper definitions regardless of which N the test
    actually uses). No single dominant hot function was isolated in the
    ~15-25 samples taken; the cost looks compounded from several sources
    (recursion depth, per-node compat-alias-resolution fan-out, and
    possibly cascading per-field dependency resolution) rather than one
    fixable bug. This is the same fragmented, redundantly-composed
    compat-path resolution architecture already documented as a
    maintainability problem in `docs/CompatPathResolutionConsolidation.md`
    ("at least three interacting rules... none has a single
    implementation... scattered... 9 files") - that consolidation, if
    completed, would plausibly also collapse much of this per-node
    resolution fan-out as a side effect. Start with proper profiling
    (`valgrind --tool=callgrind` on a smaller but still-slow case, e.g.
    8-12 columns, to keep iteration time reasonable) to get real call
    counts instead of statistical gdb sampling before attempting any fix -
    this exact subsystem produced a real regression earlier the same
    session from an under-verified change, so any fix here needs a full
    suite-wide sharded ctest regression pass before being trusted, not just
    the directly-touched test file.
  - acceptance:
    - A profiler-backed (not just statistically-sampled) breakdown of where
      wall time actually goes for a representative slow case (e.g.
      16-column) is recorded in `docs/TestRuntimeOptimization.md`.
    - Either a verified fix lands that measurably reduces the 16-column
      case's wall time (with a full sharded `ctest -R calls_flow_collections`
      run confirming no regressions), or, if no safe fix is found in scope,
      the investigation's conclusion (including why a fix wasn't safe/
      tractable) is documented clearly enough for the next attempt to skip
      re-deriving this leaf's findings.
  - stop_rule: Stop once the profiler-backed diagnosis is recorded and
    either a verified fix lands or a clear "not safely fixable in this
    pass" conclusion is documented; do not attempt a broad rewrite of the
    compat-path resolution helpers in this leaf even if the diagnosis
    points there - that overlaps `docs/CompatPathResolutionConsolidation.md`
    and should be its own coordinated effort, not a side effect of a
    performance leaf.

- [ ] TODO-4714: Fix named-argument call-form receiver dispatch for vector/map mutator helpers
  - owner: ai
  - created_at: 2026-07-15
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-collections
  - scope: `test_semantics_calls_and_flow_collections_vector_helper_call_form_named_receivers.cpp`
    (CTest shard `calls_flow_collections_811_820`, newly reachable after
    the TOTAL_CASES fix) fails on all ~10 of its cases as of 2026-07-15.
    Representative failure: `push([values] values, [value] 3i32)` used in
    expression context should be rejected with "push is only supported as
    a statement" but instead produces the generic "unknown call target:
    /std/collections/vector/push". Root cause partially traced: the
    intended diagnostic path
    (`SemanticsValidator::validateExprNamedArguments`,
    `SemanticsValidatorExprNamedArgumentBuiltins.cpp:74-79`) never runs for
    this call shape - confirmed via a temporary debug print that never
    fired. The failure must originate earlier in
    `SemanticsValidatorExpr.cpp`'s validation chain, most likely inside
    `resolveExprCollectionAccessTarget`
    (`SemanticsValidatorExprCollectionAccess.cpp`, which has 7+ separate
    `"unknown call target: " + methodResolved` construction sites around
    lines 379, 394, 528, 541, 863, 894, 902) returning `false` before
    `validateExprNamedArguments` is ever reached.
  - implementation_notes: Reproduce fast with
    `./PrimeStruct_semantics_tests --test-suite="primestruct.semantics.calls_flow.collections" --test-case="vector helper call-form expression builtin stays statement-only with named arguments" --no-skip`
    (sub-second). Static reading of the dispatch chain proved slow; use a
    gdb breakpoint on `SemanticsValidator::failExprDiagnostic` (or on each
    of the `"unknown call target: "` construction sites in
    `SemanticsValidatorExprCollectionAccess.cpp`) with `bt` to get the real
    call stack instead of re-deriving it statically. Once the actual branch
    is found, check whether the fix belongs in the named-value-receiver
    probing (`namedValuesReceiverIndex` / `tryResolveNamedValuesReceiver`
    in `SemanticsValidatorExprVectorHelpers.cpp:852-932`) or earlier, in
    `resolveExprCollectionAccessTarget` itself.
  - acceptance:
    - All cases in `test_semantics_calls_and_flow_collections_vector_helper_call_form_named_receivers.cpp`
      pass (CTest shard `calls_flow_collections_811_820` green).
    - Full `ctest -R calls_flow_collections` sharded run shows no new
      failures vs. the 2026-07-15 baseline recorded in
      `docs/failing_tests.md`.
  - stop_rule: This exact subsystem (compat-path/collection-helper call
    resolution) already produced one real regression this session from an
    under-verified change - always verify via the full sharded
    `calls_flow.collections` CTest run before committing, never just the
    directly-touched test file.
  - progress_2026-07-16: gdb-confirmed (breaking on `failExprDiagnostic`)
    the root cause for the representative case
    ("vector helper call-form expression builtin stays statement-only
    with named arguments"): `resolveExprVectorHelperCall`
    (`SemanticsValidatorExprVectorHelpers.cpp:597-999`) has an early
    guard, `isStdNamespacedVectorCountCapacityNamedArgException`
    (~line 769), that lets named-argument `count`/`capacity` calls skip
    an early "unknown call target" bail-out (~line 814-818) and reach
    the later receiver-probing logic where the correct "is only
    supported as a statement" diagnostic lives (~line 877-880). Mutator
    helpers (`push`/`pop`/`reserve`/`clear`/`remove_at`/`remove_swap`)
    have no equivalent exception, so a named-argument mutator call in
    expression context hits the early bail-out first and never reaches
    the statement-only check. **Tried and reverted**: broadening the
    exception to also cover `isPublishedVectorMutatorHelperName(...)`
    fixed the target case and several others in the same file (21 of 32
    cases in
    `test_semantics_calls_and_flow_collections_vector_helper_call_form_named_receivers.cpp`
    went from failing to passing, up from ~0), but a full 131-shard
    regression run found 4 new regressions in 3 *other* test files
    (`test_semantics_calls_and_flow_collections_bare_vector_pop_helper_resolution.cpp`,
    `test_semantics_calls_and_flow_collections_vector_capacity_alias_named_args.cpp`,
    `test_semantics_calls_and_flow_collections_vector_mutator_named_args.cpp`)
    that also exercise named-argument mutator calls but expect the
    *original* strict rejection behavior, not deferral to the
    statement-only check. Reverted (uncommitted) rather than chase a
    fourth hidden distinguishing factor without a clear hypothesis yet -
    this session already spent three separate regression-and-narrow
    cycles on neighboring `resolveMethodTarget`/
    `SemanticsValidatorExprMethodTargetResolution.cpp` bugs (see
    TODO-4723's `progress_2026-07-16`/`progress_2026-07-16b`) and this
    is the same subsystem exhibiting the same pattern - worth reading
    the 4 newly-discovered regressed tests' exact source shapes
    (probably another Name-vs-Call, or with-vs-without-existing-
    definition distinction, per the pattern already seen twice) before
    the next attempt, rather than re-broadening blind.
  - remaining_2026-07-16: the exception needs to be scoped more
    precisely than "any published vector mutator with named args" -
    likely needs to also account for whichever condition the 4 newly-
    found regressed tests share that the fixed cases don't (receiver
    kind, presence of an explicit/imported helper definition, or
    something else not yet identified). 11 of the 32 cases in the
    target file remain unfixed even with the (reverted) broad version,
    also still open.

- [ ] TODO-4715: Triage remaining calls_flow.collections hidden failures into clusters
  - owner: ai
  - created_at: 2026-07-15
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-collections
  - depends_on: TODO-4714
  - scope: Of the 33 failing `calls_flow_collections` shards found on
    2026-07-15 (ranges 791-800 through 1291-1300, all beyond the old
    771-case TOTAL_CASES cutoff), shard `811_820` (TODO-4714) has a
    partial root cause. The other 32 shards / 92 distinct failing test
    cases across 13 files were triaged in a second pass the same day: ran
    `ctest -R "<pipe-joined list of the 32 shard names>" --output-on-failure --parallel 4`
    and collected every failing case's actual-vs-expected diagnostic (full
    log at
    `/tmp/claude-.../scratchpad/collections_triage.log` - session-scratch,
    re-run if needed after this session ends).
  - implementation_notes: Two concrete root-cause patterns confirmed with
    real repros (both distinct from TODO-4714's named-argument-dispatch
    finding):
    (1) **Same-path shadow precedence for explicit namespaced method
    calls** (largest single file cluster - 23 of 92 cases, in
    `test_semantics_calls_and_flow_collections_wrapper_returned_map_method_resolution.cpp`;
    16 of the 92 case titles literally contain "same-path"). Repro
    (confirmed via `primec --dump-stage semantic-product` directly, not
    just the test binary): a user definition at
    `/std/collections/vector/count([vector<i32>] values, [bool] marker)`
    (same path as the builtin `count`, different signature) should shadow
    the builtin when called via the explicit dotted form
    `values./std/collections/vector/count(true)`, but instead the
    validator resolves it to the builtin `count` and rejects it with
    `argument count mismatch for builtin count` - the same-path user
    definition is never consulted for this call shape. This is the same
    "D5: same-path shadow precedence" rule class documented in
    `docs/CompatPathResolutionConsolidation.md`, and the same class of bug
    already caused one real regression this session (the reverted
    vector-count-alias guard, see `docs/TestRuntimeOptimization.md`'s log)
    - any fix here needs the same full-suite sharded-ctest verification
    discipline before committing.
    (2) **Generic-fallback-instead-of-specific-diagnostic**, matching
    TODO-4714's pattern exactly: many cases across
    `wrapper_temporary_access_resolution.cpp`,
    `wrapper_temporary_templated_vector_methods.cpp`,
    `vector_helper_reordered_expression.cpp`,
    `vector_capacity_alias_named_args.cpp`, etc. expect a specific
    rejection message (e.g. `"unknown method: /string/count"`,
    `"argument type mismatch for /std/collections/vector/at parameter
    index"`, `"template arguments are only supported on templated
    definitions"`) but get a different, less specific error (or, in a few
    cases, unexpectedly succeed where rejection was expected) - consistent
    with call resolution taking an earlier/wrong branch before reaching
    the code that would produce the expected diagnostic, same shape as the
    `SemanticsValidatorExprCollectionAccess.cpp`/
    `SemanticsValidatorExprNamedArgumentBuiltins.cpp` interaction TODO-4714
    already started tracing.
    Per-file breakdown (case count / file):
    23 wrapper_returned_map_method_resolution.cpp, 13
    wrapper_temporary_access_resolution.cpp, 10
    vector_stdlib_push_auto_inference.cpp, 9
    wrapper_temporary_templated_vector_methods.cpp, 9
    vector_helper_reordered_expression.cpp, 8
    vector_capacity_alias_named_args.cpp, 5
    vector_method_alias_struct_diagnostics.cpp, 4
    wrapper_returned_map_string_branch_paths.cpp, 4
    vector_helper_call_form_named_receivers.cpp (already TODO-4714's
    file - these 4 are additional cases in it beyond the ones TODO-4714
    already covers), 3 wrapper_temporary_tryat_contains_inference.cpp, 2
    vector_mutator_named_args.cpp, 1
    wrapper_returned_map_count_expression.cpp, 1
    vector_namespaced_alias_calls.cpp.
  - acceptance:
    - Every currently-failing `calls_flow_collections` case outside shard
      `811_820` is assigned to a named root-cause cluster, each with at
      least one representative case and its actual-vs-expected diagnostic
      recorded in `docs/failing_tests.md` or a linked doc. (Substantially
      done - see implementation_notes above; the exact per-case cluster
      assignment for all 92 is not yet exhaustively recorded, only the two
      dominant patterns and the per-file counts.)
    - Each identified cluster gets its own follow-up `TODO-XXXX` leaf (or
      is folded into an existing one) with `depends_on: TODO-4715`.
  - stop_rule: Triage and TODO-filing only - no source fixes in this leaf;
    that is explicitly deferred to the follow-up leaves it creates.

- [ ] TODO-4719: Fix remaining type_resolution_graph SoA-cluster compatibility failures
  - owner: ai
  - created_at: 2026-07-15
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-type-resolution-graph
  - scope: 10 test cases in
    `test_semantics_type_resolution_graph_snapshots.cpp` fail (CTest
    shards `type_resolution_graph_101_110` and `_111_120`, newly reachable
    after the TOTAL_CASES fix from the stale 18 to the real 177) - these
    are distinct from, and not fixed by, the `to_aos`/`to_aos_ref`
    owned-vs-borrowed-soa-receiver ordering bug already fixed this session
    (commit "Fix owned soa receivers misrouted to borrowed to_aos/to_aos_ref
    helper"). The 10 case names: "keeps helper-return borrowed soa read
    targets on canonical wrappers compatibility", "keeps method-like
    borrowed soa read targets on canonical wrappers compatibility", "keeps
    borrowed soa ref_ref targets on same-path helpers compatibility",
    "keeps builtin soa ref_ref targets on same-path helpers", "validates
    direct return method-like borrowed helper-return experimental soa
    reads", "keeps helper-return SoaVector mutator initializer facts on
    wrappers compatibility", "keeps helper-return borrowed soa direct-call
    targets on canonical wrappers compatibility", "keeps helper-return
    borrowed soa field views on canonical reads compatibility", "keeps
    borrowed local soa field views on canonical reads compatibility",
    "keeps method-like borrowed soa field views on canonical reads
    compatibility".
  - implementation_notes: At least one case ("keeps helper-return borrowed
    soa read targets on canonical wrappers compatibility") was already
    investigated in an earlier session: routing it through the real
    compile pipeline with genuine `SoaVector<T>`/`soaVectorNew<T>()`
    stdlib content hits a further `unknown method: /soa/push` blocker -
    possibly the test's `.push(...)` method-call form needs updating to
    the modern statement-form `push(values, ...)` syntax per the stdlib
    "surface syntax" migration referenced elsewhere in the codebase, but
    this wasn't confirmed. The other 9 cases are not yet individually
    investigated.
  - acceptance: All 10 cases pass; full `primestruct.semantics.type_resolution_graph`
    suite (177/177) is green.
  - stop_rule: If the `/soa/push` blocker turns out to require a broader
    stdlib mutator-syntax migration beyond fixing these test expectations,
    split that migration into its own TODO rather than expanding this
    leaf's scope.


- [ ] TODO-4725: Triage and fix newly-exposed non-semantics test failures from TODO-4720's shard-config fix
  - owner: ai
  - created_at: 2026-07-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-nonsemantics
  - depends_on: TODO-4720
  - scope: TODO-4720's shard-config fix (see its `progress_2026-07-16b`)
    exposed two large failure clusters CTest was never running before:
    (1) 31 shards / 60+ distinct cases in
    `primestruct.ir.pipeline.validation` (full names captured in
    `/tmp/claude-.../scratchpad/corrected_shards_gate.log` -
    session-scratch, re-run
    `ctest -R primestruct_ir_pipeline_validation --output-on-failure`
    after this session if needed); (2) 73 of 121 shards across
    `primestruct.compile.run.{smoke,vm.core,vm.collections,vm.outputs,
    emitters.cpp,examples}`'s newly-added coverage (full log at
    `/tmp/claude-.../scratchpad/newly_exposed_gate.log`), many timing
    out rather than cleanly failing.
  - implementation_notes: Before triaging individual cases, first
    determine whether the `Timeout` results in cluster (2) are genuine
    hangs/bugs or just need a larger `TIMEOUT` (these are full
    compile+execute-pipeline tests, inherently slower than semantics
    unit tests - the existing shards in these suites already use
    `TIMEOUT 900`, so a timeout at that budget is more likely a real
    performance regression or infinite loop than an under-provisioned
    budget, but confirm before assuming either way). Then triage into
    root-cause clusters the same way TODO-4715 did for the semantics
    find - do not fix cases one at a time without first grouping by
    shared cause. Specifically check whether any of cluster (2)'s
    failures share TODO-4723's "map receiver same-path-shadow" bug
    family (one sample already confirmed does - see TODO-4720's
    progress note) - if TODO-4723's remaining work fixes those, resolve
    this TODO's overlapping subset for free rather than duplicating the
    investigation.
  - acceptance: All newly-exposed failures triaged into root-cause
    clusters with their own follow-up TODOs (matching TODO-4714 through
    TODO-4723's structure for the semantics find); genuinely-fixable
    clusters fixed and verified via full-suite regression runs before
    each commit, per this session's established discipline.
  - stop_rule: This is a triage TODO, not a fix-everything TODO - once
    the failures are clustered and each cluster has its own follow-up
    TODO with a clear scope, this TODO is done. Do not attempt to fix
    300+ cases inline here.
  - progress_2026-07-16: Triaged cluster (1)
    (`primestruct.ir.pipeline.validation`, 30 failing cases, all in
    `test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp`
    - confirmed via a full standalone run of that file, log at
    `/tmp/claude-.../scratchpad/ir_val_full.log`) into 4 sub-clusters by
    root cause, matching TODO-4714..4723's granularity:
    (a) 7 cases of namespaced/rooted builtin-helper-matching bugs in
    small, pure-unit-test helper functions (`emitter::isSimpleCallName`,
    `semantics::isExplicitRemovedCollectionCallAlias`/
    `isExplicitRemovedCollectionMethodAlias`,
    `ir_lowerer::getBuiltinArrayAccessName`,
    `emitter::getBuiltinConvertName`, `emitter::isBuiltinNegate`,
    `emitter::getBuiltinComparison`/`getBuiltinMutationName`) - 3 of
    these 7 cases fixed inline this session (see below), remaining 4
    (spanning 5 functions) filed as TODO-4726; (b) 18 cases of soa
    canonical-path (`get`/`ref`/`reserve`/`to_aos`) method routing
    through the full compile pipeline (`parseAndValidate` REQUIRE
    failures and diagnostic-text mismatches) - filed as TODO-4727,
    which also covers at least one confirmed overlapping failure in
    cluster (2) (`primestruct.compile.run.vm.collections`'s
    `test_compile_run_vm_collections_vector_limits_pop_shadow.cpp`
    "rejects vm user vector pop call expression shadow" - now fails
    semantics with "unknown call target: /std/collections/vector/pop"
    instead of reaching its expected VM-lowering-stage rejection,
    because bare `pop(values)` call sugar without an import no longer
    finds a rooted user `/vector/pop` definition - the same
    same-path-shadow architecture gap as TODO-4723, just extended to
    pop/reserve/clear/remove_at/remove_swap instead of only
    count/capacity/at/at_unsafe); (c) 5 cases about
    `ir_lowerer::validateNativeProgramEffects`/`resolveEntryMetadataMasks`
    test fixtures producing "missing semantic-product callable summary:
    /main" instead of their expected behavior - looks like an unrelated
    API-shape drift in these fixtures, filed as TODO-4728.
  - progress_2026-07-16b: Fixed 3 of cluster (1)(a)'s 7 cases inline,
    verified regression-clean, and committed:
    1. `emitter::isSimpleCallName`'s `matchScopedBuiltinTail` lambda
       (`EmitterBuiltinCallPathHelpers.cpp` ~line 625) blindly tail-matched
       ANY path's last segment against a list of generic builtin names
       that includes collection-family names like `count`/`push`/
       `capacity` - so a removed alias like `/array/count` incorrectly
       matched "count". Fixed by requiring the path (minus leading `/`)
       start with `std/` before this fallback applies - every currently-
       passing use of this fallback already uses a `/std/...`-namespaced
       spelling (verified by reading every call site in the test file).
       Verified via a full standalone rerun of the test file (65->66
       passed, 30->29 failed, no other case changed) plus the full
       `primestruct.compile.run.emitters.cpp` 622-case compile+run
       regression (see below).
    2. `semantics::isExplicitRemovedCollectionCallAlias` and
       `isExplicitRemovedCollectionMethodAlias`
       (`SemanticsBuiltinPathHelpers.cpp` ~line 589-665) only recognized
       the dead legacy `/soa_vector/...` alias root, never the PUBLIC
       canonical `/soa/...` (bare) or `/std/collections/soa/...` (full)
       roots - so `*_ref` helpers retired in method/call-alias position
       (`count_ref`/`get_ref`/`ref_ref`/`to_aos_ref`) went unrecognized
       when spelled via the public soa root instead of the legacy one.
       Fixed by adding the same prefix-strip-and-check pattern for
       `soa_paths::publicSoaFolder()` (bare and
       `/std/collections/soa`-rooted) alongside the existing legacy-folder
       checks. Verified via the single test case plus a full
       `primestruct.semantics.calls_flow.collections` regression (see
       below).
    Both fixes committed together; full-suite regression logs and pass/
    fail counts recorded in `docs/failing_tests.md`'s 2026-07-16 section.

- [ ] TODO-4726: Fix remaining namespaced/rooted builtin-helper matching gaps (5 functions, 4 cases)
  - owner: ai
  - created_at: 2026-07-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-nonsemantics
  - depends_on: TODO-4725
  - scope: 4 remaining failing cases in
    `test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp`
    ("ir lowerer access helper rejects removed rooted vector access
    aliases", "ir lowerer access helper classifies namespaced access
    helpers", "ir lowerer helper keeps namespaced convert builtin
    tails", "emitter helper keeps parser-shaped rooted negate builtin",
    "shared helper bodies keep scoped stdlib builtins normalized"),
    spanning 5 distinct functions that each fail to recognize some
    namespaced/rooted spelling they're now expected to accept:
    `ir_lowerer::getBuiltinArrayAccessName` (needs to distinguish `get`/
    `get_ref` soa access verbs from `at`/`at_unsafe`, and currently has
    an early-return bailout at `IrLowererBuiltinNameHelpers.cpp:553-556`
    that returns false whenever the input exactly equals the registered
    canonical stdlib helper path - `unrootedStdlibVectorHelperPath`),
    `emitter::getBuiltinConvertName` (fails for namespacePrefix
    `/std/gfx/GfxError`), `emitter::isBuiltinNegate` (fails for bare `/`
    namespacePrefix), `emitter::getBuiltinComparison`/
    `getBuiltinMutationName` (fail for `/std/collections/soa_storage`
    and `/std/collections/experimental_soa_conversions`-style
    namespaces).
  - implementation_notes: unlike the two sibling bugs already fixed
    under TODO-4725 (which were both "too permissive"), these five are
    all "doesn't recognize a namespace it should" - each function is
    independent, so trace each with the gdb breakpoint-sweep technique
    established this session rather than assuming a shared root cause.
  - acceptance: all 4 named cases (7 CHECK failures) pass; full
    `primestruct.ir.pipeline.validation` and
    `primestruct.compile.run.emitters.cpp` regressions stay clean.
  - stop_rule: scoped to these 5 functions only - do not expand into
    TODO-4727's soa-routing-pipeline cluster from here.
  - progress_2026-07-17: Fixed 3 of the 5 functions
    (`emitter::isBuiltinNegate`, `emitter::getBuiltinConvertName`,
    `emitter::getBuiltinComparison`/`getBuiltinMutationName`), landing
    3 of the 4 named cases ("ir lowerer helper keeps namespaced convert
    builtin tails", "emitter helper keeps parser-shaped rooted negate
    builtin", "shared helper bodies keep scoped stdlib builtins
    normalized"). Root causes: `isBuiltinNegate` didn't route through
    `normalizeInternalSoaStorageBuiltinAlias` before its own leading-
    slash strip, so a bare `/` namespacePrefix (producing a double-
    slash-prefixed path via `resolveExprPath`) left one leading slash
    in place and made the immediate `find('/')` check reject it -
    fixed by adding the same `normalizeInternalSoaStorageBuiltinAlias`
    wrapping `getBuiltinConvertName` already used (which incidentally
    strips both slashes and was already passing its own bare-`/` case).
    `getBuiltinConvertName`, `getBuiltinComparison`, and
    `getBuiltinMutationName` all gave up as soon as any `/` remained
    after `normalizeInternalSoaStorageBuiltinAlias` (which only
    recognizes a fixed 11-prefix allowlist, missing arbitrary
    `/std/...` subsystems like `/std/gfx/GfxError` or literal
    `experimental_soa` spellings) - fixed by adding a shared
    `stdNamespacedBuiltinTailMatches` helper (mirrors TODO-4725's
    already-verified-safe `isSimpleCallName` fix: any path that starts
    with `std/` may still tail-match its target name, but a non-std
    multi-segment path - a removed alias, a rooted compat alias - must
    not). Verified via a full standalone rerun of the ir.pipeline.
    validation test file (67->70 of 95 passed, matching exactly +3, no
    other case changed) and a full `primestruct.compile.run.emitters.cpp`
    regression (500/622 passed both before and after - these 3 fixes
    don't affect any case in that suite, only the synthetic-Expr unit
    tests in the ir.pipeline.validation file). Remaining: 1 case
    ("ir lowerer access helper rejects removed rooted vector access
    aliases" / "ir lowerer access helper classifies namespaced access
    helpers" - `ir_lowerer::getBuiltinArrayAccessName`) still open -
    this one needs an actual behavior addition (recognizing `get`/
    `get_ref` as distinct from `at`/`at_unsafe`, and revisiting the
    `unrootedStdlibVectorHelperPath` early-return bailout), not just a
    namespace-recognition broadening like the other 3, so it's larger
    in scope than this progress note's fixes.

- [ ] TODO-4727: Fix soa canonical-path (get/ref/reserve/to_aos) method routing through the full compile pipeline
  - owner: ai
  - created_at: 2026-07-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-nonsemantics
  - depends_on: TODO-4725, TODO-4723
  - scope: 18 cases in
    `test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp`
    (from "bare soa count helper lowers through wrapper return routing
    compatibility" through "borrowed helper-return experimental wrapper
    bare conversion alias lowers through generic wildcard import") -
    `parseAndValidate` REQUIRE failures and diagnostic-text mismatches
    around bare/rooted/namespaced soa `get`/`ref`/`reserve`/`to_aos`
    method routing, wrapper-return compatibility, and imported vs. bare
    call forms. Confirmed overlapping with at least one
    `primestruct.compile.run.vm.collections` "newly exposed" shard
    failure from TODO-4725's cluster (2):
    `test_compile_run_vm_collections_vector_limits_pop_shadow.cpp`'s
    "rejects vm user vector pop call expression shadow" now fails
    semantics with "unknown call target: /std/collections/vector/pop"
    (instead of reaching its expected later VM-lowering-stage
    rejection) because bare `pop(values)` call sugar without an import
    no longer finds a rooted user `/vector/pop` definition - the same
    same-path-shadow architecture gap TODO-4723 is already tracking,
    just extended to pop/reserve/clear/remove_at/remove_swap instead of
    only count/capacity/at/at_unsafe.
  - implementation_notes: this is a large, cohesive feature-completion
    cluster, not independent point bugs - triage with TODO-4715's
    clustering approach before touching code. Check whether TODO-4723's
    still-open "definition-level namespace-hygiene check" work resolves
    much of this cluster for free before designing anything new here.
  - acceptance: the 18 named cases pass; the confirmed overlapping
    vm.collections shard (and any others found sharing the same cause)
    verified fixed too.
  - stop_rule: if triage finds more than 2-3 distinct root causes
    inside this scope, split further into separately-scoped TODOs
    rather than one giant fix.
  - progress_2026-07-18: Completed the triage of this cluster by
    hand-compiling the failing sources through `primec` directly.
    Findings, in increasing depth:
    1. **The 18 test cases use retired spellings.** They import
       `/std/collections/internal_soa/*` (a module that no longer
       exists after the TODO-4633 merge into `soa`), declare receivers
       as `SoaVector<Particle>` (user-facing use of the internal
       backing type now resolves method calls to the dead legacy
       `/std/collections/soa_vector/*` family - "unknown method"), and
       call internal `soaVector*` helpers directly (fails with
       `meta.field_count requires struct type argument: type:Particle`
       - the reflection query on the template argument only resolves
       when instantiated through the public surface, an asymmetry
       that's part of the internal surface's retirement, since the
       stdlib's own `soa<T>()` wrapper calling the same
       `soaVectorNew<T>()` works fine). The MODERN public surface
       (`soa<T>` type, `soa<T>()`/`single`/`from_aos` constructors,
       method-call and explicit `/std/collections/soa/count<T>(...)`
       forms) validates, lowers, and runs correctly - verified by
       hand-compiling modernized versions of the failing sources.
       Modernizing the 18 tests is the likely resolution, BUT see (2):
       doing so before the bare-call dispatch bug below was fixed
       would have turned silently-wrong runtime behavior green.
    2. **Real product bug found under this cluster (now fixed): bare
       `count(values)` on a public `soa<T>` receiver returned the
       struct's slot count (a constant 5) instead of the element
       count** - on BOTH the VM and C++-emitter backends, for any
       element count and any struct field count, while `values.count()`
       and `/std/collections/soa/count<T>(values)` returned correct
       values. Root cause (traced via gdb breakpoint sweep over every
       `resolvedOut =` site): `resolveVectorHelperMethodTarget`
       (`SemanticsValidatorExprVectorHelpers.cpp`) had receiver
       branches for the INTERNAL soa type paths but none for the
       public `soa`/`soa<T>` type, so public-soa receivers fell into
       the generic struct-method fallback, fabricating a
       definition-less `/soa/count` direct-call target; the IR
       lowerer's soa-count fast path only recognizes
       `/std/collections/soa/count` spellings, so the call fell
       through to the generic array/struct count emission - a raw
       slot-count memory read. Fixed by adding a public-soa branch to
       both the Name-receiver and Call-receiver paths, routing
       `isSoaReadRefHelperName` helpers (count/count_ref/get/get_ref/
       ref/ref_ref) through
       `preferredSoaHelperTargetForCurrentImports`, which keeps a
       genuine user same-path `/soa/<helper>` shadow first (verified:
       a user `/soa/count` override still wins) and canonicalizes to
       the public `/std/collections/soa/<helper>` surface otherwise.
       Verified on 9 hand-built probes covering empty/1/3-element
       soas, 1- and 2-field structs, Name and wrapper-Call receivers,
       method/explicit/bare forms, and the user-shadow case - all
       correct after the fix, plus the full regression gates (see the
       commit).
    3. The remaining work for this TODO is now purely the test-side
       modernization of the 18 cases (per (1)) plus deciding whether
       any of the "compatibility routing" behaviors they pinned are
       still supposed to exist at all - which overlaps with TODO-4723's
       open same-path-shadow architecture question and should be
       decided together with it.
  - progress_2026-07-18b: Attempted the test-side modernization and
    found it BLOCKED on real gaps in the modern surface itself - this
    crosses the stop_rule threshold, so recording the complete probe
    matrix instead of fixing further inline. Probing every failing
    case's shape against the current compiler (both `import
    /std/collections/*`, which does NOT load the soa module - the
    wildcard doesn't recurse - and the correct `import
    /std/collections/soa/*`):
    - WORKS today: `soa<T>()`/`single<T>`/`from_aos<T>` construction,
      `values.count()`/`values.get(i)`/`values.ref(i)` methods on
      Name receivers, bare `count(values)` (after this session's
      dispatch fix), explicit `/std/collections/soa/count<T>(values)`,
      `values.to_aos()` (semantics, with soa import).
    - BROKEN in the modern surface (each a distinct root cause, all
      reproduced with the correct soa import):
      (a) bare `get(values, 0i32)` in binding/expression position:
          "template arguments required for
          /std/collections/soa/soaVectorGet" - the bare-get bridge
          doesn't propagate the receiver's element type as a template
          argument (bare `count` does since the dispatch fix; `get`
          takes the extra index param and goes through a different
          bridge).
      (b) `values.reserve(2i32)`/`values.push(...)` METHOD calls:
          "unknown call target: reserve" - the SoaVector struct's own
          mutator methods aren't reachable through method dispatch
          (likely intercepted by the collection-builtin machinery
          before struct-method resolution; note the imports_operations
          suite's passing soa tests never exercise mutator methods).
      (c) `values./std/collections/soa/to_aos()` explicit canonical
          slash-method: semantics resolves it but VM lowering fails
          with "semantic-product method-call target missing lowered
          definition: /std/collections/soa/to_aos".
      (d) method chains on call-expression receivers
          (`Holder{}.cloneValues().count()`): "field access requires
          struct receiver".
      (e) no-import diagnostics leak the dead legacy family in
          user-facing text: bare `ref(vectorValues, 0)` with no
          imports reports "unknown method:
          /std/collections/soa_vector/ref" (the retired soa_vector
          spelling), and `/soa/reserve(vectorValues, 4)` reports
          "reserve is only supported as a statement" even in statement
          position.
    - Also verified: bare `ref(soaValues, 0)` with no imports now
      correctly passes semantics and stops in lowering (the original
      pinned contract for one form of case 8), while the method and
      /soa/ rooted forms of the same call fail semantics with the
      generic "binding initializer validateExpr failed" - three forms,
      three different behaviors in one test case.
    Conclusion: the 18 cases cannot be turned green by test edits
    alone - (a)-(d) are real modern-surface feature gaps needing
    compiler work, and the test contracts should be re-pinned only
    after those land. Follow-up compiler work filed as TODO-4731;
    this TODO stays open for the eventual test modernization pass
    gated on it.

- [ ] TODO-4739: Fix vector/at direct-call override precedence - multiple redundant, inconsistent native-fastpath classification sites
  - owner: ai
  - created_at: 2026-07-22
  - phase: Hidden test failure remediation (emitters cluster)
  - parallel_track: hidden-test-failures-emitters
  - depends_on: (none)
  - scope: `tests/unit/compile_run/test_compile_run_emitters_namespaced_vector_push_and_count_helpers.cpp`
    has 3+ failing cases where a user overrides the canonical
    `/std/collections/vector/at` path with a differently-typed
    definition (e.g. `return<int>` instead of the generic element
    type) and calls it via an explicit/direct (non-method) call with
    named, reordered arguments (`/std/collections/vector/at([index]
    2i32, [values] wrapVector())`). Per the session's established
    "canonical override always wins" policy (confirmed repeatedly for
    map count/capacity/at in `wrapper_map_count_sugar.cpp` and
    `explicit_vector_count_capacity_helpers.cpp`), the override should
    win. It currently doesn't: compilation fails with "EXE IR lowering
    error: native backend only supports at() on numeric/bool/string
    arrays or vectors...". The equivalent METHOD-CALL form
    (`wrapVector().at(2i32)`) already works correctly today (verified
    directly: returns the override's value, not the builtin's).
  - implementation_notes: this looked like a narrow one-line asymmetry
    at first (`IrLowererNativeTailDispatch.cpp:828`'s
    `isExplicitVectorAccessCall && explicitHelperName == "at_unsafe"`
    bypass excludes plain `"at"`), but a full gdb-traced investigation
    found it is NOT a single bug:
    1. `IrLowererNativeTailDispatch.cpp`'s classification
       (`isExplicitVectorAccessCall`, ~line 812) computes
       `arrayVectorTargetInfo` from `expr.args.front()` unconditionally
       - for reordered named-arg calls where `index` is written before
       `values`, `args.front()` is the `index` arg, not the vector, so
       `isVectorTarget` is false and any bypass gated on it silently
       never fires. Making the "at" bypass unconditional (dropping the
       `isVectorTarget` gate, matching how `at_unsafe`'s
       `isPublishedVectorAtUnsafeImplementationCall` branch already
       behaves unconditionally in practice) still didn't fix the repro.
    2. That's because `return(callExpr)` statements do NOT go through
       `tryEmitNativeCallTailDispatch` at all for this shape - gdb
       showed the actual call chain is
       `validateArrayVectorAccessTargetInfo <-
       emitArrayVectorIndexedAccess <- emitBuiltinArrayAccess <-`
       a completely separate "return statement fast path" fragment in
       `IrLowererLowerStatementsExpr.h` (included into
       `IrLowererLowerReturnEmitStage.cpp`), around line 1198's
       vector-helper-family `if` block. That block's own path-prefix
       check (`rawPath.rfind(collectionMemberRoot("vector"), 0) == 0`,
       where `collectionMemberRoot("vector")` returns the UNROOTED
       `"std/collections/vector/"`) never matches the ROOTED canonical
       call spelling (`"/std/collections/vector/at"`), so this whole
       block - which already has correct-looking `directCallee`
       (found via `resolveDefinitionCall(expr)`, confirmed via gdb to
       correctly resolve to the override, `fullPath` correct) -
       - is skipped entirely for this call shape.
    3. Fixing #2's leading-slash gap (adding a rooted canonical-path
       OR-branch, reusing the same `isCanonicalPublishedStdlibSurfaceHelperPath`
       pattern the key-value sibling lambda already uses) gets the
       call INTO the block, but the block's own `isDirectVectorBuiltin`
       check (~line 1236) DELIBERATELY treats `accessName ==
       "at"/"at_unsafe"` as "let the native path handle it, do not
       inline the override" - this appears to be intentional (probably
       to preserve bounds-checked indexed-access codegen for the
       common no-override case), meaning the override precedence for
       `at`/`at_unsafe` was ALWAYS meant to be decided later, inside
       `emitBuiltinArrayAccess`/`tryEmitNativeCallTailDispatch` (back
       to problem #1) - not here. So #1 and #2 aren't independent bugs
       to fix in sequence; #2's gap has to be closed WITHOUT changing
       the "defer to native path" outcome for `at`/`at_unsafe`, and
       then #1's mechanism needs to correctly detect the override once
       it gets there.
    4. `semanticKeyValueAccessHelperKeepsBuiltinReturn` (the existing
       override-detection heuristic already used successfully for the
       map count/capacity/at cases and for the vector `at` METHOD-CALL
       bypass) does NOT reliably detect the vector `at` override for
       DIRECT calls: probing showed `findSemanticProductDirectCallTarget`
       for the reordered-arg call resolves to the bare path string
       `"/std/collections/vector/at"`, and looking THAT UP via
       `semanticProgramLookupPublishedReturnFactByDefinitionPathId`
       (keyed by `insert_or_assign` on a shared path-string ID, in
       `SemanticPublicationBuilders.cpp::publishReturnFacts`) returns
       `structPath == "vector"` (i.e. "keeps builtin return", override
       NOT detected) even though a real, differently-typed override is
       registered at that exact call site. This is suspected to be a
       path-string collision: the builtin's own generic-template
       return fact and the user override's return fact are both keyed
       by the same interned path string, and whichever was
       `push_back`'d last into `SemanticProgram::returnFacts` wins the
       map lookup, independent of which definition actually resolved
       for THIS call. Why the same heuristic works for map
       count/capacity (return type is a concrete scalar, so no
       collision in practice) and for vector `at`'s METHOD-CALL variant
       (which reaches an entirely different, not-yet-identified
       resolution path - it never even reaches this heuristic, per gdb/
       debug-print evidence: `hasPublishedVectorAccessName` is
       unconditionally false for method calls at
       `IrLowererNativeTailDispatch.cpp:773`, and
       `getBuiltinArrayAccessName` returns false for the literal
       canonical path per its own exclusion at
       `IrLowererBuiltinNameHelpers.cpp:553-556`, so method-call `.at()`
       must resolve via a third, still-unidentified code path) but not
       for vector `at`'s direct-call variant is not yet understood.
    5. A probe with CORRECT (non-reordered) named-argument order
       (`/std/collections/vector/at([values] wrapVector(), [index]
       2i32)`, override defined) also fails, but differently: it
       compiles successfully yet returns the WRONG runtime value (0,
       matching neither the builtin's real element (7) nor the
       override's (32)) - confirming named-argument handling for this
       specific canonical direct-call shape is broken independent of
       argument order, and independent of any of this session's edits
       (reproduced against a clean stash of the base commit too).
    All exploratory edits from this investigation were reverted (no
    net diff) rather than landed partially, since neither fix attempt
    (extending the `IrLowererNativeTailDispatch.cpp` bypass alone, or
    closing the `IrLowererLowerStatementsExpr.h` leading-slash gap
    alone) produced a passing repro, and landing a change that touches
    shared, heavily-exercised native-fastpath classification code
    without a verified-working fix risks regressing the ~550+ other
    passing emitters cases that also flow through these same functions.
  - acceptance: the three named-arg-reordered `.../vector/at` override
    tests in `test_compile_run_emitters_namespaced_vector_push_and_count_helpers.cpp`
    pass with their CURRENT contracts (override wins, no re-pinning
    needed - the tests already encode the correct "canonical override
    wins" expectation); the correct-order-named-arg probe (item 5
    above) also produces the override's value (32), not 0 or 7; full
    sharded `ctest -R primestruct_compile_run_emitters_cpp_` run shows
    zero new failures relative to the pre-fix baseline.
  - stop_rule: do not attempt another single-function patch without
    first re-deriving the FULL set of call sites that classify
    `at`/`at_unsafe` calls to canonical vector paths (at minimum:
    `IrLowererNativeTailDispatch.cpp`'s `isExplicitVectorAccessCall`
    path, `IrLowererLowerStatementsExpr.h`'s return-statement fast
    path, and whatever third path method-call `.at()` actually uses -
    item 4 above) and deciding on ONE canonical place where override-
    vs-builtin precedence gets decided, with the others deferring to
    it - the current architecture has at least three places that each
    partially re-implement this decision with different (and in one
    case demonstrably wrong) logic, and patching one at a time without
    that map produces exactly the failed-attempt cycle this session
    went through.
  - progress_2026-07-22c: the SAME class of problem (a `/vector/...`
    alias-spelled receiver call not being recognized as vector-access
    for type-inference purposes) also affects the LEGACY C++ emitter,
    not just ir_lowerer's native tail dispatch - a fourth site.
    `tests/unit/compile_run/test_compile_run_emitters_vector_receiver_metadata_resolution.cpp`
    has 3 failing unit-style cases calling
    `primec::emitter::resolveMethodCallPath` directly with a receiver
    `Expr` whose call name is the alias-rooted `/vector/at` (not
    canonical `/std/collections/vector/at`) and `returnKinds` populated
    only under the canonical key; all three expect
    `resolveMethodCallPath` to succeed. Traced the live call path
    (`resolveMethodCallPath` -> the `receiver.kind == Expr::Kind::Call`
    branch at `EmitterBuiltinMethodResolutionHelpers.cpp:522` ->
    `inferMethodResolutionPrimitiveTypeName` ->
    `vectorHelperMemberNameFromExpr` in
    `EmitterBuiltinMethodResolutionTypeInferenceHelpers.cpp:53`) to the
    actual gate: `vectorHelperMemberNameFromExpr` calls
    `resolvePublishedCollectionSurfacePathMemberName(path, metadata,
    /*includeImportAliases=*/false, ...)` - the `false` means an
    alias-rooted `/vector/at` spelling is never recognized as a vector
    helper member at all, so the function bails before ever reaching
    `collectionHelperPathCandidates` (a DIFFERENT, unrelated candidate-
    list helper also named similarly, used by sibling inference lambdas
    in the same file). Tried the narrow fix of adding a `/vector/`
    cross-path-to-canonical branch to `collectionHelperPathCandidates`
    (mirroring its existing `/array/` branch) first, since that looked
    like the obvious gap - it was a no-op for this repro (confirmed via
    rebuild + targeted doctest run, all 3 cases still fail identically)
    because it's the wrong function; reverted (zero net diff, `git
    checkout --`) once confirmed. The real fix point
    (`includeImportAliases=true` in `vectorHelperMemberNameFromExpr`)
    was NOT attempted - `vectorHelperMemberNameFromExpr` is a shared
    classifier used by several other lambdas in the same function
    (`isBareVectorAccessMethod`, `isExplicitVectorAccessSlashMethod`,
    `isExplicitVectorCountCapacityDirectCall`, etc.) that each encode
    their own precedence assumptions about alias-vs-canonical
    receivers, so flipping the flag globally risks changing behavior
    for other already-passing tests in ways that would need a full
    collections+emitters regression gate to catch, which didn't fit in
    this session's remaining budget. Folding this into TODO-4739 rather
    than filing separately since it's the same underlying pattern
    (vector `at`/`at_unsafe` alias-path recognition inconsistency
    across independent classification sites) - the eventual mapping
    pass this TODO calls for should include this legacy-emitter site as
    a fourth entry alongside the three ir_lowerer ones.
  - progress_2026-07-22e: found a further affected case while sweeping
    the emitters cluster:
    `test_compile_run_emitters_wrapper_direct_call_receiver_fallbacks.cpp`'s
    "wrapper canonical direct-call struct method chain forwarding in
    C++ emitter" (an override of `/std/collections/vector/at` with a
    struct return type, called via plain POSITIONAL direct-call syntax
    `/std/collections/vector/at(wrapValues(), 2i32).tag()` - notably
    NOT named/reordered args this time) reproduces the identical "EXE
    IR lowering error: struct parameter type mismatch" already seen for
    the named-arg-reordered repros. This confirms the bug is broader
    than the named-argument-ordering angle originally suspected - it
    affects positional direct-calls too whenever the override's return
    type is a struct (not a plain scalar). Left unfixed/unre-pinned per
    this TODO's stop_rule; noted here so the eventual mapping pass has
    another concrete repro shape to check against.

- [ ] TODO-4731: Close the modern soa surface gaps (bare get template args, method mutators, canonical to_aos lowering, call-receiver method chains, legacy-path diagnostics)
  - owner: ai
  - created_at: 2026-07-18
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-soa-surface
  - depends_on: TODO-4727
  - scope: TODO-4727's `progress_2026-07-18b` probe matrix found five
    distinct real gaps in the MODERN public soa surface (all
    reproduced with the correct `import /std/collections/soa/*`):
    (a) bare `get(values, index)` fails with "template arguments
    required for /std/collections/soa/soaVectorGet" (the bare-get
    bridge doesn't propagate the receiver's element type);
    (b) `values.reserve(...)`/`values.push(...)` method calls fail
    with "unknown call target" (the SoaVector struct's own mutator
    methods aren't reachable through method dispatch - likely
    intercepted by collection-builtin machinery first);
    (c) `values./std/collections/soa/to_aos()` passes semantics but
    fails lowering with "semantic-product method-call target missing
    lowered definition: /std/collections/soa/to_aos";
    (d) method chains on call-expression receivers
    (`Holder{}.cloneValues().count()`) fail with "field access
    requires struct receiver";
    (e) several no-import diagnostics leak the retired
    /std/collections/soa_vector/* family in user-facing error text.
  - implementation_notes: each gap is independent - fix and regress
    separately using the session's established gate discipline
    (calls_flow.collections 131-shard gate, ir.pipeline.validation
    module file, emitters.cpp). The probe sources for all five live
    in this session's build-debug scratch (p5/p6/p16/p17/p18/p19
    .prime files - reconstruct from TODO-4727's notes if gone).
    TODO-4727's 18 test-case modernizations are gated on (a)-(d);
    re-pin those tests only after the gaps close.
  - acceptance: all five probe shapes behave correctly (or are
    explicitly rejected with a canonical-path diagnostic where that's
    the decided contract), and TODO-4727's test modernization can
    proceed without papering over compiler gaps.
  - stop_rule: one gap per commit with full gates; if (b)'s method
    dispatch turns out to require the TODO-4724 resolveMethodTarget
    decomposition first, do that refactor under its own TODO rather
    than inline here.
  - progress_2026-07-18: gap (a) fixed. Root cause: implicit template
    inference (TemplateMonomorphImplicitTemplateInference.h) compared
    receiver families via normalizeCollectionReceiverTypeName, which
    maps SoaVector-family spellings to the internal soa family but
    left the public "soa" spelling unmapped, so a soa<T>-typed
    argument could not unify against a helper's [SoaVector<T>]
    parameter and bare get(values, index) died with "template
    arguments required for /std/collections/soa/soaVectorGet". Fix is
    an inference-local equivalence (soa / std/collections/soa also
    map to the internal soa family name) rather than widening the
    shared classifier - a first attempt that widened
    normalizeCollectionReceiverTypeName itself flipped monomorph
    rewrite decisions and broke genuine user same-path /soa/<helper>
    shadows (shadow1 probe returned 1 instead of 42). Verified: 9
    probes correct (pa=8, cnt3=3, cnt4=3, shadow1=42, soa_test5-9,
    cnt2), calls_flow.collections gate identical 35-shard failing
    set, ir.pipeline.validation module file identical 74/95,
    emitters.cpp gate identical baseline.
  - progress_2026-07-18b: gaps (b), (c), (d) root-caused.
    (b) values.reserve/push: the public-soa dispatch branches in
    resolveVectorHelperMethodTarget and the publicPath preference in
    preferredSoaHelperTargetForCurrentImports were gated on
    isSoaReadRefHelperName, excluding reserve/push/to_aos/to_aos_ref
    even though the public wrappers exist; widened to
    isSupportedCompatibilitySoaHelperName (still guarded by actual
    visibility of the public wrapper).
    (d) soa<T>(...).count() on call receivers:
    hasVisibleExperimentalSoaSamePathHelper (SemanticsValidate.cpp)
    counted the canonical /std/collections/soa/<h> path - which every
    `import /std/collections/soa/*` covers - as proof of a user
    /soa/<h> shadow, so rewriteExperimentalSoaSamePathHelperMethodExpr
    fabricated a definition-less "/soa/<h>" call that failed with the
    legacy "unknown method: /std/collections/soa_vector/<h>" text
    (also the gap (e) leak shape for this path). Tightened the check
    to genuine /soa/<h> shadows only; a fabricated same-path rewrite
    without a user shadow could never validate, so the change can only
    turn errors into successes. Found via gdb hardware watchpoint on
    the AST node's name buffer.
    (c) values./std/collections/soa/to_aos(): semantic product
    publishes method_call_targets[0] resolved to the RAW template path
    /std/collections/soa/to_aos (no __t specialization suffix, unlike
    the direct-call targets in the same product) - the monomorphizer
    never instantiates method-call targets spelled with the explicit
    canonical path, so lowering finds no definition. Not fixed yet.
  - progress_2026-07-19: gaps (b) and (d) FIXED and gated. Beyond the
    root causes above, landing them required two more fixes: (1) a
    canonical-args encoding leak - the specialization-cache reverse
    lookups in TemplateMonomorphExperimentalCollectionReceiverResolution.h
    and TemplateMonomorphExpressionRewrite.h returned cache-key args
    ("type:Particle") verbatim, instantiating
    soaSupportedFieldCount<type:Particle> and producing the historical
    "meta.field_count requires struct type argument: type:<T>"
    diagnostic family; added stripMangledTemplateArgKindPrefix next to
    the joinMangledTemplateArgs encoder and applied it at both sites;
    (2) the gap (b) name-receiver rewrite (public soa<T> bindings in
    rewriteExperimentalSoaSamePathHelperMethodExpr) must be gated on
    the canonical surface actually being visible
    (publicSoaSurfaceVisible) - ungated it turned valid no-import
    retired-binding programs into dead-path errors ("get method
    validates retired soa binding" regression caught by the gate).
    Verification: 14-probe matrix all correct (incl. gapb1=1 for
    reserve/push, gapd2=3 for soa<T>(...).count()); collections gate:
    45 new failing cases, every one a pinned-old-behavior contract
    (re-pinned separately, see below); ir.pipeline.validation module:
    75/95, strictly better than the 74/95 baseline ("imported builtin
    soa method mutators lower through canonical helper routing" and
    both soa get/ref named-args cases now pass, zero new failures);
    emitters.cpp gate: identical 501/622 failing set. Residual gaps
    discovered while re-pinning (pinned with notes in the test file):
    (f) borrowed helper-return receivers still route count_ref through
    the retired-family diagnostic; (g) rooted helper paths on
    specialized receivers are rejected with the specialized-type
    spelling; (h) borrowed method-like ref_ref is not routed to the
    canonical wrapper; (i) bare ref with a user same-path /soa/ref
    shadow on a call-receiver argument resolves to the stdlib wrapper
    (escape diagnostic) instead of the shadow.

  - progress_2026-07-19c: gap (c) FIXED. The pre-validate method
    desugar (rewriteExperimentalSoaSamePathHelperMethodExpr) omitted
    to_aos/to_aos_ref from its helper allowlist, so to_aos method
    sugar - both bare values.to_aos() and the explicit
    values./std/collections/soa/to_aos() spelling - stayed a
    method-call target that the monomorphizer never instantiates
    (semantic product showed the raw template path with no __t
    suffix). Added both helpers to the allowlist and the
    visibleSoaHelpers precompute. One guard proved necessary: when a
    user program shadows the canonical path with type-differentiated
    overloads, the desugared direct call cannot type its rewritten
    call receiver for overload selection ("arg0 type=unknown" =>
    spurious ambiguity), so the rewrite now skips such helpers
    (overloadedCanonicalHelpers) unless a same-path /soa shadow wins
    anyway - caught by "method-like canonical soa helper shadows
    coexist as type-differentiated overloads" in the gate. Side
    effect: residual gap (g) (rooted location(values) read-only
    method chains) now validates end-to-end. Re-pinned 4 more to_aos
    contracts to success. Verification: 16-probe matrix correct
    (gapc1=2/gapc2=3 explicit canonical, gapc3=2 bare method);
    re-pinned files 270/270; collections gate identical 35-shard
    baseline; ir module identical 75/95; emitters gate identical
    501/622.
  - progress_2026-07-19b: the 45 pinned-old-behavior contracts in
    test_semantics_calls_and_flow_collections_container_error_and_result_helpers.cpp
    (44) and ..._soa_vector_builtins_named_args.cpp (1) are re-pinned:
    38 flip to plain success (validateProgram + empty error), two
    "push conflict" cases now pin the borrow checker correctly
    rejecting push while a ref borrow is live, three pin the residual
    (f)/(g)/(h) diagnostics, one pins the residual (i) escape
    diagnostic, and the uppercase-SoaVector case drops its retired
    internal_soa_vector import (previously masked by the metadata
    failure). Both files: 270/270 green.
- [ ] TODO-4728: Fix ir_lowerer effects-unit test fixtures missing semantic-product callable summaries
  - owner: ai
  - created_at: 2026-07-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-nonsemantics
  - depends_on: TODO-4725
  - scope: 5 cases in
    `test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp`
    ("ir lowerer rejects non-eliminated reflection query paths", "ir
    lowerer effects unit prefers semantic product callable summaries",
    "ir lowerer effects unit skips semantic callable summaries for sum
    types", "ir lowerer effects unit keeps nested expression effect
    checks syntax owned", "ir lowerer effects unit resolves entry
    metadata masks from semantic product"). All fail with "missing
    semantic-product {callable summary,direct-call semantic id}: /main"
    instead of their expected behavior/diagnostic - looks like the test
    fixtures build a `semanticProgram`/`SemanticProduct` object that no
    longer satisfies whatever now populates callable summaries for
    `/main`, an API-shape drift unrelated to the collections/soa work
    covering the rest of this file (different subsystem: effects
    validation and reflection-query elimination).
  - implementation_notes: determine which side is stale before fixing -
    it's equally plausible the test fixtures need updating for a real,
    correct production change as that production code regressed.
  - acceptance: all 5 cases pass without weakening the effects/
    reflection-query-elimination validation they exist to test.
  - stop_rule: if the root cause is a legitimate production API change,
    fix the test fixtures, not production code - do not force behavior
    to match possibly-stale fixtures without first confirming which
    side is wrong.
  - progress_2026-07-17: Confirmed the root cause is stale test
    fixtures, not a production regression, and fixed 4 of the 5 cases
    by updating the fixtures. `findSemanticProductCallableSummary`
    (`IrLowererSemanticProductTargetAdapters.cpp:373`) resolves through
    `semanticProgram->publishedRoutingLookups.callableSummaryIndicesByPathId`,
    an index map that a real semantic-validation publication pass
    populates (`SemanticPublicationBuilders.cpp`) - these 4 fixtures
    hand-build a `SemanticProgramCallableSummary` and push it onto
    `callableSummaries` directly, but never register it in that index
    map, so the lookup always misses. Fixed by adding the matching
    `callableSummaryIndicesByPathId[fullPathId] = index` registration
    right after each `push_back` (test-only change, zero production
    code touched). Verified via a full standalone rerun of the test
    file: 70->74 of 95 passed, exactly +4, no other case changed.
    Remaining 1 case ("ir lowerer rejects non-eliminated reflection
    query paths") is NOT a simple fixture gap like the other 4 - IR
    lowering now hard-requires a non-null, semantically-validated
    `SemanticProgram` (rejects `nullptr` outright with "semantic
    product is required for IR lowering", confirmed via
    `test_ir_pipeline_backends_registry.cpp`'s own pinned test for that
    exact behavior), but running real validation on this test's source
    causes the reflection query to be eliminated by validation itself,
    so IR lowering never sees an "un-eliminated" one to reject (it hits
    an unrelated "does not support string literal statements" error
    instead). Reproducing the test's original intent now needs a
    semantic product that passes the required-callable-summary and
    direct-call-coverage checks while still representing an
    intentionally un-eliminated reflection call - not investigated
    further this session; left as this TODO's one remaining case.
    Reverted this specific case back to its original (still-failing)
    form rather than landing an incomplete fix.
  - related_2026-07-17: TODO-4729's own 7th case
    ("preserves inline-call Result metadata from caller-scoped
    parameter defaults" in `test_ir_pipeline_conversions_numbers.cpp`)
    hits the identical class of failure - a hand-constructed/injected
    `Expr` with `semanticNodeId == 0` fails lowering with "missing
    semantic-product direct-call semantic id: /consume -> greeting".
    Whoever resolves this TODO's remaining case should check whether
    the fix also resolves that one, rather than solving the same
    architectural gap twice.

- [ ] TODO-4732: Cut compile-run test runtimes with semantic-product golden comparisons
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: many compile-run tests pay the full primec semantics + IR
    lowering + clang + link + run cost (~40-60s/case in Debug) only to
    assert an exit code that is a proxy for a routing decision. Idea
    (from the project owner): compare a stored artifact instead of
    running the full pipeline. Design sketch agreed in-session:
    prefer storing the SEMANTIC PRODUCT routing tables
    (direct_call_targets / method_call_targets) over lowest-level IR
    or generated C++ - it is tiny, stable across lowering refactors,
    available before clang, and pins exactly the decision under test;
    generated C++ churns cosmetically and IR goldens churn on slot or
    ordering refactors. Guard rails: goldens enshrine
    recording-day bugs (this session spent its bulk un-pinning ~200
    rotted contracts), so the refresh workflow must force human diff
    review, and a thin end-to-end tier that actually runs binaries
    must remain (only real runs catch miscompiles and VM/native
    divergence). Execution order across the test-runtime track: take
    the independent quick wins FIRST - TODO-4734 (RelWithDebInfo
    runner), TODO-4733 (vm-mode migration), TODO-4736 (runtime
    preamble prebuild), TODO-4735 (shared stdlib product) - plus the
    TODO-4737 lowering invariant and TODO-4738 duration telemetry;
    THIS golden-comparison item comes last, scoped to whatever is
    still slow once those land. Note the goldens also cannot see
    lowering-stage failures (the gap (c) class) - that is TODO-4737's
    job, not this item's.
  - acceptance: combined with the track's other items, emitters-suite
    wall time drops by an order of magnitude without losing the
    end-to-end miscompile net.

- [ ] TODO-4736: Prebuild the generated-C++ runtime preamble for exe-mode tests
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: test-runtime
  - scope: every exe-mode case makes clang compile ~850 lines of
    generated C++ whose stack-machine runtime preamble is identical
    across tests. Split the preamble into a precompiled header or a
    prebuilt object the generated tail links against, so clang only
    compiles the program-specific part.
  - acceptance: measured clang-step reduction on a fixed case set.

- [ ] TODO-4737: Add a lowered-module invariant - no published method-call target without a materialized definition or builtin classification
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: the TODO-4731 gap (c) class (semantic product publishes a
    method-call target the lowerer has no definition for) is invisible
    to semantic-product goldens and only surfaced case-by-case. A
    single validator pass over every lowered module asserting the
    invariant catches the whole class everywhere, replacing dozens of
    per-shape "does this lower" cases and guarding future
    materialization gaps by construction.
  - acceptance: invariant runs in the lowering pipeline under a test
    flag; deliberately re-introducing the gap (c) bug trips it.

- [ ] TODO-4738: Capture per-case durations in CI and reshard hot shards
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - scope: shard 201_210 sits at ~1530s against the 2400s ctest
    timeout; concurrent load pushed it over and produced a
    false-alarm "regression" that cost a full investigation cycle.
    Enable doctest --duration in CI, persist per-case timings, and
    reshard so no shard exceeds ~50% of its timeout budget.
  - acceptance: timing artifact per CI run; hottest shard under 50%
    of timeout at current case costs.

- [ ] TODO-4733: Migrate non-native-asserting exe-mode compile-run tests to --emit=vm
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: test-runtime
  - scope: exe-mode cases whose assertions are exit codes / stdout
    with nothing native-specific pay clang+link (~10-20s/case) for no
    signal the VM path would not give. Sweep the compile_run suites,
    classify each exe case (native-specific: keeps exe; otherwise:
    vm), and migrate the harness call. Keep a representative
    exe-tier per feature family as the miscompile net.
  - acceptance: emitters/imports/smoke wall time drops materially;
    per-family exe coverage retained; no assertion weakened.

- [ ] TODO-4734: Provide and adopt a RelWithDebInfo test-runner build
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: test-runtime
  - scope: the Debug test runner's semantic phase dominates
    compile-run case cost (~40s/case observed). Add a CMake
    preset/cache for a RelWithDebInfo (or -O2 + assertions) runner,
    measure the per-case delta on a fixed case set, and adopt it for
    the long compile-run suites if the win holds. Keep one Debug
    gate lane so assertion-only bugs stay visible.
  - acceptance: measured per-case speedup documented; suite lanes
    switched without losing Debug assertion coverage.

- [ ] TODO-4735: Share a precompiled stdlib semantic product across compile-run cases
  - owner: ai
  - created_at: 2026-07-20
  - phase: Test infrastructure
  - parallel_track: test-runtime
  - scope: every case re-parses and re-validates the imported stdlib
    modules from scratch. Investigate caching the stdlib portion of
    the semantic product (or a serialized module artifact) keyed by
    stdlib content hash, so per-case work is only the test source.
    Riskier than 4733/4734 (cache correctness must not mask
    cross-case pollution - see the top-priority pollution rule), so
    it needs an opt-out lane and a differential validation pass.
  - acceptance: measured speedup with a cache-off lane proving
    identical results.

- [ ] TODO-4723: Fix imported-helper diagnostics, nested-call "unknown call target", and rooted-helper-fallback rejection bugs (15 cases)
  - owner: ai
  - created_at: 2026-07-16
  - phase: Hidden test failure remediation
  - parallel_track: hidden-test-failures-collections
  - depends_on: TODO-4722
  - progress_2026-07-21b: found and fixed a real bug while triaging
    the RUN-mismatch subset: resolveMethodCallPath's typeName-based
    map resolution substituted an import-alias VALUE verbatim when
    the rooted candidate lookup missed, but alias values may be
    spelled without a leading slash (e.g. importAliases["Foo"] =
    "std/collections/map/at") - resolveTypePath is the one place that
    roots a bare path, so unrooted substitutions silently produced an
    unrooted resolvedOut at the generic fallback (e.g.
    "std/collections/map/at/tag" instead of
    "/std/collections/map/at/tag"), breaking every downstream lookup
    keyed on the rooted spelling. Routed the substituted value
    through resolveTypePath too. Fixes 5 emitters cases directly
    (several via shared plumbing: vector/soa alias direct-call method
    receivers). One sibling test assertion
    ("pkg/Thing/tag" without a leading slash) contradicted its own
    test's stated purpose ("normalizes slashless ... targets") and is
    corrected to the rooted form. Verified: emitters.cpp gate 551 ->
    556 passed, zero new failures (prefix-trimmed diff against the
    repin51 baseline to filter doctest log line-wrap artifacts, same
    false-positive class hit twice now in this file's history).
    4 residual cases in the same file
    (test_compile_run_emitters_map_metadata_resolution.cpp) resolve
    return-struct metadata through a DIFFERENT path
    (resolveConcreteSoaStructMethodPathFromReceiver's
    findReturnStructMetadata lookup and a still-unidentified route
    that actually applies the returnStructs marker for
    /map/count,/map/contains,/map/tryAt-style call receivers) -
    confirmed NOT caused by this session's earlier
    resolveMethodCallPath commit (6255974) via bisection (stashed
    both this session's changes, reproduced the identical failure
    pre-fix). Left unresolved rather than guessed at; needs a fresh
    trace of where the alias markers actually get applied before
    fixing or re-pinning.
    - progress_2026-07-22d: found and fixed the actual root cause of
      2 of these 4 residuals (plus a 5th, previously-undiscovered case
      in test_compile_run_emitters_vector_access_metadata_resolution.cpp,
      "resolves direct slashless canonical map receiver metadata") -
      `metadataPathCandidates` in
      `EmitterBuiltinMethodResolutionMetadataHelpers.cpp` really was
      the "still-unidentified route": it's a trivial single-candidate
      wrapper used by `findStructTypeMetadata`/`findReturnStructMetadata`/
      `findReturnKindMetadata`, but `resolveMethodCallPath`'s
      Call-receiver branch always looks up a ROOTED path (via
      `resolveExprPath`, which unconditionally prefixes a leading
      slash), while several tests populate `structTypeMap`/
      `returnStructs`/`returnKinds` with UNROOTED keys (no leading
      slash) - a mismatch metadataPathCandidates never bridged. Fix:
      added the unrooted variant as a second candidate whenever the
      input path is rooted. Verified no regressions across the 5 files
      most likely to be affected by touching this shared function
      (map_metadata_resolution.cpp 16/19, up from expected 14/19;
      vector_access_metadata_resolution.cpp 13/14; vector_receiver_
      metadata_resolution.cpp 21/25, identical 4 failures to before -
      confirmed by name, not just count; wrapper_map_count_sugar.cpp
      19/19 and explicit_vector_count_capacity_helpers.cpp 28/28,
      both unchanged). The remaining 2 of the original 4
      (`rejects explicit map slash-method count receiver fallback`,
      `rejects rooted map contains and tryAt direct-call return
      metadata`) and the 1 still-broken sibling in
      vector_access_metadata_resolution.cpp
      (`rejects bare map access metadata-only struct forwarding`) are
      all "should reject metadata-only forwarding" cases (the inverse
      shape from what this fix targets) and remain open - not
      re-pinned, not guessed at.
  - progress_2026-07-21c: re-pinned the 4-case explicit-vector-
    count-capacity duplicate/rejection cluster. One is a genuine
    behavior change worth noting: a user definition at the canonical
    /std/collections/vector/capacity path used to be rejected as a
    "duplicate definition"; it now compiles and correctly takes
    precedence (verified: returns the user's override value) - almost
    certainly a side effect of this project's completed Phase 1
    same-arity overload-resolution work (task #17, same-path
    type-differentiated overloads are now legal), landed before this
    session and not something this session needs to re-verify from
    scratch. The other three are diagnostic-text drifts (receiver-type
    rejections moved from per-type "unknown method: /map/capacity" /
    "unknown method: /string/capacity" text to a shared "capacity
    requires vector target" message, still correctly rejecting).
    File-level: 30/30. Suite-level: 556 -> 560 passed.
  - methodology-correction: the prefix-trimmed plain-text-log diffing
    used to verify the alias-rooting and capacity re-pin commits was
    itself unreliable - doctest's stdout log truncates/concatenates a
    failing case's own NAME with its immediately following error text
    when the error is long, with no separating newline, corrupting
    entries on BOTH sides of a diff (not just producing phantom "new"
    failures as first assumed, but also phantom "fixed" ones - one
    case credited as fixed by the alias-rooting commit,
    "rejects user vector access named call shadow in C++ emitter",
    was actually still failing the whole time, pre-existing and
    unrelated to that fix; the commit's numeric counts (551->556,
    556->560) are still correct since those come from doctest's own
    printed summary, only the per-case attribution prose was wrong).
    Re-verified both commits with a `--reporters=xml` run and
    `test_case_success="false"` parsing per <TestCase> block: zero
    actual regressions from either commit, confirmed against the
    current clean 62-case failing set. Use the XML reporter for all
    future emitters-suite diffs; never the plain-text log.
    - progress_2026-07-22: re-pinned all 6 failing cases in
    test_compile_run_emitters_wrapper_map_count_sugar.cpp, each
    verified individually by hand-reducing the test's inline source
    and running it through primec directly (not guessed). All 6
    converge on the SAME underlying policy: a user definition at a
    canonical /std/collections/... path is a legitimate override and
    takes precedence uniformly across bare calls, method-sugar calls,
    explicit-template calls, and expression contexts - the previous
    contracts assumed canonical only won for SOME of those call
    shapes (a mixed/inconsistent precedence rule), which no longer
    holds. File-level: 19/19 clean. This is a test-only change (no
    src/ edits), so the full collections/ir regression gates don't
    apply; verified via a 126-shard sharded ctest run of
    primestruct_compile_run_emitters_cpp_* completing cleanly (no
    process-level crashes) and the 6 re-pinned case names no longer
    appearing in its failure output.
  - infra-note: the monolithic 622-case
    `PrimeStruct_compile_run_tests --test-suite=...emitters.cpp` run
    (single process, ~15-20 min) has been getting truncated
    mid-execution by container restarts (twice in a row this
    session), producing incomplete/misleading XML output with no
    closing summary. The equivalent SHARDED ctest registration
    (`ctest -R primestruct_compile_run_emitters_cpp_`, 126
    per-file/per-range shards run --parallel 4) is far more
    restart-resilient - each shard reports to ctest's own log as it
    finishes, so a restart only loses in-flight shards, not the whole
    run's accumulated result. Prefer the sharded form for this suite
    going forward, especially when restarts are frequent; fall back
    to the monolithic + XML-reporter form only when a definitive
    single-run case-level diff against a prior monolithic baseline is
    needed.
  - progress_2026-07-22b: investigated the vector/at direct-call
    override-precedence asymmetry in
    `test_compile_run_emitters_namespaced_vector_push_and_count_helpers.cpp`
    (3 of its 6 failing cases). Found the bug is not a single
    one-line fix - at least three separate, inconsistent classification
    sites decide whether a call to canonical `/std/collections/vector/at`
    takes the native fast path or defers to a user override, and two
    attempted point-fixes (extending the `at_unsafe`-only bypass in
    `IrLowererNativeTailDispatch.cpp` to cover `at`; closing a
    leading-slash path-matching gap in `IrLowererLowerStatementsExpr.h`'s
    return-statement fast path) each individually verified-correct in
    isolation but did not fix the repro end-to-end, and a third,
    still-unidentified code path is what makes the METHOD-CALL form of
    the same override already work today. Both edits were reverted
    (zero net diff) rather than landed partially. Full findings, the
    gdb call chains, and the four sub-problems identified filed as
    TODO-4739 for a dedicated pass that maps all the classification
    sites before patching any one of them.
- progress_2026-07-21: first re-pin batch landed - the 51
    semantic-rejection COMPILE_FAIL cases from the survey (dominated
    by retired same-path/alias contracts already adjudicated in the
    collections pass: unknown call target /vector/at, /map/count,
    /std/collections/vector/count, etc.) are re-pinned to their
    surveyed current diagnostics across 12 test files. emitters.cpp
    gate: 501 -> 551 passed, zero cases failing that were not already
    failing pre-re-pin (verified by prefix-matched diff against the
    original baseline; naive full-line diffing false-positived on
    doctest log line-wrap artifacts). Remaining 71: the RUN-outcome
    cases needing rc/stdout comparison against pins, the 12-case
    args<T> pack at()/at_unsafe() lowering gap (TODO-4726 territory),
    lowering-level native-backend limitation errors, and the 17
    unit-style emitter-API cases not covered by the source-survey
    method.
  - progress_2026-07-20d: emitters_newly_exposed survey complete
    (emitters_outcomes.json in the session scratchpad): of 114 failing
    cases, 74 fail compilation - the dominant sub-clusters are retired
    same-path/alias contracts already adjudicated in the collections
    pass (unknown call target /vector/at, /map/count,
    /std/collections/vector/count etc. -> re-pin to rejection), and a
    12-case REAL gap: values.at(i)/values.at_unsafe(i) method sugar on
    args<T> pack receivers publishes the definition-less /array/at
    family and the inline dispatch fails without diagnostic ("missing
    lowered definition: /array/at"). A carve-out at the
    SetupTypeMethodCallResolution consumption point (mirroring the
    existing /array/count pack carve-out) is NOT sufficient - the
    failure originates deeper in runInlineCallDispatch, where the
    method form never reaches the builtin pack-access path; this is
    the same design knot as TODO-4726's
    ir_lowerer::getBuiltinArrayAccessName case (at/at_ref vs
    at_unsafe/at_unsafe_ref classification with the
    unrootedStdlibVectorHelperPath early-return bailouts) and should
    be fixed there, not papered at the consumption site. 23 cases
    build and run (rc/stdout captured for pin comparison); 17 are
    unit-style emitter-API assertion tests without inline sources
    (read individually). Native-backend limitation errors observed in
    the survey used --emit=exe and are representative (the cpp emit
    path routes through the same IR lowering).
  - progress_2026-07-20c: full 1877-test ctest survey: 92% passing,
    155 failing shards. Remaining clusters by family: compile_run
    emitters_newly_exposed (37 shards), compile_run
    imports_operations_and_collections (35) + versioned_import_resolution
    (10) + block_and_operator_rewrites (6), ir_pipeline
    validation_cases (27 - a second file, distinct from the
    canonical-module file already modernized to 92/95),
    vm_collections_newly_exposed (25), smoke families (~51 total),
    reflection_codegen (9), plus singles. The collections semantics
    suite (131 shards) stays green in the full run. Continue the
    survey -> adjudicate -> fix-or-repin -> gate loop per family,
    largest first.
  - progress_2026-07-20b: the ir.pipeline.validation module file
    (test_ir_pipeline_validation_ir_validator_accepts_lowered_canonical_module.cpp)
    went 75/95 -> 92/95; the three remaining reds are the tracked
    TODO-4726 getBuiltinArrayAccessName classification pair and the
    TODO-4728 reflection-query error-precedence case. Work: retired
    soa module imports (internal_soa, internal_soa_conversions)
    modernized out of the failing sources with explicit
    experimental_soa_conversions calls moved to the canonical
    /std/collections/soa/to_aos_ref spelling; to_aos materialization
    pins flipped to lowering success (gap (c)); five drifted
    diagnostics re-pinned to current text (one still leaks the
    retired soa_vector family - gap (e) evidence); and one compiler
    fix: candidatePathsForExprCall now resolves struct-literal
    constructor receivers (Holder{}.cloneValues()) to their member
    definitions so chained soa helper methods on such receivers reach
    the method desugar and materialize their canonical targets -
    fixing the nested struct-body TODO-4727 example end-to-end.
    Gates: collections 130/131 with the single failure a 2400s
    timeout on shard 201_210 caused by concurrent probe load (all ten
    cases pass individually with the change, ~1530s total, and the
    slowness is pre-existing - case 209 takes 380s with the change
    stashed too); emitters.cpp identical 501/622.
  - progress_2026-07-20: the re-pin pass LANDED and the
    calls_flow.collections gate is 131/131 GREEN for the first time.
    Method: batch-ran all 86 failing cases' inline sources through
    primec to build a per-case outcome table (63 semantic failures
    with exact diagnostics, 22 validate at the semantic phase, 1
    parameterized case probed manually), then rewrote each assertion
    tail to match. Adjudications along the way: (a) named arguments
    into args<T> packs stay rejected per PrimeStruct.md ("named
    arguments bind only fixed parameters"); (b) user /map/<helper>
    same-path alias shadows are RETIRED - four passing contracts
    ("at/at_unsafe method requires canonical map helper even when
    alias helper exists", "bare count call rejects user-defined map
    alias helper precedence", "bare map count call rejects when only
    compatibility alias is present") pin canonical-only precedence, so
    a prototyped shadow-preference fix was reverted and those cases
    re-pinned to rejection; (c) canonical map helpers on legacy-alias
    map<K,V> call receivers are deliberately retired (comment in
    validateExprPreDispatchDirectCalls); (d) named-receiver call forms
    for vector mutators (remove_at/remove_swap/pop/clear) validate and
    run cleanly now and are pinned green.
  - progress_2026-07-19: full triage of the collections gate's stable
    35-shard failing set (86 cases, 13 files; the surviving mass after
    the TODO-4731 soa work landed). Three buckets by failure kind:
    (1) 23 cases pin REJECTION but the program now validates - names
    say "accepts/resolves/prefers", so these look like straight pin
    flips once current acceptance is confirmed correct;
    (2) 43 cases still reject but with drifted diagnostic text -
    re-pin after checking the new text is canonical (no retired-family
    leaks);
    (3) 20 cases pin SUCCESS but the program now rejects - real
    compiler gaps, diagnosed by running each inline source through
    primec directly: (3a) named-argument call-form helper routing
    (`at([index] 1i32, [values] values)` -> "unknown call target",
    named-arg canonical vector constructors -> "unknown named
    argument: first", labeled receivers, user-shadow named args - the
    biggest coherent sub-cluster); (3b) map helper resolution on
    field-bound/wrapper-returned/temporary receivers ("unknown call
    target: /map/at", "/map/count"); (3c) variadic vector pack
    receiver statements ("unknown call target: clear"); (3d) singles:
    compat-alias precedence type mismatches, auto-inference alias
    precedence return-type mismatch, builtin count arity mismatch,
    "call site: /vector/count" ambiguity. Fix (3) sub-clusters first
    (compiler work, gate each), then land (1)+(2) as re-pin passes.
  - scope: 15 cases remain in
    `test_semantics_calls_and_flow_collections_wrapper_returned_map_method_resolution.cpp`
    after TODO-4722's fix, splitting into (at least) three distinct
    root causes:
    1. **Imported-helper diagnostics** (2 cases: "stdlib namespaced
       vector access slash method uses imported helper diagnostics",
       "stdlib namespaced vector access unsafe slash method uses
       imported helper diagnostics"). Source: `import
       /std/collections/*` then `values./std/collections/vector/at(true)`
       (bool arg where the real imported helper takes an int index).
       Expected: rejected with "argument type mismatch for
       /std/collections/vector/at parameter index ...". Actual (gdb
       breakpoint on `failExprDiagnostic`, confirmed 2026-07-16): hits
       the blunt "at requires integer index" shortcut in
       `SemanticsValidatorExpr.cpp` (~line 651) AND the sibling one in
       `SemanticsValidatorExprMethodResolution.cpp` (~line 153) - not
       caused by an arity mismatch: `paramsByDef_.count(canonicalVectorHelperPath)`
       is literally 0 for this test (gdb-confirmed, 2026-07-16) - imported
       definitions are NOT registered in `paramsByDef_` at the point
       these checks run, only `hasImportedDefinitionPath(...)` sees them.
       **Tried and reverted**: broadening both guards to also treat
       `hasImportedDefinitionPath(canonicalVectorHelperPath)` (regardless
       of `paramsByDef_` arity) as "a real override exists, skip the
       int-only check" does make both shortcuts stand down, but the call
       then falls through to `validateExprLateUnknownTargetFallbacks`
       and fails with "unknown call target: at" instead - i.e. normal
       call resolution for an EXPLICIT-canonical-path `at`/`at_unsafe`
       call against an *imported* (not locally-declared) real definition
       doesn't actually succeed once the shortcuts are bypassed. This
       reroutes the failure rather than fixing it, and was reverted
       (uncommitted) rather than risking a regression elsewhere from the
       broadened guard. **This means case 1 and case 2 below are likely
       the SAME root cause**, not two separate bugs: explicit-canonical-
       path `at`/`at_unsafe` calls don't resolve against imported
       definitions via the normal method-call resolution path at all
       (regardless of nesting inside `plus(...)`) - the real fix belongs
       in whatever resolves `expr.name == canonicalVectorHelperPath`
       method calls to their real imported definition (making
       `validateExprLateUnknownTargetFallbacks` unnecessary for this
       case), not in either int-only-index shortcut. Once that's fixed,
       both shortcuts can likely keep their current narrow (arity-via-
       paramsByDef_) guards from TODO-4722 unchanged.
    2. **Nested "unknown call target: at"** (1 case: "stdlib namespaced
       vector access slash method uses imported helper on vector
       receiver" - expects success, currently fails; likely the same
       root cause as case 1 above, see note there). Source:
       `plus(values./std/collections/vector/at(0i32),
       values./std/collections/vector/at_unsafe(1i32))` - `at`/`at_unsafe`
       calls nested as ARGUMENTS to a numeric builtin (`plus`). gdb
       backtrace (2026-07-16) shows this reaches
       `validateNumericBuiltinExpr` ->
       `validateExprLateUnknownTargetFallbacks`, which fails with
       "unknown call target: at" - i.e. when nested inside `plus(...)`,
       these method calls aren't going through the normal method-call
       dispatch path that recognizes `at`/`at_unsafe` receivers at all;
       they fall through to a late fallback that only sees the bare
       method name with no receiver context. Root cause not yet
       localized beyond this backtrace - needs tracing why
       `validateNumericBuiltinExpr`'s argument-expression validation
       path skips the normal method-call resolution for its operands.
    3. **"rejects ... without helper"/"rejects rooted helper fallback"
       group** (12 cases, e.g. "stdlib namespaced vector count method on
       builtin vector receiver rejects rooted helper fallback", "stdlib
       namespaced vector capacity method rejects map receiver without
       helper"). Completely uninvestigated. One sampled case (gdb,
       2026-07-16): "stdlib namespaced vector count method on builtin
       vector receiver rejects rooted helper fallback" - a *rooted*
       (non-namespaced) `/vector/count([vector<i32>] values)` definition
       exists, and the call explicitly names the *namespaced*
       `/std/collections/vector/count()` path with only the receiver arg
       (no extra args, so none of the arity-gate checks fire at all).
       Expected: rejected with "unknown method:
       /std/collections/vector/count" (no real definition at that exact
       path). Actual: currently silently SUCCEEDS - something is letting
       the call fall back to the unrelated rooted `/vector/count` alias
       even though the call spelled out the different, non-existent
       namespaced path explicitly. This looks like a *different* flavor
       of the same "same-path vs. name-pattern-only matching" bug class
       as TODO-4721/4722, but inverted (over-permissive fallback instead
       of over-strict rejection) and likely in yet another code path.
       A second sampled case (gdb, 2026-07-16): "stdlib namespaced vector
       capacity method rejects map receiver without helper" - calling
       `values./std/collections/vector/capacity()` where `values` is a
       `map<i32, i32>` (no helper defined anywhere). Expected: rejected
       with "capacity requires vector target" - and that exact message
       IS produced elsewhere in the codebase
       (`SemanticsValidatorExprMethodTargetResolution.cpp:2852`, inside
       the giant `resolveMethodTarget` function starting at line 213 of
       that file - `receiver.isMethodCall && !isExplicitVectorFamilyReceiver
       && isVectorCompatibilityHelperName(normalizedMethodName)` with no
       declared/imported definition at the explicit path leads to exactly
       this diagnostic for `capacity`, per a direct read of that code).
       Actual: gdb-confirmed (`failExprDiagnostic` breakpoint) the real
       failure is "unknown method: /map/capacity" from
       `SemanticsValidatorExprMethodResolution.cpp:785`, i.e. a
       *different, earlier* function than the one that already has the
       right diagnostic. `resolveMethodTarget` (called from
       `validateExprMethodCallTarget` at ~line 400/410) evidently returns
       `resolved = "/map/capacity"` (substituting the receiver's own type
       family for the method leaf name, discarding the explicit
       `/std/collections/vector/capacity` path the call actually wrote)
       WITHOUT ever reaching its own later "capacity requires vector
       target" logic at line ~2851 - meaning some earlier branch inside
       that 2800+-line function resolves and returns before that point.
       **Localized (2026-07-16)**: rather than hand-tracing 2800+ lines,
       set a gdb breakpoint at every one of the function's 54
       `resolvedOut = ` assignment sites in one script and ran once - the
       actual assignment for this test fires at line 3758, the function's
       *final* generic fallback (`resolvedOut = resolvedType + "/" +
       normalizedMethodName`). The specific earlier check at line
       ~2841-2853 that already has the correct "capacity requires vector
       target" diagnostic is gated on `receiver.isMethodCall` - which
       means "is the *receiver expression itself* a chained method call
       (e.g. `foo().bar()`)", not "is this call a method call" as the
       name misleadingly suggests - and is `false` for a simple local
       variable receiver like `values`, so that block never fires for
       this shape and falls through the other ~900 lines of the function
       to the generic fallback at the very end. **Fixed**: added the
       same "capacity requires vector target" guard in two places near
       the function's tail (once before the `isPrimitiveBindingTypeName`
       early-return, since primitive receivers like `string` return
       before reaching the very end; once before the final generic
       fallback, for non-primitive receivers like `map`/`array`),
       scoped to `normalizedMethodName == "capacity" &&
       normalizedCollectionTypePath != "/vector" &&
       isCanonicalVectorCompatibilityPath(explicitVectorHelperPath)` -
       deliberately using `isCanonicalVectorCompatibilityPath` (the FULL
       `/std/collections/vector/` prefix check) rather than the broader
       `explicitVectorHelperPath.empty()` check, since the latter would
       also match the *rooted* short-form spelling (`/vector/capacity`)
       and incorrectly change the message for the already-passing
       "vector namespaced capacity method rejects local string/array
       receiver without helper" tests (which correctly expect "unknown
       method: /string/capacity" / "unknown method: /array/capacity" for
       that spelling, not "capacity requires vector target"). **First
       attempt regressed 5 other tests**: this initial condition (no
       check for whether a same-path definition already exists) fixed
       the 5 targeted cases but broke "...rejects local/wrapper
       array/string same-path helper" and "...rejects wrapper map
       same-path helper" (all previously-passing, all expecting the
       *old* "unknown method: /TYPE/capacity" message, not the new one) -
       caught by the full 131-shard regression run before committing,
       per this session's established discipline. gdb comparison of the
       four "same-path helper exists" tests showed no clean single
       distinguishing factor (receiver kind, receiver type, and
       definition-existence all interact - e.g. a *local* `map` receiver
       with a same-path helper still wants "capacity requires vector
       target" while a *wrapper* (function-return) `map` receiver with
       an identical same-path helper wants "unknown method:
       /map/capacity", and local `string`/`array` receivers with a
       same-path helper want "unknown method: /TYPE/capacity" like the
       wrapper case, unlike local `map`). Rather than chase the full
       3-way interaction, **narrowed the guard** to add
       `!hasDeclaredDefinitionPath(explicitVectorHelperPath) &&
       !hasImportedDefinitionPath(explicitVectorHelperPath)` - i.e. only
       fire the new diagnostic when no definition exists at the explicit
       path at all, which is the clean, unambiguous sub-case. This fixes
       4 of the 12 cases in this group ("stdlib namespaced vector
       capacity method rejects array/map/string/wrapper map receiver
       without helper"), leaves "...rejects local map same-path helper"
       at its original (already-failing, not regressed) state for
       follow-up, and does not touch any of the 5 previously-passing
       "same-path helper exists" tests. Verified via a second full
       131-shard regression run: exactly 4 cases fixed, zero
       regressions (global sorted-unique-name diff against the
       post-TODO-4722 baseline).
       The remaining 8 cases (the "count" equivalents at lines 739-869
       and 949-980, "...rejects local map same-path helper", plus the
       two "on builtin vector receiver rejects/requires rooted helper
       fallback" cases where the receiver *is* a vector but the call
       mixes rooted vs. namespaced spellings) were NOT touched - they use
       different message conventions (e.g. "unknown call target:
       /std/collections/map/count" for wrapper-map receivers,
       "unknown method: /vector/count" for the rooted-vs-namespaced
       mismatch) that need their own individual traces, not a blind
       copy of the capacity fix's `if` condition.
       **See also TODO-4724**: `resolveMethodTarget`'s sheer size (2800+
       lines, 54 assignment sites, a misleadingly-named guard variable)
       made this one fix take far longer to localize than it should have
       - filed a follow-up to decompose the function so future bugs like
       this are traceable by reading, not by scripting 54 breakpoints.
  - progress_2026-07-16b: Fixed 2 more cases: "stdlib namespaced vector
    count method rejects wrapper map receiver without helper" and
    "...rejects wrapper map same-path helper" - both
    `wrapMap()./std/collections/vector/count()` (a wrapper function with
    an *explicit* `[return<map<i32, i32>>]` annotation, called via the
    explicit vector-namespaced spelling), expected to reject with
    "unknown call target: /std/collections/map/count" regardless of
    whether a same-path helper is declared for map. Localized in
    `tryResolveExplicitCanonicalVectorCountMethodTarget`
    (`SemanticsValidatorExprMethodTargetResolution.cpp:2327-2347`, a
    lambda inside `resolveMethodTarget` already dedicated to count +
    non-vector receivers) - it always fell through to either accepting a
    same-path helper (wrongly, for map) or the generic "unknown method: "
    + explicitVectorHelperPath diagnostic, never the map-specific
    "unknown call target: ..." form the tests want. **Two regression
    rounds before landing**, both caught by the full-regression-before-
    commit discipline: (1) an initial fix gating only on
    `receiverFamily == "map" && receiverExpr.kind == Expr::Kind::Call`
    incorrectly also rejected "wrapper temporary canonical vector count
    slash-method rejects map receiver" - a *different*, previously-
    passing test using a wrapper function with an INFERRED (not
    explicitly annotated) map return type (`[effects(heap_alloc)]` with
    a bare `return(values)`, no `[return<map<...>>]`), which correctly
    keeps the older "unknown method: /std/collections/vector/count"
    message. (2) Narrowed further by walking the wrapper function's own
    `transforms` list for an explicit `return` transform whose template
    arg's base type normalizes to "map" (mirroring the existing
    Pointer/Reference-return-type extraction pattern already used
    elsewhere in this same function, ~line 484-497) - this precisely
    distinguishes `wrapMap()` (explicit annotation, gets the new
    diagnostic) from `wrapMapAuto()` (inferred, keeps the old one).
    Verified via a third full 131-shard regression run: exactly 2 fixed,
    zero regressions. This further confirms TODO-4724's premise - three
    escalating rounds of hidden-distinguishing-factor discovery (map vs.
    array/string, Name vs. Call receiver, explicit vs. inferred return
    annotation) for what looked like one simple message-text fix is a
    strong signal this function's logic needs to be legible, not just
    correct.
  - remaining_2026-07-16b (superseded by progress_2026-07-16c below):
    6 of the original 12 "rejects ... without helper"/rooted-helper-
    fallback cases were fixed at this point (4 capacity + 2 count); the
    "on builtin vector receiver rejects rooted helper fallback" pair
    were traced and fixed next - see progress_2026-07-16c.
  - progress_2026-07-16c: Fixed "stdlib namespaced vector count method
    on builtin vector receiver rejects rooted helper fallback" and its
    capacity sibling - both: a real, plain `[vector<i32>]` receiver, a
    ROOTED alias definition declared (`/vector/count([vector<i32>]
    values)`), called via the explicit `/std/collections/vector/count()`
    spelling with no extra arguments. Expected: reject with "unknown
    method: /std/collections/vector/count" (no definition exists at
    that literal path - the rooted alias is a *different* path and must
    not be silently substituted). Actual: silently succeeded, treating
    the call as the ordinary builtin `count()`. gdb-traced (breakpoint-
    sweep across every `isBuiltinMethod = ` assignment in
    `validateExprMethodCallTarget`, the same technique used for
    TODO-4723's `resolveMethodTarget` bug) to a SECOND, unconditional
    override at ~line 662-673 of
    `SemanticsValidatorExprMethodResolution.cpp` -
    `if (isStdNamespacedVectorCompatibilityHelperPath(resolved,
    "capacity")) { isBuiltinMethod = true; } else if (... "count" ...)
    { ... isBuiltinMethod = true; }` - that unconditionally re-derives
    `isBuiltinMethod = true` whenever the resolved path is the
    std-namespaced count/capacity path and the receiver is a real
    vector, with zero consideration of rooted-alias rivals. A first fix
    attempt guarding this override (skip forcing `isBuiltinMethod` when
    `expr.name == resolved` and a rooted alias is declared/imported)
    fixed both target cases but regressed "stdlib canonical vector
    helper namespace body arguments keep unknown target" (a *different*
    call shape - extra positional arg plus block/body arguments, e.g.
    `values./std/collections/vector/count(true) { 1i32 }` - which
    expects "unknown call target: ..." via its own dedicated arity/
    body-argument handling elsewhere, not this override). Narrowed the
    guard to only fire for the plain, no-extra-argument call shape
    (`expr.args.size() == 1 && !expr.hasBodyArguments &&
    expr.bodyArguments.empty()`), which fixed the regression while
    keeping both targets fixed. Verified via a third full 131-shard
    regression run: 2 cases fixed, zero regressions.
  - remaining_2026-07-16c: 4 cases remain in this group: "stdlib
    namespaced vector capacity method rejects local map same-path
    helper" (deliberately left by the narrowed capacity guard, see
    progress_2026-07-16), "vector namespaced count method on builtin
    vector receiver requires same-path helper", "...rejects local array
    same-path helper", and "...rejects local string same-path helper"
    (all use the *rooted* `/vector/count()` short-form spelling, not the
    std canonical path - a distinct scenario, not yet traced). Plus the
    2 imported-helper-diagnostics cases and 1 nested-call case from
    earlier in this TODO, still open. 76 cases remain failing suite-wide
    (see `docs/failing_tests.md`).
  - investigated_2026-07-16d: Traced 2 of the 3 remaining rooted-spelling
    cases without landing a fix. "...rejects local array/string
    same-path helper": source declares `/vector/count([string] values)`
    (or `[array<i32>]`) and calls it via `value./vector/count()` where
    `value` genuinely has that exact declared type - the call
    TYPE-CHECKS perfectly against the definition's own signature (no
    mismatch at all). gdb-confirmed (breakpoint on
    `validateExprMethodCallTarget`) `resolved` stays `/vector/count`
    (pre-resolved by `resolveCalleePath` before this function even
    runs) and the call is accepted, because a real definition genuinely
    exists at that literal path with a matching param type. The
    rejection the test wants ("unknown method: /string/count") is not
    about a type mismatch at the call site - it's a **namespace-hygiene
    rule on the *definition itself***: a path spelled `/vector/<name>`
    should not be a valid call target at all unless its own first
    parameter is actually vector-shaped, regardless of what any
    particular caller passes. Fixing this needs a check at definition-
    validation or path-resolution time ("does this /vector/-prefixed
    definition's first param actually match the vector family?"), not
    a call-site guard like the fixes above - a materially different
    (and larger) piece of work than TODO-4723's other fixes, which have
    all been about *whether* an existing, correctly-shaped definition
    should be preferred, not about invalidating a definition based on
    its own declared shape. "...on builtin vector receiver requires
    same-path helper" (the third case) is even less understood - a
    *matching* `/vector/count([vector<i32>] values)` definition against
    a real vector receiver, called via the exact same rooted spelling,
    is still expected to be REJECTED, with a MAP-family message
    ("unknown call target: /std/collections/map/count") that has no
    obvious connection to a vector-typed call at all - not traced
    beyond confirming current (wrong) behavior accepts the call. Given
    the definition-validation-time fix shape needed for the array/string
    pair is a different class of change than anything else in TODO-4723,
    and the third case's expected behavior isn't understood well enough
    yet to even hypothesize a fix, stopping here for this pass rather
    than guessing.
  - implementation_notes: Given three apparently-independent root causes
    span (at least) `SemanticsValidatorExpr.cpp`,
    `validateNumericBuiltinExpr`/`validateExprLateUnknownTargetFallbacks`,
    and whatever resolves the rooted-vs-namespaced vector alias fallback,
    investigate and fix each independently with its own gdb trace and
    its own full-sharded-regression verification pass, rather than
    assuming a shared fix. Use the corrected CTest log-slicing technique
    from `docs/failing_tests.md`'s "Methodology note (2026-07-16)" (or
    better, the attribution-independent global sorted-unique-name diff)
    for all regression verification - do not hand-slice per-shard
    `--output-on-failure` output.
  - acceptance: All 15 listed cases pass. Full sharded
    `ctest -R calls_flow_collections_` run (global name-set diff against
    the 2026-07-16 post-TODO-4722 baseline in `docs/failing_tests.md`)
    shows no new failures.
  - stop_rule: Fix and verify each of the three root-cause groups
    independently; if a fourth distinct pattern emerges within the
    12-case "rejects ... without helper" group once individually traced,
    split it into its own follow-up TODO rather than force-fitting one
    fix across all 12.

- [ ] TODO-4724: Decompose the 2800+ line resolveMethodTarget function into smaller, traceable pieces
  - owner: ai
  - created_at: 2026-07-16
  - phase: Maintainability / tech debt
  - parallel_track: hidden-test-failures-collections
  - depends_on: (none - can run independently of TODO-4723's remaining cases)
  - scope: `SemanticsValidator::resolveMethodTarget`
    (`SemanticsValidatorExprMethodTargetResolution.cpp:213-3760`, ~2800
    lines in a single function body) resolves a method call's target
    definition path based on the receiver's inferred type. While fixing
    part of TODO-4723 (2026-07-16), localizing a single bug required
    setting gdb breakpoints at all 54 of the function's `resolvedOut = `
    assignment sites simultaneously and running once to find which one
    fired - reading the function top-to-bottom was not a practical way
    to find the relevant branch. The function also has at least one
    misleadingly-named local (`receiver.isMethodCall` at ~line 2841
    means "is the receiver expression itself a chained call", not "is
    this call a method call"), which caused an initially-plausible-looking
    condition to be ruled out only via gdb, not by reading. This is the
    same class of problem `docs/CompatPathResolutionConsolidation.md`
    already tracks for *spelling disposition* logic (compat-to-canonical
    renaming, removed-helper rejection, same-path shadow precedence) -
    but that document explicitly scopes *receiver-type resolution* (this
    function's job) as staying where it is, out of scope for that
    consolidation ("the classifier only decides spelling disposition,
    not receiver typing"). `resolveMethodTarget`'s size/traceability
    problem is therefore not covered by any existing plan.
  - implementation_notes: Likely decomposition seams, based on reading
    during TODO-4723 (not exhaustive - a fresh read-through dedicated to
    this task should re-derive/refine): (1) sum-type method target
    candidates (~740-800, the `candidates`/`appendCandidate` block); (2)
    pointer/reference-returning-definition receiver resolution (~2400-
    2470); (3) the vector-compatibility-family special cases (~2740-
    2780, ~3726-3748); (4) the primitive/struct/sum-type generic
    fallback (~3690-3759, where TODO-4723's capacity fix landed). Each
    seam should become its own named private helper (or free function in
    an anonymous namespace, matching the file's existing style for
    smaller helpers like `explicitVectorMethodPath`) taking exactly the
    inputs it needs, with the top-level `resolveMethodTarget` becoming a
    much shorter dispatcher that calls each in sequence. Also worth
    renaming `receiver.isMethodCall`'s local usage or adding a comment at
    each check site, given how easy it was to misread during this
    session's investigation - a short-term, near-zero-risk win
    independent of the larger decomposition.
  - acceptance: `resolveMethodTarget`'s own body (excluding extracted
    helpers) is under a few hundred lines; each extracted helper has a
    name that describes what receiver/method shape it handles; full
    sharded `ctest -R primestruct_semantics` run is unchanged (pure
    refactor, zero behavior change - any test delta means the extraction
    was not behavior-preserving and must be fixed before proceeding).
  - stop_rule: This is a pure internal refactor with no user-visible
    behavior change as the explicit goal - if any extraction step
    changes even one test's pass/fail outcome, stop, revert that step,
    and re-derive the seam boundary rather than "fixing" the test to
    match the refactored behavior. Do not combine this with fixing
    TODO-4723's remaining cases in the same commit - land the
    decomposition behavior-preserving first, then any subsequent
    TODO-4723 fixes get to build on smaller, more legible functions.
