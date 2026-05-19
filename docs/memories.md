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

### emitter-key-value-helper-expr-resolution
- Updated: 2026-05-19
- Tags: emitter, collections, stdlib
- Fact: Emitter expression member-name resolution for metadata-backed
  map-helper surfaces should use key/value local helper names, including
  shared helper-name, helper-path, count-helper, access-helper, and removed
  helper compatibility utilities.
- Evidence: `EmitterBuiltinCallPathHelpers.cpp` now resolves canonical and
  published helper expression members through
  `resolveCanonicalKeyValueHelperExprMemberName` and
  `resolvePublishedKeyValueHelperExprMemberName`; `EmitterHelpers.h` exposes
  `isCanonicalKeyValueHelperName`, `keyValueHelperNameFromPath`, and
  `isCanonicalKeyValueHelperPath`, and its access-helper predicates are named
  `isCanonicalKeyValueAccessHelperName` and
  `isCanonicalKeyValueAccessHelperPath`; count-helper checks use
  `isCanonicalKeyValueCountHelperName`; removed slash-method and direct-call
  compatibility predicates use key/value names; collection type inference
  receiver/probe helpers, method type-inference access helpers, and packed-args
  dereferenced access detection also use key/value access names; semantic late
  access validation local diagnostics, visible-helper checks, operand flags, and
  late-fallback rewritten access temporaries should also use key/value access
  names; semantic named-argument access-path filtering should use key/value
  helper names too; semantic collection-dispatch std-namespaced access-path
  filtering should use key/value helper names too; semantic pass-diagnostic
  visible access-builtin filtering should use key/value helper names too, with
  collection-helper rewrite bare-access predicates also using key/value helper
  names; semantic method-resolution string-count shadow checks should use
  key/value access receiver names too; infer-method access predicates should
  use key/value method names too. The stdlib ownership source lock rejects the
  old map-helper, map-count, map-access, and removed map compatibility names.

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

### lowerer-key-value-collection-kind-is-map-agnostic
- Updated: 2026-05-18
- Tags: ir, lowerer, collections
- Fact: Lowerer locals classify map-shaped collection values with
  `LocalInfo::Kind::KeyValueCollection`; `LocalInfo::Kind::Map` is retired.
  Temporary key/value collection metadata, access-target helpers, lookup
  helper APIs, setup-inference access element-kind helpers, and
  inline/native-tail contains/tryAt predicates carry backing details until
  later TODO-4464 slices remove the remaining C++ map substrate; key
  comparison opcode selection plus inline native dispatch published-helper
  metadata wrappers, builtin-name helper-surface checks, and access-target
  metadata/classifier helpers are also named in key/value terms; count-access
  local classifier wrappers, string-valued access helpers, native-tail
  access resolver metadata, shared setup-type collection alias APIs,
  uninitialized-struct inference stdlib helper metadata, lowerer
  tail-dispatch rewrite locals, and statement-call insert rewrite locals
  follow the same naming.
- Evidence: The release `PrimeStruct_backend_ir_tests` target rebuilt after
  the kind, field, target-info, lookup-helper, and setup-inference access
  renames, focused backend IR windows passed, and direct `rg` scans over
  `include`, `src`, and `tests` found no remaining `LocalInfo::Kind::Map`,
  `mapKeyKind`, `mapValueKind`, `referenceToMap`, `pointerToMap`,
  `MapAccessTargetInfo`, `isMapTarget`, `resolveMapAccessTargetInfo`,
  `MapLookupStringKeyResult`, `emitMapLookupAccess`,
  `ArrayMapAccessElementKindResolution`, or
  `resolveArrayMapAccessElementKind`, `isMapContainsHelperName`, or
  `isMapTryAtHelperName`, or `mapKeyCompareOpcode`; the inline native
  dispatch source lock passed after renaming the local published-helper
  metadata wrappers away from `MapHelper`/`mapHelper` names, and the
  builtin-name helper source lock passed after renaming the metadata-backed
  helper-surface and constructor-alias locals away from map-specific names;
  access-target validation passed after renaming metadata/classifier locals
  and making dereferenced args-pack key/value access use the same explicit
  published helper-path detection as direct args-pack access; count-access
  source-delegation coverage passed after renaming local classifier wrappers,
  semantic string access resolution, and explicit source-spelling helpers to
  key/value terms; native-tail source-delegation coverage passed after
  renaming local helper metadata, access-definition, access-alias, and
  builtin-return wrappers to key/value terms; source-delegation coverage
  also passed after renaming the shared setup-type collection alias APIs and
  downstream call-site locals away from map-specific helper names; the same
  focused release target and source-lock test passed after renaming
  uninitialized-struct inference helper metadata and published-helper locals;
  the native-tail metadata source-lock test passed after renaming tail-dispatch
  helper surface, insert rewrite, explicit helper rewrite, and borrowed
  receiver locals to key/value terms; statement-call source-lock and stdlib
  ownership source-lock tests passed after renaming statement helper metadata,
  insert rewrite, args-pack access, and inferred-kind locals.

