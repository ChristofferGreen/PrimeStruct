# PrimeStruct Spec ↔ Code TODO

Legend:
  ○ Not started
  ◐ Started
  ✓ Finished

Finished items were moved to `docs/todo_finished.md` on March 13, 2026.

**Types & Semantics**
Collection strategy note: keep `array` as a language-core collection, but migrate `vector` and `map` to stdlib `.prime` implementations so users can build equivalent containers. Progress is tracked by the slices below plus the archived finished slice bullets in `docs/todo_finished.md`.
Map stdlib-forwarding rollout note: the remaining work is split into the narrower slices below so each change can land and verify independently.
- ◐ Map stdlib-forwarding slice: finish constructor/access-helper fallback and precedence for canonical `/std/collections/map/*` spellings across semantics, template monomorphization, and IR setup-type resolution. Progress: canonical `/std/collections/map/*` spellings now reverse-fallback to explicit `/map/*` helper definitions for constructor/access helpers, including templated constructor calls and non-builtin helper arities.
- ◐ Map stdlib-forwarding slice: finish slashless/canonical map path normalization across IR lowering lookup and C++ emitter method/struct metadata resolution. Progress: slashless `std/collections/map/*` and `map/*` aliases now normalize before call-path lookup, receiver/type-name matching, struct-layout lookup, and method-path emission.
- ◐ Map stdlib-forwarding slice: finish conformance coverage for map helper precedence, template inference, and deterministic mismatch/unknown-target diagnostics across semantics, VM/native, and C++ emitter flows. Progress: focused regressions now cover `at`/`at_unsafe` wrapper fallback, canonical access-helper alias precedence, templated `/std/collections/map/count(...)` `<K, V>` inference from receiver types, and deterministic mismatch diagnostics.
- ◐ Vector/map compatibility-alias cleanup slice: finish collapsing setup-inference/call-return fast paths onto shared alias-aware collection handling so only compatibility aliases remain special. Progress: IR setup-inference call-return wiring now reuses shared namespaced collection receiver-probe gating for vector/map alias+stdlib helper calls, with focused regressions for `/map/count` compatibility alias fallback and unresolved-without-definition behavior.
- ◐ Vector/map compatibility-alias cleanup slice: finish lowerer helper alias classification cleanup so removed helper spellings never re-enter builtin fallback. Progress: IR lowerer call/setup-type/count-access helper alias classifiers now consistently treat removed `/array/*` vector helper spellings as non-builtin, including `/array/capacity`.
Compatibility-alias removal note: `/array/vector(...)` constructor compatibility alias classification is removed across semantics/IR-lowerer/C++-emitter helper paths, so `/array/vector` now deterministically reports `unknown call target: /array/vector`. The remaining alias removals are tracked by the narrower de-builtinization slices below.
Vector helper-alias removal note: the remaining `/array/*` helper removals are split into the slices below.
- ◐ Vector helper-alias removal slice: remove the remaining `/array/count` compatibility spelling after stdlib-only conformance replacements land. Progress: `/array/capacity` compatibility alias classification is removed across semantics/IR-lowerer/C++-emitter helper paths, so `/array/capacity(...)` now deterministically reports `unknown call target: /array/capacity` while `/array/count` compatibility remains.
- ○ Vector helper-alias removal slice: remove the remaining `/array/at` compatibility spelling after stdlib-only conformance replacements land.
- ○ Vector helper-alias removal slice: remove the remaining `/array/at_unsafe` compatibility spelling after stdlib-only conformance replacements land.
Vector reject-coverage note: the remaining `/vector/*` compatibility-forwarding replacements are split into the narrower reject-coverage slices below.
- ◐ Vector reject-coverage slice: convert the mutator compatibility-forwarding cluster to active reject diagnostics across semantics, IR validation, and VM/native/C++ compile-run suites. Progress: the first mutator forwarding skip cluster is now active reject coverage with naming aligned to nearby reject suites.
- ◐ Vector reject-coverage slice: convert the count/access compatibility-forwarding cluster to active reject diagnostics across semantics, IR validation, and VM/native/C++ compile-run suites. Progress: the first count/access forwarding skip cluster is now active reject coverage with naming aligned to nearby reject suites.
- ◐ Vector reject-coverage slice: convert reordered namespaced call/call-expression compatibility-forwarding cases to deterministic rejects. Progress: reordered namespaced `/vector/push` compatibility-forwarding cases now reject explicitly, and namespaced vector capacity named-argument reject coverage is unskipped.
- ◐ Vector reject-coverage slice: convert block/body-argument compatibility-forwarding cases to deterministic rejects. Progress: block-argument target resolution preserves removed `/vector/*` spellings, and statement/expression body-argument `/vector/count` cases now run as active reject diagnostics.
- ◐ Vector reject-coverage slice: convert explicit slash method-spelling compatibility forms to deterministic unknown-target rejects. Progress: explicit slash method-spelling aliases (`values./array/count(...)`, `values./vector/count(...)`, `values./std/collections/vector/count(...)`) now preserve removed alias paths and reject deterministically.
- ◐ Vector reject-coverage slice: convert namespaced compatibility-shadow forms to deterministic unknown-target rejects. Progress: std-namespaced reordered mutator compatibility-shadow paths reject consistently across semantics + VM/native/C++ suites.
- ◐ Vector reject-coverage slice: convert canonical helper precedence/template-forwarding compatibility cases to stdlib-only reject diagnostics. Progress: template monomorph explicit-template path selection now rejects removed `/vector/*` helper spellings when no compatibility definition exists, semantics + VM/native/C++ coverage now asserts deterministic reject diagnostics for templated precedence forms, IR setup-type helper tests actively reject canonical-only direct/slashless `/vector/count` return-kind probes, and the old canonical helper method-precedence coverage now runs as reject diagnostics (`argument count mismatch for builtin count`).
- ○ Vector reject-coverage slice: convert the remaining struct-return helper forwarding compatibility cluster to stdlib-only reject/diagnostic coverage across semantics, IR validation, and VM/native/C++ compile-run suites.
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
- ○ Define the `/std/intrinsics/memory/*` API surface in docs and semantics, including exact `effects(heap_alloc)` gating requirements.
- ○ Implement `/std/intrinsics/memory/*` lowering/runtime plumbing for `alloc` on supported backends.
- ○ Implement `/std/intrinsics/memory/*` lowering/runtime plumbing for `free` on supported backends.
- ○ Implement `/std/intrinsics/memory/*` lowering/runtime plumbing for `realloc` (or the chosen growth primitive) on supported backends.
- ○ Define safe pointer element access intrinsics, including bounds behavior and deterministic diagnostics.
- ○ Define unsafe pointer element access intrinsics and backend lowering rules for relocation/growth code paths.
- ○ Add safe pointer-element-access conformance tests so `.prime` containers can implement checked indexing and growth without compiler special-cases.
- ○ Add unsafe pointer-element-access conformance tests so `.prime` containers can implement relocation and unchecked access without compiler special-cases.
- ○ Define explicit ownership/drop semantics for container element lifecycles during resize/remove operations for non-trivial element types.
- ○ Define explicit ownership/drop semantics for container element lifecycles during reallocation/move operations for non-trivial element types.
- ○ Implement container element drop behavior in vector/map runtimes once the ownership contract is fixed.
- ○ Define map key trait contracts (`Hash + Eq` for hash maps and/or `Comparable` for ordered maps).
- ○ Enforce map key trait contracts in semantics with deterministic diagnostics on unsupported key types.
- ○ Define a standard container error contract (`Result`/`Maybe` and/or panic primitive) for stdlib containers.
- ○ Replace compiler-injected vector/map runtime abort paths with the standard container error contract across VM/native/C++ flows.
Vector/map de-builtinization sequencing note:
1. Land memory intrinsics and trait contracts.
2. Implement `std::vector` and `std::map` in `.prime`.
3. Keep current builtin spellings as compatibility aliases that forward to stdlib.
4. Remove semantics/lowering/emitter special-cases after parity and conformance tests pass.
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
Type-resolution graph rollout note: keep the graph migration blocked on the remaining collection-helper/runtime predecessor items so solver work does not target moving call-target semantics. Required predecessors remain the open vector/map migration items plus the started collection-runtime and dynamic-vector-runtime items above.
- ○ Add a short staged rollout note that names the implementation checkpoints (builder/dump, return-kind pilot, parity, template/local expansion, query and CT-eval follow-ups, legacy removal) and the rollback gates between them.
- ○ Add a short design note in `docs/PrimeStruct.md` defining the graph model and invariants: node kinds (definition return-kind, call-site type constraints, local `auto` constraints), edge kinds (dependency vs requirement), legal-cycle policy (SCC fixed-point) vs illegal-cycle policy (deterministic diagnostic), and deterministic ordering guarantees.
- ○ Introduce a new semantics-internal `TypeResolutionGraph` data model and builder (no behavior change): build nodes/edges from canonical AST after semantic transforms/template monomorphization, with stable node IDs and stable edge insertion order.
- ○ Add a deterministic graph dump hook for tests/tooling (`--dump-stage=type-graph` or equivalent) and snapshot tests that lock graph shape/order for representative programs (simple call chain, mutual recursion, namespace imports, templates).
- ○ Implement reusable SCC utilities (Tarjan/Kosaraju) in a standalone unit with unit tests for acyclic, single-cycle, multi-cycle, and disconnected graphs.
- ○ Implement a condensation-DAG builder on top of the SCC utilities, with deterministic ordering tests.
- ○ Land a first vertical solver slice for definition return-kind inference only: run SCC fixed-point over return constraints, preserve current diagnostics, and gate the new path behind a temporary opt-in flag (`--type-resolver=graph`).
- ○ Add parity harness tests that run legacy vs graph return-kind inference on the same corpus and assert identical diagnostics/IR for supported cases; include explicit expected-diff tests where legacy behavior is intentionally corrected.
- ○ Flip return-kind inference default to graph mode once parity is stable; keep a temporary fallback flag (`--type-resolver=legacy`) for rollback during stabilization.
- ○ Extend graph constraints to explicit template call-site inference, including chained and mutually recursive calls, with SCC-local fixed-point limits.
- ○ Extend graph constraints to implicit template call-site inference, with deterministic ambiguity diagnostics aligned with legacy behavior unless intentionally changed.
- ○ Extend graph constraints to local `auto` binding inference, including cross-statement dependencies and call-return dependencies.
- ○ Extend graph constraints to omitted-envelope inference while preserving existing unresolved/conflicting inference diagnostics.
- ○ Add explicit illegal-cycle diagnostics where DAG-only domains are required (for example struct layout if recursive layout remains unsupported), including cycle path reporting with stable source ordering.
- ○ Add performance guardrails: per-SCC iteration caps, worklist metrics, and deterministic failure diagnostics for non-converging constraint sets; add stress tests for large SCCs and deep dependency chains.
- ○ Enable optional parallel solve execution across condensation-DAG layers (or SCC-independent components) behind `--semantics-threads`, with deterministic merge ordering for diagnostics.
- ○ Add a design-gate architecture note for incremental/query integration with type resolution: query keys, dependency edges, invalidation units, cache boundaries, and deterministic recompute rules; require doc approval before implementation.
- ○ Implement a minimal query substrate for graph resolution (memoized query nodes for return constraints, call constraints, and local inference constraints) with deterministic cache keying and invalidation tests.
- ○ Add import/transform-aware invalidation for query-backed type resolution (canonical path + transform phase dependencies) and lock invalidation order via conformance tests.
- ○ Add a design-gate architecture note for compile-time execution (CT eval) interaction with type resolution: trigger policy, dependency registration, cycle policy, termination/fuel rules, and deterministic diagnostics; require doc approval before implementation.
- ○ Implement CT-eval/type-resolution dependency bridging so CT-evaluated type facts feed graph constraints without hidden side effects; include deterministic cycle and non-convergence diagnostics.
- ○ Add a short inventory/design note for the remaining inference islands that still live outside the graph resolver, with a migration order and parity expectations for each.
- ○ Migrate the remaining inference islands into graph ownership one slice at a time after return/template/local inference parity is stable.
- ○ Add sustained performance/convergence suites for graph+query resolution on large dependency workloads (deep chains, wide SCCs, import-heavy graphs) with deterministic output and regression thresholds.
- ○ Remove legacy pass-ordered inference paths after graph-mode parity and convergence tests pass; keep one cleanup commit dedicated to dead-code removal and doc updates.
- ○ Add end-to-end conformance suite entries dedicated to graph resolution semantics (mutual recursion, cycle diagnostics, mixed template+auto inference, import-crossing dependencies) and keep them required in `./scripts/compile.sh`.

