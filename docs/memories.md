# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

- `soa-pending-diagnostics-canonicalized`: direct pending SoA borrowed/field-view
  rejects now canonicalize onto stdlib helper-contract unavailable-method
  diagnostics (`/std/collections/soa_vector/ref(_ref)` and
  `/std/collections/soa_vector/field_view/<field>`) instead of bespoke
  `soa_vector ... not implemented yet` strings, and source-lock tests now
  assert canonical helper-path usage in validator + monomorph fallback code.
- `soa-pending-diagnostics-no-optional-branch`: semantics helpers no longer
  expose the optional `soaPendingUnavailableMethodDiagnostic(...)` branch;
  semantics helpers also no longer expose
  `soaDirectPendingUnavailableMethodDiagnostic(...)`, and monomorph fallback
  plus shared helper diagnostics now route SoA pending borrowed/field-view
  rejects through canonical single-argument
  `soaUnavailableMethodDiagnostic(...)` unknown-method paths after dropping
  the unused borrowed-helper visibility probe argument.
- `soa-field-view-helper-canonical-prefix-only`: `splitSoaFieldViewHelperPath`
  now accepts only the canonical `/std/collections/soa_vector/field_view/`
  prefix and no longer matches legacy `/soa_vector/field_view/` helper paths;
  source-lock IR pipeline coverage now asserts that the legacy prefix string
  does not appear in `SemanticsBuiltinPathHelpers.cpp`.
- `soa-pending-diagnostics-template-normalization`: pending helper diagnostic
  canonicalization now strips resolved helper template suffixes (`__t...`)
  before applying legacy `/soa_vector/ref(_ref)` remapping, so templated
  legacy helper spellings also report canonical
  `/std/collections/soa_vector/ref(_ref)` unknown-method diagnostics; semantic
  coverage now explicitly checks canonical `ref_ref` rejects alongside `ref`.
- `soa-map-builtin-resolved-path-canonicalization`: expr-map SoA builtin
  validation now canonicalizes resolved legacy helper spellings
  (`/soa_vector/get|get_ref|ref|ref_ref`) onto
  `/std/collections/soa_vector/*` before dispatch matching, reducing direct
  legacy resolved-path comparisons while preserving helper compatibility.
- `soa-statement-return-ref-path-canonicalization`: statement-return SoA
  ref/ref_ref escape detection now canonicalizes legacy resolved helper
  spellings (`/soa_vector/ref(_ref)`) onto canonical
  `/std/collections/soa_vector/ref(_ref)` before old-surface/helper-family
  checks, and source-lock coverage now rejects direct statement-return
  `resolvedPath == "/soa_vector/ref(_ref)"` or
  `resolvedPath.rfind("/soa_vector/ref(_ref)", 0)` checks.
- `soa-statement-binding-ref-path-canonicalization`: statement-binding SoA
  standalone-ref classification now canonicalizes resolved helper paths
  (strips template suffixes and remaps legacy `/soa_vector/ref(_ref)` onto
  canonical `/std/collections/soa_vector/ref(_ref)`) before method/helper
  family detection in both `isStandaloneRefCall` and
  `resolveStandaloneRefRootExpr`; source-lock coverage now rejects direct
  legacy `resolvedPath.rfind("/soa_vector/ref(_ref)", 0)` checks there.
- `soa-infer-collection-dispatch-ref-path-canonicalization`: collection
  method return-kind dispatch now canonicalizes resolved SoA helper paths
  (including templated and legacy `/soa_vector/get|get_ref|ref|ref_ref`
  spellings) onto canonical `/std/collections/soa_vector/*` before helper
  family checks in `resolveBuiltinCollectionMethodReturnKind`, and source-lock
  coverage now rejects direct `resolvedPath == "/soa_vector/*"` comparisons
  there.
- `soa-build-initializer-helper-path-canonicalization`: build-initializer
  SoA access helper classification now canonicalizes templated and legacy
  resolved helper spellings (`/soa_vector/get|get_ref|ref|ref_ref`) onto
  canonical `/std/collections/soa_vector/*` before helper-family matching in
  `builtinSoaAccessHelperName`, and source-lock coverage now rejects direct
  `resolved == "/soa_vector/*"` comparisons there.
- `soa-infer-return-helper-path-canonicalization`: infer-collection
  return-inference SoA helper gating now canonicalizes templated and legacy
  resolved helper spellings (`/soa_vector/get|get_ref|ref|ref_ref`) onto
  canonical `/std/collections/soa_vector/*` before old-surface helper-family
  checks, and source-lock coverage now rejects direct
  `resolvedCandidate == "/soa_vector/*"` comparisons there.
- `soa-expr-call-args-ref-path-canonicalization`: resolved-call argument SoA
  standalone `ref`/`ref_ref` escape classification now canonicalizes resolved
  helper paths (template-suffix stripping plus legacy `/soa_vector/ref(_ref)`
  remapping) onto canonical `/std/collections/soa_vector/ref(_ref)` before
  helper-family matching, and source-lock coverage now rejects direct legacy
  `resolvedPath.rfind("/soa_vector/ref(_ref)", 0)` checks there.
- `soa-expr-mutation-borrows-ref-path-canonicalization`: SoA receiver
  detection for mutation-borrow `ref`/`ref_ref` calls now canonicalizes
  resolved helper paths (template-suffix stripping plus legacy
  `/soa_vector/ref(_ref)` remapping) before helper-family checks, and
  source-lock coverage now rejects direct legacy
  `resolvedPath.rfind("/soa_vector/ref(_ref)", 0) == 0` checks there.
- `soa-ir-lowerer-mutation-ref-write-target-canonicalization`: IR lowerer
  collection-mutation SoA `ref` write-target same-path visibility probes now
  canonicalize templated and legacy resolved helper spellings before
  fallback gating, and source-lock coverage now rejects direct
  `samePathCallee->fullPath.rfind("/soa_vector/ref", 0) == 0` checks there.
- `soa-monomorph-expression-rewrite-ref-path-canonicalization`:
  template-monomorph expression rewrite now canonicalizes templated and
  legacy resolved SoA `ref`/`ref_ref` helper spellings before helper-family
  checks in both call and method dispatch paths, and source-lock coverage now
  rejects direct `resolvedPath == "/soa_vector/ref"` and
  `methodPath == "/soa_vector/ref"` checks there.