### map-compatibility-aliases-require-source-definitions
- Updated: 2026-05-18
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
  `experimentalCollectionTypePath`; setup-type collection helper map helper
  and constructor surface checks should derive surface IDs from
  `collections.map_helpers` and `collections.map_constructors` bridge-key
  metadata wrappers instead of direct map surface IDs;
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
  experimental map backing paths through its collection path helper;
  statement-call map insert recognition and rewrite targets should resolve the
  map helper family through `collections.map_helpers` metadata and the stdlib
  surface canonical-helper API instead of spelling public canonical map helper
  paths or naming `StdlibSurfaceId::CollectionsMapHelpers` directly; inline
  native map helper recognition should also resolve the map helper family
  through `collections.map_helpers` metadata instead of naming
  `StdlibSurfaceId::CollectionsMapHelpers` directly; emitter
  return-kind type parsing should classify experimental map backing bases and
  generated prefixes through a local storage-base helper instead of direct
  path text; collection expression lowering should derive experimental map
  backing member roots and generated `Map` prefixes through local collection
  path helpers; inline call map-kind inference and experimental map receiver
  classification should derive rooted, slashless, and generated experimental
  map backing paths through local collection path helpers; lowerer inline-call
  map-kind inference and semantic collection-family gates should use the shared
  builtin/experimental collection classifiers instead of local rooted/canonical
  map type strings; uninitialized-struct inference should recognize explicit map
  args-pack access through the published map-helper surface instead of direct
  rooted/canonical map access strings; struct-slot layout should recognize
  builtin map type names through the shared collection classifier instead of
  direct rooted/canonical map type strings; declared collection inference
  should recognize map type bases and direct map constructors through shared
  collection classifiers and constructor surface metadata instead of direct map
  type and constructor path strings; binding type normalization should recognize
  map types through shared collection classifiers instead of direct
  rooted/canonical map type strings; inference base-kind and dispatch setup
  should recognize map family type text through shared collection classifiers
  instead of direct rooted/canonical map strings; setup-type method target
  resolution should recognize map receiver types through shared collection
  classifiers instead of direct rooted/canonical map strings; setup-type
  method-call canonical map constructor direct-target checks should construct
  the canonical path through collection path helpers instead of a direct map
  path literal; setup-type method-call map helper prefix stripping should derive
  rooted and canonical map prefixes through collection path helpers instead of
  direct map path strings; setup-type method-call canonical map helper lookup
  should build the helper path through collection path helpers instead of direct
  map helper path concatenation; setup-type method-call synthetic fallback
  blocking and explicit vector-count map-target checks should derive map paths
  through collection path helpers instead of direct map path strings; inference
  base-kind map `tryAt`/`contains` call-name checks should derive slashless
  canonical helper paths through collection path helpers instead of direct map
  helper path strings; lowerer struct-return map helper candidate paths should
  derive rooted and canonical prefixes through collection path helpers instead
  of direct map helper path strings; emitter binding-type map compatibility
  checks should derive canonical and experimental map type paths through local
  collection path helpers instead of direct map path fragments; setup-type
  collection helper map alias and normalization checks should derive rooted
  and canonical map prefixes through collection path helpers instead of direct
  map helper path strings; setup-type method-target map method prefix stripping
  should derive rooted and canonical map prefixes through collection path
  helpers instead of direct map path strings; count/access map access helper
  lookup, explicit std-map spelling checks, and rewritten parser-shaped map
  access receivers should derive canonical map helper paths and prefixes
  through collection path helpers instead of direct map path strings; Result
  metadata direct map constructor recognition should derive rooted and
  unrooted canonical map constructor paths through collection path helpers
  instead of direct map path strings; inline-call context generated canonical
  map constructor helper recognition should derive the map constructor prefix
  through collection path helpers instead of a direct map path string;
  statement binding explicit map helper canonicalization should derive the
  canonical map helper root through a local collection path helper instead of
  a direct canonical map path string; lower emit expression collection helper
  explicit map helper canonicalization should derive the canonical map helper
  root through the local collection path helper instead of a direct canonical
  map path string; emitter method metadata removed-alias detection should
  resolve canonical map helper paths through stdlib surface metadata instead
  of a direct slashless canonical map path string; emitter method metadata
  removed-alias detection should resolve both map import aliases and canonical
  map helper paths through the published stdlib surface member resolver instead
  of direct map helper prefix checks; emitter method metadata receiver
  normalization should rely on the shared map type classifier instead of a
  direct rooted `/map` type check; emitter setup return-inference map method
  candidate construction should derive explicit import-alias and canonical
  helper paths through published stdlib surface metadata instead of direct map
  helper path strings; emitter method resolution should resolve explicit map
  helper names and helper paths through published stdlib surface metadata
  instead of direct map helper prefix and path strings; emitter method type
  inference should classify canonical and import-alias map access helper paths
  through published stdlib surface metadata instead of direct map helper prefix
  checks;
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
  map/SOA builtin validation should resolve canonical map contains helper
  paths and diagnostics through stdlib surface metadata instead of direct
  path text;
  collection argument validation should resolve canonical map access helper
  paths and explicit access-path classification through stdlib surface
  metadata instead of direct path text;
  statement printability should resolve canonical map access helper
  classification through stdlib surface metadata instead of direct path
  text;
  pointer-like collection method normalization should derive the canonical
  map helper prefix through stdlib surface metadata instead of direct path
  text;
  collection access target resolution should resolve canonical map
  access/contains helper paths, namespace checks, and missing-definition
  diagnostics through stdlib surface metadata instead of direct path text;
  method target resolution should derive canonical map helper paths,
  namespace checks, explicit helper spelling, and builtin-method
  classification through stdlib surface metadata instead of direct path
  text;
  pre-dispatch direct-call handling should derive canonical map helper paths,
  source-spelling checks, builtin helper classification, and diagnostics
  through stdlib surface metadata instead of direct path text;
  template-monomorph map helper rewrite and receiver-resolution should derive
  canonical helper paths, helper classification, preferred lowering
  spellings, and unknown-target paths through stdlib surface metadata instead
  of direct path text;
  late fallback return-kind inference should derive canonical map helper
  paths and contains/tryAt/access helper classification through stdlib
  surface metadata instead of direct path text;
  semantic collection compatibility should derive canonical map helper paths,
  helper metadata lookups, wrapper-surface detection, and removed
  body-argument targets through stdlib surface metadata while preserving bare
  map-helper canonical diagnostics when the canonical helper is not imported
  or declared;
  IR access-target resolution should derive canonical map constructor and
  access-helper path recognition from stdlib surface metadata instead of
  embedding canonical map path literals;
  native-tail dispatch should derive canonical map helper paths, explicit
  helper classification, import coverage, and missing-helper diagnostics
  from stdlib surface metadata instead of direct canonical map path text;
  statement-expression lowering should derive direct canonical map helper and
  constructor recognition from stdlib surface metadata instead of direct
  canonical map path literals;
  generated collection struct classification should derive the canonical map
  `MapValue__*` prefix through `collectionTypePath("map")` instead of
  carrying a split-string map root; semantic initializer helper preference
  should derive explicit stdlib map helper names and canonical helper paths
  through `collections.map_helpers` metadata instead of a hard-coded
  canonical map namespace, helper-name table, or path concatenation;
  build-parameter default classification should use the shared map collection
  classifier instead of direct canonical map type text; semantic
  struct-return map method probing should also resolve explicit access helper
  spellings, canonical method candidates, and canonical candidate filtering
  through that metadata instead of direct rooted/canonical map helper strings;
  semantic map/SOA builtin validation should derive canonical map contains
  helper paths and resolved helper matching through the same metadata instead
  of direct map surface IDs; statement printability should derive canonical
  map access helper matching through the same metadata instead of direct map
  surface IDs; expression collection-dispatch setup should use the same
  metadata-backed map access matching; expression method resolution should
  derive canonical map access helper paths for string-count shadow rejection
  through `collections.map_helpers` metadata instead of direct map surface
  IDs; try builtin validation should derive canonical map tryAt helper paths
  through the same metadata instead of direct map surface IDs; pointer-like
  collection method normalization should derive the unrooted canonical map
  helper prefix and rooted import-alias helper prefix through the same metadata
  instead of direct map surface IDs or direct `map/` text;
  collection return inference should derive canonical map access helper
  return lookup paths through the same metadata instead of direct path
  concatenation; collection buffer/map resolver inference should derive the
  canonical map constructor path from `collections.map_constructors`
  metadata instead of a hard-coded `/std/collections/map/map` string;
  template monomorph canonical experimental map constructor rewrites should
  derive the canonical constructor path and rooted import alias from the same
  metadata instead of direct `/map` and `/std/collections/map/map`
  comparisons; IR lowerer declared-collection inference should resolve map
  constructor metadata through the `collections.map_constructors` bridge key
  instead of naming the map constructor surface ID directly; packed Result
  map-constructor rewriting should resolve constructor metadata through the
  same bridge key instead of directly naming the map constructor surface ID;
  direct Result value collection metadata should resolve constructor metadata
  through the same bridge key instead of naming the map constructor surface ID;
  lowerer builtin-name and shared helper map-helper metadata lookups should
  resolve through `collections.map_helpers` metadata instead of naming the map
  helper surface ID directly;
  setup-type method-call bridge path filtering should resolve map constructor
  metadata through `collections.map_constructors` instead of naming the map
  constructor surface ID directly;
  statement-binding explicit map helper canonicalization should resolve map
  helper metadata through `collections.map_helpers` instead of naming the map
  helper surface ID directly;
  semantic Result helper inference should derive canonical
  `tryAt`/`tryAt_ref` helper paths and rooted map helper aliases from
  `collections.map_helpers` metadata instead of direct map path comparisons;
  semantic receiver-path resolution should skip map collection temporaries
  through map helper metadata instead of direct rooted `/map` checks;
  statement return auto-diagnostic fallback should derive unknown rooted map
  access method diagnostics from `collections.map_helpers` metadata instead
  of hard-coded `/map/at*` message strings; statement return collection
  normalization should derive the rooted map collection marker from
  `collections.map_constructors` import-alias metadata instead of a direct
  `/map` string; build-return-kind map collection normalization should derive
  the same rooted marker from constructor import-alias metadata instead of a
  direct `/map` string; infer-struct-return map collection fallback should
  derive the same rooted marker from constructor import-alias metadata instead
  of a direct `/map` string; IR access-target forwarded empty map constructor
  checks should derive the `mapNew` token from map constructor metadata plus
  the empty-constructor suffix convention instead of spelling it directly;
  inline packed-args map constructor rewriting should obtain constructor
  metadata through the `collections.map_constructors` bridge key instead of
  directly naming the map constructor surface ID; inline parameter map
  constructor rewriting should use the same bridge-key metadata lookup instead
  of directly naming the map constructor surface ID; effect-free collection
  map helper lookup should use `collections.map_helpers` bridge-key metadata
  instead of directly naming the map helper surface ID; infer
  collection-dispatch map helper return-kind lookup should use the same
  bridge-key metadata lookup instead of directly naming the map helper surface
  ID; pre-dispatch direct-call validation should derive canonical helper
  paths, rooted removed-alias paths, and helper member resolution from
  `collections.map_helpers` metadata instead of directly naming the map helper
  surface ID or constructing rooted map helper aliases; collection-access
  validation should use the same metadata-backed helper path, namespace,
  resolved-name, and rooted-alias derivation pattern; IR uninitialized-struct
  inference should derive map helper and constructor surface IDs and the
  forwarded empty constructor member name from stdlib surface metadata; IR
  lower emit-expression collection helper late dispatch should also derive map
  helper and constructor surface IDs from stdlib metadata wrappers; template
  expression rewriting should derive forwarded empty constructor paths from
  `collections.map_constructors` metadata and removed rooted-alias helper
  matching from `collections.map_helpers` import-alias metadata; semantic
  builtin path helpers should resolve map helper member names and rooted
  import-alias helpers through `collections.map_helpers` metadata instead of
  direct rooted/canonical map helper prefixes; collection buffer/map resolver
  inference should derive rooted map alias checks and map collection alias
  tokens from `collections.map_constructors` metadata instead of direct `/map`
  strings; scalar pointer/memory builtin validation should classify map-like
  builtin collection calls through the shared collection type classifier
  instead of a direct `map` collection token; semantic collection dispatch
  resolver construction should use shared experimental map backing classifiers
  from `MapConstructorHelpers.h` instead of local direct experimental map
  backing predicates; collection return inference should also use those shared
  backing classifiers and the metadata-derived map alias token for map return
  type text; initializer call binding should use shared map backing
  classifiers, map collection type classification, and the metadata-derived
  map alias token instead of direct map collection strings; concrete call
  resolution should classify map entry constructor arguments by resolving the
  `entry` member through map helper stdlib surface metadata instead of
  carrying a direct experimental-map entry fallback; statement init type
  matching should classify generated experimental map backing paths through
  shared map backing helpers and derive the public map collection alias from
  constructor metadata instead of direct map strings; infer-struct-return
  helpers should derive rooted map aliases, unrooted helper prefixes, and
  canonical `MapValue` roots from stdlib surface metadata instead of local
  collection path builders; template-monomorph canonical map constructor
  rewrites should use shared map constructor member and `Entry` backing
  helpers instead of direct experimental-map constructor paths; template
  monomorph binding-call inference currently has no direct map-surface traces
  and should stay source-locked at a zero inventory cap; template-monomorph
  fallback type inference should use shared map backing classifiers and
  metadata-derived map collection aliases instead of direct map strings;
  statement-binding explicit helper canonicalization local helpers should use
  key/value naming even while they still resolve `collections.map_helpers`
  metadata during the map substrate migration; setup-type method-target
  local receiver and canonical helper preference predicates should use
  key/value naming while they still route through the map collection
  metadata bridge; inference dispatch setup map-family type and access
  override locals should use key/value naming while still resolving the
  current map helper metadata bridge; lower emit-expression collection helper
  local metadata, constructor, helper rewrite, and access rewrite names should
  use key/value terminology around the current map bridge metadata; setup-type
  method-call resolution local prefixes, source-helper extraction, receiver
  local checks, and canonical helper lookup names should use key/value
  terminology while still using the map bridge path; setup-type receiver
  target fallback and receiver-probe block names should use key/value naming
  for map-shaped access helpers; setup-type collection helper surface ID and
  surface member token helpers should use key/value naming around the current
  map helper and constructor bridge metadata; statement-expression lowering
  metadata/classifier helpers should use key/value naming for canonical map
  helper and constructor paths; method-call resolution constructor bridge
  choice and bare receiver probe locals should use key/value naming around
  map constructor metadata; access-target resolution constructor path,
  published-constructor, forwarded-empty-constructor, and direct-constructor
  helpers should use key/value naming around map constructor metadata; inline
  packed-args constructor metadata and rewrite helpers should use key/value
  naming around map constructor metadata; inline parameter constructor rewrite
  helpers should use the same key/value naming; packed Result constructor
  metadata and rewrite helpers should use key/value naming around map
  constructor metadata; Result metadata direct constructor detection and local
  key-kind temporaries should use key/value naming around map constructor
  metadata; declared collection inference direct constructor probes should use
  key/value naming around map constructor metadata; generic lowerer helper
  surface probes should use key/value naming around map helper metadata;
  emitter call-path local bridge constants, metadata lookups, and constructor
  alias token helpers should use key/value naming around map metadata; emitter
  return-inference local map helper bridge constants and helper-path locals
  should use key/value naming around map helper metadata; emitter method
  type-inference local canonical/import-alias helper path probes should use
  key/value naming around map helper metadata; emitter collection-type helper
  local bridge and access-helper probes should use key/value naming around
  map helper metadata; emitter method-resolution local map helper bridge and
  helper-method flag should use key/value naming around map helper metadata;
  emitter method-metadata local map helper bridge, surface, and removed-helper
  wrappers should use key/value naming around map helper metadata; semantic
  collection dispatch setup local access-helper probes should use key/value
  naming around map helper metadata; semantic pre-dispatch direct-call local
  helper metadata, path, member-token, and spelling helpers should use
  key/value naming around map helper metadata; semantic initializer inference
  explicit std helper lambda should use key/value naming around map helper
  metadata; semantic return-kind collection marker locals should use key/value
  naming around map constructor metadata; semantic call-resolution local
  helper-root, alias-path, constructor-metadata, and namespace-prefix probes
  should use key/value naming around map metadata; semantic infer-struct-return
  collection marker, helper-prefix, and backing-root locals should use
  key/value naming around map metadata; semantic return-statement collection
  marker, borrowed-access diagnostic, and backing-root locals should use
  key/value naming around map metadata; semantic pointer-like helper-prefix
  locals should use key/value naming around map helper metadata; semantic
  statement-printability resolved-path helper locals should use key/value
  naming around map helper metadata; semantic argument-validation canonical
  access-helper locals should use key/value naming around map helper metadata;
  semantic map-SOA builtin canonical helper-path locals should use key/value
  naming around map helper metadata; semantic collection-access validation
  canonical path, namespace, resolver, alias-path, and rewritten-call locals
  should use key/value naming around map helper metadata; semantic
  collection-access target metadata, path, namespace, resolver, and method
  helper locals should use key/value naming around map helper metadata;
  semantic collection-rewrite metadata, path, token, explicit-path, and
  lowering-path locals should use key/value naming around map helper metadata;
  semantic binding-type collection-root and backing-root metadata matchers
  should use key/value naming around map helper metadata; semantic collection
  compatibility canonical path, published-token, resolved-path, and removed
  helper locals should use key/value naming around map helper metadata;
  semantic method-target canonical path, namespace, resolver, alias, and
  canonical-helper locals should use key/value naming around map helper
  metadata; semantic late map-access canonical path, namespace, resolver,
  resolved-helper, and rewritten-call locals should use key/value naming
  around map helper metadata; semantic pre-dispatch inferred-call unrooted
  surface, canonicalized target, rewritten-call, borrowed-path, and removed
  compatibility locals should use key/value naming around map helper metadata;
  semantic infer-struct-return explicit-access, candidate-helper, and source
  method-path locals should use key/value naming around map helper metadata;
  semantic collection-compatibility internal published-helper, borrowed-helper,
  import-alias, canonical-resolver, and explicit-member helper APIs should use
  key/value naming around map helper metadata; semantic pre-dispatch
  direct-call canonicalized target, rewritten-call, and borrowed-path locals
  should use key/value naming around map helper metadata; semantic
  collection-rewrite direct experimental spelling locals should use key/value
  naming around map helper metadata; semantic effect-free collection metadata,
  canonical-path, and unrooted-prefix locals should use key/value naming
  around map helper metadata; semantic late-fallback collection metadata,
  canonical-path, resolver, alias, and rewritten-call locals should use
  key/value naming around map helper metadata; native-tail import-probe and
  read-helper import locals should use key/value naming around map helper
  metadata; IR setup return-kind canonical helper locals should use key/value
  naming around map helper metadata; lowerer call-helper removed-alias
  fallback locals should use key/value naming around map helper metadata;
  semantic infer-dispatch metadata and resolved-helper locals should use
  key/value naming around map helper metadata; semantic statement body-argument
  compatibility, metadata, canonical-path, alias-path, and receiver locals
  should use key/value naming around map helper metadata; semantic
  infer-struct-return helper metadata locals should use key/value naming around
  map helper metadata; semantic argument-validation collection metadata,
  canonical-path, resolver, and resolved-helper locals should use key/value
  naming around map helper metadata; semantic collection-access validation
  access-helper predicate locals should use key/value naming around map helper
  metadata; semantic collection-access access-helper predicate and canonical
  path locals should use key/value naming around map helper metadata; semantic
  collection-access dispatch/access-result locals should use key/value naming
  around map helper metadata; semantic method-resolution and builtin-path
  helper resolved-name locals should use key/value naming around map helper
  metadata; semantic method-resolution access/string-return/helper-name locals
  should use key/value naming around map helper metadata; semantic
  collection-dispatch setup access-helper predicates should use key/value
  naming around map helper metadata; semantic collection-access validation
  explicit-access helper locals should use key/value naming around map helper
  metadata; semantic argument-validation collection access locals should use
  key/value naming around map helper metadata; semantic late-fallback access
  helper predicates should use key/value naming around map helper metadata;
  semantic late-access builtin helper predicates and rewrite locals should use
  key/value naming around map helper metadata; semantic infer-definition access
  helper predicates should use key/value naming around map helper metadata;
  semantic builtin-path helper resolved-token locals should use key/value
  naming around map helper metadata; semantic vector-helper resolved-token
  locals should use key/value naming around map helper metadata; semantic
  call-path helper resolved-token locals should use key/value naming around
  map helper metadata; semantic validator diagnostic-path resolved-token
  locals should use key/value naming around map helper metadata; semantic
  infer-method resolved-token locals should use key/value naming around map
  helper metadata; emitter metadata resolved-token locals should use
  key/value naming around map helper metadata; semantic method-target
  explicit-path and preferred-target locals should use key/value naming around
  map helper metadata; semantic infer-method helper metadata accessors and
  canonical-path wrappers should use key/value naming around map helper
  metadata; semantic collection-rewrite visibility predicates should use
  key/value naming around map helper metadata; template expression rewrite
  borrowed unknown-target locals should use key/value naming around map helper
  metadata; template monomorph receiver helper metadata accessors should use
  key/value naming around map helper metadata; template monomorph receiver
  helper-name resolvers should use key/value naming around map helper
  metadata; template monomorph canonical helper path wrappers should use
  key/value naming around map helper metadata; template monomorph canonical
  helper-name resolvers should use key/value naming around map helper
  metadata; template monomorph canonical helper path predicates should use
  key/value naming around map helper metadata; template monomorph canonical
  access/count predicates should use key/value naming around map helper
  metadata; template monomorph preferred-spelling helpers should use key/value
  naming around map helper metadata; template monomorph unknown-target helpers
  should use key/value naming around map helper metadata; template monomorph
  canonical-to-experimental helper path wrappers should use key/value naming
  around map helper metadata; template monomorph wrapper-to-experimental helper
  path wrappers should use key/value naming around map helper metadata; vector
  count wrapper target diagnostics should use key/value naming around map
  helper metadata; preferred experimental helper target selectors should use
  key/value naming around map helper metadata; canonical experimental helper
  path canonicalizers should use key/value naming around map helper metadata;
  bare and specialized experimental helper target selectors should use
  key/value naming around map helper metadata; canonical experimental helper
  rewrite helpers should use key/value naming around map helper metadata;
  removed-compatibility helper path resolvers should use key/value naming
  around map helper metadata; try-builtin and method-target compatibility
  callbacks should use direct key/value helper naming around map helper
  metadata; template-monomorph removed-compatibility locals should use
  key/value naming around map helper metadata; pre-dispatch removed alias
  helper locals should use key/value naming around map helper metadata;
  shared collection helper rewrite APIs should use key/value naming around
  map helper metadata; infer collection compatibility path helpers and prefix
  helpers should use key/value naming around map helper metadata;
  template-monomorph removed-compatibility predicate/base helpers should use
  key/value naming around map helper metadata; semantic builtin-path removed
  compatibility predicates should use key/value naming around map helper
  metadata; semantic method-target removed compatibility predicates should use
  key/value naming around map helper metadata; semantic build-call removed
  compatibility predicates should use key/value naming around map helper
  metadata; semantic pre-dispatch removed compatibility predicates should use
  key/value naming around map helper metadata; template-monomorph type
  resolution removed compatibility predicates should use key/value naming
  around map helper metadata; semantic validation removed-read compatibility
  locals should use key/value naming around map helper metadata; emitter
  storage compatibility helpers should use key/value naming around map helper
  metadata.
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
  vector stdlib helper preference no longer normalizes map paths; emitter
  call-path helper classification now derives map helper and constructor path
  ownership through published stdlib surface metadata instead of direct map
  helper prefix strings; emitter collection-type canonical map access
  detection now resolves helper ownership through `collections.map_helpers`
  metadata instead of parsing the canonical map helper prefix; lowerer
  statement-binding explicit map helper canonicalization now detects raw
  published map helper paths through `CollectionsMapHelpers` metadata instead
  of direct rooted/canonical map path prefix checks; lowerer emit-expression
  explicit map helper rewriting now detects raw alias and canonical map helper
  paths through `CollectionsMapHelpers` metadata instead of direct
  rooted/canonical map helper prefix checks; lowerer builtin array-access
  classification now rejects published map helper surface paths through
  `CollectionsMapHelpers` metadata instead of direct `map/` and
  `std/collections/map/` prefix checks; inline native dispatch now classifies
  removed explicit map access helper paths through `CollectionsMapHelpers`
  metadata instead of a literal `map/at*` raw-path table; lowerer simple-call
  scoped collection alias rejection now recognizes map helper surface paths
  through `CollectionsMapHelpers` metadata instead of a direct slashless
  `map/` prefix check; semantic method-target resolution now recognizes
  slashless map helper alias paths through
  `metadataBackedMapHelperRootAliasMethodName` instead of direct `map/`
  prefix checks; semantic simple-call path helpers now resolve map helper
  member names through `collections.map_helpers` metadata instead of direct
  slashless and canonical map prefix stripping; semantic concrete call
  resolution now recognizes map entry helper and constructor base paths
  through map surface metadata instead of direct canonical map
  entry/constructor literals; lowerer count fallback now recognizes removed
  map helper alias calls through `collections.map_helpers` metadata instead
  of a local slashless `map/` helper table; semantic collection dispatch
  setup now recognizes rooted map access compatibility paths through
  `CollectionsMapHelpers` import-alias metadata instead of a literal
  `/map/at*` table; and semantic initializer helper preference now resolves
  explicit stdlib map helper names through `collections.map_helpers` metadata
  instead of hard-coded `std/collections/map` namespace and helper tables;
  semantic struct-return map method probing now uses the same metadata for
  explicit access helper detection, canonical method candidates, and
  candidate filtering instead of direct rooted/canonical map helper strings;
  semantic map/SOA builtin validation now derives canonical map contains
  helper paths and resolved helper matching through
  `collections.map_helpers` metadata instead of direct map surface IDs;
  statement printability now uses the same metadata for canonical map access
  helper matching instead of direct map surface IDs; expression
  collection-dispatch setup now also uses metadata-backed canonical map
  access helper matching instead of direct map surface IDs; argument
  validation map access helper lookup should also use the bridge-key
  `collections.map_helpers` metadata instead of directly naming the map
  helper surface ID; infer-method map helper target selection should derive
  canonical helper paths from that same bridge-key metadata instead of
  concatenating canonical map helper paths; lowerer dispatch setup should do
  the same for semantic-product map access helper return-kind checks; lowerer
  setup-type method-target resolution should use `collectionTypePath("map")`
  instead of spelling the canonical map root directly; template monomorph
  receiver map helper lowering should use `collections.map_helpers` metadata
  to detect rooted import-alias helper paths instead of spelling that rooted
  helper prefix directly; late map access builtin helper lookup should use the
  same bridge-key metadata lookup instead of directly naming the map helper
  surface ID; emitter method metadata should classify map helper surface
  identity and removed map helper aliases through `collections.map_helpers`
  metadata instead of directly naming the map helper surface ID; inline
  native dispatch should also resolve removed explicit map access helpers and
  canonical published map helper paths through local `collections.map_helpers`
  metadata wrappers instead of directly naming the map helper surface ID;
  semantic build call resolution should derive the canonical map helper root,
  public map constructor path, and map helper namespace prefix from stdlib
  surface metadata instead of direct canonical map path literals; semantic
  infer pre-dispatch call handling should derive slashless canonical map
  helper path detection and rooted map access alias classification from
  stdlib surface metadata instead of direct map path literals; semantic vector
  helper method-target normalization should derive map helper member names
  through stdlib surface metadata instead of direct rooted/canonical map helper
  prefix stripping; infer-method method-name normalization should derive
  explicit map helper names through stdlib surface metadata instead of direct
  `map/` and `std/collections/map/` prefix stripping; semantic-product
  collection specialization publication should derive map type-root recognition
  and helper/constructor surface ids through stdlib surface metadata instead of
  direct map paths and map surface enum constants; template monomorph
  collection compatibility should derive explicit map helper member names and
  map collection-root matching through stdlib surface metadata instead of
  direct `map/` or `std/collections/map` checks; core semantic unknown-call
  formatting and diagnostic target normalization should derive map helper
  membership and canonical count paths through that metadata instead of direct
  map path checks; late fallback map access diagnostics should derive rooted
  import-alias access helper recognition from stdlib map import-alias metadata
  instead of a direct `/map/at*` path table; template monomorph core
  collection-base/import coverage plus map entry and constructor overload
  checks should derive roots and member paths through stdlib surface metadata
  instead of direct map path literals; infer collection dispatch setup should
  derive map helper metadata through the bridge key before resolving rooted
  aliases, canonical access helpers, and namespace checks instead of directly
  naming the map helper surface ID; expression argument validation should
  derive canonical map access helper matching through stdlib map helper
  metadata and map template-base checks through `isMapCollectionTypeName`
  instead of direct canonical map path and type spellings; definition return
  inference should detect deferred canonical map access helpers through stdlib
  map helper metadata instead of direct canonical map access path literals;
  semantic collection-helper rewrites should derive canonical helper paths,
  explicit helper-path recognition, member-token resolution, and lowering
  spelling preference through `collections.map_helpers` metadata instead of
  direct map surface IDs or rooted/canonical helper path concatenation;
  semantic collection-access resolution should not carry rooted `/map/at*` or
  `/map/contains*` compatibility branches, and should only recognize canonical
  map helper spellings through stdlib surface metadata so removed aliases fall
  through as ordinary unresolved calls; lower emit-expression tail dispatch
  should derive map helper surface IDs and import-alias helper checks through
  `collections.map_helpers` metadata instead of direct map surface enum
  constants or raw rooted alias string checks; semantic method-target
  resolution should derive canonical map helper IDs, root-alias helper paths,
  direct-call compatibility helper names, and explicit rooted-helper detection
  from `collections.map_helpers` metadata instead of direct map surface IDs,
  literal rooted helper tables, or raw rooted path-prefix checks; shared
  semantic collection-compatibility helpers should resolve map helper tokens,
  unrooted helper paths, direct-import diagnostics, and removed-helper rooted
  paths from `collections.map_helpers` metadata instead of direct map helper
  surface IDs, literal namespace comparisons, or hard-coded rooted path
  construction; semantic collection-compatibility inference should also derive
  canonical map type-root checks, import matching, explicit helper-name routing,
  removed-helper path construction, and explicit rooted-alias detection from
  `collections.map_helpers` metadata rather than literal map path strings.
  Semantic validation builtin map insert/read rewrites should also derive
  insert/read member names, canonical helper paths, constructor detection, and
  removed rooted access alias paths from `collections.map_helpers` and map
  constructor metadata instead of direct map surface IDs or path strings.
  Semantic binding type helpers should derive public map roots and `MapValue`
  roots from `collections.map_helpers` metadata, and route experimental map
  backing return-kind detection through the shared map backing predicate
  instead of direct public map root strings.
  The stale
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

