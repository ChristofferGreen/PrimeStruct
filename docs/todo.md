# PrimeStruct TODO Log

## Operating Rules (Human + AI)

1. This file contains only open work (`[ ]` or `[~]`).
2. Use one task block per item with a stable ID: `TODO-XXXX`.
3. Keep task ordering aligned with dependency and execution priority. New
   tasks should be inserted into the relevant lane/queue position rather than
   blindly prepended by creation date.
4. Every task must include:
   - clear scope
   - acceptance criteria
   - owner (`human` or `ai`)
5. When a task is completed:
   - mark it `[x]`
   - add `finished_at` and short evidence note
   - move the full block to `docs/todo_finished.md`
   - remove it from this file (do not keep completed tasks here)
6. `docs/todo_finished.md` is append-only history. Do not rewrite old entries except to fix factual mistakes.
7. Each implementation task SHOULD include `phase` and `depends_on` to keep execution order explicit.
8. Prefer small, testable tasks over broad epics; split before starting if acceptance cannot be verified in one PR.
9. Every task block must be implementable by someone arriving with no session context, including an AI agent. Include
   enough local detail to identify the intended behavior, likely source/test areas, dependencies, diagnostics, and
   validation gate without relying on prior conversation.
10. Implementation tasks SHOULD include `implementation_notes` when source entry points, nearby tests, migration
    hazards, or validation strategy are not obvious from scope and acceptance alone.
11. Keep the `Execution Queue` and coverage snapshots current when adding/removing tasks.
    When adding, splitting, renaming, completing, or deleting a task, update every
    reference to that TODO ID in lanes, queues, snapshots, `depends_on`, stop
    rules, and notes; use `rg TODO-XXXX docs/todo.md` to catch stale references.
12. Keep an explicit `Ready Now` shortlist synced with dependencies; only items with no unmet TODO dependencies belong there.
13. When splitting broad tasks, update parent task scope to avoid duplicated acceptance criteria across child tasks.
14. Keep `Priority Lanes` aligned with queue order so critical-path tasks remain visible.
15. For phase-level tracking tasks, pair planning trackers with explicit acceptance-gate tasks before marking a phase complete.
16. Every active leaf must include a stop rule and deliver at least one of:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
17. Avoid standalone micro-cleanups (alias renames, trivial bool rewrites, local dedup) unless bundled into one value outcome above.
18. If a leaf misses its value target after 2 attempts, archive it as low-value and replace it with a different hotspot.
19. Keep the live execution queue short: no more than 8 leaf tasks in `Ready Now` at once. Additional dependency-blocked or deferred follow-up leaves may stay in the task blocks and `Immediate Next 10`, but only `Ready Now` counts as the live queue cap.
20. Treat disabled tests as debt: every retained `doctest::skip(true)` cluster must either map to an active TODO leaf with a clear re-enable-or-delete outcome, or be removed once proven stale.

Status legend:
- `[ ]` queued
- `[~]` in progress
- `[x]` completed (must be moved to `docs/todo_finished.md`)

Task template:

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional but recommended)
  - depends_on: TODO-XXXX, TODO-YYYY (optional but recommended)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

## Open Tasks

### Ready Now (Live Leaves; No Unmet TODO Dependencies)

- TODO-4266: Rewire `?` to the `Result` sum contract

### Immediate Next 10 (After Ready Now)

- TODO-4267: Retire legacy Maybe/Result representations
- TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
- TODO-4268: Add heterogeneous type-pack syntax and metadata
- TODO-4269: Bind and monomorphize type-pack arguments
- TODO-4270: Add compile-time integer template arguments

### Priority Lanes (Current)

- Deferred stdlib ADT migration: TODO-4266 -> TODO-4267
  -> TODO-4291
- Deferred generic tuple substrate: TODO-4268 -> TODO-4269 -> TODO-4270
  -> TODO-4275 -> TODO-4276 -> TODO-4271 -> TODO-4272 -> TODO-4274
  -> TODO-4273 -> TODO-4277 -> TODO-4278
- Deferred dynamic runtime storage follow-up: TODO-4281

### Execution Queue (Recommended)

- TODO-4266: Rewire `?` to the `Result` sum contract
- TODO-4267: Retire legacy Maybe/Result representations
- TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
- TODO-4268: Add heterogeneous type-pack syntax and metadata
- TODO-4269: Bind and monomorphize type-pack arguments
- TODO-4270: Add compile-time integer template arguments
- TODO-4275: Expand type packs into struct storage
- TODO-4276: Expand type packs in helpers and lifecycle hooks
- TODO-4271: Add compile-time pack indexing
- TODO-4272: Add stdlib `tuple<Ts...>`
- TODO-4274: Add tuple bracket indexing sugar
- TODO-4273: Add heterogeneous value-pack inference
- TODO-4277: Add tuple destructuring sugar
- TODO-4278: Integrate multi-wait with stdlib tuple
- TODO-4281: Lift vector dynamic capacity limit