- `soa-monomorph-implicit-shared-ref-canonicalizer`: template-monomorph
  implicit-template pending SoA `ref`/`ref_ref` helper gating now reuses the
  shared `canonicalizeLegacySoaRefHelperPath(...)` semantics helper instead
  of a local lambda; canonical builtin `ref`/`ref_ref` call detection there
  now routes resolved helper-path matching through shared
  `isLegacyOrCanonicalSoaHelperPath(...)` checks instead of direct canonical
  path comparisons, and source-lock coverage now asserts those shared helper
  call paths and declaration contracts.
- `soa-monomorph-fallback-shared-ref-canonicalizer`: template-monomorph
  fallback stdlib implicit-template path gating now reuses shared
  `canonicalizeLegacySoaRefHelperPath(...)` semantics helper wiring instead
  of a local lambda, and source-lock coverage now asserts that shared helper
  call path in fallback canonicalization checks.
- `soa-monomorph-expression-rewrite-shared-ref-canonicalizer`:
  template-monomorph expression rewrite now reuses shared
  `canonicalizeLegacySoaRefHelperPath(...)` semantics helper wiring instead
  of a local lambda, and source-lock coverage now asserts the shared helper
  call path while rejecting local canonicalization lambda definitions there.
- `soa-statement-returns-shared-ref-canonicalizer`: statement-return SoA
  `ref`/`ref_ref` escape classification now reuses shared
  `canonicalizeLegacySoaRefHelperPath(...)` semantics helper wiring instead
  of a local lambda; canonical SoA `ref`/`ref_ref` helper-family detection
  there now routes through shared `isCanonicalSoaRefLikeHelperPath(...)`
  checks instead of duplicated canonical-path prefix comparisons, and
  source-lock coverage now asserts those shared helper call paths while
  rejecting local canonicalization lambda definitions there.
- `soa-statement-bindings-shared-ref-canonicalizer`: statement-binding SoA
  standalone `ref`/`ref_ref` classifiers now reuse shared
  `canonicalizeLegacySoaRefHelperPath(...)` semantics helper wiring instead
  of a local lambda; canonical SoA `ref`/`ref_ref` helper-family detection
  there now routes through shared `isCanonicalSoaRefLikeHelperPath(...)`
  checks instead of duplicated canonical-path prefix comparisons, and
  source-lock coverage now asserts those shared helper call paths while
  rejecting local canonicalization lambda definitions there.
- `soa-infer-return-shared-ref-canonicalizer`: infer-collection return
  inference now reuses shared `canonicalizeLegacySoaRefHelperPath(...)`
  semantics helper wiring for SoA `ref`/`ref_ref` canonicalization instead of
  a local lambda, while keeping explicit legacy get/get_ref remapping;
  canonical SoA `ref`/`ref_ref` helper-family old-surface gating there now
  routes through shared `isLegacyOrCanonicalSoaHelperPath(...)` checks
  instead of direct canonical-path comparisons, and source-lock coverage now
  asserts those shared helper call paths.
- `soa-ir-lowerer-mutation-shared-ref-canonicalizer`: IR-lowerer
  collection-mutation same-path SoA `ref` write-target visibility probes now
  reuse shared `semantics::canonicalizeLegacySoaRefHelperPath(...)` wiring
  instead of a local canonicalization lambda, and source-lock coverage now
  asserts that shared helper call path.
- `soa-infer-dispatch-shared-ref-canonicalizer`: infer-collection dispatch
  method return-kind resolution now reuses shared
  `canonicalizeLegacySoaRefHelperPath(...)` for SoA `ref`/`ref_ref`
  canonicalization instead of a local lambda, while keeping explicit legacy
  get/get_ref remapping; canonical SoA `ref`/`ref_ref` helper-family checks
  there now also route through shared
  `isCanonicalSoaRefLikeHelperPath(...)` predicate wiring instead of direct
  canonical-path comparisons, and source-lock coverage now asserts those
  shared helper call paths.
- `soa-expr-map-shared-ref-canonicalizer`: expr-map SoA builtin dispatch now
  reuses shared `canonicalizeLegacySoaRefHelperPath(...)` for SoA
  `ref`/`ref_ref` canonicalization instead of a local lambda, while keeping
  explicit legacy get/get_ref remapping; canonical SoA `ref`/`ref_ref`
  helper-family gating there now also routes through shared
  `isCanonicalSoaRefLikeHelperPath(...)` predicate checks instead of direct
  canonical-path comparisons, and source-lock coverage now asserts those
  shared helper call paths.
- `soa-build-initializer-shared-ref-canonicalizer`: build-initializer SoA
  access-helper classification now reuses shared
  `canonicalizeLegacySoaRefHelperPath(...)` for SoA `ref`/`ref_ref`
  canonicalization instead of a local lambda, while keeping explicit legacy
  get/get_ref remapping, and source-lock coverage now asserts that shared
  helper call path.
- `soa-arg-and-borrow-shared-ref-canonicalizer`: resolved-call argument SoA
  standalone `ref`/`ref_ref` escape classification and expr-mutation borrow SoA
  receiver helper-path detection now both reuse shared
  `canonicalizeLegacySoaRefHelperPath(...)` wiring instead of local
  canonicalization helpers, and source-lock coverage now asserts those shared
  helper call paths.
- `soa-ir-lowerer-ref-matcher-shared-canonicalizer`: ir-lowerer
  collection-mutation non-method SoA `ref` write-target builtin-path matching
  now canonicalizes candidate callee paths through shared
  `semantics::canonicalizeLegacySoaRefHelperPath(...)` before builtin
  ref-family checks, and source-lock coverage now asserts that shared helper
  call path in the matcher.
- `soa-method-target-shared-ref-canonicalizer`: method-target SoA
  builtin-helper classification now canonicalizes legacy resolved
  `ref`/`ref_ref` spellings through shared
  `canonicalizeLegacySoaRefHelperPath(...)` before builtin
  import/local-definition gating; canonical `ref`/`ref_ref` helper-family
  detection there now routes through shared
  `isCanonicalSoaRefLikeHelperPath(...)` predicate checks instead of direct
  canonical-path comparisons, and source-lock coverage now asserts those
  shared helper call paths in method-target resolution.