### map-constructor-aliases-come-from-metadata
- Updated: 2026-05-18
- Tags: ir, collections, stdlib
- Fact: Lowerer builtin-name classification must derive the public map
  constructor alias from `collections.map_constructors` metadata instead of
  comparing against a literal `map` token.
- Evidence: `IrLowererBuiltinNameHelpers.cpp` now reads the slashless
  constructor import alias from the stdlib surface registry, the map ownership
  source lock rejects direct `alias == "map"` and `rawName == "map"` checks in
  that file, and the file's map-surface inventory allowance is zero.

### map-helper-classification-is-local
- Updated: 2026-05-19
- Tags: semantics, collections, stdlib
- Fact: Map base/borrowed helper classification should be local to semantic
  compatibility code and derived from `collections.map_helpers` metadata,
  not exposed as public map-specific stdlib registry APIs; semantic simple-call
  and builtin-path helper member resolution should also use key/value local
  names around that metadata-backed map surface.
- Evidence: `isStdlibMapBaseHelperName` and `isStdlibMapBorrowedHelperName`
  were removed from `StdlibSurfaceRegistry`; the semantic compatibility helper
  now resolves member names through bridge-key metadata directly, and
  `SemanticsCallPathHelpers.cpp` and `SemanticsBuiltinPathHelpers.cpp` resolve
  helper member names with key/value local resolver names.

