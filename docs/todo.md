# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished
  - Informational note, checkpoint, or rule

Finished items are periodically moved to `docs/todo_finished.md`.

**Types & Semantics**
**Group 1 - Roadmap framing**
Guiding notes:
- **Collection strategy:** keep `array` as a language-core collection, but migrate `vector` and `map` to stdlib
  `.prime` implementations so users can build equivalent containers. Develop the language/runtime substrate far enough
  that `soa_vector` can follow the same end-state rather than remaining a permanent compiler-owned collection. Progress
  is tracked by the slices below plus the archived finished slice bullets in `docs/todo_finished.md`.
- **Vector/map target state:** aside from minimal runtime/memory substrate, C++ compiler/backend code should not know
  collection semantics for `vector` or `map`; that behavior should live in stdlib `.prime` files.
- **SoA target state:** `soa_vector` should also end up as a stdlib `.prime` implementation. Any temporary
  compiler/runtime support should be reduced to generic substrate only: field-layout/codegen/introspection, column
  storage, field-view borrowing/invalidation, and allocation primitives that do not mention `soa_vector` in C++ source.
  The public `/std/collections/soa_vector/*` behavior should eventually live entirely in `.prime`.
- **Type ownership alignment:** the canonical `core` vs `hybrid` vs `stdlib-owned` matrix now lives in
  `docs/PrimeStruct.md` under "Type Ownership Model". Keep roadmap slices aligned to that table: fixed-width scalars,
  `string`, `array<T>`, `Pointer<T>`, and `Reference<T>` are core substrate; `Result<T, Error>`, `File<Mode>`,
  `Buffer<T>`, and `/std/gfx/*` are hybrid surfaces; `Maybe<T>`, `vector<T>`, `map<K, V>`, and target-state
  `soa_vector<T>` are stdlib-owned public surfaces.
- **Execution order:** use this sequence and resist bridge churn outside it.
  Finish only the runtime/ownership substrate that self-contained `.prime` containers genuinely need; then keep landing
  real `.prime` container behavior under the experimental namespaces until VM/native/C++ parity is good enough to trust
  the implementations; then replace wrapper-only canonical `/std/collections/vector/*` and
  `/std/collections/map/*` entry points with those real `.prime` implementations; then delete compiler-owned
  compatibility handling and helper routing from semantics first, then IR lowering, then emitters/backends; and leave
  only allocation/error/drop substrate in C++ at the end.
- **SoA rollout:** follow the same end-state as `vector`/`map`, but use a tighter vertical-slice sequence. First lock
  the `soa_vector` contract and invalidation rules. Then land only the minimum substrate needed for one real `.prime`
  implementation under an experimental namespace. Let that experimental implementation drive the next substrate slices,
  then route canonical `/std/collections/soa_vector/*` names to it, and finally delete all `soa_vector`-named
  compiler/backend/runtime paths.
- **Map-first deletion order:** `map` should be deleted from compiler-owned code first because experimental `map`
  already has a real `.prime` storage/runtime implementation; `vector` should follow once the remaining ownership/drop
  substrate is in place.
- **Map replacement milestone:** the next serious `map` milestone is constructor/type-surface migration from builtin
  `map<K, V>` onto the real experimental `.prime` `Map<K, V>` implementation. Do not add more helper-routing
  micro-slices unless they directly unblock that migration or deletion of a concrete compiler-owned path.
- **Deletion gating:** once a direct compatibility/fallback cleanup slice is stabilized, prefer the next real canonical
  `.prime` replacement or compiler-path deletion milestone over more narrow bridge cleanup. Do not keep peeling
  compatibility sub-slices when the next real de-builtinization step is ready.
- **Substrate bar:** the items below are the hard prerequisites for moving `vector`/`map` into self-contained `.prime`
  implementations instead of extending compiler-owned collection behavior. Keep them only when they describe substrate
  the `.prime` containers genuinely need. Apply the same rule to `soa_vector`: do not add more compiler-owned SoA
  behavior unless it directly unblocks the minimum substrate, experimental `.prime` bring-up, or deletion of an
  existing compiler-owned path.

**Group 2 - Hybrid surfaces**
Guiding notes:
- Do not treat `Result`, `File`, `Buffer`, or `/std/gfx/*` follow-up work as candidates for full de-builtinization in
  the same sense as `vector`/`map`; the target there is a smaller builtin substrate with stdlib-owned public surface,
  not zero compiler/runtime knowledge.
- Completed `Result`, `File`, `Buffer`, and gfx parity/cleanup slices now live in `docs/todo_finished.md`. Keep this
  section focused on checkpoint notes plus any genuinely unfinished hybrid-surface work.
- **File facade checkpoint:** the public `File<Mode>` helper surface is now primarily stdlib-owned. `/std/file/*`
  provides `.prime` wrappers for constructor-shaped opens plus `read_byte`, `write`, `write_line`, `write_byte`,
  `write_bytes`, `flush`, and `close`, and broader multi-value `write(...)` / `write_line(...)` arities above nine
  values now fail through an explicit stdlib-facade diagnostic instead of silently depending on further fixed-arity
  ladder growth. Keep any new `File` follow-up work split into concrete parity or cleanup slices instead of reopening
  one umbrella item.
- **Buffer/gfx facade checkpoint:** canonical and experimental gfx packages now expose `.prime`-authored `Buffer<T>`
  inspection/allocation/storage helpers plus `.prime` helper layers for the canonical `Window(...)`, `Device()`,
  swapchain, mesh, pipeline, frame, render-pass, submit, and present entrypoints. Keep any new `Buffer<T>` or
  `/std/gfx/*` follow-up work split into concrete parity, runtime, or cleanup slices instead of reopening one umbrella
  item.
- **Follow-up rule:** the `Result` work plus the `File` / `Buffer` checkpoints above are already too broad to extend
  directly. Land any remaining work through concrete slices instead of reopening those broader buckets.

**Group 3 - Arg-pack runtime materialization**
Guiding notes:
- Completed parser/body-API work and the finished IR element-kind slices are archived in `docs/todo_finished.md`. Keep
  only the started runtime-materialization slices and remaining gaps here.
- **Legacy backend checkpoint:** the C++ emitter already materializes trailing concrete `[args<T>]` parameters as
  `std::vector<T>` values, supports direct variadic calls plus mixed explicit-prefix + final-spread forwarding, and
  executes the read-only `count` / `at` / `at_unsafe` / indexing body API including downstream method resolution on
  indexed values.