- `soa-monomorph-template-carry-shared-helper-predicate`:
  template-monomorph expression-rewrite synthetic same-path SoA template-carry
  gating now removes duplicated inline helper-family ref/ref_ref path lists
  and reuses `isSyntheticSamePathSoaHelperTemplateCarryPath(...)` for both
  call and method resolved-path branches, while preserving the
  call-receiver/`vector`-receiver behavior gate.
- `soa-monomorph-template-carry-redundant-canonical-ref-checks-removed`:
  template-monomorph expression-rewrite synthetic same-path SoA template-carry
  helper-path predicate now routes canonicalized SoA `ref`/`ref_ref` matching
  through shared `isCanonicalSoaRefLikeHelperPath(...)` checks instead of
  duplicating direct canonical `/std/collections/soa_vector/ref(_ref)` path
  comparisons.
- `soa-monomorph-template-carry-nonref-helper-path-factoring`:
  template-monomorph expression-rewrite synthetic same-path SoA template-carry
  helper-path predicate now factors legacy/canonical non-ref helper-path
  matching through `isSyntheticSamePathSoaCarryNonRefHelperPath(...)` instead
  of duplicated inline helper-path disjunctions, while keeping canonicalized
  `ref`/`ref_ref` matching unchanged.
- `soa-monomorph-fallback-get-ref-helper-path-factoring`:
  legacy/canonical SoA helper-path matching is now shared through
  `isLegacyOrCanonicalSoaHelperPath(...)` in semantics helpers, and
  template-monomorph fallback stdlib implicit-template path gating now reuses
  that shared predicate for `get_ref` helper paths instead of a local helper.
- `soa-monomorph-fallback-ref-like-canonical-path-factoring`:
  canonical SoA `ref`/`ref_ref` helper-family detection is now shared through
  `isCanonicalSoaRefLikeHelperPath(...)` in semantics helpers, and both
  template-monomorph fallback stdlib implicit-template path gating plus
  pending unknown-method helper canonicalization now reuse that shared
  predicate instead of duplicated direct canonical-path comparisons.
- `soa-monomorph-implicit-ref-path-canonicalization`: template-monomorph
  implicit-template SoA pending `ref`/`ref_ref` helper gating now canonicalizes
  resolved helper paths (template-suffix stripping plus legacy
  `/soa_vector/ref(_ref)` remapping) before helper-family checks, and
  canonical `ref`/`ref_ref` helper-family gating there now also routes
  through shared `isCanonicalSoaRefLikeHelperPath(...)` predicate checks
  instead of direct canonical-path disjunctions, and source-lock coverage now
  rejects the old direct
  `if (resolvedPath == "/soa_vector/ref" || ...)` call-shape check there.
- `soa-monomorph-fallback-ref-path-canonicalization`: template-monomorph
  fallback stdlib implicit-template path gating now canonicalizes legacy
  resolved SoA `ref`/`ref_ref` helper spellings before helper-family checks,
  and source-lock coverage now rejects direct
  `path == "/soa_vector/ref(_ref)"` checks there.
- `arg-pack-dereference-struct-inference`: IR struct-type inference now
  propagates through `dereference(...)`, so indexed borrowed arg-pack values
  (for example `args<Reference<Map<K, V>>>`) preserve their struct identity
  when forwarded directly into helper calls instead of failing with
  `<unknown>` struct-parameter mismatches.
- `experimental-vector-negative-index-guard`: `/std/collections/experimental_vector/*`
  checked access now rejects negative indices via `vectorCheckIndex` (same
  bounds-error path as high indices), and `vectorAtUnsafe` now routes through
  the same guard for both negative and positive out-of-range indices, so those
  accesses no longer fall through to invalid pointer arithmetic,
  uninitialized-slot reads, or indirect-address traps; the guard now triggers
  bounds errors through zero-length `array` indexing instead of constructing a
  canonical `/vector/*` value.
- `experimental-vector-push-growth-overflow-contract`:
  `/std/collections/experimental_vector/vectorPush` now diverts capacity-
  doubling overflow into the shared runtime-contract path instead of letting
  the doubled capacity wrap or continue into unchecked growth.
- `ir-lowerer-vector-mutator-call-form-expression-resolution`: IR lowering now
  probes bare vector mutator expression calls (`push`, `pop`, `reserve`,
  `clear`, `remove_at`, `remove_swap`) through the same receiver-based method
  resolution path as statement form, so VM-lowered user-defined `/vector/*`
  expression shadows no longer fall into backend-owned `vector helper`
  rejects.
- `ir-lowerer-query-local-collection-join-inference`: IR lowering now
  preserves collection binding metadata for local `auto` bindings sourced from
  resolved collection-returning calls and `if` branch joins, so query-local
  `/vector/count` direct-call and slash-method forms lower successfully across
  compile-pipeline, C++ emitter, VM, and native coverage instead of failing
  the old branch-compatibility boundary.
- `map-bare-access-canonical-helper-contract`: bare-root
  `contains(values, key)`, `at(values, key)`, and `at_unsafe(values, key)`
  on builtin `map<K, V>` now reject with canonical
  `unknown call target: /std/collections/map/*` diagnostics unless a
  canonical helper is imported or explicitly defined, and bare
  `values.at(...)` / `values.at_unsafe(...)` method sugar now follows the
  same-path reject contract instead of falling back to compiler-owned
  access behavior; the C++ emitter now matches that direct-call contract for
  bare `at` / `at_unsafe` calls instead of compiling them through the old
  builtin access fallback.
- `semantic-memory-no-import-math-vector-phase-coverage`: semantic-memory
  benchmark harness coverage now executes `semantic_memory_benchmark.py` with
  `--fixtures no_import,math_vector --phases ast-semantic,semantic-product`
  and asserts the exact four fixture-phase output tuples, guarding the P0
  attribution fixture contract.
- `semantic-memory-primary-math-star-repro-fixture`: semantic-memory
  benchmarking now treats `benchmarks/semantic_memory/fixtures/math_star_repro.prime`
  as the primary minimized high-RSS `/std/math/*` reproducer, with benchmark
  harness coverage locking that fixture as the first `primary` fixture and
  keeping its minimal three-step call-chain shape.