### collection-manifest-loader-is-id-agnostic
- Updated: 2026-05-18
- Tags: stdlib, registry, collections
- Fact: The collection surface manifest loader should not parse map-specific
  manifest IDs in C++; collection records are applied to their ordered surface
  slots and the manifest remains the data source for bridge keys and members.
- Evidence: `StdlibSurfaceRegistry.cpp` no longer has
  `parseManifestSurfaceId` or a `CollectionsMap*` manifest-load switch; its
  map-surface inventory allowance is down to the metadata ID and resolver
  switch traces.

### collection-member-resolution-is-generic
- Updated: 2026-05-18
- Tags: stdlib, registry, collections
- Fact: Collection helper/constructor member resolution should depend on
  surface domain and shape, not individual vector/map/SoA surface IDs.
- Evidence: `resolveSurfaceMemberNameImpl` now sends every non-error
  collection surface through `resolveMetadataMemberName`, removing the
  map-specific resolver switch cases from `StdlibSurfaceRegistry.cpp`.

### collection-surface-ids-are-slot-derived
- Updated: 2026-05-18
- Tags: stdlib, registry, collections
- Fact: Collection registry metadata should derive public surface IDs from the
  ordered manifest slot contract instead of spelling map-specific enum names
  in `StdlibSurfaceRegistry.cpp`.
