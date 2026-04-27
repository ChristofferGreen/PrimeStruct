# Semantics Multithread Design (P4-01)

Status: design baseline for Group 15 P4.

Goal: define a deterministic multithread semantics architecture that preserves
single-thread behavior while enabling parallel definition validation.

## Scope

This note defines:
- Phase boundaries for semantics validation and semantic-product publication.
- Immutable shared state vs thread-local mutable state.
- Deterministic merge boundaries between worker outputs.

This note does not define:
- Final interner merge tie-break rules (P4-02).
- Concrete worker-count runtime flags or CLI wiring.
- Final implementation details of partitioning and scheduler internals.

## Invariants

- Visible behavior parity: multithread mode must produce the same diagnostics
  and semantic-product output as single-thread mode for the same input.
- Deterministic output: worker scheduling order must not affect emitted order.
- No shared hot-path mutex for per-expression/per-statement semantic work.
- `Semantics::validate` remains the single publish boundary for semantic output.

## Phase Boundaries

## Phase A: Serial Prepass (Build Immutable Inputs)

Inputs:
- Canonical AST (`Program`) after existing semantic transforms.
- Import/module inventory and compile options.

Outputs (immutable for worker phase):
- Definition and execution index with stable order keys.
- Import/alias resolution tables.
- Read-only type/layout registries needed for validation.
- Read-only configuration and builtin capability tables.

Rules:
- No worker tasks run until this prepass snapshot is complete.
- Any data needed by workers must be finalized here or carried in task input.

## Phase B: Parallel Validation (Thread-Local Mutation Only)

Unit of work:
- Deterministic partitions of definitions/executions (partition rules in P4-09).

Each worker owns:
- Worker-local validation context.
- Worker-local symbol interner.
- Worker-local diagnostics buffer.
- Worker-local fact buffers for semantic-product families.

Allowed behavior:
- Read immutable shared prepass state.
- Mutate only worker-local state.

Disallowed behavior:
- Mutating shared registries/maps during validation.
- Publishing diagnostics/facts directly to global output containers.

## Phase C: Deterministic Merge (Single Writer)

Inputs:
- `WorkerResult` records from all workers:
  - diagnostics
  - semantic facts
  - worker-local interned symbol tables

Outputs:
- One deterministic merged diagnostics stream.
- One deterministic merged semantic-product fact stream.
- One merged symbol-id view (merge policy defined in P4-02..P4-06).

Rules:
- Merge runs with a single writer.
- Merge order follows deterministic keys only (never completion time).
- Conflict handling must match single-thread contradiction/error policy.

## Phase D: Final Publication

Steps:
- Apply existing first-error policy over merged diagnostics.
- Build `SemanticProgram` from merged facts.
- Expose output through existing compile pipeline boundaries.

Rule:
- Post-publication structures are immutable to downstream stages.

## State Ownership Model

## Immutable Shared State (read-only in workers)

- Canonical AST and source/provenance backing structures.
- Definition/execution stable-order index.
- Import/alias resolution results.
- Canonical type/layout registries.
- Compile options and capability tables.

## Thread-Local Mutable State

- Current definition/execution validation context.
- Local binding/query/try/on_error transient maps.
- Temporary inference/scratch containers.
- Worker-local diagnostics and semantic-fact buffers.
- Worker-local string interner state.

## Merge-Only Mutable State (single-writer boundary)

- Global diagnostics output ordering.
- Global semantic-product ordering/publication vectors.
- Global merged symbol-id map/table.

## Worker Result Contract

Each worker returns a deterministic `WorkerResult` bundle. Current definition
validation uses `SemanticDefinitionWorkerResultBundle` for this boundary:
- `partitionKey`: deterministic partition identity.
- `diagnostics[]`: with stable per-diagnostic ordering keys.
- validation counters and execution/callable-summary slices.
- migrated semantic facts grouped by fact family with stable intra-partition
  order.
- `symbolTable[]`: worker-local interned strings in stable local-id order.