- **IR-backed checkpoint:** IR/VM/native now cover concrete value packs across direct, pure-spread, and mixed
  forwarding for numeric, bool, string, struct, `Result`, `FileError`, `File<Mode>`, `Buffer<T>`, `array`, `vector`,
  `soa_vector`, `map`, and many `Reference<T>` / `Pointer<T>` forms, including indexed downstream helper/method access
  for the already-landed kinds.
- ◐ Variadic arg-pack legacy-backend parity slice: finish any remaining C++ emitter gaps for concrete value packs and
  their read-only body API so stdlib `.prime` variadic helpers do not need constructor ladders or backend-only special
  cases. Progress: C++ compile-run coverage now includes concrete value-pack `Result<i32, ParseError>` and status-only
  `Result<ParseError>` flows across direct invocation, pure spread forwarding, and mixed explicit-prefix + spread
  forwarding, plus direct/pure-spread/mixed compile-run coverage for canonical `map<i32, i32>`, `Buffer<i32>`,
  `FileError`, and `File<Write>` packs exercising indexed count/why/file-helper access on the legacy backend.
- ◐ Variadic arg-pack legacy-backend borrowed/location slice: finish any remaining C++ emitter gaps for
  `[args<Reference<T>>]` / `[args<Pointer<T>>]` materialization, explicit `location(...)` forwarding, and indexed
  `dereference(...)` receiver flows. Progress: C++ emitter variadic pack-location binding resolution now tracks
  `dereference(...)` receiver element types, so `location(dereference(values.at(...)).<reference_field>)` and equivalent
  indexed-dereference receiver shapes materialize deterministic pointer pack elements instead of falling back to
  wrapper-address forms, and compile-run coverage now locks direct/pure-spread/mixed `Reference<FileError>`,
  `Pointer<FileError>`, `Reference<File<Write>>`, and `Pointer<File<Write>>` packs exercising explicit `location(...)`
  forwarding plus indexed `dereference(...).why()` / file-method access on the legacy backend.
- ◐ Variadic arg-pack legacy-backend reject/diagnostic slice: lock deterministic rejects for unsupported element kinds
  and unsupported forwarding/body-API shapes on the C++ emitter path. Progress: inline-parameter lowering now rejects
  `args<Pointer<string>>` / `args<Reference<string>>` with the explicit `variadic args<T> does not support string
  pointers or references` diagnostic instead of falling through generic variadic type-mismatch handling, directly
  rejects non-`location(...)` value forwarding into `args<Reference<T>>` / `args<Pointer<T>>` with dedicated
  wrapped-pack diagnostics, and helper-level IR validation plus C++ compile coverage now assert both reject contracts
  directly.
- ◐ Variadic arg-pack IR value-pack parity slice: finish any remaining IR/VM/native gaps for concrete value packs across
  direct calls plus pure/mixed spread forwarding. Progress: VM compile-run coverage now includes concrete value-pack
  `Result<i32, ParseError>` and status-only `Result<ParseError>` flows across direct invocation, pure spread forwarding,
  and mixed explicit-prefix + spread forwarding, matching the existing IR/native parity expectations for those
  value-pack shapes.
- ◐ Variadic arg-pack IR borrowed/pointer parity slice: finish any remaining IR/VM/native gaps for `Reference<T>` /
  `Pointer<T>` packs, explicit `location(...)` forwarding, and indexed `dereference(...)` flows. Progress: IR
  struct-type inference now preserves indexed args-pack struct identity through `dereference(...)`, so borrowed variadic
  `Reference<Map<K, V>>` flows can forward indexed values into canonical experimental-map helpers without `struct
  parameter type mismatch` rejects on the C++ emitter path, and base-kind `try(...)` inference now resolves indexed
  `dereference(at(values, ...))` `Reference<Result<T, E>>` / `Pointer<Result<T, E>>` packs symmetrically instead of only
  recognizing pointer-backed result packs.
- ◐ Variadic arg-pack IR container/resource receiver slice: finish any remaining IR/VM/native gaps where indexed pack
  access still falls back or rejects for collection/resource element kinds used by stdlib-owned variadic helpers.
  Progress: method-receiver type resolution now recognizes indexed `Buffer<T>` args-pack receivers, count-access
  classification now accepts borrowed/pointer `dereference(at(values, ...))` buffer targets, and array/vector
  access-target inference now treats direct and indexed `Buffer<T>` pack elements as count-capable collection receivers,
  so VM lowering no longer falls back to the generic `count requires array, vector, map, or string target` path when
  variadic buffer helpers call `.empty()`, `.is_valid()`, or `.count()` on indexed pack elements. Wrapped
  `Reference<File<Mode>>` / `Pointer<File<Mode>>` canonical `at([values] ..., [index] ...)` receivers now also lower
  through the file-handle result/emission path directly, so variadic stdlib file helpers no longer trip the generic `try
  requires Result argument` failure on indexed `write_line` / `flush` / `read_byte` flows.
- ◐ Variadic arg-pack IR reject/diagnostic slice: lock deterministic unsupported-kind and unsupported-forwarding
  diagnostics once the remaining materialization gaps are closed. Progress: IR lowering now emits an explicit `variadic
  args<T> does not support string pointers or references` reject for `args<Pointer<string>>` / `args<Reference<string>>`
  declarations instead of the previous generic backend-level pointer/reference string diagnostic, and VM/C++ compile-run
  coverage now asserts that diagnostic directly so successful program exits cannot mask missing rejects.
- **Completion rule:** do not keep another catch-all "remaining non-string element kinds" umbrella once only scattered
  gaps remain. Add new arg-pack TODOs only for concrete unsupported element kinds backed by a reproducible
  semantics/lowering/backend failure.
- Detailed finished arg-pack IR element-kind checkpoints were moved to `docs/todo_finished.md`. Keep this file focused
  on the active materialization slices plus any truly remaining unsupported kinds.

**Group 4 - Ownership and lifetime substrate**
Guiding notes:
- Completed guard and container-error-contract checkpoints are archived in `docs/todo_finished.md`. Keep the live file
  on the remaining non-trivial destruction and lifetime substrate work.