- Evidence: Collection registry entries now use `collectionSurfaceId(slot)`;
  the direct map-surface trace inventory allowance for
  `src/StdlibSurfaceRegistry.cpp` is zero.

### public-surface-enum-is-map-agnostic
- Updated: 2026-05-18
- Tags: stdlib, registry, collections
- Fact: Public stdlib surface IDs should not expose map-specific enum names.
  Consumers that need a map helper or constructor surface ID should resolve it
  from bridge-key metadata.
- Evidence: `StdlibSurfaceId` now uses generic manifest surface slots for the
  former map helper/constructor positions, and the trace inventory allowance
  for `include/primec/StdlibSurfaceRegistry.h` is zero.

### map-low-count-lowerer-caps-are-stale
- Updated: 2026-05-18
- Tags: ir, collections, audit
- Fact: Some lowerer map-surface inventory caps can be retired by direct
  source scan without code changes when earlier slices already removed the
  traces.
- Evidence: Direct scans found no map-surface traces in lowerer try-expression
  helpers, packed Result helpers, Result metadata helpers, struct-layout
  helpers, struct-type helpers, inference fallback setup, statement-binding
  helpers, operator collection mutation helpers, count-access helpers,
  statement-binding type metadata, struct-field binding helpers, binding-type
  helpers, inference base-kind helpers, statement-binding helpers, or
  struct-return path helpers, inline-call context helpers, lowerer inline
  calls, native tail dispatch, struct-slot layout helpers, call helpers, call
  resolution, lower-statement expression helpers, setup-type method-call
  resolution, statement-call emission, IR printer helpers, emitter
  method-resolution helpers, emitter method-resolution type inference helpers,
  emitter setup-return inference collection helpers, emitter collection-type
  helpers, emitter helper type parsing, or emitter builtin call-path helpers;
  their inventory caps are now zero and the obsolete backing-trace ownership
  allowlist entries were removed.