### PrimeStruct Coverage Snapshot

| PrimeStruct area | Primary TODO IDs |
| --- | --- |
| Semantic ownership boundary and graph/local-auto authority | none |
| Compile-pipeline stage and publication-boundary contracts | none |
| Compile-time macro hooks and AST transform ownership | none |
| Stdlib surface-style alignment and public helper readability | none |
| Stdlib bridge consolidation and collection/file/gfx surface authority | none |
| Vector/map stdlib ownership cutover and collection surface authority | TODO-4281 |
| Stdlib de-experimentalization and public/internal namespace cleanup | none |
| SoA maturity and `soa_vector` promotion | none |
| Validator entrypoint and benchmark-plumbing split | none |
| Semantic-product publication by module and fact family | none |
| Semantic-product public API factoring and versioning | none |
| IR lowerer compile-unit breakup | none |
| Backend validation/build ergonomics | none |
| Emitter/semantics map-helper parity | none |
| VM debug-session argv ownership | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay robustness | none |
| VM/runtime debug stateful opcode parity | none |
| Test-suite audit follow-up and release-gate stability | none |
| Algebraic sum types and brace-only construction | none |
| Stdlib ADT migration for `Maybe` and `Result` | TODO-4266, TODO-4267, TODO-4291 |
| Generic type packs and tuple stdlib surface | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

### Validation Coverage Snapshot

| Validation area | Primary TODO IDs |
| --- | --- |
| Semantic-product-authority conformance | none |
| AST transform hook conformance | none |
| CodeExamples-aligned stdlib surface syntax conformance | none |
| Compile-pipeline stage handoff conformance | none |
| Semantic-product publication parity and deterministic ordering | none |
| Lowerer/source-composition contract coverage | none |
| Vector/map bridge parity for imports, rewrites, and lowering | TODO-4281 |
| De-experimentalization surface and namespace parity | none |
| `soa_vector` maturity and canonical surface parity | none |
| Focused backend rerun ergonomics and suite partitioning | none |
| Architecture contract probe migration | none |
| Emitter map-helper canonicalization parity | none |
| VM debug-session argv lifetime coverage | none |
| Debugger/source-map provenance parity | none |
| Debug trace replay malformed-input coverage | none |
| Shared VM/debug stateful opcode behavior | none |
| Release benchmark/example suite stability and doctest governance | none |
| Sum-type and brace-construction conformance | none |
| Maybe/Result sum migration conformance | TODO-4266, TODO-4267, TODO-4291 |
| Generic type-pack and tuple conformance | TODO-4268, TODO-4269, TODO-4270, TODO-4275, TODO-4276, TODO-4271, TODO-4272, TODO-4274, TODO-4273, TODO-4277, TODO-4278 |

### Vector/Map Bridge Contract Summary

- Bridge-owned public contract: exact and wildcard `/std/collections` imports,
  `vector<T>` / `map<K, V>` constructor and literal-rewrite surfaces, helper
  families, compatibility spellings plus removed-helper diagnostics, semantic
  surface IDs, and lowerer dispatch metadata.
- Migration-only seams: rooted `/vector/*` and `/map/*` spellings,
  `vectorCount` / `mapCount`-style lowering names, and
  `/std/collections/experimental_*` implementation modules stay temporary.
  The vector/map adapter cutover is complete for semantic and
  template-monomorph helper decisions; no active TODO targets deleting or
  accepting those temporary seams, so add a concrete successor TODO before
  changing their public status.
- Compatibility adapter inventory: map insert helper compatibility is migrated
  through `StdlibSurfaceRegistry::CollectionsMapHelpers` for canonical
  `/std/collections/map/insert(_ref)`, compatibility `/map/insert(_ref)`,
  wrapper `/std/collections/mapInsert(_Ref)`, and experimental
  `/std/collections/experimental_map/mapInsert(_Ref)` spellings. Template
  monomorphization now asks the registry for preferred experimental vector/map/SoA
  helper spellings instead of carrying bespoke canonical-to-experimental maps.
  SoA helper compatibility is routed through
  `StdlibSurfaceRegistry::CollectionsSoaVectorHelpers` for canonical
  `/std/collections/soa_vector/*`, same-path `/soa_vector/*`, mixed
  `/std/collections/{count,get,ref,reserve,push}`, experimental helper wrapper,
  and conversion-helper spellings. Vector/map and SoA constructor compatibility
  is already metadata-backed by the constructor surface adapters. Gfx Buffer
  helper compatibility is routed through
  `StdlibSurfaceRegistry::GfxBufferHelpers` for canonical `/std/gfx/Buffer/*`,
  legacy `/std/gfx/experimental/Buffer/*`, and rooted `/Buffer/*` helper
  spellings before semantic gfx-buffer rewrites and GPU wrapper diagnostics
  choose canonical builtin targets. Remaining removed-helper diagnostics,
  import spellings, wildcard expansion, user-defined helper precedence,
  field-view field-name lowering, gfx constructor sugar, and lowerer raw-path
  dispatch checks are syntax/provenance-owned or lowering-owned.