- `semantic-memory-vector-matrix-math-star-phase-coverage`: semantic-memory
  benchmark harness coverage now executes `semantic_memory_benchmark.py` with
  `--fixtures math_vector_matrix,math_star_repro --phases ast-semantic,semantic-product`
  and asserts the exact four fixture-phase output tuples, guarding the
  vector+matrix-vs-star attribution fixture contract.
- `semantic-memory-non-math-large-include-comparability`: semantic-memory
  benchmark fixtures now include `/std/bench_non_math/*` (non-math
  `/std/collections/*` + `/std/file/*`) via
  `benchmarks/semantic_memory/fixtures/non_math_large_include.prime`, and
  benchmark-harness coverage asserts non-math-vs-math-star ast-semantic
  top-level definition counts stay both large and ratio-bounded for
  include-scaling attribution.
- `semantic-memory-inline-vs-import-math-pair`: semantic-memory benchmark
  harness now validates `inline_math_body.prime` and `imported_math_body.prime`
  keep identical body text after stripping import lines, then executes
  `semantic_memory_benchmark.py` with those fixtures across
  `ast-semantic,semantic-product` and asserts the exact four fixture-phase
  tuples to guard inline-vs-stdlib-import attribution runs.
- `semantic-memory-scale-slope-guard`: semantic-memory benchmark harness now
  validates `scale_1x`/`scale_2x`/`scale_4x` fixture-size contracts and runs
  `semantic_memory_benchmark.py --fixtures scale_1x,scale_2x,scale_4x
  --phases ast-semantic,semantic-product`, asserting the exact six
  fixture-phase tuples plus canonical per-phase `scale_slopes` rows
  (`x_axis=[1,2,4]`, ordered fixtures, finite RSS/time slope values).
- `semantic-memory-wall-rss-machine-report-rows`: semantic-memory benchmark
  harness coverage now executes `semantic_memory_benchmark.py` with
  `--runs 1 --fixtures no_import --phases ast-semantic --report-json ...`
  and asserts machine-readable report contract rows include the schema,
  one-sample `wall_seconds`/`peak_rss_bytes` arrays, and median/worst
  aggregates equal to that single sample for the fixture-phase tuple.
- `semantic-memory-key-cardinality-report-rows`: semantic-memory benchmark
  harness coverage now executes `semantic_memory_benchmark.py` with
  `--runs 1 --fixtures no_import,math_vector --phases ast-semantic,semantic-product --report-json ...`
  and asserts semantic-product rows carry `key_cardinality` fields
  (`distinct_direct_call_target_keys`, `distinct_method_call_target_keys`,
  `max_target_key_length`) while ast-semantic rows omit that payload.
- `semantic-memory-default-three-run-rollups`: semantic-memory benchmark
  harness coverage now executes `semantic_memory_benchmark.py` without an
  explicit `--runs` override and asserts the default `runs == 3` contract,
  three-sample `wall_seconds`/`peak_rss_bytes` arrays, and row
  median/worst rollups that match computed values from those samples.
- `semantic-memory-independent-fact-family-toggles`: semantic-memory
  benchmark harness coverage now runs `semantic_memory_benchmark.py` with
  `--fact-families direct_call_targets`, `method_call_targets`, and
  `direct_call_targets,method_call_targets` on the same semantic-product
  fixture and asserts direct/method key-cardinality metrics toggle
  independently while `fact_families` row metadata preserves the selected
  collector set.
- `semantic-memory-validator-vs-fact-ab-mode`: semantic-memory benchmark
  harness now supports `--semantic-validation-without-fact-emission
  off|on|both` (with `--no-fact-emission` as a compatibility alias), emits
  `semantic_validation_without_fact_emission_deltas` in report output, and
  runtime benchmark-harness coverage now asserts `both` mode yields paired
  facts-on/facts-off semantic-product rows with the expected key-cardinality
  suppression and delta-report metadata.
- `semantic-memory-semantic-product-force-ab-mode`: semantic-memory
  benchmark harness now supports `--semantic-product-force both` for paired
  forced on/off A/B runs (restricted to non-`semantic-product` phases),
  emits `semantic_product_force_deltas` in report output, and runtime
  benchmark-harness coverage now asserts forced-on `ast-semantic` runs
  produce semantic-product-build facts while forced-off runs keep those fact
  counters at zero.
- `semantic-memory-baseline-report-matrix-guard`: benchmark harness now
  validates the checked-in baseline report as structured JSON (schema/runs,
  full fixture×phase pair coverage, per-row RSS/time metrics, and expensive
  offender annotations) instead of relying on substring presence checks.
- `semantic-memory-expensive-ctest-threshold-guard`: benchmark harness now
  validates baseline expensive-threshold exceedance evidence and locks both
  semantic-memory CTest entries (`PrimeStruct_semantic_memory_benchmark` and
  `PrimeStruct_semantic_memory_trend`) to serial expensive properties.
- `compile-pipeline-semantic-product-decision-states`: compile-pipeline
  output now publishes an explicit `CompilePipelineSemanticProductDecision`
  (`SkipForAstSemanticDump`, non-consuming skip, and benchmark force on/off
  states), and semantic-product request/build behavior derives from that
  decision helper instead of an inline boolean expression.
- `compile-pipeline-ast-semantic-phase-counter-proof`: semantic phase
  counters now set `semanticProductBuild.callsVisited = 1` only when
  semantic-product build runs, and compile-pipeline ast-semantic benchmark
  counter coverage asserts that validation counters are populated while
  semantic-product build counters stay zero on skipped paths.
- `compile-pipeline-semantic-product-generation-matrix`: compile-pipeline
  registry coverage now validates a focused dump/emit matrix where
  non-consuming dump stages (`pre_ast`, `ast`, `ir`, `type-graph`,
  `ast-semantic`) keep semantic-product request/build/publication disabled
  while `semantic-product` dumps plus consuming `native`/`vm` emit paths keep
  it enabled.
- `primec-unknown-emit-preflight-skip`: `primec` now resolves backend kind
  before running compile pipeline and flips
  `skipSemanticProductForNonConsumingPath` for no-dump unknown emit kinds, so
  non-consuming unknown-backend dispatch paths avoid semantic-product
  allocation before reporting the canonical emit error.
