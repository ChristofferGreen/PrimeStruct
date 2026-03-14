# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items were moved to `docs/todo_finished.md` on March 13, 2026.

**Types & Semantics**
Collection strategy note: keep `array` as a language-core collection, but migrate `vector` and `map` to stdlib `.prime` implementations so users can build equivalent containers. Progress is tracked by the slices below plus the archived finished slice bullets in `docs/todo_finished.md`.
Vector/map de-builtinization roadmap note: prioritize the substrate, parity harness, and stdlib bring-up work that deletes compiler-owned collection behavior. Bridge cleanup is only worth tracking when it directly unblocks `vector.prime` / `map.prime` bring-up or removal of a concrete semantics/lowering/emitter special-case.
Stdlib container substrate note: the items below are the hard prerequisites for moving `vector`/`map` into self-contained `.prime` implementations instead of extending compiler-owned collection behavior.
- ○ Ownership/drop resize/remove slice: define indexed `remove_at` / `remove_swap` semantics for non-trivial vector elements after the removed-element drop + compaction contract is fixed.
- ○ Ownership/drop reallocation slice: define non-trivial element move/rehash semantics for stdlib-owned vector/map storage once experimental containers stop forwarding to builtin runtimes.
- ○ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed.
- ○ Map runtime-contract slice: teach semantics and backend packing to accept additional concrete payload families now that `Result<T, Error>` payload-kind metadata survives return/local inference and `try(...)` resolution.
- ○ Replace compiler-injected vector index/pop runtime abort paths with the standard container error contract across VM/native/C++ flows.
Stdlib container bring-up note: land a real `.prime` implementation under an experimental path/name first, prove parity there, then swap canonical names and remove the builtin implementation.
- ○ Implement an experimental stdlib `vector` in `.prime` under a temporary path/name so conformance can advance without blocking on canonical-name migration.
- ○ Experimental stdlib `map` storage/runtime slice: land a `.prime`-owned storage/runtime path for `/std/collections/experimental_map/*` once pointer/ownership substrate lands.
- ○ Experimental stdlib `map` helper-surface slice: route `/std/collections/experimental_map/*` helpers through the `.prime` implementation instead of compiler-owned behavior once the storage/runtime path exists.
- ○ Shared collection conformance harness slice: add reusable backend coverage for builtin and experimental-stdlib `vector`/`map` helper parity.
- ○ Switch canonical `/std/collections/vector/*` helpers to the stdlib implementation once vector parity is proven.
- ○ Switch canonical `/std/collections/map/*` helpers to the stdlib implementation once map parity is proven.
- ○ Remove compiler-owned `map` behavior from semantics once canonical stdlib `map` parity is proven.
- ○ Remove compiler-owned `map` behavior from IR lowering once canonical stdlib `map` parity is proven.
- ○ Remove compiler-owned `map` behavior from emitters/backends once canonical stdlib `map` parity is proven.
- ○ Remove remaining compiler-owned `vector` behavior from semantics/lowering/emitter after canonical stdlib `vector` parity is proven.
Compatibility bridge note: the started slices below are temporary mixed-mode stabilizers while stdlib containers come online. Do not add new bridge-only TODOs unless they directly unblock experimental stdlib bring-up or removal of a concrete compiler-owned collection path.
Map stdlib-forwarding rollout note: success here means `vector`/`map` behavior lives in `.prime` plus minimal runtime substrate, not that every compatibility edge case gets its own long-lived C++ diagnostic path.
- ◐ Vector/map compatibility-alias cleanup slice: finish helper alias classification cleanup so removed compatibility spellings never re-enter builtin fallback. Progress: lowerer/setup/count-access helper alias classifiers now keep removed `/array/*` compatibility spellings out of builtin classification, including `/array/capacity`, and `/array/vector(...)` now deterministically reports `unknown call target: /array/vector` instead of re-entering constructor fallback.
- ◐ Vector/map bridge reject-coverage slice: keep mixed builtin+stdlib compatibility paths on deterministic builtin or canonical diagnostics instead of forwarding through removed helper spellings. Progress: started coverage now locks mutator/count/access, direct vs method vs slash-method access, wrapper-returned receiver typing, canonical-vs-alias precedence, and map value-result fallback behavior across semantics, IR validation, and VM/native/C++ suites.
Bridge parking note: do not split this bridge bucket further unless a specific failure blocks experimental stdlib bring-up, parity harness work, or deletion of an existing compiler-owned collection path.
- ◐ Collection runtime-contract slice: align builtin `capacity()` receiver validation with the documented fixed-size `array<T>` vs dynamic `vector<T>` contract. Progress: method-form `capacity()` on known collection receivers now routes through builtin validation, so non-vector targets emit deterministic `capacity requires vector target` diagnostics while user-defined `/array/capacity` helpers still take precedence.
- ◐ Collection runtime-contract slice: align builtin `push` receiver-shape diagnostics with the fixed-size `array<T>` vs dynamic `vector<T>` contract. Progress: builtin `push` now validates receiver shape before heap-effect checks, so fixed-size `array` targets deterministically report `push requires vector binding` in call and method forms.
- ◐ Collection runtime-contract slice: align builtin `reserve` receiver-shape diagnostics with the fixed-size `array<T>` vs dynamic `vector<T>` contract. Progress: builtin `reserve` now validates receiver shape before heap-effect checks, so fixed-size `array` targets deterministically report `reserve requires vector binding` in call and method forms.
- ◐ Collection runtime-contract slice: enforce deterministic numeric-argument validation for vector mutators. Progress: builtin `reserve`/`remove_at`/`remove_swap` now reject boolean capacities/indices in both call and method forms with deterministic `requires integer ...` diagnostics.
- ◐ Collection runtime-contract slice: stabilize named-argument diagnostic ordering and receiver probing for builtin mutator calls. Progress: expression-context mutator calls now prioritize the statement-only diagnostic even with named arguments, and statement-context named-argument calls validate receiver shape first while still finding valid vector receivers when `[values]` is absent.
- ◐ Dynamic vector-runtime slice: lower VM/native vector locals through `count/capacity/data_ptr` records instead of fixed-capacity frame layouts. Progress: vector locals now lower through `count/capacity/data_ptr` records.
- ◐ Dynamic vector-runtime slice: emit deterministic compile-time overflow/limit diagnostics for `reserve` expressions. Progress: compile-time `reserve` diagnostics now fold signed/unsigned `plus`/`minus`/`negate` literal expressions and report deterministic overflow/limit errors.
- ◐ Dynamic vector-runtime slice: replace `push` growth overflow abort behavior with deterministic allocation-failure runtime semantics. Progress: runtime `push` growth overflow paths now emit deterministic allocation-failure diagnostics.
- ◐ Dynamic vector-runtime slice: replace `reserve` growth overflow abort behavior with deterministic allocation-failure runtime semantics. Progress: runtime `reserve` growth overflow paths now emit deterministic allocation-failure diagnostics.
- ◐ Dynamic vector heap-backing slice: route VM/native vector literal `data_ptr` storage to runtime heap allocation instead of frame-local slots. Progress: vector literal lowering now allocates `data_ptr` storage via runtime `HeapAlloc`.
- ◐ Dynamic vector heap-backing slice: route `reserve` growth reallocation paths through runtime heap allocation instead of frame-local max-capacity slots. Progress: `reserve` growth now reallocates element storage through runtime heap allocations.
- ◐ Dynamic vector heap-backing slice: route `push` growth copy/reallocation paths through runtime heap allocation instead of frame-local max-capacity slots. Progress: `push` growth now reallocates and copies element storage through runtime heap allocations.
- ◐ `soa_vector<T>` runtime/backend slice: keep empty literals and builtin `count` lowering working with user-helper precedence across semantics, IR, and native flows. Progress: lowering now treats `soa_vector` as a collection builtin, lifts the old semantic hard rejection, supports header-only empty literals, routes builtin `count(...)` through native count lowering, and preserves user-defined `/soa_vector/count` precedence.
- ○ `soa_vector<T>` runtime/backend slice: implement non-empty literal materialization in VM/native backends with deterministic allocation failures.
- ○ `soa_vector<T>` runtime/backend slice: implement field-wise storage layout for `soa_vector<T>` in VM/native backends.
- ○ `soa_vector<T>` runtime/backend slice: implement builtin `push` lowering and execution instead of the current deterministic unsupported-helper diagnostics.
- ○ `soa_vector<T>` runtime/backend slice: implement builtin `reserve` lowering and execution instead of the current deterministic unsupported-helper diagnostics.
- ○ `soa_vector<T>` runtime/backend slice: define and enforce reallocation invalidation rules once builtin mutator support exists.
- ◐ `soa_vector<T>` access slice: keep `get`/`ref` semantic validation and user-helper precedence stable for call and method forms. Progress: semantics validates receiver/index requirements and rejects builtin named-argument forms, while lowering preserves user-defined `/soa_vector/get` and `/soa_vector/ref` precedence.
- ○ `soa_vector<T>` access slice: lower builtin `get` for supported backends instead of emitting deterministic unsupported diagnostics.
- ○ `soa_vector<T>` access slice: lower builtin `ref` for supported backends instead of emitting deterministic unsupported diagnostics.
- ○ `soa_vector<T>` access slice: implement field-view indexing (`value.field()[i]`) with canonical lowering and deterministic diagnostics/tests once field-view storage exists.
- ○ Add stdlib matrix types (`Mat2`, `Mat3`, `Mat4`) with constructors and component accessors.
- ○ Add stdlib `Quat` with constructors, component accessors, and normalization helpers.
- ○ Implement matrix/quaternion `plus` shape validation with deterministic diagnostics.
- ○ Implement matrix/quaternion `minus` shape validation with deterministic diagnostics.
- ○ Implement matrix/quaternion `multiply` allowlist validation with deterministic diagnostics.
- ○ Implement matrix/quaternion `divide` composite-by-scalar-only validation with deterministic diagnostics.
- ○ Implement `quat_to_mat3`.
- ○ Implement `quat_to_mat4`.
- ○ Implement `mat3_to_quat`.
- ○ Reject implicit matrix/quaternion family conversions with deterministic diagnostics.
- ○ Add VM support-matrix conformance tests for matrix/quaternion behavior, including accept/reject cases and deterministic diagnostics.
- ○ Add native support-matrix conformance tests for matrix/quaternion behavior, including accept/reject cases and deterministic diagnostics.
- ○ Add Wasm support-matrix conformance tests for matrix/quaternion behavior, including accept/reject cases and deterministic diagnostics.
- ○ Add C++ support-matrix conformance tests for matrix/quaternion behavior, including accept/reject cases and deterministic diagnostics.
- ○ Add GLSL support-matrix conformance tests for matrix/quaternion behavior and deterministic unsupported/reject diagnostics where features are unavailable.
- ○ Extend math conformance suite with quaternion reference tests, including Hamilton product and quaternion-vector rotation.
- ○ Extend math conformance suite with matrix/vector composition-order references and an explicit tolerance policy.