- Outside this lane: `array<T>` core ownership, promoted `soa_vector<T>`, and
  runtime/storage redesign remain separate boundaries and should not be folded
  into the vector/map bridge tasks below.

### Stdlib De-Experimentalization Policy Summary

- Canonical public API: non-`experimental` namespaces are the intended
  long-term user-facing contracts.
- Canonical collection contract: `/std/collections/vector/*` and
  `/std/collections/map/*` are the sole public vector/map collection surfaces.
- Accepted compatibility namespaces:
  `/std/collections/experimental_soa_vector/*` remains importable only for
  targeted compatibility and conformance coverage. Ordinary public examples
  should use `/std/collections/soa_vector/*`.
  `/std/collections/experimental_soa_vector_conversions/*` remains a bridge
  for direct experimental imports. Canonical conversion helpers now route
  through `/std/collections/internal_soa_vector_conversions/*`. These direct
  experimental imports are retained compatibility shims for targeted tests
  only, not ordinary public imports.
- Internal collection implementation modules:
  `/std/collections/experimental_vector/*` and
  `/std/collections/experimental_map/*` now remain implementation-owned seams
  behind canonical `/std/collections/vector/*` and `/std/collections/map/*`,
  with direct imports kept only for targeted compatibility or conformance
  coverage.
- Legacy gfx compatibility seam: `/std/gfx/experimental/*` remains importable
  only for targeted compatibility coverage and staged migration support;
  canonical `/std/gfx/*` is the only public gfx namespace.
- Internal substrate/helper namespaces:
  `/std/collections/internal_buffer_checked/*`,
  `/std/collections/internal_buffer_unchecked/*`,
  `/std/collections/internal_soa_vector_conversions/*`,
  `/std/collections/internal_soa_vector/*`, and
  `/std/collections/internal_soa_storage/*` are implementation-facing plumbing
  rather than public API.
- Default rule: no `experimental` namespace is canonical public API unless the
  docs call out an accepted compatibility exception explicitly.

### SoA Public Collection Summary

- `soa_vector<T>` is a promoted stdlib-owned public collection surface.
- Canonical public spellings for current docs/examples are
  `/std/collections/soa_vector/*` and
  `/std/collections/soa_vector_conversions/*`.
- Accepted compatibility seams: `/std/collections/experimental_soa_vector/*`
  and `/std/collections/experimental_soa_vector_conversions/*` remain importable
  only for targeted compatibility and conformance coverage; C++/VM/native
  direct-import tests lock that compatibility contract.
- Internal substrate namespaces: `/std/collections/internal_soa_vector/*`
  owns canonical wrapper implementation forwarding,
  `/std/collections/internal_soa_vector_conversions/*` owns canonical
  conversion implementation forwarding, while
  `/std/collections/internal_soa_storage/*` remains implementation-facing SoA
  storage/layout plumbing. The internal wrapper adapter still preserves the
  compatibility `SoaVector<T>` type identity behind the promoted public wrapper
  surface. The inline-parameter and direct lowerer wrapper-dispatch bridges no
  longer use rooted `/soa_vector/*`, `/to_aos`, or `/to_aos_ref` spellings as
  hidden raw fallbacks.
- Promoted contract complete: the canonical public helper wrapper is
  authoritative for ordinary construction/read/ref/mutator/conversion helper
  names, bound field-view borrow-root invalidation, and canonical-only
  C++/VM/native helper, field-view, and conversion parity coverage. Conversion
  receiver contracts are spelled through canonical `SoaVector<T>` surfaces, the
  checked-in ECS example uses canonical wrapper/conversion imports, and ordinary
  public code no longer needs `experimental_soa_vector` or
  `experimental_soa_vector_conversions` imports. The canonical wrapper routes through
  `/std/collections/internal_soa_vector/*` and canonical conversions route
  through `/std/collections/internal_soa_vector_conversions/*` instead of
  directly importing experimental implementation modules.

### Skipped Doctest Debt Summary

- Retained `doctest::skip(true)` coverage is currently absent from the active
  queue because no skipped doctest cases remain under `tests/unit`.
- New skipped doctest coverage must create a new explicit TODO before it lands.
- The success condition for each lane is re-enable-or-delete; indefinite
  skipped coverage is not a stable end state.

### Task Blocks