Merge requirements:
- Equivalent inputs produce equivalent merged bundles regardless of thread
  count and worker completion order.

## Deterministic Interner Merge Rules (P4-02)

This section defines the merge contract for worker-local symbol interners.

## Definitions

- `SymbolId`: compilation-local non-zero id.
- `0` is always invalid and is never assigned to real text.
- `SymbolOriginKey`: stable semantic occurrence key attached to intern calls:
  module order, definition order, semantic-node order, fact-family field order.

`SymbolOriginKey` must be independent of worker count and scheduling.

## Worker Output Requirements

Each worker exports:
- `localSymbols[]`: local-id-ordered text table.
- `firstOriginByLocalId[]`: earliest `SymbolOriginKey` seen for each local id.

Within a worker:
- local ids are assigned in local first-seen order.
- duplicate text in one worker maps to one local id.
- empty text is not interned and resolves to id `0`.

## Global Merge Algorithm

1. Build one candidate record per local symbol:
   `Candidate{text, firstOrigin, partitionKey, localId}`.
2. Group candidates by exact text bytes.
3. For each text group, select one canonical representative:
   - lowest `firstOrigin`
   - tie-break: lowest `partitionKey`
   - tie-break: lowest `localId`
4. Sort unique texts into final global order by:
   - representative `firstOrigin`
   - tie-break: text bytewise lexical order
5. Assign global `SymbolId` sequentially from `1..N` in sorted order.
6. Build per-worker remap tables from local id to global id.

This guarantees that the merged id map depends only on stable semantic order,
not worker completion time.

## Collision and Equality Rules

- Text equality is exact byte equality.
- Hash collisions in merge maps must be resolved by full byte comparison.
- Two different texts may not collapse even if hashes collide.
- If two records share identical `firstOrigin` unexpectedly, preserve
  determinism with tie-breaks (`partitionKey`, `localId`, then text lexical).

## Determinism Requirements

- For the same canonical input, merged global symbol table and remap tables
  must be identical across repeated runs and worker counts (`1`, `2`, `4`).
- Merge must be single-writer; no concurrent mutation of global symbol table.
- Merge must not depend on iteration order from unordered containers.

## Compatibility Rules

- Existing single-thread semantics remain source-of-truth behavior.
- Multithreaded merge must preserve existing semantic-product text output.
- Diagnostics and semantic-product facts continue to resolve text via merged
  `SymbolId` without changing visible ordering contracts.

## Diagnostic and Fact Ordering

- Diagnostics are sorted by a deterministic composite key:
  module order, definition order, local semantic node/order key, diagnostic
  class/code tie-breakers.
- Fact family output order follows deterministic semantic ordering keys, not
  worker completion order.
- First-error behavior remains unchanged from current single-thread mode.

## Rollout Plan Mapping (P4)

- P4-02: define interner-merge ordering/tie-break rules.
- P4-03..P4-06: implement + validate deterministic interner merge.
- P4-07..P4-08: extract and route through read-only prepass.
- P4-09..P4-14: deterministic partitioning/scheduling + equivalence tests.
- P4-15..P4-16: stress/TSAN coverage for race and determinism confidence.

## Optional CI Gate (P4-16)

The ThreadSanitizer smoke test is feature-flagged so default local and default
CI jobs do not pay the sanitizer cost.

- CMake option: `PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON`
- CTest label: `optional-ci` (plus `tsan`, `serial-required`)
- Smoke test name: `PrimeStruct_semantics_tsan_smoke`

Recommended optional-CI invocation:

```bash
./scripts/run_semantics_tsan_smoke.sh
```

This configures a dedicated TSan build directory, builds the smoke target, and
runs only the deterministic multithread semantics smoke suite.

## Acceptance Criteria for This Design Note

- Phase boundaries are explicit and implementation-addressable.
- Shared immutable vs thread-local mutable state is explicit.
- Merge boundary and determinism constraints are explicit.
- The document can be used as the implementation contract for P4-02 onward.