**Architecture follow-up (type-resolution dependency graph / SCC solver)**
Type-resolution graph rollout note: keep the graph migration blocked on the remaining collection-helper/runtime predecessor items so solver work does not target moving call-target semantics. Near-term graph work should stop at a return-kind pilot plus parity; query, CT-eval, parallelization, and broad inference migration stay deferred until that path is stable.
- ○ Add a short staged rollout note that names the implementation checkpoints (builder/dump, return-kind pilot, parity, template/local expansion, query and CT-eval follow-ups, legacy removal) and the rollback gates between them.
- ○ Add a short design note in `docs/PrimeStruct.md` defining the graph model and invariants: node kinds (definition return-kind, call-site type constraints, local `auto` constraints), edge kinds (dependency vs requirement), legal-cycle policy (SCC fixed-point) vs illegal-cycle policy (deterministic diagnostic), and deterministic ordering guarantees.
- ○ Introduce a new semantics-internal `TypeResolutionGraph` data model and builder (no behavior change): build nodes/edges from canonical AST after semantic transforms/template monomorphization, with stable node IDs and stable edge insertion order.
- ○ Add a deterministic graph dump hook for tests/tooling (`--dump-stage=type-graph` or equivalent) and snapshot tests that lock graph shape/order for representative programs (simple call chain, mutual recursion, namespace imports, templates).
- ○ Implement reusable SCC utilities (Tarjan/Kosaraju) in a standalone unit with unit tests for acyclic, single-cycle, multi-cycle, and disconnected graphs.
- ○ Implement a condensation-DAG builder on top of the SCC utilities, with deterministic ordering tests.
- ○ Land a first vertical solver slice for definition return-kind inference only: run SCC fixed-point over return constraints, preserve current diagnostics, and gate the new path behind a temporary opt-in flag (`--type-resolver=graph`).
- ○ Add parity harness tests that run legacy vs graph return-kind inference on the same corpus and assert identical diagnostics/IR for supported cases; include explicit expected-diff tests where legacy behavior is intentionally corrected.
- ○ Flip return-kind inference default to graph mode once parity is stable; keep a temporary fallback flag (`--type-resolver=legacy`) for rollback during stabilization.
Deferred graph follow-up note: explicit/implicit template inference, local `auto`, omitted-envelope inference, illegal-cycle diagnostics, performance guardrails, optional parallel solve, query/invalidation work, CT-eval interaction, remaining inference-island migration, sustained performance suites, legacy removal, and end-to-end graph conformance expansion stay parked until the return-kind graph path is stable.

**Backends & IR**
- ○ Add `/std/image/ppm/*` read implementations and conformance tests for VM/native/Wasm on top of `File<Read>.read_byte(...)`.
- ○ Add `/std/image/ppm/*` write implementations and conformance tests for VM/native/Wasm.
- ○ Add `/std/image/png/*` read implementations and conformance tests for VM/native/Wasm.
- ○ Add `/std/image/png/*` write implementations and conformance tests for VM/native/Wasm.
- ○ Add GLSL reject/diagnostic coverage for image file I/O.

**Web + Native + Metal 3D Target (Spinning Cube)**