- [ ] TODO-4266: Rewire `?` to the `Result` sum contract
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - scope: Make postfix `?`, `try(...)`, and `on_error` propagation consume the
    stdlib-owned `Result` sum shape instead of bespoke Result metadata or
    legacy runtime representation.
  - implementation_notes:
    - Start from result/try/on_error semantic validation, result metadata in
      `include/primec/SemanticProduct.h`, and IR lowerer result helpers.
    - Semantic validation, semantic-product `try(...)`/query facts, and IR
      Result type parsing now recognize both `Result<T, E>` and the qualified
      `/std/result/Result<T, E>` spelling for value-carrying results. The
      remaining work is the actual sum-backed propagation/lowering contract.
    - IR-backed `try(...)` now consumes local imported stdlib value-result sums
      for `return<int> on_error<...>` status-code flows by branching on the
      `ok`/`error` tag and extracting int-backed error payloads. Result-return
      functions now propagate local imported stdlib value-result sum errors by
      copying the `error` payload into the declared return `Result` sum. Direct
      calls that return imported stdlib value-result sums can now be consumed
      by postfix `?` on the same VM/native sum-backed paths. Legacy
      `Result.map`, `Result.and_then`, and `Result.map2` construction for typed
      imported stdlib Result sums now accepts local sources and direct calls
      that return imported stdlib Result sums. Dereferenced local
      `Reference<Result<T, E>>` and `Pointer<Result<T, E>>` values now feed
      `try(...)`, `Result.error(...)`, and `Result.why(...)` when they point at
      imported stdlib Result sums. `/std/result/*` now exposes the
      status-only `Result<E>` sum beside `Result<T, E>` at the same public path,
      with a unit `ok` variant, an `error(E)` payload variant, default `ok`
      construction, `ok<E>()`, and semantic `pick` coverage. IR-backed
      `try(...)` now consumes local, direct-call, and dereferenced local
      borrowed/pointer imported status-only `Result<E>` sums for status-code
      returns and Result-return error propagation. IR-backed
      `Result.error(...)` / `Result.why(...)` now inspect those same imported
      status-only operand families. Remaining work covers non-VM/native bridge
      cleanup. SyntaxSpec now documents the landed IR-backed status-only
      `try(...)`, postfix `?`, `Result.error(...)`, and `Result.why(...)`
      operand families and leaves only the broader/non-IR bridge cleanup as
      migration work. The legacy source C++ emitter now preserves nested
      `Result<T...>` types under `Reference` / `Pointer` and recognizes
      dereferenced local/indexed borrowed Result operands for `try(...)`,
      `Result.error(...)`, and `Result.why(...)` while it still uses a
      compatibility bridge. Its source C++ Result storage-width decisions and
      construction/accessor expression emission are quarantined behind named
      emitter helpers. Value-carrying source C++ Result storage now emits the
      tagged `ps_result_value` bridge type instead of raw `uint64_t`
      return/binding types or legacy `ps_legacy_result_*` helper names; the
      bridge exposes value-qualified tag, error-payload, and success-payload
      accessors plus explicit ok/error construction helpers. Raw
      packed-integer conversion, construction compatibility, generic
      `ps_result_*` value accessor names, and the generated
      `ps_result_pack(...)` helper have been deleted. Status-only source C++
      Result storage now emits the tagged `ps_result_status` bridge type
      instead of raw `uint32_t` return/binding types, and low-level file helper
      status codes are wrapped at the source Result boundary. Remaining
      cleanup should retarget broader bridge construction to the stdlib Result
      sum contract.
    - Preserve current user-facing `?` behavior first; any broader propagation
      syntax changes should be split into separate TODOs.
    - Add semantic-product and IR tests before broad compile-run tests so the
      result-sum contract is observable.
  - acceptance:
    - `?` unwraps the `ok`/success variant and propagates the `error` variant
      using the same observable behavior as the current Result contract.
    - `on_error` handlers and status-code entry flows still work for current
      file, image, collection, and gfx error domains.
    - Unsupported or malformed Result-like sums do not accidentally participate
      in `?` unless they match the documented Result contract.
    - VM/native/C++ tests cover direct Result values, map/and_then/map2
      sources, borrowed/pointer Result values, and status-only results.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once `?` and `try(...)` are sum-backed for the supported
    Result surface and unsupported cases fail deterministically.

- [ ] TODO-4267: Retire legacy Maybe/Result representations
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4266
  - scope: Remove or quarantine old compiler/runtime special cases for Maybe
    and Result after both are stdlib-owned sums and `?` consumes the Result sum
    contract.
  - implementation_notes:
    - Inventory bespoke Maybe/Result code in semantics, semantic product,
      lowerer, VM/native runtime helpers, C++ emitter, docs, and tests before
      deleting behavior.
    - Keep compatibility helpers only when they are implemented in stdlib or
      routed through documented helper-call bridges.
    - Split any deletion that risks changing unrelated error domains or payload
      support into a follow-up TODO.
  - acceptance:
    - No production path silently depends on the retired Maybe/Result struct or
      bespoke runtime representation.
    - Compatibility helpers either live in stdlib, have explicit bridge tests,
      or produce deterministic migration diagnostics.
    - Docs and examples describe Maybe/Result as sums and remove obsolete
      representation claims.
    - Release, compile-run, semantic-product, and IR tests cover the migrated
      Maybe/Result surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once legacy Maybe/Result special cases are removed,
    quarantined behind named compatibility bridges, or split into explicit
    follow-up TODOs.