### map-return-kind-targets-use-key-value-names
- Updated: 2026-05-18
- Tags: ir, collections, stdlib
- Fact: Lowerer setup-type semantic return-kind target metadata should use
  key/value terminology for map collection facts instead of `mapInfo` or
  map-receiver helper names.
- Evidence: `SemanticReturnKindTargetInfo` now stores `keyValueInfo`, the
  setup-type local semantic target resolver is named
  `resolveSemanticKeyValueTargetInfo`, and focused release setup-type/source
  delegation tests passed after the rename.

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
- Updated: 2026-05-18
- Tags: semantics, ir, collections
- Fact: Rooted retired public map constructor aliases such as
  `/std/collections/mapSingle`, `/std/collections/mapPair`, and
  `/map/entry` must not be treated as canonical map constructors; unqualified
  names remain separate legacy/internal compatibility surfaces, and
  metadata-backed constructor rewrites must not synthesize fixed-arity
  replacements for them or keep no-op map rewrite shims alive. Map
  constructor helper path classification should reuse
  `resolveMapConstructorMemberPath(...)` instead of naming the map constructor
  surface ID directly.
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
  classify rooted `/map/entry` spellings as map entry constructors; map
  constructor helper path checks now route through the local metadata wrapper
  and have zero targeted map-surface trace matches.

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