- ◐ Ownership/drop reallocation slice: extend the same non-trivial move semantics to the remaining canonical
  wrapper-backed vector/map growth paths now that experimental `Vector<T>` and experimental `.prime` `Map<K, V>` storage
  no longer depend on builtin relocation gates. Progress: canonical wrapper-backed builtin `map` constructor growth now
  rejects non-relocation-trivial key/value element types, and canonical wrapper-backed builtin `vector` constructor
  growth now rejects non-relocation-trivial element types, with deterministic semantics diagnostics and focused
  ownership regression coverage so unsupported ownership paths fail before lowering while experimental `.prime`
  map/vector ownership-sensitive growth remains available.
- ○ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed.

**Group 5 - Container bring-up and de-builtinization**
Guiding notes:
- Land a real `.prime` implementation under an experimental path/name first, prove parity there, replace the canonical
  wrapper-backed names with those real implementations, then remove the builtin/compiler-owned implementation.
- Completed runtime-contract, experimental parity, and wrapper-routing checkpoints are archived in
  `docs/todo_finished.md`.
- ◐ Experimental stdlib `vector` storage/runtime slice: land a read-only `.prime`-owned storage/runtime path for
  `/std/collections/experimental_vector/*` with constructors/count/capacity/access backed by heap pointers. Progress:
  `/std/collections/experimental_vector/*` now returns a real experimental `Vector<T>` record backed by pointer-based
  `.prime` storage, and that storage now uses `Pointer<uninitialized<T>>` slots plus stdlib-owned `borrow(...)` access
  so shared C++/VM/native conformance covers constructors through `vectorOct`, `vectorCount`, `vectorCapacity`,
  `vectorAt`, and `vectorAtUnsafe` even for non-trivial experimental element flows; checked experimental index
  operations now also reject negative indices through the shared bounds guard instead of falling through to invalid
  pointer arithmetic, and `vectorAtUnsafe` now rejects both negative and out-of-range positive indices through the same
  deterministic bounds-error path instead of reaching invalid pointer arithmetic or uninitialized-slot access, with the
  bounds guard now triggering that error through array indexing instead of allocating a canonical `/vector/*` value,
  runtime shape guards now reject forged invalid `count/capacity` combinations (`count < 0`, `capacity < 0`, `capacity >
  1073741823`, or `count > capacity`) before accessor, mutator, and destroy paths touch pointer slots, struct field
  mutators now enforce the same invariants at write time, and the internal pointer-slot and drop-range helpers now
  enforce the same shape/capacity boundary before unchecked offset arithmetic or drop iteration.
- ✓ Experimental stdlib `vector` growth helper slice: add `/std/collections/experimental_vector/vectorReserve` and
  `vectorPush` parity on the pointer-backed `.prime` storage path. Completed: experimental vector now grows via
  stdlib-owned allocate-and-move logic built on `take(...)` / `init(...)` instead of raw buffer reallocation, shared
  C++/VM/native conformance covers `vectorReserve`, `vectorPush`, and element preservation across non-trivial
  reserve-then-grow sequences, and both `vectorReserve` capacity overflow plus `vectorPush` growth-capacity overflow now
  route through shared deterministic runtime-contract paths instead of falling through to unchecked allocation growth.

**Group 6 - Map migration and compiler-path deletion**
Guiding notes:
- Read the remaining `map` items in this order: replace canonical helper routing with the real experimental `.prime`
  implementation, finish canonical constructor/type-surface migration, then delete the remaining compiler-owned
  semantics/IR/emitter paths. Completed parity, wrapper-routing, and fallback-removal checkpoints are archived in
  `docs/todo_finished.md`.
- **Helper-routing checkpoint:** canonical and wrapper-layer
  `/std/collections/map/count|contains|tryAt|at|at_unsafe` calls on experimental `Map<K, V>` value receivers, direct
  helper receivers built from canonical or wrapper constructor expressions, and borrowed `Reference<Map<K, V>>`
  receivers now route onto the real experimental `.prime` helper path. Keep new helper-routing work limited to concrete
  remaining parity gaps instead of reopening another wrapper-routing umbrella.
- **Constructor-routing checkpoint:** canonical and wrapper constructor spellings already rewrite onto
  `/std/collections/experimental_map/mapNew|mapSingle|mapDouble|mapPair|...` for explicit experimental destinations and
  the first layer of inferred experimental destinations.
- ◐ Canonical `map` constructor broader inferred-destination slice: finish moving helper-wrapped and control-flow-driven
  inferred returns, inferred struct fields, and default-parameter targets off `collections.prime` and onto the
  experimental `.prime` `Map<K, V>` constructor path.
- ◐ Canonical `map` constructor wrapped binding/assignment/receiver slice: finish moving helper-wrapped explicit and
  inferred local bindings, assignment RHS values, and direct helper/method receiver expressions onto the experimental
  `.prime` constructor path.
- ◐ Canonical `map` constructor `Result.ok(...)` destination slice: finish constructor rewriting through helper-wrapped
  `Result.ok(...)` payloads for explicit and inferred `Result<Map<K, V>, Error>` bindings, parameters, assignments, and
  field targets.
- ◐ Canonical `map` constructor storage/dereference-target slice: finish constructor rewriting for
  `init(uninitialized<T>, value)` storage targets, dereference-based assignment/init targets, dereferenced struct-field
  targets, and struct-field storage targets that should produce experimental `Map<K, V>` values.
- **Constructor migration rule:** do not keep another catch-all "remaining non-return destination" umbrella here. Add
  new constructor-routing TODOs only for concrete unreduced destination shapes with a reproducer; otherwise prefer the
  wrapper-bridge deletion and compiler-path removal milestones below.
- ○ Canonical `map` wrapper-bridge deletion slice: delete the wrapper-only `collections.prime` bridge for canonical
  `map<K, V>` construction and any remaining constructor-surface fallback once the experimental `.prime` constructor
  path has full parity.
- **Semantics checkpoint:** direct `/map/count|contains|tryAt|at|at_unsafe` compatibility spellings no longer inherit
  canonical `/std/collections/map/*` helpers, and bare root `count(values)`, `contains(values, key)`, `tryAt(values,
  key)`, and `at*(values, key)` calls on builtin `map<K, V>` targets now require imported canonical helpers or
  explicit same-path definitions instead of falling back to compiler-owned semantics.
- ◐ Map semantics bare-root helper cleanup slice: finish removing any remaining compiler-owned bare `count` / `contains`
  / `tryAt` / `at` / `at_unsafe` call classification paths on builtin `map<K, V>` targets once canonical stdlib parity
  is proven. Progress: bare-root `contains(values, key)`, `at(values, key)`, and `at_unsafe(values, key)` now match the
  canonical-helper contract in VM/native compile-run coverage, rejecting with deterministic `unknown call target:
  /std/collections/map/*` diagnostics unless the canonical helper is imported or explicitly defined, and bare
  `values.at(...)` / `values.at_unsafe(...)` method sugar now follows the same-path reject contract instead of falling
  back to compiler-owned access behavior.