- [ ] TODO-4291: Decide sum-backed mutable `Maybe<T>` helpers
  - owner: ai
  - created_at: 2026-04-28
  - phase: Deferred stdlib ADT migration
  - depends_on: TODO-4267
  - scope: Decide whether `Maybe<T>.set(value)`, `Maybe<T>.clear()`, and
    `Maybe<T>.take()` should return as stdlib helpers on top of sum-backed
    `Maybe<T>`, or remain retired in favor of explicit `pick` plus value
    construction.
  - implementation_notes:
    - Start from `stdlib/std/maybe/maybe.prime`,
      `tests/unit/test_semantics_maybe.cpp`, and the VM/native Maybe
      compile-run tests.
    - If mutable helpers return, first define the general language contract for
      in-place active-variant mutation and payload movement on sum values.
    - If mutable helpers stay retired, strengthen diagnostics so callers get a
      named migration error rather than a generic missing-helper path.
  - acceptance:
    - The chosen direction is documented in `docs/PrimeStruct.md`.
    - Semantic tests cover `set`, `clear`, and `take` either as supported
      helper behavior or as deterministic migration diagnostics.
    - Compile-run coverage covers the chosen supported Maybe mutation or
      explicit-construction/pick replacement surface.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the sum-backed Maybe mutable-helper policy is
    implemented or intentionally retired with focused diagnostics and docs.

- [ ] TODO-4268: Add heterogeneous type-pack syntax and metadata
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4267
  - scope: Add parser, AST, semantic-product, and diagnostic support for
    heterogeneous type-parameter packs such as `tuple<Ts...>` without adding
    pack expansion or tuple implementation yet.
  - implementation_notes:
    - Start from `docs/TuplePrototype.md`,
      `docs/PrimeStruct_SyntaxSpec.md`, `include/primec/Ast.h`, parser
      template-list handling under `src/parser/`, template setup under
      `src/semantics/TemplateMonomorph*.h`, and semantic-product metadata.
    - Current `args<T>` is homogeneous value-pack support and must not be
      reused as the representation for heterogeneous type packs.
    - Keep the syntax generic; do not add tuple-specific parsing rules.
  - acceptance:
    - Definitions and structs can declare a single type pack parameter using a
      documented syntax such as `Ts...`.
    - The AST and semantic product distinguish ordinary template parameters
      from pack parameters deterministically.
    - Pack parameters can appear in type positions only where the language has
      explicitly documented them as valid; unsupported placements produce
      stable diagnostics.
    - Parser/semantic tests reject duplicate pack names, more than one pack
      when not supported, packs before ordinary template parameters if that is
      disallowed, and `args<T>` misuse as a heterogeneous pack.
    - `docs/TuplePrototype.md` and `docs/PrimeStruct_SyntaxSpec.md` record the
      accepted syntax and note that binding is deferred to TODO-4269 and
      expansion is deferred to TODO-4275/TODO-4276.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once heterogeneous type-pack declarations are parseable,
    represented, and diagnosable; leave binding and monomorphization to
    TODO-4269.

- [ ] TODO-4269: Bind and monomorphize type-pack arguments
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4268
  - scope: Add template binding and monomorphization support for an arbitrary
    number of type arguments bound to a type-pack parameter, without expanding
    pack fields or helper bodies yet.
  - implementation_notes:
    - Build on the pack metadata from TODO-4268 and the existing template
      monomorphization path; avoid generating source-level `tuple1`/`tuple2`
      families.
    - Keep the output as monomorphized pack metadata and deterministic
      specialized names that later expansion tasks can consume.
    - Treat pack argument order as source-order and deterministic because tuple
      layout will depend on it.
  - acceptance:
    - A definition or struct with `Ts...` can be specialized with zero, one, or
      multiple concrete type arguments and records the bound pack contents
      deterministically.
    - Specialized names, semantic-product metadata, and serialized diagnostics
      preserve pack argument order across repeated builds and import order.
    - Ordinary non-pack template parameters continue to monomorphize exactly as
      before.
    - Monomorphized names and serialized diagnostics are deterministic across
      repeated builds and import order.
    - Negative tests cover missing pack arguments where disallowed, conflicting
      pack inference, unsupported multiple packs, and attempts to use pack
      expansion before TODO-4275.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once type-pack arguments bind and monomorphize
    deterministically; leave field expansion to TODO-4275 and helper/lifecycle
    expansion to TODO-4276.

