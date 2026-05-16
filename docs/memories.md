# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

### canonical-map-count-fallbacks-use-visible-helpers
- Updated: 2026-05-04
- Tags: semantics, collections, diagnostics
- Fact: Visible canonical `/std/collections/map/count` helpers must be routed
  as declared helpers for wrapper-returned map direct and method count calls so
  arity diagnostics outrank omitted-initializer effect-free fallback checks.
- Evidence: The retained `bindings_struct_defaults_21_28` shard failed until
  semantic map-count routing skipped builtin fallback when a visible canonical
  helper exists and treated wrapper-returned map method receivers as canonical
  map count fallback surfaces.

### compile-run-tests-shell-out-to-build-binaries
- Updated: 2026-05-15
- Tags: tests, build, tooling
- Fact: Compile-run suites invoke `./primec` and `./primevm` from the active build directory, so focused reruns need fresh standalone tool binaries as well as the doctest binary.
- Evidence: Stale `build-release/primec` made SoA helper-return compile-run reruns keep failing after the doctest binary was rebuilt; rebuilding `primec` with `PrimeStruct_compile_run_tests` made the same focused cases pass.

### cpp-emitter-wrapper-map-direct-count-diagnostics
- Updated: 2026-04-20
- Tags: tests, emitters, collections
- Fact: In the C++ emitter, wrapper-returned canonical map indexing and non-imported direct reference count flows reject with `unknown call target: /std/collections/map/at`, imported wrapper-reference count falls through to `unknown call target: /map/at`, and direct custom `/std/collections/map/at(wrapMap(), ...)` count calls fail later with `EXE IR lowering error: debug: branch=inferExprString`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_emitters_explicit_vector_mutator_statement_helpers.cpp` plus direct `./primec --emit=exe` reproductions against those wrapper-map fixtures.

### cpp-emitter-vector-mutator-shadow-precedence
- Updated: 2026-04-20
- Tags: tests, emitters, collections
- Fact: In the C++ emitter, imported stdlib vector mutators outrank rooted `/vector/*` shadows for direct and named calls, while positional `push(value, values)` shadow calls still reject during EXE lowering.
- Evidence: `tests/unit/test_compile_run_emitters_lambda_mutator_resolution.cpp` now passes focused release reruns that return `0` for the full mutator matrix, `3` for the named-call case, and `push requires mutable vector binding` for the positional shadow reject.

### exact-stdlib-vector-import-covers-helper-surface
- Updated: 2026-04-19
- Tags: semantics, imports, collections
- Fact: Exact `import /std/collections/vector` is special-cased to expose the bare `vector(...)` alias and to treat `/std/collections/vector/*` helper paths as imported.
- Evidence: `src/semantics/SemanticsValidatorBuildImports.cpp`, `src/semantics/SemanticsValidatorInferCollectionCompatibility.cpp`, and `src/semantics/TemplateMonomorphCoreUtilities.h` all special-case that import path, and the behavior is covered in `tests/unit/test_compile_run_imports_operations.cpp` and `tests/unit/test_semantics_calls_and_flow_collections_bare_map_call_form_statement_args.cpp`.

### fileerror-why-callbacks-preserve-empty-state
- Updated: 2026-04-28
- Tags: ir, native, diagnostics
- Fact: Native `FileError.why` and `Result.why(FileError)` lowering must forward the FileError why emitter as a possibly-empty callback instead of wrapping it in an unconditional lambda.
- Evidence: Known native image compile-run failures aborted with `std::bad_function_call`; `IrLowererLowerEmitExpr.h`, `IrLowererResultWhyHelpers.cpp`, and `IrLowererRuntimeErrorHelpers.cpp` now preserve empty callbacks and report `FileError.why emitter is unavailable` through focused helper tests.

### lower-case-constructors-in-typed-initializers
- Updated: 2026-05-04
- Tags: parser, semantics, structs
- Fact: A single lower-case `Type{...}` value inside an explicitly typed binding initializer must be reinterpreted from binding-shaped parser output to a brace constructor when its name matches the binding type.
- Evidence: The retained `struct_transforms_11_20` shard failed because `[thing] a{thing{}}` reached block validation as a final binding; `Parser::finalizeBindingInitializer` now rewrites that matching shape and focused parser/semantic regressions cover it.

### map-compatibility-aliases-require-source-definitions
- Updated: 2026-05-16
- Tags: semantics, collections, compatibility
- Fact: Removed map aliases such as `/map/count` should count as
  available only when an explicit source definition family exists, not when
  template monomorphization has generated a `__...` specialization or when a
  map receiver method, scalar pointer/memory validation, or string argument
  validation could otherwise mirror to rooted `/map/*` candidates; collection
  access validation must also avoid treating rooted `/map/at*` resolved paths
  as canonical helper paths or recognizing slashless `map/at*_ref` names and
  the `map` namespace prefix as canonical helper names, and collection
  return-kind inference must not grant rooted `/map/*` helpers builtin map
  return kinds; collection-dispatch setup inference follows the same
  canonical-only helper-name rule, and infer-definition deferred alias
  detection must ignore rooted `/map/at*` resolved paths; late map access
  validation follows the same canonical-only helper-name rule; statement
  printability must not treat rooted `/map/at*` calls as builtin map access
  printability shortcuts, collection-access resolution keeps removed-alias
  rejection separate from canonical helper-name classification, and
  struct-return inference should not carry an explicit `map/at` or `/map/at`
  compatibility probe for map access helper return structs; template
  monomorphization should not canonicalize unknown-target diagnostics from
  rooted `/map/*` helper paths to `/std/collections/map/*`, and
  collection-access validation should not retarget rooted `/map/*`
  diagnostic targets to `/std/collections/map/*`; direct call resolution
  should not return a missing-target shortcut for rooted `/map/at*_ref`
  access helper calls, and diagnostic target formatting should not carry a
  rooted `/map/count__t` specialization shortcut; infer-method resolution
  should not mirror `/map/*` receiver paths to `/std/collections/map/*`
  candidates or vice versa, and pointer-like call target candidate generation
  should not append reciprocal `/map/*` and `/std/collections/map/*` helper
  candidates; preferred map method target selection should not return
  explicit `/map/<helper>` definitions or imports ahead of canonical stdlib
  map helpers, and bare map helper rewrite target selection should not fall
  back to visible rooted `/map/<helper>` helper families after canonical
  lookup; initializer inference should not prefer or fall back to rooted
  `/map/<helper>` aliases for explicit stdlib map helper targets, and
  method-target resolution should not prefer rooted `/map/<helper>`
  definitions or imports when choosing map method targets; removed-map
  body-argument target resolution should not fall back from canonical
  `/std/collections/map/<helper>` to rooted `/map/<helper>` definitions, and
  collection-access resolution should not prefer rooted `/map/at*` helper
  definitions when resolving canonical map access helper calls; template
  monomorphization should not treat rooted `/map/count` as equivalent to
  canonical `/std/collections/map/count` for non-templated count diagnostics
  or clear inferred canonical map receiver template arguments through rooted
  `/map/*` paths; collection-dispatch setup should not prefer an explicit
  rooted `/map/<access>` definition over canonical map access inference, and
  method-target resolution should not reclassify a direct call as removed
  `/map/*` compatibility solely from its resolved callee path; template
  expression rewriting should not probe visible rooted `/map/<helper>`
  definitions to report explicit-template diagnostics for bare map receiver
  calls, and late fallback return-kind inference should target canonical
  `/std/collections/map/<helper>` paths for map receivers instead of rooted
  `/map/<helper>` paths; emitter method resolution should not let implicit
  map helper methods resolve through rooted `/map/<helper>` metadata when
  canonical stdlib map helper metadata is absent, and emitter method type
  inference should not fall back from canonical map access metadata to rooted
  `/map/<access>` metadata; emitter call-path preference should not fall back
  from missing canonical `/std/collections/map/<suffix>` paths to rooted
  `/map/<suffix>` aliases, and emitter return inference should not add or
  prefer rooted `/map/<suffix>` aliases when canonical map helper paths are
  missing; emitter collection-type inference should not prune rooted
  `/map/<access>` candidates from canonical map access paths after those
  reverse candidates are no longer generated, and emitter method metadata
  and return inference should follow the same rule; emitter return inference
  should not rewrite missing rooted `/map/<suffix>` paths to canonical
  `/std/collections/map/<suffix>` paths; the shared emitter call-path
  preference should follow the same no-rooted-to-canonical fallback rule, and
  emitter return inference should not add implicit rooted `/map/<method>`
  candidates after canonical map method candidates; emitter method metadata
  should not rewrite rooted `/map/<helper>` method paths to canonical
  `/std/collections/map/<helper>` paths when canonical metadata exists; and
  emitter return-struct inference should not synthesize canonical
  `/std/collections/map/<suffix>` candidates from rooted `/map/<suffix>`
  inputs; unused rooted `/map/<access>` return-struct pruning should stay
  deleted once no call site remains; vector stdlib helper preference should
  not normalize map paths while handling array-to-vector fallback; map method
  sugar should ignore rooted `/map/at` struct-return helpers and use the
  canonical helper return type; emitter collection-type inference should not
  prune canonical map access candidates from rooted `/map/<access>` inputs,
  and the shared emitter method metadata/type-inference helper should not
  prune rooted map access candidates either; emitter method metadata should
  not normalize slashless rooted/canonical map helper paths to leading-slash
  candidates; emitter import-alias resolution should use alias paths directly
  instead of routing through the identity `normalizeMapImportAliasPath` helper;
  emitter method metadata should not add slashless candidates for rooted map
  helper metadata paths; emitter return inference should not normalize
  slashless rooted/canonical map helper paths to leading-slash candidates or
  strip `map/` and `std/collections/map/` prefixes before explicit map helper
  candidate construction; IR call resolution should classify the map helper
  bridge surface through `collections.map_helpers` metadata instead of a
  direct `StdlibSurfaceId::CollectionsMapHelpers` production trace; map
  literal lowering should obtain the empty inferred-map backing path through
  `experimentalCollectionTypePath("map", "Map")` instead of hard-coding the
  experimental map path in the mutation lowerer; struct-type map local
  inference should build the canonical `MapValue` root from
  `collectionTypePath("map")` rather than a hard-coded canonical map path
  literal; packed Result payload metadata should identify experimental map
  payload bases through `isExperimentalCollectionTypeName` instead of a
  hard-coded experimental map path; statement-binding map struct metadata
  should build canonical `MapValue` roots from `collectionTypePath("map")`;
  uninitialized map target inference should also build that `MapValue` root
  from `collectionTypePath("map")`, with unit coverage aimed at the canonical
  `/std/collections/map/map` constructor and stdlib-owned value metadata
  instead of retired `mapPair`/builtin-map compatibility expectations;
  struct-slot map type recognition and specialized `MapValue` path
  construction should also derive that root through
  `collectionTypePath("map")` instead of spelling it directly; map access
  target struct-path inference should likewise derive the root through
  `collectionTypePath("map")`, and access-target tests should assert the
  stdlib-owned `MapValue__*` identity instead of stale experimental `Map__*`
  expectations; statement binding helper map struct-path inference should
  also derive `MapValue` roots through `collectionTypePath("map")`; semantics
  binding map type recognition should derive canonical map and `MapValue`
  roots through a local collection path helper; semantics struct-return helper
  path normalization and specialized `MapValue` return-path construction
  should use that same local path-helper pattern; setup-type map struct
  classification, Result map identity, and statement-return collection
  normalization should derive canonical `MapValue` roots through collection
  path helpers instead of split-string map roots; lowerer fallback setup and
  struct-layout generated map classifiers should derive experimental map
  backing paths through shared collection helpers; uninitialized-struct
  specialized map detection should use the same helper for generated
  experimental map backing paths; initializer inference should derive
  generated experimental map backing recognition through
  `isExperimentalCollectionBackingTypeName`, including graph binding
  initializer inference; statement return collection normalization should use
  the same helper for generated map backing recognition; collection-type
  normalization should use the same helper for generated map backing
  recognition; struct-return inference should use the same helper for
  generated map backing recognition; Result helper map payload recognition
  should use the same helper for generated map backing paths; receiver-path
  map backing exclusions should use the same helper; late map-access receiver
  classification should use the same helper; statement init map type matching
  should use the same helper, and argument-validation extraction should use
  the same helper for generated map backing structs; collection-dispatch map
  field extraction should also use the same helper for unspecialized and
  generated experimental map backing types; statement printability should use
  the same helper when excluding unspecialized experimental map backing
  returns; collection return inference should use the same helper for
  unspecialized and generated experimental map backing return paths;
  semantic binding type helpers should classify experimental map backing names
  through generic experimental collection helpers instead of direct map
  backing paths;
  semantic argument validation should use the shared helper for experimental
  map backing template-base compatibility checks;
  semantic method-target resolution should use the shared helper for
  experimental map backing receiver and generated fallback checks;
  template-monomorph collection receiver normalization should use the shared
  helper for experimental map backing paths while constructing bare generated
  backing prefixes from the backing name;
  template-monomorph fallback type inference should use the shared helper for
  unspecialized and generated experimental map value types;
  borrowed experimental map extraction in semantic validation should use the
  shared helper instead of direct experimental map backing prefixes;
  semantic publication collection specialization should use the shared helper
  for unspecialized experimental map backing types;
  collection-access validation should use the shared helper for
  unspecialized experimental map type text;
  pre-dispatch direct-call validation should use the same helper for
  unspecialized experimental map type text;
  build-parameter experimental map value detection should use the shared
  helper for unspecialized and generated experimental map value types;
  template-monomorph experimental collection receiver resolution should use
  shared helpers for unspecialized and generated map receiver types;
  late map-access validation should use the shared helper for unspecialized
  experimental map type text;
  declared collection map type normalization should derive unspecialized and
  generated experimental map backing bases through
  `experimentalCollectionTypePath`;
  IR setup method-target receiver classification should derive the
  experimental map receiver path through `experimentalCollectionTypePath`;
  IR inference dispatch map-family detection should derive unspecialized and
  generated experimental map backing bases through
  `experimentalCollectionTypePath`;
  statement-binding metadata should derive generated experimental map backing
  prefixes through `experimentalCollectionTypePath`;
  struct-field binding helper classification should use shared experimental
  collection classifiers for generated map backing names;
  inline packed-args map detection should derive unspecialized and generated
  experimental map backing bases through `experimentalCollectionTypePath`;
  result metadata map backing checks should derive unspecialized and generated
  experimental map backing paths through `experimentalCollectionTypePath`;
  call-helper generated collection struct classification should derive the
  generated experimental map backing prefix through
  `experimentalCollectionTypePath`;
  statement-binding helper generated map backing checks should derive the
  generated experimental map backing prefix through
  `experimentalCollectionTypePath`;
  inline call context generated map struct and experimental map
  constructor-helper prefixes should derive backing paths through local
  collection path builders;
  emitter builtin call-path map type classification should derive the
  experimental map type path through the local experimental collection path
  builder;
  setup-type collection helper generated map struct path inference should
  derive the experimental map backing prefix through
  `experimentalCollectionTypePath`;
  inference base-kind map-family detection should derive unspecialized and
  generated experimental map backing bases through
  `experimentalCollectionTypePath`;
  inline parameter map-like struct detection should derive experimental map
  backing paths through `experimentalCollectionTypePath`;
  setup-type method-call synthetic collection fallback blocking should derive
  the experimental map member root through `experimentalCollectionMemberRoot`;
  struct-slot map type recognition should derive rooted and slashless
  experimental map backing paths through `experimentalCollectionTypePath`;
  binding type normalization should derive rooted and slashless experimental
  map backing paths through `experimentalCollectionTypePath`; access target
  map struct inference should derive rooted and slashless experimental map
  backing paths through `experimentalCollectionTypePath`; statement-call map
  insert rewriting should derive rooted, slashless, and generated
  experimental map backing paths through its collection path helper; emitter
  return-kind type parsing should classify experimental map backing bases and
  generated prefixes through a local storage-base helper instead of direct
  path text; collection expression lowering should derive experimental map
  backing member roots and generated `Map` prefixes through local collection
  path helpers; inline call map-kind inference and experimental map receiver
  classification should derive rooted, slashless, and generated experimental
  map backing paths through local collection path helpers;
  statement-expression map count/access target classification should reuse
  local experimental collection type and generated-specialization helpers
  instead of direct experimental map backing path text;
  try-expression specialized map Result payload inference should derive the
  generated experimental map backing prefix through the local collection type
  helper; IR-printer return-kind type parsing should classify experimental
  map backing bases and generated prefixes through the existing generic
  experimental collection type helper; semantic map constructor and
  entry-argument path checks should route experimental map backing member
  paths through shared constructor helpers instead of direct path text;
  semantic collection compatibility helper selection should use shared
  constructor root/path helpers for experimental map helper backing paths;
  semantic and template-monomorph generated map backing detection should rely
  on the shared experimental collection backing classifier instead of spelling
  the generated `Map` prefix;
  semantic collection-helper rewrite and compatibility internals should use
  the shared constructor root helper for experimental map helper backing
  paths;
  template-monomorph expression rewrite, receiver-resolution, and semantic
  builtin path helpers should use shared experimental map root/path helpers;
  semantic `try` builtin validation should obtain canonical map tryAt helper
  paths from stdlib surface metadata instead of direct path text;
  semantic infer pre-dispatch map helper visibility checks should resolve
  canonical helper names through stdlib surface metadata instead of direct
  path text;
  semantic method-resolution map helper return and missing-definition checks
  should resolve canonical helper names through stdlib surface metadata
  instead of direct path text;
  semantic collection-dispatch return-kind checks should resolve canonical
  map helper names through stdlib surface metadata instead of direct path
  text;
  semantic infer collection-dispatch setup should resolve canonical map
  access helper names and namespaces through stdlib surface metadata instead
  of direct path text;
  semantic expression collection-dispatch setup should resolve canonical map
  access helper names through stdlib surface metadata instead of direct path
  text;
  semantic effect-free collection helper routing should build canonical map
  helper paths and slashless prefixes from stdlib surface metadata instead
  of direct path text;
  semantic collection-access validation should resolve canonical map access
  helper names, namespace checks, and diagnostic paths through stdlib surface
  metadata instead of direct path text;
  late map access builtin validation should resolve canonical map helper
  names, namespace checks, and unknown-target diagnostics through stdlib
  surface metadata instead of direct path text;
  generated collection struct classification should derive the canonical map
  `MapValue__*` prefix through `collectionTypePath("map")` instead of
  carrying a split-string map root.
- Evidence: Field-bound `Map<K, V>` compatibility triage showed generated
  map helper specializations could mask missing `/map/count` aliases unless
  removed-alias checks ignored generated-only definition paths; later
  rooted-method cleanup showed `values./map/count()` also needs early
  semantic and inference rejection so pre-dispatch cannot borrow
  `/std/collections/map/count`; semantic struct-return method inference now
  probes only canonical `/std/collections/map/*` candidates for map receivers,
  and scalar pointer/memory builtin validation no longer carries a direct
  rooted-or-canonical map access helper path classifier; string argument
  validation no longer treats rooted `/map/at*` resolved paths or slashless
  `map/at*_ref` names as explicit map access helper classifiers; collection
  access validation no longer includes rooted `/map/at*` in its canonical
  resolved-path helper table and no longer recognizes slashless `map/at*_ref`
  names or a `map` namespace prefix as canonical map access helper names;
  collection return-kind inference now only handles canonical
  `/std/collections/map/*` helper paths, and collection-dispatch setup
  inference no longer treats rooted `/map/at*_ref` resolved paths or a `map`
  namespace prefix as canonical map access helper-name classifiers;
  infer-definition deferred map alias detection no longer treats rooted
  `/map/at*` resolved paths as map access helpers, and late map access
  validation no longer treats slashless `map/at*_ref` names or a `map`
  namespace prefix as canonical map access helper names; statement
  printability no longer treats rooted `/map/at` or `/map/at_unsafe` calls
  as builtin map access printability shortcuts; collection-access resolution
  no longer treats rooted `/map/at*_ref`, slashless `map/at*`, or a `map`
  namespace prefix as canonical map access helper-name classifiers;
  struct-return inference no longer probes explicit `map/at` or `/map/at`
  compatibility calls for map access helper return structs; template
  monomorphization no longer rewrites rooted `/map/{count,contains,tryAt,at,
  at_unsafe,insert}` unknown-target diagnostics to
  `/std/collections/map/*`; collection-access validation no longer retargets
  rooted `/map/*` diagnostic targets to `/std/collections/map/*`; direct
  call resolution no longer returns a missing-target shortcut for rooted
  `/map/at*_ref` access helper calls; diagnostic target formatting no
  longer carries a rooted `/map/count__t` specialization shortcut;
  infer-method resolution no longer mirrors `/map/*` receiver paths to
  `/std/collections/map/*` candidates or vice versa; pointer-like call target
  candidate generation no longer appends reciprocal `/map/*` and
  `/std/collections/map/*` helper candidates; preferred map method target
  selection no longer returns explicit `/map/<helper>` definitions or imports
  ahead of canonical stdlib map helpers; bare map helper rewrite target
  selection no longer falls back to visible rooted `/map/<helper>` helper
  families after canonical lookup; initializer inference no longer prefers or
  falls back to rooted `/map/<helper>` aliases for explicit stdlib map helper
  targets; method-target resolution no longer prefers rooted
  `/map/<helper>` definitions or imports when choosing map method targets;
  removed-map body-argument target resolution no longer falls back from
  canonical `/std/collections/map/<helper>` to rooted `/map/<helper>`
  definitions; collection-access resolution no longer prefers rooted
  `/map/at*` helper definitions when resolving canonical map access helper
  calls; template monomorphization now only uses the canonical count helper
  path for the non-templated count diagnostic and no longer includes rooted
  `/map/*` in the inferred canonical map receiver template-argument cleanup;
  collection-dispatch setup no longer suppresses namespaced canonical map
  access inference just because a rooted `/map/<access>` definition exists;
  method-target resolution no longer has a resolved-callee `/map/*` table for
  direct map helper compatibility detection; template expression rewriting no
  longer probes rooted `/map/<helper>` source definitions for explicit
  template-argument diagnostics on bare map receiver calls; late fallback
  return-kind inference now routes map receiver builtin access spelling to
  canonical `/std/collections/map/<helper>` paths instead of rooted
  `/map/<helper>` paths; emitter method resolution now requires canonical
  `/std/collections/map/<helper>` metadata for implicit map helper methods
  instead of accepting rooted `/map/<helper>` metadata as the only match;
  emitter method type inference no longer falls back from canonical
  `/std/collections/map/<access>` metadata to rooted `/map/<access>`
  metadata for bare map access methods; emitter call-path preference no
  longer falls back from missing canonical `/std/collections/map/<suffix>`
  paths to rooted `/map/<suffix>` aliases; emitter return inference no longer
  adds or prefers rooted `/map/<suffix>` aliases when canonical
  `/std/collections/map/<suffix>` paths are missing; emitter collection-type
  inference no longer prunes rooted `/map/<access>` candidates from canonical
  map access paths after the reverse candidates were removed; emitter method
  metadata no longer prunes rooted `/map/<access>` candidates from canonical
  map access paths either; emitter return inference follows the same rule and
  no longer rewrites missing rooted `/map/<suffix>` paths to canonical
  `/std/collections/map/<suffix>` paths, and the shared emitter call-path
  preference follows the same rule; emitter return inference no longer adds
  an implicit rooted `/map/<method>` candidate after canonical map method
  candidates; emitter method metadata no longer has a shared
  rooted-to-canonical map method helper preference; emitter return-struct
  inference no longer synthesizes canonical
  `/std/collections/map/<suffix>` candidates from rooted `/map/<suffix>`
  inputs; its unused rooted `/map/<access>` pruning lambda is also gone; and
  vector stdlib helper preference no longer normalizes map paths. The stale
  rooted-alias struct-return semantic fixture now expects `values.at()` to use
  the canonical map helper return type instead of a user-defined `/map/at`
  return type, which allowed the emitter collection-type rooted map access
  pruning branch to be removed. The same obsolete pruning helper was then
  removed from shared emitter method metadata/type inference and setup-return
  inference. Emitter method metadata no longer normalizes slashless
  rooted/canonical map helper paths to leading-slash candidates, and the
  identity `normalizeMapImportAliasPath` hook has been removed from emitter
  control call and method-resolution paths. Emitter method metadata also no
  longer adds slashless candidates for rooted map helper metadata paths, and
  emitter return inference no longer normalizes slashless rooted/canonical
  map helper paths to leading-slash candidates or strips map helper prefixes
  before explicit candidate construction.

### map-constructor-normalization-uses-public-path
- Updated: 2026-05-16
- Tags: ir, collections, compatibility
- Fact: Temporary lowerer normalization for inline map parameters,
  variadic map arguments, and packed `Result.ok` map payloads must target the
  public `/std/collections/map/map` constructor, not the retired rooted
  `/map/map` builtin spelling.
- Evidence: Replacing those three `rewrittenExpr.name = "/map/map"` sites
  with `collectionMemberPath("map", "map")` kept focused public-constructor
  semantics and VM smoke tests passing while the map ownership source lock now
  rejects the retired rooted rewrite.

### mapvalue-public-insert-uses-stdlib
- Updated: 2026-05-16
- Tags: ir, collections, native
- Fact: Public `/std/collections/map/insert*` must inline the stdlib-owned
  `MapValue` helpers instead of using generated native flat-map insert
  lowering; non-local `MapValue` field receivers are compile-reject fixtures
  until a non-bridge field path exists.
- Evidence: Canonical local map insert/overwrite/growth VM/native/C++ emitter
  conformance hung or hit invalid indirect addresses until public insert was
  removed from `isGeneratedMapInsertHelper`, while field receiver cases still
  fail with `struct field type mismatch` without the retired map bridge.

### native-map-values-must-be-scalars
- Updated: 2026-05-15
- Tags: native, collections, lowering
- Fact: Native map lowering only supports numeric, bool, and string value kinds, so map specializations with unknown semantic-product value kinds must reject before lowering.
- Evidence: The ownership-sensitive experimental map value fixture compiled into a native crash until entry setup validated semantic-product map value kinds and rejected unsupported values before map lowering.

### maybe-method-helpers-use-rooted-families
- Updated: 2026-05-01
- Tags: semantics, stdlib, sums
- Fact: Imported stdlib `Maybe<T>` method calls resolve through rooted helper families such as `/Maybe/is_empty` and `/Maybe/isSome` even when the receiver type is monomorphized under `/std/maybe/Maybe__...`.
- Evidence: The saved release log rejected `empty.is_empty()` and `value.isSome()` with unknown call targets until semantic validation and template monomorphization fell back from missing same-path helpers to the rooted `Maybe` helper family.

### native-vector-auto-inference-expression-blockers
- Updated: 2026-04-20
- Tags: tests, native, collections
- Fact: In the native compile-run path, expression-form named `push` receiver precedence cases now fail in native lowering with the generic “calls in expressions” diagnostic, while the std namespaced auto-inference/count/capacity alias-precedence rejects still fail semantically but now say `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_native_backend_collections_auto_inferred_helper_precedence.cpp` plus direct `./primec --emit=native` reproductions against the same fixtures.

### namespaced-indexed-writes-use-access-aliases
- Updated: 2026-04-28
- Tags: semantics, ir, collections
- Fact: Namespaced indexed writes can be validated or lowered after access calls have been canonicalized to stdlib legacy helper paths such as `/std/collections/vectorAt__...`, so semantic, IR-lowerer, and emitter array-access classifiers must all recognize those aliases.
- Evidence: `./primec --emit=ir` on a `/std/image` helper with `values[0i32] = 9i32` reproduced the original semantic rejection until `getBuiltinArrayAccessName` in semantics accepted `vectorAt` aliases; the follow-on IR lowering repro required the matching alias support in `IrLowererBuiltinNameHelpers.cpp` and `EmitterBuiltinCallPathHelpers.cpp`.

### result-ok-arithmetic-payloads-are-syntax-owned
- Updated: 2026-05-01
- Tags: ir, semantics, results
- Fact: Direct `Result.ok(...)` arithmetic payload calls such as `multiply(...)` are syntax-owned builtin payloads and must not require semantic-product query metadata before normal expression-kind inference can classify them.
- Evidence: The saved release log showed `missing semantic-product Result.ok payload metadata: multiply` and unsupported auto-bound `Result.and_then` payloads; `IrLowererPackedResultHelpers.cpp` and `IrLowererResultMetadataHelpers.cpp` now exempt builtin operators from semantic query requirements alongside builtin comparisons.

### return-info-precompute-requires-callable-map
- Updated: 2026-05-06
- Tags: ir, semantics, return-info
- Fact: Semantic-product return-info precomputation enumerates callable definitions only through `callableSummaryIndicesByPathId`, so tests that manually create callable summaries must seed that published map.
- Evidence: `precomputeSemanticProductReturnInfoCache(...)` now fails closed for raw callable summaries without the map, and inference get-return-info setup tests pin both mapped and raw-only paths.

### retired-public-map-constructors-not-canonical
- Updated: 2026-05-16
- Tags: semantics, ir, collections
- Fact: Rooted retired public map constructor aliases such as
  `/std/collections/mapSingle`, `/std/collections/mapPair`, and
  `/map/entry` must not be treated as canonical map constructors; unqualified
  names remain separate legacy/internal compatibility surfaces, and
  metadata-backed constructor rewrites must not synthesize fixed-arity
  replacements for them or keep no-op map rewrite shims alive.
- Evidence: `isBuiltinCanonicalMapConstructorExpr(...)` and
  `isMapConstructorDirectTargetPath(...)` now only accept the public rooted
  `/std/collections/map/map` constructor path, while `MapConstructorHelpers.h`
  no longer carries the fixed-arity map constructor rewrite table or the
  metadata-backed map constructor rewrite functions; semantic
  constructor-backed binding and parameter map-value checks no longer
  classify fixed-arity `mapSingle`/`mapPair`-style names as canonical
  constructors; lowerer assignment/init/packed-Result paths no longer rewrite
  public map constructors to experimental-map fixed-arity helpers; semantic
  call resolution and template-monomorph overload selection no longer
  classify rooted `/map/entry` spellings as map entry constructors.

### semantic-memory-policy-uses-runner-headroom
- Updated: 2026-05-14
- Tags: benchmarks, ci, memory
- Fact: Release semantic-memory RSS budgets use runner-observed soft caps plus about five percent hard-cap headroom for noisy fixture/phase pairs.
- Evidence: Stabilizing the release gate required refreshing imported-math,
  vector, and `math_vector_matrix:ast-semantic` caps in
  `benchmarks/semantic_memory_budget_policy.json` plus
  `docs/semantic_memory_benchmark_policy.md`, after which the focused
  semantic-memory gate and full `./scripts/compile.sh --release` passed.

### semantic-product-allocation-bytes-are-net
- Updated: 2026-05-14
- Tags: semantics, tests, memory
- Fact: Semantic allocation `allocatedBytes` counters are sampled net byte
  deltas and can be zero for semantic-product builds even when allocation
  counts and produced facts prove the phase executed.
- Evidence: The backend registry release shard observed
  `semanticProductBuild.allocationCount > 0`, produced semantic facts, and
  `semanticProductBuild.allocatedBytes == 0`; the registry test now checks the
  allocation count instead of requiring positive net bytes.

### semantic-product-index-map-authority
- Updated: 2026-05-06
- Tags: semantics, ir, tests
- Fact: `buildSemanticProductIndex(...)` populates query, try, binding, `on_error`, and return semantic-id lookups only from `queryFactIndicesByExpr`, `tryFactIndicesByExpr`, `bindingFactIndicesByExpr`, `onErrorFactIndicesByDefinitionId`, and `returnFactIndicesByDefinitionId`; entry-argument setup also enumerates only published `bindingFactIndicesByExpr` entries, and result metadata completeness validation enumerates return facts only through `returnFactIndicesByDefinitionId`. Tests that manually create those facts must seed the published maps before expecting lowerer lookup or validation to succeed.
- Evidence: Raw query/try/binding/on_error/return fact backfills were removed from `SemanticProductIndexBuilder::buildQueryIndex(...)`, `buildTryIndex(...)`, `buildBindingIndex(...)`, `buildOnErrorIndex(...)`, and `buildReturnIndex(...)`; `resolveEntryArgsParameterFromSemanticProduct(...)` no longer calls `semanticProgramBindingFactView(...)`; `validateSemanticProductResultMetadataCompleteness(...)` no longer calls `semanticProgramReturnFactView(...)`; call-helper, count-access, result-completeness, and inference setup tests pin both raw-only failure and mapped success paths.

### semantic-product-pick-target-query-facts
- Updated: 2026-05-01
- Tags: semantics, ir, sums
- Fact: Sum `pick(...)` targets that are direct or method calls must publish semantic-product query facts because native/VM pick lowering resolves the target sum type from those facts.
- Evidence: The saved release log failed `native pick call target sum resolution uses query facts` and its method-target companion; `SemanticsValidatorSnapshotLocals.cpp` now adds a deduped recovery pass for `pick` target calls, with regression coverage in `test_semantics_type_resolution_graph_snapshots.cpp`.

### semantic-product-routing-completeness-gates-lowering
- Updated: 2026-05-06
- Tags: semantics, ir, routing
- Fact: IR lowering entry setup and routing coverage validators treat
  semantic-product direct-call, bridge-path, and method-call routing facts as
  published-map required gates; helper tests that manually build call targets
  must seed the published routing lookup maps because raw routing facts are
  not lookup fallbacks.
- Evidence: The saved release log showed `IrLowerer::lower` accepting missing
  direct-call and bridge-path facts; `IrLowererLowerEntrySetup.cpp` now includes
  routing.direct-call, routing.bridge-path, and routing.method-call in the
  semantic-product completeness manifest, and `published target lookups ignore
  raw routing facts without maps` plus `semantic-product coverage validators
  ignore raw routing facts without maps` lock the no-raw-routing-fallback
  contract.

### variadic-pointer-struct-metadata-keeps-wrapper
- Updated: 2026-05-04
- Tags: ir, metadata, variadic
- Fact: Variadic `args<Reference<T>>` and `args<Pointer<T>>` parameter metadata
  must preserve the wrapper transform when probing struct pointee metadata, even
  though the resulting args-pack element kind is pointer-like.
- Evidence: The retained `PrimeStruct_primestruct_ir_pipeline_validation_cases_851_860`
  shard failed because `args<Reference<Pair>>` and `args<Pointer<Pair>>` lost
  `/pkg/Pair` as `LocalInfo::structTypeName`; preserving the wrapper transform
  lets the existing struct binding callbacks resolve the pointee.

### semantic-product-sums-omit-callable-summaries
- Updated: 2026-05-01
- Tags: semantics, ir, sums
- Fact: Semantic-product sum definitions publish type metadata but not callable summaries, so lowerer setup code must treat sums as type metadata instead of requiring callable-summary facts.
- Evidence: The saved release log showed `missing semantic-product callable summary: /Choice`; `SemanticsValidatorSnapshots.cpp` skips sum definitions when collecting callable summaries, and `IrLowererOnErrorHelpers.cpp`, `IrLowererLowerEffects.cpp`, and `IrLowererLowerInferenceReturnInfoHelpers.cpp` now skip or short-circuit semantic-product sum metadata before requiring callable summaries.

### skipped-doctest-queue-locks-current-docs-and-source-surfaces
- Updated: 2026-04-20
- Tags: tests, docs, todo
- Fact: `tests/unit/test_compile_run_examples_docs_locks.cpp` source-locks both the skipped-debt queue shape in `docs/todo*.md` and the current active `vm_math` and `vm_maps` surfaces, so resolving skipped-doctest lanes requires updating the queue docs and that lock file together.
- Evidence: Refreshing the `examples_docs_locks` shard after landing the VM math/map skip cleanups required retiring `TODO-4110`, `TODO-4117`, and `TODO-4118` from `docs/todo.md`, archiving them in `docs/todo_finished.md`, and updating the queue/surface assertions in `tests/unit/test_compile_run_examples_docs_locks.cpp` before the focused release rerun passed.

### soa-field-view-borrows-use-reference-root
- Updated: 2026-05-14
- Tags: semantics, collections, soa
- Fact: SoA field-view borrow liveness must honor `BindingInfo::referenceRoot`
  for recognized `SoaFieldView` spellings and `[auto]` bindings that recorded a
  field-view root, while keeping pointer aliases on their pointer-specific
  paths.
- Evidence: TODO-4511 canonical `soa<T>` field-view invalidation passed only
  after semantic borrow helpers recognized specialized `SoaFieldView__*`
  paths, active-borrow checks honored recorded auto field-view roots, and
  ordinary pointer-root borrows remained excluded from reference conflict
  scans.

### soa-public-wrapper-lowering-gaps
- Updated: 2026-05-14
- Tags: semantics, ir, collections, soa
- Fact: Public `/std/collections/soa/soa`, `single`, `reserve`, `push`,
  `from_aos`, and literal-indexed direct `field_view<Struct, Field>(...)[i]`
  wrappers lower on VM/native. Canonical `soa<T>.field()[i]` rewrites through
  the public `soa/get(...).field` path for public `SoaVector<T>` locals
  inferred from wrapper initializers.
- Evidence: TODO-4517 added focused native and VM compile-run coverage for
  direct public field-view wrapper indexing and canonical field syntax after
  the AST rewrite stopped materializing temporary `SoaFieldView` carriers in
  `/main`.

### soa-vector-struct-access-needs-paths
- Updated: 2026-05-15
- Tags: ir, collections, soa
- Fact: Native array/vector access semantic facts for `vector<Struct>` must carry specialized vector struct paths so stale local vector fallback blocking does not reject public SoA `from_aos` wrappers.
- Evidence: Public SoA `from_aos` and field-view wrapper compile-run shards failed until `IrLowererAccessTargetResolution` recovered specialized vector struct paths for unknown element-kind vectors.

### soa-storage-temporaries-own-nested-buffers
- Updated: 2026-04-28
- Tags: ir, native, collections
- Fact: `SoaColumn`, generated `SoaColumnsN`, and `SoaVector` temporary copies need native disarm logic for nested `ownsData` fields so copied heap buffers are not freed through the source temporary.
- Evidence: `stdlib/std/collections/internal_soa_storage.prime` stores `ownsData` inside each `SoaColumn`, and `IrLowererFlowControlHelpers.cpp` now disarms `SoaColumn`, `SoaColumnsN`, and `SoaVector` temporary copies at their computed `ownsData` slot offsets.

### soa-vector-metadata-offsets
- Updated: 2026-05-04
- Tags: ir, collections, soa
- Fact: IR lowering treats `SoaVector` metadata as nested storage where count, capacity, data pointer, and storage-owner flag live at slot offsets 2, 3, 4, and 5 respectively.
- Evidence: Stabilizing `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_borrowed_vectors` required SoA count/push/reserve lowering to use those offsets while leaving variadic args-pack counts on the pack header.

### stdlib-vector-capacity-direct-calls-keep-helper
- Updated: 2026-05-01
- Tags: semantics, collections, emitters
- Fact: Explicit `/std/collections/vector/capacity(...)` direct calls with a declared same-path helper must remain non-builtin semantic targets so C++ emission calls the helper instead of falling through to `ps_vector_capacity`.
- Evidence: The saved release log showed a same-path helper returning `15` still producing runtime capacity `3`; `SemanticsValidatorExprCollectionCountCapacity.cpp` now keeps declared canonical capacity helpers as non-builtin targets, with focused semantics coverage beside the stdlib vector count cases.

### struct-brace-constructors-recover-callees-by-type
- Updated: 2026-05-01
- Tags: ir, structs, lowering
- Fact: Statement binding lowering can infer a brace constructor's struct path even when direct call resolution misses the constructor, so it must recover the struct definition by inferred path before falling back to generic struct copy lowering.
- Evidence: The saved release log failed `ir lowers struct brace constructor binding`; `IrLowererLowerStatementsBindings.h` now looks up inferred struct constructor paths in `defMap` and inline-lowers them, and the serialization test executes the lowered module to confirm the named-field default path returns `3`.

### vm-vector-shadow-precedence-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, imported stdlib vector `push` still wins over rooted `/vector/push` shadows for named-call and method syntax, but `push(...)` expression forms reject during VM lowering, auto-inferred named access helpers fail with `missing semantic-product local-auto fact`, and auto-inferred std namespaced access/push alias cases fail earlier with `return type mismatch: expected i32`.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_shadow_access.cpp` plus direct `./primec --emit=vm` reproductions against the same fixtures.

### vm-canonical-map-helper-failure-modes
- Updated: 2026-04-25
- Tags: tests, vm, collections
- Fact: In the VM path, explicit canonical map helper overrides can still reach runtime and fail with `VM error: unaligned indirect address in IR`, while imported canonical map-reference helper calls fail earlier in VM lowering with `call=/std/collections/map/at`.
- Evidence: Focused release reruns of `PrimeStruct_primestruct_compile_run_vm_collections_alias_and_basics_51_60` plus direct `./primec --emit=vm` reproductions of `vm_direct_canonical_map_helper_same_path_precedence.prime` and `vm_stdlib_map_reference_helpers.prime`.

### vm-map-helper-calls-currently-reject-without-canonical-support
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: The VM compile-run path currently rejects map indexing and direct `at`/`at_unsafe` helper use for numeric, bool, and `u64` map keys with `unknown call target: /std/collections/map/at` rather than executing those helper forms.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_maps.cpp` now lock those rejects in `tests/unit/test_compile_run_vm_maps.cpp`.

### vm-math-support-matrix-lowering-split
- Updated: 2026-04-20
- Tags: tests, vm, math
- Fact: In the VM compile-run math shard, nominal support-matrix helpers execute successfully, but quaternion-reference and matrix-composition reference flows still fail in VM lowering through `/std/math/quat_multiply_internal` and `/std/math/mat3_mul_vec3_internal`, while the broad matrix and quaternion arithmetic tolerance tests intentionally accept either success or the current unsupported-call fallback.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_math.cpp --no-skip --duration=true` plus direct `./primec --emit=vm` reproductions against the rewritten fixtures.

### vm-vector-limits-growth-and-expression-blockers
- Updated: 2026-04-20
- Tags: tests, vm, collections
- Fact: In the VM compile-run path, reserve/push growth helpers and the exact 256-element local vector literal now execute successfully, but expression-form `reserve`, `remove_at`, and `remove_swap` user shadows still reject with the VM “calls in expressions” lowering diagnostic.
- Evidence: Focused release reruns of `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_vm_collections_vector_limits_a.cpp` plus direct `./primec --emit=vm` reproductions against those fixtures.

### source-lock-tests-read-private-semantics-fragments-directly
- Updated: 2026-04-27
- Tags: tests, semantics, source-lock
- Fact: `docs/source_lock_inventory.md` is the canonical inventory for source-lock tests that read private `src/` files, private headers, public headers as architecture proxies, or checked-in docs/examples, including the existing private semantics fragment locks.
- Evidence: `tests/unit/test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h` explicitly loads `SemanticsValidatorPrivateExprValidation.h` and many sibling fragments with `readText(...)`; `docs/source_lock_inventory.md` classifies the wider source-lock surface and records intended replacement contracts.

### sum-variant-envelopes-are-line-sensitive
- Updated: 2026-04-29
- Tags: parser, text-filter, sums
- Fact: A bracketed sum payload envelope on the line after a bare unit variant must stay a separate variant declaration instead of being rewritten or parsed as postfix indexing on the previous name.
- Evidence: Release triage showed `Maybe<T> { none\n  [T] some }` becoming `at(none, T)` before `TextFilterPipelineOperatorRewrite` and `ParserExpr` learned to stop cross-line bracket chaining.

### text-filter-collect-diagnostics-locks-stdlib-fallback-messages
- Updated: 2026-04-20
- Tags: tests, diagnostics, text-filter
- Fact: The wrapper-temporary collect-diagnostics coverage in `tests/unit/test_compile_run_text_filters_diagnostics_c.cpp` currently locks the stdlib fallback messages `unknown call target: /std/collections/map/at` and `unknown method: /vector/capacity` rather than the older user-helper arg-mismatch wording.
- Evidence: Release reruns from `build-release/PrimeStruct_compile_run_tests --source-file=*test_compile_run_text_filters_diagnostics_c.cpp` plus direct `./primec --emit-diagnostics --collect-diagnostics` reproductions against the same sources emitted those messages.

### type-pack-declarations-bind-specialization-metadata
- Updated: 2026-05-16
- Tags: parser, semantics, generics, reflection
- Fact: Heterogeneous type-pack declarations record final `Ts...` metadata,
  monomorphized specializations bind trailing type arguments into
  deterministic pack metadata, and concrete specialization rewrites expand
  struct fields, helper parameters, helper locals, nested type/call template
  arguments, and generated reflection helpers from that pack metadata.
- Evidence: `Parser::parseTemplateParameterList` records `templateArgIsPack`,
  `bindTemplateArguments` captures trailing pack arguments in
  `TemplatePackBinding`, semantic-product formatting publishes
  `template_pack_bindings`, `expandTypePackBindingList` materializes concrete
  bindings during template monomorphization, and
  `rewriteReflectionGeneratedHelpersForPackSpecializations` regenerates
  reflection helpers from expanded fields.

### variadic-borrowed-pointer-packs-are-supported
- Updated: 2026-05-01
- Tags: tests, ir, native, variadic
- Fact: Direct struct and Result packs plus borrowed and pointer variadic arg-pack access for scalar, array, struct, nested struct-field, uninitialized, Result, and FileError surfaces are supported materialized behavior, so regression tests should assert direct value results instead of older rejection diagnostics or raw-pointer-shaped numeric results.
- Evidence: Saved release logs showed current IR/native lowering accepting these pack forms and returning direct sums such as `2`, `3`, `15`, `17`, `23`, `24`, `27`, `36`, `39`, `65`, `72`, and `75` while older tests still expected `variadic parameter type mismatch`, `struct field type mismatch`, `unknown struct field: value`, or stale large numeric constants.

### variadic-pointer-map-lookups-are-supported
- Updated: 2026-05-01
- Tags: tests, ir, native, variadic, collections
- Fact: Pointer map variadic pack elements support direct `contains` and `at`/`at_unsafe` lookup helpers, so regression tests should assert the materialized lookup result instead of native-call rejection diagnostics.
- Evidence: Saved release logs showed the IR lowerer and native backend accepting the direct pointer-map lookup pack case that returns `48` while older tests still expected a `/std/collections/map/at_unsafe` native-call rejection.

### vector-args-pack-elements-are-handles
- Updated: 2026-04-28
- Tags: ir, collections, variadic
- Fact: `args<vector<T>>` pack elements are single-slot vector handles, not inline vector struct payloads, so indexed access and binding initialization must not classify them as inline struct args-pack storage.
- Evidence: `PrimeStruct_primestruct_ir_pipeline_conversions_variadic_collection_refs` failed until `IrLowererAccessTargetResolution`, `IrLowererIndexedAccessEmit`, and statement binding initialization all preserved vector args-pack handle semantics.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