**Backends & IR**
- ○ Define the shared language-level `/std/image/*` file-I/O API.
- ○ Define the deterministic unsupported-diagnostic contract for backends that cannot do image file I/O.
- ○ Add `/std/image/ppm/*` read implementations and conformance tests for VM/native/Wasm.
- ○ Add `/std/image/ppm/*` write implementations and conformance tests for VM/native/Wasm.
- ○ Add `/std/image/png/*` read implementations and conformance tests for VM/native/Wasm.
- ○ Add `/std/image/png/*` write implementations and conformance tests for VM/native/Wasm.
- ○ Add GLSL reject/diagnostic coverage for image file I/O.

**Web + Native + Metal 3D Target (Spinning Cube)**
- ○ Implement profile-deduced device creation and deterministic `GfxError` code mapping for the v1 spinning-cube path.
- ○ Lock `/std/gfx/VertexColored` wire layout across lowering/backends and add source-lock coverage on the canonical spinning-cube snippets.
- ○ Implement `?` fallible flow for the v1 graphics path with compile-run coverage on canonical sample snippets.
- ○ Implement `on_error<...>` fallible flow for the v1 graphics path with compile-run coverage on canonical sample snippets.
- ○ Add base software renderer draw-command primitives (`draw_text`, `draw_rounded_rect`) with deterministic ordering and command serialization tests.
- ○ Add base software renderer clip/push-pop command-list support with deterministic stack behavior and serialization tests.
- ○ Add a software color-buffer/shared-surface bridge so software-rendered output can be presented through the native presenter path (including Metal host blit/present flow).
- ○ Add a two-pass layout engine contract (children-up measure + parents-down arrange) with deterministic layout golden tests.
- ○ Add a basic widget control layer on top of the layout contract (`button`, `label`, `input`) that emits the renderer command list.
- ○ Add a basic widget container primitive on top of the layout contract that emits the renderer command list.
- ○ Add a composite widget layer composed strictly from basic widgets, with conformance tests that ban direct backend draw calls in composite widgets.
- ○ Add an HTML backend adapter that emits DOM/CSS/event wiring from the shared widget/layout model as an alternative to surface presentation.
- ○ Add a platform input adapter contract that normalizes pointer and keyboard events into one UI event stream for widget runtime consumption.
- ○ Add a platform input adapter contract that normalizes IME events into the shared UI event stream for widget runtime consumption.
- ○ Add a platform input adapter contract that normalizes resize and focus events into the shared UI event stream for widget runtime consumption.