- ◐ Map semantics compatibility-path cleanup slice: finish removing any remaining inherited
  `/map/count|contains|tryAt|at|at_unsafe` compatibility behavior so those spellings resolve only through explicit
  same-path definitions.
- **IR checkpoint:** direct `/map/count|contains|tryAt|at|at_unsafe` compatibility spellings now stop at IR setup
  return-kind resolution instead of inheriting canonical `/std/collections/map/*` return metadata when only canonical
  helpers exist.
- ◐ Map IR direct-helper fallback cleanup slice: finish removing direct-call builtin fallback and return-kind
  inheritance for explicit `/map/*` and `/std/collections/map/*` helper spellings.
- ◐ Map IR receiver-probe and slash-method cleanup slice: finish removing cross-path receiver metadata fallback for
  explicit helper receivers, direct `tryAt(...).method()` / `at(...).method()` probes, and slash-method forms so missing
  same-path helpers stay unresolved.
- ◐ Map IR reject-contract slice: lock the direct-definition, native-dispatch `NotHandled`, slash-method reject,
  receiver-kind reject, and compatibility-receiver reject contracts once the remaining fallback paths are gone.
- ◐ Map emitter bare-call and bare-method fallback cleanup slice: finish removing backend fallback for bare
  `count|contains|tryAt|at|at_unsafe` calls and method chains so emitters prefer canonical same-path helper metadata or
  reject cleanly instead of falling back to compiler-owned map behavior. Progress: the C++ emitter now rejects bare
  `at(values, key)` and `at_unsafe(values, key)` calls with same-path `unknown call target` diagnostics when canonical
  helpers are missing instead of compiling through the old builtin access fallback.
- ◐ Map emitter explicit slash-method and receiver-metadata cleanup slice: finish removing backend cross-path metadata
  fallback for explicit `/map/*` and `/std/collections/map/*` slash-method and receiver-probe forms.
- ◐ Map emitter deleted-helper stub cleanup slice: finish replacing the remaining builtin `ps_map_*` fallback emission
  paths with deleted-helper stubs or same-path rejects for missing canonical map helpers.

**Group 7 - Compatibility cleanup and vector runtime substrate**
Guiding notes:
- Completed canonical wrapper-routing and early no-import fallback-removal checkpoints are archived in
  `docs/todo_finished.md`.
- ◐ Remove the remaining compiler-owned `vector` explicit helper/method routing from emitters/backends beyond explicit
  canonical slash-method `count()` builtin emission fallback on map and array receivers, including broader expression
  builtin helper emission fallback. Progress: explicit canonical slash-method local bindings such as
  `values./std/collections/vector/count()` and `values./std/collections/vector/capacity()` on string, map, and array
  receivers now stop in semantics with deterministic same-path `unknown method` diagnostics when no same-path helper
  definition exists instead of reaching backend deleted-stub routing, bare `count(wrapperVector())` plus direct
  `/std/collections/vector/count(wrapperVector())` wrapper-temporary vector expressions without imported or declared
  same-path helpers now stop in semantics with deterministic `unknown call target: /std/collections/vector/count`
  diagnostics instead of reaching backend deleted-stub routing, bare `capacity(wrapperVector())` plus direct
  `/std/collections/vector/capacity(wrapperVector())` wrapper-temporary vector expressions without imported or declared
  same-path helpers now stop in semantics with deterministic `unknown call target: /std/collections/vector/capacity`
  diagnostics instead of reaching backend deleted-stub routing, bare `wrapperVector().count()` method expressions
  without imported or declared same-path helpers now stop in semantics with deterministic `unknown method:
  /vector/count` diagnostics instead of reaching backend deleted-stub routing, bare local-binding `values.count()`
  method expressions on builtin `vector<T>` receivers now also stop in semantics with deterministic `unknown method:
  /vector/count` diagnostics instead of reaching backend deleted-stub routing, bare `wrapperVector().capacity()` method
  expressions without imported or declared same-path helpers now stop in semantics with deterministic `unknown method:
  /vector/capacity` diagnostics instead of reaching backend deleted-stub routing, explicit canonical plus alias
  slash-method `wrapMap()./std/collections/vector/capacity()` and `wrapMap()./vector/capacity()` now also stop in
  semantics with same-path `unknown method` diagnostics instead of reaching backend deleted-stub routing, alias
  slash-method `wrapMap()./vector/count()` now also stops in semantics with same-path `unknown method: /vector/count`
  diagnostics instead of reaching backend deleted-stub routing, direct compatibility-alias wrapper calls
  `/vector/count(wrapperVector())` plus `/vector/capacity(wrapperVector())` now require same-path `/vector/*` helpers
  instead of reaching backend deleted-stub routing or inheriting canonical helper availability, canonical direct-call
  `/std/collections/vector/count(wrapperMap())`, `/std/collections/vector/count(wrapperText())`, and
  `/std/collections/vector/count(wrapperArray())` now also stop in semantics with same-path `unknown call target:
  /std/collections/vector/count` diagnostics instead of reaching deleted C++ helper stubs or the old wrapper-map
  template-argument detour, alias direct-call `/vector/count(wrapperText())` plus `/vector/count(wrapperArray())` now
  stop in semantics with same-path `unknown call target: /vector/count` diagnostics instead of reaching deleted C++
  helper stubs, alias slash-method `wrapperText()./vector/count()` plus `wrapperArray()./vector/count()` now stop in
  semantics with same-path `unknown method: /vector/count` diagnostics instead of reaching deleted C++ helper stubs, and
  VM/IR-lowered bare vector mutator expression shadows such as `return(push(values, 3i32))` now resolve through the same
  receiver-probing user-helper path as statement form instead of failing with backend-owned `vector helper` rejects.
- **Compatibility bridge rule:** the started slices below are temporary mixed-mode stabilizers while stdlib containers
  come online. Do not add new bridge-only TODOs unless they directly unblock experimental stdlib parity, canonical
  replacement with the real `.prime` containers, or deletion of a concrete compiler-owned collection path.
- **Deletion bar:** success here means canonical `vector`/`map` behavior lives in `.prime` plus minimal runtime
  substrate, not that every compatibility edge case gets its own long-lived C++ diagnostic path or that builtin
  collection knowledge grows inside C++.
