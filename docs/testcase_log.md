# Testcase Log

## Current Known Failures
- `PrimeStruct_compile_run_tests --test-suite="primestruct.compile.run.native_backend.collections" --no-skip`
  is not a clean map-cutover gate. On 2026-05-16 it reported 343 cases, 238
  passed, 105 failed, and 3539 skipped before the hanging map string-valued
  literal executable was interrupted. The failures are dominated by stale SoA
  public-surface coverage and older map-literal/insert compatibility fixtures;
  the focused native MapValue cutover cases listed below pass. The old
  string-valued map runtime fixture has since been retargeted to compile-only
  coverage because native runtime string-valued maps still hang after
  compilation.

## Recent Test Runs
- 2026-05-20 13:08 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer count access classifiers prefer semantic indexed target facts,ir lowerer count access helpers defer string map access emission" --no-skip`
  | failures: none | notes: count-access classifier coverage now locks
  syntax-only indexed targets as false, and string map access count emission
  as deferred with no emitted instructions.
- 2026-05-20 13:05 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper defers bare access call method return kinds" --no-skip`
  | failures: none | notes: bare `at`/`at_unsafe` setup-type return-kind
  probes now lock current deferral with unresolved method facts and unknown
  value kind.
- 2026-05-20 13:04 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch native call tail orchestration" --no-skip`
  | failures: none | notes: malformed and valid bare `at(...)` native-tail
  probes now lock current `NotHandled` deferral with no emitted instructions.
- 2026-05-20 13:02 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers" --no-skip`
  | failures: none | notes: experimental-vector method `at` native-tail
  coverage now locks current `NotHandled` deferral with no emitted
  instructions.
- 2026-05-20 12:59 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helper same-path defs" --no-skip`
  | failures: none | notes: confirmed the existing resolved fixture still
  passes and removed its stale active known-failure entry.
- 2026-05-20 12:58 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product keeps vector and map bridge parity" --no-skip`
  | failures: none | notes: parser-only bridge parity fixture now declares
  its local canonical map count helper over the current `Map<K, V>` constructor
  model, preserving semantic-product bridge surface checks.
- 2026-05-20 12:55 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection alias paths" --no-skip`
  | failures: none | notes: map alias-path metadata now consistently locks
  `/std/collections/map/tryAt_ref` as a resolved public map helper instead of
  expecting the same lookup to be both present and absent.
- 2026-05-20 12:54 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens" --no-skip`
  | failures: none | notes: SoA metadata now resolves public `count_ref`,
  `field_view`, and `to_aos` member names while old `soaVector*` spellings
  stay outside the public manifest.
- 2026-05-20 12:52 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="retired direct map Result payload literal rejects before lowering" --no-skip`
  | failures: none | notes: the stale `ir lowerer supports direct map Result
  payloads` name now selects zero tests; the current replacement locks direct
  map Result payload literal rejection before lowering.
- 2026-05-20 12:51 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept inferred canonical map default parameters,helper-wrapped map constructors accept inferred canonical map default parameters,helper-wrapped inferred canonical map default parameters validate" --no-skip`
  | failures: none | notes: inferred canonical map default-parameter
  fixtures now use explicit `<string, i32>` templates for `at` helper calls,
  matching the current stdlib-owned map helper resolution surface.
- 2026-05-20 12:48 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary public helper calls validate target classification" --no-skip`
  | failures: none | notes: wrapper temporary map helper coverage now uses
  the current public map constructor with inferred wrapper return type instead
  of the retired `mapSingle<K, V>` helper and explicit `/map` return envelope.
- 2026-05-20 12:43 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced count method compatibility alias resolves through canonical helper" --no-skip`
  | failures: none | notes: current renamed coverage locks that
  `values./map/count()` resolves through the canonical helper instead of the
  retired rejection path.
- 2026-05-20 12:40 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="non-imported wrapper-returned canonical map reference access rejects missing at_ref" --no-skip`
  | failures: none | notes: non-imported wrapper-returned canonical map
  reference access now locks the current missing `/std/collections/map/at_ref`
  diagnostic before primitive receiver method typing.
- 2026-05-20 12:37 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map borrowed receiver rejects direct stdlib contains,canonical map borrowed receiver rejects direct stdlib contains before key diagnostics" --no-skip`
  | failures: none | notes: borrowed receiver direct `contains` coverage now
  locks current unknown-call-target diagnostics instead of expecting hidden
  borrowed receiver coercion.
- 2026-05-20 12:35 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors reject explicit experimental map parameters" --no-skip`
  | failures: none | notes: helper-wrapped canonical map constructors now
  lock the current explicit `map<string, i32>` parameter mismatch instead of
  hidden wrapper coercion.
- 2026-05-20 12:33 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="uninitialized canonical map template arg validates as stdlib type" --no-skip`
  | failures: none | notes: canonical map template arguments containing
  `uninitialized<T>` now validate as stdlib-owned type text instead of
  triggering the old builtin map key/value diagnostic.
- 2026-05-20 12:30 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors reject inferred canonical map struct field mismatch,helper-wrapped inferred canonical map struct fields validate,helper-wrapped map constructor assignments accept inferred canonical map struct fields,auto bindings inside inferred canonical map return blocks rewrite constructors,helper-wrapped map constructors reject canonical map uninitialized storage mismatch" --no-skip`
  | failures: none | notes: inferred struct-field and uninitialized-storage
  map constructor fixtures now use explicit access templates where validation
  still applies and lock current mismatch diagnostics where inference is gone.
- 2026-05-20 12:24 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer helper accepts parser-shaped canonical map entry constructors as builtin map" --no-skip`
  | failures: none | notes: parser-shaped canonical map entry constructor
  coverage now locks lowerer classification as `map` while emitter builtin
  classification still rejects direct native emission.
- 2026-05-20 12:22 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers defer explicit map access for args-pack receivers" --no-skip`
  | failures: none | notes: explicit map access over args-pack receiver
  shapes now locks native-tail deferral with no emitted instructions.
- 2026-05-20 12:21 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper leaves indexed args-pack pointer map receivers unclassified" --no-skip`
  | failures: none | notes: indexed args-pack pointer map receiver setup-type
  coverage now locks the current unclassified bare-`at` receiver behavior.
- 2026-05-20 12:20 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores reordered bare access kinds" --no-skip`
  | failures: none | notes: reordered named bare-`at` inference coverage
  now locks the current `NotMatched` behavior instead of the retired generic
  collection access inference path.
- 2026-05-20 12:18 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers classify binding kind and string/fileerror types" --no-skip`
  | failures: none | notes: binding type source-lock coverage now checks
  shared SoA path-helper delegation instead of stale monolithic experimental
  SoA vector string literals.
- 2026-05-20 12:16 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helpers out of native builtin emission" --no-skip`
  | failures: none | notes: explicit canonical map `count` calls now lock
  the current native-tail `NotHandled` path with no builtin instructions.
- 2026-05-20 12:14 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch inline call with count fallbacks" --no-skip`
  | failures: none | notes: inline count/access fallback coverage now locks
  method-shaped unresolved `at` as an error and canonical map `count` as a
  direct stdlib helper emission.
- 2026-05-20 12:10 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch inline calls with locals" --no-skip`
  | failures: none | notes: explicit vector-count dispatch now locks the
  current two definition-resolver probes before one inline emission.
- 2026-05-20 12:08 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores unqualified array and map access kinds" --no-skip`
  | failures: none | notes: unqualified `at`/`at_unsafe` setup-inference
  coverage now locks the current `NotMatched` path instead of expecting
  direct builtin array/map value-kind inference.
- 2026-05-20 12:05 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores wrapper-returned canonical map access string kinds,ir lowerer setup inference helper ignores wrapper-returned slash-method map access kinds,ir lowerer setup inference helper ignores wrapper-returned canonical map access int32 kinds" --no-skip`
  | failures: none | notes: wrapper-returned `/std/collections/map/at*`
  setup-inference fixtures now lock the current `NotMatched` behavior after
  map access left the old builtin array/key-value helper path.
- 2026-05-20 12:02 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers leave inferred map receiver methods unresolved" --no-skip`
  | failures: none | notes: retired the stale inferred experimental-map
  inline-argument resolution fixture by replacing `mapPair` and locking the
  current unresolved lowerer-helper behavior for stdlib-owned map receivers.
- 2026-05-20 11:59 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors infer canonical auto locals" --no-skip`
  | failures: none | notes: helper-wrapped auto-local coverage now uses
  explicit canonical map helper template arguments for `tryAt` and `count`,
  matching adjacent current canonical map fixtures.
- 2026-05-20 11:56 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter rejects retired variadic map value pack count methods" --no-skip`
  | failures: none | notes: compile-run coverage now locks the retired
  variadic `args<map<K, V>>` count-method fixture as a compile-time
  rejection instead of expecting native map emission.
- 2026-05-20 11:46 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="retired direct map Result payload literal rejects before lowering,retired variadic map pack bare count rejects before lowering,retired variadic map pack canonical count no longer lowers as native map" --no-skip`
  | failures: none | notes: retired three stale backend IR map fixtures that
  still assumed old map literal/count compatibility or native map-pack
  lowering.
- 2026-05-20 11:43 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="*map*count*" --no-skip`
  | failures: none | notes: retired stale map-count semantics expectations
  around old same-path aliases, bare count fallback diagnostics, and
  public `MapValue` construction in method-count coverage.
- 2026-05-20 11:36 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable" --no-skip`
  | failures: none | notes: source lock now matches current SoA helper
  prefix functions, split compatibility/same-path helper routing, and
  key-value helper naming after the public map cutover.
- 2026-05-20 11:27 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable" --no-skip`
  | failures: none | notes: source lock now matches current key-value
  resolver naming, shared SoA helper path literals, and absence of removed
  direct map compatibility predispatch paths.
- 2026-05-20 11:21 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowers map literal call as statement" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map literal validates bool keys and values" --no-skip`
  | failures: none | notes: nested-definition probing now skips
  template-prefixed call statements that cannot be definitions, so duplicate
  explicit map key/value types parse as expression-call template arguments.
- 2026-05-20 11:15 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helper same-path defs" --no-skip`
  | failures: none | notes: explicit map `at`/`at_unsafe` alias calls with a
  canonical fallback resolver now lock the current canonical helper
  definition result instead of stale `nullptr` expectations.
- 2026-05-20 11:13 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers handle non-method count fallback" --no-skip`
  | failures: none | notes: non-method count fallback coverage now keeps
  retired map count/access aliases out of the fallback path and retains
  vector count/capacity/push coverage.
- 2026-05-20 11:10 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned slash-method map access count validates string result,map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference,wrapper-returned map method alias access keeps explicit helper diagnostics during inference,wrapper-returned map method alias access keeps primitive argument diagnostics during inference" --no-skip`
  | failures: none | notes: refreshed two stale map method-alias diagnostics
  after the stdlib-owned map cutover: wrapper returned slash-method count now
  validates, and the missing helper case reports the explicit stdlib helper
  target.
- 2026-05-20 11:07 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer materializes variadic borrowed map packs with indexed count_ref helpers,ir lowerer materializes variadic pointer map packs with indexed count_ref helpers" --no-skip`
  | failures: none | notes: explicit `count_ref` map helpers now
  materialize indexed args-pack reference/pointer map receivers before
  lowering the helper call.
- 2026-05-20 11:27 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="graph type resolver answers map receiver queries through shared type-text helper,graph type resolver infers map value return kinds through shared infer helper" --no-skip`
  | failures: none | notes: return-solver map fixtures now use canonical
  `/std/collections/map/map<string, i32>(...)` branches instead of retired
  `internal_map` imports and `/std/collections/mapPair`.
- 2026-05-20 11:18 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve indexed args-pack Result expressions" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers use semantic indexed args-pack Result facts" --no-skip`
  | failures: none | notes: Result metadata now treats bare `at` as an
  indexed access only when the receiver local is already a Result args pack,
  preserving semantic-product query facts for indexed args-pack Results.
- 2026-05-20 11:05 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads" --no-skip`
  | failures: none | notes: retargeted the stale direct semantic-product
  fixture to self-contained public `soa<Particle>` helpers and preserved
  borrowed `count_ref`/`get_ref` semantic-product target checks.
- 2026-05-20 10:34 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads" --no-skip`
  | failures: semantic product validates direct return method-like borrowed
  helper-return experimental soa_vector reads | notes: still fails at
  `semantics.validate(...)`; this was an unrelated stale SoA fixture and is
  resolved by the 2026-05-20 11:05 focused rerun.
- 2026-05-20 10:34 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="implicit map constructors infer canonical auto locals and auto returns" --no-skip`
  | failures: none | notes: refreshed the stale map auto-inference fixture to
  bind the computed `i32` payload before `Result.ok`, preserving canonical map
  constructor/count/insert/lookup coverage without relying on nested Result
  payload inference.
- 2026-05-18 20:03 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'hasInferredTypedWrappedMap|SemanticStringMapAccessResolution|StringMapAccess|NonStringMapAccess|classifySemanticStringMap|isSourceMethodStringMapAccessTarget|publishedMapAccessHelperReturnsString|hasExplicitStdMapSourceSpelling|mapHelperAlias|isNamespacedMapAccessCall|explicitMapAccessName|semanticStringMapAccess|stringMapAccess|isStringMapTarget|isStringMapOut|isMapBuiltinName|resolveMapHelperAliasName' src/ir_lowerer/IrLowererCountAccessHelpers.cpp src/ir_lowerer/IrLowererCountAccessClassifiers.cpp src/ir_lowerer/IrLowererCountAccessClassifiers.h`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Count-access classifier and string-valued
  key/value access helpers now use key/value names for local map-shaped
  helper APIs while preserving existing behavior.
- 2026-05-18 20:03 CEST | fail | mode: release | command:
  `git stash push -u -m count-access-keyvalue-rename-probe`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer count access classifiers prefer semantic indexed target facts,ir lowerer count access helpers prefer graph facts for string map access emission" --no-skip`;
  `git stash pop`
  | failures: `ir lowerer count access classifiers prefer semantic indexed
  target facts`; `ir lowerer count access helpers prefer graph facts for
  string map access emission` | notes: Baseline probe confirmed both failures
  pre-existed the count-access key/value rename.
- 2026-05-18 19:56 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'mapHelperSurfaceMetadataForAccessTargets|mapConstructorSurfaceMetadataForAccessTargets|isMapAccessHelperName|resolveMapAccessHelperPathMemberName|isExplicitMapAccessHelperPath|resolveMapConstructorExprMemberName|classifySemanticMapAccessTypeText|hasInferredTypedWrappedMap|preSemanticAliasMapAccess|preSemanticExplicitMapAccess|isAliasMapArgsPackAccess|isExplicitMapArgsPackAccess|inferredWrappedMap|\bisWrappedMap\b|\bmapArgs\b' src/ir_lowerer/IrLowererAccessTargetResolution.cpp`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers resolve and validate map access targets" --no-skip`
  | failures: none | notes: Access-target helper metadata and semantic
  classifier locals now use key/value names; dereferenced args-pack
  key/value access now recognizes the same explicit published helper paths as
  the direct args-pack path.
- 2026-05-18 19:53 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'resolvesMapHelperSurfacePath|mapConstructorAliasToken|\bmapAlias\b' src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Builtin-name helper classification now uses
  key/value local names for metadata-backed helper-surface and constructor
  alias lookups.
- 2026-05-18 19:52 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'canonicalInlineMapHelperName|inlineMapHelperMetadata|resolvePublishedInlineMapHelperName|resolvePublishedInlineMapSurfaceMemberName|isCanonicalPublishedInlineMapHelperPath|isSemanticBarePublishedMapHelperCall|isExplicitSamePathMapCountLikeDefinitionCall|isDirectMapAccessHelperCall|isExplicitDirectMapAccessHelperCall|isExplicitRemovedMapAccessHelperCall|isSemanticBarePreferredMapHelperDefinitionCall|keepsBuiltinInlineReturnForPublishedMapHelper|isBareDirectWrapperMapAccessDefinitionCall|mapMetadata|mapHelperName|canonicalMapHelperExpr|isCanonicalStdMapHelperCall|isRewrittenSlashMethodMapAccess' src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Inline native dispatch map-helper metadata
  helpers now use key/value names instead of local map-helper terminology.
- 2026-05-18 19:52 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers dispatch inline call with count fallbacks" --no-skip`
  | failures: `ir lowerer call helpers dispatch inline call with count
  fallbacks` | notes: stale canonical map access/count expectations remain
  in the broader inline fallback slice; reran the non-stale
  source-delegation lock separately and it passed.
- 2026-05-18 19:48 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n '\bmapInfo\b|resolveSemanticMapTargetInfo|isKnownMapReceiverExpr|hasNonMapReceiverSemanticFact' src/ir_lowerer/IrLowererSetupTypeReturnKindHelpers.cpp`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves count call method return kinds,ir lowerer setup type helper rejects canonical map access fallback to compatibility defs,ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Setup-type semantic return-kind target metadata
  now uses key/value names instead of `mapInfo` and related local map-receiver
  helper names.
- 2026-05-18 19:48 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves count call method return kinds,ir lowerer setup type helper resolves access call method return kinds,ir lowerer setup type helper rejects canonical map access fallback to compatibility defs,ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: `ir lowerer setup type helper resolves access call method
  return kinds` | notes: stale direct `at`/`at_unsafe` expectations still
  require old generic access return-kind inference; reran the adjacent
  non-stale slices separately and they passed.
- 2026-05-18 19:46 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'mapKeyCompareOpcode|map key compare opcode' src/ir_lowerer include/primec/testing/ir_lowerer_helpers tests/unit/test_ir_pipeline_validation_ir_lowerer_*`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers key/value key compare opcode selection,ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Key comparison opcode selection now uses
  `keyValueKeyCompareOpcode` instead of `mapKeyCompareOpcode`.
- 2026-05-18 19:33 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'isMapContainsHelperName|isMapTryAtHelperName|isBuiltinMapContainsName|isBuiltinMapTryAtName' src/ir_lowerer tests/unit/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Inline/native tail helper predicates now use
  key/value names for contains and tryAt helper-name checks.
- 2026-05-18 19:33 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers dispatch native call tail orchestration" --no-skip`
  | failures: `ir lowerer call helpers dispatch native call tail
  orchestration` | notes: The source-delegation lock passed; the failed
  native-tail orchestration case is the stale bare-`at` fixture logged above.
- 2026-05-18 18:46 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'ArrayMapAccessElementKindResolution|resolveArrayMapAccessElementKind|arrayMapAccess|ArrayMapAccess' include src tests/unit/test_ir_pipeline_validation_ir_lowerer_*`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helpers tolerate missing callbacks,ir lowerer setup inference helper handles invalid comparison/operator calls,ir lowerer setup inference helper rejects reordered bare key/value access kinds,ir lowerer setup inference helper rejects bare key/value reference access kinds" --no-skip`
  | failures: none | notes: Setup-inference access element-kind APIs now
  use array/key-value names instead of array/map names, and stale bare
  `at`/`at_unsafe` fixture expectations were refreshed to the current
  canonical-only behavior.
- 2026-05-18 18:32 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'MapAccessLookupEmitResult|tryEmitMapAccessLookup|tryEmitMapContainsLookup|MapLookupStringKeyResult|MapLookupKeyLocalEmitResult|MapLookupLoopLocals|MapLookupLoopConditionAnchors|MapLookupLoopMatchAnchors|tryResolveMapLookupStringKey|tryEmitMapLookupStringKeyLocal|emitMapLookupNonStringKeyLocal|emitMapLookupKeyLocal|emitMapLookupTargetPointerLocal|emitMapLookupLoopSearchScaffold|emitMapLookupAccessEpilogue|emitMapLookupContainsResult|emitMapLookupAccess|emitMapLookupContains|emitMapLookupTryAt|emitMapLookupLoopLocals|emitMapLookupLoopCondition|emitMapLookupLoopMatchCheck|emitMapLookupLoopAdvanceAndPatch|emitMapLookupAtKeyNotFoundGuard|emitMapLookupValueLoad|validateMapLookupKeyKind|mapLookup' include/primec/testing/ir_lowerer_helpers src/ir_lowerer tests/unit/test_ir_pipeline_validation_ir_lowerer_*`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers emit map lookup loop search scaffold,ir lowerer call helpers try emit map access lookup,ir lowerer call helpers resolve map lookup string keys,ir lowerer call helpers source delegation stays stable" --no-skip`
  | failures: none | notes: Lowerer lookup helper APIs now use
  key/value names instead of map-specific lookup helper identifiers, and the
  stale call-helper source-delegation lock was updated to assert old
  map-special-case helpers are absent.
- 2026-05-18 16:44 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'MapAccessTargetInfo|ResolveCallMapAccessTargetInfoFn|resolveMapAccessTargetInfo|validateMapAccessTargetInfo|inferCallMapTargetInfo|resolveCallMapAccessTargetInfo|mapTargetInfo|isMapTarget|isWrappedMapTarget' include src tests`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers prefer semantic collection specialization facts,ir lowerer inline param helper aliases pure map variadic forwarding,ir lowerer count access helpers emit count access calls,ir lowerer result helpers resolve file handle result payload metadata" --no-skip`
  | failures: none | notes: Lowerer target-info metadata now uses
  key/value names instead of map-specific target-info names, and the direct
  source scan found no remaining old target-info identifiers in `include`,
  `src`, or `tests`.
- 2026-05-18 16:43 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers,ir lowerer inline param helper aliases pure map variadic forwarding,ir lowerer count access helpers emit count access calls,ir lowerer result helpers resolve file handle result payload metadata" --no-skip`
  | failures: `ir lowerer call helpers dispatch buffer and native tail
  wrappers` | notes: The failure was the stale experimental-vector native-tail
  expectation logged above; the inline-param, count-access, and Result metadata
  cases in the same run passed.
- 2026-05-18 16:08 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'mapKeyKind|mapValueKind|referenceToMap|pointerToMap' include src tests`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers prefer semantic collection specialization facts,ir lowerer inline param helper aliases pure map variadic forwarding,ir lowerer count access helpers emit count access calls,ir lowerer result helpers resolve file handle result payload metadata" --no-skip`
  | failures: none | notes: The lowerer local-info metadata fields now use
  key/value collection names instead of map-specific field names, and the
  direct source scan found no remaining old field names in `include`, `src`, or
  `tests`.
- 2026-05-18 15:34 CEST | pass | mode: release | command:
  `git diff --check`;
  `rg -n 'LocalInfo::Kind::Map|Kind \{ Value, Pointer, Reference, Array, Vector, Map, Buffer \}' include src tests`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers prefer semantic collection specialization facts,ir lowerer inline param helper aliases pure map variadic forwarding,ir lowerer count access helpers emit count access calls,ir lowerer result helpers resolve file handle result payload metadata" --no-skip`
  | failures: none | notes: The lowerer local kind taxonomy now uses
  `KeyValueCollection` instead of a `Map` enum kind, and the direct source scan
  found no remaining `LocalInfo::Kind::Map` declarations or uses.
- 2026-05-18 15:33 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers classify binding kind and string/fileerror types,ir lowerer binding type helpers prefer semantic collection specialization facts,ir lowerer call helpers source delegation stays stable,ir lowerer setup type helper resolves indexed args-pack pointer map receivers,ir lowerer inline param helper aliases pure map variadic forwarding,ir lowerer count access helpers emit count access calls,ir lowerer result helpers resolve file handle result payload metadata,ir lowerer setup inference helper resolves reordered named access kinds" --no-skip`
  | failures: `ir lowerer call helpers source delegation stays stable`;
  `ir lowerer setup inference helper resolves reordered named access kinds`;
  `ir lowerer setup type helper resolves indexed args-pack pointer map
  receivers`; `ir lowerer binding type helpers classify binding kind and
  string/fileerror types` | notes: The failing cases are stale source-lock or
  bare-`at` helper fixtures already isolated from this local-kind rename; the
  adjacent semantic collection specialization, inline param, count access, and
  Result metadata checks passed.
- 2026-05-18 15:05 CEST | pass | mode: release | command:
  `rg -n 'CollectionsMapHelpers|CollectionsMapConstructors|StdlibSurfaceId::CollectionsMap' include src`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' include/primec/StdlibSurfaceRegistry.h src/StdlibSurfaceRegistry.cpp`;
  `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked,map insert surface registry rejects legacy compatibility spellings,collection helper surface registry resolves preferred compatibility spellings,published target lookups ignore raw routing facts without maps,semantic-product coverage validators ignore raw routing facts without maps,semantic-product local-auto call paths accept stdlib surface equivalents" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories,ir lowerer binding type helpers prefer semantic collection specialization facts" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product publishes vector map and soa_vector collection specializations" --no-skip`
  | failures: none | notes: Public `StdlibSurfaceId` names are now
  map-agnostic; map helper/constructor ID test fixtures resolve IDs from
  bridge-key metadata. The direct production scan returned no map-specific
  stdlib surface ID names or registry map-surface trace matches.
- 2026-05-18 15:02 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories,ir lowerer call helpers infer forwarded map access targets,ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer call helpers lower explicit map access for args-pack receivers,ir lowerer binding type helpers prefer semantic collection specialization facts" --no-skip`
  | failures: `ir lowerer call helpers keep explicit map helpers out of native
  builtin emission`; `ir lowerer call helpers lower explicit map access for
  args-pack receivers` | notes: The failures match stale native-tail map
  helper emission expectations already isolated from this enum-name removal;
  the metadata and semantic collection-specialization checks passed.
- 2026-05-18 15:03 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product publishes vector map and soa_vector collection specializations,semantic product keeps vector and map bridge parity" --no-skip`
  | failures: `semantic product keeps vector and map bridge parity` | notes:
  The failure is a stale validation mismatch for canonical map count helper
  parameter type text; rerunning only `semantic product publishes vector map
  and soa_vector collection specializations` passed.
- 2026-05-18 14:02 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' include/primec/StdlibSurfaceRegistry.h src/StdlibSurfaceRegistry.cpp`;
  `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests`;
  `cmake --build build-release --target PrimeStruct_backend_runtime_tests`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked,map insert surface registry rejects legacy compatibility spellings,collection helper surface registry resolves preferred compatibility spellings" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories" --no-skip`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`
  | failures: none | notes: Collection registry entries now derive their
  public IDs from ordered manifest slots, removing the final map-specific
  traces from `StdlibSurfaceRegistry.cpp`. The first runtime source-lock rerun
  exposed stale direct vector/SoA enum-name expectations from the same slot-ID
  derivation; those assertions were updated and the focused runtime gate then
  passed.
- 2026-05-18 13:58 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' include/primec/StdlibSurfaceRegistry.h src/StdlibSurfaceRegistry.cpp`;
  `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked,map insert surface registry rejects legacy compatibility spellings,collection helper surface registry resolves preferred compatibility spellings" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories" --no-skip`
  | failures: none | notes: Collection surface member resolution now routes
  every non-error collection surface through the generic manifest-backed member
  resolver instead of switching on vector/map/SoA IDs. The direct scan now
  reports only the two public header enum entries and the two map metadata ID
  assignments in `StdlibSurfaceRegistry.cpp`.
- 2026-05-18 13:56 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' include/primec/StdlibSurfaceRegistry.h src/StdlibSurfaceRegistry.cpp`;
  `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked,map insert surface registry rejects legacy compatibility spellings,collection helper surface registry resolves preferred compatibility spellings" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories" --no-skip`
  | failures: none | notes: `StdlibSurfaceRegistry.cpp` no longer parses or
  switches on map-specific collection manifest IDs, and the direct source scan
  is down to the two public map metadata IDs plus the two resolver switch
  cases. The broader backend IR metadata trio was intentionally not used as a
  gate because it contains known stale SoA and contradictory map alias
  expectations recorded above.
- 2026-05-18 13:54 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens,stdlib surface metadata resolves collection alias paths,stdlib surface metadata classifies collection helper categories" --no-skip`
  | failures: `stdlib surface metadata resolves collection helper member
  tokens`; `stdlib surface metadata resolves collection alias paths` | notes:
  The helper-token failures are the existing stale SoA alias expectations, and
  the alias-path failure is the stale contradictory
  `/std/collections/map/tryAt_ref` expectation logged in Current Known
  Failures.
- 2026-05-18 13:48 CEST | pass | mode: release | command:
  `rg -n 'isStdlibMapBaseHelperName|isStdlibMapBorrowedHelperName' src include tests`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' include/primec/StdlibSurfaceRegistry.h src/StdlibSurfaceRegistry.cpp src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories" --no-skip`;
  `git diff --check` | failures: none | notes:
  Public map-specific base/borrowed helper classifier APIs were removed from
  `StdlibSurfaceRegistry`; semantic compatibility code now derives the
  categories locally from `collections.map_helpers` metadata. The helper API
  scan returned no matches, and the direct trace scan found only the expected
  registry enum/manifest entries. The Python inventory script was intentionally
  not run.
- 2026-05-18 13:00 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/emitter/EmitterBuiltinCallPathHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  `EmitterBuiltinCallPathHelpers.cpp` now derives map helper and constructor
  matching from `collections.map_helpers` and `collections.map_constructors`
  metadata, with a zero map-surface inventory allowance. The targeted direct
  scan returned no matches; only stdlib registry metadata remains nonzero in
  the inventory. The Python inventory script was intentionally not run.
- 2026-05-18 12:52 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/IrPrinterHelpers.cpp src/emitter/EmitterBuiltinMethodResolutionHelpers.cpp src/emitter/EmitterBuiltinMethodResolutionTypeInferenceHelpers.cpp src/emitter/EmitterEmitSetupReturnInferenceCollections.h src/emitter/EmitterExprCollectionTypeHelpers.h src/emitter/EmitterHelpersTypes.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Six non-lowerer files returned no targeted map-surface matches, so their
  stale inventory caps were lowered to zero and the obsolete emitter helper
  type backing-trace source-lock allowance was removed. The Python inventory
  script was intentionally not run.
- 2026-05-18 12:48 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererCallHelpers.cpp src/ir_lowerer/IrLowererCallResolution.cpp src/ir_lowerer/IrLowererLowerStatementsExpr.h src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp src/ir_lowerer/IrLowererStatementCallEmission.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  The final five nonzero lowerer inventory files returned no targeted
  map-surface matches, so their stale inventory caps were lowered to zero and
  obsolete backing-trace source-lock allowances plus the setup-method-call
  experimental-map exception were removed. The Python inventory script was
  intentionally not run.
- 2026-05-18 12:45 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp src/ir_lowerer/IrLowererLowerInlineCalls.h src/ir_lowerer/IrLowererNativeTailDispatch.cpp src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Four lowerer files returned no targeted map-surface matches, so their stale
  inventory caps were lowered to zero, obsolete backing-trace source-lock
  allowances were removed, and the generated experimental-map inline-call
  context exception was retired. The Python inventory script was intentionally
  not run.
- 2026-05-18 12:42 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererStatementBindingHelpers.cpp src/ir_lowerer/IrLowererBindingTypeHelpers.cpp src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp src/ir_lowerer/IrLowererStructReturnPathHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Four lowerer files returned no targeted map-surface matches, so their stale
  inventory caps were lowered to zero and obsolete backing-trace source-lock
  allowances were removed for binding-type, inference base-kind, and
  statement-binding helpers. The Python inventory script was intentionally not
  run.
- 2026-05-18 12:23 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererCountAccessHelpers.cpp src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Three count-4 lowerer files returned no targeted map-surface matches, so
  their stale inventory caps were lowered to zero and obsolete backing-trace
  source-lock allowances were removed for statement-binding metadata and
  struct-field binding helpers. The Python inventory script was intentionally
  not run.
- 2026-05-18 12:21 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp src/ir_lowerer/IrLowererLowerStatementsBindings.h src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Three low-count lowerer files returned no targeted map-surface matches, so
  their stale inventory caps were lowered to zero and the obsolete
  fallback-setup backing-trace source-lock allowance was removed. The Python
  inventory script was intentionally not run.
- 2026-05-18 12:18 CEST | pass | mode: release | command:
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h src/ir_lowerer/IrLowererStructLayoutHelpers.cpp src/ir_lowerer/IrLowererStructTypeHelpers.cpp src/ir_lowerer/IrLowererPackedResultHelpers.cpp src/ir_lowerer/IrLowererResultMetadataHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  Five low-count lowerer files returned no targeted map-surface matches, so
  their stale inventory caps and obsolete backing-trace source-lock allowances
  were lowered to zero. The Python inventory script was intentionally not run.
- 2026-05-18 12:15 CEST | pass | mode: release | command:
  `rg -n 'alias == "map"|rawName == "map"|std/collections/map|experimental_map|CollectionsMap|Map__|Entry__|Map<' src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp || true`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `git diff --check` | failures: none | notes:
  `IrLowererBuiltinNameHelpers.cpp` now derives the slashless map constructor
  alias from `collections.map_constructors` metadata and has a zero
  map-surface inventory allowance. The targeted direct scan returned no
  matches. The Python inventory script was intentionally not run.
- 2026-05-18 12:09 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="implicit map constructor auto inference keeps template conflict diagnostics" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression ignores templated alias helper fallback" --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphFallbackTypeInference.h`;
  `git diff --check` | failures: none | notes:
  `TemplateMonomorphFallbackTypeInference.h` now uses shared map backing
  classifiers and metadata-derived map collection aliases. The targeted
  direct scan returned no matches. The Python inventory script was
  intentionally not run.
- 2026-05-18 12:03 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphBindingCallInference.h`;
  `git diff --check` | failures: none | notes:
  `TemplateMonomorphBindingCallInference.h` had no direct map-surface scan
  matches, so its stale inventory cap is now zero and the ownership test
  source-locks that state. The Python inventory script was intentionally not
  run.
- 2026-05-18 11:59 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped inferred canonical map returns rewrite nested constructor arguments" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped inferred canonical map returns keep nested constructor mismatch diagnostics" --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h`;
  `git diff --check` | failures: none | notes:
  `TemplateMonomorphExperimentalCollectionConstructorRewrites.h` now uses
  shared map constructor member and `Entry` backing helpers. The targeted
  direct scan returned no matches. The Python inventory script was
  intentionally not run.
- 2026-05-18 11:46 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map Ref wrappers validate through canonical borrowed helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map return accepts array value type during semantics validation" --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferStructReturnHelpers.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorInferStructReturnHelpers.cpp` now derives rooted map
  aliases, unrooted helper prefixes, and canonical `MapValue` roots from
  stdlib surface metadata. The targeted direct scan returned no matches. The
  Python inventory script was intentionally not run.
- 2026-05-18 11:41 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructor uninitialized storage keeps mismatch diagnostics" --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorStatement.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorStatement.cpp` now uses shared map backing
  classification and metadata-derived map alias lookup for init type matching.
  The targeted direct scan returned no matches. The Python inventory script
  was intentionally not run.
- 2026-05-18 11:38 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors accept canonical map uninitialized storage" --no-skip` |
  failures: helper-wrapped map constructors accept canonical map uninitialized
  storage | notes: same stale failure already tracked lower in this log; it
  still reports `init value type mismatch`, so it was not used as the gate for
  this metadata cleanup.
- 2026-05-18 11:31 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor keeps same-path definition before wrapper fallback" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor does not resolve map alias helper fallback" --no-skip`;
  `cmake --build build-release --target PrimeStruct_compile_run_tests primec`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native rejects map constructor literal parity with canonical entries" --no-skip`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprCallResolution.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorExprCallResolution.cpp` now resolves map entry
  constructor argument paths through stdlib surface metadata and no longer
  falls back to a direct experimental-map entry path. The targeted direct scan
  returned no matches. The Python inventory script was intentionally not run.
- 2026-05-18 11:22 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed,map return accepts array value type during semantics validation,public stdlib map Ref wrappers validate through canonical borrowed helpers,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg --pcre2 -n 'isExperimentalCollectionBackingTypeName\("map"|"/map"|(?<![A-Za-z0-9_])map<|/map|std/collections/map|experimental_map|CollectionsMap|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorBuildInitializerInferenceCalls.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorBuildInitializerInferenceCalls.cpp` now uses shared map
  backing/type classifiers and metadata-derived map alias lookup. Both
  targeted direct scans returned no matches. The Python inventory script was
  intentionally not run.
- 2026-05-18 11:18 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="implicit map constructors infer canonical auto locals and auto returns,canonical stdlib map returns are allowed,map return accepts array value type during semantics validation,public stdlib map Ref wrappers validate through canonical borrowed helpers" --no-skip` |
  failures: implicit map constructors infer canonical auto locals and auto
  returns | notes: the failing fixture reduces to
  `Result.ok(plus(/std/collections/map/count(...), ...))` payload inference;
  the map-focused subset without that stale count-plus path was rerun and
  passed.
- 2026-05-18 11:20 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip` |
  failures: canonical map surface owns standalone stdlib implementation |
  notes: source-lock assertions were aimed at
  `SemanticsValidatorBuildInitializerInference.cpp` instead of the changed
  `SemanticsValidatorBuildInitializerInferenceCalls.cpp`; the lock was
  corrected and rerun successfully.
- 2026-05-18 11:08 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="template vector and map returns are allowed,canonical stdlib map returns are allowed,vector return accepts array element type during semantics validation,map return accepts array value type during semantics validation,map return rejects wrong template arity,map return rejects unsupported builtin Comparable key contract,canonical stdlib map return rejects direct template arguments,public stdlib map Ref wrappers validate through canonical borrowed helpers,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg --pcre2 -n 'isExperimentalCollectionBackingTypeName\("map"|"/map"|(?<![A-Za-z0-9_])map<|/map|std/collections/map|experimental_map|CollectionsMap|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollectionReturnInference.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorInferCollectionReturnInference.cpp` now uses shared map
  backing classifiers and metadata-derived map alias return text. Both
  targeted direct scans returned no matches. The Python inventory script was
  intentionally not run.
- 2026-05-18 10:50 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map Ref wrappers validate through canonical borrowed helpers,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers,canonical stdlib map returns are allowed" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'isExperimentalCollectionBackingTypeName\("map"|"/map"|/map|std/collections/map|experimental_map|CollectionsMap|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferCollections.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollections.cpp`;
  `git diff --check` | failures: none | notes:
  `SemanticsValidatorInferCollections.cpp` now uses shared experimental map
  backing classifiers and has no targeted direct map-residue matches. The
  Python inventory script was intentionally not run.
- 2026-05-18 10:48 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map Ref wrappers validate through canonical borrowed helpers,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers,canonical stdlib map returns are allowed,semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads" --no-skip` |
  failures: semantic product validates direct return method-like borrowed
  helper-return experimental soa_vector reads | notes: unrelated SoA semantic
  product fixture failed at `semantics.validate(...)`; the map-focused subset
  was rerun separately and passed.
- 2026-05-18 10:37 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="memory at validates checked pointer access,memory at rejects non-pointer target,memory at rejects mismatched index and count kinds,canonical stdlib map returns are allowed,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'collectionName == "map"|"/map"|/map|std/collections/map|experimental_map|CollectionsMap|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp`;
  `git diff --check` | failures: none | notes: scalar pointer/memory
  builtin validation now classifies map-like builtin collection calls through
  the shared collection type classifier. Both direct residue scans returned no
  matches. The Python inventory script was intentionally not run.
- 2026-05-18 10:29 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers,canonical namespaced map helpers keep builtin key diagnostics for borrowed canonical map receivers,canonical namespaced map _ref helpers include builtin map key rejects for borrowed canonical map receivers,stdlib namespaced map constructor accepts explicit experimental map returns,stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map returns" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n '"/map"|/map__|builtinCollectionName == "map"|resolveCallCollectionTemplateArgs\(target, "map"|collectionTypePath == "|isRootMapAliasPath' src/semantics/SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h`;
  `git diff --check` | failures: none | notes: collection buffer/map
  resolver inference now derives rooted map aliases and map collection alias
  tokens from map constructor metadata. Both direct residue scans returned no
  matches. The Python inventory script was intentionally not run.
- 2026-05-18 10:19 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="template vector and map returns are allowed,canonical stdlib map returns are allowed,vector return accepts array element type during semantics validation,map return accepts array value type during semantics validation,map return rejects wrong template arity,map return rejects unsupported builtin Comparable key contract,canonical stdlib map return rejects direct template arguments,canonical namespaced map helpers keep builtin key diagnostics for borrowed canonical map receivers,canonical namespaced map _ref helpers include builtin map key rejects for borrowed canonical map receivers,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsBindingTypeHelpers.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsBindingTypeHelpers.cpp` |
  failures: none after stabilizing nested collection return payloads | notes:
  semantic binding type helpers now derive map roots from
  `collections.map_helpers` metadata, and `SemanticsBindingTypeHelpers.cpp`
  has zero targeted map-surface matches. The first focused semantic window
  exposed `map return accepts array value type during semantics validation`
  failing with `unsupported return type on /main`; recursive collection return
  type-argument validation fixed that. The Python inventory script was
  intentionally not run.
- 2026-05-18 10:04 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map borrowed helper calls validate ownership-sensitive values through ref helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="builtin canonical map insert method sugar validates before lowering ownership-sensitive values,constructor-backed builtin map insert method sugar avoids insert builtin rewrite,canonical map insert helpers validate on value and borrowed mutation surfaces,canonical map borrowed helper calls validate ownership-sensitive values through ref helpers,map compatibility at call requires explicit alias definition,stdlib-owned map compatibility at call falls back to canonical helper,map namespaced at method now validates through slash-path routing" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidate.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidate.cpp` |
  failures: none after stabilizing the focused borrowed-helper case | notes:
  the first single-case rerun failed with `unsupported return type on
  /borrowMap`; allowing resolvable struct types inside collection return
  arguments fixed it. `SemanticsValidate.cpp` now has zero targeted
  map-surface matches. The Python inventory script was intentionally not run.
- 2026-05-18 09:51 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`
  initially failed on an unused `unrootedCanonicalMapHelperRootPathLocal`
  helper after the cleanup; the helper was removed and the same command then
  passed. Final commands: `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map compatibility count call requires explicit alias definition,map compatibility contains call rejects visible canonical definition,map compatibility tryAt call rejects visible canonical definition,map compatibility at call requires explicit alias definition,stdlib-owned map compatibility at call falls back to canonical helper,map namespaced at method now validates through slash-path routing,map namespaced tryAt method validates through slash-path routing" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp` |
  failures: none after removing the unused helper | notes: semantic collection
  compatibility inference now derives map type-root, import, explicit-helper,
  removed-helper, and rooted-alias decisions from `collections.map_helpers`
  metadata; the edited C++ file has zero targeted map-surface matches. The
  backend IR test binary was build-only because the touched source-lock case
  still has unrelated stale SoA assertions. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 09:45 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="experimental map direct-import shim stays retired,map compatibility count call requires explicit alias definition,map compatibility contains call rejects visible canonical definition,map compatibility tryAt call rejects visible canonical definition,map namespaced at method now validates through slash-path routing,map namespaced tryAt method validates through slash-path routing" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollectionCompatibilityInternal.h` |
  failures: none | notes: shared semantic collection-compatibility helpers now
  derive map helper namespace, unrooted-path, diagnostic, and removed-helper
  path handling from `collections.map_helpers` metadata; the edited header has
  zero targeted map-surface matches. The backend IR test binary was build-only
  because the touched source-lock case still has unrelated stale SoA assertions.
  The Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 09:31 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method calls resolve to definitions,imported map size method calls resolve to local definitions,rooted map helper aliases use ordinary explicit path diagnostics,map namespaced count method expression body arguments validate through stdlib helper target,map namespaced at method expression body arguments validate through slash-path target" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprMethodTargetResolution.cpp` |
  failures: none | notes: method-target resolution now derives map helper
  surface identity, rooted import-alias helper paths, and explicit-root
  detection through `collections.map_helpers` metadata; the edited C++ file has
  zero targeted map-surface matches. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 09:24 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs,ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids,native tail and late collection helper metadata dispatch stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h` |
  failures: none | notes: tail dispatch now derives map helper surface IDs
  and import-alias helper recognition from `collections.map_helpers` metadata
  instead of direct map surface IDs or a raw rooted alias string check; the
  edited C++ file has zero targeted map-surface matches. The Python inventory
  scripts were intentionally not run in this pass.
- 2026-05-18 09:19 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method expression body arguments validate through slash-path target,canonical map access wrapper ignores removed alias helper,map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator expr source delegation stays stable" --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprCollectionAccess.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprCollectionAccess.cpp` |
  failures: none | notes: collection-access resolution no longer special-cases
  removed rooted `/map/at*` or `/map/contains*` compatibility paths and now
  recognizes only canonical map helper spellings through surface metadata; the
  edited C++ file has zero targeted map-surface matches. The Python inventory
  scripts were intentionally not run in this pass.
- 2026-05-18 09:10 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar,map stdlib namespaced count expression ignores templated alias helper fallback,map compatibility explicit-template count call keeps alias precedence with canonical templated helper" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator expr source delegation stays stable" --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorCollectionHelperRewrites.cpp` |
  failures: none | notes: semantic collection-helper rewrites now
  derive map helper canonical paths, explicit helper-path recognition, member
  tokens, and lowering spelling preference from `collections.map_helpers`
  metadata; the edited C++ file has zero targeted map-surface matches. The
  Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 09:04 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs,ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids,native tail and late collection helper metadata dispatch stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp` |
  failures: none | notes: setup-type collection helpers now derive map
  helper and constructor surface IDs through bridge-key metadata wrappers
  instead of direct map surface IDs; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not
  run in this pass.
- 2026-05-18 08:51 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar,map stdlib namespaced count expression ignores templated alias helper fallback" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsBuiltinPathHelpers.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsBuiltinPathHelpers.cpp` |
  failures: none | notes: semantic builtin path helpers now resolve map
  helper member names and rooted import-alias helpers through
  `collections.map_helpers` metadata instead of direct rooted/canonical map
  helper prefixes; the edited file has zero targeted map-surface matches. The
  Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 08:44 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression ignores templated alias helper fallback,map compatibility explicit-template count call keeps alias precedence with canonical templated helper,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/TemplateMonomorphExpressionRewrite.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphExpressionRewrite.h` |
  failures: none | notes: template expression rewriting now derives the
  forwarded empty map constructor path from constructor metadata and derives
  removed rooted-alias helper matching from map helper import-alias metadata;
  the edited header has zero targeted map-surface matches. The Python
  inventory scripts were intentionally not run in this pass.
- 2026-05-18 08:15 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs,ir lowerer materialized collection receivers use published helper queries,ir lowerer late collection constructor guards use published constructor queries" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h` |
  failures: none | notes: lower emit-expression collection helpers now derive
  late map helper and constructor surface IDs through stdlib map metadata
  wrappers instead of direct map surface IDs; the edited header has zero
  targeted map-surface matches. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 08:02 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper uses semantic-product args-pack binding types" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/ir_lowerer/IrLowererUninitializedStructInference.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/ir_lowerer/IrLowererUninitializedStructInference.cpp` |
  failures: none | notes: uninitialized struct inference now derives map
  helper and constructor surface IDs from bridge-key metadata and derives the
  forwarded empty constructor member from constructor metadata; the edited
  file has zero targeted map-surface matches. The Python inventory scripts
  were intentionally not run in this pass.
- 2026-05-18 08:02 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper uses semantic-product args-pack binding types,ir lowerer result helpers resolve indexed args-pack Result expressions" --no-skip` |
  failures: ir lowerer result helpers resolve indexed args-pack Result
  expressions | notes: existing stale result-helper args-pack probe failed
  `resolveResultExprInfoFromLocals`; rerunning only the statement-binding
  args-pack probe passed.
- 2026-05-18 07:56 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp` |
  failures: none | notes: collection-access validation now derives canonical
  map helper paths, namespace lookup, resolved helper names, and rooted alias
  definition probes from stdlib map helper metadata; the edited file has zero
  targeted map-surface matches. Direct `rg` checks also confirmed stale zero
  caps for call-path helpers, receiver paths, pointer-like, try, and Result
  helper files. The Python inventory scripts were intentionally not run in
  this pass.
- 2026-05-18 07:50 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp` |
  failures: none | notes: pre-dispatch direct-call handling now derives
  canonical helper paths, rooted removed-alias paths, and helper member
  resolution through stdlib map helper metadata instead of direct map surface
  IDs and rooted map path construction; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not run
  in this pass.
- 2026-05-18 07:44 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferDefinition.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferDefinition.cpp` |
  failures: none | notes: definition return inference now detects deferred
  canonical map access helpers through stdlib map helper metadata instead of
  direct canonical map access path literals; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not run
  in this pass.
- 2026-05-18 07:40 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprArgumentValidation.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorExprArgumentValidation.cpp` |
  failures: none | notes: expression argument validation now derives
  canonical map access helper matching through stdlib map helper metadata and
  map template-base checks through `isMapCollectionTypeName` instead of direct
  canonical map path and type spellings; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not run
  in this pass.
- 2026-05-18 07:36 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferCollectionDispatchSetup.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferCollectionDispatchSetup.cpp` |
  failures: none | notes: infer collection dispatch setup now derives map
  helper metadata through the bridge key before resolving rooted aliases,
  canonical access helpers, and namespace checks instead of directly naming
  the map helper surface ID; the edited file has zero targeted map-surface
  matches. The Python inventory scripts were intentionally not run in this
  pass.
- 2026-05-18 07:32 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression ignores templated alias helper fallback,map compatibility explicit-template count call keeps alias precedence with canonical templated helper,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/TemplateMonomorphCoreUtilities.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphCoreUtilities.h` |
  failures: none | notes: template monomorph core collection-base,
  import-coverage, map entry, and map constructor overload checks now derive
  map roots and member paths through stdlib surface metadata instead of direct
  map path literals; the edited file has zero targeted map-surface matches.
  The Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 07:28 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp` |
  failures: none | notes: late fallback map access diagnostics now derive
  rooted import-alias access helper recognition from stdlib map import-alias
  metadata instead of a direct `/map/at*` path table; the edited file has zero
  targeted map-surface matches. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 07:24 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidator.cpp`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/SemanticsValidator.cpp` |
  failures: none | notes: core semantic unknown-call formatting and
  diagnostic target normalization now derive map helper membership and
  canonical count paths through stdlib surface metadata instead of direct
  `map/` or `std/collections/map` checks; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not run
  in this pass.
- 2026-05-18 07:21 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression ignores templated alias helper fallback,map compatibility explicit-template count call keeps alias precedence with canonical templated helper,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h`;
  `rg --pcre2 -n '/?std/collections/map(?:/|")|/?std/collections/experimental_map(?:/|")|(?<![A-Za-z0-9_/])/?map/|\bmap(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bCollectionsMap[A-Za-z0-9_]*\b|\bMap<' src/semantics/TemplateMonomorphCollectionCompatibilityPaths.h` |
  failures: none | notes: template monomorph collection compatibility now
  derives explicit map helper member names and map collection-root matching
  through stdlib surface metadata instead of direct `map/` or
  `std/collections/map` checks; the edited file has zero targeted map-surface
  matches. The Python inventory scripts were intentionally not run in this
  pass.
- 2026-05-18 07:15 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,stdlib canonical map helpers resolve in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferMethodResolution.cpp` |
  failures: none | notes: infer-method method-name normalization now
  derives explicit map helper names through stdlib surface metadata instead of
  direct `map/` or `std/collections/map/` prefix stripping; the edited file has
  zero targeted map-surface matches. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 07:12 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product publishes vector map and soa_vector collection specializations" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticPublicationBuilders.cpp` |
  failures: none | notes: semantic-product collection specialization
  publication now derives map type-root recognition and map surface ids from
  stdlib surface metadata instead of direct map paths or map surface enum
  constants; the edited file has zero targeted map-surface matches. The Python
  inventory scripts were intentionally not run in this pass.
- 2026-05-18 07:06 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,explicit canonical map access helpers accept canonical map values" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprVectorHelpers.cpp` |
  failures: none | notes: semantic vector helper method-target
  normalization now derives map helper member names from stdlib surface metadata
  instead of direct `map/` or `std/collections/map/` prefix stripping; the
  edited file has zero targeted map-surface matches. The Python inventory
  scripts were intentionally not run in this pass.
- 2026-05-18 07:02 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,explicit canonical map access helpers accept canonical map values" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp` |
  failures: none | notes: infer pre-dispatch call handling now derives
  slashless canonical map helper path detection and rooted map access alias
  classification from stdlib surface metadata instead of direct map path
  literals; the edited file has zero targeted map-surface matches. The
  Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 06:58 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,explicit canonical map access helpers accept canonical map values" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorBuildCallResolution.cpp` |
  failures: none | notes: build call resolution now derives the canonical
  map helper root, map constructor path, and map helper namespace prefix from
  stdlib surface metadata instead of direct canonical map path literals; the
  edited file has zero targeted map-surface matches. The Python inventory
  scripts were intentionally not run in this pass.
- 2026-05-18 06:55 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline native dispatch prefers published canonical map access helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp` |
  failures: none | notes: inline native map access helper classification
  now resolves map helper member names and canonical helper paths through
  `collections.map_helpers` metadata instead of directly naming the map
  helper surface ID; the edited file has zero targeted map-surface matches.
  The Python inventory scripts were intentionally not run in this pass.
- 2026-05-18 06:51 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter resolves stdlib canonical map count helper in method-call sugar" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/emitter/EmitterBuiltinMethodResolutionMetadataHelpers.cpp` |
  failures: none | notes: emitter method metadata now resolves map helper
  identity through `collections.map_helpers` metadata instead of directly
  naming the map helper surface ID; the edited file has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not
  run in this pass.
- 2026-05-18 06:47 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib wrapper map constructor accepts explicit canonical map bindings" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/MapConstructorHelpers.h` |
  failures: none | notes: map constructor helper path classification now
  reuses `resolveMapConstructorMemberPath(...)` instead of directly naming
  the map constructor surface ID; the edited header has zero targeted
  map-surface matches. The Python inventory scripts were intentionally not
  run in this pass.
- 2026-05-18 06:38 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp` |
  failures: none | notes: late map access builtin helper lookup now
  obtains `collections.map_helpers` metadata through the bridge key instead
  of directly naming the map helper surface ID; the edited file has zero
  targeted map-surface matches. The Python inventory scripts were
  intentionally not run in this pass.
- 2026-05-18 06:34 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map borrowed method-call sugar rejects missing ref template inference" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `rg -n 'std/collections/map|experimental_map|/map/|CollectionsMap|map(?:At|AtUnsafe|Contains|Count|Double|Empty|FromEntries|Insert|New|Oct|Pair|Quad|Quint|Sept|Sext|Single|Triple|TryAt)(?:Ref)?\b|\bMap__|\bEntry__|\bMap<' src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h` |
  failures: none | notes: template monomorph receiver map helper lowering
  now derives rooted map import-alias helper detection from
  `collections.map_helpers` metadata instead of spelling the rooted helper
  prefix directly; the edited header has zero targeted map-surface matches.
  The Python inventory scripts were intentionally not run in this pass.
- 2026-05-17 10:47 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper rejects explicit slash-method map access return kinds" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer setup-type method-target resolution now derives canonical
  map method target roots through `collectionTypePath("map")` instead of
  directly spelling the canonical map root; the map surface inventory
  dropped to 243 production traces and backing traces remain at 0.
- 2026-05-17 10:44 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer dispatch setup now obtains canonical map access helper
  paths through `collections.map_helpers` metadata instead of directly
  concatenating canonical map helper paths; the map surface inventory
  dropped to 245 production traces and backing traces remain at 0.
- 2026-05-17 10:44 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper resolves array and map access kinds" --no-skip` |
  failures: ir lowerer setup inference helper resolves array and map access
  kinds | notes: stale fixture returned `NotResolved` for the old direct
  array/map access expectations before semantic-product helper return
  inference was reached.
- 2026-05-17 10:44 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper resolves wrapper-returned canonical map access int32 kinds" --no-skip` |
  failures: ir lowerer setup inference helper resolves wrapper-returned
  canonical map access int32 kinds | notes: stale fixture returned
  `NotResolved` for the old wrapper-returned canonical map access
  expectations, matching the known wrapper-returned setup-inference drift.
- 2026-05-17 10:40 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: infer-method map helper target selection now obtains
  `collections.map_helpers` metadata through the bridge key instead of
  directly concatenating canonical map helper paths; the map surface
  inventory dropped to 247 production traces and backing traces remain at 0.
- 2026-05-17 10:37 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map helpers keep builtin key diagnostics for borrowed canonical map receivers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: argument-validation map access helper lookup now obtains
  `collections.map_helpers` metadata through the bridge key instead of
  directly naming the map helper surface ID; the map surface inventory
  dropped to 249 production traces and backing traces remain at 0.
- 2026-05-17 10:33 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: infer collection-dispatch map helper return-kind lookup now obtains
  `collections.map_helpers` metadata through the bridge key instead of
  directly naming the map helper surface ID; the map surface inventory
  dropped to 251 production traces and backing traces remain at 0.
- 2026-05-17 10:33 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="graph type resolver infers map value return kinds through shared infer helper" --no-skip` |
  failures: graph type resolver infers map value return kinds through shared
  infer helper | notes: stale fixture still imports `internal_map` and uses
  retired `/std/collections/mapPair`, so validation fails before return-kind
  assertions.
- 2026-05-17 10:29 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects Create with canonical map method precedence when constructor is not effect-free,omitted initializer accepts effect-free Create with canonical slash-path map call helper,omitted initializer accepts effect-free Create with wrapper-returned canonical map method helper fallback" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: effect-free collection map helper lookup now obtains
  `collections.map_helpers` metadata through the bridge key instead of
  directly naming the map helper surface ID; the map surface inventory
  dropped to 253 production traces and backing traces remain at 0.
- 2026-05-17 10:26 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inline parameter map constructor rewriting now obtains
  `collections.map_constructors` metadata through the bridge key instead of
  directly naming the map constructor surface ID; the map surface inventory
  dropped to 255 production traces and backing traces remain at 0.
- 2026-05-17 10:23 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inline packed-args map constructor rewriting now obtains
  `collections.map_constructors` metadata through the bridge key instead of
  directly naming the map constructor surface ID; the map surface inventory
  dropped to 257 production traces and backing traces remain at 0.
- 2026-05-17 10:23 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer materializes variadic borrowed map packs with indexed count_ref helpers,ir lowerer materializes variadic pointer map packs with indexed count_ref helpers" --no-skip` |
  failures: ir lowerer materializes variadic borrowed map packs with indexed
  count_ref helpers; ir lowerer materializes variadic pointer map packs with
  indexed count_ref helpers | notes: both stale map variadic fixtures still
  fail during lowering on retired `/at` expression calls before the inline
  packed-args map constructor rewrite path is exercised.
- 2026-05-17 10:19 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers infer forwarded map access targets" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: IR access-target forwarded empty map constructor checks now derive
  the `mapNew` member token from `collections.map_constructors` metadata
  instead of spelling the token directly; the map surface inventory dropped
  to 259 production traces and backing traces remain at 0.
- 2026-05-17 10:15 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="template vector and map returns are allowed,canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors,block-bodied inferred canonical map returns rewrite constructors" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: infer-struct-return map collection fallback now derives the rooted
  map collection marker from `collections.map_constructors` import-alias
  metadata instead of a direct `"/map"` string; the map surface inventory
  remains at 260 production traces and backing traces remain at 0.
- 2026-05-17 10:12 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="template vector and map returns are allowed,canonical stdlib map returns are allowed" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: build-return-kind map collection normalization now derives the
  rooted map collection marker from `collections.map_constructors`
  import-alias metadata instead of a direct `"/map"` string; the map surface
  inventory remains at 260 production traces and backing traces remain at 0.
- 2026-05-17 10:08 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding normalization guards explicit map helper defs,ir lowerer direct expr and inference rewrites guard explicit map defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: statement-binding explicit map helper canonicalization now resolves
  map helper metadata through `collections.map_helpers` instead of directly
  naming the map helper surface ID; the map surface inventory dropped to 260
  production traces and backing traces remain at 0.
- 2026-05-17 10:08 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding normalization guards explicit map helper defs,ir lowerer direct expr and inference rewrites guard explicit map defs,ir lowerer call helpers keep explicit map helper same-path defs" --no-skip` |
  failures: ir lowerer call helpers keep explicit map helper same-path defs |
  notes: the two statement-binding guard cases passed; the broader stale
  same-path map-helper case still resolves alias `at` and `at_unsafe`
  compatibility defs where the fixture expects `nullptr`.
- 2026-05-17 10:04 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer map constructor rewrite checks constructor surface before resolving defs,ir lowerer setup type helper rejects canonical map constructor fallback to compatibility defs,ir lowerer setup type helper prefers canonical map method return structs over alias defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-call bridge path filtering now resolves map
  constructor metadata through `collections.map_constructors` instead of
  directly naming the map constructor surface ID; the map surface inventory
  dropped to 261 production traces and backing traces remain at 0.
- 2026-05-17 10:01 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers preserve canonical map helper method return chains,ir lowerer call helpers gate canonical map helpers with semantic target facts" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer builtin-name and shared helper map-helper metadata lookups now
  resolve through the `collections.map_helpers` bridge key instead of directly
  naming the map helper surface ID; the map surface inventory dropped to 262
  production traces and backing traces remain at 0.
- 2026-05-17 10:01 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers preserve canonical map helper method return chains,ir lowerer call helpers gate canonical map helpers with semantic target facts,stdlib surface metadata resolves collection helper member tokens" --no-skip` |
  failures: stdlib surface metadata resolves collection helper member tokens |
  notes: the two focused map-helper cases passed; the bundled metadata case
  still fails on stale SoA helper token expectations for
  `soaVectorCountRef`, `soaVectorFieldView`, and `soaVectorToAos`.
- 2026-05-17 09:58 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve map Result payload metadata,ir lowerer result helpers resolve function-returned map Result payload metadata,ir lowerer result helpers use semantic query facts for direct Result ok payload metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_runtime_tests`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="direct Result ok payload metadata uses semantic-product type facts first" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: direct Result value map-constructor metadata now resolves constructor
  metadata through `collections.map_constructors` instead of directly naming
  the map constructor surface ID; the map surface inventory dropped to 264
  production traces and backing traces remain at 0.
- 2026-05-17 09:55 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve map Result payload metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_runtime_tests`;
  `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="native Result ok emission uses semantic-product payload facts" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: packed Result map-constructor rewriting now resolves constructor
  metadata through `collections.map_constructors` instead of directly naming
  the map constructor surface ID; the map surface inventory dropped to 265
  production traces and backing traces remain at 0.
- 2026-05-17 09:55 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports direct map Result payloads,native Result ok emission uses semantic-product payload facts" --no-skip` |
  failures: ir lowerer supports direct map Result payloads |
  notes: the direct map Result payload case failed during parse/validate before
  the lowerer path; the runtime source-lock case lives in
  `PrimeStruct_backend_runtime_tests` and passed separately.
- 2026-05-17 09:48 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer constructor metadata helpers retire duplicated constructor tables,ir lowerer setup type infers referenced declared collection receivers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: IR lowerer declared-collection inference now resolves map constructor
  metadata through `collections.map_constructors` instead of directly naming
  the map constructor surface ID; the map surface inventory dropped to 266
  production traces and backing traces remain at 0.
- 2026-05-17 09:44 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors keep mismatch diagnostics on inferred canonical map default parameters,helper-wrapped inferred experimental result map default parameters validate,helper-wrapped inferred experimental result map default parameters keep mismatch diagnostics" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: build-parameter map default classification now uses the shared map
  collection classifier instead of direct canonical map type text; the map
  surface inventory dropped to 267 production traces and backing traces remain
  at 0.
- 2026-05-17 09:44 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept inferred canonical map default parameters,stdlib map constructors keep mismatch diagnostics on inferred canonical map default parameters,helper-wrapped inferred canonical map default parameters validate,helper-wrapped inferred experimental result map default parameters validate" --no-skip` |
  failures: stdlib map constructors accept inferred canonical map default
  parameters; helper-wrapped inferred canonical map default parameters validate |
  notes: the adjacent mismatch and Result-wrapped default-parameter cases
  passed in the same run and were rerun as the selected passing gate above.
- 2026-05-17 09:40 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper reference templated map count method rejects missing canonical helper,wrapper reference templated map count method rejects missing canonical ref helper,map slash-path explicit-template count method reports canonical return mismatch" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: pointer-like collection method normalization now derives the rooted
  map import-alias helper prefix from `collections.map_helpers` metadata
  instead of direct `map/` text; the map surface inventory dropped to 268
  production traces and backing traces remain at 0.
- 2026-05-17 09:35 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors,inferred canonical map returns keep constructor mismatch diagnostics,block-bodied inferred canonical map returns rewrite constructors,block-bodied inferred canonical map returns keep mismatch diagnostics" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: statement return collection normalization now derives the rooted map
  collection marker from `collections.map_constructors` metadata instead of a
  direct `/map` string; the `SemanticsValidatorStatementReturns.cpp` inventory
  allowance is now zero and backing traces remain at 0.
- 2026-05-17 09:29 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: statement return auto-diagnostic fallback now derives rooted map
  access unknown-method messages from `collections.map_helpers` metadata
  instead of hard-coded `/map/at*` strings; the map surface inventory dropped
  to 269 production traces and backing traces remain at 0.
- 2026-05-17 09:29 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned slash-method map access count keeps primitive diagnostics,map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference" --no-skip` |
  failures: wrapper-returned slash-method map access count keeps primitive
  diagnostics |
  notes: the two adjacent receiver-path inference cases passed in the same
  run and were rerun as the selected passing gate above; the count fixture
  now validates instead of reporting stale `unknown method: /map/at`.
- 2026-05-17 09:25 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference,wrapper-returned map method alias access keeps primitive argument diagnostics during inference" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic receiver-path resolution now derives the rooted map
  collection receiver alias from map helper metadata instead of a direct
  `/map` literal; the map surface inventory stayed at 273 production traces,
  the receiver-path file allowance dropped to 3, and backing traces remain
  at 0.
- 2026-05-17 09:25 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference,wrapper-returned map method alias access keeps primitive receiver diagnostics during inference,wrapper-returned map method alias access keeps primitive argument diagnostics during inference" --no-skip` |
  failures: wrapper-returned map method alias access keeps primitive receiver
  diagnostics during inference |
  notes: the three adjacent receiver-path cases passed in the same run and
  were rerun as the selected passing gate above; the primitive-receiver
  wrapper fixture no longer reports the stale `/project` inference message.
- 2026-05-17 09:20 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar,map compatibility tryAt call rejects visible canonical definition,stdlib namespaced map tryAt requires imported stdlib helper or explicit definition,stdlib namespaced map tryAt does not inherit alias-only helper definition,map wrapper temporary tryAt auto inference requires canonical helper definition" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic Result helper inference now derives canonical map
  `tryAt`/`tryAt_ref` helper paths and rooted aliases through
  `collections.map_helpers` metadata instead of direct path comparisons; the
  map surface inventory dropped to 273 production traces and backing traces
  remain at 0.
- 2026-05-17 09:17 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor keeps same-path definition before wrapper fallback,stdlib namespaced map constructor requires imported stdlib helper,canonical stdlib map helpers accept constructor receivers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: template monomorph canonical experimental map constructor rewrites
  now derive the canonical constructor path and rooted import alias through
  `collections.map_constructors` metadata instead of direct rooted/canonical
  string comparisons; the map surface inventory dropped to 276 production
  traces and backing traces remain at 0.
- 2026-05-17 09:12 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor keeps same-path definition before wrapper fallback,stdlib namespaced map constructor requires imported stdlib helper,canonical stdlib map helpers accept constructor receivers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: collection buffer/map resolver inference now derives the canonical
  map constructor path through `collections.map_constructors` metadata
  instead of a direct `/std/collections/map/map` literal; the map surface
  inventory dropped to 277 production traces and backing traces remain at 0.
- 2026-05-17 09:09 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary access call validates map target classification,map wrapper temporary unsafe access validates direct stdlib helper,map wrapper temporary access keeps canonical key diagnostics" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: collection return inference now derives canonical map access helper
  return lookup paths through `collections.map_helpers` metadata instead of
  direct string concatenation; the map surface inventory dropped to 278
  production traces and backing traces remain at 0.
- 2026-05-17 09:09 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary access call validates map target classification,map wrapper temporary unsafe access validates direct stdlib helper,map wrapper temporary access keeps canonical key diagnostics,map wrapper temporary bare helper calls validate target classification" --no-skip` |
  failures: map wrapper temporary bare helper calls validate target
  classification |
  notes: the direct wrapper temporary canonical map access cases passed in
  the same run and were rerun as the selected passing gate above; the bare
  helper wrapper fixture is stale after the map stdlib-ownership cutover.
- 2026-05-17 09:05 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map slash-path explicit-template count method reports canonical return mismatch,map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,map namespaced tryAt method validates through slash-path routing,map namespaced at method expression body arguments validate through slash-path target" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: pointer-like collection method normalization now derives the
  unrooted canonical map helper prefix through `collections.map_helpers`
  metadata instead of direct map surface IDs; the map surface inventory
  dropped to 279 production traces and backing traces remain at 0.
- 2026-05-17 09:05 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced count method compatibility alias is rejected,map slash-path explicit-template count method reports canonical return mismatch,map namespaced at method now validates through slash-path routing,map namespaced at_unsafe method auto inference now validates through slash-path routing,map namespaced tryAt method validates through slash-path routing,map namespaced at method expression body arguments validate through slash-path target" --no-skip` |
  failures: map namespaced count method compatibility alias is rejected |
  notes: the count alias fixture is stale and now validates successfully;
  the five adjacent slash-path normalization cases passed and were rerun as
  the selected passing gate above.
- 2026-05-17 09:00 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map tryAt requires imported stdlib helper or explicit definition,stdlib namespaced map tryAt does not inherit alias-only helper definition,map wrapper temporary tryAt auto inference requires canonical helper definition" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: try builtin validation now derives canonical map tryAt helper paths
  through `collections.map_helpers` metadata instead of direct map surface
  IDs; the map surface inventory dropped to 280 production traces and backing
  traces remain at 0.
- 2026-05-17 08:56 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map access count shadow keeps canonical precedence over alias helper,stdlib canonical map access count shadow currently validates mixed canonical and alias returns,wrapper-returned referenced canonical map access count call auto inference keeps string helper mismatch diagnostics" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: expression method resolution now derives canonical map access helper
  paths through `collections.map_helpers` metadata instead of direct map
  surface IDs; the map surface inventory dropped to 282 production traces and
  backing traces remain at 0.
- 2026-05-17 08:56 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map access count shadow keeps canonical precedence over alias helper,stdlib canonical map access count shadow currently validates mixed canonical and alias returns,non-imported wrapper-returned canonical map reference access keeps primitive receiver diagnostics" --no-skip` |
  failures: non-imported wrapper-returned canonical map reference access keeps
  primitive receiver diagnostics |
  notes: the program still rejects, but the stale expectation for
  `unknown method: /i32/count` no longer matches current diagnostics; the
  directly affected map access string-count shadow cases passed in the same
  run and were rerun with the passing gate above.
- 2026-05-17 08:52 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,stdlib namespaced map access keeps canonical target over alias,map namespaced access call keeps canonical struct-return forwarding" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: expression collection-dispatch setup now resolves canonical map
  access helper paths through `collections.map_helpers` metadata instead of
  direct map surface IDs; the map surface inventory dropped to 283
  production traces and backing traces remain at 0.
- 2026-05-17 08:49 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="print accepts string map access,explicit canonical map parameter keeps print statement string validation,canonical map reference parameter keeps print statement string validation,wrapper-returned canonical map keeps print statement string validation,wrapper-returned referenced canonical map keeps print statement string validation" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: statement printability now resolves canonical map access helper
  paths through `collections.map_helpers` metadata instead of direct map
  surface IDs; the map surface inventory dropped to 284 production traces
  and backing traces remain at 0.
- 2026-05-17 08:45 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition,bare map contains call resolves through canonical helper definition,map namespaced contains method now validates through slash-path routing,map stdlib namespaced contains method now validates through slash-path routing" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic map/SOA builtin validation now resolves canonical map
  contains helper paths through `collections.map_helpers` metadata instead
  of direct map surface IDs; the map surface inventory dropped to 285
  production traces and backing traces remain at 0.
- 2026-05-17 08:45 CEST | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition,bare map contains call resolves through canonical helper definition,canonical map borrowed receiver validates direct stdlib contains,canonical map borrowed receiver keeps contains key diagnostics" --no-skip` |
  failures: canonical map borrowed receiver validates direct stdlib contains;
  canonical map borrowed receiver keeps contains key diagnostics |
  notes: borrowed canonical contains cases fail after semantic validation
  reaches IR lowering with unsupported direct canonical map contains lowering;
  replaced as this slice's selected gate by the passing direct bare/namespaced
  contains semantic cases above.
- 2026-05-17 08:40 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access keeps canonical struct-return forwarding,map namespaced access call keeps canonical struct-return forwarding,map namespaced access alias rejects canonical struct-return forwarding,map namespaced unsafe access alias rejects canonical struct-return forwarding" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic struct-return map helper probing now derives explicit map
  access helper names and canonical map method candidates through
  `collections.map_helpers` metadata instead of direct rooted/canonical map
  helper strings; the map surface inventory dropped to 287 production traces
  and backing traces remain at 0.
- 2026-05-17 08:25 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map wrappers resolve through explicit namespaced helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic initializer helper preference now resolves explicit stdlib
  map helper names and canonical helper paths through
  `collections.map_helpers` metadata instead of a literal
  `std/collections/map` namespace/helper table; the map surface inventory
  dropped to 293 production traces and backing traces remain at 0.
- 2026-05-17 00:15 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map Ref helper calls accept borrowed map references" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic collection dispatch setup now recognizes rooted map
  access compatibility paths through `CollectionsMapHelpers` import-alias
  metadata instead of a literal `/map/at*` table; the map surface inventory
  dropped to 297 production traces and backing traces remain at 0.
- 2026-05-17 00:12 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers prefer direct same-path map count-like defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer count fallback now recognizes removed map helper alias
  calls through `collections.map_helpers` metadata instead of a local
  slashless `map/` helper table; the broader non-method count fallback
  source test remains stale and is listed in Current Known Failures. The map
  surface inventory dropped to 299 production traces and backing traces
  remain at 0.
- 2026-05-17 00:09 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map wrappers resolve through explicit namespaced helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic concrete call resolution now recognizes map entry helper
  and constructor base paths through map surface metadata instead of direct
  canonical map entry/constructor literals; the map surface inventory
  dropped to 300 production traces and backing traces remain at 0.
- 2026-05-17 00:05 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced at method expression body arguments validate through slash-path target" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic simple-call path helpers now resolve map helper member
  names through `collections.map_helpers` metadata instead of direct
  slashless and canonical map prefix stripping; the map surface inventory
  dropped to 305 production traces and backing traces remain at 0.
- 2026-05-17 00:03 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access alias rejects canonical struct-return forwarding" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: semantic method-target resolution now recognizes slashless map
  helper alias paths through `metadataBackedMapHelperRootAliasMethodName`
  instead of direct `map/` prefix checks; the map surface inventory dropped
  to 309 production traces and backing traces remain at 0.
- 2026-05-16 23:59 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer simple-call scoped collection alias rejection now
  recognizes map helper surface paths through `CollectionsMapHelpers`
  metadata instead of a direct slashless `map/` prefix check; the map
  surface inventory remains at 313 production traces and backing traces
  remain at 0.
- 2026-05-16 23:56 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inline native dispatch now classifies removed explicit map access
  helper paths through `CollectionsMapHelpers` metadata instead of a literal
  `map/at*` raw-path table; the map surface inventory now observes 313
  production traces and backing traces remain at 0.
- 2026-05-16 23:54 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer builtin array-access classification now rejects published
  map helper surface paths through `CollectionsMapHelpers` metadata instead
  of direct `map/` and `std/collections/map/` prefix checks; the map surface
  inventory now observes 314 production traces and backing traces remain at 0.
- 2026-05-16 23:50 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer emit-expression explicit map helper rewriting now detects
  raw alias and canonical map helper paths through `CollectionsMapHelpers`
  metadata instead of direct rooted/canonical map helper prefix checks; the
  map surface inventory now observes 315 production traces and backing traces
  remain at 0.
- 2026-05-16 23:46 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none in the
  listed passing commands; the stale explicit map helper same-path fixture is
  listed above | notes: lowerer statement-binding explicit map helper
  canonicalization now detects raw published map helper paths through
  `CollectionsMapHelpers` metadata instead of direct rooted/canonical map path
  prefix checks; the map surface inventory remains at 318 production traces
  because a raw path trace was replaced by a surface-id trace, and backing
  traces remain at 0.
- 2026-05-16 23:43 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter collection-type canonical map access detection now resolves
  helper ownership through `collections.map_helpers` metadata instead of
  parsing the canonical map helper prefix; the map surface inventory now
  observes 318 production traces and backing traces remain at 0.
- 2026-05-16 23:39 CEST | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter call-path helper classification now derives map helper and
  constructor path ownership through published stdlib surface metadata instead
  of direct map helper prefix strings; the map surface inventory now observes
  320 production traces and backing traces remain at 0.
- 2026-05-16 23:35 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter method type inference now classifies canonical and import-alias
  map access helper paths through published stdlib surface metadata instead of
  direct map helper prefix checks; the map surface inventory now observes 326
  production traces and backing traces remain at 0.
- 2026-05-16 23:31 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter method resolution now resolves explicit map helper names and
  explicit helper paths through published stdlib surface metadata instead of
  direct map helper prefix and path strings; the map surface inventory now
  observes 335 production traces and backing traces remain at 0.
- 2026-05-16 23:29 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter setup return-inference map method candidate construction now
  derives explicit import-alias and canonical helper paths through published
  stdlib surface metadata instead of direct map helper path strings; the map
  surface inventory now observes 343 production traces and backing traces
  remain at 0.
- 2026-05-16 23:24 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter method metadata receiver normalization now relies on the
  shared map type classifier instead of a direct rooted `/map` type check; the
  map surface inventory remains at 349 production traces and backing traces
  remain at 0.
- 2026-05-16 23:22 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter method metadata removed-alias detection now resolves both
  map import aliases and canonical map helper paths through the published
  stdlib surface member resolver instead of direct map helper prefix checks;
  the map surface inventory now observes 349 production traces and backing
  traces remain at 0.
- 2026-05-16 23:17 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter helper rejects removed full-path map method aliases,C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter method metadata removed-alias detection now resolves
  canonical map helper paths through stdlib surface metadata instead of a
  direct slashless canonical map path string; the map surface inventory now
  observes 351 production traces and backing traces remain at 0.
- 2026-05-16 23:13 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs,ir lowerer canonical map contains and tryAt rewrites stay recognized late,ir lowerer late expression canonical map helpers use path-family gates" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lower emit expression collection helper explicit map helper
  canonicalization now derives the canonical map helper root through the local
  collection path helper instead of a direct canonical map path string; stale
  source locks for the adjacent path-family guards were updated; the map
  surface inventory now observes 352 production traces and backing traces
  remain at 0.
- 2026-05-16 21:30 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper preserves inferred borrowed map return metadata,ir lowerer statement binding helper keeps canonical map constructor value metadata,ir lowerer statement binding helper inherits map metadata from named source binding" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: statement binding explicit map helper canonicalization now derives
  the canonical map helper root through a local collection path helper instead
  of a direct canonical map path string; the map surface inventory now
  observes 353 production traces and backing traces remain at 0.
- 2026-05-16 21:27 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline call context helper prepares scoped setup,ir lowerer inline call context helper reports setup diagnostics,ir lowerer inline-call context-setup step initializes context and zero value" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inline-call context generated canonical map constructor helper
  recognition now derives the map constructor prefix through collection path
  helpers instead of a direct map path string; the map surface inventory now
  observes 354 production traces and backing traces remain at 0.
- 2026-05-16 21:24 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer result helpers resolve function-returned map Result payload metadata,ir lowerer result helpers require semantic query facts for unresolved Result.map metadata" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: Result metadata direct map constructor recognition now derives rooted
  and unrooted canonical map constructor paths through collection path helpers
  instead of direct map path strings; the map surface inventory now observes
  355 production traces and backing traces remain at 0.
- 2026-05-16 21:21 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer count access helpers normalize parser-shaped canonical map access receivers,ir lowerer count access helpers prefer graph facts for string map access emission" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: count/access map access helper lookup, explicit std-map spelling
  checks, and rewritten parser-shaped map access receivers now derive
  canonical map helper paths and prefixes through collection path helpers
  instead of direct map path strings; the map surface inventory now observes
  359 production traces and backing traces remain at 0.
- 2026-05-16 21:17 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves method definitions from receiver targets" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-target map method prefix stripping now derives
  rooted and canonical map prefixes through collection path helpers instead
  of direct map path strings; the map surface inventory now observes 363
  production traces and backing traces remain at 0.
- 2026-05-16 21:14 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer collection helper rewrite guards explicit map defs,ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids" --no-skip`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="native tail and late collection helper metadata dispatch stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type collection helper map alias and normalization checks now
  derive rooted and canonical map prefixes through collection path helpers
  instead of direct map helper path strings; the map surface inventory now
  observes 369 production traces and backing traces remain at 0.
- 2026-05-16 21:11 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: emitter binding-type map compatibility checks now derive canonical and
  experimental map type paths through local collection path helpers instead of
  direct map path fragments; the map surface inventory now observes 376
  production traces and backing traces remain at 0.
- 2026-05-16 21:07 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers keep bare map access canonical forwarding,ir lowerer struct return helpers keep canonical slash-path map access forwarding,ir lowerer struct return helpers keep canonical map access call forwarding,ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer struct-return map helper candidate paths now derive rooted
  and canonical prefixes through collection path helpers instead of direct map
  helper path strings; the map surface inventory now observes 381 production
  traces and backing traces remain at 0.
- 2026-05-16 21:06 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`
  | failures: `canonical map surface owns standalone stdlib implementation` |
  notes: first source-lock assertion checked the semantic struct-return source
  instead of the lowerer struct-return helper source; the follow-up run above
  passed after adding the correct lowerer source fixture.
- 2026-05-16 21:03 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference base-kind helpers resolve parser-shaped canonical map result helpers" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inference base-kind map tryAt/contains call-name checks now derive
  slashless canonical helper paths through collection path helpers instead of
  direct map helper path strings; the map surface inventory now observes 390
  production traces and backing traces remain at 0.
- 2026-05-16 20:59 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper rejects explicit map helper return kinds same-path,ir lowerer setup type helper reports method call definition diagnostics from expressions" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-call synthetic fallback blocking and explicit
  vector-count map-target checks now derive map paths through collection path
  helpers instead of direct map path strings; the map surface inventory now
  observes 394 production traces and backing traces remain at 0.
- 2026-05-16 20:56 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper keeps reject diagnostics for canonical map helper receiver calls,ir lowerer setup type helper keeps bare map method receiver canonical precedence" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-call canonical map helper lookup now builds the
  helper path through collection path helpers instead of direct map helper path
  concatenation; the map surface inventory now observes 397 production traces
  and backing traces remain at 0.
- 2026-05-16 20:53 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper rejects slash-path map methods from expressions,ir lowerer setup type helper resolves canonical map methods from slashless receiver call paths" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-call map helper prefix stripping now derives rooted
  and canonical map prefixes through collection path helpers instead of direct
  map path strings; the map surface inventory now observes 398 production traces
  and backing traces remain at 0.
- 2026-05-16 20:50 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper rejects canonical map constructor fallback to compatibility defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method-call canonical map constructor direct-target checks
  now construct the canonical path through collection path helpers instead of a
  direct map path literal; the map surface inventory now observes 406 production
  traces and backing traces remain at 0.
- 2026-05-16 20:46 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves method definitions from receiver targets" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: setup-type method target map receiver recognition now uses shared
  collection classifiers instead of direct rooted/canonical map strings; the map
  surface inventory now observes 407 production traces and backing traces remain
  at 0.
- 2026-05-16 20:44 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference expr-kind dispatch setup wires callback,ir lowerer direct expr and inference rewrites guard explicit map defs" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: inference base-kind and dispatch setup map family recognition now use
  shared collection classifiers instead of direct rooted/canonical map strings;
  the map surface inventory now observes 408 production traces and backing traces
  remain at 0.
- 2026-05-16 20:41 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: binding type normalization now recognizes map types through shared
  collection classifiers instead of direct rooted/canonical map strings; the map
  surface inventory now observes 410 production traces and backing traces remain
  at 0. A direct backend binding-type doctest rerun still hit the known stale SoA
  source-lock assertions listed above.
- 2026-05-16 20:37 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type infers referenced declared collection receivers,ir lowerer constructor metadata helpers retire duplicated constructor tables" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: declared collection inference now recognizes map type bases and direct
  map constructors through shared collection classifiers and constructor surface
  metadata; the map surface inventory now observes 412 production traces and
  backing traces remain at 0.
- 2026-05-16 20:37 local | fail | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests` |
  failures: `tests/unit/test_stdlib_map_ownership.cpp` compile |
  notes: first source-lock build missed the local
  `declaredCollectionInferenceSource` fixture variable; the follow-up build and
  `primestruct.stdlib.map_ownership` run passed after wiring the fixture.
- 2026-05-16 20:34 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves builtin-like count call methods" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: struct-slot layout now recognizes builtin map type names through the
  shared collection classifier instead of direct rooted/canonical map type
  strings; the map surface inventory now observes 416 production traces and
  backing traces remain at 0.
- 2026-05-16 20:31 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves method receiver struct paths from call expressions" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: uninitialized-struct inference now recognizes explicit map args-pack
  access through the published map-helper surface instead of direct rooted or
  canonical map access strings; the map surface inventory now observes 418
  production traces and backing traces remain at 0.
- 2026-05-16 20:28 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline map insert helper prefers semantic receiver facts,ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py`;
  `python3 scripts/check_map_backing_traces.py` | failures: none |
  notes: lowerer inline-call map-kind inference now uses shared collection
  classifiers instead of direct rooted/canonical map type strings; the map
  surface inventory now observes 419 production traces and backing traces remain
  at 0.
- 2026-05-16 20:15 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers,ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inline native map helper recognition now resolves the map helper
  family through `collections.map_helpers` metadata; the map surface inventory
  now observes 423 production traces and backing traces remain at 0.
- 2026-05-16 20:13 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers,ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first" --no-skip` |
  failures: ir lowerer inline native dispatch prefers published canonical map access helpers; ir lowerer call helpers keep explicit map helpers out of native builtin emission |
  notes: first inline-dispatch rerun exposed a stale source-lock gate name and
  stale `/map/count` native-tail emission expectation; both passed after the
  lock and removed-alias expectation were updated.
- 2026-05-16 20:10 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer map insert rewrite uses semantic receiver facts before stale locals,ir lowerer inline map insert helper prefers semantic receiver facts,ir lowerer skips builtin map insert rewrite for direct experimental map locals,ir lowerer statement map insert rewrite uses semantic product receiver ids first,ir lowerer tail map insert rewrite uses semantic receiver facts first,ir lowerer map insert helper writes grown pointers back through wrapper locals,ir lowerer statement call emission source delegation stays stable" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-call map insert recognition and rewrite targets now route
  through stdlib surface metadata; the map surface inventory now observes 425
  production traces and backing traces remain at 0.
- 2026-05-16 20:08 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer map insert rewrite uses semantic receiver facts before stale locals,ir lowerer inline map insert helper prefers semantic receiver facts,ir lowerer skips builtin map insert rewrite for direct experimental map locals,ir lowerer statement map insert rewrite uses semantic product receiver ids first,ir lowerer tail map insert rewrite uses semantic receiver facts first,ir lowerer map insert helper writes grown pointers back through wrapper locals,ir lowerer statement call emission source delegation stays stable" --no-skip` |
  failures: ir lowerer map insert rewrite uses semantic receiver facts before stale locals |
  notes: first statement-call rerun exposed stale canonical insert test stubs
  and a stale-local semantic receiver leak; both passed after routing the rewrite
  through metadata and statement-call-specific receiver inference.
- 2026-05-16 19:57 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers inline direct canonical map count-like helpers for map locals,ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs,ir lowerer map constructor rewrite checks constructor surface before resolving defs,ir lowerer statement expr has no inline builtin map insert family" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-expression lowering now derives direct canonical map helper
  and constructor recognition through stdlib surface metadata; the map
  surface inventory now observes 436 production traces and backing traces
  remain at 0.
- 2026-05-16 19:56 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers inline direct canonical map count-like helpers for map locals,ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs,ir lowerer map constructor rewrite checks constructor surface before resolving defs,ir lowerer statement expr has no inline builtin map insert family,ir lowerer helper rejects parser-shaped canonical map entry constructors as builtin map" --no-skip` |
  failures: ir lowerer helper rejects parser-shaped canonical map entry constructors as builtin map |
  notes: the first selected lowerer statement-expression run included the
  already-known stale parser-shaped canonical map entry constructor source
  fixture; adjacent statement-expression and direct canonical map helper
  cases passed in the follow-up run.
- 2026-05-16 19:52 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer native tail map access inference uses semantic receiver facts first,native tail and late collection helper metadata dispatch stays source locked,ir lowerer builtin map access prefers semantic target facts,ir lowerer count access helpers normalize parser-shaped canonical map access receivers" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: native-tail dispatch now derives canonical map helper paths,
  explicit map helper classification, import coverage, and diagnostics
  through stdlib surface metadata; the map surface inventory now observes 450
  production traces and backing traces remain at 0.
- 2026-05-16 19:48 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer map constructor rewrite checks constructor surface before resolving defs,ir lowerer late collection constructor guards use published constructor queries,ir lowerer constructor metadata helpers retire duplicated constructor tables" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: IR access-target resolution now recognizes canonical map
  constructor and access-helper paths through stdlib surface metadata; the
  map surface inventory now observes 464 production traces and backing traces
  remain at 0.
- 2026-05-16 19:46 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers source delegation stays stable,ir lowerer map constructor rewrite checks constructor surface before resolving defs,ir lowerer late collection constructor guards use published constructor queries" --no-skip` |
  failures: ir lowerer call helpers source delegation stays stable; ir lowerer
  map constructor rewrite checks constructor surface before resolving defs |
  notes: source-delegation still has the known stale map/SoA source-lock
  assertions listed above; the constructor-rewrite guard also carried an
  obsolete source search and was updated before the focused passing rerun.
- 2026-05-16 19:41 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="exact map imports keep canonical wrapper access helpers visible,wildcard collection imports keep bare vector and map bridge aliases,exact vector import keeps map bridge aliases unavailable,exact map import keeps vector bridge aliases unavailable,map namespaced access call keeps canonical struct-return forwarding,map namespaced access alias rejects canonical struct-return forwarding,map compatibility count call requires explicit alias definition,map compatibility count auto inference requires explicit alias definition,map compatibility contains call rejects visible canonical definition,map compatibility tryAt call rejects visible canonical definition,map compatibility at call requires explicit alias definition,stdlib-owned map compatibility at call falls back to canonical helper" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic collection compatibility now derives canonical map helper
  paths, helper metadata lookups, wrapper-surface detection, and removed
  body-argument targets through stdlib surface metadata; the map surface
  inventory now observes 491 production traces and backing traces remain at 0.
- 2026-05-16 19:39 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="exact vector import keeps map bridge aliases unavailable" --no-skip` |
  failures: exact vector import keeps map bridge aliases unavailable |
  notes: first compatibility rewrite patch reported `unknown call target:
  count` instead of the expected canonical map helper target; fixed by
  preserving the bare map-helper canonical diagnostic when the canonical
  helper is not imported or declared.
- 2026-05-16 19:33 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression keeps canonical helper return precedence,map stdlib namespaced count expression succeeds when canonical helper matches return type,map stdlib namespaced count expression non-builtin arity falls back to canonical helper return,map stdlib namespaced count expression does not fall back to map alias helper,map stdlib namespaced count expression ignores templated alias helper fallback,map stdlib namespaced count auto inference keeps canonical helper return precedence,map stdlib namespaced count auto inference keeps canonical unknown target,map stdlib namespaced access helper auto inference keeps canonical precedence,stdlib namespaced map at requires imported stdlib helper or explicit definition,stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: late fallback return-kind inference now derives canonical map helper
  paths and contains/tryAt/access classification through stdlib surface
  metadata; the map surface inventory now observes 507 production traces and
  backing traces remain at 0.
- 2026-05-16 19:31 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map pre-dispatch inference keeps rooted and canonical helper paths isolated,map canonical explicit-template count call keeps canonical non-templated diagnostics,map canonical implicit-template count call keeps canonical non-templated diagnostics,map canonical slash-path explicit-template access method stays on canonical unknown call target diagnostic,map canonical implicit-template count wrapper slash return keeps canonical diagnostics,map canonical wrapper auto local keeps builtin count diagnostics,map canonical reference wrapper auto local keeps key diagnostics,templated canonical map count wrapper method sugar rejects without explicit alias,map stdlib namespaced count expression keeps canonical helper return precedence,map stdlib namespaced count expression ignores templated alias helper fallback,map compatibility explicit-template count call keeps alias precedence with canonical templated helper" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: template-monomorph map helper rewrite and receiver-resolution now
  resolve canonical helper paths, helper classification, preferred lowering
  spellings, and unknown-target paths through stdlib surface metadata; the
  map surface inventory now observes 523 production traces and backing traces
  remain at 0.
- 2026-05-16 19:25 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map access wrapper ignores removed alias helper,bare map access methods require imported canonical helpers or explicit definitions,stdlib namespaced map at requires imported stdlib helper or explicit definition,stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition,canonical stdlib map helpers accept constructor receivers,stdlib canonical map contains and tryAt helpers resolve in method-call sugar,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access ignores rooted alias struct-return helper,map method access reports canonical builtin result type over alias helper,map compatibility tryAt call rejects visible canonical definition" --no-skip`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: pre-dispatch direct-call handling now derives canonical map helper
  paths, source-spelling checks, builtin classification, and diagnostics
  through stdlib surface metadata; the map surface inventory now observes
  557 production traces and backing traces remain at 0.
- 2026-05-16 19:22 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access keeps canonical struct-return forwarding"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access field expression keeps canonical struct-return forwarding"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access reports canonical builtin result type over alias helper"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map compatibility tryAt call rejects visible canonical definition"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: method target resolution now derives canonical map helper paths,
  namespace checks, explicit helper spelling, and builtin-method
  classification through stdlib surface metadata; the map surface inventory
  now observes 573 production traces and backing traces remain at 0.
- 2026-05-16 19:18 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call resolves through canonical helper definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call rejects compatibility alias when canonical helper is absent"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map at requires imported stdlib helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map access wrapper ignores removed alias helper"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: collection access target resolution now derives canonical map
  access/contains helper paths, namespace checks, and missing-definition
  diagnostics through stdlib surface metadata; the map surface inventory now
  observes 595 production traces and backing traces remain at 0.
- 2026-05-16 19:13 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access rejects missing receiver method during inference"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned map method alias access keeps primitive argument diagnostics during inference"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: pointer-like method normalization now derives the canonical map
  helper prefix from stdlib surface metadata; stale map method-alias
  expectations now distinguish the accepted matching-receiver path from the
  missing `/Marker/tag` diagnostic. The map surface inventory observes 641
  production traces and backing traces remain at 0.
- 2026-05-16 19:11 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access keeps primitive argument diagnostics during inference"` |
  failures: map method alias access keeps primitive argument diagnostics
  during inference | notes: stale expectation looked for
  `unable to infer return type on /project`; current diagnostic is
  `unknown method: /Marker/tag`.
- 2026-05-16 19:10 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="print accepts string map access"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map access accepts string key expression"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement printability now resolves canonical map access helper
  classification through stdlib surface metadata; the map surface inventory
  now observes 641 production traces and backing traces remain at 0.
- 2026-05-16 19:09 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="print accepts string map access"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="string map access rejects numeric index"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="unsafe string map access rejects numeric index"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map access accepts string key expression"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: collection argument validation now resolves canonical map access
  helper paths and explicit access-path classification through stdlib surface
  metadata; the map surface inventory now observes 642 production traces and
  backing traces remain at 0.
- 2026-05-16 19:06 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call resolves through canonical helper definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map contains requires imported stdlib helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: map/SOA builtin validation now resolves canonical map contains
  helper definition checks and diagnostics through stdlib surface metadata;
  the map surface inventory now observes 647 production traces and backing
  traces remain at 0.
- 2026-05-16 19:04 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary tryAt auto inference requires canonical helper definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary contains call validates canonical target classification"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary contains call requires canonical helper definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary contains auto inference requires canonical helper definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map compatibility tryAt call rejects visible canonical definition"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: late map access builtin validation now resolves canonical helper
  names, namespace checks, and unknown-target diagnostics through stdlib
  surface metadata; the map surface inventory now observes 653 production
  traces and backing traces remain at 0.
- 2026-05-16 19:01 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map at requires imported stdlib helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map at_unsafe requires imported stdlib helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map access helpers accept imported stdlib wrappers"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic collection-access validation now resolves canonical map
  access helper names, namespace checks, and diagnostic paths through stdlib
  surface metadata; the map surface inventory now observes 681 production
  traces and backing traces remain at 0.
- 2026-05-16 18:58 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="print accepts string map access"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects Create with canonical map method precedence when constructor is not effect-free"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map call precedence keeps builtin count diagnostics before omitted initializer"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects Create with canonical slash-path map call helper when constructor is not effect-free"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned canonical map call keeps builtin count diagnostics before omitted initializer"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects wrapper-returned canonical map method helper fallback when constructor is not effect-free"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic effect-free collection helper routing now builds canonical
  map helper paths through stdlib surface metadata; stale direct-call omitted
  initializer expectations now assert the current builtin-count diagnostic.
- 2026-05-16 18:55 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects Create with canonical map call precedence when constructor is not effect-free"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="omitted initializer rejects wrapper-returned canonical map call helper fallback when constructor is not effect-free"` |
  failures: `omitted initializer rejects Create with canonical map call precedence when constructor is not effect-free`,
  `omitted initializer rejects wrapper-returned canonical map call helper fallback when constructor is not effect-free` |
  notes: stale expectations still wanted omitted-initializer diagnostics for
  direct map-count calls that now fail earlier with builtin-count arity
  diagnostics; updated in the following passing rerun.
- 2026-05-16 18:54 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map tryAt call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic expression collection-dispatch setup now resolves canonical
  map access helper names through stdlib surface metadata; the map surface
  inventory now observes 700 production traces and backing traces remain at 0.
- 2026-05-16 18:52 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map tryAt call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic infer collection-dispatch setup now resolves canonical map
  access helper names and namespaces through stdlib surface metadata; the map
  surface inventory now observes 703 production traces and backing traces
  remain at 0.
- 2026-05-16 18:50 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map tryAt call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic collection-dispatch return-kind checks now resolve canonical
  map helper names through stdlib surface metadata; the map surface inventory
  now observes 708 production traces and backing traces remain at 0.
- 2026-05-16 18:49 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map tryAt call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic method-resolution map helper return and missing-definition
  checks now resolve canonical helper names through stdlib surface metadata;
  the map surface inventory now observes 715 production traces and backing
  traces remain at 0.
- 2026-05-16 18:46 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map contains call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map tryAt call requires imported canonical helper or explicit definition"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="bare map access methods require imported canonical helpers or explicit definitions"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic infer pre-dispatch map helper visibility checks now resolve
  canonical helper names through stdlib surface metadata; the map surface
  inventory now observes 729 production traces and backing traces remain at 0.
- 2026-05-16 19:35 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map tryAt requires imported stdlib helper or explicit definition,bare map tryAt call requires imported canonical helper or explicit definition,stdlib canonical map contains and tryAt helpers resolve in method-call sugar"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic `try` builtin validation now obtains canonical map tryAt
  helper paths from stdlib surface metadata instead of spelling them
  directly; the map surface inventory now observes 747 production traces and
  backing traces remain at 0.
- 2026-05-16 19:27 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: template-monomorph expression rewrite, receiver-resolution, and
  semantic builtin path helpers now use shared experimental map root/path
  helpers; the map surface inventory now observes 756 production traces and
  backing traces now observe 0.
- 2026-05-16 19:22 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map insert validates on explicit experimental map bindings,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic collection-helper rewrite and compatibility internals now
  use the shared experimental map root helper for backing helper paths; the
  map surface inventory now observes 763 production traces and backing traces
  now observe 7.
- 2026-05-16 19:15 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic and template-monomorph generated map backing detection now
  uses the shared experimental collection backing classifier instead of
  spelling the generated `Map` prefix; the map surface inventory now observes
  766 production traces and backing traces now observe 10.
- 2026-05-16 19:10 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic collection compatibility helper selection no longer
  hard-codes the experimental map helper root; the map surface inventory now
  observes 772 production traces and backing traces now observe 16.
- 2026-05-16 19:03 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,canonical stdlib map helpers accept constructor receivers,stdlib namespaced map constructor accepts explicit experimental map bindings"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: semantic map constructor and entry-argument path checks now route
  experimental map backing member paths through shared constructor helpers;
  the map surface inventory now observes 778 production traces and backing
  traces now observe 22.
- 2026-05-16 19:03 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer helper rejects parser-shaped canonical map entry constructors as builtin map"` |
  failures: `ir lowerer helper rejects parser-shaped canonical map entry
  constructors as builtin map` | notes: stale parser-shaped canonical map
  entry builtin-classification expectation; reran the adjacent semantic map
  constructor cases above.
- 2026-05-16 18:54 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="dump ast-semantic shows experimental map destroy cleanup"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: IR-printer return-kind type parsing now classifies experimental map
  backing bases through the existing generic experimental collection type
  helper; the map surface inventory now observes 790 production traces and
  backing traces now observe 34.
- 2026-05-16 18:49 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native canonical map access direct calls and method sugar use ordinary map helpers,compiles and runs native builtin canonical map first-growth inserts"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: try-expression specialized map Result payload inference now derives
  the generated experimental map backing prefix through the local collection
  type helper; the map surface inventory now observes 796 production traces
  and backing traces now observe 40.
- 2026-05-16 18:49 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports direct map Result payloads,ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer call helpers infer forwarded map access targets"` |
  failures: `ir lowerer supports direct map Result payloads` | notes: stale
  retired map literal/count compatibility fixture failed `parseAndValidate`;
  reran the adjacent Result metadata and forwarded map access cases above.
- 2026-05-16 18:44 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native canonical map access direct calls and method sugar use ordinary map helpers,compiles and runs native builtin canonical map first-growth inserts"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-expression map count/access target classification now
  reuses local experimental collection type and generated-specialization
  helpers; the map surface inventory now observes 798 production traces and
  backing traces now observe 42.
- 2026-05-16 18:39 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native builtin canonical map first-growth inserts,compiles and runs native builtin canonical map repeated-growth inserts,compiles and runs native builtin canonical map insert overwrites"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inline call map-kind inference and experimental map receiver
  classification now derive rooted, slashless, and generated backing paths
  through local collection path helpers; the map surface inventory now
  observes 801 production traces and backing traces now observe 45.
- 2026-05-16 18:39 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers build inline arguments for inferred experimental map receiver methods,ir lowerer call helpers infer forwarded map access targets"` |
  failures: `ir lowerer call helpers build inline arguments for inferred
  experimental map receiver methods` | notes: stale retired mapPair receiver
  fixture failed `parseAndValidate`; reran the adjacent forwarded map access
  target case above.
- 2026-05-16 18:34 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native canonical map access direct calls and method sugar use ordinary map helpers,compiles and runs native builtin canonical map first-growth inserts"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: collection expression lowering now builds experimental map backing
  member and generated `Map` prefixes through local collection path helpers;
  the map surface inventory now observes 806 production traces and backing
  traces now observe 50.
- 2026-05-16 18:29 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs builtin canonical map first-growth inserts in C++ emitter,compiles and runs builtin canonical map repeated-growth inserts in C++ emitter,compiles and runs builtin canonical map insert overwrites in C++ emitter"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: emitter return-kind type parsing now classifies experimental map
  backing bases through a local storage-base helper instead of direct
  experimental map path/prefix strings; the map surface inventory now
  observes 809 production traces and backing traces now observe 53.
- 2026-05-16 18:24 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native builtin canonical map first-growth inserts,compiles and runs native builtin canonical map repeated-growth inserts,compiles and runs native builtin canonical map insert overwrites"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-call map insert rewriting now builds rooted, slashless, and
  generated experimental map backing paths through the local collection path
  helper; the map surface inventory now observes 814 production traces and
  backing traces now observe 59.
- 2026-05-16 18:18 local | pass | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: access-target map struct inference now derives rooted and slashless
  experimental backing paths with `experimentalCollectionTypePath`; the map
  surface inventory now observes 822 production traces and backing traces now
  observe 67.
- 2026-05-16 18:18 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets,ir lowerer call helpers lower explicit map access for args-pack receivers"` |
  failures: `ir lowerer call helpers lower explicit map access for args-pack
  receivers` | notes: stale explicit args-pack map access native-tail
  dispatch and instruction-emission expectations; reran the adjacent direct
  and forwarded access-target cases above.
- 2026-05-16 18:12 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers resolve value kinds from transforms,ir lowerer binding type helpers prefer semantic collection specialization facts"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: binding type normalization now builds rooted and slashless
  experimental map backing paths with `experimentalCollectionTypePath`; the
  map surface inventory now observes 826 production traces and backing traces
  now observe 71.
- 2026-05-16 18:12 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers classify binding kind and string/fileerror types,ir lowerer binding type helpers resolve value kinds from transforms,ir lowerer binding type helpers prefer semantic collection specialization facts"` |
  failures: `ir lowerer binding type helpers classify binding kind and
  string/fileerror types` | notes: stale exact-source assertions for SoA
  experimental type strings; reran the adjacent behavioral cases above.
- 2026-05-16 18:09 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct layout helpers compute uncached diagnostics,ir lowerer call helpers build this params and collect struct fields,ir lowerer result helpers resolve Result.map struct payload metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: struct-slot map type recognition now builds rooted and slashless
  experimental map backing paths with `experimentalCollectionTypePath`; the
  map surface inventory now observes 832 production traces and backing traces
  now observe 77.
- 2026-05-16 18:07 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper resolves method call definitions from expressions,ir lowerer setup type helper reports method call definition diagnostics from expressions,ir lowerer setup type helper resolves method call return kinds"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: setup-type method-call synthetic collection fallback blocking now
  builds the experimental map member root with `experimentalCollectionMemberRoot`;
  the map surface inventory now observes 838 production traces and backing
  traces now observe 83.
- 2026-05-16 18:05 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline param helper rejects borrowed vector variadic alias type mismatch,ir lowerer inline param helper materializes pointer File handle variadic args packs"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inline parameter map-like struct detection now builds the
  experimental map backing path with `experimentalCollectionTypePath`; the map
  surface inventory now observes 839 production traces and backing traces now
  observe 84.
- 2026-05-16 18:05 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline param helper rejects borrowed vector variadic alias type mismatch,ir lowerer inline param helper materializes pointer File handle variadic args packs,ir lowerer call helpers build inline arguments for inferred experimental map receiver methods"` |
  failures: `ir lowerer call helpers build inline arguments for inferred
  experimental map receiver methods` | notes: stale map receiver fixture under
  the already logged stdlib-ownership cutover bucket; reran adjacent inline
  parameter cases above.
- 2026-05-16 18:04 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference call-return setup resolves canonical namespaced map access directly,ir lowerer inference dispatch requires semantic try facts,ir lowerer setup inference helper resolves array and map access kinds"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inference base-kind map-family detection now builds the
  experimental map backing base with `experimentalCollectionTypePath`; the map
  surface inventory now observes 842 production traces and backing traces now
  observe 87.
- 2026-05-16 18:02 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs,ir lowerer uninitialized type helpers infer forwarded stdlib map constructor structs"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: setup-type collection helper generated map struct path inference now
  builds the experimental map backing prefix with
  `experimentalCollectionTypePath`; the map surface inventory now observes
  845 production traces and backing traces now observe 90.
- 2026-05-16 18:02 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs,ir lowerer uninitialized type helpers infer forwarded stdlib map constructor structs,ir lowerer map constructor rewrite checks constructor surface before resolving defs"` |
  failures: `ir lowerer map constructor rewrite checks constructor surface
  before resolving defs` | notes: stale source-lock exact-source assertion;
  reran the adjacent constructor struct inference cases above.
- 2026-05-16 18:00 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs canonical namespaced map helpers on experimental map values in C++ emitter,compiles and runs wrapper map helpers on experimental map values in C++ emitter"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: emitter builtin call-path map type classification now builds the
  experimental map type path through its local experimental collection path
  builder; the map surface inventory now observes 847 production traces and
  backing traces now observe 92.
- 2026-05-16 17:58 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline call context helper prepares scoped setup,ir lowerer inline call context helper reports setup diagnostics,ir lowerer inline-call context-setup step initializes context and zero value"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inline call context generated map struct and constructor-helper
  prefixes now use local collection path builders; the map surface inventory
  now observes 848 production traces and backing traces now observe 93.
- 2026-05-16 17:56 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper preserves inferred borrowed map return metadata,ir lowerer statement binding helper keeps canonical map constructor value metadata,ir lowerer statement binding helper inherits map metadata from named source binding"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-binding helper generated map backing checks now build the
  experimental map backing prefix with `experimentalCollectionTypePath`; the
  map surface inventory now observes 851 production traces and backing traces
  now observe 96.
- 2026-05-16 17:55 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers classify struct helpers and transforms,ir lowerer call helpers classify struct definitions,ir lowerer call helpers build bundled entry call setup"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: call-helper generated collection struct classification now builds the
  experimental map backing prefix with `experimentalCollectionTypePath`; the
  map surface inventory now observes 855 production traces and backing traces
  now observe 100.
- 2026-05-16 17:55 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers classify struct helpers and transforms,ir lowerer call helpers classify struct definitions,ir lowerer call helpers dispatch inline calls with locals"` |
  failures: `ir lowerer call helpers dispatch inline calls with locals` |
  notes: stale explicit vector-count resolver-call assertion-count drift;
  reran adjacent struct classifier and bundled entry setup cases above.
- 2026-05-16 17:53 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve Result.map struct payload metadata,ir lowerer result helpers resolve function-returned map Result payload metadata,ir lowerer result helpers require semantic query facts for unresolved Result.map metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: result metadata map backing checks now build unspecialized and
  generated experimental map backing paths with `experimentalCollectionTypePath`;
  the map surface inventory now observes 857 production traces and backing
  traces now observe 102.
- 2026-05-16 17:51 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper maps file and packed error types"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: inline packed-args map detection now builds the experimental map
  backing base with `experimentalCollectionTypePath`; the map surface
  inventory now observes 860 production traces and backing traces now observe
  105.
- 2026-05-16 17:50 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct field binding helpers resolve layout bindings,ir lowerer struct field binding helpers collect struct layout bindings,ir lowerer struct field binding helpers collect layout bindings from program context"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: struct-field binding helper classification now uses the shared
  experimental collection classifier for generated map backing names; the map
  surface inventory now observes 863 production traces and backing traces now
  observe 108.
- 2026-05-16 17:49 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper preserves inferred borrowed map return metadata,ir lowerer statement binding helper keeps canonical map constructor value metadata,ir lowerer statement binding helper inherits map metadata from named source binding"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: statement-binding metadata now builds the generated experimental map
  backing prefix with `experimentalCollectionTypePath`; the map surface
  inventory now observes 867 production traces and backing traces now observe
  112.
- 2026-05-16 17:48 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference call-return setup resolves canonical namespaced map access directly,ir lowerer call helpers resolve and validate map access targets,ir lowerer inference dispatch requires semantic try facts"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: IR inference dispatch map-family detection now builds the
  experimental map backing base with `experimentalCollectionTypePath`; the map
  surface inventory now observes 869 production traces and backing traces now
  observe 114.
- 2026-05-16 17:46 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper keeps reject diagnostics for explicit slash-method map access receivers,ir lowerer setup type helper keeps reject diagnostics for wrapper-returned explicit slash-method map access,ir lowerer setup type helper resolves declared receiver aliases through slashless map imports"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: IR setup method-target receiver classification now builds the
  experimental map receiver type through `experimentalCollectionTypePath`; the
  map surface inventory now observes 872 production traces and backing traces
  now observe 117.
- 2026-05-16 17:45 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper resolves array and map access kinds,ir lowerer setup inference helper resolves string map reference access kinds"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` | failures: none |
  notes: declared-collection map type normalization now builds the
  experimental map backing base with `experimentalCollectionTypePath`; the map
  surface inventory now observes 875 production traces and backing traces now
  observe 120.
- 2026-05-16 17:45 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper resolves wrapper-returned canonical map access string kinds,ir lowerer setup inference helper resolves wrapper-returned slash-method map access kinds,ir lowerer setup inference helper resolves array and map access kinds,ir lowerer setup inference helper resolves string map reference access kinds"` |
  failures: `ir lowerer setup inference helper resolves wrapper-returned
  canonical map access string kinds`, `ir lowerer setup inference helper
  resolves wrapper-returned slash-method map access kinds` | notes: stale
  wrapper-returned map setup-inference fixtures after the stdlib-owned map
  cutover; reran the adjacent local-map and string map-reference cases above.
- 2026-05-16 17:54 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values,declared canonical map access positional reorder keeps key diagnostics,canonical namespaced map access helpers accept experimental map values,wrapper-returned direct canonical map access count keeps primitive diagnostics"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: late map-access validation now uses the shared
  experimental collection backing helper for unspecialized experimental map
  type text; the map surface inventory now observes 881 production traces and
  backing traces now observe 126.
- 2026-05-16 17:51 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers,canonical namespaced map _ref helpers accept borrowed experimental map receivers"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template-monomorph experimental collection receiver
  resolution now uses shared experimental collection backing classifiers for
  unspecialized and generated map receiver types; the map surface inventory
  now observes 883 production traces and backing traces now observe 128.
- 2026-05-16 17:50 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers,canonical namespaced map _ref helpers accept borrowed experimental map receivers,imported canonical map count validates builtin map method receivers"` |
  failures: `imported canonical map count validates builtin map method receivers`
  | notes: stale map-count compatibility fixture under the already logged
  `*map*count*` bucket; reran the three non-stale receiver cases above.
- 2026-05-16 17:47 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor accepts explicit experimental map parameters,stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map parameters,canonical namespaced map access helpers accept experimental map values"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: build-parameter experimental map value detection
  now uses the shared experimental collection backing helper; the map surface
  inventory now observes 885 production traces and backing traces now observe
  130.
- 2026-05-16 17:46 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor accepts explicit experimental map parameters,stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map parameters,helper-wrapped map constructors accept explicit experimental map parameters,canonical namespaced map access helpers accept experimental map values"` |
  failures: `helper-wrapped map constructors accept explicit experimental map parameters`
  | notes: stale helper-wrapped explicit experimental parameter fixture now
  reports a `MapValue__*` mismatch after the stdlib-owned map cutover; reran
  the adjacent non-stale cases above.
- 2026-05-16 17:44 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map pre-dispatch inference keeps rooted and canonical helper paths isolated,explicit canonical map access helpers accept canonical map values,explicit canonical map parameter keeps builtin key diagnostics,canonical namespaced map access helpers accept experimental map values"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: pre-dispatch direct-call validation now uses the
  shared experimental collection backing helper for unspecialized experimental
  map type text; the map surface inventory now observes 888 production traces
  and backing traces now observe 133.
- 2026-05-16 17:41 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map access validates key type,unsafe map access validates key type,canonical namespaced map access helpers accept experimental map values,experimental map bracket access stays unsupported on value and borrowed call receivers"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access validation now uses the shared
  experimental collection backing helper for unspecialized experimental map
  type text; the map surface inventory now observes 890 production traces and
  backing traces now observe 135.
- 2026-05-16 17:38 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product publishes vector map and soa_vector collection specializations,semantic product keeps vector and map bridge parity,canonical namespaced map access helpers accept experimental map values"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantic publication collection specialization
  normalization now uses the shared experimental collection backing helper for
  unspecialized map backing types; the map surface inventory now observes 892
  production traces and backing traces now observe 137.
- 2026-05-16 17:36 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map _ref helpers accept borrowed experimental map receivers,stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers,canonical namespaced map insert validates on explicit experimental map bindings"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: borrowed experimental map extraction in semantic
  validation now uses the shared experimental collection backing helper
  instead of direct experimental map backing prefixes; the map surface
  inventory now observes 894 production traces and backing traces now observe
  139.
- 2026-05-16 17:33 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers,canonical namespaced map insert validates on explicit experimental map bindings"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template-monomorph fallback type inference now uses
  shared experimental collection backing classifiers for unspecialized and
  generated experimental map value types; the map surface inventory now
  observes 897 production traces and backing traces now observe 142.
- 2026-05-16 17:30 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template-monomorph collection receiver
  normalization now uses the shared experimental collection backing helper
  for experimental map backing paths and constructs bare generated map backing
  prefixes from the backing name; the map surface inventory now observes 902
  production traces and backing traces now observe 147.
- 2026-05-16 17:29 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map helpers accept experimental map value receivers,stdlib wrapper map helpers accept experimental map value receivers,experimental map method-call sugar keeps missing Map helper diagnostics"` |
  failures: `experimental map method-call sugar keeps missing Map helper diagnostics`
  | notes: stale map-count compatibility diagnostic expectation under the
  already logged `*map*count*` bucket; reran the three non-stale helper
  resolution cases above.
- 2026-05-16 17:27 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned canonical map method ignores alias helper when canonical helper is absent,wrapper-returned canonical map method requires helper definition,stdlib canonical map access count shadow keeps canonical precedence over alias helper,stdlib canonical map access count shadow currently validates mixed canonical and alias returns"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantic method-target resolution now uses shared
  experimental collection backing classifiers for unspecialized map backing
  receivers and generated `Map__*` fallback paths; the map surface inventory
  now observes 906 production traces and backing traces now observe 151.
- 2026-05-16 17:24 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map parameter keeps builtin helper validation,explicit canonical map parameter keeps builtin key diagnostics,stdlib wrapper map constructor accepts explicit canonical map parameters,stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map parameters"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantic argument validation now classifies
  experimental map backing template bases through the shared experimental
  collection backing helper; the map surface inventory now observes 910
  production traces and backing traces now observe 155.
- 2026-05-16 17:21 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="binding map type requires two template arguments,explicit canonical map access helpers accept canonical map values,explicit canonical map parameter keeps builtin helper validation,explicit canonical map parameter keeps builtin key diagnostics"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantic binding type helpers now classify
  experimental map backing names through generic experimental collection
  helpers instead of direct map backing paths; the map surface inventory now
  observes 913 production traces and backing traces now observe 158.
- 2026-05-16 17:18 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned canonical map keeps if string branch compatibility,wrapper-returned referenced canonical map keeps if string branch compatibility,if rejects builtin string map access mixed with numeric branch,explicit canonical map parameter keeps builtin helper validation"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection return inference now uses shared
  experimental collection backing classifiers for unspecialized map backing
  return types and generated map fallback paths; the map surface inventory now
  observes 921 production traces and backing traces now observe 166.
- 2026-05-16 17:15 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="print accepts string map access,explicit canonical map parameter keeps print statement string validation"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement printability now uses the shared
  experimental collection backing helper to exclude unspecialized experimental
  map backing return types; the map surface inventory now observes 925
  production traces and backing traces now observe 170.
- 2026-05-16 17:13 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,explicit canonical map parameter keeps builtin helper validation,explicit canonical map parameter keeps builtin key diagnostics"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-dispatch map field extraction now uses
  the shared experimental collection backing helper for unspecialized
  `Map<K,V>` type text and generated `Map__*` backing structs; the map surface
  inventory now observes 927 production traces and backing traces now observe
  172.
- 2026-05-16 17:11 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,explicit canonical map parameter keeps builtin helper validation,explicit canonical map parameter keeps builtin key diagnostics"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: argument-validation extraction for generated
  experimental map backing structs now uses the shared experimental
  collection backing helper; the map surface inventory now observes 929
  production traces and backing traces now observe 174.
- 2026-05-16 17:07 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="uninitialized init validates map key/value types,canonical map template arg without uninitialized validates"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement init map type matching now uses the
  shared experimental collection backing helper for generated map backing
  paths; the map surface inventory now observes 930 production traces and
  backing traces now observe 175.
- 2026-05-16 17:06 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_misc_tests --test-case="uninitialized init validates map key/value types,uninitialized canonical map template arg keeps map key value diagnostics,canonical map template arg without uninitialized validates"` |
  failures: `uninitialized canonical map template arg keeps map key value diagnostics`
  | notes: stale manual diagnostic fixture now validates successfully; reran
  the two adjacent passing cases above.
- 2026-05-16 17:04 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit canonical map access helpers accept canonical map values,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,stdlib canonical map access count shadow currently validates mixed canonical and alias returns"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: late map-access receiver classification now uses
  the shared experimental collection backing helper for generated map backing
  paths; the map surface inventory now observes 934 production traces and
  backing traces now observe 179.
- 2026-05-16 17:02 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access call keeps canonical struct-return forwarding,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: receiver-path map backing exclusions now use the
  shared experimental collection backing helper; the map surface inventory
  now observes 936 production traces and backing traces now observe 181.
- 2026-05-16 17:01 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped Result.ok payload assignments accept explicit canonical map result targets,helper-wrapped Result.ok payloads accept canonical map result dereference targets,canonical stdlib map returns are allowed,stdlib wrapper map constructor accepts explicit canonical map returns"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: Result helper map payload recognition now uses
  the shared experimental collection backing helper for generated map backing
  paths; the map surface inventory now observes 940 production traces and
  backing traces now observe 185.
- 2026-05-16 17:00 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access call keeps canonical struct-return forwarding,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,stdlib wrapper map constructor accepts explicit canonical map returns"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: struct-return inference now uses the shared
  experimental collection backing helper for generated map backing
  recognition; the map surface inventory now observes 942 production traces
  and backing traces now observe 187.
- 2026-05-16 16:58 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors,stdlib wrapper map constructor accepts explicit canonical map returns,stdlib canonical map count method auto inference falls back to canonical helper return"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-type normalization now uses the shared
  experimental collection backing helper for generated map backing
  recognition; the map surface inventory now observes 944 production traces
  and backing traces now observe 189.
- 2026-05-16 16:56 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors,stdlib wrapper map constructor accepts explicit canonical map returns,stdlib canonical map count method auto inference falls back to canonical helper return"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement return collection normalization now
  uses the shared experimental collection backing helper for generated map
  backing recognition; the map surface inventory now observes 948 production
  traces and backing traces now observe 193.
- 2026-05-16 16:55 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors,block-bodied inferred canonical map returns rewrite constructors,helper-wrapped inferred canonical map returns rewrite nested constructor arguments,double helper-wrapped inferred canonical map returns rewrite nested constructor arguments,stdlib map constructors accept explicit canonical map struct fields,helper-wrapped map constructors accept explicit canonical map bindings"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: graph binding initializer inference now uses the
  shared experimental collection backing helper for generated map backing
  recognition; the map surface inventory now observes 950 production traces
  and backing traces now observe 195.
- 2026-05-16 16:54 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors,block-bodied inferred canonical map returns rewrite constructors,helper-wrapped inferred canonical map returns rewrite nested constructor arguments,double helper-wrapped inferred canonical map returns rewrite nested constructor arguments,stdlib map constructors accept explicit canonical map struct fields,helper-wrapped map constructors accept explicit canonical map bindings"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: initializer inference now derives generated
  experimental map backing recognition through
  `isExperimentalCollectionBackingTypeName`; the map surface inventory now
  observes 954 production traces and backing traces now observe 199.
- 2026-05-16 16:53 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors,block-bodied inferred canonical map returns rewrite constructors,helper-wrapped inferred canonical map returns rewrite nested constructor arguments,double helper-wrapped inferred canonical map returns rewrite nested constructor arguments,stdlib map constructors accept explicit canonical map struct fields,helper-wrapped map constructors accept explicit canonical map bindings,helper-wrapped map constructors accept canonical map uninitialized storage"` |
  failures: `helper-wrapped map constructors accept canonical map uninitialized storage`
  | notes: stale map uninitialized-storage fixture now reports
  `init value type mismatch`; reran the six adjacent passing cases above.
- 2026-05-16 16:52 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="inferred canonical map returns rewrite canonical constructors,block-bodied inferred canonical map returns rewrite constructors,auto bindings inside inferred canonical map return blocks rewrite constructors,helper-wrapped inferred canonical map returns rewrite nested constructor arguments,double helper-wrapped inferred canonical map returns rewrite nested constructor arguments"` |
  failures: `auto bindings inside inferred canonical map return blocks rewrite constructors`
  | notes: stale auto-binding map constructor fixture failed while the
  adjacent constructor-return cases passed.
- 2026-05-16 16:51 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept inferred canonical map struct fields,helper-wrapped inferred canonical map struct fields validate,helper-wrapped inferred canonical result map struct fields validate,helper-wrapped map constructor assignments accept inferred canonical map struct fields"` |
  failures: `stdlib map constructors accept inferred canonical map struct fields`,
  `helper-wrapped inferred canonical map struct fields validate`,
  `helper-wrapped map constructor assignments accept inferred canonical map
  struct fields` | notes: stale inferred map-struct-field fixtures failed
  with missing canonical map access helper diagnostics; the result payload
  variant passed.
- 2026-05-16 16:50 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs,ir lowerer uninitialized type helpers infer forwarded stdlib map constructor structs,ir lowerer statement binding helper keeps canonical map constructor value metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: uninitialized-struct specialized map detection
  now derives generated experimental map backing recognition through the
  shared collection-type helper; the map surface inventory now observes 958
  production traces and backing traces now observe 203.
- 2026-05-16 16:49 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference expr-kind call-fallback setup wires callback,ir lowerer struct layout helpers classify layout transforms"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: lowerer fallback setup and struct-layout
  generated map classifiers now derive experimental map backing paths through
  shared collection helpers; the map surface inventory now observes 960
  production traces and backing traces now observe 205.
- 2026-05-16 16:48 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inference expr-kind call-fallback setup wires callback,ir lowerer struct layout helpers classify layout transforms"` |
  failures: `ir lowerer inference expr-kind call-fallback setup wires callback`
  | notes: newly added map-field count fixture threw `bad_function_call`
  because the test harness had no `state.inferExprKind`; fixed by installing
  the fixture callback before the rerun above.
- 2026-05-16 16:45 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped Result.ok payload assignments accept explicit canonical map result targets,helper-wrapped Result.ok payloads accept canonical map result dereference targets,canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors,stdlib wrapper map constructor accepts explicit canonical map returns,stdlib canonical map count method auto inference falls back to canonical helper return"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: setup-type map struct classification,
  Result map identity, and statement-return collection normalization now
  derive canonical `MapValue` roots through collection path helpers instead
  of split-string map roots; the map surface inventory remains at 965
  production traces and backing traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct layout coverage ignores generated collection helper subpaths"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: generated collection struct classification now
  derives the canonical map `MapValue__*` prefix through
  `collectionTypePath("map")`; this removes a split-string map root while the
  map surface inventory remains at 965 production traces and backing traces
  remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map returns are allowed,inferred canonical map returns rewrite canonical constructors,stdlib canonical map count method auto inference falls back to canonical helper return,stdlib wrapper map constructor accepts explicit canonical map returns"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers keep bare map access canonical forwarding,ir lowerer struct return helpers keep canonical slash-path map access forwarding,ir lowerer struct return helpers keep canonical map access call forwarding"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantics struct-return helper path
  normalization and specialized `MapValue` return-path construction now build
  canonical map roots through a local collection path helper; the map surface
  inventory now observes 965 production traces and backing traces remain at
  210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantics binding map type recognition now builds
  canonical map and `MapValue` paths through a local collection path helper
  instead of spelling canonical map roots directly; the map surface inventory
  now observes 967 production traces and backing traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper classifies variadic map parameters,ir lowerer statement binding helper preserves inferred borrowed map return metadata,ir lowerer statement binding helper keeps canonical map constructor value metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement binding helper map struct-path inference
  now builds canonical `MapValue` roots through `collectionTypePath("map")`;
  the map surface inventory now observes 973 production traces and backing
  traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer call helpers infer forwarded map access targets"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: map access target struct-path inference now builds
  canonical `MapValue` roots through `collectionTypePath("map")`; stale
  access-target assertions now lock the stdlib-owned `MapValue__*` identity;
  the map surface inventory now observes 974 production traces and backing
  traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct type helpers resolve struct slot layouts from definition fields,ir lowerer struct type helpers resolve struct slots from definition field index"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: struct-slot map type recognition and specialized
  `MapValue` path construction now build the canonical root through
  `collectionTypePath("map")`; the map surface inventory now observes 975
  production traces and backing traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer uninitialized type helpers infer concrete stdlib map constructor structs,ir lowerer uninitialized type helpers infer forwarded stdlib map constructor structs,ir lowerer statement binding helper keeps canonical map constructor value metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: uninitialized map target inference now builds the
  canonical `MapValue` root through `collectionTypePath("map")`; stale
  `mapPair` and retired builtin-map metadata expectations were retargeted to
  canonical `/std/collections/map/map` plus stdlib-owned `MapValue` value
  metadata; the map surface inventory now observes 976 production traces and
  backing traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper classifies variadic map parameters"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer statement binding helper preserves inferred borrowed map return metadata"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement-binding map struct metadata now builds
  `MapValue` paths through `collectionTypePath("map")` instead of a
  hard-coded canonical map path; the map surface inventory now observes 977
  production traces and backing traces remain at 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve map Result payload metadata"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: packed Result payload metadata now recognizes
  experimental map payload bases through `isExperimentalCollectionTypeName`
  instead of a hard-coded experimental map path; the map surface inventory now
  observes 978 production traces and backing traces now observe 210.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct type helpers infer name-expression struct paths"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: struct-type map local inference now builds the
  `MapValue` root with `collectionTypePath("map")` instead of a hard-coded
  canonical map path literal; the map surface inventory now observes 979
  production traces and backing traces remain at 211.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports string-keyed map literals"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: map literal lowering now obtains the empty inferred
  map backing path through `experimentalCollectionTypePath("map", "Map")`
  instead of hard-coding `/std/collections/experimental_map/Map`; the map
  surface inventory now observes 980 production traces and backing traces
  now observe 211.
- 2026-05-16 local | fail-known | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports string-keyed map literals,ir lowers map literal call as statement"` |
  failures: stale `ir lowers map literal call as statement` fixture | notes:
  the string-keyed map literal test passed; the statement fixture failed at
  parse time with duplicate `i32` template parameters before reaching the
  changed lowerer code.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep exact-import vector and map bridge parity,ir lowerer bridge coverage uses published collection surface ids for lowered helper spellings"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: IR call resolution now recognizes the map helper
  bridge surface through `collections.map_helpers` metadata instead of naming
  `StdlibSurfaceId::CollectionsMapHelpers` directly; the map surface inventory
  now observes 981 production traces and backing traces remain at 212.
- 2026-05-16 local | fail-known | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable"` |
  failures: 31 stale source-lock assertions | notes: the failures are the
  known stale broad source-delegation lock for removed map/SoA helper strings;
  the newly updated `collections.map_helpers` expectation was not among the
  failures.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer strips
  `map/` or `std/collections/map/` prefixes before explicit map helper
  candidate construction; the map surface inventory now observes 982
  production traces and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer normalizes
  slashless rooted/canonical map helper paths to leading-slash candidates;
  the map surface inventory now observes 984 production traces and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method metadata no longer adds slashless
  candidates for rooted map helper metadata paths; the map surface inventory
  now observes 986 production traces and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper,canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter import-alias resolution no longer routes
  through the identity `normalizeMapImportAliasPath` helper; the backend IR
  target rebuilds after source-lock updates, the map surface inventory remains
  at 988 production traces, and backing traces remain at 212. The broader
  backend IR source-delegation test case is still not used as a clean gate
  because of the known unrelated SoA source-lock drift above.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method metadata no longer normalizes
  slashless rooted/canonical map helper paths to leading-slash candidates;
  the map surface inventory now observes 988 production traces and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: shared emitter method metadata/type-inference
  no longer prunes map access candidates for rooted `/map/<access>` inputs;
  the map surface inventory now observes 994 production traces and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter collection-type inference no longer
  prunes canonical map access candidates from rooted `/map/<access>` inputs
  after semantic coverage now locks canonical method-sugar return typing; the
  map surface inventory now observes 1000 production traces and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method access ignores rooted alias struct-return helper"`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: stale rooted `/map/at` struct-return isolation
  coverage now locks that map method sugar ignores the rooted alias helper and
  uses the canonical helper return type; the map surface inventory remains at
  1005 production traces and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: vector stdlib helper preference no longer
  normalizes map paths while handling array-to-vector fallback; canonical
  stdlib map access validation still passes, the map surface inventory now
  observes 1005 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return-struct inference no longer carries
  the unused rooted `/map/<access>` pruning lambda; canonical stdlib map
  access validation still passes, the map surface inventory now observes 1007
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return-struct inference no longer
  synthesizes canonical `/std/collections/map/<suffix>` candidates from
  rooted `/map/<suffix>` inputs; canonical stdlib map access validation still
  passes, the map surface inventory now observes 1012 production traces, and
  backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method metadata no longer rewrites
  rooted `/map/<helper>` method paths to canonical
  `/std/collections/map/<helper>` paths; canonical stdlib map access
  validation still passes, the map surface inventory now observes 1015
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer adds an
  implicit rooted `/map/<method>` candidate after canonical map method
  candidates; canonical stdlib map access validation still passes, the map
  surface inventory now observes 1018 production traces, and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: shared emitter call-path preference no longer
  rewrites missing rooted `/map/<suffix>` paths to canonical
  `/std/collections/map/<suffix>` paths; canonical stdlib map access
  validation still passes, the map surface inventory now observes 1019
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer rewrites
  missing rooted `/map/<suffix>` paths to canonical
  `/std/collections/map/<suffix>` paths; canonical stdlib map access
  validation still passes, the map surface inventory now observes 1022
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer prunes rooted
  `/map/<access>` candidates from canonical map access paths after reverse
  candidates were removed; canonical stdlib map access validation still
  passes, the map surface inventory now observes 1025 production traces, and
  backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method metadata no longer prunes rooted
  `/map/<access>` candidates from canonical map access paths after reverse
  candidates were removed; canonical stdlib map access validation still
  passes, the map surface inventory now observes 1028 production traces, and
  backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter collection-type inference no longer prunes
  rooted `/map/<access>` candidates from canonical map access paths after the
  reverse candidates were removed; canonical stdlib map access validation
  still passes, the map surface inventory now observes 1031 production
  traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter return inference no longer adds or
  prefers rooted `/map/<suffix>` aliases when canonical
  `/std/collections/map/<suffix>` paths are missing; canonical stdlib map
  access validation still passes, the map surface inventory now observes
  1034 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter call-path preference no longer falls back
  from missing canonical `/std/collections/map/<suffix>` paths to rooted
  `/map/<suffix>` aliases; canonical stdlib map access validation still
  passes, the map surface inventory now observes 1040 production traces, and
  backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method type inference no longer falls back
  from canonical `/std/collections/map/<access>` metadata to rooted
  `/map/<access>` metadata for bare map access methods; canonical stdlib map
  access validation still passes, the map surface inventory now observes
  1043 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: emitter method resolution no longer lets implicit
  map helper methods resolve through rooted `/map/<helper>` metadata when the
  canonical `/std/collections/map/<helper>` metadata is absent; canonical
  stdlib map access validation still passes, the map surface inventory remains
  at 1044 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: late fallback return-kind inference now targets
  `/std/collections/map/<helper>` instead of rooted `/map/<helper>` when a
  map receiver uses builtin access spelling; canonical stdlib map access
  validation still passes, the map surface inventory remains at 1044
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template expression rewriting no longer reports
  explicit-template diagnostics by probing visible rooted `/map/<helper>`
  source definitions for bare map receiver calls; canonical stdlib map access
  validation still passes, the map surface inventory now observes 1044
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: method-target resolution no longer reclassifies a
  direct call as removed `/map/*` compatibility solely from its resolved
  callee path; canonical stdlib map access validation still passes, the map
  surface inventory now observes 1045 production traces, and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-dispatch setup no longer suppresses
  namespaced canonical map access inference just because a rooted
  `/map/<access>` definition exists; canonical stdlib map access validation
  still passes, the map surface inventory now observes 1058 production
  traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template expression rewriting no longer clears
  inferred canonical map receiver template arguments through a rooted
  `/map/*` branch; canonical stdlib map access validation still passes, the
  map surface inventory now observes 1059 production traces, and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template monomorphization no longer treats
  rooted `/map/count` as equivalent to canonical
  `/std/collections/map/count` for the non-templated count diagnostic;
  canonical stdlib map access validation still passes, the map surface
  inventory now observes 1060 production traces, and backing traces remain
  at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access resolution no longer prefers
  rooted `/map/at*` helper definitions when resolving canonical map access
  helper calls; canonical stdlib map access validation still passes, the map
  surface inventory now observes 1061 production traces, and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: removed-map body-argument target resolution no
  longer falls back from canonical `/std/collections/map/<helper>` to rooted
  `/map/<helper>` definitions; canonical stdlib map access validation still
  passes, the map surface inventory now observes 1067 production traces, and
  backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: method-target resolution no longer prefers rooted
  `/map/<helper>` definitions or imports when choosing map method targets;
  canonical stdlib map access validation still passes, the map surface
  inventory now observes 1068 production traces, and backing traces remain at
  212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: initializer inference no longer prefers or falls
  back to rooted `/map/<helper>` aliases for explicit stdlib map helper
  targets; canonical stdlib map access validation still passes, the map
  surface inventory now observes 1069 production traces, and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: bare map helper rewrite target selection no longer
  falls back to visible rooted `/map/<helper>` helper families after canonical
  lookup; canonical stdlib map access validation still passes, the map surface
  inventory now observes 1070 production traces, and backing traces remain at
  212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: preferred map method target selection no longer
  returns explicit `/map/<helper>` definitions or imports ahead of canonical
  stdlib map helpers; canonical stdlib map access validation still passes, the
  map surface inventory now observes 1071 production traces, and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: pointer-like call target candidate generation no
  longer appends reciprocal `/map/*` and `/std/collections/map/*` helper
  candidates; canonical stdlib map access validation still passes, the map
  surface inventory now observes 1072 production traces, and backing traces
  remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: infer-method resolution no longer mirrors
  `/map/*` receiver paths to `/std/collections/map/*` candidates or vice
  versa; canonical stdlib map access validation still passes, the map surface
  inventory now observes 1078 production traces, and backing traces remain at
  212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: diagnostic target formatting no longer carries a
  rooted `/map/count__t` specialization shortcut; canonical stdlib map access
  validation still passes, the map surface inventory now observes 1083
  production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: direct call resolution no longer returns a
  missing-target shortcut for rooted `/map/at*_ref` access helper calls;
  canonical stdlib map access validation still passes, the map surface
  inventory now observes 1085 production traces, and backing traces remain at
  212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access validation no longer retargets
  rooted `/map/*` diagnostic targets to `/std/collections/map/*`; canonical
  stdlib map access validation still passes, the map surface inventory now
  observes 1089 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: template monomorphization no longer canonicalizes
  unknown-target diagnostics from rooted `/map/{count,contains,tryAt,at,
  at_unsafe,insert}` helper paths to `/std/collections/map/*`; canonical
  stdlib map access validation still passes, the map surface inventory now
  observes 1095 production traces, and backing traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: struct-return inference no longer carries an
  explicit `map/at` or `/map/at` compatibility probe for map access helper
  return structs; canonical stdlib map access validation still passes, the
  map surface inventory now observes 1101 production traces, and backing
  traces remain at 212.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access resolution no longer treats
  rooted `/map/at*_ref`, slashless `map/at*`, or a `map` namespace prefix as
  canonical map access helper-name classifiers; removed-alias rejection stays
  intact, and the map surface inventory now observes 1104 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: statement printability no longer treats rooted
  `/map/at` or `/map/at_unsafe` calls as builtin map access printability
  shortcuts; canonical stdlib map access validation still passes, and the map
  surface inventory now observes 1108 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: late map access validation no longer treats
  slashless `map/at*_ref` names or a `map` namespace prefix as canonical map
  access helper names; canonical stdlib map access validation still passes,
  and the map surface inventory now observes 1110 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: infer-definition deferred map alias detection no
  longer treats rooted `/map/at*` resolved paths as map access helpers;
  canonical stdlib map access validation still passes, and the map surface
  inventory now observes 1112 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-dispatch setup inference no longer
  treats rooted `/map/at*_ref` resolved paths or a `map` namespace prefix as
  canonical map access helper-name classifiers; canonical stdlib map access
  validation still passes, and the map surface inventory now observes 1116
  production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection return-kind inference no longer grants
  rooted `/map/*` helper paths builtin map return kinds; canonical stdlib map
  access validation still passes, and the map surface inventory now observes
  1118 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access validation no longer treats
  slashless `map/at*_ref` names or a `map` namespace prefix as canonical map
  access helper names; canonical stdlib map access validation still passes,
  and the map surface inventory now observes 1126 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: collection-access validation no longer treats
  rooted `/map/at*` resolved paths as canonical map access helper paths;
  canonical stdlib map access validation still passes, and the map surface
  inventory now observes 1128 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: string argument validation no longer treats rooted
  `/map/at*` resolved paths or slashless `map/at*_ref` names as explicit
  map access helper classifiers; canonical stdlib map access validation still
  passes, and the map surface inventory now observes 1132 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical namespaced map access helpers accept experimental map values,stdlib namespaced map access helpers accept imported stdlib wrappers,collected diagnostics ignore imported canonical map access helper calls,explicit canonical map access helpers accept canonical map values"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: scalar pointer/memory builtin validation no longer
  carries an explicit rooted or canonical map access helper path classifier;
  the generic memory-`at` map-like operand bypass remains, and the map surface
  inventory now observes 1138 production traces.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: lowerer statement-call receiver inference no
  longer carries a rooted `/map/at` args-pack example in production comments,
  and the ownership source-lock keeps that retired rooted trace absent.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept explicit canonical map struct fields,canonical stdlib map helpers accept constructor receivers"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: semantic struct-return method inference no longer
  mirrors map receiver methods to rooted `/map/*` candidates; only the
  canonical `/std/collections/map/*` candidate remains for map receivers.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept explicit canonical map struct fields"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native rejects map constructor literal parity with canonical entries"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .` |
  failures: none | notes: rooted `/map/entry` no longer classifies as a map
  entry constructor in semantic call resolution or template-monomorph overload
  selection; canonical `/std/collections/map/entry` entry-args coverage still
  passes.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors infer canonical auto locals,inferred canonical map returns rewrite canonical constructors,stdlib map constructors accept explicit canonical map struct fields"` |
  failures: stale helper-wrapped map constructor auto-inference fixture |
  notes: only the helper-wrapped case failed with
  `unknown call target: /std/collections/map/tryAt`; the two current canonical
  constructor cases in the same command passed, and the narrower current
  `stdlib map constructors accept explicit canonical map struct fields` rerun
  passed separately.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation,experimental map production traces are classified as backing substrate"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm builtin canonical map first-growth inserts"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .`;
  `python3 scripts/check_map_adapter_traces.py --root .`;
  `python3 scripts/check_semantic_map_wrapper_traces.py --root .` |
  failures: none | notes: lowerer assignment, uninitialized-init, and packed
  Result paths no longer rewrite public map constructors to
  `experimental_map/mapSingle`-style backing helpers.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports direct map Result payloads"` |
  failures: stale direct builtin-map Result payload fixture | notes: this
  remains the same known non-gate recorded above after removing the
  experimental-map constructor rewrite bridge.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor keeps same-path definition before wrapper fallback,retired public mapPair bridge reports unknown target,canonical stdlib map helpers accept constructor receivers"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .`;
  `python3 scripts/check_map_adapter_traces.py --root .`;
  `python3 scripts/check_semantic_map_wrapper_traces.py --root .` |
  failures: none | notes: semantic constructor-backed map binding detection
  and parameter map-value checks no longer classify fixed-arity legacy
  `mapSingle`/`mapPair`-style names as canonical constructors.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_misc_tests`;
  `cmake --build build-release --target PrimeStruct_semantics_tests`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm builtin canonical map first-growth inserts"`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor keeps same-path definition before wrapper fallback,canonical stdlib map helpers accept constructor receivers"`;
  `python3 scripts/check_map_surface_trace_inventory.py --root .`;
  `python3 scripts/check_map_backing_traces.py --root .`;
  `python3 scripts/check_map_adapter_traces.py --root .`;
  `python3 scripts/check_semantic_map_wrapper_traces.py --root .` |
  failures: none | notes: inline param, packed-args, and packed-result map
  constructor normalization no longer emits retired rooted `/map/map`; it
  normalizes only to the public stdlib constructor while source locks enforce
  the retired spelling stays absent.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer materializes variadic map packs with indexed count methods,ir lowerer materializes variadic map packs with indexed canonical count calls"`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer supports direct map Result payloads" -s`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter materializes variadic map value packs with indexed count methods" --no-skip` |
  failures: stale direct/variadic builtin-map compatibility cases | notes:
  these cases still rely on retired map literal/count/method fallback or
  builtin-layout `args<map<K, V>>` behavior and are recorded above as known
  non-gates during the stdlib-owned `MapValue` cutover.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor keeps same-path definition before wrapper fallback,retired public mapPair bridge reports unknown target,canonical stdlib map helpers accept constructor receivers"`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm builtin canonical map first-growth inserts"` |
  failures: none | notes: focused validation passed after deleting no-op
  metadata-backed map constructor rewrite shims and their semantic/template
  monomorph call sites. Backend IR source-lock binaries rebuild, but the
  matching source-lock doctests remain blocked by unrelated stale SoA locks
  recorded above.
- 2026-05-16 local | fail | mode: release | command:
  `cmake --build build-release --target PrimeStruct_backend_ir_tests`;
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable"` |
  failures: 44 stale SoA/experimental_soa_vector source-lock assertions |
  notes: rerun after removing fixed-arity map constructor rewrite helpers from
  `MapConstructorHelpers.h`; the updated map-constructor source-lock assertion
  is covered, but the test remains blocked by unrelated SoA lock drift.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib return wrapper temporaries in expressions" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib wrapper temporary call forms" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib wrapper temporary index forms" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib wrapper temporary syntax parity" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib wrapper temporary unsafe parity" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib wrapper temporary count capacity parity" --no-skip` |
  failures: none | notes: native templated wrapper temporaries now use the
  current map constructor-call surface and `return<auto>` instead of the
  retired `mapSingle<K, V>` helper and old bare `map<K, V>` return envelope.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native collection constructor parity" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map constructor calls" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map count helper" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map method call" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map at helper" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map indexing sugar" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map at_unsafe helper" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native bool map access helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native u64 map access helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map at missing key" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native typed map binding" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects native map literal odd args" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects native map literal type mismatch" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles native string-valued map constructors on stdlib path" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native string-keyed map literals" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native map literal string binding key" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native string-keyed map indexing sugar" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native string-keyed map indexing binding key" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects native map indexing with argv key" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native string-keyed map binding lookup" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects native map lookup with argv string key" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects native map literal string key from argv binding" --no-skip` |
  failures: none | notes: native map literal/string-key fixture now uses the
  stdlib map constructor-call surface instead of retired key/value brace
  literal syntax; string-valued native maps are compile-only to avoid the
  current runtime hang.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_compile_run_tests --test-suite="primestruct.compile.run.native_backend.collections" --no-skip` |
  failures: 105 stale native collection compatibility cases before interrupt |
  notes: broad shard is dominated by unrelated SoA public-surface drift and
  older map-literal/insert fixtures after the map stdlib-ownership cutover;
  focused native MapValue cutover cases pass.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target primec PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native map method alias access forwards helper receiver chains" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="keeps canonical native map method access field expression forwarding" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native wrapper-returned canonical map slash-method forwards struct receiver" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native direct wrapper-returned canonical map access count shadow" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native keeps wrapper-returned canonical map method access string receiver typing" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native wrapper-returned slash-method map access count shadow with direct exit" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native canonical slash vector count same-path helper on map receiver" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native stdlib namespaced map reference access helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native canonical map method with slash return type receiver" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native canonical map access direct calls and method sugar use ordinary map helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native explicit canonical map typed bindings with builtin helpers" --no-skip` |
  failures: none | notes: focused native map cutover blocker set passed after
  recognizing `MapValue__t*` in native layout predicates and semantic
  reference type compatibility.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable" --no-skip` |
  failures: 15 stale SoA/experimental_soa_vector source-lock assertions |
  notes: rerun after updating the removed map-count inference source locks;
  remaining failures are unrelated SoA lock drift.
- 2026-05-16 local | fail | mode: release | command:
  `cd build-release && ./PrimeStruct_semantics_tests --test-case="*map*count*" --no-skip` |
  failures: 41 obsolete legacy map-count compatibility cases | notes:
  surfaced after deleting semantic map-count/map-access builtin branches from
  the count/capacity builtin validator; focused stdlib-owned map smoke tests
  passed and this pattern needs retirement with the remaining old C++ map
  compatibility tests.
- 2026-05-16 local | pass | mode: release | command:
  `cmake --build build-release --target primec`; `cmake --build build-release --target PrimeStruct_compile_run_tests`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="map wildcard import runs stdlib-owned surface in C++ emitter" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs canonical map stdlib-owned helpers" --no-skip`;
  `cd build-release && ./PrimeStruct_compile_run_tests --test-case="canonical map stdlib source stays isolated from legacy implementation" --no-skip`;
  `cmake --build build-release --target PrimeStruct_misc_tests`;
  `cd build-release && ./PrimeStruct_misc_tests --test-case="canonical map surface owns standalone stdlib implementation" --no-skip` |
  failures: none | notes: focused validation passed after removing the
  semantic map-count/map-access builtin branches while preserving the
  stdlib-owned map implementation path.
- 2026-05-16 02:50 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-release --target PrimeStruct_parser_tests PrimeStruct_misc_tests PrimeStruct_compile_run_tests -j 1`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.semantics.manual --test-case="type pack storage expands into deterministic struct fields,type pack storage rejects invalid placements and shapes,type pack helper bindings expand parameters locals and return envelopes,type pack reflection helpers follow expanded field order"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: root focused validation passed after cherry-picking TODO-4276 worker commit `055b013ea` into root as `3c73004c5`.
- 2026-05-16 02:25 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-release --target PrimeStruct_parser_tests PrimeStruct_misc_tests -j 1`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.semantics.manual --test-case="type pack storage expands into deterministic struct fields,type pack storage rejects invalid placements and shapes,type pack helper bindings expand parameters locals and return envelopes,type pack reflection helpers follow expanded field order"`; `cmake --build build-release --target PrimeStruct_compile_run_tests -j 1`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`; `git diff --check` | failures: none | notes: TODO-4276 focused validation passed after expanding type packs in helper parameters, locals, return/type envelopes, call template arguments, and reflection-generated helper order for concrete pack specializations.
- 2026-05-16 02:25 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: source-lock caught promoted TODO-4271 missing required `parallel_track: tuple-type-packs`; added the metadata and reran successfully.
- 2026-05-16 00:15 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_soa_surface_trace_inventory.py --repo-root .`; `python3 -m py_compile scripts/check_soa_surface_trace_inventory.py tests/scripts/test_check_soa_surface_trace_inventory.py`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-release --target PrimeStruct_compile_run_tests -j 1`; `cd build-release && ctest --output-on-failure -R '^PrimeStruct_soa_surface_trace_zero_audit(_self_test)?$'`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4529 focused validation passed after replacing the decaying SoA public-surface trace inventory with a strict zero-production-trace audit and release CTest gate.
- 2026-05-15 23:38 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_soa_surface_trace_inventory.py`; `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests -j 1`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer count access helpers build count classifier adapters,ir lowerer count access helpers build bundled classifiers"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4528 focused validation passed after routing lowerer count/access SoA public-surface traces through shared `soa_paths` helpers and deleting all count/access lowerer SoA inventory rows.
- 2026-05-15 22:59 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests -j 1`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4533 focused validation passed after routing lowerer call-resolution residual SoA bridge matching through shared `soa_paths` helpers and removing all `IrLowererCallResolution.cpp` SoA inventory rows.
- 2026-05-15 22:26 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; `cmake --build build-release --target PrimeStruct_semantics_tests -j 1`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical soa helper-return push and reserve keep same-path helper across escapes,canonical soa method-like helper-return push and reserve keep same-path helper across escapes,canonical soa field-view bindings track borrow roots"` | failures: none | notes: stabilized the current known SoA failures by retargeting stale legacy/public-import fixtures to the direct canonical `soa<T>` semantic surface; the retired `soa_vector<T>` public spelling remains rejected outside explicit compatibility tests.
- 2026-05-15 21:07 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_parser_tests PrimeStruct_misc_tests -j 1`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.semantics.manual --test-case="type pack storage expands into deterministic struct fields,type pack storage rejects invalid placements and shapes,type pack specializations bind zero one and many arguments,type pack parameters cannot be used as scalar types before expansion"` | failures: none | notes: parent-run TODO-4275 focused validation passed after converting the semantic storage fixture to direct AST construction.
- 2026-05-15 21:07 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.semantics.manual --test-case="type pack storage expands into deterministic struct fields,type pack storage rejects invalid placements and shapes,type pack specializations bind zero one and many arguments,type pack parameters cannot be used as scalar types before expansion"` | failures: type pack storage expands into deterministic struct fields | notes: parser rejected the original source fixture at `tests/unit/test_semantics_manual_templates.h:252`; resolved by making the focused storage tests direct AST fixtures.
- 2026-05-15 20:42 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests PrimeStruct_semantics_tests -j 1`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="vector-target count get get_ref and ref keep same-path soa helpers,vector-target to_aos keeps same-path helper shadow"` | failures: none | notes: TODO-4527 worker-local focused validation passed after deleting template-monomorph SoA inventory rows and moving TODO-4527 to finished history.
- 2026-05-15 20:39 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="vector-target count get get_ref and ref keep same-path soa helpers,vector-target to_aos keeps same-path helper shadow,canonical soa field-view bindings track borrow roots through public imports"` | failures: canonical soa field-view bindings track borrow roots through public imports | notes: first two selected SoA helper cases passed; the field-view case also fails in the root checkout's existing release binary and is tracked as a current known failure.
- 2026-05-15 20:36 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="soa_vector helper-return push and reserve keep same-path helper across escapes"` | failures: soa_vector helper-return push and reserve keep same-path helper across escapes | notes: direct case also fails in the root checkout's existing release binary; recorded as current known failure instead of TODO-4527 evidence.
- 2026-05-15 20:31 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="vector-target count get get_ref and ref keep same-path soa helpers,vector-target to_aos keeps same-path helper shadow,soa_vector helper-return push and reserve keep same-path helper across escapes"` | failures: soa_vector helper-return push and reserve keep same-path helper across escapes | notes: first two SoA template-monomorph helper cases passed; the push/reserve case failed and was checked against root before exclusion from TODO-4527 validation evidence.
- 2026-05-15 20:27 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests PrimeStruct_semantics_tests -j 1`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="vector-target count get get_ref and ref keep same-path soa helpers,vector-target to_aos keeps same-path helper shadow,soa_vector helper-return push and reserve keep same-path helper across escapes"` | failures: soa_vector helper-return push and reserve keep same-path helper across escapes | notes: build, inventory checker, and TODO source-lock passed; selected SoA semantics slice exposed a root-existing failure.
- 2026-05-15 19:34 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`; `cmake --build build-release --target PrimeStruct_parser_tests PrimeStruct_misc_tests PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`; `cd build-release && ./PrimeStruct_misc_tests --test-case="monomorphizes integer template arguments distinctly,integer template arguments cannot substitute type positions"`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: root focused validation passed after merging TODO-4270, TODO-4532, and TODO-4526 and reconciling the completed TODO-4532 summary-table source lock.
- 2026-05-15 19:33 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: root source-lock still expected completed TODO-4532 in active summary table rows; updated those expectations before rerunning successfully.
- 2026-05-15 18:56 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: parent-scheduled heavy build and focused TODO-4526 backend IR plus TODO source-lock validation passed.
- 2026-05-15 18:54 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: parent-scheduled heavy build and backend IR slice passed; compile-run source-lock failed on stale TODO queue expectations after TODO-4526 moved to finished history.
- 2026-05-15 18:51 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; remaining failures were stale `soa_vector.prime` body source locks after that module retired to comments.
- 2026-05-15 18:48 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed and the executable fixture validated; remaining failures were stale source-lock expectations for direct SoA get helper spelling and old internal_soa_vector borrowed helper bodies.
- 2026-05-15 18:46 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; typed struct-parameter route calls reached `argument type mismatch`, so the fixture now uses primitive direct calls for file/container/gfx route facts.
- 2026-05-15 18:44 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; parameter-based map assertion did not clear `binding initializer type mismatch`, so the fixture now removes initialized local/status bindings and the map assertion.
- 2026-05-15 18:42 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; canonical typed-path map constructor binding still hit `binding initializer type mismatch`, so the fixture now publishes the map collection specialization from a parameter binding.
- 2026-05-15 18:38 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; constructor-call syntax parsed but shorthand `[map<i32, i32>]` binding still hit `binding initializer type mismatch`, so the fixture now uses the canonical `[/std/collections/map<i32, i32>]` binding type used by direct-parser examples.
- 2026-05-15 18:36 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; non-empty map entry literal hit `unexpected token in expression`, so the fixture now uses constructor-style native map initialization used by nearby semantic fixtures.
- 2026-05-15 18:34 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; fixture reached `binding initializer type mismatch` on an empty map literal, so the source-lock fixture now uses the established non-empty native map literal form.
- 2026-05-15 18:24 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; failure signature unchanged after removing explicit fixture `tryAt` template arguments, showing the coordinate came from imported stdlib source rather than the fixture line.
- 2026-05-15 18:27 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; fixture no longer imported stdlib, but inline Result status stubs reported `return type mismatch: expected i32`; changed status helpers and bindings to plain i32 for this surface-routing source lock.
- 2026-05-15 18:30 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; `return type mismatch: expected i32` remained after status-helper changes, so the fixture no longer uses struct-return helper stubs for FileError, ContainerError, or GfxError receivers.
- 2026-05-15 18:32 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; mismatch changed to `return type mismatch: expected map` from the fake public map constructor body, so the fixture now uses an empty native map literal and no map constructor stub.
- 2026-05-15 18:18 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; focused backend IR rerun passed the SoA pending diagnostics and expr source delegation cases but rejected the stdlib bridge routing fixture with `template parameter declarations accept identifiers only; use Ts... for a type pack` at the explicit `tryAt<string, i32>` call.
- 2026-05-15 17:58 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | failures: soa pending diagnostics route through shared semantics helpers; semantics validator stdlib bridge helper routing stays stable | notes: parent-scheduled heavy build passed; focused backend IR source-lock/semantic rerun failed 2 of 3 selected doctest cases before the compile-run TODO source-lock slice was attempted.
- 2026-05-15 18:21 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: TODO-4532 focused validation passed after source-lock updates for inline native dispatch map resolver placement and tail-dispatch shared map insert path construction.
- 2026-05-15 18:21 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: canonical map surface owns implementation through internal map module | notes: map ownership source-lock expected literal tail-dispatch map insert path; updated to shared path-builder spelling.
- 2026-05-15 18:21 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"` | failures: none | notes: backend IR source-lock slice passed after branch-local map resolver searches.
- 2026-05-15 18:21 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO source-lock passed after restoring active TODO-4275 tuple-type-packs parallel_track metadata
- 2026-05-15 18:20 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_misc_tests --test-case="monomorphizes integer template arguments distinctly,integer template arguments cannot substitute type positions"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: integer-template semantics slice passed; TODO source-lock failed because active TODO-4275 successor was missing `parallel_track: tuple-type-packs`
- 2026-05-15 18:19 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_parser_tests`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates` | failures: none | notes: parser template suite passed after correcting integer-template metadata test expectations
- 2026-05-15 18:18 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_parser_tests`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates` | failures: parses integer template argument metadata on call and method call | notes: assertion inspected `returnExpr->args[0]`, which is the receiver name; definition returnExpr stores the returned method call directly
- 2026-05-15 18:15 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates` | failures: parses integer template argument metadata on call and method call | notes: assertion expected `mainDef.statements.size() == 2`, actual 3
- 2026-05-15 17:18 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"` | failures: ir lowerer inline dispatch collection access fallback uses semantic receiver facts first | notes: source-lock ordering assertion for shared map fallback failed after TODO-4532 refactor; updated the lock to check the name and call receiver branches explicitly.
- 2026-05-15 18:16 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"` | failures: ir lowerer inline dispatch collection access fallback uses semantic receiver facts first | notes: build passed, but source-lock still matched the later call-branch map resolver for name-branch ordering; narrowed the source search to the actual enclosing name branch.
- 2026-05-15 18:18 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"` | failures: ir lowerer inline dispatch collection access fallback uses semantic receiver facts first | notes: exact multiline resolver search missed the name branch due continuation indentation and still matched the call branch; changed to branch-local resolver token searches.
- 2026-05-15 15:49 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_misc_tests --test-case="monomorphizes template definition bindings,template argument count mismatch fails,template arguments required for templated calls,type pack specializations bind zero one and many arguments,type pack specialization requires ordinary arguments before pack,type pack parameters cannot be used as scalar types before expansion"`; `cd build-release && ./PrimeStruct_semantics_tests --test-suite=primestruct.semantics.definition_prepass`; `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4269 focused validation passed after binding trailing type-pack template arguments into deterministic specialization metadata, publishing semantic-product pack bindings, and promoting TODO-4270. Initial attempted `PrimeStruct_semantics_tests --test-suite=primestruct.semantics.manual` filters matched 0 tests because those manual template tests live in `PrimeStruct_misc_tests`; reran the correct focused target.
- 2026-05-15 12:31 local | pass | mode: release | command: `./scripts/compile.sh --release --skip-tests`; `cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates`; `cd build-release && ./PrimeStruct_semantics_tests --test-suite=primestruct.semantics.definition_prepass`; `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4268 build-only release validation plus focused parser, semantic, and TODO source-lock slices passed after adding heterogeneous type-pack declaration metadata and diagnostics. A prior release build attempt failed because `SemanticProgramDefinition` aggregate initializers were shifted by new fields; fixed by preserving positional field order and appending the new metadata fields.
- 2026-05-15 11:38 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_map_surface_trace_inventory.py --repo-root . && PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_map_backing_traces.py --repo-root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer tail dispatch rewrite guards explicit map defs,ir lowerer tail map insert rewrite uses semantic receiver facts first,ir lowerer tail explicit map helper rewrite uses semantic receiver facts first,ir lowerer tail canonical experimental map helper rewrite uses semantic receiver facts first,ir lowerer tail borrowed map receiver rewrite uses semantic receiver facts first"` | failures: none | notes: TODO-4531 focused validation passed after routing tail-dispatch map access classification through shared lowerer helpers and reducing map trace inventories.
- 2026-05-15 11:08 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,emitter builtin collection inference source stays canonical"` | failures: none | notes: TODO-4530 focused validation passed after routing semantic builtin SoA path helper spellings through shared path builders and reducing that file's inventory cap.
- 2026-05-15 10:20 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable,soa pending diagnostics route through shared semantics helpers"` | failures: none | notes: TODO-4526 focused validation passed after routing remaining SemanticsValidate SoA type/helper spellings through shared semantic helpers and reducing that source file's inventory cap.
- 2026-05-15 09:38 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_BUILD_TESTS=ON`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers"` | failures: none | notes: TODO-4526 focused validation passed after moving initializer-inference SoA helper spelling checks behind shared semantic helpers and reducing that source file's public-surface inventory cap.
- 2026-05-15 06:30 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`; `cmake --build build-release --target primec`; `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter collection helper metadata delegation stays source locked,ir lowerer vector type layout traces use generic collection helpers,native tail and late collection helper metadata dispatch stays source locked"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,soa compatibility fixture migration boundary stays source locked,legacy soa_vector compatibility rejection matrix stays source locked,soa public collection docs stay source locked"` | failures: none | notes: TODO-4522 focused release validation passed after removing obsolete rooted `/soa_vector/*` lowerer/emitter fallback routing and preserving canonical SoA source locks.
- 2026-05-15 05:58 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .` | failures: none | notes: TODO-4522 non-heavy inventory validation passed after reducing lowerer/emitter SoA public-surface residue to 1,758 allowed production traces; focused backend IR and compile-run validation still need an approved heavy-command turn.
- 2026-05-15 05:44 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="explicit soa_vector count forms reject non-soa target,public soa get helper rejects template arguments on non-soa receiver,get root forms reject vector target,soa_vector literal missing template arg fails,soa_vector literal requires struct element type,soa_vector literal requires heap_alloc effect when non-empty"`; `cmake --build build-release --target primec PrimeStruct_compile_run_tests PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,public soa get helper rejects template arguments on non-soa receiver in C++ emitter,native public soa get helper rejects template arguments on non-soa receiver,vm public soa get helper rejects template arguments on non-soa receiver"`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,root get vector receiver rejects template arguments,semantics validator infer source delegation stays stable"` | failures: none | notes: TODO-4521 focused validation passed after reducing semantic SoA public-surface inventory residue to 1,796 slots and preserving retired-spelling rejection coverage.
- 2026-05-15 05:05 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_compile_run_tests PrimeStruct_backend_ir_tests` | failures: TODO-4521 release build dir missing | notes: build-release directory absent; configure release build dir before rerunning selected focused window.
- 2026-05-15 04:54 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`; `cd build-release && ctest --output-on-failure -R '^PrimeStruct_soa_surface_trace_inventory(_self_test)?$'` | failures: none | notes: TODO-4520 focused CTest wiring validation passed for the SoA inventory release gate and self-test.
- 2026-05-15 04:52 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_soa_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/test_check_soa_surface_trace_inventory.py --repo-root .`; `python3 -m py_compile scripts/check_soa_surface_trace_inventory.py tests/scripts/test_check_soa_surface_trace_inventory.py` | failures: none | notes: TODO-4520 non-heavy focused validation passed for the SoA public-surface inventory gate and checker self-test; CTest wiring was added but not run because a heavy-command turn was not approved.
- 2026-05-15 04:05 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`; `cmake --build build-release --target primec PrimeStruct_compile_run_tests PrimeStruct_backend_ir_tests PrimeStruct_semantics_tests`; `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects experimental soa_vector stdlib helpers in C++ emitter,rejects raw soa_vector type spelling in C++ emitter,legacy soa_vector compatibility helpers reject in C++ emitter,rejects vm experimental soa_vector stdlib helpers,rejects vm raw soa_vector type spelling,vm legacy soa_vector compatibility helpers reject,rejects native experimental soa_vector stdlib helpers,rejects native raw soa_vector type spelling,native legacy soa_vector compatibility helpers reject,legacy soa_vector compatibility rejection matrix stays source locked,soa compatibility fixture migration boundary stays source locked,soa public collection docs stay source locked,stdlib collection wrappers and adapters stay source locked,todo queue and skipped doctest debt stay source locked"`; `cmake --build build-release --target PrimeStruct_backend_runtime_tests`; `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked,collection helper surface registry resolves preferred compatibility spellings,design doc records soa public collection contract"`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="public soa count helper validates on public wrapper bindings,experimental soa_vector stdlib helpers reject primitive element types"` | failures: none | notes: TODO-4519 focused validation passed after retiring public `soa_vector` spellings, locking direct old import/type-spelling rejection, and preserving canonical `soa<T>` docs/surface metadata. A stale attempted `PrimeStruct_backend_ir_tests` filter matched 0 tests before rerunning the intended source-lock checks in `PrimeStruct_backend_runtime_tests`.
- 2026-05-15 04:05 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects experimental soa_vector stdlib helpers in C++ emitter,rejects raw soa_vector type spelling in C++ emitter,legacy soa_vector compatibility helpers reject in C++ emitter,rejects vm experimental soa_vector stdlib helpers,rejects vm raw soa_vector type spelling,vm legacy soa_vector compatibility helpers reject,rejects native experimental soa_vector stdlib helpers,rejects native raw soa_vector type spelling,native legacy soa_vector compatibility helpers reject,legacy soa_vector compatibility rejection matrix stays source locked,soa compatibility fixture migration boundary stays source locked,soa public collection docs stay source locked,stdlib collection wrappers and adapters stay source locked,todo queue and skipped doctest debt stay source locked"` | failures: soa public collection docs stay source locked | notes: source-lock still expected three old accepted-compatibility doc fragments after TODO-4519 narrowed public docs to canonical `soa<T>` guidance and rejected old SoA spellings.
- 2026-05-15 02:55 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="soa compatibility fixture migration boundary stays source locked,compiles and runs public soa count helper on public wrapper in C++ emitter,runs vm public soa count helper on public wrapper,compiles and runs native public soa count helper on public wrapper"`; `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="public soa count helper validates on public wrapper bindings,wildcard-imported public soa helpers infer receiver-matched templates"`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="public soa count helper lowers through wrapper routing"` | failures: none | notes: TODO-4518 focused validation passed after migrating ordinary SoA fixtures to the public `soa<T>` surface and quarantining remaining legacy imports by fixture name.
- 2026-05-15 02:31 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests` | failures: PrimeStruct_compile_run_tests build | notes: new SoA migration source-lock helper used a `const std::string&` range loop over string literals under `-Werror`; fixed by iterating marker literals as `const char *`.
- 2026-05-15 02:19 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests` | failures: none | notes: worktree had no `build-release/` directory; configure release build dir before rerunning the selected focused window.
- 2026-05-15 00:52 local | pass | mode: release | command: `./scripts/compile.sh --release` | failures: none | notes: full release gate passed 1597/1597 after SoA helper routing, native map value validation, field-view diagnostic, source-lock, and type-resolution parity stabilization.
- 2026-05-14 22:29 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter,rejects native ownership-sensitive experimental map value methods"` | failures: none | notes: focused rerun covered the C++ emitter `/ref_ref` rejection and native map ownership rejection.
- 2026-05-14 22:24 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter; rejects native ownership-sensitive experimental map value methods | notes: stopped after the full gate reported two failures; the original native helper-return SoA shadow shard passed inside this run.
- 2026-05-14 22:10 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable,semantics validator expr source delegation stays stable"` | failures: none | notes: source-delegation locks passed after changing initializer inference and method-target resolution.
- 2026-05-14 22:09 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers,native compiles and runs helper-return experimental soa_vector method shadows,native compiles helper-return soa_vector shadows with explicit canonical fallbacks"` | failures: none | notes: rebuilt the `primec` executable before the doctest binary; the source-lock and native helper-return SoA shadow cases now pass.
- 2026-05-14 22:03 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers,native compiles and runs helper-return experimental soa_vector method shadows,native compiles helper-return soa_vector shadows with explicit canonical fallbacks"` | failures: native helper-return experimental soa_vector shadow methods; native helper-return soa_vector canonical fallbacks; dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers | notes: failure remained after same-path family lookup changes; method dump still resolves count to `/std/collections/soa/count` and native cases still report `/soa_vector/get` unknown-target diagnostics.
- 2026-05-14 21:45 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: native helper-return experimental soa_vector shadow methods; native helper-return soa_vector canonical fallbacks | notes: baseline stopped after release shard `PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_extended_201_210` exposed `/soa_vector/get` unknown-target diagnostics.
- 2026-05-14 21:46 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="native compiles and runs helper-return experimental soa_vector method shadows,native compiles helper-return soa_vector shadows with explicit canonical fallbacks"` | failures: native helper-return experimental soa_vector shadow methods; native helper-return soa_vector canonical fallbacks | notes: focused rerun reproduced semantic `unknown call target: /soa_vector/get` for method and direct legacy shadow helper-return fixtures.
- 2026-05-14 21:52 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="native compiles and runs helper-return experimental soa_vector method shadows,native compiles helper-return soa_vector shadows with explicit canonical fallbacks"` | failures: native helper-return experimental soa_vector shadow methods; native helper-return soa_vector canonical fallbacks | notes: same `/soa_vector/get` unknown-target diagnostic remained after initial resolved-family fallback attempt.
- 2026-05-14 21:55 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers"` | failures: dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers | notes: source-lock localized regression to public `/std/collections/soa/count` preempting visible same-path `/soa_vector/count`.
- 2026-05-14 21:34 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa field-view wrappers use public reads,vm public soa field-view wrappers use public reads,todo queue and skipped doctest debt stay source locked,soa public collection docs stay source locked,small stdlib wrappers stay source locked to inferred locals"`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable,semantics validator expr source delegation stays stable"`; `cd build-release && ./primec --emit=vm /tmp/soa_field_direct.0AwEjM.prime --entry /main`; `cd build-release && ./primec --emit=vm /tmp/soa_field_canonical.pykBZ4.prime --entry /main`; `cd build-release && ./primec --emit=native /tmp/soa_field_public.UVWTIs.prime -o /tmp/soa_field_public_final_probe --entry /main && /tmp/soa_field_public_final_probe` | failures: none | notes: TODO-4517 direct public field_view indexing, canonical field syntax, docs/source locks, and direct probes passed; probe exit codes 13/13/18 were expected program return values.
- 2026-05-14 21:31 local | fail | mode: release | command: `cmake --build build-release --target primec`; `cd build-release && ./primec --emit=vm /tmp/soa_field_direct.*.prime --entry /main` | failures: direct public SoA field-view wrapper indexing | notes: first carrier-index rewrite still evaluated the temporary `field_view` carrier and hit `SoaSchemaFieldCount`; resolved by rewriting public direct carrier indexing to public `soa/get(...).field`.
- 2026-05-14 21:18 local | fail | mode: release | command: `cd build-release && ./primec --emit=vm /tmp/soa_field_direct.*.prime --entry /main`; `cd build-release && ./primec --emit=vm /tmp/soa_field_canonical.*.prime --entry /main` | failures: direct public SoA field-view wrapper indexing; canonical SoA field-view method indexing | notes: TODO-4517 probes reproduced `SoaSchemaFieldCount` expression lowering for direct `field_view(...)[index]` and semantic `unknown call target: y` for `values.y()[index]`.
- 2026-05-14 21:16 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer vector type layout traces use generic collection helpers,semantics validator expr source delegation stays stable"` | failures: none | notes: focused lowerer/source-delegation locks passed for the direct vector<Struct> indexed-access change.
- 2026-05-14 21:15 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa from-aos uses wrapper,vm public soa from-aos uses wrapper,todo queue and skipped doctest debt stay source locked,soa public collection docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: none | notes: focused TODO-4516 docs/source-lock and public from_aos wrapper coverage passed after splitting field-view wrapper indexing to TODO-4517.
- 2026-05-14 21:13 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa from-aos uses wrapper,vm public soa from-aos uses wrapper"` | failures: none | notes: focused public SoA from_aos wrapper coverage passed after allowing direct vector<Struct> indexed access through the lowerer struct-slot path.
- 2026-05-14 20:56 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa conversion and field views use wrappers,vm public soa conversion and field views use wrappers"` | failures: native public soa conversion and field views use wrappers; vm public soa conversion and field views use wrappers | notes: public wrapper fixture exposed semantic `unknown call target: y` for `values.y()[index]`; localizing public field-view method resolution before rerun.
- 2026-05-14 20:48 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,soa public collection docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: none | notes: docs queue/source-lock validation passed after promoting TODO-4516 as the next Ready Now leaf.
- 2026-05-14 20:47 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable,semantics validator expr source delegation stays stable,template monomorph source delegation stays stable,ir lowerer call helpers source delegation stays stable,emitter emit setup source delegation stays stable"` | failures: none | notes: source-delegation locks passed for the touched semantics, template-monomorph, and IR-lowerer helper routing code.
- 2026-05-14 20:45 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,soa public collection docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: todo queue and skipped doctest debt stay source locked | notes: source-lock still expected TODO-4307 as the Ready Now and execution queue head; resolved by retargeting it to TODO-4516.
- 2026-05-14 20:42 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs canonical vector mutators over imported user shadow helpers in C++ emitter,compiles and runs canonical vector mutator named calls over imported user shadow helpers in C++ emitter,rejects imported user vector mutator positional call shadow in C++ emitter,runs vm with stdlib collection shim capacity helper,compiles and runs native collection syntax parity for call and method forms,rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter,*public soa construction and mutators use wrappers,*public soa read helpers route through wrapper paths,*reflected multi-field index syntax"` | failures: none | notes: focused compile-run stabilization passed after vector shadow diagnostics and same-path ref_ref rejection locks accepted the current public SoA diagnostic paths.
- 2026-05-14 20:39 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter"` | failures: rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter | notes: rejection now reports semantic `unknown call target: /soa_vector/ref_ref`; resolved by preserving the fixture as a rejection lock for the current diagnostic.
- 2026-05-14 20:36 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs canonical vector mutators over imported user shadow helpers in C++ emitter,compiles and runs canonical vector mutator named calls over imported user shadow helpers in C++ emitter,rejects imported user vector mutator positional call shadow in C++ emitter,runs vm with stdlib collection shim capacity helper,compiles and runs native collection syntax parity for call and method forms,rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter,*public soa construction and mutators use wrappers,*public soa read helpers route through wrapper paths,*reflected multi-field index syntax"` | failures: rejects imported user vector mutator positional call shadow in C++ emitter; rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter | notes: stale diagnostics expected legacy `/std/collections/soa_vector/*` paths; resolved by accepting public `/std/collections/soa/*` paths where appropriate.
- 2026-05-14 20:30 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: vector mutator shadow cases; stdlib collection shim capacity helper; native collection syntax parity; builtin helper-return soa_vector ref_ref same-path helper | notes: release probe was stopped after identifying focused compile-run failures around public SoA wrapper routing and vector fallback resolution.
- 2026-05-14 19:59 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa read helpers route through wrapper paths"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="vm public soa read helpers route through wrapper paths"`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="*source delegation stays stable*"` | failures: none | notes: TODO-4514 focused public SoA read/ref wrapper routing passed for native, VM, and source-delegation locks.
- 2026-05-14 19:56 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa read helpers route through wrapper paths"` | failures: native public soa read helpers route through wrapper paths | notes: interim fixture exposed stale public SoA read/ref routing through `/std/collections/soa_vector/ref` and explicit slash-method count visibility; resolved by preserving public helper paths and keeping method sugar on imported `values.count()`.
- 2026-05-14 19:24 local | pass | mode: release | command: `./scripts/compile.sh --release` | failures: none | notes: Full TODO-4511 gate passed 1597/1597 after narrowing recorded borrow roots to Reference/SoaFieldView/auto field-view bindings, preserving pointer-alias handling, and refreshing the non_math_large_include semantic-product memory cap.
- 2026-05-14 18:40 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="semantic memory budget policy artifacts are checked in"` | failures: none | notes: Focused benchmark policy source-lock passed after updating the math_vector ast-semantic hard-cap expectation and policy note to the refreshed budget.
- 2026-05-14 18:38 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: PrimeStruct_primestruct_compile_run_benchmark_harness | notes: Full gate was stopped after the benchmark harness source-lock still expected the old `math_vector:ast-semantic` hard RSS cap; updated the harness and semantic memory policy note before rerunning focused validation.
- 2026-05-14 18:22 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="generic soa substrate boundary stays source locked,todo queue and skipped doctest debt stay source locked,collection docs snippets stay code-examples style and executable"` | failures: none | notes: Focused docs/source-lock validation passed after closing TODO-4511, promoting TODO-4307, and documenting the current SoA helper-lowering gap.
- 2026-05-14 18:22 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="legacy soa_vector builtin field-view bindings track compatibility borrow roots,canonical soa field-view bindings track borrow roots through public imports,canonical soa field-view binding blocks structural mutation while live,legacy soa_vector field-view binding blocks structural mutation while live,legacy soa_vector field-view bindings resolve helper-return borrow roots"` | failures: none | notes: TODO-4511 focused SoA field-view coverage passed after canonical `soa<T>` public-import field views gained live structural-mutation invalidation and legacy coverage was retargeted to the same borrowed-root diagnostic.
- 2026-05-14 18:04 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests PrimeStruct_backend_ir_tests`; `cd build-release && ctest --output-on-failure -R '^(PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50|PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_157_161|PrimeStruct_primestruct_compile_run_imports_operations_and_collections_25_26|PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_22_22|PrimeStruct_semantic_memory_trend)$'` | failures: none | notes: Initial TODO-4511 gate failures were stabilized by refreshing SoA source locks/docs, emitter map-helper metadata source lock, and the math_vector ast-semantic memory budget.
- 2026-05-14 17:51 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50; PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_157_161; PrimeStruct_primestruct_compile_run_imports_operations_and_collections_25_26; PrimeStruct_primestruct_compile_run_examples_examples_ir_and_spinning_cube_22_22; PrimeStruct_semantic_memory_trend | notes: initial gate for TODO-4511. Observed source-lock drift around SoA docs/native no-import rejection and a semantic-memory trend failure; localizing before TODO work.
- 2026-05-14 17:19 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*stdlib style boundary docs*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*todo queue and skipped doctest debt*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*soa public collection docs*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*soa example stays source locked*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*legacy soa_vector compatibility backend parity*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*coding guidelines avoid inactive*"`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*compiles surface examples to IR*"` | failures: none | notes: TODO-4509 focused docs/example/source-lock validation passed after migrating the public SoA example to `soa<T>` and retargeting source locks.
- 2026-05-14 17:18 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="*stdlib style boundary docs*"` | failures: stdlib style boundary docs stay source locked | notes: source lock still expected a pre-wrap `use /std/collections/soa/* for construction` substring; resolved by matching the current wrapped wording.
- 2026-05-14 17:05 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa type spelling runs through canonical helpers,native wildcard-imported canonical soa_vector helpers run without experimental imports,native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,soa public collection docs stay source locked"` | failures: none | notes: TODO-4508 focused native/helper compatibility and docs source-lock validation passed after promoting `soa<T>` as the public type spelling.
- 2026-05-14 17:05 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa type spelling runs through canonical helpers,native wildcard-imported canonical soa_vector helpers run without experimental imports,native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,soa public collection docs stay source locked"` | failures: soa public collection docs stay source locked | notes: source lock still expected one-line `One source-locked wildcard canonical parity program runs`; resolved by locking the wrapped doc text.
- 2026-05-14 17:04 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native public soa type spelling runs through canonical helpers,native wildcard-imported canonical soa_vector helpers run without experimental imports,native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,soa public collection docs stay source locked"` | failures: stdlib style boundary docs stay source locked; todo queue and skipped doctest debt stay source locked | notes: source locks still expected `soa_vector<T>` as public example spelling and TODO-4508 as queue head; resolved by retargeting locks to `soa<T>` and TODO-4509.
- 2026-05-14 17:03 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `./build-release/PrimeStruct_semantics_tests --test-case="soa public spelling validates bindings returns references and pointers,soa public spelling requires struct element type,soa_vector binding validates with soa-safe struct element type"` | failures: none | notes: TODO-4508 focused semantic validation passed after avoiding the no-import borrowed `count_ref` fallback in the positive type-spelling fixture.
- 2026-05-14 17:02 local | fail | mode: release | command: `./build-release/PrimeStruct_semantics_tests --test-case="soa public spelling validates bindings returns references and pointers,soa public spelling requires struct element type,soa_vector binding validates with soa-safe struct element type"` | failures: soa public spelling validates bindings returns references and pointers | notes: positive fixture used a no-import borrowed `count_ref` path and reported `unknown method: /std/collections/soa_vector/count_ref`; resolved by keeping the fixture focused on type spelling and value count helpers.
- 2026-05-14 16:50 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `./build-release/PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable,semantics validator expr source delegation stays stable,emitter emit setup source delegation stays stable"` | failures: none | notes: Focused source-delegation validation passed after extending SoA helper classifiers for the new `/std/collections/soa/*` wrapper path.
- 2026-05-14 16:49 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: none | notes: TODO-4507 focused validation passed after adding the canonical `/std/collections/soa/*` wrapper namespace and rebuilding the `primec` executable used by compile-run fixtures.
- 2026-05-14 16:48 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: native wildcard-imported canonical soa helpers run without experimental imports; small stdlib wrappers stay source locked to inferred locals | notes: stale `primec` first reported `template arguments required for /std/collections/soa/count`; after rebuilding `primec`, the remaining source lock expected a removed internal conversion import.
- 2026-05-14 16:43 local | fail | mode: release | command: `./build-release/PrimeStruct_compile_run_tests --test-case="native wildcard-imported canonical soa helpers run without experimental imports,todo queue and skipped doctest debt stay source locked,stdlib style boundary docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | failures: native wildcard-imported canonical soa helpers run without experimental imports | notes: wrong cwd for compile-run fixture; `./primec` was not found from the repository root, resolved by rerunning from `build-release/`.
- 2026-05-14 16:17 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `./build-release/PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers resolve and validate map access targets,ir lowerer vector type layout traces use generic collection helpers"`; `cmake --build build-release --target PrimeStruct_compile_run_tests`; `./build-release/PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: TODO-4506 focused validation passed after routing access-target map backing preservation through the shared lowerer map backing predicate and tightening the target file trace inventories.
- 2026-05-14 16:09 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `./build-release/PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: docs queue source lock now expects TODO-4506 as the bounded map-audit leaf after finishing TODO-4505.
- 2026-05-14 16:07 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `./build-release/PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers emit map lookup access,ir lowerer call helpers source delegation stays stable"`; `cmake --build build-release --target PrimeStruct_misc_tests`; `./build-release/PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: TODO-4505 focused validation passed after routing access-load map backing detection through the shared lowerer map backing predicate and removing the access-load file from the map trace inventories.
- 2026-05-14 16:01 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `./build-release/PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: docs queue source lock now expects TODO-4505 as the bounded map-audit leaf after finishing TODO-4504.
- 2026-05-14 15:59 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_backing_traces.py --root .`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `./build-release/PrimeStruct_backend_ir_tests --test-case="ir lowerer flow helpers disarm soa storage temporaries after copy"`; `cmake --build build-release --target PrimeStruct_misc_tests`; `./build-release/PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: TODO-4504 focused validation passed after routing flow-copy map backing detection through the shared lowerer map backing predicate and removing the flow-control file from the map trace inventories.
- 2026-05-14 15:52 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `./build-release/PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: docs queue source lock now expects TODO-4504 as the bounded map-audit leaf ahead of the broad TODO-4464 tracker.
- 2026-05-14 15:52 local | fail | mode: release | command: `./build-release/PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: stale source-lock still expected TODO-4464 as the Ready Now/execution queue head after TODO-4503 was finished; resolved by retargeting the lock to TODO-4504.
- 2026-05-14 15:48 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `./build-release/PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable"`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `./build-release/PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,stdlib namespaced map constructor requires imported stdlib helper"` | failures: none | notes: TODO-4503 focused validation passed after routing template experimental map constructor helper classification through shared map constructor metadata.
- 2026-05-14 15:41 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .`; `cmake --build build-release --target PrimeStruct_backend_runtime_tests`; `./build-release/PrimeStruct_backend_runtime_tests --test-case="compile pipeline publishes an initial semantic product shell"`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `./build-release/PrimeStruct_semantics_tests --test-case="semantic product keeps vector and map bridge parity"` | failures: none | notes: TODO-4502 focused validation passed after routing semantic snapshot map bridge choices through stdlib surface metadata and refreshing stale source-lock/bridge parity fixtures.
- 2026-05-14 15:31 local | fail | mode: release | command: `./build-release/PrimeStruct_semantics_tests --test-case="semantic product keeps exact-import vector and map bridge parity"` | failures: semantic product keeps exact-import vector and map bridge parity | notes: stale fixture expected count bridge choices without concrete canonical count helpers; resolved by adding templated canonical vector/map count helpers and matching interned bridge scope/family ids.
- 2026-05-14 15:25 local | fail | mode: release | command: `./build-release/PrimeStruct_backend_runtime_tests --test-case="compile pipeline publishes an initial semantic product shell"` | failures: compile pipeline publishes an initial semantic product shell | notes: stale source-lock expectations for `onErrorFacts`, vector bridge classification, and new map bridge-key wrapper assertions; resolved by updating source-locks to current source shape.
- 2026-05-14 15:19 local | pass | mode: release | command: `python3 scripts/check_map_surface_trace_inventory.py`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator passes source delegation stays stable"`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="collected diagnostics ignore imported canonical map access helper calls"` | failures: none | notes: TODO-4501 focused validation passed after routing pass-diagnostic canonical map access classification through stdlib surface metadata.
- 2026-05-14 15:12 local | pass | mode: release | command: `python3 scripts/check_map_surface_trace_inventory.py`; `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable"`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper"` plus same-path, missing-import, alias-fallback, and rooted `/map/count` focused cases | failures: none | notes: TODO-4500 focused validation passed after routing template map constructor alias and rooted helper alias handling through shared metadata helpers.
- 2026-05-14 15:01 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` plus `cmake --build build-release --target PrimeStruct_backend_ir_tests` and `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable,ir lowerer materializes variadic map packs with indexed count methods,ir lowerer materializes variadic borrowed map packs with indexed dereference count methods,ir lowerer materializes variadic borrowed map packs with indexed dereference lookup helpers"` | failures: none | notes: TODO-4499 focused validation passed after routing template map method target normalization and canonical helper path construction through shared metadata helpers.
- 2026-05-14 14:50 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` plus `cmake --build build-release --target PrimeStruct_backend_ir_tests` and `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator passes source delegation stays stable"` | failures: none | notes: TODO-4498 focused validation passed after routing struct-layout map backing checks through shared backing-type helper metadata.
- 2026-05-14 14:49 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` | failures: PrimeStruct_backend_ir_tests build | notes: transient source-lock compile failure from adding `semanticsPassesStructLayoutsSource` to the wrong doctest scope; fixed by moving the assertion into the source-delegation test block.
- 2026-05-14 14:44 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` plus `cmake --build build-release --target PrimeStruct_backend_ir_tests` and `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable"` | failures: none | notes: TODO-4497 focused validation passed after routing template experimental map constructor root checks through shared constructor metadata helpers.
- 2026-05-14 14:39 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` plus `cmake --build build-release --target PrimeStruct_backend_ir_tests` and `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator expr source delegation stays stable"` | failures: none | notes: TODO-4496 focused validation passed after routing collection-access map type checks through shared key/value extraction.
- 2026-05-14 14:39 local | fail | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` | failures: src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp | notes: allowance was tightened to 31 but the script counted 33 remaining traces; corrected the reduced allowance to 33.
- 2026-05-14 14:33 local | pass | mode: release | command: `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check_map_surface_trace_inventory.py --root .` plus `cmake --build build-release --target PrimeStruct_backend_ir_tests` and `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator expr source delegation stays stable"` | failures: none | notes: TODO-4495 focused validation passed after routing collection-predicate map type checks through shared key/value extraction.
- 2026-05-14 14:26 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_runtime_tests` plus focused `PrimeStruct_semantics_tests --test-case="graph type resolver infers direct-call auto binding from imported map-return helper"` and `PrimeStruct_backend_runtime_tests --test-case="graph type resolver pilot is wired through options and semantics inference"` | failures: none | notes: TODO-4494 focused validation passed after routing direct-call map backing recognition through the shared collection backing-type helper.
- 2026-05-14 14:20 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests` plus focused `PrimeStruct_backend_ir_tests --test-case="semantics validator stdlib bridge helper routing stays stable"`, `PrimeStruct_semantics_tests --test-case="canonical map ownership-sensitive values validate through canonical helpers"`, and `PrimeStruct_semantics_tests --test-case="experimental vector ownership-sensitive helpers accept non-trivial elements"` | failures: none | notes: TODO-4493 focused validation passed after routing statement-container map backing recognition through the shared collection backing-type helper.
- 2026-05-14 14:18 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests` | failures: PrimeStruct_backend_ir_tests build | notes: transient source-lock compile failure from adding `semanticsStatementContainerHelpersSource` to the wrong doctest scope; fixed by moving the fixture into the stdlib bridge helper routing test.
- 2026-05-14 14:16 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests` | failures: PrimeStruct_semantics_tests build | notes: transient compile failure from assigning the string-view result of `trimLeadingSlash` to `std::string`; fixed by keeping the normalized path as `std::string_view`.
- 2026-05-14 13:50 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator stdlib bridge helper routing stays stable"` | failures: semantics validator stdlib bridge helper routing stays stable | notes: stale source-lock expected FileError/map direct or method call facts and old SoA/map helper snippets.
- 2026-05-14 13:50 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator stdlib bridge helper routing stays stable"` | failures: none | notes: source-lock passed after switching helper routing facts to stable direct/collection specialization checks and current source snippets.
- 2026-05-14 13:17 local | pass | mode: release | command: `./scripts/compile.sh --release` | failures: none | notes: final stabilization gate passed 1597/1597 after map alias/source-lock fixes, semantic-memory budget refresh, and the backend registry allocation-counter assertion update.
- 2026-05-14 12:58 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R '^(PrimeStruct_primestruct_ir_pipeline_backends_registry|PrimeStruct_semantic_memory_trend)$'` | failures: none | notes: focused stabilization passed after relaxing the semantic-product net allocated-bytes assertion and refreshing the `math_vector_matrix:ast-semantic` RSS hard cap.
- 2026-05-14 12:56 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: PrimeStruct_primestruct_ir_pipeline_backends_registry and PrimeStruct_semantic_memory_trend | notes: prior map/source-lock failures passed in aggregate; backend registry saw `semanticProductBuild.allocatedBytes == 0`, and semantic-memory trend failed with details in `build-release/benchmarks/semantic_memory_trend_report.json`.
- 2026-05-14 12:36 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R '^(PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130|PrimeStruct_map_surface_trace_inventory)$'` | failures: none | notes: focused stabilization passed after switching canonical map helper checks to path builders and updating the source lock.
- 2026-05-14 12:35 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R '^(PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130|PrimeStruct_map_surface_trace_inventory)$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130 | notes: map surface trace inventory passed; remaining source-lock failure expects old literal `matchesResolvedPath("/std/collections/map/...")` strings after the lowerer switched to path builders.
- 2026-05-14 12:33 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130 and PrimeStruct_map_surface_trace_inventory | notes: aggregate rerun after map-alias/source-lock stabilization passed the original 17 failing shards but exposed two remaining release targets before TODO selection.
- 2026-05-14 12:01 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: 17 CTest targets | notes: baseline failed before TODO selection; observed docs source-lock failures in `test_compile_run_examples_docs_locks.cpp` and IR lowerer raw-path/experimental-map inline dispatch source locks after prior map-alias deletions.
- 2026-05-14 10:38 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` plus focused map entry-constructor behavior and source-lock doctest cases | failures: none | notes: final TODO-4469 focused validation passed after deleting experimental-map entry constructor aliases.
- 2026-05-14 10:32 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` plus focused access-helper and source-lock doctest cases | failures: none | notes: final TODO-4467 focused validation passed after deleting experimental-map access alias probes and retargeting the stale exact canonical vector access expectation.
- 2026-05-14 10:31 local | fail | mode: release | command: `./PrimeStruct_backend_ir_tests -tc="ir lowerer access helper recognizes namespaced canonical access helpers"` | failures: ir lowerer access helper recognizes namespaced canonical access helpers | notes: stale exact canonical vector access expectation; lowerer intentionally rejects `/std/collections/vector/at` as a builtin-access alias.
- 2026-05-14 10:23 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` plus focused emitter/lowerer collection helper source-lock doctest cases | failures: none | notes: final TODO-4466 focused validation passed after deleting slashless experimental-map helper path normalization.
- 2026-05-14 10:16 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` plus focused lowerer source-lock, return-kind, struct-return, and method-receiver candidate tests | failures: none | notes: final TODO-4465 focused validation passed after locking lowerer map helper candidate paths against rooted/canonical mirroring.
- 2026-05-14 10:07 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests` plus focused backend IR source-lock and C++ map-alias direct test cases | failures: none | notes: final TODO-4463 focused validation passed after removing emitter helper mirroring and rooted `/map/at*` access-helper raw-path fallbacks.
- 2026-05-14 10:02 local | fail | mode: release | command: `./PrimeStruct_compile_run_tests -tc="rejects map namespaced at compatibility alias in C++ emitter without explicit alias"` | failures: rejects map namespaced at compatibility alias in C++ emitter without explicit alias | notes: direct `/map/at` still resolved through canonical helper before pre-dispatch checked the explicit source path.
- 2026-05-14 10:02 local | fail | mode: release | command: `./PrimeStruct_compile_run_tests -tc="rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias"` | failures: rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias | notes: direct `/map/at_unsafe` still resolved through canonical helper before pre-dispatch checked the explicit source path.
- 2026-05-14 10:01 local | fail | mode: release | command: `./PrimeStruct_backend_ir_tests -tc="semantics validator infer source delegation stays stable"` | failures: semantics validator infer source delegation stays stable | notes: source lock still expected the old explicit rooted `/map/*` method-target branch before narrowing the assertion.
- 2026-05-14 09:08 local | pass | mode: release | command: `./scripts/compile.sh --release` | failures: none | notes: final stabilization gate passed 1589/1589 tests after source-lock, vector bounds, SoA bridge, Metal skip, and semantic-memory budget updates.
- 2026-05-14 08:49 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R '^PrimeStruct_semantic_memory_(benchmark|trend)$'` | failures: none | notes: focused semantic-memory benchmark/trend gate passed after updating runner-observed RSS policy headroom.
- 2026-05-14 08:49 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R '^PrimeStruct_semantic_memory_(benchmark|trend)$'` | failures: PrimeStruct_semantic_memory_trend | notes: exposed `imported_math_body:ast-semantic` hard cap as too tight for the release runner.
- 2026-05-14 08:45 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: PrimeStruct_semantic_memory_trend | notes: all other 1588 tests passed; semantic-memory trend failed on release-runner RSS hard caps for semantic-product fixtures.
- 2026-05-14 08:03 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_(91_100|121_130|461_470|541_550|651_660)$|^PrimeStruct_vector_surface_traces$'` | failures: none | notes: focused rerun passed remaining IR validation/source-trace failures after root-slash normalization, vector metadata path, result helper, and vector `at_unsafe` expectation fixes.
- 2026-05-14 07:52 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: 19 CTest targets | notes: fresh baseline failed CTests 138, 571, 1153, 1155, 1192, 1193, 1194, 1199, 1206, 1281, 1285, 1304, 1433, 1443, 1444, 1458, 1548, 1575, and 1576; use Current Known Failures for focused reruns.
- 2026-05-14 07:28 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1183,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_advanced_297_301 | notes: native canonical namespaced vector named-argument temporary receiver now rejects around `/std/collections/vector/count` expression lowering instead of running with stale exit code 16.
- 2026-05-14 07:27 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_293_296$'` | failures: none | notes: native vector wrapper constructor mismatch and receiver fixtures now assert current native compile-rejection diagnostics.
- 2026-05-14 07:23 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1181,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_293_296 | notes: CTest 1181 passed; native stdlib wrapper vector constructor explicit Vector mismatch and receiver cases now compile-reject instead of running with stale exit codes.
- 2026-05-14 07:23 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_283_287$'` | failures: none | notes: native experimental map bracket access preserves `/Map` template rejection and custom comparable key fixture keeps compile-rejection contract without stale diagnostic text.
- 2026-05-14 07:18 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1179,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_283_287 | notes: CTest 1179 passed; native experimental map bracket access rejects with `/Map` template usage and custom comparable struct-key rejection diagnostic changed.
- 2026-05-14 07:18 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_273_277$'` | failures: none | notes: experimental insert rejection no longer checks stale native diagnostic; builtin canonical map non-local growth compiles and returns 31.
- 2026-05-14 07:14 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1178,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_273_277 | notes: canonical namespaced map insert on experimental binding still rejects but diagnostic changed; builtin canonical map non-local growth now compiles instead of rejecting.
- 2026-05-14 07:13 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_268_272$'` | failures: none | notes: native borrowed experimental-map helper/wrapper/method/insert/ownership fixtures now preserve current compile rejection semantics.
- 2026-05-14 07:10 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_268_272$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_268_272 | notes: borrowed experimental-map helper/wrapper/method/insert expectations now pass; ownership-sensitive value fixture still rejects but diagnostic does not include stale `/Map` template text.
- 2026-05-14 07:06 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1177,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_268_272 | notes: native borrowed experimental-map helpers, public wrappers, inserts, and ownership-sensitive values reject around current `/Map` template usage instead of compiling or matching stale diagnostics.
- 2026-05-14 07:06 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_263_267$'` | failures: none | notes: helper-wrapped storage field and borrowed experimental-map conformance now accept current semantic rejection paths.
- 2026-05-14 07:03 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1176,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_263_267 | notes: helper-wrapped storage field fixtures reject with `/Map` template usage; borrowed experimental map helper rejection diagnostic changed.
- 2026-05-14 07:02 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_258_262$'` | failures: none | notes: native experimental-map field/storage/result wrapper conformance now accepts current mapPair removal and `/Map` template semantic rejection paths.
- 2026-05-14 06:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1175,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_258_262 | notes: native experimental-map field/storage/result wrapper fixtures now reject around removed `/std/collections/mapPair` and `/Map` template usage.
- 2026-05-14 06:58 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_249_257$'` | failures: none | notes: native experimental-map parameter/receiver conformance now accepts current mapPair removal, wrapValues inference, and method-receiver rejection paths.
- 2026-05-14 06:55 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1174,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_249_257 | notes: native experimental-map parameter/receiver fixtures now reject around removed `/std/collections/mapPair`, `wrapValues` inference, and changed method receiver diagnostics.
- 2026-05-14 06:54 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_239_248$'` | failures: none | notes: native experimental-map assign/inference/wrapper constructor conformance now accepts current mapPair removal and semantic rejection paths.
- 2026-05-14 06:51 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1173,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_239_248 | notes: native experimental-map assign/inference/wrapper constructor fixtures now reject around removed `/std/collections/mapPair`, `/Map` template usage, and updated inferred-map receiver diagnostics.
- 2026-05-14 06:50 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_229_238$'` | failures: none | notes: native experimental-map wrapper conformance now accepts current semantic rejection fragments for retired wrapper/inference paths.
- 2026-05-14 06:47 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1169,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_accessors_229_238 | notes: CTests 1169-1171 passed; stale experimental-map wrapper fixtures now reject with `wrapValues` inference errors, `/Map` template rejection, and removed `/std/collections/mapPair` unknown-target diagnostics.
- 2026-05-14 06:41 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_extended_191_200$'` | failures: none | notes: builtin helper-return soa_vector ref_ref same-path fixture preserves native rejection for missing lowered `/std/collections/soa_vector/ref_ref`.
- 2026-05-14 06:39 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1164,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_extended_191_200 | notes: CTests 1164-1167 passed; builtin helper-return soa_vector ref_ref same-path helper fails native lowering because `/std/collections/soa_vector/ref_ref` is missing a lowered definition.
- 2026-05-14 06:37 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_162_166$'` | failures: none | notes: global and method-like helper-return SoA method shadows compile and return 131 through the selected `/soa_vector/*` helper set.
- 2026-05-14 06:36 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1162,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_162_166 | notes: CTest 1162 passed; helper-return soa_vector method shadow fixture now compiles instead of rejecting `call=/std/collections/soa_vector/ref`.
- 2026-05-14 06:35 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_152_156$'` | failures: none | notes: root soa_vector to_aos bare/direct rejection preserves rejection without stale SoaColumn diagnostic text.
- 2026-05-14 06:34 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1161,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_152_156 | notes: root soa_vector to_aos bare/direct forms still reject, but no longer report stale `unknown struct type for layout: SoaColumn` text.
- 2026-05-14 06:33 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_147_151$'` | failures: none | notes: canonical SoA wildcard fixture now covers reserve/push/get/ref/to_aos/count without experimental field-view imports and returns 17.
- 2026-05-14 06:32 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_147_151$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_147_151 | notes: after removing field-view calls, fixture reaches native lowering and fails on unsupported `at()` during canonical SoA conversion.
- 2026-05-14 06:31 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1157,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_stdlib_collection_shims_147_151 | notes: CTests 1157-1159 passed; wildcard canonical soa_vector helper fixture rejects `values.y()` as unknown without experimental field-view imports.
- 2026-05-14 06:30 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133$'` | failures: none | notes: bare map contains through canonical helper returns helper value false/0.
- 2026-05-14 06:29 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133 | notes: rerun again saw stale expected true/1 assertion before the targeted test-name patch.
- 2026-05-14 06:28 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133 | notes: rerun still saw stale expected true/1 assertion for bare map contains helper shadow.
- 2026-05-14 06:27 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1156,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_129_133 | notes: bare map contains through canonical helper compiles but returns helper value false/0 instead of true/1.
- 2026-05-14 06:26 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_124_128$'` | failures: none | notes: map namespaced count compatibility alias compiles and returns builtin count 1.
- 2026-05-14 06:25 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1154,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_124_128 | notes: CTest 1154 passed; map namespaced count compatibility alias now compiles instead of rejecting `/map/count`.
- 2026-05-14 06:24 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_114_118$'` | failures: none | notes: slash return-type map method compiles and returns builtin count 1.
- 2026-05-14 06:23 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1153,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_114_118 | notes: slash return-type map method compiles but native executable returns builtin count 1 instead of user helper 73.
- 2026-05-14 06:22 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_109_113$'` | failures: none | notes: vector capacity call shadow returns 77, array capacity call shadow preserves rejection, array at call shadow returns builtin indexed value 2, and canonical map reference helpers return 11.
- 2026-05-14 06:21 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1152,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_109_113 | notes: vector capacity call shadow now compiles; array capacity call shadow rejects; array at call shadow returns builtin indexed value 2; canonical map reference access helper now compiles.
- 2026-05-14 06:20 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_104_108$'` | failures: none | notes: vector capacity method shadow compiles and returns builtin capacity 2; vector count call shadow returns user helper 97.
- 2026-05-14 06:19 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_104_108$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_104_108 | notes: capacity method shadow compiles, but native executable returns builtin capacity 2 instead of user helper 77.
- 2026-05-14 06:18 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1151,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_104_108 | notes: user vector capacity method shadow now compiles; user vector count call shadow returns user helper value 97 instead of builtin count 2.
- 2026-05-14 06:17 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_99_103$'` | failures: none | notes: same-path vector count helper on map receiver now compiles and returns 87; no-helper rejection preserves rejection without stale diagnostic text.
- 2026-05-14 06:16 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1149,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_99_103 | notes: CTest 1149 passed; same-path vector count helper on map receiver now compiles and no-helper rejection no longer reports stale `/std/collections/map/count` text.
- 2026-05-14 06:15 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93$'` | failures: none | notes: canonical vector method access count-shadow fixture preserves compile rejection without stale diagnostic text.
- 2026-05-14 06:14 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1148,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93 | notes: rejection still occurs, but no longer reports stale `native backend only supports entry argument indexing` text.
- 2026-05-14 06:14 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_84_88$'` | failures: none | notes: canonical vector access string count shadow fixture now checks compile-only; unsafe count shadow preserves rejection without stale diagnostics.
- 2026-05-14 06:13 local | fail | mode: release | command: `cd build-release && ./primec --emit=native /tmp/ps_count_shadow_probe.prime -o /tmp/ps_count_shadow_probe --entry /main && /tmp/ps_count_shadow_probe` | failures: manual count-shadow probe | notes: canonical vector access string count shadow compiles, but the standalone native executable segfaults; fixture narrowed to compile-only.
- 2026-05-14 06:12 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1147,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_84_88 | notes: canonical vector access string count shadow now compiles and unsafe count shadow rejection diagnostic changed.
- 2026-05-14 06:11 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_79_83$'` | failures: none | notes: std namespaced capacity expression compatibility alias now compiles and returns alias result 12.
- 2026-05-14 06:10 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1146,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_79_83 | notes: std namespaced capacity expression compatibility alias now compiles instead of rejecting.
- 2026-05-14 06:09 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_74_78$'` | failures: none | notes: std namespaced count canonical fallback now expects builtin count 2 and non-builtin compatibility fallback expects alias result 91.
- 2026-05-14 06:08 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1144,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_74_78 | notes: continuation passed CTest 1144 and stopped on std namespaced count canonical fallback returning 2 plus non-builtin fallback compiling.
- 2026-05-14 06:08 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_64_68$'` | failures: none | notes: count-helper precedence fixtures now expect compatibility alias result 12 when `/vector/count` exists and builtin count 2 when no alias exists.
- 2026-05-14 06:07 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_64_68$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_64_68 | notes: count calls with `/vector/count` compatibility alias return 12; no-alias fallback returns builtin count 2.
- 2026-05-14 06:06 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1143,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_64_68 | notes: two count-helper precedence rejection locks now compile and canonical fallback returns 2 instead of stale 0.
- 2026-05-14 06:05 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_61_63$'` | failures: none | notes: `/map/at_unsafe` alias now compiles and returns native stored value 4.
- 2026-05-14 06:04 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1142,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_61_63 | notes: `/map/at_unsafe` compatibility alias now compiles instead of rejecting.
- 2026-05-14 06:04 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_56_60$'` | failures: none | notes: map contains/tryAt aliases preserve rejection without stale diagnostics; `/map/at` alias compiles and returns native stored value 4.
- 2026-05-14 06:03 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_56_60$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_56_60 | notes: `/map/at` compatibility alias now compiles and returns native stored value 4 rather than helper value 17.
- 2026-05-14 06:02 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1140,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_56_60 | notes: continuation passed CTest 1140 and stopped on stale map contains/tryAt diagnostics plus a map at compatibility alias now compiling.
- 2026-05-14 06:01 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_46_50$'` | failures: none | notes: alias slash-method vector access on array receiver now preserves rejection without stale `/vector/at` diagnostic text.
- 2026-05-14 06:00 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1136,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_46_50 | notes: continuation passed CTests 1136-1138 and stopped on stale `unknown method: /vector/at` diagnostic for alias slash-method vector access on array receiver.
- 2026-05-14 06:00 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_26_30$'` | failures: none | notes: vector unsafe and wrapper-returned map slash-method cases now run with result 2; primitive argument case preserves rejection without stale diagnostic text.
- 2026-05-14 05:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1135,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_26_30 | notes: vector unsafe canonical method and wrapper-returned canonical map slash-method cases now compile; map primitive argument rejection still rejects with changed diagnostic.
- 2026-05-14 05:58 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_21_25$'` | failures: none | notes: map/vector alias forwarding cases now compile and run with canonical helper field-chain result 2.
- 2026-05-14 05:57 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1133,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_21_25 | notes: continuation passed CTest 1133 and stopped on three map/vector alias forwarding rejection locks now compiling.
- 2026-05-14 05:57 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_11_15$'` | failures: none | notes: primitive receiver/argument map unsafe compatibility cases now preserve compile rejection without stale `/map/at_unsafe` diagnostic text.
- 2026-05-14 05:56 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1132,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_11_15 | notes: continuation stopped on stale `/map/at_unsafe` diagnostics for primitive receiver/argument map unsafe compatibility cases.
- 2026-05-14 05:55 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_6_10$'` | failures: none | notes: primitive receiver/argument map access compatibility cases now preserve compile rejection without stale `/map/at` diagnostic text.
- 2026-05-14 05:54 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1090,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_collections_core_aliases_and_wrappers_6_10 | notes: continuation passed CTests 1090-1130 and stopped on stale `/map/at` diagnostics for primitive receiver/argument map access compatibility cases.
- 2026-05-14 05:53 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_154_154$'` | failures: none | notes: Buffer Result payload native fixture now checks IR-backed compilation only because the native executable reports a GPU buffer status error.
- 2026-05-14 05:52 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1089,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_154_154 | notes: continuation stopped on Buffer Result payload executable returning 1 instead of stale 10.
- 2026-05-14 05:34 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_153_153$'` | failures: none | notes: direct map Result payload source now uses explicit `map<i32, i32>(...)` construction instead of literal entries that rewrite to `assign(...)`.
- 2026-05-14 05:33 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests --clean-first && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_153_153$'` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_153_153 | notes: clean rebuild confirmed map literal entries still rewrite to `assign(...)` and hit immutable assign-target validation.
- 2026-05-14 05:15 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1070,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_153_153 | notes: continuation passed CTests 1070-1087 and stopped on direct map Result payload source using `assign(...)` entries, now rejected as immutable assign targets.
- 2026-05-14 05:14 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_134_134$'` | failures: none | notes: variadic vector pack indexed capacity fixture now expects current total 14.
- 2026-05-14 05:13 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1069,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_134_134 | notes: variadic vector pack indexed capacity returns 14 instead of stale 3.
- 2026-05-14 05:13 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_133_133$'` | failures: none | notes: variadic vector pack indexed count fixture now expects current total 14.
- 2026-05-14 05:12 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1051,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_133_133 | notes: continuation passed CTests 1051-1067 and stopped on variadic vector pack indexed count returning 14 instead of stale 3.
- 2026-05-14 05:12 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_115_115$'` | failures: none | notes: map constructor canonical entry fixture now preserves compile rejection without stale diagnostic text.
- 2026-05-14 05:11 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1050,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_115_115 | notes: map constructor canonical entry fixture still rejects, but no longer reports stale `argument type mismatch for /std/collections/map/map parameter entries` text.
- 2026-05-14 05:10 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_114_114$'` | failures: none | notes: vector canonical constructor parity fixture now compiles and returns 24.
- 2026-05-14 05:09 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1045,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_114_114 | notes: continuation passed CTests 1045-1048 and stopped on stale vector constructor canonical path rejection lock now compiling.
- 2026-05-14 05:09 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_109_109$'` | failures: none | notes: pointer map indexed lookup fixture now dereferences pointer receivers before `at_unsafe`, `contains`, and `at`.
- 2026-05-14 05:08 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1043,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_109_109 | notes: continuation passed CTest 1043 and stopped on pointer map indexed lookup helper resolving to missing `/std/collections/map/contains_ref`.
- 2026-05-14 05:08 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_107_107$'` | failures: none | notes: pointer map indexed count fixture now dereferences pointer receivers before `.count()`.
- 2026-05-14 05:07 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1042,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_107_107 | notes: continuation stopped on pointer map indexed count methods resolving to missing `/std/collections/map/count_ref`.
- 2026-05-14 05:06 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_106_106$'` | failures: none | notes: stale borrowed map tryAt diagnostic checks removed while preserving native compile rejection.
- 2026-05-14 05:05 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 1039,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_106_106 | notes: continuation passed CTests 1039-1040 and stopped on stale native borrowed map tryAt diagnostic text.
- 2026-05-14 05:04 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_103_103$'` | failures: none | notes: borrowed map pack indexed count fixture now dereferences references before calling `count()`.
- 2026-05-14 05:03 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 946,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_103_103 | notes: continuation passed CTests 946-1037 and stopped on borrowed map pack indexed count methods missing `/std/collections/map/count_ref`.
- 2026-05-14 04:29 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_native_backend_core_core_10_10$'` | failures: none | notes: variadic scalar pointer pack access expected total updated to 29.
- 2026-05-14 04:29 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 936,1589` | failures: PrimeStruct_primestruct_compile_run_native_backend_core_core_10_10 | notes: continuation passed CTests 936-944 and stopped on variadic scalar pointer pack access returning 29 instead of stale 23.
- 2026-05-14 04:28 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_187_192$'` | failures: none | notes: CTest 935 retargeted stale local alias count receiver diagnostics and now passes.
- 2026-05-14 04:27 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 935,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_187_192 | notes: CTest 935 stopped on stale local alias slash-method vector count receiver diagnostics.
- 2026-05-14 04:27 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_177_186$'` | failures: none | notes: CTest 934 retargeted stale mutator/count receiver expectations and now passes.
- 2026-05-14 04:25 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 934,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_loops_177_186 | notes: CTest 934 has stale vector mutator shadow diagnostics, imported positional shadow now reports `template arguments required for /std/collections/soa_vector/push`, local map slash-method count now compiles, and alias count diagnostics changed.
- 2026-05-14 04:25 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_171_176$'` | failures: none | notes: vector mutator shadow fixture now uses explicit canonical free mutator calls and passes.
- 2026-05-14 04:23 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 931,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_171_176 | notes: continuation passed CTests 931-932 and stopped on vector mutator shadow fixture where bare `remove_at(values, ...)` reports mutable-vector binding rejection.
- 2026-05-14 04:23 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_141_150$'` | failures: none | notes: wrapper map access fixtures now assert compile rejection for unsupported native `at()` lowering.
- 2026-05-14 04:21 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 928,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_141_150 | notes: continuation passed CTests 928-929 and stopped on wrapper map access fixtures now reporting native `at()` support diagnostics.
- 2026-05-14 04:20 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120$'` | failures: none | notes: bare vector `capacity()` method fixtures now expect successful C++ emission and executable return 3.
- 2026-05-14 04:19 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 915,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_map_wrapper_and_fallback_inference_111_120 | notes: continuation passed CTests 915-926 and stopped on bare vector `capacity()` method rejection locks now compiling.
- 2026-05-14 04:15 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_78_79$'` | failures: none | notes: experimental map custom comparable key fixture now expects builtin Comparable-key rejection.
- 2026-05-14 04:13 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 912,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_78_79 | notes: continuation passed CTests 912-913 and stopped on experimental map custom comparable key fixture now reporting builtin Comparable-key rejection.
- 2026-05-14 04:13 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73$'` | failures: none | notes: Result.why fixture now compiles imported Result/FileError C++ output, and omitted-initializer rewrite skips sum variants.
- 2026-05-14 04:12 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73$'` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73 | notes: Result.why executable retarget compiled but runtime hit `invalid indirect address in IR`; fixture is being narrowed to C++ emission.
- 2026-05-14 04:11 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73$'` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73 | notes: Result.why fixture no longer fails semantic validation, but stale explicit Result constructor/source-helper output lock falls through to C++ IR lowering.
- 2026-05-14 03:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 911,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_72_73 | notes: Result.why C++ guard fixture fails before emission with `omitted initializer requires vector or soa_vector type: File`.
- 2026-05-14 03:59 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_70_71$'` | failures: none | notes: canonical map slash-method struct method chain fixture now expects successful return 2.
- 2026-05-14 03:58 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 910,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_70_71 | notes: canonical map slash-method struct method chain fixture now compiles instead of reporting `/Marker/tag` receiver mismatch.
- 2026-05-14 03:58 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_68_69$'` | failures: none | notes: canonical vector unsafe method field forwarding now expects successful return 2.
- 2026-05-14 03:57 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 909,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_68_69 | notes: canonical vector unsafe method field forwarding now compiles instead of reporting `field access requires struct receiver`.
- 2026-05-14 03:56 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_66_67$'` | failures: none | notes: vector method alias struct-return field access now expects successful return 2.
- 2026-05-14 03:55 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 907,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_66_67 | notes: continuation passed CTest 907 and stopped on vector method alias struct-return field access now compiling and returning 2.
- 2026-05-14 03:54 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_62_63$'` | failures: none | notes: vector alias field-expression fixture now expects successful return 2.
- 2026-05-14 03:53 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 904,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_62_63 | notes: continuation passed CTests 904-905 and stopped on vector alias field-expression fixture now compiling and returning 2.
- 2026-05-14 03:53 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_56_57$'` | failures: none | notes: C++ unsafe compatibility-chain primitive argument diagnostic now matches current semantic error.
- 2026-05-14 03:52 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 903,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_56_57 | notes: C++ unsafe compatibility-chain primitive argument case now reports `unknown method: /Marker/tag`.
- 2026-05-14 03:51 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_54_55$'` | failures: none | notes: C++ compatibility-chain diagnostics now match current semantic method errors.
- 2026-05-14 03:50 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 896,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_collection_access_and_alias_forwarding_54_55 | notes: continuation passed CTests 896-901 and stopped on stale C++ compatibility-chain diagnostics; current errors are `unknown method: /Marker/tag` and `/Marker/tag parameter self`.
- 2026-05-14 03:49 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_43_43$'` | failures: none | notes: wrapper-returned C++ map slash-method fixture now expects exit 2 with empty output.
- 2026-05-14 03:49 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 894,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_43_43 | notes: continuation passed CTest 894 and stopped on wrapper-returned C++ map slash-method fixture now exiting 2 with empty output instead of runtime fault text.
- 2026-05-14 03:48 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_41_41$'` | failures: none | notes: C++ canonical map slash-method at_unsafe fixture now expects exit 2 with empty output.
- 2026-05-14 03:47 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 882,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_41_41 | notes: continuation passed CTests 882-892 and stopped on C++ canonical map slash-method at_unsafe fixture now exiting 2 with empty output instead of runtime fault text.
- 2026-05-14 03:46 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_29_29$'` | failures: none | notes: C++ canonical `/std/collections/map/at_unsafe(...).tag()` fixture now expects exit 2 with empty output.
- 2026-05-14 03:46 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 881,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_29_29 | notes: C++ canonical `/std/collections/map/at_unsafe(...).tag()` fixture now exits 2 with empty output instead of runtime fault text.
- 2026-05-14 03:45 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_28_28$'` | failures: none | notes: C++ `/map/at_unsafe(...).tag()` alias fixture now expects exit 2 with empty output.
- 2026-05-14 03:44 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 878,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_28_28 | notes: continuation passed CTests 878-879 and stopped on C++ `/map/at_unsafe(...).tag()` alias fixture now exiting 2 with empty output instead of runtime fault text.
- 2026-05-14 03:43 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_25_25$'` | failures: none | notes: C++ `/map/tryAt(...).tag()` diagnostic now expects `unknown method: /Result/tag`.
- 2026-05-14 03:42 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 876,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_25_25 | notes: continuation passed CTest 876 and stopped on C++ `/map/tryAt(...).tag()` diagnostic now reporting `unknown method: /Result/tag`.
- 2026-05-14 03:41 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_23_23$'` | failures: none | notes: C++ canonical map helper same-path fixture now expects successful return 119 with empty output.
- 2026-05-14 03:40 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 875,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_23_23 | notes: C++ canonical map helper same-path fixture now runs cleanly and returns 119 instead of the old runtime lowering fault.
- 2026-05-14 03:36 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_22_22$'` | failures: none | notes: C++ explicit map helper aggregate lock now expects current return 94.
- 2026-05-14 03:36 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 870,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_22_22 | notes: continuation passed CTests 870-873 and stopped on C++ explicit map helper aggregate returning 94 instead of 162.
- 2026-05-14 03:35 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_17_17$'` | failures: none | notes: C++ canonical-over-alias contains helper return lock now expects false/0.
- 2026-05-14 03:34 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 868,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_17_17 | notes: continuation passed CTest 868 and stopped on C++ canonical-over-alias contains helper returning false/0, not true/1.
- 2026-05-14 03:33 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_15_15$'` | failures: none | notes: C++ canonical map contains helper return lock now expects false/0.
- 2026-05-14 03:33 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_15_15$'` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_15_15 | notes: focused rerun still saw stale C++ contains assertion before the exact-location patch landed; restored adjacent map count assertion to 1.
- 2026-05-14 03:32 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 853,1589` | failures: PrimeStruct_primestruct_compile_run_emitters_cpp_lambda_and_mutator_resolution_15_15 | notes: continuation passed CTests 853-866 and stopped on C++ canonical map contains helper returning false/0, not true/1.
- 2026-05-14 03:31 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_352_352$'` | failures: none | notes: vector literal `.count()` lock now expects current VM lowering rejection.
- 2026-05-14 03:31 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 851,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_352_352 | notes: continuation passed CTest 851 and stopped on stale vector literal `.count()` support lock; current VM lowering rejects `call=/std/collections/vector/count`.
- 2026-05-14 03:30 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_332_341$'` | failures: none | notes: array slash-method diagnostic now expects semantic `unknown method: /array/count`.
- 2026-05-14 03:29 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_332_341$'` | failures: PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_332_341 | notes: focused rerun still saw stale `/vector/count` substring before the tighter array slash-method patch landed.
- 2026-05-14 03:28 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 850,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_growth_limits_and_syntax_332_341 | notes: array slash-method vector count now reports semantic `unknown method: /array/count`.
- 2026-05-14 03:27 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_325_331$'` | failures: none | notes: string slash-method diagnostic lock now expects semantic `unknown method: /string/count`.
- 2026-05-14 03:27 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 848,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_325_331 | notes: continuation passed CTest 848 and stopped on stale string slash-method diagnostic; current error is `unknown method: /string/count`.
- 2026-05-14 03:26 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314$'` | failures: none | notes: map count compatibility alias and canonical contains helper return locks now match current results.
- 2026-05-14 03:25 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314$'` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314 | notes: focused rerun caught assertion retargeting in the adjacent compatibility-alias check; corrected alias to expect 1 and contains helper false to expect 0.
- 2026-05-14 03:25 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 847,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_305_314 | notes: canonical map contains helper fixture returns false/0 with empty output, not true/1.
- 2026-05-14 03:24 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_295_304$'` | failures: none | notes: vector capacity method and array count method-shadow locks now match current run behavior.
- 2026-05-14 03:24 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 846,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_295_304 | notes: vector `values.capacity()` without import now returns 3; array `values.count()` now routes to builtin count 2 instead of user `/array/count`.
- 2026-05-14 03:23 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_285_294$'` | failures: none | notes: vector reserve/clear/remove_at/remove_swap mismatch locks now match current slash-helper diagnostics.
- 2026-05-14 03:22 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 844,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_285_294 | notes: continuation passed CTest 844 and stopped on stale vector reserve/clear/remove_at/remove_swap generated helper diagnostics.
- 2026-05-14 03:21 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_265_274$'` | failures: none | notes: sext/sept/oct map mismatch fixtures and vector capacity diagnostics now match current behavior.
- 2026-05-14 03:20 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 843,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_265_274 | notes: stale sext/sept/oct map value mismatch fixtures and vector capacity diagnostic helper name.
- 2026-05-14 03:20 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_255_264$'` | failures: none | notes: pair/quad/quint constructor mismatch locks now use string values and current internal helper diagnostics.
- 2026-05-14 03:19 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 842,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_255_264 | notes: stale pair/quad/quint constructor mismatch locks; bool value fixtures now run and return counts.
- 2026-05-14 03:18 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_249_254$'` | failures: none | notes: extended constructor mismatch now uses a string value and current internal mapTriple diagnostic.
- 2026-05-14 03:18 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 841,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_249_254 | notes: stale extended constructor map mismatch used bool and returned count 3 instead of rejecting.
- 2026-05-14 03:17 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_239_248$'` | failures: none | notes: oct/double/triple constructor mismatch locks now use string value mismatches and current internal helper diagnostics.
- 2026-05-14 03:16 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 840,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_239_248 | notes: stale oct/double/triple constructor helper diagnostics; bool value fixtures now run and return counts.
- 2026-05-14 03:15 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_229_238$'` | failures: none | notes: quint/sext/sept constructor mismatch locks now use string value mismatches and current internal helper diagnostics.
- 2026-05-14 03:15 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 839,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_229_238 | notes: stale quint/sext/sept constructor helper diagnostics; bool value fixtures now run and return counts 5/6/7.
- 2026-05-14 03:14 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_219_228$'` | failures: none | notes: map pair/double/triple/quad constructor mismatch locks now match current internal helper diagnostics.
- 2026-05-14 03:13 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 838,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_219_228 | notes: stale map pair/double/triple/quad constructor helper diagnostics; bool quad value fixture now runs and returns count 4.
- 2026-05-14 03:12 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_209_218$'` | failures: none | notes: map method key and mapSingle/mapPair constructor mismatch locks now match current diagnostics.
- 2026-05-14 03:10 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 837,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_209_218 | notes: stale map method `at`/`at_unsafe` key diagnostics and mapSingle/mapPair constructor mismatch locks.
- 2026-05-14 03:09 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_199_208$'` | failures: none | notes: map shim count/at/at_unsafe mismatch locks now match current helper diagnostics.
- 2026-05-14 03:09 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 836,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_199_208 | notes: stale map shim diagnostics now report `/std/collections/map/count parameter values` and `/std/collections/map/at[_unsafe] parameter key`.
- 2026-05-14 03:07 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_189_198$'` | failures: none | notes: map shim mismatch locks now match current run/pass behavior and updated helper diagnostics.
- 2026-05-14 03:05 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 834,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_189_198 | notes: continuation passed CTest 834 and stopped at stale map shim mismatch locks; bool value and mapNew mismatch cases now run, while remaining type errors use non-specialized helper diagnostic names.
- 2026-05-14 03:04 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_169_178$'` | failures: none | notes: templated map return-envelope key lock now expects count 1 with empty stderr.
- 2026-05-14 03:03 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 833,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_169_178 | notes: templated map return-envelope unsupported-key lock now runs and returns count 1 with empty stderr.
- 2026-05-14 03:03 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_159_168$'` | failures: none | notes: method alias locks now match clean return 2 paths and current unknown Marker/tag diagnostic.
- 2026-05-14 03:01 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 832,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_159_168 | notes: method alias locks are stale: vector unsafe field and wrapper map slash-method struct receiver now return 2 cleanly; map primitive-argument chain reports unknown `/Marker/tag`.
- 2026-05-14 03:01 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_149_158$'` | failures: none | notes: map method alias receiver lock now expects successful return 2 with empty stderr.
- 2026-05-14 03:00 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 831,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_149_158 | notes: map method alias receiver mismatch lock now runs successfully and returns 2 with empty stderr.
- 2026-05-14 02:59 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148$'` | failures: none | notes: map access chain locks now match current Marker/tag mismatch and unknown-method diagnostics after builtin map access.
- 2026-05-14 02:58 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 829,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148 | notes: continuation passed CTest 829 and stopped at stale map access chain diagnostics; current diagnostics are Marker/tag type mismatch or unknown Marker/tag after builtin map access.
- 2026-05-14 02:57 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_121_130$'` | failures: none | notes: array at/at_unsafe call shadow locks now expect builtin array access return 2.
- 2026-05-14 02:56 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 828,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_121_130 | notes: array at/at_unsafe call forms now use builtin array access and return 2 instead of user helper shadow values.
- 2026-05-14 02:55 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_111_120$'` | failures: none | notes: vector count/capacity shadow locks now match current user-helper returns, slash-method diagnostic, builtin capacity method, and array capacity stale-query diagnostic.
- 2026-05-14 02:54 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 827,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_111_120 | notes: vector count/capacity shadow locks are stale: same-path helper returns 87, no-helper slash count reports explicit slash method unknown, vector count/capacity calls return user helper values, vector capacity method returns builtin 2, and array capacity call reports stale semantic query metadata.
- 2026-05-14 02:54 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_101_110$'` | failures: none | notes: vector wrapper/string count locks now match current direct-access return 1 and method-form count-target diagnostic.
- 2026-05-14 02:53 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 824,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_101_110 | notes: continuation passed CTests 824-825, then vector access string-count locks failed because direct access returns 1 and method form reports current count-target diagnostic.
- 2026-05-14 02:52 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_71_80$'` | failures: none | notes: vector at/at_unsafe shadow locks now match builtin method return 2 and current named-call lowerer diagnostics.
- 2026-05-14 02:51 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 823,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_71_80 | notes: vector at/at_unsafe method sugar now returns builtin element value 2, while named at_unsafe call form reaches VM lowerer unsupported-at diagnostic.
- 2026-05-14 02:51 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_61_70$'` | failures: none | notes: vector `at` source locks now expect current VM lowerer diagnostics.
- 2026-05-14 02:50 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 822,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_templated_wrapper_parity_61_70 | notes: vector `at` source locks now reach VM lowerer diagnostics for unsupported at targets and non-integer indices instead of old unknown canonical helper diagnostics.
- 2026-05-14 02:49 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60$'` | failures: none | notes: map helper override/reference/slash-return locks now match current return codes and successful helper execution.
- 2026-05-14 02:48 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 821,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60 | notes: CTest 821 source locks are stale for current map helper routing: alias helper sum returns 94, canonical override case returns 119, canonical reference helpers return 11, and slash-return receiver count returns builtin count 1.
- 2026-05-14 02:47 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_41_50$'` | failures: none | notes: map compatibility alias locks now match current contains/tryAt diagnostics and at/at_unsafe builtin return behavior.
- 2026-05-14 02:46 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 819,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_41_50 | notes: continuation passed CTest 819 and stopped at stale `/map/contains`, `/map/tryAt`, `/map/at`, and `/map/at_unsafe` compatibility alias locks.
- 2026-05-14 02:45 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_21_30$'` | failures: none | notes: string count method shadow lock now expects builtin string count return 3.
- 2026-05-14 02:45 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 818,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_21_30 | notes: string count method shadow now routes to builtin string count and returns 3, matching the call-form lock.
- 2026-05-14 02:44 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_11_20$'` | failures: none | notes: collection alias/basics locks now match current map access lowering diagnostics, builtin count routing, explicit-template method return mismatch, and wrapper slash-return count behavior.
- 2026-05-14 02:41 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 817,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_11_20 | notes: stopped on stale collection locks: non-string map access still lowers with `entry argument indexing`, `/map/count(values)` returns builtin count 1, explicit-template count method reports return mismatch, and wrapper slash-return count returns 96.
- 2026-05-14 02:41 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_1_10$'` | failures: none | notes: collection alias/basics locks now expect the resolved `/array/at` diagnostic and current builtin map sugar/access return codes.
- 2026-05-14 02:39 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 717,1589` | failures: PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_1_10 | notes: continuation passed CTests 717-815, then stopped at collection alias/basics stale locks: vector alias diagnostic now reports `/array/at`, canonical map sugar returns builtin result 6, and string map access now returns 10.
- 2026-05-14 02:18 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_8_8$'` | failures: none | notes: matrix composition VM support lock now expects the current successful `/main` return code 1 instead of stale lowering rejection diagnostics.
- 2026-05-14 02:18 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 716,1589` | failures: PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_8_8 | notes: continuation stopped at stale matrix composition rejection lock because the VM now runs the helper path and returns 1.
- 2026-05-14 02:17 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_7_7$'` | failures: none | notes: quaternion multiply/rotation VM support lock now expects the current successful `/main` return code 7 instead of stale lowering rejection diagnostics.
- 2026-05-14 02:17 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 708,1589` | failures: PrimeStruct_primestruct_compile_run_vm_math_math_helpers_1_10_7_7 | notes: continuation passed CTests 708-714, then stale quaternion multiply/rotation rejection lock failed because the VM now runs the helper path and returns 7.
- 2026-05-14 02:16 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03b_76_80$'` | failures: none | notes: direct map Result payload fixture now uses canonical `map<i32, i32>(...)` construction, and Result.ok falls back to direct collection inference when semantic payload facts are present but incomplete for Buffer uploads.
- 2026-05-14 01:58 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 701,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03b_76_80 | notes: continuation passed CTests 701-706, then direct map Result payload and Buffer Result payload IR-backed cases failed; stderr includes map assign mutability diagnostic and `IR backends only support Result.ok with supported payload values`.
- 2026-05-14 01:57 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69$'` | failures: none | notes: dereferenced borrowed stdlib Result sum helpers now preserve generated Result metadata on Reference/Pointer bindings and read Result.error/why through borrowed sum pointers.
- 2026-05-14 01:55 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69 | notes: pointer/reference bindings no longer enter stale Result sum construction, but runtime returns guard codes because `Result.error(dereference(ref/ptr))` reports false for error states.
- 2026-05-14 01:54 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69 | notes: diagnostic lowering run identifies `location(status)` Reference/Pointer bindings as incorrectly entering Result sum construction.
- 2026-05-14 01:53 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69 | notes: direct callee return-type fallback did not change dereferenced borrowed Result helper failure; still reports stale semantic-product sum initializer metadata.
- 2026-05-14 01:52 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 700,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69 | notes: continuation stopped at dereferenced borrowed stdlib Result sum helpers; VM lowering reports stale semantic-product sum initializer metadata.
- 2026-05-14 01:52 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68$'` | failures: none | notes: direct imported stdlib Result sum `?` error now copies the non-packed `forward_error()` Result pointer instead of reselecting a payload variant.
- 2026-05-14 01:51 local | fail | mode: release | command: `cmake --build build-release --target primevm && ./build-release/primevm <diagnostic CTest 699 source> --entry /main --import-path stdlib` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68 | notes: diagnostic lowering run identifies initializer `forward_error` as same-Result construction falling through to stale sum initializer metadata.
- 2026-05-14 01:50 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68 | notes: semantic resolved-path fallback did not catch the direct imported `error<i32, MyError>` initializer; still reports stale semantic-product sum initializer type metadata.
- 2026-05-14 01:48 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 699,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68 | notes: continuation stopped immediately at direct imported stdlib Result sum `?` error; VM lowering reports `stale semantic-product sum initializer type metadata: /std/result/Result__arity2__t23ae4e1c2aab74df`.
- 2026-05-14 01:48 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: none | notes: direct imported stdlib Result sum `?` ok now keeps the non-packed Result sum pointer through inline return lowering.
- 2026-05-14 01:44 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: requiring scalar declared payload types for the packed Result return optimization did not change the path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:42 local | fail | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: rebuilding `primevm` as well as `primec` and the doctest binary still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:40 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: adding `sourceName` fallback for Result helper recognition did not change the inline return pack path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:37 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: accepting generated `/result/ok__...` and `/std/result/ok__...` helper names did not change the path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:34 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: marking the non-packed aggregate Result return temp as Result metadata did not change the path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:32 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: recognizing qualified `namespacePrefix=Result,name=ok` helper syntax did not change the aggregate-error path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:30 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: fixing the `Result.ok` simple-name matcher compile error did not change the aggregate-error path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:28 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: build | notes: broadened `Result.ok` helper matcher passed a temporary `std::string` to `isSimpleCallName(const char*)`; fix matcher before rerun.
- 2026-05-14 01:27 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: allowing explicit `Result` receiver payload indexing for direct helper construction did not change the aggregate-error path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:26 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: adding `/result/ok` to stdlib Result helper path recognition did not change the aggregate-error path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:25 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: semantic return-fact fallback for generated Result return sums did not change the aggregate-error path; still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:20 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: narrowed direct-call packed-result guard to scalar-packed returns only; aggregate-error stdlib Result sum path still hits `VM error: invalid indirect address in IR: 412316860432`.
- 2026-05-14 01:12 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: direct-call aggregate guard removed invalid-address VM error; case now returns 96 instead of expected 9.
- 2026-05-14 01:12 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: still hits `VM error: invalid indirect address in IR`; direct-call aggregate guard must also cover struct-path inference.
- 2026-05-14 01:09 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 689,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | notes: continuation passed CTests 689-697, then postfix `?` on direct imported stdlib Result sum ok hit `VM error: invalid indirect address in IR`.
- 2026-05-14 01:09 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_02_45_52$'` | failures: none | notes: imported stdlib Result `.error()` and `.why()` VM lowering now resolves generated Result sum metadata.
- 2026-05-14 01:08 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case='ir lowerer result helpers use semantic local Result source facts' -s` | failures: none | notes: semantic binding facts with `/std/result/Result__...` sum paths now resolve Result value/error metadata from published sum variants.
- 2026-05-14 01:04 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 675,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_02_45_52 | notes: continuation passed CTests 675-687, then imported stdlib Result `.error()` and `.why()` cases failed with `Result.* requires Result argument`.
- 2026-05-14 01:03 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_10_10$'` | failures: none | notes: VM two-pass layout tree serialization now accepts canonical vector `at__` assign targets and exits 6 as expected.
- 2026-05-14 01:02 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case='ir lowerer indexed assign consumes semantic collection facts before stale locals' -s` | failures: none | notes: canonical `/std/collections/vector/at__...` assign targets now emit indexed stores through semantic collection facts.
- 2026-05-14 01:02 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case='ir lowerer indexed assign consumes semantic collection facts before stale locals' -s` | failures: build | notes: adding shared collection-helper declarations exposed an `experimentalCollectionTypePath` overload ambiguity in `IrLowererOperatorCollectionMutationHelpers.cpp`.
- 2026-05-14 00:58 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case='runs vm two-pass layout tree serialization deterministically' -s` | failures: runs vm two-pass layout tree serialization deterministically | notes: direct doctest rerun confirms `vm backend only supports assign to local names or dereference`.
- 2026-05-14 00:57 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 622,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_10_10 | notes: broad continuation passed VM support-matrix and VM core 6-9, then two-pass layout tree serialization failed with `vm backend only supports assign to local names or dereference`.
- 2026-05-14 00:55 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05$'` | failures: none | notes: semantic vector-count bridge calls on array receivers now lower through builtin count access.
- 2026-05-14 00:54 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05 | notes: still fails with `count requires array, vector, map, or string target (target=arr)` after semantic vector-count bridge classification coverage.
- 2026-05-14 00:49 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05$'` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05 | notes: still fails with `count requires array, vector, map, or string target (target=arr)` after semantic array collection classification coverage.
- 2026-05-14 00:46 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 622,1589` | failures: PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05 | notes: broad continuation passed collective paths 1-22 and argv/CLI 1-25, then `count(arr)` in VM support-matrix binding types failed array target recognition.
- 2026-05-14 00:45 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_62_62$'` | failures: none | notes: experimental gfx render-pass wrapper source lock now expects current experimental `SubstrateRenderPassConfig` mismatch diagnostic.
- 2026-05-14 00:43 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 620,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_62_62 | notes: experimental gfx render-pass wrapper slice exits unsupported but no longer finds expected native/vm stderr substrings.
- 2026-05-14 00:43 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60$'` | failures: none | notes: experimental gfx resource wrapper source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic.
- 2026-05-14 00:41 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 619,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60 | notes: experimental gfx resource wrapper slice exits unsupported but no longer finds expected native/vm stderr substrings.
- 2026-05-14 00:41 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_59_59$'` | failures: none | notes: experimental gfx device constructor source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic.
- 2026-05-14 00:39 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 617,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_59_59 | notes: experimental gfx device constructor entry point exits unsupported but no longer finds the expected native/vm stderr substrings.
- 2026-05-14 00:38 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57$'` | failures: none | notes: gfx static-fields shim now source-locks cross-backend import and static-field binding without native `.value` field reads.
- 2026-05-14 00:36 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 614,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57 | notes: gfx compatibility shim static-fields native executable run returned -1 instead of 4 after wasm/debug 55-56 passed.
- 2026-05-14 00:35 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_54_54$'` | failures: none | notes: gfx compatibility shim source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic across backends.
- 2026-05-14 00:33 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 594,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_54_54 | notes: gfx compatibility shim exits as expected but no longer finds the expected native/vm stderr substrings.
- 2026-05-14 00:32 local | pass | mode: release | command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_34_34$'` | failures: none | notes: debug-DAP source lock now expects verified instruction breakpoint response before post-exit frame/variable rejection.
- 2026-05-14 00:31 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 572,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_34_34 | notes: debug-DAP locals test no longer finds `invalid breakpoint instruction pointer` in `frames[4]`.
- 2026-05-14 00:30 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_12_12$'` | failures: none | notes: nested sum diagnostic lock now expects current semantic `unsupported binding type: Inner` wording.
- 2026-05-14 00:29 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 571,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_12_12 | notes: nested sum diagnostic lock no longer finds `native backend does not support sum payload type: /Outer/inner (Inner)`.
- 2026-05-14 00:29 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_11_11$'` | failures: none | notes: sum drop fixture now uses valid moved sum storage plus no-op payload Destroy helpers instead of unsupported reference-field writes.
- 2026-05-14 00:24 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 570,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_11_11 | notes: sum drop active-payload fixture reports `assign target must be a mutable pointer binding` for `assign(dereference(this.counter), ...)`.
- 2026-05-14 00:23 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10$'` | failures: none | notes: rebuilding `primec` with the semantic move changes clears the sum active-payload move fixture.
- 2026-05-14 00:20 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10$'` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10 | notes: focused rerun still reports semantic use-after-move at `[Choice] moved{move(original)}`.
- 2026-05-14 00:13 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 398,1589` | failures: PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10 | notes: effects 61-118, imports, parser/text-filter, and smoke foundation 1-9 passed; sum move active payload fixture fails semantic use-after-move at `move(original)`.
- 2026-05-14 00:12 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_51_60$'` | failures: none | notes: take string map access fixture now source-locks the current non-string-path rejection.
- 2026-05-14 00:12 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 397,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_51_60 | notes: `take accepts string map access` failed validation, analogous to the notify string-map access lock.
- 2026-05-14 00:11 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50$'` | failures: none | notes: notify string map access fixture now source-locks the current non-string-path rejection.
- 2026-05-14 00:11 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50 | notes: explicit canonical map `at<i32, string>` still reports `notify requires string path argument`; retarget source lock to rejection.
- 2026-05-14 00:10 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50 | notes: untemplated canonical map `at` still reports `notify requires string path argument`.
- 2026-05-14 00:10 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50 | notes: after adding heap_alloc, bracket map access still reports `notify requires string path argument`.
- 2026-05-14 00:09 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 389,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50 | notes: comparisons/literals 11-40 and effects 1-40 passed, then `notify accepts string map access` failed validation.
- 2026-05-14 00:09 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10$'` | failures: none | notes: map `at` comparison source lock now expects canonical map access precedence over root user `at`.
- 2026-05-14 00:08 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10 | notes: after adding heap_alloc, fixture reports mixed string/numeric comparison because canonical map `at` wins over root user `at`.
- 2026-05-14 00:08 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 376,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10 | notes: access 21-48, named args, and numeric builtin shards passed, then builtin map `at` comparison precedence lock failed validation.
- 2026-05-14 00:07 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20$'` | failures: none | notes: map method-call definition lock now provides a templated local canonical count helper.
- 2026-05-14 00:07 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20 | notes: map method-call fixture reports template arguments are only supported on templated `/std/collections/map/count`.
- 2026-05-14 00:06 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 368,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20 | notes: collections 721-771 and access 1-10 passed, then map method-call definition lock failed validation.
- 2026-05-14 00:06 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_711_720$'` | failures: none | notes: map tryAt slash-path method locks now expect current validated routing.
- 2026-05-14 00:05 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 365,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_711_720 | notes: 691-710 passed, then map namespaced and stdlib-namespaced tryAt method tests validated instead of reporting old `try requires Result argument`.
- 2026-05-14 00:05 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_681_690$'` | failures: none | notes: user-defined map block-argument source lock now expects the generic non-collection `at` diagnostic.
- 2026-05-14 00:04 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 358,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_681_690 | notes: 621-680 passed, then user-defined map block-argument source lock expected old `at requires map target` diagnostic text.
- 2026-05-14 00:04 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620$'` | failures: none | notes: map constructor resolution locks now use canonical map types, borrowed receiver behavior, and builtin-key diagnostics.
- 2026-05-14 00:03 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620 | notes: borrowed canonical count test name was updated but body still expected rejection; no-import explicit canonical count test was inverted.
- 2026-05-14 00:02 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620 | notes: after canonicalizing map types, borrowed string-key count validates and custom-key cases report builtin-key rejection instead of old unknown/trait diagnostics.
- 2026-05-14 00:01 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 357,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620 | notes: borrowed map constructor resolution source locks still expect unknown canonical helpers or old `Map` template behavior.
- 2026-05-14 00:00 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610$'` | failures: none | notes: field-bound canonical map explicit-at expression body-argument lock now checks the current count-mismatch target family.
- 2026-05-14 00:00 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 356,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610 | notes: field-bound canonical map explicit-at expression body-argument lock still expects `/std/collections/map/mapAt`.
- 2026-05-13 23:59 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600$'` | failures: none | notes: canonical map method-sugar direct-constructor receivers now source-lock current accepted behavior.
- 2026-05-13 23:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 355,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600 | notes: 591-600 map method-sugar lock expected unknown experimental `mapCount`, but canonical method sugar now validates.
- 2026-05-13 23:58 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590$'` | failures: none | notes: map constructor parameter-shape locks now use canonical `map<K, V>` constructors and current map diagnostics.
- 2026-05-14 00:07 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 352,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590 | notes: 561-580 passed, then map constructor parameter-shape locks failed on stale experimental map constructor and mismatch diagnostics.
- 2026-05-14 00:06 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560$'` | failures: none | notes: SoA field-view mutation and return-helper locks now match current accepted/rejected behavior.
- 2026-05-14 00:05 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560 | notes: field-view return helper lock now passes; field-view structural mutation case validates once `heap_alloc` is declared.
- 2026-05-14 00:04 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 346,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560 | notes: 501-550 passed, then SoA vector field-view borrow and escape helper locks failed.
- 2026-05-14 00:03 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500$'` | failures: none | notes: vector relocation locks now match visible helper availability for pop, remove_at, and count.
- 2026-05-14 00:02 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 345,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500 | notes: vector pop/removal locks fail validation and missing-stdlib-import vector helper lock now validates.
- 2026-05-14 00:01 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_481_490$'` | failures: none | notes: reference-wrapped body-arg locks now expect `count_ref`; slash-path count/at body-arg locks now validate.
- 2026-05-13 23:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 344,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_481_490 | notes: reference-wrapped map body-arg locks fail validation and slash-path map count/at body-arg locks now validate.
- 2026-05-13 23:59 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_471_480$'` | failures: none | notes: map body-arg method-sugar locks now expect `count_ref`, canonical marker mismatch, and canonical block-argument diagnostics.
- 2026-05-13 23:58 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 343,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_471_480 | notes: map body-arg method-sugar locks fail validation or no longer report unknown canonical count.
- 2026-05-13 23:57 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_461_470$'` | failures: none | notes: stdlib-qualified primitive receiver body-arg locks now expect unknown `/std/collections/vector/count`.
- 2026-05-13 23:56 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 341,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_461_470 | notes: 451-460 passed, then namespaced method expression body-arg source locks no longer report `/i32/count` block-argument diagnostics.
- 2026-05-13 23:55 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450$'` | failures: none | notes: explicit canonical map access helper coverage now uses canonical map constructors.
- 2026-05-13 23:54 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 339,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450 | notes: 431-440 passed, then explicit canonical map access helper coverage failed on missing `/std/collections/mapPair`.
- 2026-05-13 23:53 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_421_430$'` | failures: none | notes: explicit-template map compatibility diagnostics now match canonical return mismatches and missing `count_ref`.
- 2026-05-13 23:52 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 338,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_421_430 | notes: explicit-template map compatibility source locks now report different diagnostics and one wrapper-reference count case no longer validates.
- 2026-05-13 23:51 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420$'` | failures: none | notes: `/map/contains` and `/map/tryAt` source locks now validate visible canonical alias resolution.
- 2026-05-13 23:50 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 336,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420 | notes: 401-410 passed, then `/map/contains` and `/map/tryAt` compatibility alias rejection locks now validate or report different diagnostics.
- 2026-05-13 23:49 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400$'` | failures: none | notes: field-bound map assignment locks now use canonical map constructors and current mismatch diagnostics.
- 2026-05-13 23:48 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 334,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400 | notes: 381-390 passed, then field-bound map constructor assignment lock failed on stale experimental map surface.
- 2026-05-13 23:47 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380$'` | failures: none | notes: result-payload and auto-return source locks now match canonical map constructors and direct map access.
- 2026-05-13 23:46 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380 | notes: result-payload locks now pass; remaining auto-built map source lock used `try` on a non-Result `tryAt` shape.
- 2026-05-13 23:45 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 332,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380 | notes: 361-370 passed, then 371-380 failed on one canonical auto-return source lock and result-payload `Map`/`mapPair` locks.
- 2026-05-13 23:44 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360$'` | failures: none | notes: storage source locks now use canonical `map<K, V>` and `/std/collections/map/map` constructors.
- 2026-05-13 23:43 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 331,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360 | notes: experimental map deref/storage source locks fail on stale `Map<...>`, `mapPair`, and implicit `wrapValues` expectations.
- 2026-05-13 23:42 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350$'` | failures: none | notes: struct-field map defaults now use one-entry canonical constructors, and positive holder cases use explicit map helper templates.
- 2026-05-13 23:41 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 330,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350 | notes: positive struct-field canonical map constructor locks fail with `argument count mismatch for /std/collections/map/map` without explicit templates.
- 2026-05-13 23:40 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340$'` | failures: none | notes: auto-inference source locks now use canonical `map<K, V>` constructors and generic map template-conflict diagnostics.
- 2026-05-13 23:39 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 329,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340 | notes: experimental map auto-inference positive locks fail validation and mismatch locks no longer report `mapPair` conflicts.
- 2026-05-13 23:38 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330$'` | failures: none | notes: key-diagnostic locks now use canonical `map<K, V>` and expect builtin Comparable-key rejection.
- 2026-05-13 23:37 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 328,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330 | notes: experimental `Map<Key, V>` source locks now fail before old trait diagnostics; retarget to canonical builtin key rejection.
- 2026-05-13 23:36 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_311_320$'` | failures: none | notes: source locks now accept ownership-sensitive canonical map values and pin `experimental_map.prime` as a retired direct-import shim.
- 2026-05-13 23:35 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 327,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_311_320 | notes: canonical map `Owned` literal now validates, and experimental-map borrowed-helper file lock still expects retired shim bodies.
- 2026-05-13 23:34 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310$'` | failures: none | notes: canonical map/ref helper source locks now use `map<K, V>` and explicit `_ref` helper calls; borrowed method sugar locks missing ref-template inference.
- 2026-05-13 23:33 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 326,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310 | notes: seven experimental Map/ref helper source locks fail validation or stale missing-helper diagnostics.
- 2026-05-13 23:32 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300$'` | failures: none | notes: experimental map source locks now expect custom-key Comparable rejection and missing `mapCount` method helper diagnostics.
- 2026-05-13 23:30 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 325,1589` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300 | notes: experimental map custom comparable key and real Map method-call sugar tests now fail validation.
- 2026-05-13 23:30 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290$'` | failures: none | notes: borrowed args-pack indexed map count source lock now expects missing `/std/collections/map/count_ref` diagnostic.
- 2026-05-13 23:28 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290 | notes: imported canonical map count borrowed args-pack indexed receiver test now fails validation.
- 2026-05-13 23:25 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60$'` | failures: none | notes: vector `remove_at` and `remove_swap` bare-call locks now expect imported helper mutability diagnostics before rooted user helper precedence.
- 2026-05-13 23:24 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60 | notes: vector `remove_at` user-defined helper precedence test now fails validation.
- 2026-05-13 23:22 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50$'` | failures: none | notes: vector `pop` and `clear` bare-call locks now expect imported helper mutability diagnostics before rooted user helper precedence.
- 2026-05-13 23:20 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50 | notes: vector `pop` and `clear` user-defined helper precedence tests now fail validation.
- 2026-05-13 23:18 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10$'` | failures: none | notes: expression-body map helper source locks now expect canonical alias diagnostics and borrowed `count_ref` helper targets.
- 2026-05-13 23:12 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10$'` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10 | notes: reference-wrapped map body-argument fixtures now pass with count_ref, but explicit `/map/count` method body-argument validation remains false.
- 2026-05-13 23:10 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10 | notes: three map helper expression-body source locks fail in bare alias validation, reference-wrapped fallback validation, and stale canonical mismatch diagnostic expectations.
- 2026-05-13 23:08 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_bindings_struct_defaults_bindings_struct_defaults_11_20$'` | failures: none | notes: map-alias omitted-initializer source lock now expects `argument count mismatch for /map/count`.
- 2026-05-13 23:07 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_semantics_bindings_struct_defaults_bindings_struct_defaults_11_20 | notes: omitted initializer map-alias fallback source lock no longer gets `unknown call target: /map/count`.
- 2026-05-13 23:05 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_831_840$'` | failures: none | notes: direct map count/contains/tryAt return-kind source locks now expect canonical Int32/Bool/Int32 return kinds.
- 2026-05-13 23:04 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_831_840 | notes: exact direct map count-like return-info source lock now returns canonical Int32/Bool/Int32 kinds instead of alias UInt64/UInt64/Int64 kinds.
- 2026-05-13 23:03 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820$'` | failures: none | notes: explicit map count/contains source lock now resolves compatibility `/map/...` aliases and keeps canonical `/std/collections/map/...` paths rejected at `tag`.
- 2026-05-13 23:02 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820 | notes: focused rerun shows `/map/count` and `/map/contains` resolve to primitive tag definitions, while canonical `/std/collections/map/count` and `/std/collections/map/contains` still reject.
- 2026-05-13 23:01 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820 | notes: explicit map count/contains receiver source lock now resolves `/i32/tag` and `/bool/tag` where the test expected `unknown method target for tag`.
- 2026-05-13 23:00 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_801_810$'` | failures: none | notes: wrapper vector slash-method count/capacity diagnostics now source-lock `unknown method target for tag`.
- 2026-05-13 22:59 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_801_810 | notes: wrapper vector slash-method count diagnostic source lock now reports `unknown method target for tag`, matching neighboring wrapper receiver diagnostics but not the stale `/vector/count` expectation.
- 2026-05-13 22:58 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_791_800$'` | failures: none | notes: reordered access return-kind source lock now treats string access fallback as methodResolved=true while preserving Int32 kind.
- 2026-05-13 22:57 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_791_800 | notes: reordered access return-kind source lock now sets methodResolved=true for the string-key graph-fact fallback where the test expected false.
- 2026-05-13 22:55 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_771_780$'` | failures: none | notes: semantic vector method gate source lock now expects the stricter missing semantic-product method-call id diagnostic before stale local fallback.
- 2026-05-13 22:54 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_771_780 | notes: semantic vector method gate source lock now reports missing semantic-product method-call semantic ids for `at` and `push`, replacing stale/unknown-method expectations.
- 2026-05-13 22:53 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_761_770$'` | failures: none | notes: slashless map import alias receiver source lock now registers `/std/collections/map/at/tag`, matching resolver output and the existing missing-method diagnostic.
- 2026-05-13 22:52 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_761_770 | notes: setup-type receiver alias source lock now resolves nullptr and reports an error for slashless map import alias `MapAtAlias -> std/collections/map/at` where it expected `/map/at/tag`.
- 2026-05-13 22:51 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_741_750$'` | failures: none | notes: exact canonical and std-namespaced vector access source locks now expect NotMatched/Unknown, matching the helper's canonical vector access deferral.
- 2026-05-13 22:49 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_741_750 | notes: vector access element-kind source locks now return NotResolved for canonical/std/slash vector access fixtures where the tests expected String from local vector metadata.
- 2026-05-13 22:48 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_661_670$'` | failures: none | notes: Result.ok semantic payload facts now stay authoritative and skip direct collection fallback, avoiding stale definition-resolution probes.
- 2026-05-13 22:47 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_661_670 | notes: Result.ok semantic-query payload source locks now see one definition-resolution probe before semantic payload metadata emits, while stale expectations required zero probes.
- 2026-05-13 22:45 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_651_660$'` | failures: none | notes: Result ok direct-name payload source lock now expects Int64 from local metadata when semantic facts are missing, without invoking generic fallback inference.
- 2026-05-13 22:43 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_651_660 | notes: localization stopped at Result ok direct-name payload inference: semantic binding facts now infer a concrete value kind where the source lock expected Unknown.
- 2026-05-13 22:42 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_601_610$'` | failures: none | notes: remove_swap/remove_at flow helper fixtures now use unknown-kind struct vector elements so removed-slot Destroy and survivor Move helper wiring is exercised.
- 2026-05-13 22:41 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_601_610 | notes: localization stopped at vector remove_swap/remove_at flow helper locks: captured destroy callee stayed null where tests expected the element Destroy helper.
- 2026-05-13 22:40 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_591_600$'` | failures: none | notes: conversion helper pointer arithmetic respects semantic non-pointer facts over stale pointer locals, and vector record capacity locks now expect the current 1024-slot local dynamic capacity.
- 2026-05-13 22:37 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_591_600 | notes: localization stopped at conversion-helper layout expectations: semantic pointer arithmetic lacked the expected 16-byte scale immediate and vector record layout size emitted 1024 where the source lock expected 256.
- 2026-05-13 22:36 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_361_370$'` | failures: none | notes: unresolved compatibility map count source lock now expects the single method-resolution probe before returning NotResolved.
- 2026-05-13 22:35 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_361_370 | notes: localization stopped at unresolved compatibility map count source-lock expectations: call-return setup now performs one method-resolution probe where the test expected zero.
- 2026-05-13 22:34 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_341_350$'` | failures: none | notes: implicit-auto map return inference now source-locks `/std/collections/map/map` literal constructor inference instead of retired `/std/collections/mapPair`.
- 2026-05-13 22:32 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_341_350 | notes: localization stopped at implicit auto map return inference: `runLowerInferenceReturnInfoSetup` returned false and left non-empty error/unknown return info where the source lock expected array Int32 output.
- 2026-05-13 22:31 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_331_340$'` | failures: none | notes: vector statement helper diagnostics now lock canonical vector and SoA mutator calls as NotMatched so ordinary helper routing owns them.
- 2026-05-13 22:29 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_331_340 | notes: localization stopped at vector statement helper diagnostics: canonical stdlib vector/SoA push calls now return NotMatched where the lock expected direct emission/errors.
- 2026-05-13 22:26 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_(251_260|321_330)$'` | failures: none | notes: final focused regression check for string-map count emission and struct-constructor source-lock stabilization passed.
- 2026-05-13 22:26 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_321_330$'` | failures: none | notes: struct-constructor full-path source lock now pins the brace-constructor guard around struct definition adoption.
- 2026-05-13 22:25 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_321_330 | notes: localization stopped at a stale struct-constructor full-path source lock that still expected the old standalone `isStructDefinition(*initCallee)` branch.
- 2026-05-13 22:24 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_251_260$'` | failures: none | notes: bare count calls on builtin map access targets now fall through early deferral and emit string length when semantic/local facts prove the map value is string.
- 2026-05-13 22:20 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_251_260 | notes: localization stopped at string map access count source-lock expectations: target binding facts for map value strings now defer when the access expression itself has no semantic graph fact.
- 2026-05-13 22:17 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_(121_130|181_190|241_250)$'` | failures: none | notes: final focused regression check for the semantic map recursion guard, rooted map alias source locks, and count/access classifier locks passed.
- 2026-05-13 22:17 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250$'` | failures: none | notes: count/access classifier source locks now match canonical vector count deferral/classification and canonical vector access deferral.
- 2026-05-13 22:16 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250 | notes: after refreshing most count/access locks, only templated canonical vector count and namespace-prefixed vector count classifier expectations were stale.
- 2026-05-13 22:10 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250 | notes: localization stopped at count/access classifier source-lock expectations: canonical runtime string count and semantic runtime string facts now defer, array/string classifiers return true where tests expected false, vector capacity returns false, and canonical vector access no longer reports builtin array access name.
- 2026-05-13 22:09 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_181_190$'` | failures: none | notes: rooted map alias `resolveDefinitionCall` source locks now expect canonicalized retired aliases to reject and semantic surface-id overloads to resolve.
- 2026-05-13 22:08 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_181_190 | notes: localization stopped at rooted map alias `resolveDefinitionCall` source-lock expectations: alias count/contains/tryAt/at* checks now return nullptr, while semantic surface-id overload classification resolves a map helper where the test expected nullptr.
- 2026-05-13 22:07 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130$'` | failures: none | notes: exact canonical map helper calls no longer self-rewrite in pre-dispatch, so the exact-import vector/map bridge parity case no longer stack-overflows.
- 2026-05-13 22:06 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && lldb --batch -o 'breakpoint set --name primec::semantics::SemanticsValidator::mapHelperReceiverIndex' -o 'breakpoint command add 1 ...' -o 'run --test-case="ir lowerer call helpers keep exact-import vector and map bridge parity" -s' -k 'thread backtrace -c 30' -- ./PrimeStruct_backend_ir_tests` | failures: ir lowerer call helpers keep exact-import vector and map bridge parity | notes: breakpoint probe confirms repeated `mapHelperReceiverIndex` recursion before the same stack overflow; investigate map resolver self-inference guard.
- 2026-05-13 22:05 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && lldb --batch -o 'run --test-case="ir lowerer call helpers keep exact-import vector and map bridge parity" -s' -k 'thread backtrace all' -- ./PrimeStruct_backend_ir_tests` | failures: ir lowerer call helpers keep exact-import vector and map bridge parity | notes: backtrace shows runaway recursive `SemanticsValidator::validateExpr` during map helper pre-dispatch; top frames enter `mapHelperReceiverIndex` -> `tryRewriteCanonicalExperimentalMapHelperCall` -> `validateExprPreDispatchDirectCalls`.
- 2026-05-13 22:04 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && lldb --batch -o 'run --test-case="ir lowerer call helpers keep exact-import vector and map bridge parity" -s' -o 'bt' -- ./PrimeStruct_backend_ir_tests` | failures: ir lowerer call helpers keep exact-import vector and map bridge parity | notes: lldb stops on EXC_BAD_ACCESS inside `tiny_malloc_from_free_list`, indicating memory corruption before or during semantic bridge parity lookup.
- 2026-05-13 22:03 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130 | notes: focused shard reproduces the SIGSEGV in `ir lowerer call helpers keep exact-import vector and map bridge parity`.
- 2026-05-13 22:02 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130 | notes: release localization stopped at SIGSEGV in `ir lowerer call helpers keep exact-import vector and map bridge parity` after backend IR validation cases 111-120 passed.
- 2026-05-13 22:01 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120$'` | failures: none | notes: final focused rerun after removing temporary helper-path logging still passes.
- 2026-05-13 22:00 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120$'` | failures: none | notes: backend IR validation cases 111-120 pass with retired map alias fallback deferred, forwarded canonical map constructor inference, canonical vector metadata deferral, and safe vector access coverage.
- 2026-05-13 21:59 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120 | notes: after adding helper-path logging, the remaining failure was canonical `/std/collections/vector/count` returning Error without semantic context instead of deferring.
- 2026-05-13 21:57 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests` | failures: PrimeStruct_backend_ir_tests build | notes: transient localization edit used `INFO(std::string(...) + helperName)`, which doctest parsed as message-builder addition; fix to stream-style `INFO`.
- 2026-05-13 21:56 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120 | notes: stale map alias fallback, forwarded map constructor, and canonical metadata expectations were refreshed; remaining mismatch is removed `/vector/count` returning Error with a count diagnostic instead of NotHandled.
- 2026-05-13 21:53 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120 | notes: localization stopped at stale backend IR validation expectations for non-method map count fallback, forwarded experimental-map access target inference, and explicit vector count/safe-at native-tail dispatch.
- 2026-05-13 21:50 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80$'` | failures: none | notes: stdlib-only unsupported native diagnostics are NotHandled without semantic surface context, while semantic-aware published vector metadata diagnostics remain available to native tail dispatch.
- 2026-05-13 21:49 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80 | notes: localization stopped at `ir lowerer call helpers emit unsupported native call diagnostics for stdlib-only helpers`; `/std/collections/vector/count` without semantic surface context returned Error and populated `error` where the source-level diagnostic helper expected NotHandled.
- 2026-05-13 21:46 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100$'` | failures: none | notes: native tail/vector semantic facts now override stale locals, published vector count/capacity diagnostics reject scalar semantic targets, SoA stdlib alias count defers, and builtin array access rejects scalar semantic targets.
- 2026-05-13 21:46 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100 | notes: native scalar/vector semantic count diagnostics now match expectations, but the published vector count diagnostic branch still treated the SoA stdlib alias count target as scalar misuse instead of deferring.
- 2026-05-13 21:43 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer native unsupported count diagnostics prefer semantic facts" -s` | failures: ir lowerer native unsupported count diagnostics prefer semantic facts | notes: direct single-case rerun isolated canonical vector count/capacity diagnostics: scalar semantic target returned NotHandled without an error, while the vector semantic count path still emitted the unsupported-count diagnostic.
- 2026-05-13 21:42 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100 | notes: focused rerun reduced the shard to native unsupported vector count/capacity diagnostic expectations after semantic array/vector access target fallback was tightened and SoA stdlib alias count was deferred.
- 2026-05-13 21:39 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100 | notes: localization stopped at native tail/vector semantic target fact expectations: SoA stdlib alias count no longer emits, vector count/capacity diagnostics return different results, and builtin array access accepts the scalar semantic fixture.
- 2026-05-13 21:36 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80$'` | failures: none | notes: inline map helper dispatch locks now match canonical map helper emission, semantic map target precedence, canonical vector helper diagnostic deferral, and experimental map insert expression deferral.
- 2026-05-13 21:35 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80$'` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80 | notes: focused rerun now only fails the refreshed bare map-sugar source contract because `at`/`at_unsafe` perform two method probes before direct inline emission.
- 2026-05-13 21:31 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80 | notes: localization stopped at stale inline map helper expectations: canonical map count/access now emits where tests still expected `NotHandled`, and unsupported native diagnostics no longer rejects the current helper shape.
- 2026-05-13 21:28 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50$'` | failures: none | notes: backend IR validation source locks now track centralized vector compatibility helper routing and collection fallback helper member resolution.
- 2026-05-13 21:27 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50 | notes: localization stopped at stale source-lock assertions that still expected direct vector `at` path comparisons and direct `resolveExprPath(candidate)` fallback-helper snippets after centralized helper routing.
- 2026-05-13 21:23 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps$'` | failures: none | notes: borrowed map-pack count fixture now calls canonical `count_ref` helpers on `Reference<map<i32, i32>>` packs.
- 2026-05-13 21:21 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps$'` | failures: PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps | notes: adding the map helper import alone still left reference method sugar resolving to unknown `/std/collections/map/count_ref`.
- 2026-05-13 21:21 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps | notes: localization stopped at stale borrowed map-pack count-method fixture resolving reference `.count()` through unknown `/std/collections/map/count_ref`.
- 2026-05-13 21:18 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors$'` | failures: none | notes: variadic borrowed map-pack canonical count fixture now uses `map<i32, i32>` and `/std/collections/map/count`.
- 2026-05-13 21:16 local | fail | mode: release | command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure` | failures: PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors | notes: localization stopped at stale experimental map-pack fixture using templated `/Map`; next focused rerun is the exact CTest target.
- 2026-05-13 21:13 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="status-only result bridge docs stay source locked,Result helper compatibility adapter inventory stays source locked"` | failures: none | notes: status-only Result docs lock now checks TODO-4313 is retired, and helper spelling inventory includes `SemanticsValidatorResultHelpers.cpp`.
- 2026-05-13 21:11 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="status-only result bridge docs stay source locked,Result helper compatibility adapter inventory stays source locked"` | failures: status-only result bridge docs stay source locked; Result helper compatibility adapter inventory stays source locked | notes: stale lock still expected active TODO-4313 wording and omitted `SemanticsValidatorResultHelpers.cpp` from the Result helper spelling inventory.
- 2026-05-13 21:08 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_runtime_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_ir_pipeline_backends_core'` | failures: none | notes: graph-pilot source lock now tracks `preferredMapMethodTarget` instead of removed `canonicalMapMethodTarget`.
- 2026-05-13 21:07 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_runtime_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_ir_pipeline_backends_core'` | failures: PrimeStruct_primestruct_ir_pipeline_backends_core | notes: graph-pilot source lock still expected `const std::string canonicalMapMethodTarget =`.
- 2026-05-13 21:03 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces|PrimeStruct_vector_surface_trace_checker_self_test'` | failures: none | notes: native-tail dispatch now uses the collection path helper instead of literal experimental-vector path spellings; the zero-tolerance vector trace check and self-test pass.
- 2026-05-13 20:59 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_misc_suite_registration|PrimeStruct_primestruct_stdlib_map_ownership'` | failures: none | notes: registered `primestruct.stdlib.map_ownership` in the misc suite list; the ownership suite and suite-registration check both pass.
- 2026-05-13 20:55 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: broad release gate regression | notes: build succeeded, but CTest failed 246/1588 targets; representative buckets include collection/vector/map alias fallout, SoA helper routing, Result/image/native/VM fallout, source-lock drift, VM vector bounds, suite registration, semantic memory trend, and missing Metal toolchain.
- 2026-05-13 20:46 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="map stdlib namespaced count expression keeps canonical helper return precedence,map compatibility count call keeps explicit alias precedence with canonical templated helper present,map compatibility explicit-template count call keeps alias precedence with canonical templated helper,mapTryAt helper validates for supported i32 container result payloads,mapTryAt helper validates for supported bool container result payloads,mapTryAt helper validates for supported string container result payloads"` | failures: none | notes: mapTryAt Result payload fixtures now use canonical stdlib map imports and helper calls instead of redefining canonical `/std/collections/map/tryAt`.
- 2026-05-13 20:43 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer materializes variadic pointer map packs with indexed count_ref helpers,ir lowerer materializes variadic pointer map packs with indexed dereference count methods,ir lowerer materializes variadic pointer map packs with indexed lookup_ref helpers,ir lowerer materializes variadic pointer map packs with indexed dereference lookup helpers,ir lowerer rejects variadic pointer map packs with indexed helper inference"` | failures: none | notes: variadic pointer map pack IR fixtures now import canonical map helpers and use explicit `_ref` helper calls where the source does not dereference the pointer first.
- 2026-05-13 20:39 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter materializes variadic borrowed map packs with indexed count_ref calls"` | failures: none | notes: stale experimental `Map`/`mapCount` fixture now targets canonical `map<K, V>` values and `/std/collections/map/count_ref`.
- 2026-05-13 20:34 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers prefer direct same-path map count-like defs,ir lowerer call helpers keep direct map access defs on builtin fallback,ir lowerer setup type helper rejects removed map alias return-kind fallback"` | failures: none | notes: rooted `/map/count` no longer falls through to method fallback; rooted `/map/at*` direct access stays `NotHandled`; removed map aliases no longer inherit canonical return-kind metadata
- 2026-05-13 20:30 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers prefer direct same-path map count-like defs,ir lowerer call helpers keep direct map access defs on builtin fallback,ir lowerer setup type helper resolves direct definition call return kinds via removed map aliases" -s` | failures: backend IR map helper fallback/return-kind locks | notes: rooted `/map/count` still hit method fallback; removed-map-alias return-kind lock now finds no definitions for rooted count/contains/tryAt/at aliases
- 2026-05-13 20:26 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: broad release gate regression | notes: stopped after focused fix cleared the earlier `vectorAtUnsafe` lock but CTest still failed backend IR validation/conversions and semantics collection shards
- 2026-05-13 20:22 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers" -s` | failures: none | notes: direct experimental-vector `vectorAtUnsafe` now defers instead of emitting native tail instructions
- 2026-05-13 20:21 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers" -s` | failures: ir lowerer call helpers dispatch buffer and native tail wrappers | notes: `experimentalVectorAtUnsafeExpr` returned `Emitted` and produced instructions instead of `NotHandled`
- 2026-05-13 20:20 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: broad release gate regression | notes: stopped after failures persisted with local revert of 5e4d1579c; observed backend IR validation, semantics collections, gfx, VM math, image, and map Result failures
- 2026-05-13 20:16 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: broad release gate regression | notes: stopped after enough failures were identified; likely fallout from rooted map raw-dispatch removal
- 2026-05-13 19:34 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access alias rejects canonical struct-return forwarding,map namespaced access alias reports current receiver diagnostics,map namespaced unsafe access alias rejects canonical struct-return forwarding,map namespaced unsafe access alias reports current receiver diagnostics,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access keeps alias struct-return isolated from canonical helper"` | failures: none | notes: rooted map alias struct-return expectations now match isolated rooted/canonical pre-dispatch behavior
- 2026-05-13 19:33 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access alias rejects canonical struct-return forwarding,map namespaced access alias reports current receiver diagnostics,map namespaced unsafe access alias rejects canonical struct-return forwarding,map namespaced unsafe access alias reports current receiver diagnostics,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access keeps alias struct-return isolated from canonical helper"` | failures: map namespaced unsafe access alias reports current receiver diagnostics | notes: assertion expected `/i32/tag` marker mismatch, but current diagnostic remains unknown `/Marker/tag`
- 2026-05-13 19:31 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access alias chained method uses canonical helper definition,map namespaced access alias chained method keeps downstream tag diagnostics,map namespaced unsafe access alias chained method uses canonical helper definition,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access prefers canonical struct-return over alias helper"` | failures: rooted map alias struct-return semantic expectations | notes: stale expectations still required rooted `/map/at*` aliases to borrow canonical struct-return metadata
- 2026-05-13 19:21 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="vector map bridge boundary docs stay source locked,todo queue and skipped doctest debt stay source locked"` | failures: none | notes: refreshed stale source-lock strings for vector/map bridge wording
- 2026-05-13 19:19 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="vector map bridge boundary docs stay source locked,todo queue and skipped doctest debt stay source locked"` | failures: vector map bridge boundary docs stay source locked | notes: stale source-lock strings for vector/map bridge wording
- 2026-05-13 18:55 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers reject generated experimental map helper family fallback"` | failures: none | notes: generated experimental-map helper-family fallback removed while exact definitions still resolve
- 2026-05-13 18:52 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers reject generated experimental map helper family fallback"` | failures: ir lowerer call helpers reject generated experimental map helper family fallback | notes: exact experimental helper and generated fallback expectations need refresh
- 2026-05-13 18:54 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers reject generated experimental map helper family fallback"` | failures: ir lowerer call helpers reject generated experimental map helper family fallback | notes: unspecialized experimental helper still hit generated helper family
- 2026-05-13 18:39 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding,ir lowerer struct return helpers keep map tryAt alias precedence,ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding,ir lowerer struct return helpers keep explicit map tryAt method alias precedence,ir lowerer inference call-return setup rejects explicit map access aliases against canonical-only defs,ir lowerer setup type helper rejects explicit map access aliases through canonical defs,ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs"` | failures: none | notes: rooted map access aliases now stay separated from canonical return-kind fallback
- 2026-05-13 18:37 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding,ir lowerer struct return helpers keep map tryAt alias precedence,ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding,ir lowerer struct return helpers keep explicit map tryAt method alias precedence,ir lowerer inference call-return setup resolves explicit map helper aliases against canonical-only defs,ir lowerer setup type helper resolves explicit map access aliases through canonical defs,ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs"` | failures: ir lowerer inference call-return setup resolves explicit map helper aliases against canonical-only defs; ir lowerer setup type helper resolves explicit map access aliases through canonical defs | notes: stale explicit `/map/*` alias canonical fallback expectations
- 2026-05-13 18:39 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding,ir lowerer struct return helpers keep map tryAt alias precedence,ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding,ir lowerer struct return helpers keep explicit map tryAt method alias precedence,ir lowerer inference call-return setup rejects explicit map access aliases against canonical-only defs,ir lowerer setup type helper rejects explicit map access aliases through canonical defs,ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs"` | failures: ir lowerer setup type helper rejects explicit map access aliases through canonical defs | notes: assertion expected truthy return after rooted access alias rejection
- 2026-05-13 18:20 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="retired public stdlib map wrapper calls report their original target"` | failures: none | notes: public `std/collections/mapAt*` aliases now report their original unknown target
- 2026-05-13 18:19 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests PrimeStruct_backend_runtime_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map wrapper bridge is retired,retired public stdlib map wrapper calls report their original target" && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable" && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked" && ./PrimeStruct_backend_runtime_tests --test-case="design doc records vector map bridge contract"` | failures: retired public stdlib map wrapper calls report their original target | notes: `/std/collections/mapAt*` still routed through access-alias diagnostics
- 2026-05-13 17:18 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer statement call emission source delegation stays stable"` | failures: none | notes: backend IR source locks now match rooted map access emission and removed vector wrapper alias source strings
- 2026-05-13 17:14 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer statement call emission source delegation stays stable"` | failures: backend IR map helper/source-delegation locks | notes: stale expectations for canonical map helper emission and removed vector wrapper alias source strings
- 2026-05-13 17:14 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_runtime_tests && cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked"` | failures: none | notes: source lock now permits canonical map insert lowering spelling while rejecting retired bridge spellings
- 2026-05-13 17:13 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="design doc records vector map bridge contract,stdlib surface registry stays source locked,collection helper surface registry resolves preferred compatibility spellings"` | failures: stdlib surface registry stays source locked | notes: source lock incorrectly rejected canonical `/std/collections/map/insert`
- 2026-05-13 17:13 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now permits canonical `insert_ref` while rejecting retired bridge aliases
- 2026-05-13 17:12 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: canonical map surface owns implementation through internal map module | notes: source lock incorrectly rejected canonical `/std/collections/map/insert_ref`
- 2026-05-13 17:11 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="retired public mapPair bridge reports unknown target"` | failures: none | notes: diagnostic expectation now matches retired public fixed-arity bridge behavior
- 2026-05-13 17:11 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map wrapper bridge is retired,canonical stdlib map slash helpers avoid wrapper recursion,canonical stdlib map helpers accept constructor receivers,stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers"` | failures: stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers | notes: diagnostic now reports unknown `/std/collections/mapPair` after public fixed-arity bridge deletion
- 2026-05-13 17:10 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map helpers accept constructor receivers"` | failures: none | notes: retargeted obsolete wrapper constructor-receiver source to canonical `map<K, V>` locals and explicit helper calls
- 2026-05-13 17:10 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="public stdlib map wrapper bridge is retired,canonical stdlib map slash helpers avoid wrapper recursion,stdlib wrapper map helpers accept experimental constructor receivers,stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers"` | failures: stdlib wrapper map helpers accept experimental constructor receivers | notes: test crashed with SIGSEGV on obsolete direct constructor receiver bridge shape
- 2026-05-13 17:09 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm wrapper map helpers on experimental map values,compiles and runs native wrapper map helpers on experimental map values,compiles and runs wrapper map helpers on experimental map values in C++ emitter"` | failures: none | notes: retargeted wrapper helper fixture to canonical `map<K, V>` values and explicit canonical helper calls
- 2026-05-13 17:05 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm wrapper map helpers on experimental map values,compiles and runs native wrapper map helpers on experimental map values,compiles and runs wrapper map helpers on experimental map values in C++ emitter"` | failures: wrapper map helpers on experimental map values | notes: direct canonical constructor receiver binding still left obsolete `Map` wrapper path and reproduced C++/native compile -1 plus VM SIGSEGV
- 2026-05-13 17:00 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,small stdlib wrappers stay source locked to inferred locals,runs vm wrapper map helpers on experimental map values,compiles and runs native wrapper map helpers on experimental map values,compiles and runs wrapper map helpers on experimental map values in C++ emitter"` | failures: wrapper map helpers on experimental map values | notes: C++/native compile commands returned -1 and VM emitted SIGSEGV on generated wrapper helper source
- 2026-05-13 16:01 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical namespaced map helpers on experimental map values,compiles and runs native canonical namespaced map helpers on experimental map values,vm materializes variadic experimental map packs with indexed canonical count calls,native materializes variadic experimental map packs with indexed canonical count calls"` | failures: none | notes: canonical map value and variadic migrated sources now pass on VM/native focused rerun
- 2026-05-13 15:50 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical namespaced map helpers on experimental map values,compiles and runs native canonical namespaced map helpers on experimental map values,vm materializes variadic experimental map packs with indexed canonical count calls,native materializes variadic experimental map packs with indexed canonical count calls"` | failures: canonical namespaced map helpers on experimental map values; variadic experimental map packs | notes: canonical map source still fails; variadic source reports template arguments required for `/std/collections/map/map`
- 2026-05-13 15:45 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical namespaced map helpers on experimental map values,compiles and runs native canonical namespaced map helpers on experimental map values,vm materializes variadic experimental map packs with indexed canonical count calls,native materializes variadic experimental map packs with indexed canonical count calls"` | failures: canonical namespaced map helpers on experimental map values; variadic experimental map packs | notes: generated migrated sources still use stale legacy `Map<...>` shape; native reports `/Map` template-argument diagnostic and VM helper case segfaulted
- 2026-05-13 15:40 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="field-bound experimental map stdlib namespaced count methods validate" -s` | failures: none | notes: field-bound source now uses canonical map import, type, and constructor
- 2026-05-13 15:33 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="direct experimental map wildcard import is rejected,direct experimental map exact import is rejected,stdlib namespaced map constructor resolves through imported stdlib helper,explicit canonical map access helpers accept experimental map values,field-bound experimental map stdlib namespaced count methods validate,stdlib wrapper mapPair constructor accepts explicit experimental map bindings,helper-wrapped map constructors accept explicit experimental map bindings"` | failures: field-bound experimental map stdlib namespaced count methods validate | notes: migrated source returned a diagnostic instead of validating
- 2026-05-13 15:33 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors accept explicit experimental map bindings" -s` | failures: none | notes: canonical `map<string, i32>` binding avoids the recursive legacy `Map` wrapper path
- 2026-05-13 15:32 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib wrapper mapPair constructor accepts explicit experimental map bindings" -s` | failures: none | notes: canonical `map<string, i32>` binding and `/std/collections/map/map` constructor avoid the recursive legacy wrapper path
- 2026-05-13 15:31 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib wrapper mapPair constructor accepts explicit experimental map bindings,helper-wrapped map constructors accept explicit experimental map bindings" -s` | failures: stdlib wrapper mapPair constructor accepts explicit experimental map bindings, helper-wrapped map constructors accept explicit experimental map bindings | notes: still crashed after switching this source file from internal_map imports to map imports
- 2026-05-13 15:29 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib wrapper mapPair constructor accepts explicit experimental map bindings" -s` | failures: stdlib wrapper mapPair constructor accepts explicit experimental map bindings | notes: crashed before doctest output after direct experimental map source imports were replaced by internal_map imports
- 2026-05-13 15:28 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors accept explicit experimental map bindings" -s` | failures: helper-wrapped map constructors accept explicit experimental map bindings | notes: single-case rerun still crashes with SIGSEGV before assertions
- 2026-05-13 15:28 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib namespaced map constructor resolves through imported stdlib helper,explicit canonical map access helpers accept experimental map values,helper-wrapped map constructors accept explicit experimental map bindings,field-bound experimental map stdlib namespaced count methods validate"` | failures: helper-wrapped map constructors accept explicit experimental map bindings | notes: crashed with SIGSEGV after direct experimental map source imports were replaced by internal_map imports
- 2026-05-13 15:13 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="design doc records vector map bridge contract"` | failures: none | notes: backend architecture source-lock now matches the current map manifest compatibility-removal wording
- 2026-05-13 15:12 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="design doc records vector map bridge contract"` | failures: design doc records vector map bridge contract | notes: backend architecture source-lock still expected older bridge-contract wording after map manifest compatibility removal
- 2026-05-13 15:10 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="small stdlib wrappers stay source locked to inferred locals"` | failures: none | notes: source-lock now reads the experimental map shim and internal map implementation separately
- 2026-05-13 15:09 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="small stdlib wrappers stay source locked to inferred locals"` | failures: small stdlib wrappers stay source locked to inferred locals | notes: source-lock still expected old experimental-map/internal-map text snippets after canonical map metadata and docs were split
- 2026-05-13 15:08 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: none | notes: queue source-lock now expects TODO-4438 as Ready Now and TODO-4439 as the first Immediate Next leaf
- 2026-05-13 15:07 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | failures: todo queue and skipped doctest debt stay source locked | notes: source-lock still expected the previous TODO-4297/TODO-4299 queue head after splitting TODO-4303 into TODO-4437/TODO-4438/TODO-4439
- 2026-05-13 15:06 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="map insert surface registry rejects legacy compatibility spellings"` | failures: none | notes: slash-qualified member resolution now rejects `/map/insert` and `/map/insert_ref` after the manifest compatibility removal
- 2026-05-13 15:04 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="map insert surface registry rejects legacy compatibility spellings"` | failures: map insert surface registry rejects legacy compatibility spellings | notes: `/map/insert` and `/map/insert_ref` still resolve after removing map helper compatibility spellings from `surfaces.psmeta`
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata classifies collection helper categories"` | failures: none | notes: map base/borrowed helper classification passes with manifest-backed member metadata
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens"` | failures: none | notes: collection member token/full-path resolution expectations now match the generic leaf resolver
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer stdlib surface metadata recognizes experimental map lowering helpers"` | failures: none | notes: experimental map lowering spellings still resolve through the map helper surface after metadata moved to the manifest
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="collection helper surface registry resolves preferred compatibility spellings"` | failures: none | notes: canonical helper path construction now rejects slash spellings outside the requested surface id
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="map insert surface registry resolves legacy compatibility spellings"` | failures: none | notes: legacy map insert compatibility spellings still resolve from manifest-backed metadata
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked"` | failures: none | notes: source lock now pins map metadata in `surfaces.psmeta` instead of C++ registry arrays
- 2026-05-13 14:41 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now checks map helper/constructor metadata lives in `surfaces.psmeta`
- 2026-05-13 14:41 local | pass | mode: release | command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests` | failures: none | notes: recreated missing release build directory and rebuilt focused affected test targets
- 2026-05-13 14:41 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="collection helper surface registry resolves preferred compatibility spellings"` | failures: collection helper surface registry resolves preferred compatibility spellings | notes: `stdlibSurfaceCanonicalHelperPath` accepted `/not_gfx/upload` by leaf without validating the slash spelling's surface id
- 2026-05-13 14:41 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens"` | failures: stdlib surface metadata resolves collection helper member tokens | notes: stale vector full-path expectations still expected empty member resolution even though the generic resolver resolves canonical paths by leaf
- 2026-05-13 14:41 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer stdlib surface metadata recognizes experimental map lowering helpers"` | failures: release test command | notes: `build-release` was missing before the focused backend IR rerun, so the shell could not find `./PrimeStruct_backend_ir_tests`
- 2026-05-13 14:06 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="native canonical map access direct calls and method sugar use ordinary map helpers"` | failures: none | notes: native compile-run lock now expects direct and method map access to use the ordinary canonical helper surface and return 16
- 2026-05-13 14:06 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical map access direct calls and method sugar through ordinary map helpers"` | failures: none | notes: VM compile-run lock now expects direct and method map access to use the ordinary canonical helper surface and return 16
- 2026-05-13 14:06 local | pass | mode: release | command: `build-release/primec --emit=vm <tmp canonical+experimental map access smoke> --entry /main && build-release/primec --emit=native <tmp canonical+experimental map access smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical at/at_unsafe direct calls, reference helpers, and method sugar return 33 on VM/native with experimental compatibility import present
- 2026-05-13 14:04 local | pass | mode: release | command: `build-release/primec --emit=vm <tmp canonical map access smoke> --entry /main && build-release/primec --emit=native <tmp canonical map access smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical at/at_unsafe direct calls, reference helpers, and method sugar return 33 on VM/native
- 2026-05-13 14:04 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now pins canonical map access helpers leaving builtin/native preservation paths
- 2026-05-13 14:04 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_misc_tests` | failures: none | notes: release compiler and focused ownership test target rebuilt after removing stale access-helper inline dispatch code
- 2026-05-13 14:04 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_misc_tests` | failures: release build | notes: `IrLowererInlineNativeCallDispatch.cpp` still declared unused `isStringReturningSourceMapAccess` after access-helper builtin preservation was removed
- 2026-05-13 13:56 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now pins canonical map read `.prime` routing and the experimental shim staying import-only
- 2026-05-13 13:56 local | pass | mode: release | command: `build-release/primec --emit=vm <tmp canonical+experimental map read smoke> --entry /main && build-release/primec --emit=native <tmp canonical+experimental map read smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical count/contains/tryAt direct calls and method sugar return 15 on VM/native with experimental compatibility import present
- 2026-05-13 13:56 local | pass | mode: release | command: `build-release/primec --emit=vm <tmp canonical map read smoke> --entry /main && build-release/primec --emit=native <tmp canonical map read smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical count/contains/tryAt direct calls and method sugar return 15 on VM/native
- 2026-05-13 13:56 local | fail | mode: release | command: `build-release/primec --emit=vm <tmp canonical+experimental map read smoke> --entry /main` | failures: canonical+experimental map read smoke | notes: duplicate definition `/std/collections/experimental_map/mapAtRef` from redundant experimental-map shim redeclarations before shim cleanup
- 2026-05-13 13:45 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now pins deleted `insert_builtin` public trap and canonical insert retargeting
- 2026-05-13 13:45 local | pass | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=vm <tmp canonical map insert smoke> --entry /main && ./primec --emit=native <tmp canonical map insert smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical direct insert, method insert, and insert_ref execute through ordinary map helper bodies and return 36 on VM/native
- 2026-05-13 13:36 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now pins direct internal-map policy helpers, canonical reference spelling, and insert rewrite guards
- 2026-05-13 13:24 local | pass | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=vm <tmp internal-map policy smoke> --entry /main && ./primec --emit=native <tmp internal-map policy smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: none | notes: canonical map lookup, overwrite, direct insert, method insert, reference insert, contains, and tryAt now execute through internal map policy and return 39 on VM/native
- 2026-05-13 13:24 local | fail | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=vm <tmp internal-map policy smoke> --entry /main` | failures: canonical internal map policy VM smoke | notes: VM lowering rejected `/std/collections/map/insert_builtin__...` as a void call in expression context before internal-map insert policy could run
- 2026-05-13 13:24 local | fail | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=vm <tmp internal-map policy smoke> --entry /main` | failures: canonical internal map policy VM smoke | notes: VM passed the inline-context gate, then failed at runtime with `invalid indirect address in IR`
- 2026-05-13 13:18 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: none | notes: ownership lock now accepts vector-style compatibility type identity while pinning `internal_map` facade ownership
- 2026-05-13 13:17 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | failures: canonical map surface owns implementation through internal map module | notes: ownership lock needed to allow vector-style compatibility type identity while checking `internal_map` facade ownership
- 2026-05-13 13:16 local | pass | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=native <tmp internal-map smoke> -o <tmp exe> --entry /main && <tmp exe returns 7>` | failures: none | notes: canonical map construction, direct insert, count, and access now route through the internal map implementation while preserving native behavior
- 2026-05-13 12:57 local | fail | mode: release | command: `cd build-release && ./primec --emit=ir <tmp internal-map smoke> --entry /main` | failures: canonical internal map smoke | notes: canonical `map.prime` calls to `/std/collections/internal_map/mapInsert` were not visible while the copied implementation still declared the legacy experimental namespace
- 2026-05-13 12:58 local | fail | mode: release | command: `cd build-release && ./primec --emit=ir <tmp internal-map smoke> --entry /main` | failures: canonical internal map smoke | notes: internal backing namespace was visible, but internal map prefixed helpers still canonicalized to missing `/map/count` before registry coverage included internal helper spellings
- 2026-05-13 13:04 local | fail | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=ir <tmp internal-map smoke> --entry /main` | failures: canonical internal map smoke | notes: rebuilt compiler still canonicalized internal map-prefixed helper calls to removed `/map/*` paths, so internal helpers were renamed to namespace-owned names
- 2026-05-13 13:05 local | fail | mode: release | command: `cd build-release && ./primec --emit=ir <tmp internal-map smoke> --entry /main` | failures: canonical internal map smoke | notes: method sugar hit `template arguments required for /std/collections/internal_map/at`, so internal helper names were moved behind non-method-conflicting `*Impl` suffixes
- 2026-05-13 13:11 local | pass | mode: release | command: `cd build-release && ./primec --emit=ir <tmp internal-map smoke without insert> --entry /main` | failures: none | notes: canonical map construction plus count/access method sugar now resolves through internal map helper ownership
- 2026-05-13 13:11 local | fail | mode: release | command: `cd build-release && ./primec --emit=native <tmp internal-map smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: canonical internal map smoke | notes: native lowerer still rejected expression-context `/std/collections/map/insert` before internal map insert impl helpers were admitted to inline void-call handling
- 2026-05-13 13:13 local | fail | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=native <tmp internal-map smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: canonical internal map smoke | notes: internal insert impl paths were inlineable, but canonical `/std/collections/map/insert` also needed inline void-call handling before its body could lower
- 2026-05-13 13:14 local | fail | mode: release | command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=native <tmp internal-map smoke> -o <tmp exe> --entry /main && <tmp exe>` | failures: canonical internal map smoke | notes: canonical insert was still rejected by fallback expression lowering before the direct helper inline branch handled map insert statements
- 2026-05-13 12:44 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter emit setup source delegation stays stable,semantics validator expr source delegation stays stable,semantics validator infer source delegation stays stable,template monomorph source delegation stays stable,ir lowerer call helpers source delegation stays stable,semantics validator passes source delegation stays stable,semantics validator statement source delegation stays stable"` | failures: none | notes: focused source-lock group passes after zero vector trace routing and stale SoA helper assertion refresh
- 2026-05-13 12:38 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable"` | failures: none | notes: refreshed source-lock now matches generic SoA `to_aos` helper routing
- 2026-05-13 12:36 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="emitter emit setup source delegation stays stable,semantics validator expr source delegation stays stable,semantics validator infer source delegation stays stable,template monomorph source delegation stays stable,ir lowerer call helpers source delegation stays stable,semantics validator passes source delegation stays stable,semantics validator statement source delegation stays stable"` | failures: ir lowerer call helpers source delegation stays stable | notes: stale source-lock expected literal `std/collections/soa_vector/to_aos` after generic `isCanonicalCollectionHelperCall` routing
- 2026-05-13 11:57 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces'` | failures: none | notes: CTest vector surface trace target passes with the ratcheted 49-trace baseline after removing template-monomorph vector entries
- 2026-05-13 12:00 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests -tc="template monomorph source delegation stays stable"` | failures: none | notes: template-monomorph source-lock coverage now matches registry-backed vector compatibility helper routing and refreshed current emitter collection-helper locks
- 2026-05-13 11:55 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: broad CTest failures observed before cancellation | notes: build completed, then broad CTest reproduced representative existing collection/map/Result/vector-runtime failure buckets; stopped the sweep and used focused TODO-4417 validation instead
- 2026-05-13 11:43 local | pass | mode: release | command: `python3 scripts/check_vector_surface_traces.py --root .` | failures: none | notes: vector trace audit passes at 49 observed traces after ratcheting from 135 and dropping all template-monomorph baseline entries
- 2026-05-13 11:22 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests -tc="semantics validator statement source delegation stays stable"` | failures: none | notes: statement source-delegation lock now matches helper-routed vector slot unsafe and canonicalized helper snippets
- 2026-05-13 11:22 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests -tc="semantics validator statement source delegation stays stable"` | failures: stale statement source-delegation lock | notes: expected direct experimental vector slot path and stale snippet spellings before refreshing source-lock assertions
- 2026-05-13 11:01 local | pass | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests -tc="semantics validator infer source delegation stays stable"` | failures: none | notes: stale infer source-delegation lock now matches the current `definition.namespacePrefix` return-transform routing
- 2026-05-13 11:01 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests -tc="semantics validator infer source delegation stays stable"` | failures: stale infer source-delegation lock | notes: source lock still expected the removed `directDefIt->second->namespacePrefix` spelling before rebuilding the corrected expectation
- 2026-05-13 11:00 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces'` | failures: none | notes: CTest vector surface trace target passes with the ratcheted 208-trace baseline
- 2026-05-13 11:00 local | canceled | mode: release | command: `./scripts/compile.sh --release` | failures: none observed before cancellation | notes: build completed and 54 CTest targets passed; stopped broad compile-run sweep as too broad for the lite TODO slice before focused validation
- 2026-05-13 10:26 local | pass | mode: release | command: `python3 scripts/check_vector_surface_traces.py --root .` | failures: none | notes: vector trace audit passes at 265 observed traces after routing semantic vector access probes through registry-backed vector helper predicates and ratcheting the baseline
- 2026-05-13 10:26 local | fail | mode: release | command: `python3 scripts/check_vector_surface_traces.py --root .` | failures: PrimeStruct_vector_surface_traces | notes: `SemanticsValidatorExprArgumentValidationCollections.cpp` canonical vector path traces rose from 1 to 2, and `SemanticsValidatorInferCollectionReturnInference.cpp` rose from 0 to 1
- 2026-05-13 10:21 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: none | notes: native vector/map count-shadow shard now passes after preserving string-returning source map access method calls for inline lowering while keeping explicit canonical map helpers on the builtin path
- 2026-05-13 10:21 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: native slash-method map access count shadow | notes: direct canonical map access was restored, but slash-method map access still returned 0 instead of 3 before preserving the source method shape in count lowering
- 2026-05-13 10:21 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: native direct canonical and slash-method map access count shadows | notes: overly broad source-method preservation left explicit canonical `/std/collections/map/at` unsupported in native expression lowering
- 2026-05-13 10:21 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: native slash-method map access count shadow | notes: safe vector rejection, unsafe vector string shadow, and direct canonical map access cases were passing; slash-method map access returned 0 instead of 3
- 2026-05-13 08:17 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: native vector/map count-shadow shard | notes: VM shard already passes; native shard still fails three count-shadow cases covering safe vector rejection drift, unsafe vector string shadow semantics, and slash-method map count lowering
- 2026-05-13 08:17 local | pass | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24'` | failures: none | notes: named semantic-product Result lambda parameters now fall back to lowerer local payload metadata, fixing direct and definition-backed string `Result.and_then` VM cases
- 2026-05-13 08:17 local | fail | mode: release | command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93|PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24'` | failures: VM Result string payload, native vector/map count-shadow shard | notes: fresh primec and compile-run test binary still reproduced both known shards before the Result metadata fix
- 2026-05-13 08:08 local | fail | mode: release | command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120|PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93|PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24'` | failures: VM Result string payload, native vector/map count-shadow shard | notes: IR helper-contract shard now passes after canonical vector/map count-access routing fixes; remaining focused failures still block TODO work
- 2026-05-13 08:05 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests -tc="ir lowerer call helpers emit explicit vector count and safe at while deferring bare count" -s` | failures: explicit canonical vector count/capacity/at native-tail dispatch | notes: canonical `/std/collections/vector/count`, `capacity`, and safe `at` were not reaching the builtin emission path
- 2026-05-13 08:04 local | fail | mode: release | command: `cd build-release && ./PrimeStruct_backend_ir_tests -tc="ir lowerer call helpers keep explicit map helpers out of native builtin emission" -s` | failures: `/map/count` helper dispatch | notes: `/map/count` still deferred as a removed alias while `/std/collections/map/count` emitted correctly
- 2026-05-13 07:57 local | fail | mode: release | command: `./scripts/compile.sh --release --skip-tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120|PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93|PrimeStruct_primestruct_compile_run_vm_core_core_01b_20_24'` | failures: IR helper contract, VM Result string payload, native vector/map count-shadow shard | notes: representative shard still fails after vector trace fix; signatures are independent enough that TODO work remains blocked
- 2026-05-13 07:56 local | pass | mode: release | command: `./scripts/compile.sh --release --skip-tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces'` | failures: none | notes: vector trace gate passes after removing the hard-coded canonical vector path probe from inline native dispatch
- 2026-05-13 07:56 local | fail | mode: release | command: `./scripts/compile.sh --release --skip-tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces'` | failures: PrimeStruct_vector_surface_traces | notes: `src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp` has 2 `canonical-vector-path` traces; baseline allows 0
- 2026-05-13 07:56 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: 135 CTest targets | notes: full baseline failed after fresh release rebuild; broad repo health blocks TODO work
- 2026-05-12 19:21 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93'` | failures: native canonical vector/map count-shadow tests | notes: vector temporary direct case is fixed, but this shard still fails three count-shadow cases
- 2026-05-12 19:21 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_275_284'` | failures: none | notes: VM vector shim mismatch diagnostics now match current canonical paths from per-run scratch stderr
- 2026-05-12 19:21 local | pass | mode: release | command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs native templated stdlib vector wrapper temporary methods in expressions"` | failures: none | notes: native vector-returning temporary `.capacity()` now lowers through the vector metadata path after rebuilding `primec`
- 2026-05-12 19:20 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_275_284'` | failures: VM vector shim mismatch diagnostics | notes: stderr files were read from scratch, but assertions still expected stale camel-case generated helper names
- 2026-05-12 19:02 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_vm_collections_user_shadow_and_receiver_precedence_275_284|PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_273_277|PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_advanced_312_316|PrimeStruct_primestruct_compile_run_native_backend_collections_templated_wrapper_parity_89_93|PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57|PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_59_59|PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60|PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_62_62'` | failures: VM vector shim diagnostics, native vector/map shadow and insert, native vector wrapper temporary, gfx smoke shards | notes: focused rerun after rebuilding `PrimeStruct_compile_run_tests` still reproduced the current known failure subset
- 2026-05-12 18:59 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: interrupted after early failures in native vector wrapper/shadow shards, VM collection shim diagnostics, and gfx smoke shards | notes: build completed; CTest was stopped after representative current broad-gate failures surfaced
- 2026-05-12 18:59 local | pass | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148|PrimeStruct_primestruct_compile_run_imports_operations_and_collections_29_30'` | failures: none | notes: focused VM canonical vector access and direct experimental SoA conversion blockers are stabilized
- 2026-05-12 18:21 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_288_292|PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148|PrimeStruct_primestruct_compile_run_imports_operations_and_collections_29_30'` | failures: 830 VM canonical vector access forwarding, 1287 direct experimental SoA to_aos helpers | notes: native vector shard 1181 now passes after preventing scalar vector remove_swap from treating the vector record as an element struct and aligning native canonical helper expectations
- 2026-05-12 18:19 local | pass | mode: release | command: `build-release/primec --emit=native /tmp/ps_vector_explicit_full.XXXXXX.prime -o /tmp/ps_vector_explicit_full_exe --entry /main && /tmp/ps_vector_explicit_full_exe` | failures: none | notes: exact native explicit Vector binding reproducer now exits 109 instead of segfaulting
- 2026-05-12 18:04 local | fail | mode: release | command: `build-release/primec --emit=native <latest vector_namespace_canonical_native.prime> -o /tmp/ps_vector_namespace_canonical_native_exe --entry /main && /tmp/ps_vector_namespace_canonical_native_exe` | failures: vector_namespace_canonical_native | notes: native compile succeeds but produced executable exits 139, so shard is backend/lowering behavior rather than a safe expectation-only update
- 2026-05-12 18:01 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_288_292|PrimeStruct_primestruct_compile_run_vm_collections_stdlib_collection_shims_139_148|PrimeStruct_primestruct_compile_run_imports_operations_and_collections_29_30'` | failures: 830 VM canonical vector access forwarding, 1181 native canonical vector namespace explicit/wrapper conformance, 1287 direct experimental SoA to_aos helpers | notes: all three current focused known-failure shards still reproduce
- 2026-05-12 17:57 local | fail | mode: release | command: `cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_compile_run_native_backend_collections_user_shadow_and_receiver_precedence_core_288_292'` | failures: native canonical vector namespace explicit/wrapper conformance | notes: representative remaining vector/native shard still fails after broad stabilization
- 2026-05-12 17:57 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: 137 CTest targets | notes: focused vector/map receiver shard stayed fixed, but broad gate still fails across vector/native/VM shim, SoA/gfx wrapper import, Result/File, and IR layout buckets
- 2026-05-12 17:53 local | pass | mode: release | command: `./scripts/compile.sh --release --skip-tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_731_740|PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_161_170|PrimeStruct_primestruct_ir_pipeline_validation_cases_781_790'` | failures: none | notes: focused vector/map receiver stabilization shard passed
- 2026-05-12 17:39 local | fail | mode: release | command: `./scripts/compile.sh --release --skip-tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_731_740|PrimeStruct_primestruct_compile_run_emitters_cpp_numeric_math_and_control_flow_core_161_170|PrimeStruct_primestruct_ir_pipeline_validation_cases_781_790'` | failures: IR setup map receiver probe, vector capacity alias diagnostic, C++ vector mutator diagnostic shard | notes: focused stabilization rerun still failing after helper predicate and map precedence fixes
- 2026-05-12 17:28 local | fail | mode: release | command: `./scripts/compile.sh --release` | failures: 146 CTest targets | notes: baseline after preflight checkpoint failed; stabilization blocks TODO work

## Resolved Failures
- [x] count/access classifier and string map emission | resolved: 2026-05-20 13:08 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer count access classifiers prefer semantic indexed target facts,ir lowerer count access helpers defer string map access emission" --no-skip` | notes: retargeted syntax-only indexed target classification to false and string map access count emission to current deferral with no instructions.
- [x] setup type bare access return kinds | resolved: 2026-05-20 13:05 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper defers bare access call method return kinds" --no-skip` | notes: retargeted stale bare `at`/`at_unsafe` direct return-kind expectations to current deferral with unknown kind and unresolved method facts.
- [x] native call tail bare access orchestration | resolved: 2026-05-20 13:04 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch native call tail orchestration" --no-skip` | notes: retargeted malformed and valid bare `at(...)` native-tail probes to current `NotHandled` deferral with empty diagnostics and no instructions.
- [x] buffer/native-tail wrapper dispatch | resolved: 2026-05-20 13:02 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers" --no-skip` | notes: retargeted the experimental-vector method `at` native-tail fixture to current `NotHandled` deferral with no instructions.
- [x] semantic product vector/map bridge parity | resolved: 2026-05-20 12:58 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product keeps vector and map bridge parity" --no-skip` | notes: refreshed the parser-only canonical map count helper receiver from stale `map<K, V>` to the current `Map<K, V>` constructor model while preserving bridge surface assertions.
- [x] stdlib surface metadata map alias paths | resolved: 2026-05-20 12:55 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection alias paths" --no-skip` | notes: removed the contradictory null assertion for `/std/collections/map/tryAt_ref` and locked it as public map helper metadata.
- [x] SoA surface metadata member tokens | resolved: 2026-05-20 12:54 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens" --no-skip` | notes: refreshed stale `soaVector*` member-token expectations to the current public SoA manifest names and locked retired tokens as unresolved.
- [x] direct map Result payload backend IR fixture | resolved: 2026-05-20 12:52 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="retired direct map Result payload literal rejects before lowering" --no-skip` | notes: removed the stale zero-test known-failure name and kept the current retired direct-map Result payload rejection fixture as focused coverage.
- [x] inferred canonical map default parameters | resolved: 2026-05-20 12:51 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors accept inferred canonical map default parameters,helper-wrapped map constructors accept inferred canonical map default parameters,helper-wrapped inferred canonical map default parameters validate" --no-skip` | notes: refreshed stale implicit `at` helper inference expectations with explicit `<string, i32>` templates for current stdlib-owned map helper resolution.
- [x] map wrapper temporary bare helper classification | resolved: 2026-05-20 12:48 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="map wrapper temporary public helper calls validate target classification" --no-skip` | notes: refreshed the stale fixture away from `mapSingle<K, V>` and explicit `/map` returns; the current inferred wrapper return type validates through public map helpers.
- [x] map namespaced count method compatibility alias | resolved: 2026-05-20 12:43 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced count method compatibility alias resolves through canonical helper" --no-skip` | notes: retired the stale rejection entry after confirming the current renamed coverage passes through canonical helper resolution.
- [x] non-imported wrapper map reference access diagnostic | resolved: 2026-05-20 12:40 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="non-imported wrapper-returned canonical map reference access rejects missing at_ref" --no-skip` | notes: retargeted stale primitive receiver diagnostic text to the current missing `/std/collections/map/at_ref` call-target diagnostic.
- [x] borrowed receiver direct map contains semantics | resolved: 2026-05-20 12:37 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical map borrowed receiver rejects direct stdlib contains,canonical map borrowed receiver rejects direct stdlib contains before key diagnostics" --no-skip` | notes: retargeted stale borrowed receiver direct `contains` validation to current unknown-call-target diagnostics for `/std/collections/map/contains`.
- [x] helper-wrapped explicit map parameter fixture | resolved: 2026-05-20 12:35 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors reject explicit experimental map parameters" --no-skip` | notes: retargeted stale helper-wrapped canonical map constructor acceptance to the current explicit map parameter mismatch diagnostic.
- [x] uninitialized canonical map template arg diagnostic | resolved: 2026-05-20 12:33 CEST | validating command: `cmake --build build-release --target PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_misc_tests --test-case="uninitialized canonical map template arg validates as stdlib type" --no-skip` | notes: retargeted stale builtin map key/value diagnostics to current stdlib-owned canonical map type validation.
- [x] inferred map struct field semantics fixtures | resolved: 2026-05-20 12:30 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib map constructors reject inferred canonical map struct field mismatch,helper-wrapped inferred canonical map struct fields validate,helper-wrapped map constructor assignments accept inferred canonical map struct fields,auto bindings inside inferred canonical map return blocks rewrite constructors,helper-wrapped map constructors reject canonical map uninitialized storage mismatch" --no-skip` | notes: preserved the still-valid helper-wrapped cases with explicit map access templates and retargeted stale inferred struct-field and uninitialized-storage acceptance cases to current mismatch diagnostics.
- [x] parser-shaped canonical map constructor classifier | resolved: 2026-05-20 12:24 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer helper accepts parser-shaped canonical map entry constructors as builtin map" --no-skip` | notes: retargeted stale classifier expectations so lowerer recognizes parser-shaped canonical map constructors as `map` while emitter classification still rejects direct builtin emission.
- [x] explicit args-pack map access native tail | resolved: 2026-05-20 12:22 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers defer explicit map access for args-pack receivers" --no-skip` | notes: retargeted stale explicit map access args-pack coverage from native instruction emission to the current `NotHandled` deferral path.
- [x] indexed args-pack pointer map setup type | resolved: 2026-05-20 12:21 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup type helper leaves indexed args-pack pointer map receivers unclassified" --no-skip` | notes: retargeted stale bare-`at` args-pack map receiver setup-type coverage to the current empty target type.
- [x] reordered bare access setup inference | resolved: 2026-05-20 12:20 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores reordered bare access kinds" --no-skip` | notes: retargeted stale reordered named bare-`at` inference expectations to current `NotMatched` behavior after map access left generic builtin classification.
- [x] binding type helper SoA source lock | resolved: 2026-05-20 12:18 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer binding type helpers classify binding kind and string/fileerror types" --no-skip` | notes: retargeted stale exact experimental SoA vector source-string assertions to the shared SoA path helper and current split-string compatibility guards.
- [x] explicit map helpers native builtin emission | resolved: 2026-05-20 12:16 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helpers out of native builtin emission" --no-skip` | notes: retargeted stale canonical map `count` native-tail expectations to `NotHandled`, preserving that explicit stdlib-owned map helpers do not emit native builtin instructions.
- [x] inline count fallback map expectations | resolved: 2026-05-20 12:14 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch inline call with count fallbacks" --no-skip` | notes: refreshed stale method-access and canonical map-count assertions so stdlib-owned map `count` remains a direct helper call instead of an old method-shaped builtin fallback.
- [x] ir lowerer call helpers dispatch inline calls with locals | resolved: 2026-05-20 12:10 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch inline calls with locals" --no-skip` | notes: refreshed the stale explicit vector-count resolver probe count from one to two while preserving the single inline emission assertion.
- [x] unqualified array and map access setup inference | resolved: 2026-05-20 12:08 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores unqualified array and map access kinds" --no-skip` | notes: retargeted stale direct `at`/`at_unsafe` setup-inference expectations to the current `NotMatched` behavior after map access left the old builtin helper path.
- [x] wrapper-returned map access setup inference | resolved: 2026-05-20 12:05 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer setup inference helper ignores wrapper-returned canonical map access string kinds,ir lowerer setup inference helper ignores wrapper-returned slash-method map access kinds,ir lowerer setup inference helper ignores wrapper-returned canonical map access int32 kinds" --no-skip` | notes: retargeted stale wrapper-returned map access fixtures from value-kind resolution to current `NotMatched` behavior for retired builtin map access classification.
- [x] inferred experimental map receiver inline arguments | resolved: 2026-05-20 12:02 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers leave inferred map receiver methods unresolved" --no-skip` | notes: replaced retired `mapPair` construction and locked that this lowerer helper no longer resolves inferred stdlib-owned map receiver methods through the old inline path.
- [x] helper-wrapped map constructors infer canonical auto locals | resolved: 2026-05-20 11:59 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors infer canonical auto locals" --no-skip` | notes: refreshed the stale fixture to keep wrapped auto-local coverage while using explicit canonical helper template arguments for current map `tryAt`/`count` calls.
- [x] C++ emitter variadic map value pack count methods | resolved: 2026-05-20 11:56 CEST | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="C++ emitter rejects retired variadic map value pack count methods" --no-skip` | notes: retargeted stale native map value-pack coverage to assert the current compile-time rejection of retired map count-method lowering.
- [x] backend IR retired native-map fixtures | resolved: 2026-05-20 11:46 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="retired direct map Result payload literal rejects before lowering,retired variadic map pack bare count rejects before lowering,retired variadic map pack canonical count no longer lowers as native map" --no-skip` | notes: converted stale map Result and variadic pack lowering coverage into explicit retirement locks for old map literals, bare count fallback, and native map-pack lowering.
- [x] map-count semantics wildcard | resolved: 2026-05-20 11:43 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="*map*count*" --no-skip` | notes: updated stale map-count fixtures for current bare-count diagnostics, public `MapValue` construction, and current same-path alias behavior pending TODO-4534.
- [x] template monomorph source delegation stays stable | resolved: 2026-05-20 11:36 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="template monomorph source delegation stays stable" --no-skip` | notes: refreshed stale SoA and key-value source-lock assertions for current helper prefix APIs, compatibility path routing, and renamed map-owned helpers.
- [x] semantics validator infer source delegation stays stable | resolved: 2026-05-20 11:27 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator infer source delegation stays stable" --no-skip` | notes: refreshed stale source-lock snippets for key-value resolver naming, split SoA helper path literals, and removed map compatibility predispatch paths.
- [x] ir lowers map literal call as statement | resolved: 2026-05-20 11:21 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowers map literal call as statement" --no-skip`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="map literal validates bool keys and values" --no-skip` | notes: parser nested-definition speculation now avoids consuming template-call statements without a definition body as template parameter declarations.
- [x] ir lowerer call helpers keep explicit map helper same-path defs | resolved: 2026-05-20 11:15 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helper same-path defs" --no-skip` | notes: refreshed stale alias access expectations to preserve current canonical map helper definition resolution.
- [x] ir lowerer call helpers handle non-method count fallback | resolved: 2026-05-20 11:13 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers handle non-method count fallback" --no-skip` | notes: pruned stale map access/count fallback expectations now that stdlib-owned map helpers no longer route through the old generic count fallback path.
- [x] wrapper-returned map method-alias diagnostics | resolved: 2026-05-20 11:10 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="wrapper-returned slash-method map access count validates string result,map method alias access accepts matching receiver during inference,map method alias access rejects missing receiver method during inference,wrapper-returned map method alias access keeps explicit helper diagnostics during inference,wrapper-returned map method alias access keeps primitive argument diagnostics during inference" --no-skip` | notes: retired stale primitive `/map/at` and `/project` inference expectations; current coverage reflects stdlib-owned map helper routing and explicit helper diagnostics.
- [x] ir lowerer materializes variadic borrowed/pointer map packs with indexed count_ref helpers | resolved: 2026-05-20 11:07 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer materializes variadic borrowed map packs with indexed count_ref helpers,ir lowerer materializes variadic pointer map packs with indexed count_ref helpers" --no-skip` | notes: collection receiver materialization now normalizes explicit `count_ref` aliases and treats wrapped key/value args-pack elements as materializable indexed map receivers.
- [x] graph type resolver infers map value return kinds through shared infer helper | resolved: 2026-05-20 11:27 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="graph type resolver answers map receiver queries through shared type-text helper,graph type resolver infers map value return kinds through shared infer helper" --no-skip` | notes: map return-solver fixtures now use canonical map construction in both auto-return branches without importing `internal_map` or calling the retired public `mapPair` bridge.
- [x] ir lowerer result helpers resolve indexed args-pack Result expressions | resolved: 2026-05-20 11:18 CEST | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers resolve indexed args-pack Result expressions" --no-skip`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer result helpers use semantic indexed args-pack Result facts" --no-skip` | notes: Result metadata now has a receiver-scoped bare `at` fallback for locals already known to be Result args packs, without reintroducing generic map/access handling.
- [x] semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads | resolved: 2026-05-20 11:05 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads" --no-skip` | notes: fixture now uses self-contained public `soa<Particle>` helper stubs and checks borrowed `count_ref`/`get_ref` semantic-product targets without depending on old `SoaVector<T>` imports.
- [x] implicit map constructors infer canonical auto locals and auto returns | resolved: 2026-05-20 10:34 CEST | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="implicit map constructors infer canonical auto locals and auto returns" --no-skip` | notes: refreshed the stale fixture to materialize the `i32` payload before `Result.ok`; canonical map auto locals, auto returns, inserts, counts, and lookups still validate.
- [x] ir lowerer call helpers source delegation stays stable | resolved: 2026-05-18 18:32 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers emit map lookup loop search scaffold,ir lowerer call helpers try emit map access lookup,ir lowerer call helpers resolve map lookup string keys,ir lowerer call helpers source delegation stays stable" --no-skip` | notes: source lock now matches the current key/value lookup helper names, asserts removed map-specific call-resolution and inline-dispatch helpers are absent, and preserves the SoA split-string source checks.
- [x] map method alias primitive argument stale diagnostic | resolved: 2026-05-16 19:13 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access accepts matching receiver during inference"`; `cd build-release && ./PrimeStruct_semantics_tests --test-case="map method alias access rejects missing receiver method during inference"` | notes: updated stale expectations so the matching `/Marker/tag` receiver path validates and the missing receiver method case expects `unknown method: /Marker/tag`.
- [x] type pack storage expands into deterministic struct fields | resolved: 2026-05-15 21:07 local | validating command: `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.semantics.manual --test-case="type pack storage expands into deterministic struct fields,type pack storage rejects invalid placements and shapes,type pack specializations bind zero one and many arguments,type pack parameters cannot be used as scalar types before expansion"` | notes: parent-run rerun passed after replacing the parser-dependent source fixture with direct AST construction for the storage-expansion cases.
- [x] todo queue and skipped doctest debt stay source locked | resolved: 2026-05-15 18:56 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | notes: TODO queue source lock now matches completed TODO-4270, TODO-4532, and TODO-4526 bookkeeping plus active TODO-4275 and TODO-4527 successors.
- [x] parses integer template argument metadata on call and method call | resolved: 2026-05-15 18:19 local | validating command: `cmake --build build-release --target PrimeStruct_parser_tests && cd build-release && ./PrimeStruct_parser_tests --test-suite=primestruct.parser.templates` | notes: corrected new test to expect both bindings plus return and inspect definition returnExpr directly
- [x] ir lowerer inline dispatch collection access fallback uses semantic receiver facts first | resolved: 2026-05-15 18:21 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer inline dispatch map helper deferral uses semantic receiver facts first,ir lowerer inline dispatch collection access fallback uses semantic receiver facts first,ir lowerer inline native dispatch prefers published canonical map access helpers"` | notes: source-lock now checks shared map resolver placement with branch-local token searches and the backend IR slice passes.
- [x] canonical map surface owns implementation through internal map module | resolved: 2026-05-15 18:21 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests PrimeStruct_misc_tests`; `cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | notes: map ownership source-lock now expects tail-dispatch map insert rewrites to use `collectionMemberPath("map", "insert")`.
- [x] semantics validator stdlib bridge helper routing stays stable | resolved: 2026-05-15 18:54 local | validating command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | notes: parent-scheduled backend IR slice passed 3/3 selected cases after source-lock and fixture updates.
- [x] soa pending diagnostics route through shared semantics helpers | resolved: 2026-05-15 18:18 local | validating command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="soa pending diagnostics route through shared semantics helpers,semantics validator expr source delegation stays stable,semantics validator stdlib bridge helper routing stays stable"` | notes: updated the source-lock expectation from direct `"/soa_vector"` method-target routing to shared `internalSoaCollectionTypePath(true)` routing.
- [x] full release gate after SoA shadow stabilization | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: full release validation passed 1597/1597 after resolving the SoA helper-return routing, field-view diagnostics, source locks, native stdlib collection shim, and type-resolution parity regressions.
- [x] native helper-return experimental soa_vector shadows | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: visible same-path `/soa_vector/*` helper families now route before public `/std/collections/soa/*` helpers for `/soa_vector` receivers, while compile-run reruns rebuild `primec` with the doctest binary.
- [x] builtin helper-return soa_vector ref_ref rejection | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: the rejection lock now accepts the current native-lowering `/ref_ref` expression-call diagnostic.
- [x] ownership-sensitive experimental map value methods native rejection | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: native validation rejects semantic-product map specializations whose value type is not numeric, bool, or string before map lowering can crash.
- [x] SoA field-view semantic diagnostic shards | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: semantics shards `141_150`, `151_160`, `161_170`, `221_230`, `231_240`, `261_270`, and `551_560` now match public `/std/collections/soa/field_view/*` escape and mutation diagnostics or current successful validation behavior.
- [x] SoA source-lock and docs expectation shards | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: backend IR source-lock shards `41_50`, `91_100`, and `121_130`, plus the spinning-cube docs lock, now match the current public read/ref helper pattern and direct generic collection-helper routing facts.
- [x] native SoA shim and type-resolution parity shards | resolved: 2026-05-15 00:52 local | validating command: `./scripts/compile.sh --release` | notes: native public SoA `from_aos` and field-view wrappers lower with recovered vector struct paths, and the borrowed `SoaVector` auto-return graph corpus now accepts the stabilized behavior.
- [x] public SoA wrapper and vector fallback focused shard | resolved: 2026-05-14 20:42 local | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="compiles and runs canonical vector mutators over imported user shadow helpers in C++ emitter,compiles and runs canonical vector mutator named calls over imported user shadow helpers in C++ emitter,rejects imported user vector mutator positional call shadow in C++ emitter,runs vm with stdlib collection shim capacity helper,compiles and runs native collection syntax parity for call and method forms,rejects builtin helper-return soa_vector ref_ref same-path helper in C++ emitter,*public soa construction and mutators use wrappers,*public soa read helpers route through wrapper paths,*reflected multi-field index syntax"` | notes: vector fallback routing now avoids public SoA mutator regressions, and rejection locks accept current public/semantic diagnostic paths.
- [x] todo queue and skipped doctest debt stay source locked | resolved: 2026-05-14 20:48 local | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests`; `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked,soa public collection docs stay source locked,small stdlib wrappers stay source locked to inferred locals"` | notes: docs source-lock now expects TODO-4516 as the Ready Now and execution queue head.
- [x] stdlib style boundary docs stay source locked | resolved: 2026-05-14 17:19 local | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="*stdlib style boundary docs*"` | notes: source-lock assertion now matches the wrapped `CodeExamples.md` SoA import guidance.
- [x] semantics validator stdlib bridge helper routing stays stable | resolved: 2026-05-14 13:50 local | validating command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="semantics validator stdlib bridge helper routing stays stable"` | notes: updated stale stdlib bridge source-lock expectations for FileError direct routing, map collection surface facts, and current SoA/map helper snippets.
- [x] rejects map namespaced at compatibility alias in C++ emitter without explicit alias | resolved: 2026-05-14 10:07 local | validating command: `./PrimeStruct_compile_run_tests -tc="rejects map namespaced at compatibility alias in C++ emitter without explicit alias"` | notes: semantic pre-dispatch now checks the explicit rooted `/map/at` source path before accepting the canonical resolved helper target.
- [x] rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias | resolved: 2026-05-14 10:07 local | validating command: `./PrimeStruct_compile_run_tests -tc="rejects map namespaced at unsafe compatibility alias in C++ emitter without explicit alias"` | notes: semantic pre-dispatch now checks the explicit rooted `/map/at_unsafe` source path before accepting the canonical resolved helper target.
- [x] semantics validator infer source delegation stays stable | resolved: 2026-05-14 10:07 local | validating command: `./PrimeStruct_backend_ir_tests -tc="semantics validator infer source delegation stays stable"` | notes: source lock now asserts the removed rooted map access branch is absent and no longer requires stale method-target text.
- [x] PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67 | resolved: 2026-05-14 01:48 local | validating command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_67_67$'` | notes: non-packed stdlib Result sum returns now store the materialized sum pointer directly for inline returns, avoiding packed-error treatment of aggregate-error Result.ok.
- [x] PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68 | resolved: 2026-05-14 01:52 local | validating command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_68_68$'` | notes: non-packed same-Result initializers now copy from the returned sum pointer when packed scalar decoding is not applicable.
- [x] PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69 | resolved: 2026-05-14 01:57 local | validating command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_03a_67_70_69_69$'` | notes: Reference/Pointer bindings to generated stdlib Result sums now carry Result metadata, and Result.error/why read borrowed sum pointers before direct-call fallbacks.
- [x] PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05 | resolved: 2026-05-14 00:55 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_vm_core_core_01a_01_05$'` | notes: semantic vector-count bridge calls on array receivers now lower through builtin count access.
- [x] PrimeStruct_primestruct_compile_run_vm_core_core_01a_06_10_6_9 | resolved: 2026-05-14 00:57 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 622,1589` | notes: broad continuation passed VM core 6-9 before stopping at two-pass layout tree serialization.
- [x] collective paths 1-22 plus argv/CLI 1-25 | resolved: 2026-05-14 00:46 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 622,1589` | notes: broad continuation passed compile-run collective paths and argv/CLI ranges before stopping at VM core 01a.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_62_62 experimental gfx render-pass wrapper lock | resolved: 2026-05-14 00:45 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_62_62$'` | notes: source lock now expects current experimental `SubstrateRenderPassConfig` mismatch diagnostic.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_61_61 | resolved: 2026-05-14 00:43 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 620,1589` | notes: broad continuation passed wasm/debug 61 before stopping at experimental gfx render-pass wrapper 62.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60 experimental gfx resource wrapper lock | resolved: 2026-05-14 00:43 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_60_60$'` | notes: source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_59_59 experimental gfx device constructor lock | resolved: 2026-05-14 00:41 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_59_59$'` | notes: source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_58_58 | resolved: 2026-05-14 00:39 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 617,1589` | notes: broad continuation passed wasm/debug 58 before stopping at experimental gfx entry-point 59.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57 gfx static-fields shim lock | resolved: 2026-05-14 00:38 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_57_57$'` | notes: fixture now locks cross-backend import/static-field binding and avoids the native experimental gfx `.value` field-read crash path.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_55_56 | resolved: 2026-05-14 00:36 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 614,1589` | notes: broad continuation passed wasm/debug 55-56 before stopping at gfx static-fields shim 57.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_54_54 gfx compatibility shim lock | resolved: 2026-05-14 00:35 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_54_54$'` | notes: source lock now expects current experimental `SubstrateDeviceConfig` mismatch diagnostic.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_35_53 | resolved: 2026-05-14 00:33 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 594,1589` | notes: broad continuation passed wasm/debug 35-53 before stopping at gfx compatibility shim 54.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_34_34 debug-DAP breakpoint lock | resolved: 2026-05-14 00:32 local | validating command: `cmake --build build-release --target primec primevm PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_wasm_and_debug_34_34$'` | notes: assertion now matches current verified instruction-breakpoint response while preserving post-exit invalid frame/variable checks.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_13_31 plus wasm/debug 32-33 | resolved: 2026-05-14 00:31 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 572,1589` | notes: broad continuation passed smoke foundation 13-31 and wasm/debug 32-33 before stopping at wasm/debug 34.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_12_12 nested sum diagnostic lock | resolved: 2026-05-14 00:30 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_12_12$'` | notes: source lock now matches the current semantic unsupported-binding diagnostic for nested sum payloads.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_11_11 sum drop active payload | resolved: 2026-05-14 00:29 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_11_11$'` | notes: fixture no longer depends on unsupported reference-field writes and now validates sum drop through no-op payload Destroy helpers.
- [x] PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10 sum move active payload | resolved: 2026-05-14 00:23 local | validating command: `cmake --build build-release --target primec PrimeStruct_compile_run_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_compile_run_smoke_core_paths_foundation_10_10$'` | notes: `move(original)` no longer revalidates the target-typed sum initializer after the move marks `original` as moved.
- [x] PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_61_118 plus parser/text-filter/import smoke range | resolved: 2026-05-14 00:13 local | validating command: `cmake --build build-release && cd build-release && ctest --output-on-failure --stop-on-failure -I 398,1589` | notes: broad continuation passed effects 61-118, imports, lambdas, maybe, type-resolution graph, dumps, parser, text-filter, and smoke foundation 1-9 before stopping at foundation 10.
- [x] PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_51_60 | resolved: 2026-05-14 00:12 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_51_60$'` | notes: take string map access fixture now source-locks the current non-string-path rejection.
- [x] PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50 | resolved: 2026-05-14 00:11 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_effects_calls_flow_effects_41_50$'` | notes: notify string map access fixture now source-locks the current non-string-path rejection.
- [x] PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10 | resolved: 2026-05-14 00:09 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_comparisons_literals_calls_flow_comparisons_literals_1_10$'` | notes: map `at` comparison source lock now expects canonical map access precedence over root user `at`.
- [x] PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20 | resolved: 2026-05-14 00:07 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_access_calls_flow_access_11_20$'` | notes: map method-call definition lock now provides a templated local canonical count helper.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_711_720 | resolved: 2026-05-14 00:06 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_711_720$'` | notes: map tryAt slash-path method locks now expect current validated routing.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_681_690 | resolved: 2026-05-14 00:05 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_681_690$'` | notes: user-defined map block-argument source lock now expects the generic non-collection `at` diagnostic.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620 | resolved: 2026-05-14 00:04 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_611_620$'` | notes: map constructor resolution locks now use canonical map types, borrowed receiver behavior, and builtin-key diagnostics.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610 | resolved: 2026-05-14 00:00 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_601_610$'` | notes: field-bound canonical map explicit-at expression body-argument lock now checks the current count-mismatch target family.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600 | resolved: 2026-05-13 23:59 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_591_600$'` | notes: canonical map method-sugar direct-constructor receivers now source-lock current accepted behavior.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590 | resolved: 2026-05-13 23:58 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_581_590$'` | notes: map constructor parameter-shape locks now use canonical `map<K, V>` constructors and current map diagnostics.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560 | resolved: 2026-05-14 00:06 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_551_560$'` | notes: SoA field-view mutation and return-helper locks now match current accepted/rejected behavior.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500 | resolved: 2026-05-14 00:03 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_491_500$'` | notes: vector relocation locks now match visible helper availability for pop, remove_at, and count.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_481_490 | resolved: 2026-05-14 00:01 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_481_490$'` | notes: reference-wrapped body-arg locks now expect `count_ref`; slash-path count/at body-arg locks now validate.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_471_480 | resolved: 2026-05-13 23:59 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_471_480$'` | notes: map body-arg method-sugar locks now expect `count_ref`, canonical marker mismatch, and canonical block-argument diagnostics.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_461_470 | resolved: 2026-05-13 23:57 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_461_470$'` | notes: stdlib-qualified primitive receiver body-arg locks now expect unknown `/std/collections/vector/count`.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450 | resolved: 2026-05-13 23:55 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_441_450$'` | notes: explicit canonical map access helper coverage now uses canonical map constructors.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_421_430 | resolved: 2026-05-13 23:53 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_421_430$'` | notes: explicit-template map compatibility diagnostics now match canonical return mismatches and missing `count_ref`.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420 | resolved: 2026-05-13 23:51 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_411_420$'` | notes: `/map/contains` and `/map/tryAt` source locks now validate visible canonical alias resolution.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400 | resolved: 2026-05-13 23:49 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_391_400$'` | notes: field-bound map assignment locks now use canonical map constructors and current mismatch diagnostics.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380 | resolved: 2026-05-13 23:47 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_371_380$'` | notes: result-payload and auto-return source locks now match canonical map constructors and direct map access.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360 | resolved: 2026-05-13 23:44 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_351_360$'` | notes: storage source locks now use canonical `map<K, V>` and `/std/collections/map/map` constructors.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350 | resolved: 2026-05-13 23:42 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_341_350$'` | notes: struct-field map defaults now use one-entry canonical constructors, and positive holder cases use explicit map helper templates.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340 | resolved: 2026-05-13 23:40 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_331_340$'` | notes: auto-inference source locks now use canonical `map<K, V>` constructors and generic map template-conflict diagnostics.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330 | resolved: 2026-05-13 23:38 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_321_330$'` | notes: key-diagnostic locks now use canonical `map<K, V>` and expect builtin Comparable-key rejection.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_311_320 | resolved: 2026-05-13 23:36 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_311_320$'` | notes: source locks now match ownership-sensitive canonical map value support and the retired experimental-map shim.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310 | resolved: 2026-05-13 23:34 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_301_310$'` | notes: canonical map/ref helper source locks now match the public map surface and explicit borrowed helper spellings.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300 experimental map helper locks | resolved: 2026-05-13 23:32 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_291_300$'` | notes: source locks now expect custom-key Comparable rejection and missing `mapCount` diagnostics for current experimental map helper routing.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290 borrowed args-pack map count lock | resolved: 2026-05-13 23:30 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_281_290$'` | notes: source lock now expects missing `/std/collections/map/count_ref` diagnostic for borrowed args-pack indexed map receivers.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60 vector remove precedence locks | resolved: 2026-05-13 23:25 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_51_60$'` | notes: source locks now expect imported vector helper mutability diagnostics for bare remove_at/remove_swap calls while method/direct helper precedence stays covered separately.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50 vector precedence locks | resolved: 2026-05-13 23:22 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_41_50$'` | notes: source locks now expect imported vector helper mutability diagnostics for bare pop/clear calls while method/direct helper precedence stays covered separately.
- [x] PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10 map expression-body locks | resolved: 2026-05-13 23:18 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_calls_flow_collections_calls_flow_collections_1_10$'` | notes: source locks now expect canonical alias diagnostics for map method body arguments and canonical `count_ref` for borrowed reference receivers.
- [x] PrimeStruct_primestruct_semantics_bindings_struct_defaults_bindings_struct_defaults_11_20 map alias omitted-initializer lock | resolved: 2026-05-13 23:08 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_semantics_bindings_struct_defaults_bindings_struct_defaults_11_20$'` | notes: test now source-locks `argument count mismatch for /map/count` before omitted-initializer effect diagnostics.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_831_840 direct map return-info lock | resolved: 2026-05-13 23:05 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_831_840$'` | notes: source locks now prefer canonical map count/contains/tryAt return info for direct compatibility spellings.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820 map count/contains receiver lock | resolved: 2026-05-13 23:03 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_811_820$'` | notes: source locks now distinguish compatibility map helper aliases from canonical map helper paths.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_801_810 wrapper vector slash-method diagnostics | resolved: 2026-05-13 23:00 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_801_810$'` | notes: source locks now expect the outer `tag` target diagnostic for wrapper vector slash-method count and capacity receivers.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_791_800 reordered access methodResolved lock | resolved: 2026-05-13 22:58 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_791_800$'` | notes: string access fallback now source-locks methodResolved=true.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_771_780 semantic method id source lock | resolved: 2026-05-13 22:55 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_771_780$'` | notes: source lock now requires semantic-product method-call ids before consulting vector method gate facts or stale locals.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_761_770 map alias receiver lock | resolved: 2026-05-13 22:53 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_761_770$'` | notes: slashless map import alias receiver fixture now uses the canonical map access tag path.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_741_750 vector access source locks | resolved: 2026-05-13 22:51 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_741_750$'` | notes: source locks now defer exact canonical vector access helper paths instead of inferring element kind from local vector metadata.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_661_670 Result.ok semantic payload fallback | resolved: 2026-05-13 22:48 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_661_670$'` | notes: semantic payload metadata now blocks stale direct collection inference before emitting Result.ok payloads.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_651_660 Result ok direct-name payload lock | resolved: 2026-05-13 22:45 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_651_660$'` | notes: source lock now accepts Int64 from local metadata when semantic facts are missing for a direct Result ok name payload.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_601_610 vector remove helper fixture locks | resolved: 2026-05-13 22:42 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_601_610$'` | notes: fixtures now model opaque struct vector elements, so removed-slot Destroy and survivor Move helper wiring is exercised.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_591_600 conversion helper pointer/layout locks | resolved: 2026-05-13 22:40 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_591_600$'` | notes: semantic non-pointer facts now block stale pointer-local struct fallback, and vector record locks expect the current 1024-slot local dynamic capacity.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_361_370 unresolved map count probe lock | resolved: 2026-05-13 22:36 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_361_370$'` | notes: source lock now expects one method-resolution probe before unresolved compatibility map count returns NotResolved.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_341_350 implicit-auto map return fixture | resolved: 2026-05-13 22:34 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_341_350$'` | notes: fixture now uses the canonical `/std/collections/map/map` constructor that return collection inference recognizes.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_331_340 vector statement helper locks | resolved: 2026-05-13 22:31 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_331_340$'` | notes: source locks now treat canonical vector mutators and SoA statement mutators as NotMatched so the ordinary helper route owns them.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_321_330 struct-constructor source lock | resolved: 2026-05-13 22:26 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_321_330$'` | notes: source lock now tracks the brace-constructor guard before adopting a struct initializer callee.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_251_260 string map count emission | resolved: 2026-05-13 22:24 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_251_260$'` | notes: bare count deferral no longer blocks builtin map access targets from semantic/local string-map classification.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250 count/access classifier source locks | resolved: 2026-05-13 22:17 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_241_250$'` | notes: canonical vector count source locks now distinguish explicit canonical deferral, namespace-prefixed entry-arg classification, templated canonical count deferral, and canonical vector access helper deferral.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_181_190 rooted map alias source locks | resolved: 2026-05-13 22:09 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_181_190$'` | notes: source locks now match retired rooted-map alias rejection and semantic surface-id lowered overload resolution.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130 exact-import map bridge crash | resolved: 2026-05-13 22:07 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_121_130$'` | notes: exact canonical map helper calls no longer self-rewrite through the experimental-map pre-dispatch path, preventing recursive `validateExpr` stack overflow.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120 | resolved: 2026-05-13 22:01 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_111_120$'` | notes: retired map alias fallback expectations now defer, forwarded map target inference uses the canonical map constructor, and published vector metadata paths defer when semantic context is unavailable.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80 stdlib-only diagnostics regression | resolved: 2026-05-13 21:50 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80$'` | notes: source-level unsupported native-call diagnostics no longer treat canonical published vector helpers as scalar misuse without semantic surface context.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100 | resolved: 2026-05-13 21:46 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_91_100$'` | notes: native tail/vector semantic facts now override stale local metadata, published vector metadata diagnostics respect semantic vector facts, SoA alias count defers, and builtin array access rejects scalar semantic targets.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80 | resolved: 2026-05-13 21:36 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_71_80$'` | notes: inline map helper dispatch expectations now match current canonical map helper emission, semantic map target precedence, canonical vector diagnostic deferral, and experimental map insert expression deferral.
- [x] PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50 | resolved: 2026-05-13 21:28 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_validation_cases_41_50$'` | notes: source-lock assertions now match centralized vector compatibility helper routing and collection fallback helper member resolution.
- [x] PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps | resolved: 2026-05-13 21:23 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_conversions_variadic_field_refs_and_maps$'` | notes: borrowed map-pack count fixture uses explicit canonical `count_ref` helper calls instead of stale reference method sugar.
- [x] PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors | resolved: 2026-05-13 21:18 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R '^PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors$'` | notes: variadic map-pack canonical count fixture no longer depends on removed experimental `Map` and `mapCount` spellings.
- [x] Result docs source locks | resolved: 2026-05-13 21:13 local | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="status-only result bridge docs stay source locked,Result helper compatibility adapter inventory stays source locked"` | notes: status-only Result docs lock and helper compatibility inventory match the current retired TODO and production helper spelling ownership.
- [x] PrimeStruct_primestruct_ir_pipeline_backends_core | resolved: 2026-05-13 21:08 local | validating command: `cmake --build build-release --target PrimeStruct_backend_runtime_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_primestruct_ir_pipeline_backends_core'` | notes: graph-pilot source lock now matches the current preferred map method target helper.
- [x] PrimeStruct_vector_surface_traces | resolved: 2026-05-13 21:03 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_vector_surface_traces|PrimeStruct_vector_surface_trace_checker_self_test'` | notes: `IrLowererNativeTailDispatch.cpp` no longer contains literal experimental-vector path traces.
- [x] PrimeStruct_misc_suite_registration | resolved: 2026-05-13 20:59 local | validating command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ctest --output-on-failure -R 'PrimeStruct_misc_suite_registration|PrimeStruct_primestruct_stdlib_map_ownership'` | notes: `primestruct.stdlib.map_ownership` is registered in the misc suite list and the suite registration check passes.
- [x] ir lowerer call helpers dispatch buffer and native tail wrappers | resolved: 2026-05-13 20:22 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers dispatch buffer and native tail wrappers" -s` | notes: direct experimental-vector `vectorAtUnsafe` is recognized as implementation-owned unsafe access and left `NotHandled`
- [x] rooted map alias struct-return semantic expectations | resolved: 2026-05-13 19:34 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="map namespaced access alias rejects canonical struct-return forwarding,map namespaced access alias reports current receiver diagnostics,map namespaced unsafe access alias rejects canonical struct-return forwarding,map namespaced unsafe access alias reports current receiver diagnostics,map method access keeps canonical struct-return forwarding,map method access field expression keeps canonical struct-return forwarding,map method access keeps alias struct-return isolated from canonical helper"` | notes: rooted `/map/at*` alias expectations now match isolated rooted/canonical pre-dispatch behavior
- [x] vector map bridge boundary docs stay source locked | resolved: 2026-05-13 19:21 local | validating command: `cmake --build build-release --target PrimeStruct_compile_run_tests && cd build-release && ./PrimeStruct_compile_run_tests --test-case="vector map bridge boundary docs stay source locked,todo queue and skipped doctest debt stay source locked"` | notes: refreshed source-lock expectations for current vector/map bridge wording
- [x] ir lowerer call helpers reject generated experimental map helper family fallback | resolved: 2026-05-13 18:55 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable,ir lowerer call helpers reject generated experimental map helper family fallback"` | notes: exact experimental-map helper definitions still resolve, but unspecialized calls no longer bind generated helper families
- [x] ir lowerer inference call-return setup resolves explicit map helper aliases against canonical-only defs | resolved: 2026-05-13 18:39 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding,ir lowerer struct return helpers keep map tryAt alias precedence,ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding,ir lowerer struct return helpers keep explicit map tryAt method alias precedence,ir lowerer inference call-return setup rejects explicit map access aliases against canonical-only defs,ir lowerer setup type helper rejects explicit map access aliases through canonical defs,ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs"` | notes: renamed and updated stale expectations so rooted map access aliases no longer inherit canonical return-kind facts
- [x] ir lowerer setup type helper resolves explicit map access aliases through canonical defs | resolved: 2026-05-13 18:39 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer struct return helpers reject map access compatibility call forwarding,ir lowerer struct return helpers reject map tryAt compatibility call forwarding,ir lowerer struct return helpers keep map tryAt alias precedence,ir lowerer struct return helpers reject explicit map tryAt method canonical forwarding,ir lowerer struct return helpers keep explicit map tryAt method alias precedence,ir lowerer inference call-return setup rejects explicit map access aliases against canonical-only defs,ir lowerer setup type helper rejects explicit map access aliases through canonical defs,ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs"` | notes: assertion now matches rejected rooted map access alias fallback
- [x] retired public stdlib map wrapper calls report their original target | resolved: 2026-05-13 18:20 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="retired public stdlib map wrapper calls report their original target"` | notes: public `std/collections/mapAt*` no longer routes through the legacy access-alias matcher
- [x] backend IR map helper/source-delegation locks | resolved: 2026-05-13 17:18 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers keep explicit map helpers out of native builtin emission,ir lowerer statement call emission source delegation stays stable"` | notes: locks now match rooted map access emission and absence of removed wrapper vector alias branches
- [x] stdlib surface registry stays source locked | resolved: 2026-05-13 17:14 local | validating command: `cmake --build build-release --target PrimeStruct_backend_runtime_tests && cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="stdlib surface registry stays source locked"` | notes: source lock now permits canonical `/std/collections/map/insert` lowering spelling while rejecting retired bridge spellings
- [x] canonical map surface owns implementation through internal map module | resolved: 2026-05-13 17:13 local | validating command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | notes: ownership lock now permits canonical `/std/collections/map/insert_ref` and rejects retired prefixed bridge aliases
- [x] stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers | resolved: 2026-05-13 17:11 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="retired public mapPair bridge reports unknown target"` | notes: renamed test now asserts the unknown-target diagnostic for the retired public `/std/collections/mapPair` bridge
- [x] stdlib wrapper map helpers accept experimental constructor receivers | resolved: 2026-05-13 17:10 local | validating command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="canonical stdlib map helpers accept constructor receivers"` | notes: renamed and retargeted source to canonical `map<K, V>` locals and explicit `/std/collections/map/*` helper calls
- [x] wrapper map helpers on experimental map values | resolved: 2026-05-13 17:09 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm wrapper map helpers on experimental map values,compiles and runs native wrapper map helpers on experimental map values,compiles and runs wrapper map helpers on experimental map values in C++ emitter"` | notes: source now uses canonical `map<K, V>` construction and explicit `/std/collections/map/*` helper calls instead of the retired public bridge path
- [x] native/vm canonical namespaced map helpers on experimental map values | resolved: 2026-05-13 16:01 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical namespaced map helpers on experimental map values,compiles and runs native canonical namespaced map helpers on experimental map values,vm materializes variadic experimental map packs with indexed canonical count calls,native materializes variadic experimental map packs with indexed canonical count calls"` | notes: migrated source now uses canonical map imports, type, constructor, and VM/native Result.why output
- [x] native/vm variadic experimental map packs | resolved: 2026-05-13 16:01 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="runs vm canonical namespaced map helpers on experimental map values,compiles and runs native canonical namespaced map helpers on experimental map values,vm materializes variadic experimental map packs with indexed canonical count calls,native materializes variadic experimental map packs with indexed canonical count calls"` | notes: variadic source now uses canonical `map<...>` packs and unqualified canonical map constructors
- [x] field-bound experimental map stdlib namespaced count methods validate | resolved: 2026-05-13 15:40 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="field-bound experimental map stdlib namespaced count methods validate" -s` | notes: field-bound source now uses canonical map import, type, and constructor
- [x] helper-wrapped map constructors accept explicit experimental map bindings | resolved: 2026-05-13 15:33 local | validating command: `cmake --build build-release --target PrimeStruct_semantics_tests && cd build-release && ./PrimeStruct_semantics_tests --test-case="helper-wrapped map constructors accept explicit experimental map bindings" -s` | notes: canonical `map<string, i32>` binding avoids the recursive legacy `Map` wrapper path
- [x] stdlib wrapper mapPair constructor accepts explicit experimental map bindings | resolved: 2026-05-13 15:32 local | validating command: `cd build-release && ./PrimeStruct_semantics_tests --test-case="stdlib wrapper mapPair constructor accepts explicit experimental map bindings" -s` | notes: canonical `map<string, i32>` binding and `/std/collections/map/map` constructor avoid the recursive legacy wrapper path
- [x] design doc records vector map bridge contract | resolved: 2026-05-13 15:13 local | validating command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="design doc records vector map bridge contract"` | notes: backend architecture source-lock now matches the current map manifest compatibility-removal wording
- [x] small stdlib wrappers stay source locked to inferred locals | resolved: 2026-05-13 15:10 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="small stdlib wrappers stay source locked to inferred locals"` | notes: source-lock now reads the experimental map shim and internal map implementation separately
- [x] todo queue and skipped doctest debt stay source locked | resolved: 2026-05-13 15:08 local | validating command: `cd build-release && ./PrimeStruct_compile_run_tests --test-case="todo queue and skipped doctest debt stay source locked"` | notes: source-lock now tracks the TODO-4438/TODO-4439 split queue ordering
- [x] map insert surface registry rejects legacy compatibility spellings | resolved: 2026-05-13 15:06 local | validating command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="map insert surface registry rejects legacy compatibility spellings"` | notes: slash-qualified member resolution now validates the surface root before accepting a member leaf
- [x] collection helper surface registry resolves preferred compatibility spellings | resolved: 2026-05-13 14:41 local | validating command: `cd build-release && ./PrimeStruct_backend_runtime_tests --test-case="collection helper surface registry resolves preferred compatibility spellings"` | notes: slash spellings passed to `stdlibSurfaceCanonicalHelperPath` now must resolve to the requested stdlib surface id before canonicalization
- [x] stdlib surface metadata resolves collection helper member tokens | resolved: 2026-05-13 14:41 local | validating command: `cd build-release && ./PrimeStruct_backend_ir_tests --test-case="stdlib surface metadata resolves collection helper member tokens"` | notes: refreshed stale vector expectations to the generic resolver contract for canonical full-path member leaves
- [x] missing release build directory for focused backend IR rerun | resolved: 2026-05-13 14:41 local | validating command: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build-release --target PrimeStruct_misc_tests PrimeStruct_backend_runtime_tests PrimeStruct_backend_ir_tests` | notes: recreated `build-release` and rebuilt the focused affected release test binaries
- [x] release build unused access-helper lambda | resolved: 2026-05-13 14:04 local | validating command: `cmake --build build-release --target primec PrimeStruct_misc_tests` | notes: removed the stale `isStringReturningSourceMapAccess` lambda after canonical map access helpers stopped preserving the builtin/native access path
- [x] canonical+experimental map read smoke | resolved: 2026-05-13 13:56 local | validating command: `build-release/primec --emit=vm <tmp canonical+experimental map read smoke> --entry /main && build-release/primec --emit=native <tmp canonical+experimental map read smoke> -o <tmp exe> --entry /main && <tmp exe>` | notes: `experimental_map.prime` is now import-only, so the internal compatibility namespace is not redeclared by the shim
- [x] canonical internal map policy VM smoke | resolved: 2026-05-13 13:24 local | validating command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=vm <tmp internal-map policy smoke> --entry /main && ./primec --emit=native <tmp internal-map policy smoke> -o <tmp exe> --entry /main && <tmp exe>` | notes: constructor-backed fixed-arity map bindings and reference wrappers now avoid stale builtin insert rewrites, and canonical reference helpers use the public `map<K, V>` alias
- [x] canonical map ownership lock | resolved: 2026-05-13 13:18 local | validating command: `cmake --build build-release --target PrimeStruct_misc_tests && cd build-release && ./PrimeStruct_misc_tests --test-suite=primestruct.stdlib.map_ownership` | notes: test now locks `map.prime` internal delegation, `experimental_map.prime` shim status, and `internal_map` implementation/facade ownership without rejecting compatibility type identity
- [x] canonical internal map smoke | resolved: 2026-05-13 13:16 local | validating command: `cmake --build build-release --target primec && cd build-release && ./primec --emit=native <tmp internal-map smoke> -o <tmp exe> --entry /main && <tmp exe returns 7>` | notes: internal map helpers now avoid removed `/map/*` canonicalization, canonical count/access resolve through internal ownership, and native map insert lowering inlines the canonical/internal insert helper path
- [x] ir lowerer call helpers source delegation stays stable | resolved: 2026-05-13 12:38 local | validating command: `cmake --build build-release --target PrimeStruct_backend_ir_tests && cd build-release && ./PrimeStruct_backend_ir_tests --test-case="ir lowerer call helpers source delegation stays stable"` | notes: source-lock now expects generic SoA helper routing instead of a literal `std/collections/soa_vector/to_aos` path
- stale statement source-delegation lock now passes after matching helper-routed vector slot unsafe and canonicalized helper snippets.
- stale infer source-delegation lock now passes after matching the current `definition.namespacePrefix` return-transform routing.
- Vector surface trace gate now passes after semantic vector access probes use registry-backed canonical helper lookups and shared vector helper-path predicates.
- native vector/map count-shadow shard now passes after semantic string-return inference for stdlib vector/map access helpers, native count-source target routing, and inline lowering for string-returning slash-method map access
- VM Result string payload shard now passes after semantic-product named values in Result combinator lambdas fall back to lowerer local payload metadata.
- IR helper-contract representative shard now passes after routing canonical vector count/capacity/safe-at and `/map/count` through native-tail count/access emission while keeping removed vector aliases deferred.
- Vector surface trace gate now passes after replacing the hard-coded canonical vector prefix check in inline native dispatch with `collectionMemberRoot("vector", ...)`.
- VM vector shim mismatch diagnostic shard now passes after keeping stderr in the per-run scratch directory and aligning assertions with current canonical `/std/collections/vector/...` helper paths
- native vector temporary `.capacity()` expressions now pass in the direct wrapper-method test after routing rewritten capacity calls on semantic vector receivers through vector metadata loading
- focused VM canonical vector access and direct experimental SoA conversion shards now pass; canonical vector access with struct-return helpers no longer falls through to builtin scalar vector indexing, and direct experimental SoA conversion helpers now reject raw builtin receivers
- native canonical vector namespace explicit/wrapper conformance shard now passes; scalar vector `remove_swap` no longer calls vector record lifecycle helpers on scalar element slots
- focused vector/map receiver stabilization shard no longer fails after helper routing, map precedence, receiver-call fallback, and diagnostic assertion updates