- [ ] TODO-4270: Add compile-time integer template arguments
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4268
  - scope: Extend template arguments to support compile-time integer values
    needed by APIs such as `get<0>(tuple)` and
    `tuple_element<1, tuple<...>>()`.
  - implementation_notes:
    - Current template arguments are parsed as envelope/type names. Add a
      typed representation for non-type template arguments rather than
      smuggling integer indexes through strings.
    - Start from `include/primec/Ast.h`, parser template-list code,
      template-argument validation, AST/IR printers, semantic diagnostics, and
      tests that currently assume template args are strings.
    - Keep this generic; do not special-case the name `get`.
  - acceptance:
    - Calls and method calls accept integer template arguments in documented
      positions, for example `get<0>(value)` and `value.get<1>()`.
    - Non-type template arguments are represented distinctly from type
      template arguments through parser, AST printer, semantic validation, and
      monomorphization.
    - Invalid non-constant expressions, negative indexes if unsupported,
      floats, strings, and bools produce stable diagnostics.
    - Existing type-template behavior for collections, transforms, and
      user-defined helpers remains source-compatible.
    - Focused tests cover parsing, printing, semantic rejection, and at least
      one generic helper that accepts an integer template argument.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once integer template arguments are a generic language
    feature with diagnostics; leave pack-index lookup to TODO-4271.

- [ ] TODO-4275: Expand type packs into struct storage
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4269
  - scope: Add `.prime` struct-field expansion for heterogeneous type packs so
    a generic struct can store one field per bound type, without adding helper
    body expansion or tuple-specific APIs yet.
  - implementation_notes:
    - Build on type-pack monomorphization from TODO-4269.
    - The expansion syntax should stay generic and documented in
      `docs/PrimeStruct_SyntaxSpec.md`; `tuple<Ts...>` is only the first known
      consumer.
    - Use deterministic internal field names for expanded storage so semantic
      product, diagnostics, and lowering do not depend on source text hacks.
  - acceptance:
    - A `.prime` generic struct can expand a type pack into one stored field
      per bound type with deterministic field order and stable internal names.
    - Pack-expanded fields participate in brace construction and field
      initialization for zero, one, and multi-element packs.
    - Semantic-product or layout snapshots expose enough field metadata for
      later indexed access without tuple-specific facts.
    - Negative tests cover invalid field modifiers on pack expansions,
      unsupported recursive pack-expanded storage, arity mismatches, and
      unsupported placements outside documented struct-field contexts.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once pack-expanded storage is represented and initialized
    deterministically; leave helper bodies and lifecycle hooks to TODO-4276.

- [ ] TODO-4276: Expand type packs in helpers and lifecycle hooks
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4275
  - scope: Extend pack expansion from stored fields into helper signatures,
    helper bodies, return envelopes, and generated copy/move/destroy lifecycle
    behavior.
  - implementation_notes:
    - Build on pack-expanded storage from TODO-4275 and existing lifecycle
      semantics for ordinary struct fields.
    - Cross-reference the reflection field metadata API and generated helper
      model in `docs/PrimeStruct.md`: `meta.field_count<T>()`,
      `meta.field_name<T>(i)`, `meta.field_type<T>(i)`,
      `meta.field_visibility<T>(i)`, plus generated `/Type/Default`,
      `/Type/Clone`, `/Type/Clear`, and lifecycle behavior. Pack-expanded
      lifecycle order should reuse that source-order field model rather than
      introducing tuple-specific `drop(item...)` semantics.
    - Keep lifecycle order deterministic and documented; tuple element
      lifecycle depends on it.
    - Add focused tests before broad tuple tests so generic pack expansion is
      validated independently of `tuple`.
  - acceptance:
    - Helper definitions can mention pack-expanded parameters, locals, and
      return envelopes in documented positions.
    - Pack-expanded fields participate in copy, move, and destruction according
      to ordinary struct field rules, including non-trivial element types.
    - Reflection metadata for reflect-enabled pack-expanded structs reports
      expanded fields in the same order that lifecycle hooks copy, move, clear,
      and destroy them.
    - Lifecycle failures in one element type produce diagnostics that identify
      the element index or expanded field.
    - Tests cover zero-element, one-element, and multi-element packs in helper
      and lifecycle contexts, including at least one reflect-enabled
      pack-expanded struct that exercises `meta.field_count<T>()` and generated
      helper behavior.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once generic pack expansion works outside stored fields and
    lifecycle behavior is covered; leave indexed access to TODO-4271.