- ◐ Vector/map compatibility-alias cleanup slice: finish helper alias classification cleanup so removed compatibility
  spellings never re-enter builtin fallback. Progress: lowerer/setup/count-access helper alias classifiers now keep
  removed `/array/*` compatibility spellings out of builtin classification, including `/array/capacity`, and
  `/array/vector(...)` now deterministically reports `unknown call target: /array/vector` instead of re-entering
  constructor fallback.
- ◐ Vector/map bridge reject-coverage slice: lock mutator/count/access direct-call rejects plus canonical-vs-alias
  precedence across semantics, IR validation, and VM/native/C++ suites.
- ◐ Vector/map bridge reject-coverage slice: lock method and slash-method reject coverage, including wrapper-returned
  receiver typing, across semantics, IR validation, and VM/native/C++ suites.
- ◐ Vector/map bridge reject-coverage slice: lock map value-result fallback rejects and downstream receiver-typing
  coverage so removed helper spellings cannot re-enter builtin paths indirectly.
- **Bridge parking rule:** do not split this bridge bucket further unless a specific failure blocks experimental stdlib
  bring-up, parity harness work, or deletion of an existing compiler-owned collection path.
- **Minimal vector substrate rule:** the remaining items below are only worth keeping insofar as they extract the
  runtime/error/allocation substrate that stdlib `.prime` `vector` needs; they are not goals to preserve
  compiler-owned `vector` semantics indefinitely.
- ◐ Collection runtime-contract slice: align builtin `capacity()` receiver validation with the documented fixed-size
  `array<T>` vs dynamic `vector<T>` contract while canonical helper routing is still mixed-mode. Progress: method-form
  `capacity()` on known collection receivers now routes through builtin validation, so non-vector targets emit
  deterministic `capacity requires vector target` diagnostics while user-defined `/array/capacity` helpers still take
  precedence.
- ◐ Collection runtime-contract slice: align builtin `push` receiver-shape diagnostics with the fixed-size `array<T>` vs
  dynamic `vector<T>` contract while canonical helper routing is still mixed-mode. Progress: builtin `push` now
  validates receiver shape before heap-effect checks, so fixed-size `array` targets deterministically report `push
  requires vector binding` in call and method forms.
- ◐ Collection runtime-contract slice: align builtin `reserve` receiver-shape diagnostics with the fixed-size `array<T>`
  vs dynamic `vector<T>` contract while canonical helper routing is still mixed-mode. Progress: builtin `reserve` now
  validates receiver shape before heap-effect checks, so fixed-size `array` targets deterministically report `reserve
  requires vector binding` in call and method forms.
- ◐ Collection runtime-contract slice: enforce deterministic numeric-argument validation for vector mutators while
  canonical helper routing is still mixed-mode. Progress: builtin `reserve`/`remove_at`/`remove_swap` now reject boolean
  capacities/indices in both call and method forms with deterministic `requires integer ...` diagnostics.
- ◐ Collection runtime-contract slice: stabilize named-argument diagnostic ordering and receiver probing for builtin
  mutator calls while canonical helper routing is still mixed-mode. Progress: expression-context mutator calls now
  prioritize the statement-only diagnostic even with named arguments, and statement-context named-argument calls
  validate receiver shape first while still finding valid vector receivers when `[values]` is absent.
- ◐ Dynamic vector-runtime substrate slice: lower VM/native vector locals through `count/capacity/data_ptr` records
  instead of fixed-capacity frame layouts. Progress: vector locals now lower through `count/capacity/data_ptr` records.
- ◐ Dynamic vector-runtime substrate slice: emit deterministic compile-time overflow/limit diagnostics for `reserve`
  expressions. Progress: compile-time `reserve` diagnostics now fold signed/unsigned `plus`/`minus`/`negate` literal
  expressions and report deterministic overflow/limit errors.
- ◐ Dynamic vector-runtime substrate slice: replace `push` growth overflow abort behavior with deterministic
  allocation-failure runtime semantics. Progress: runtime `push` growth overflow paths now emit deterministic
  allocation-failure diagnostics.
- ◐ Dynamic vector-runtime substrate slice: replace `reserve` growth overflow abort behavior with deterministic
  allocation-failure runtime semantics. Progress: runtime `reserve` growth overflow paths now emit deterministic
  allocation-failure diagnostics.
- ◐ Dynamic vector heap-backing substrate slice: route VM/native vector literal `data_ptr` storage to runtime heap
  allocation instead of frame-local slots. Progress: vector literal lowering now allocates `data_ptr` storage via
  runtime `HeapAlloc`.
- ◐ Dynamic vector heap-backing substrate slice: route `reserve` growth reallocation paths through runtime heap
  allocation instead of frame-local max-capacity slots. Progress: `reserve` growth now reallocates element storage
  through runtime heap allocations.
- ◐ Dynamic vector heap-backing substrate slice: route `push` growth copy/reallocation paths through runtime heap
  allocation instead of frame-local max-capacity slots. Progress: `push` growth now reallocates and copies element
  storage through runtime heap allocations.

**Group 8 - SoA de-builtinization**
- ◐ `soa_vector<T>` freeze/stabilize slice: keep the current empty/header-only construction, `count`, validation, and
  existing helper-precedence behavior working while the new path lands, but do not expand compiler-owned `soa_vector`
  surface area unless it directly unblocks the items below.
- ◐ Lock the v1 `soa_vector` contract before more implementation work: construction forms, `count`, `get`, `ref`,
  field-view indexing shape, explicit `to_soa` / `to_aos`, and invalidation boundaries after structural mutation.
  Progress: semantic validation now emits deterministic indexing-shape diagnostics (`soa_vector field views require
  value.<field>()[index] syntax: <field>`) for invalid direct field-view argument forms such as `values.field(i)` and
  `field(values, i)` instead of a generic argument-reject message, unresolved call-form field-view index attempts
  (`field(values, i)`) now emit that same deterministic syntax diagnostic instead of `unknown call target`, and builtin
  `get`/`ref` method forms now reject named arguments through the same deterministic `named arguments not supported for
  builtin calls` contract as call form.
- ○ Land the minimum compile-time struct-field introspection/codegen needed for one `.prime` `soa_vector` implementation
  to derive SoA column schemas from `T`.
- ○ Land the minimum column-storage substrate needed for one `.prime` `soa_vector` implementation: multi-column
  alloc/grow/free/read/write primitives with deterministic allocation-failure behavior.