- `ir-pipeline-helper-non-consuming-semantic-intent`: the stdlib-import
  test helper `parseAndValidateThroughCompilePipeline(...)` now selects
  semantic-product intent from `semanticProgramOut` and applies
  `SkipForNonConsumingPath` when semantic output is not requested, with
  backend-registry source-lock coverage pinning this wiring.
- `semantic-product-first-module-index-slice`: module-resolved
  `direct_call_targets`, `method_call_targets`, `callable_summaries`, and
  `binding_facts` are now indexed (`*Indices`) into canonical semantic-program
  vectors instead of storing per-module copied entries; formatter/adapter
  update paths resolve those families through
  `semanticProgramDirectCallTargetView(...)`,
  `semanticProgramMethodCallTargetView(...)`,
  `semanticProgramCallableSummaryView(...)`, and
  `semanticProgramBindingFactView(...)`, and combined first-slice parity
  coverage now locks identical semantic-product formatter output between flat
  and module-index storage for those families.
- `semantic-product-second-module-index-slice`: module-resolved
  `bridge_path_choices`, `return_facts`, `local_auto_facts`, `query_facts`,
  `try_facts`, and `on_error_facts` now route through
  `bridgePathChoiceIndices`, `returnFactIndices`, `localAutoFactIndices`,
  `queryFactIndices`, `tryFactIndices`, and `onErrorFactIndices` into
  canonical semantic-program vectors, and source-lock coverage now rejects
  regression strings that would repopulate per-module copied
  `returnFacts`/`localAutoFacts`/`queryFacts` vectors.
- `semantic-product-second-slice-parity`: semantic snapshot coverage now
  includes a combined second-slice formatter parity regression that compares
  flat vs module-index output for `return_facts`, `local_auto_facts`,
  `query_facts`, `bridge_path_choices`, `try_facts`, and `on_error_facts`
  in one semantic-product fixture.
- `semantic-snapshot-traversal-overlap-ranking`: the snapshot traversal
  inventory now includes an explicit overlap-scoring rubric and ranking table;
  the highest-overlap first merge pair for P2 remains
  `directCallTargetSnapshotForSemanticProduct` +
  `bridgePathChoiceSnapshotForSemanticProduct` because both share the same
  non-method recursive traversal and callee-path resolution path with minimal
  collector-specific branching.
- `semantic-snapshot-direct-bridge-shared-collection`: semantic-product build
  now derives bridge-path snapshots from already-collected direct-call
  snapshots when both collector families are enabled, avoiding a second
  redundant traversal while keeping output-order parity locked by
  `compile pipeline direct and bridge collector merge keeps output-order parity`.
- `semantic-graph-local-auto-structured-key-ab-deltas`: graph-local
  `auto` fact storage now uses structured `GraphLocalAutoKey`
  (`scopePathId`, `sourceLine`, `sourceColumn`) with a dedicated
  `graphLocalAutoScopePathInterner_` instead of string-concatenated key
  text, and benchmark A/B runs now support
  `--benchmark-semantic-graph-local-auto-legacy-key-shadow` plus
  `scripts/semantic_memory_benchmark.py --graph-local-auto-key-mode
  compact|legacy-shadow|both`, emitting per-fixture/phase
  `graph_local_auto_key_mode_deltas` RSS/time rollups.
- `semantic-graph-local-auto-side-channel-flat-map-ab-deltas`: graph-local
  `auto` side-channel state now co-locates binding/initializer/query/try/
  direct/method fields in single-map `GraphLocalAutoFacts` entries keyed by
  `GraphLocalAutoKey` via `graphLocalAutoFacts_` (instead of maintaining
  nine separate keyed maps), and benchmark A/B runs now support
  `--benchmark-semantic-graph-local-auto-legacy-side-channel-shadow` plus
  `scripts/semantic_memory_benchmark.py --graph-local-auto-side-channel-mode
  flat|legacy-shadow|both`, emitting per-fixture/phase
  `graph_local_auto_side_channel_mode_deltas` RSS/time rollups.
- `semantic-graph-local-auto-dependency-scratch-pmr-ab-deltas`: graph-local
  `auto` dependency-count transient storage now uses PMR-backed
  `GraphLocalAutoDependencyScratch` (`std::pmr::monotonic_buffer_resource`
  plus PMR dependency map) behind benchmark toggle
  `--benchmark-semantic-disable-graph-local-auto-dependency-scratch-pmr`,
  and semantic-memory benchmark A/B runs now support
  `--graph-local-auto-dependency-scratch-mode pmr|std|both`, emitting
  per-fixture/phase `graph_local_auto_dependency_scratch_mode_deltas`
  RSS/time rollups.
- `semantic-map-non-local-growth-inventory-2026-04`: section 3 of
  `docs/ownership_runtime_soa_touchpoints.md` now carries the refreshed
  Group 13 non-local map-growth receiver inventory with per-family rewrite
  touchpoints and an explicit `Still-unhandled non-local receiver families`
  state of `None in the current inventory`.
- `semantic-map-non-local-growth-group13-closed-2026-04`: Group 13 map
  non-local growth TODO leaves are now fully archived to
  `docs/todo_finished.md`, with compile-run conformance coverage for helper-
  return receiver insert forms across C++/VM/native harnesses and rewrite
  coverage locking `/std/collections/map/insert_builtin` normalization.
- `semantic-soa-fallback-inventory-2026-04`: section 4 of
  `docs/ownership_runtime_soa_touchpoints.md` now carries the Group 14
  still-unhandled SoA fallback inventory as explicit S1/S2/S3 families with
  helper shapes, behavior notes, and owning-file touchpoints, and the
  matching refresh leaf is archived in `docs/todo_finished.md`.
- `semantic-soa-non-struct-literal-parity-test-2026-04`: Group 14 now
  archives the focused SoA fallback parity reject at
  `tests/unit/test_compile_run_imports_operations.h` (`rejects root
  non-struct non-empty soa_vector literal with semantic/emit parity in C++
  emitter`), locking matching diagnostics across `--dump-stage ast-semantic`
  and `--emit=exe`.
- `semantic-soa-to-aos-ref-bridge-migration-2026-04`: Group 14 archives the
  S2 migration slice where canonical `/std/collections/soa_vector/to_aos_ref`
  compatibility was brought to parity with `to_aos` for inline-parameter SoA
  bridge matching, and focused compile-run coverage for root
  `/std/collections/soa_vector/to_aos_ref<T>(location(values))` in the C++
  emitter is locked passing.