- [ ] TODO-4271: Add compile-time pack indexing
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4276, TODO-4270
  - scope: Add generic compile-time pack indexing so helpers can resolve the
    `I`th type and storage slot of a heterogeneous type pack.
  - implementation_notes:
    - Build on pack expansion from TODO-4276 and integer template arguments
      from TODO-4270.
    - The feature must be generic enough for `get<I>`, `tuple_element<I, T>`,
      and future stdlib metaprogramming helpers.
    - Publish enough semantic-product facts that lowerers do not need to
      re-derive tuple field selection from source text.
  - acceptance:
    - A `.prime` helper can use an integer template argument to select the
      corresponding expanded pack field and return that field's precise type.
    - Borrowed and mutable borrowed pack-index access preserves
      `Reference<TI>` behavior when ordinary field borrowing would allow it.
    - Out-of-range indexes, non-pack operands, and non-integer indexes produce
      stable diagnostics at semantic validation time.
    - Semantic-product and IR-lowering tests verify that indexed pack lookup is
      represented without tuple-specific AST/source fallbacks.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once indexed pack lookup can power a generic `get<I>`-style
    helper; leave the public tuple stdlib surface to TODO-4272.

- [ ] TODO-4272: Add stdlib `tuple<Ts...>`
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4271
  - scope: Implement the public `tuple<Ts...>` stdlib type and minimal helper
    surface in `.prime` using the generic type-pack substrate.
  - implementation_notes:
    - Add files under `stdlib/std/tuple/` plus a public import surface aligned
      with existing stdlib naming/import conventions.
    - Do not add compiler/runtime tuple opcodes, special envelopes, generated
      fixed-arity tuple families, or fixed helper names such as `get0`.
    - Start with construction, `get<I>`, borrowed access where supported,
      tuple arity metadata, and focused diagnostics.
  - acceptance:
    - `tuple<Ts...>{...}` constructs owned heterogeneous tuples of arbitrary
      arity using `.prime` source and ordinary field lifecycle rules.
    - `get<I>(tuple)` and method sugar where supported return the precise
      element type for value and borrowed receivers.
    - `tuple<>`, `tuple<T>`, and multi-element tuples are covered without
      separate arity-specific implementations.
    - Diagnostics cover initializer arity mismatch, bad element type,
      out-of-range `get`, non-constant `get`, and unsupported element
      lifecycle operations.
    - Docs record `tuple` as stdlib-owned and explicitly reject fixed-arity
      tuple families as implementation strategy.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the minimal tuple API is implemented in `.prime` and
    validated; leave heterogeneous value-pack inference to TODO-4273.

- [ ] TODO-4273: Add heterogeneous value-pack inference
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4272
  - scope: Add generic heterogeneous value-pack inference for helpers such as
    `make_tuple(values...)` without weakening the homogeneous `args<T>` model.
  - implementation_notes:
    - Keep `args<T>` homogeneous. Add a separate heterogeneous value-pack
      inference path that binds one type per positional argument.
    - Start from implicit-template inference, spread-argument validation, and
      call binding; do not change vector/map variadic behavior unless covered
      by explicit compatibility tests.
    - Treat spread forwarding of heterogeneous packs as a separate documented
      feature; split it out if it grows beyond this task.
  - acceptance:
    - A `.prime` helper can infer `Ts...` from heterogeneous positional values
      and return `tuple<Ts...>`.
    - `make_tuple(1, "x", true)` or the accepted stdlib spelling constructs
      `tuple<i32, string, bool>` through ordinary tuple construction.
    - Existing homogeneous `args<T>` diagnostics and vector/map constructors
      remain unchanged.
    - Negative tests cover conflicting inference, empty packs if unsupported,
      named-argument interactions, and unsupported spread forwarding.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once heterogeneous value-pack inference is available for
    tuple helper construction and does not regress existing homogeneous packs.

- [ ] TODO-4274: Add tuple bracket indexing sugar
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4272
  - scope: Add `value[0]` style tuple element access as compile-time-indexed
    sugar over `get<0>(value)`, distinct from runtime collection indexing.
  - implementation_notes:
    - Start from `docs/TuplePrototype.md`, parser/indexing forms, semantic
      collection access validation, and tuple `get<I>` from TODO-4272.
    - Tuple indexing must use a compile-time integer literal or equivalent
      folded constant; it must not route through runtime `at(tuple, index)`.
    - Preserve existing runtime indexing behavior for arrays, vectors, maps,
      strings, and homogeneous `args<T>` packs.
  - acceptance:
    - `pair[0]` and `pair[1]` on `tuple<i32, string>` resolve to `i32` and
      `string` respectively.
    - Runtime index variables such as `pair[i]` reject with a diagnostic that
      explains tuple indexes must be compile-time constants.
    - Out-of-range tuple indexes reject with the same index provenance as
      `get<I>`.
    - Existing array/vector/map/string/indexed-pack tests keep their current
      runtime-index behavior.
    - `docs/TuplePrototype.md` records bracket indexing as accepted sugar.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once bracket indexing is validated as tuple-specific
    compile-time sugar and ordinary runtime indexing behavior is unchanged.