- ○ Land the minimum borrowed-view / invalidation substrate needed for one `.prime` `soa_vector` implementation so
  `ref(...)` and field views have language-level semantics independent of compiler-owned `soa_vector` paths.
- ○ Add experimental stdlib `/std/collections/experimental_soa_vector/*` early with empty construction, `count`,
  SoA-safe type validation, and the smallest `.prime` storage/runtime path that exercises the new substrate.
- ○ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to non-empty literal construction, `push`,
  `reserve`, `get`, `ref`, and explicit `to_soa` / `to_aos`, adding only the next substrate pieces that the `.prime`
  implementation proves it needs.
- ○ Extend experimental stdlib `/std/collections/experimental_soa_vector/*` to field-view indexing (`value.field()[i]`)
  once the experimental `.prime` implementation reaches that boundary and the next minimal substrate slice is clear.
- ○ Route canonical `/std/collections/soa_vector/*` names through the real experimental `.prime` implementation once
  parity is proven.
- ○ Delete compiler-owned `soa_vector` helper routing and backend special cases from semantics, IR lowering, emitters,
  and runtime code until no C++ source mentions `soa_vector`.

**Section - Maintainability / File-size refactors**
Guiding note: keep production source under `src/` below roughly 700 lines when practical. The items below track
oversized production files under `src/` as behavior-preserving refactors; prefer extracting coherent helper units over
purely mechanical file splits.

**Group 9 - Test layout / compile units**
Guiding notes:
- The `tests/` tree is still heavily include-composed today: most test files are headers, and major doctest binaries are
  assembled from a few `.cpp` drivers that `#include` many header-based test fragments.
- Prefer explicit test translation units for actual doctest case definitions. Keep headers only for reusable harnesses,
  conformance helpers, and stable testing APIs.
- When converting these suites, update CMake target source lists and managed suite wiring so test discovery and
  `--test-suite=` coverage stay stable.
- As test helpers stabilize, prefer moving reusable helper declarations under `include/primec/testing/` instead of
  keeping long-lived doctest case bodies in `tests/unit/*.h`.
- ○ Refactor `PrimeStruct_backend_tests` away from include-composed doctest fragments by converting the
  `test_compile_run.cpp` and `test_ir_pipeline.cpp` header-included suites into explicit `tests/unit/*.cpp` compile
  units. Keep only shared conformance/harness helpers in headers.
- ○ Refactor `PrimeStruct_semantics_tests` away from include-composed doctest fragments by converting
  `test_semantics.cpp` header-included suites into explicit `tests/unit/*.cpp` compile units. Keep only reusable
  semantics/testing helpers in headers or `include/primec/testing/`.
- ○ Refactor parser-facing include-composed doctest fragments by converting the `test_parser_basic.cpp` and
  `test_parser_errors_more.cpp` header-included suites into explicit `tests/unit/*.cpp` compile units and leaving only
  helper utilities in headers.

**Group 10 - Focused refactor slices**
- ◐ Refactor `src/semantics/TemplateMonomorph.cpp` (`2355` lines) below the `700`-line target. Progress: extracted
  templated fallback type-inference helpers into `src/semantics/TemplateMonomorphFallbackTypeInference.h`, method-call
  target resolution helpers into `src/semantics/TemplateMonomorphMethodTargets.h`, call/return binding inference into
  `src/semantics/TemplateMonomorphBindingCallInference.h`, block-bodied binding inference into
  `src/semantics/TemplateMonomorphBindingBlockInference.h`, type-string resolution plus transform/callee-path rewriting
  helpers into `src/semantics/TemplateMonomorphTypeResolution.h`, stdlib collection helper template inference into
  `src/semantics/TemplateMonomorphCollectionHelperInference.h`, assignment-target resolution helpers into
  `src/semantics/TemplateMonomorphAssignmentTargetResolution.h`, experimental collection argument rewrite helpers into
  `src/semantics/TemplateMonomorphExperimentalCollectionArgumentRewrites.h`, canonical experimental collection
  constructor rewrites into `src/semantics/TemplateMonomorphExperimentalCollectionConstructorRewrites.h`, experimental
  collection target-value rewrites into `src/semantics/TemplateMonomorphExperimentalCollectionTargetValueRewrites.h`,
  experimental collection value-rewrite helpers into
  `src/semantics/TemplateMonomorphExperimentalCollectionValueRewrites.h`, experimental collection
  receiver/template-resolution helpers into `src/semantics/TemplateMonomorphExperimentalCollectionReceiverResolution.h`,
  shared experimental collection constructor path rewrites into
  `src/semantics/TemplateMonomorphExperimentalCollectionConstructorPaths.h`, return-path constructor rewrites into
  `src/semantics/TemplateMonomorphExperimentalCollectionReturnRewrites.h`, return-plan setup helpers into
  `src/semantics/TemplateMonomorphExperimentalCollectionReturnSetup.h`, definition binding setup helpers into
  `src/semantics/TemplateMonomorphDefinitionBindingSetup.h`, definition return orchestration helpers into
  `src/semantics/TemplateMonomorphDefinitionReturnOrchestration.h`, definition experimental collection rewrite helpers
  into `src/semantics/TemplateMonomorphDefinitionExperimentalCollectionRewrites.h`, execution rewrite entrypoint helpers
  into `src/semantics/TemplateMonomorphExecutionRewrites.h`, top-level definition rewrite entrypoint helpers into
  `src/semantics/TemplateMonomorphDefinitionRewrites.h`, and template-specialization family helpers into
  `src/semantics/TemplateMonomorphTemplateSpecialization.h`.
- **TemplateMonomorph next-slice rule:** the remaining file mass is now concentrated in the pre-include core plus the
  top-level orchestration around helper-overload, import-alias, and template-root setup. Split further work along
  those seams instead of keeping one generic size-reduction epic.
- ◐ Refactor `src/semantics/TemplateMonomorph.cpp` core type/helper utilities below the `700`-line target: move builtin
  type/collection classification, overload-path helpers, and path/import utility code into dedicated units. Progress:
  extracted the shared non-type-transform classification, builtin collection normalization/classification, import-path
  coverage helper, helper-overload path utilities, and whitespace helpers into
  `src/semantics/TemplateMonomorphCoreUtilities.h`.