- `semantic-soa-inline-param-canonical-callee-guard-2026-04`: Group 14 now
  archives removal of the empty-callee builtin `soa_vector` -> experimental
  `SoaVector__*` bridge fallback in inline-parameter lowering: S2 bridge
  matching now requires canonical helper-path classification, and
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_56.h`
  locks that empty-callee inputs fail while helper-path-scoped bridge matching
  remains explicit.
- `semantic-soa-s2-canonical-conversion-loops-2026-04`: Group 14 now archives
  the S2 slice where canonical `/std/collections/soa_vector/to_aos` accepts
  builtin `soa_vector<T>` at the canonical surface and now loops through
  canonical `/std/collections/soa_vector/count|get` helpers (instead of
  directly forwarding to experimental conversion helpers), with AST-semantic
  dump coverage in `tests/unit/test_compile_run_text_filters_dumps.h`
  locking canonical helper calls inside the canonical `to_aos` body.
- `semantic-soa-s2-to-aos-bridge-eligibility-prune-2026-04`: Group 14 now
  archives the S2 slice where inline-parameter bridge matching no longer
  treats canonical `/std/collections/soa_vector/to_aos` as a builtin
  `/soa_vector` -> `SoaVector__*` bridge-eligible callee path; focused IR
  validation now locks canonical `to_aos` rejection while direct experimental
  conversion helper paths remain bridge-eligible.
- `semantic-soa-s2-to-aos-ref-bridge-eligibility-prune-2026-04`: Group 14 now
  archives the S2 slice where canonical
  `/std/collections/soa_vector/to_aos_ref` accepts `Reference<soa_vector<T>>`
  and now loops through canonical
  `/std/collections/soa_vector/count_ref|get_ref` (instead of forwarding
  through `to_aos(dereference(...))`), with the helper signature now normalized
  to `Reference<SoaVector<T>>` so canonical `to_aos_ref` still accepts borrowed
  experimental `SoaVector<T>` receivers, inline-parameter bridge matching no
  longer treats canonical `to_aos_ref` as a bridge-eligible callee path, and
  template-monomorph receiver resolution no longer maps canonical `to_aos_ref`
  to direct experimental `soaVectorToAosRef`.
- `semantic-soa-s2-direct-helper-bridge-removal-2026-04`: S2 inline-parameter
  lowering no longer treats direct
  `/std/collections/experimental_soa_vector_conversions/soaVectorToAos(_Ref)`
  callees as bridge-eligible for builtin `soa_vector`, and focused IR +
  compile-run coverage now lock the resulting `struct parameter type mismatch`
  contract on those direct helper paths.
- `semantic-soa-s2-to-aos-canonical-helper-path-factoring-2026-04`: infer-
  collection, expr-method-target, expr-map late-builtin, expr
  late-map/soa pre-dispatch, and method-target builtin
  import/local-definition `to_aos(_ref)` gating now canonicalize legacy
  resolved helper spellings (`/to_aos`, `/to_aos_ref`,
  `/soa_vector/to_aos_ref`) through shared
  `canonicalizeLegacySoaToAosHelperPath(...)` +
  `isCanonicalSoaToAosHelperPath(...)` checks instead of duplicated direct
  resolved-path comparisons; expr pre-dispatch gating also requires the
  canonical `/std/collections/soa_vector/` namespace before forcing late
  builtin validation so same-path `/to_aos(_ref)` helper contracts stay
  unchanged, and template-monomorph expression-rewrite canonical stdlib
  collection helper whitelisting now canonicalizes helper spellings through
  shared `canonicalizeLegacySoaToAosHelperPath(...)` and routes helper-family
  checks through explicit shared
  `isLegacyOrCanonicalSoaHelperPath(..., "to_aos")` and
  `isLegacyOrCanonicalSoaHelperPath(..., "to_aos_ref")` predicates instead of
  broad canonical-family predicates, and template-monomorph method-target
  `to_aos(_ref)` fallback preference now canonicalizes same-path helper
  spellings through shared `canonicalizeLegacySoaToAosHelperPath(...)` plus
  `isCanonicalSoaToAosHelperPath(...)` guards instead of hardcoded canonical-
  path literals, and template-monomorph expression-rewrite root-builtin
  `to_aos(_ref)` shadow-receiver fallback now canonicalizes same-path helper
  spellings through shared `canonicalizeLegacySoaToAosHelperPath(...)` and
  routes helper-family checks through explicit shared
  `isLegacyOrCanonicalSoaHelperPath(..., "to_aos")` and
  `isLegacyOrCanonicalSoaHelperPath(..., "to_aos_ref")` predicates instead of
  broad canonical-family predicates before root-builtin helper visibility
  probes, and expr/expr-map borrowed canonical `to_aos_ref`
  gating now routes through shared
  `isLegacyOrCanonicalSoaHelperPath(..., "to_aos_ref")` checks instead of
  direct canonical-path equality checks, and infer-collection/expr-method-
  target `to_aos(_ref)` vector-target inference now routes through per-helper
  `isLegacyOrCanonicalSoaHelperPath(...)` checks for `to_aos` and
  `to_aos_ref` instead of broad canonical-family predicate checks, and
  expr/expr-map canonical `to_aos` helper-family gating now routes through
  shared `isLegacyOrCanonicalSoaHelperPath(..., "to_aos")` checks instead of
  broad `isCanonicalSoaToAosHelperPath(...)` family predicates, and
  method-target builtin SoA helper import/local-definition `to_aos(_ref)`
  matching now routes through explicit shared
  `isLegacyOrCanonicalSoaHelperPath(...)` checks for both `to_aos` and
  `to_aos_ref` instead of broad canonical-family predicates, and template-
  monomorph method-target same-path `to_aos(_ref)` fallback family gating now
  routes through explicit shared `isLegacyOrCanonicalSoaHelperPath(...)`
  checks for `to_aos` and `to_aos_ref` instead of broad
  `isCanonicalSoaToAosHelperPath(...)` family predicates; after that
  migration, the now-unused `isCanonicalSoaToAosHelperPath(...)`
  semantics-helper declaration/definition surface was removed, and
  build-initializer SoA helper same-path target synthesis now routes
  through shared `preferredSamePathSoaHelperTarget(...)` so both `to_aos`
  and `to_aos_ref` use root same-path helper targets (`/to_aos(_ref)`)
  instead of legacy `/soa_vector/to_aos_ref`, and
  `canonicalizeLegacySoaToAosHelperPath(...)` now only remaps root
  same-path aliases (`/to_aos`, `/to_aos_ref`) and no longer remaps
  `/soa_vector/to_aos_ref`.
- `semantic-soa-s2-canonical-count-get-bridge-substrate-2026-04`: S2 inline-
  parameter bridge matching now treats canonical `/std/collections/soa_vector`
  `count|get` callee paths (plus same-path aliases) as bridge-eligible for
  builtin `/soa_vector` -> experimental `SoaVector__*` value-parameter
  adaptation, and focused IR validation in
  `tests/unit/test_ir_pipeline_validation_chunks/test_ir_pipeline_validation_56.h`
  locks those canonical count/get bridge paths as passing.
- `semantic-soa-s1-literal-capacity-slice-2026-04`: Group 14 archives the
  S1 literal-capacity migration slice where VM/native lowering no longer
  rejects struct-element non-empty builtin `soa_vector` literals above the
  former 256-element local-capacity ceiling; compile-run coverage now locks
  successful native/VM materialization for 257-element root
  `soa_vector<Particle>(...)` literals.
- `semantic-soa-s1-literal-diagnostics-semantic-stage-2026-04`: Group 14 now
  archives the S1 diagnostics slice where non-struct non-empty builtin
  `soa_vector` literal errors come from semantic validation (canonical
  `soa_vector requires struct element type`, `stage: semantic`) instead of
  lowering-specific diagnostics; VM/native compile-run regressions now lock the
  semantic-stage provenance.
- `semantic-method-target-scratch-candidates`: infer-stage method-target
  resolution now reuses `CallTargetResolutionScratch::methodReceiverResolutionCandidates`
  for scoped definitions/executions in `resolveInferMethodCallPath(...)`,
  avoiding per-call receiver-candidate vector allocations when probing
  canonical receiver alias paths before return-type inference.
- `semantic-method-target-canonical-fragment-reuse`: infer-stage method-target
  receiver alias candidate construction now routes through
  `appendCanonicalReceiverResolutionCandidates(...)` and cached
  `joinMethodTarget(...)` fragments (`/vector`↔`/std/collections/vector`,
  `/map`↔`/std/collections/map`) so scoped method-path resolution avoids
  ad-hoc repeated canonical-path concatenation churn.
- `semantic-method-target-memoization-ab-deltas`: infer-stage method-target
  resolution now memoizes resolved method targets in
  `methodTargetMemoCache` keyed by semantic node + receiver type + method
  name, with benchmark-controlled bypass via
  `--benchmark-semantic-disable-method-target-memoization`; semantic-memory
  benchmark A/B runs now support `--method-target-memoization on|off|both`
  and emit per-fixture/phase `method_target_memoization_deltas` RSS/time
  rollups.
- `semantic-call-target-canonical-fragment-caches`: scoped call-target
  concrete-path resolution now caches canonical overload/specialization
  fragments (`overloadFamilyPrefixCache`, `specializationPrefixCache`, and
  `overloadCandidatePathCache`) in `CallTargetResolutionScratch`, avoiding
  repeated `path + "__ov..."` / `path + "__t..."` concatenation churn on
  repeated call-target resolution within the same owner scope.
- `compile-pipeline-dump-helper-explicit-semantic-intent`: compile-pipeline
  dump capture helpers now require explicit
  `CompilePipelineSemanticProductIntent` plumbing at callsites (instead of an
  implicit dump-stage inference helper), with semantic-boundary helpers
  passing `SkipForNonConsumingPath` for `ast-semantic`/`ir` and `Require` for
  `semantic-product`; backend-registry source-lock coverage pins this contract.
- `primevm-debug-replay-trace-only-fast-path`: `primevm` now keeps
  `--debug-replay` on a trace-only fast path whenever no dump stage is
  requested, so that non-consuming entrypoint bypasses compile-pipeline/IR
  preparation semantic-product demand; backend-registry source-lock coverage
  asserts this branch remains before the `runCompilePipeline(...)` call.
- `semantic-product-direct-call-routing`: production lowering now validates
  that every non-method direct call has a published semantic-product routing
  fact before lowering starts, and semantic-product-aware direct-call
  resolution no longer falls back to scope/import path recovery when those
  facts are absent.
- `semantic-product-method-call-routing`: production lowering now validates
  that every method call has a published semantic-product routing fact
  before lowering starts, and semantic-product-aware method resolution no
  longer falls back to receiver/helper inference when those facts are
  absent.
- `semantic-product-method-call-path-id-only`: `SemanticProgramMethodCallTarget`
  no longer stores a `resolvedPath` string shadow; semantic-product formatter,
  backend conformance helpers, and IR-lowerer method-target adapters now read
  method-call resolved paths exclusively via `resolvedPathId`, and adapter
  indexing ignores method-call facts with missing/invalid path ids.
- `semantic-product-bridge-path-routing`: production lowering now requires
  published semantic-product bridge-path choices for collection-helper
  bridge calls, and semantic-aware direct-call resolution no longer routes
  same-path or canonical collection-helper bridge calls through duplicated
  direct-call targets when the bridge choice is absent.
- `semantic-product-bridge-path-helper-id-only`: `SemanticProgramBridgePathChoice`
  no longer stores a `helperName` string shadow; semantic-product formatter,
  semantic snapshot tests, and backend conformance tests now consume bridge
  helper names exclusively via `helperNameId`, and bridge-path completeness
  rejects missing/out-of-range helper IDs.
- `semantic-product-callable-summary-path-id-only`: `SemanticProgramCallableSummary`
  no longer stores a `fullPath` string shadow; semantic-product formatter,
  semantic-product target adapters, and manual callable-summary fixtures now
  resolve callable paths exclusively through `fullPathId`, and callable
  summary completeness rejects missing/out-of-range path IDs.
- `semantic-product-completeness-and-provenance`: mutation-based lowering
  tests now prove post-validation AST call and transform drift does not
  change semantic-product-owned lowering meaning, while canonical
  instruction source maps still follow AST-backed line and column data.
- `semantic-product-binding-routing`: production lowering now validates
  that parameter/local binding facts are published before lowering starts,
  and semantic-aware binding classification no longer falls back to AST
  transforms for semantic-id-backed binding expressions. Binding
  completeness now rejects facts with missing/invalid `resolvedPathId`
  values (including `InvalidSymbolId`).
- `semantic-product-cross-backend-conformance`: compile-pipeline backend
  conformance now pins the same semantic-product-owned direct-call,
  method-call, bridge-path, local-`auto`, query, `try(...)`, `on_error`,
  and return facts across `cpp-ir`, `vm`, and `native`, so those backend
  entrypoints all prove they consume the same lowering-facing semantic
  product boundary.
- `semantic-product-local-auto-lowering-input`: production lowering now
  validates that semantic-id-backed implicit/`auto` local bindings have
  published semantic-product local-`auto` facts before lowering starts, and
  local binding setup now consumes the published local-`auto` binding type
  instead of re-inferring that metadata from AST initializer shape.
- `semantic-product-query-and-try-lowering-inputs`: semantic-id-backed
  generic call-result metadata now requires published semantic-product query
  facts, and semantic-id-backed `try(...)` lowering plus `try(...)`
  expression-kind inference now require published semantic-product `try(...)`
  facts instead of probing AST/result metadata as an optional fallback.
- `semantic-product-lowering-entrypoints`: `prepareIrModule(...)` and
  `IrLowerer::lower(...)` now reject null `SemanticProgram` inputs
  immediately, so production lowering only crosses the boundary with a
  published semantic product.
- `semantic-product-entry-params-and-on-error-bound-args`: lowering now
  requires published semantic-product entry parameter facts for entry
  count/argv setup, and semantic-product-backed `on_error` setup now
  consumes published `boundArgTexts` instead of falling back to AST
  parameters or reparsing `on_error` transforms.
- `semantic-product-required-callable-metadata`: once lowering is handed a
  `SemanticProgram`, entry effect/capability masks, callable effect validation,
  lowered callable metadata, entry return metadata, and `on_error` wiring now
  require published callable summaries plus the matching return/`on_error`
  facts instead of silently falling back to AST transforms.
- `semantic-product-return-metadata-consumption`: once lowering is handed a
  `SemanticProgram`, `getReturnInfo(...)` now precomputes callable return and
  result metadata from published callable summaries plus return facts instead
  of rebuilding that cache from AST/type-resolution inference, so downstream
  result/return consumers stay on semantic-product-owned metadata after the
  lowering boundary.
- `semantic-product-temporary-call-classification`: statement binding and
  default-parameter classification now thread the semantic-product target
  adapter through `inferStatementBindingTypeInfo(...)` and
  `inferCallParameterLocalInfo(...)`, so semantic-id-backed temporary calls
  prefer published binding/query facts over AST transform/collection-shape
  fallback when lowering classifies local/result metadata after the boundary.
- `semantic-product-provenance-handles`: lowering-consumed semantic-product
  fact families now carry explicit stable `provenanceHandle` values derived
  from the published semantic node ids, the semantic-product dump exposes
  `provenance_handle=<id> source="line:column"` for those facts, and the
  semantic-product snapshot suite now locks provenance-handle determinism
  alongside semantic-node id determinism across repeated runs plus unrelated
  helper-definition reorderings.
- `semantic-product-result-metadata-preflight`: lowerer entry setup now
  validates callable-summary result metadata, return-fact binding types,
  query result payload metadata, and `try(...)` value metadata before
  lowering begins, so malformed published semantic-product facts fail at the
  entrypoint instead of surfacing later inside result/return inference.
- `semantic-product-return-query-try-on-error-joins`: the temporary lowerer
  semantic-product adapter now keys return, local-`auto`, query, `try(...)`,
  and `on_error` facts by structural semantic ids; local-`auto` lookups stay
  semantic-id-first and then fall back deterministically to
  (`initializerResolvedPathId`, `bindingNameId`), query lookups stay
  semantic-id-first and then fall back deterministically to
  (`resolvedPathId`, `callNameId`), try lookups stay semantic-id-first and
  then fall back deterministically to (`operandResolvedPathId`, source
  location), and return/`on_error` lookups stay semantic-id-first while
  keeping deterministic `definitionPathId` fallback when definition semantic
  ids are absent in manual fixture coverage.
- `semantic-product-structural-node-ids`: validated AST definitions,
  executions, and expressions now carry deterministic structural
  `semanticNodeId` values hashed from their resolved scope path plus traversal
  segment, and the lowerer semantic-product adapter now keys direct-call,
  method-call, bridge-path, and binding facts off those ids instead of the old
  `line:column:name` join plus duplicate-key collapse.
- `template-monomorph-core-utilities-header`:
  shared TemplateMonomorph core utility helpers now live in
  `src/semantics/TemplateMonomorphCoreUtilities.h` and are included by
  `TemplateMonomorph.cpp`, covering builtin type and collection
  classification plus overload path and import utility helpers.
- `uninitialized-type-helpers-split`: Group 10 reduced
  `src/ir_lowerer/IrLowererUninitializedTypeHelpers.cpp` to 578 lines by
  moving entry/runtime setup-builder orchestration into
  `src/ir_lowerer/IrLowererUninitializedSetupBuilders.cpp` and struct-return
  inference into `src/ir_lowerer/IrLowererUninitializedStructInference.cpp`,
  while leaving storage-access helpers in `IrLowererUninitializedTypeHelpers.cpp`
  so the existing helper API surface and validation coverage remain intact.

### template-monomorph-final-orchestration-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Final monomorphization orchestration now routes import-alias setup and definition/execution rewrite passes
  through `src/semantics/TemplateMonomorphFinalOrchestration.h`.
- Evidence: Group 9 extracted `buildImportAliases(...)`, definition rewrite coordination, and execution rewrite
  coordination out of `monomorphizeTemplates(...)` into dedicated helper functions.

### template-monomorph-source-definition-setup-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Source-definition collation, helper-overload family expansion, and template-root entry-path validation for
  monomorphization now run via `src/semantics/TemplateMonomorphSourceDefinitionSetup.h`.
- Evidence: Group 9 extracted the monomorph setup block from `monomorphizeTemplates(...)` into
  `initializeTemplateMonomorphSourceDefinitions(...)`.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique
  operational value.