- [ ] TODO-4277: Add tuple destructuring sugar
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4274
  - scope: Add tuple destructuring binding sugar over ordinary `tuple<...>`
    values without involving task spawning or multi-wait.
  - implementation_notes:
    - Start from parser binding forms, semantic validation, and tuple indexed
      access from TODO-4272/TODO-4274.
    - Follow the bracket-list name binding rule in `docs/PrimeStruct.md`, as
      summarized by `docs/TuplePrototype.md`: known bracket entries keep their
      existing transform/type/modifier meaning, while fresh identifiers in a
      destructuring pattern introduce new bindings.
    - Destructuring should lower to `get<I>` calls, bracket-index facts, or
      equivalent semantic facts over ordinary tuple values; do not introduce a
      separate product type.
    - Keep mutable and borrowed destructuring narrow; split advanced patterns
      into follow-ups if needed.
  - acceptance:
    - tuple destructuring accepts a documented binding form such as
      `[left right] tupleValue` or the accepted final syntax and binds elements
      in tuple order.
    - Parser and semantic diagnostics distinguish destructuring patterns from
      existing transform/type bracket lists, including cases where one bracket
      entry is already known and another is fresh.
    - Destructuring rejects arity mismatch, non-tuple operands, duplicate
      binding names, and unsupported mutable/borrowed patterns with stable
      diagnostics.
    - Destructured element bindings have the precise element types and obey
      normal move/copy/borrow rules.
    - Docs and examples show destructuring as sugar over ordinary stdlib
      `tuple`, not as a separate product type.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once tuple destructuring works for ordinary tuple values;
    leave multi-wait integration to TODO-4278.

- [ ] TODO-4278: Integrate multi-wait with stdlib tuple
  - owner: ai
  - created_at: 2026-04-27
  - phase: Deferred generic tuple substrate
  - depends_on: TODO-4277
  - scope: Wire future multi-task `wait(left, right, ...)` to return ordinary
    `tuple<...>` values instead of a task-specific product type.
  - implementation_notes:
    - Start from `docs/MultithreadingPrototype.md`,
      `docs/TuplePrototype.md`, tuple destructuring from TODO-4277, and task
      handle facts once task support exists.
    - If task spawning/wait is not implemented yet, keep this TODO blocked or
      split the task-side prerequisite into the multithreading lane rather than
      adding placeholder runtime behavior.
  - acceptance:
    - Multi-task `wait(left, right, ...)` returns `tuple<Left, Right, ...>`
      using the stdlib tuple type when task support is present.
    - The result can be consumed through `get<I>`, bracket indexing, and tuple
      destructuring.
    - Multi-wait preserves the structured task lifetime rule from
      `docs/MultithreadingPrototype.md`.
    - `docs/MultithreadingPrototype.md` and `docs/TuplePrototype.md` describe
      the implemented integration and remove prototype-only caveats that are no
      longer true.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once multi-wait returns stdlib `tuple<...>` or the missing
    task-side prerequisite is split into an explicit multithreading TODO.

- [ ] TODO-4281: Lift vector dynamic capacity limit
  - owner: ai
  - created_at: 2026-04-28
  - phase: Deferred dynamic runtime storage follow-up
  - scope: Lift the current VM/native vector local dynamic-capacity limit
    beyond `256` now that vector locals use heap-backed
    `count/capacity/data_ptr` storage and push/reserve growth paths are
    executable.
  - implementation_notes:
    - Start from `src/ir_lowerer/IrLowererHelpers.{h,cpp}`,
      `src/ir_lowerer/IrLowererFlowVectorHelpers.cpp`,
      `src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp`,
      `tests/unit/test_compile_run_vm_collections_vector_limits_a.cpp`, and
      `tests/unit/test_compile_run_native_backend_collections_mutators_and_limits_*.cpp`.
    - Preserve deterministic allocation-failure diagnostics for requests that
      still exceed the widened runtime/allocator contract.
    - Update docs and source locks that mention the `256` ceiling in the same
      change.
  - acceptance:
    - VM and native vector literals, `reserve`, and repeated `push` can grow
      past the old `256` local dynamic-capacity limit for supported element
      kinds.
    - Out-of-range folded and runtime capacity requests still produce stable
      diagnostics instead of silent wraparound or backend faults.
    - Existing ownership, relocation, drop, and borrowed-access behavior stays
      stable across growth.
    - Docs record the new capacity contract and any remaining backend limits.
    - `./scripts/compile.sh --release` passes.
  - stop_rule: Stop once the old `256` ceiling is either removed or replaced by
    a documented wider allocator/runtime bound with VM/native coverage.