- ◐ Refactor `src/semantics/TemplateMonomorph.cpp` source-definition and template-root setup below the `700`-line
  target: move source-def collection, overload-family expansion, and occupied-path validation out of the root file.
  Progress: extracted source definition collection, helper-overload family expansion, occupied-path conflict validation,
  and template-root entry-path validation into `src/semantics/TemplateMonomorphSourceDefinitionSetup.h`.
- ◐ Refactor `src/semantics/TemplateMonomorph.cpp` final monomorphize orchestration below the `700`-line target: move
  import-alias setup plus definition/execution rewrite coordination into dedicated units, leaving a thin
  `monomorphizeTemplates(...)` entrypoint. Progress: extracted import-alias setup and definition/execution rewrite
  coordination helpers into `src/semantics/TemplateMonomorphFinalOrchestration.h`, leaving `monomorphizeTemplates(...)`
  as orchestration glue.

**Group 11 - Remaining oversized-file queue**
- ○ Refactor `src/ir_lowerer/IrLowererSetupTypeHelpers.cpp` (`1989` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererLowerInferenceSetup.cpp` (`1976` lines) below the `700`-line target.
- ○ Refactor `src/semantics/SemanticsHelpersCore.cpp` (`1973` lines) below the `700`-line target.
- ○ Refactor `src/emitter/EmitterHelpersBuiltins.cpp` (`1695` lines) below the `700`-line target.
- ○ Refactor `src/native_emitter/NativeEmitterInternals.h` (`1619` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererFlowHelpers.cpp` (`1568` lines) below the `700`-line target.
- ○ Refactor `src/emitter/EmitterExprCalls.h` (`1512` lines) below the `700`-line target.
- ○ Refactor `src/emitter/EmitterEmitSetup.h` (`1458` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.cpp` (`1427` lines) below the `700`-line
  target.
- ○ Refactor `src/IrToCppEmitter.cpp` (`1421` lines) below the `700`-line target.
- ○ Refactor `src/parser/ParserCore.cpp` (`1289` lines) below the `700`-line target.
- ○ Refactor `src/parser/ParserLists.cpp` (`1245` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererStatementBindingHelpers.cpp` (`1190` lines) below the `700`-line target.
- ○ Refactor `src/VmDebugDap.cpp` (`1104` lines) below the `700`-line target.
- ○ Refactor `src/parser/ParserExpr.cpp` (`1083` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererStructTypeHelpers.cpp` (`1058` lines) below the `700`-line target.
- ○ Refactor `src/text_filter/TextFilterPipelinePass.cpp` (`1037` lines) below the `700`-line target.
- ○ Refactor `src/ImportResolver.cpp` (`1010` lines) below the `700`-line target.
- ○ Refactor `src/native_emitter/NativeEmitterHelpers.cpp` (`991` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.cpp` (`953` lines) below the `700`-line target.
- ○ Refactor `src/IrPrinter.cpp` (`943` lines) below the `700`-line target.
- ○ Refactor `src/semantics/SemanticsValidator.cpp` (`938` lines) below the `700`-line target.
- ✓ Refactor `src/ir_lowerer/IrLowererUninitializedTypeHelpers.cpp` (`924` lines) below the `700`-line target by
  extracting entry/runtime setup-builder orchestration into `src/ir_lowerer/IrLowererUninitializedSetupBuilders.cpp` and
  struct inference helpers into `src/ir_lowerer/IrLowererUninitializedStructInference.cpp`, leaving
  `IrLowererUninitializedTypeHelpers.cpp` below target while preserving the existing helper APIs.
- ○ Refactor `src/text_filter/TextFilterPipelineEnvelope.cpp` (`906` lines) below the `700`-line target.
- ○ Refactor `src/primevm_main.cpp` (`868` lines) below the `700`-line target.
- ○ Refactor `src/emitter/EmitterHelpersTypes.cpp` (`857` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.cpp` (`828` lines) below the `700`-line target.
- ○ Refactor `src/native_emitter/NativeEmitterEmit.cpp` (`822` lines) below the `700`-line target.
- ✓ Refactor `src/ir_lowerer/IrLowererSetupInferenceHelpers.cpp` (`514` lines) below the `700`-line target by extracting the call return-kind classifiers into `src/ir_lowerer/IrLowererSetupInferenceReturnKinds.cpp`, leaving `IrLowererSetupInferenceHelpers.cpp` focused on pointer, buffer, array, and body/local inference helpers.
- ○ Refactor `src/ir_lowerer/IrLowererResultHelpers.cpp` (`805` lines) below the `700`-line target.
- ✓ Refactor `src/IrToGlslEmitter.cpp` (`46` lines) below the `700`-line target by extracting GLSL instruction/function emission helpers into `src/IrToGlslEmitterFunctionEmitter.cpp`, leaving `IrToGlslEmitter.cpp` focused on top-level shader source assembly.
- ✓ Refactor `src/text_filter/TextFilterHelpers.cpp` (`537` lines) below the `700`-line target by extracting unary rewrite helpers into `src/text_filter/TextFilterUnaryRewrites.cpp`, leaving `TextFilterHelpers.cpp` focused on token classification and operand-scanning helpers.
- ○ Refactor `src/glsl_emitter/GlslEmitterExpr.cpp` (`788` lines) below the `700`-line target.
- ✓ Refactor `src/semantics/SemanticsHelpersValidation.cpp` (`618` lines) below the `700`-line target by extracting argument-ordering and named-argument validation helpers into `src/semantics/SemanticsArgumentOrdering.cpp`, leaving `SemanticsHelpersValidation.cpp` focused on transform/default-expression/type-inference validation helpers.
- ○ Refactor `src/emitter/EmitterEmitBody.h` (`781` lines) below the `700`-line target.
- ○ Refactor `src/ir_lowerer/IrLowererStatementCallHelpers.cpp` (`768` lines) below the `700`-line target.
- ✓ Refactor `src/ir_lowerer/IrLowererHelpers.cpp` (`338` lines) below the `700`-line target by extracting builtin math/name classification helpers into `src/ir_lowerer/IrLowererBuiltinNameHelpers.cpp`, leaving `IrLowererHelpers.cpp` focused on shared control-flow, match-lowering, and core builtin classification helpers.
- ○ Refactor `src/ir_lowerer/IrLowererLowerEmitExpr.h` (`735` lines) below the `700`-line target.
- ✓ Refactor `src/ir_lowerer/IrLowererCallHelpers.h` (`654` lines) below the `700`-line target by extracting call-dispatch/access-target result enums plus map-lookup loop helper structs into `src/ir_lowerer/IrLowererCallHelperTypes.h`, leaving `IrLowererCallHelpers.h` focused on call-helper function declarations.

**Section - Architecture / Type-resolution graph**
Guiding note: the finished graph/SCC parity and snapshot work above now needs to turn back into a short explicit queue.
Keep new work grouped into broader migration slices instead of inventing more one-off snapshot micro-follow-ups.

**Group 12 - Near-term graph queue**
Guiding note: blocked by Group 14 rollout constraints until the remaining collection-helper/runtime predecessor items
are finished and the return-kind pilot path is stable enough to resume broader graph work.
- ○ Expand end-to-end graph conformance coverage for local `auto`, query, and `try(...)` consumers on the single
  graph-resolver path so the current snapshot-heavy checks are backed by real compile-pipeline scenarios.
- ○ Expand end-to-end graph conformance coverage for backend dispatch, deleted-path diagnostics, and canonical
  helper/method resolution on C++/VM/native so graph-solved metadata is exercised beyond the snapshot harness.
- ○ Land graph-backed query invalidation rules and coverage for local-binding, control-flow, and initializer-shape edits
  so cached query/binding/result metadata has one explicit contract for intra-definition churn.
- ○ Land graph-backed query invalidation rules and coverage for definition-signature, import-alias, and receiver-type
  edits before more inference consumers depend on the cache.
- ○ Migrate the next remaining non-template inference islands onto graph-backed query/binding state instead of leaving
  mixed ad hoc inference paths around the stabilized return/query/local pilot.
- ○ Define and validate graph interaction with CT-eval consumers so compile-time evaluation either consumes the shared
  dependency state directly or keeps one explicit boundary instead of implicit drift.
- ○ Migrate explicit and implicit template inference dependencies onto the graph-backed path once the non-template
  inference islands and invalidation rules are pinned, so template solving stops being a deferred side-system.
- ○ Broaden omitted-envelope and local-auto inference beyond the current first-step slices only after the next
  inference-island migrations prove stable on the graph path.
- ○ Add graph performance guardrails and sustained perf coverage before optional parallel solve work so future graph
  expansion has a deterministic regression budget.
- ○ Prototype optional parallel solve only after the single-threaded graph path, invalidation rules, CT-eval boundary,
  and performance guardrails are all stable.

**Group 13 - Semantics/lowering boundary**
- ○ Define a first-class `SemanticProgram` or `ResolvedModule` boundary type that captures all lowering-required
  semantic facts and document its ownership/lifetime contract.
- ○ Add a first semantic-product builder slice that materializes resolved call targets, binding types,
  effects/capabilities, and struct metadata from `SemanticsValidator`.
- ○ Add a second semantic-product builder slice that moves graph-backed local `auto`, query, `try(...)`, and `on_error`
  metadata out of test-only snapshot plumbing and into the semantic product.
- ○ Thread the semantic product through CLI/runtime plumbing (`primec`, `primevm`, dump-stage handling, and
  failure/report paths) so the new semantics boundary is carried end-to-end outside the core compile/lower APIs.
- ○ Add a temporary migration adapter that can derive lowerer input from either raw `Program` or the semantic product
  during the cutover, with explicit removal criteria once all lowering entrypoints use the new boundary.
- ○ Cut over `CompilePipelineOutput` and `Semantics::validate` so the compile pipeline publishes the semantic product as
  its post-semantics success artifact instead of only a mutated raw `Program`.
- ○ Cut over `IrLowerer::lower` and `prepareIrModule` so IR preparation consumes the semantic product directly, then
  retire the raw-`Program` lowering path once the temporary adapter is removed.
- ○ Add a deterministic semantic-product dump/formatter plus golden coverage so the new boundary is inspectable in the
  same way as AST/IR stages.
- ○ Document and test the ownership split between raw AST and semantic product, especially for source spans,
  debug/source-map provenance, and other syntax-faithful data that lowering/debuggers still need.
- ○ Switch `IrLowerer` entry setup to consume resolved call targets from the semantic product instead of re-deriving
  them from AST state.
- ○ Switch lowerer type/binding setup to consume semantic-product binding metadata instead of repeating helper/type
  inference logic.
- ○ Switch lowerer effect/capability and struct-layout setup to consume semantic-product metadata instead of re-reading
  AST/transforms for those facts.
- ○ Add a narrow semantic-product unit/golden suite that asserts exported lowering facts directly instead of only
  through snapshot helpers.
- ○ Add end-to-end compile-pipeline conformance cases that exercise the semantic-product boundary through C++/VM/native
  lowering paths.
- ○ Migrate tests and public testing helpers from `primec/testing/SemanticsValidationHelpers.h` snapshot surfaces onto
  the semantic-product inspection surface where they are asserting lowering-facing facts.
- ○ Delete redundant testing-only semantic snapshot plumbing once equivalent semantic-product dumps and conformance
  coverage exist, leaving one canonical inspection surface for lowering-facing semantic facts.
- ○ Delete lowerer-side stdlib/helper alias fallback paths once equivalent canonical resolution is provided by the
  semantic product.
- ○ Replace `SemanticsValidator`'s shared mutable `error_` flow with a structured diagnostic sink in the current
  single-threaded path while preserving deterministic first-error behavior.
- ○ Attach stable semantic-product node identities or sort keys to diagnostics so later parallel validation can merge
  and order them deterministically.
- ○ Split per-definition validation state from validator-global caches so semantic-product construction uses explicit
  contexts rather than shared mutable fields.
- ○ Update CLI help, dump-stage docs/spec text, and pipeline-facing tests to document the semantic-product inspection
  surface and its relation to existing AST/IR dumps.
- ○ Add a staged migration note in `docs/PrimeStruct.md` for the semantics-to-lowering boundary, including exit criteria
  for removing AST-dependent lowerer logic.

**Group 14 - Rollout constraints**
Guiding notes:
- Keep the graph migration blocked on the remaining collection-helper/runtime predecessor items so solver work does not
  target moving call-target semantics. Near-term graph work should stop at a return-kind pilot plus parity; query,
  CT-eval, parallelization, and broad inference migration stay deferred until that path is stable.
- The graph builder, deterministic dump, return-kind pilot, default-on flip, and temporary rollback path are complete.
  Remaining graph work should build directly on the single default graph resolver; do not reintroduce a second
  resolver mode while expanding template/local inference, query/invalidation work, CT-eval consumers, or optional
  parallel solve.
