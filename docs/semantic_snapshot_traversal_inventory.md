# Semantic Snapshot Traversal Inventory

## Scope

This note fulfills Group 15 `P2-01` by inventorying snapshot collectors in
`src/semantics/SemanticsValidatorSnapshots.cpp` by traversal shape and then
selecting the highest-overlap merge pair for `P2-02`.

## Traversal Shapes

| Shape ID | Traversal skeleton | Primary helpers | Snapshot collectors |
|---|---|---|---|
| `S1` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferQuerySnapshotData(...)` | `queryCallTypeSnapshotForTesting`, `queryBindingSnapshotForTesting`, `queryResultTypeSnapshotForTesting`, `queryFactSnapshotForSemanticProduct`, `queryReceiverBindingSnapshotForTesting` |
| `S2` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferCallSnapshotData(...)` | `callBindingSnapshotForTesting` |
| `S3` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferTrySnapshotData(...)` | `tryValueSnapshotForTesting` (`tryFactSnapshotForSemanticProduct` forwards to this) |
| `S4` | Manual recursive AST walk over `definitions.parameters`, `definitions.statements`, `definitions.returnExpr`, `executions.arguments`, `executions.bodyArguments` | `resolveCalleePath(...)` | `directCallTargetSnapshotForSemanticProduct`, `bridgePathChoiceSnapshotForSemanticProduct` |
| `S5` | Flat definition/execution loops (no call-expression recursion) | `buildDefinitionValidationState(...)`, `buildExecutionValidationState(...)` | `returnResolutionSnapshotForTesting`, `returnFactSnapshotForSemanticProduct`, `callableSummarySnapshotForSemanticProduct`, `typeMetadataSnapshotForSemanticProduct`, `onErrorSnapshotForTesting`, `validationContextSnapshotForTesting` |
| `S6` | Definition + statement-field loops (struct/enums/binding walks) | `resolveStructFieldBinding(...)`, local binding parsing/inference helpers | `structFieldMetadataSnapshotForSemanticProduct`, `bindingFactSnapshotForSemanticProduct`, `localAutoBindingSnapshotForTesting` (`localAutoFactSnapshotForSemanticProduct` forwards to this) |

## Overlap Ranking

Scoring rubric (`0-10` total):

- Traversal skeleton reuse (`0-3`)
- Shared helper path reuse (`0-3`)
- Ordering-key overlap (`0-2`)
- Collector-specific branching delta (`0-2`, higher means lower delta)

| Rank | Candidate pair | Score | Rationale |
|---|---|---:|---|
| 1 | `directCallTargetSnapshotForSemanticProduct` + `bridgePathChoiceSnapshotForSemanticProduct` | 10 | Same `S4` recursive traversal, same `resolveCalleePath(...)` source, same ordering prefix, and only one bridge-specific filter delta (`collectionBridgeChoiceFromResolvedPath(...)`). |
| 2 | `queryFactSnapshotForSemanticProduct` + `queryReceiverBindingSnapshotForTesting` | 8 | Same `S1` call traversal and query inference substrate, but receiver-binding output adds extra field-level shaping and test-facing payload differences. |
| 3 | `callBindingSnapshotForTesting` + `tryValueSnapshotForTesting` | 6 | Both are local-aware call traversals, but they use different inference helpers (`inferCallSnapshotData(...)` vs `inferTrySnapshotData(...)`) and have less direct ordering/output overlap. |

## Highest-Overlap Pair Selection

Selected pair for `P2-02`:

1. `directCallTargetSnapshotForSemanticProduct`
2. `bridgePathChoiceSnapshotForSemanticProduct`

### Why this pair first

- They duplicate the same recursive walk shape (`S4`) over the same AST
  surfaces:
  - `directCallTargetSnapshotForSemanticProduct`: `src/semantics/SemanticsValidatorSnapshots.cpp:472`
  - `bridgePathChoiceSnapshotForSemanticProduct`: `src/semantics/SemanticsValidatorSnapshots.cpp:582`
- Both gate on the same non-method call predicate and derive from
  `resolveCalleePath(...)`; the bridge collector only adds one extra filter
  (`collectionBridgeChoiceFromResolvedPath(...)`).
- Both apply the same initial ordering prefix (`scopePath`, `sourceLine`,
  `sourceColumn`) before collector-specific tiebreakers.

This makes the pair low-risk for a first shared-traversal merge with clear
output-order parity checks in `P2-03`.

## Implementation Status (Group 15 P2)

- `P2-02` implemented in `src/semantics/SemanticsValidate.cpp`:
  - when both `direct_call_targets` and `bridge_path_choices` collectors are
    enabled, bridge snapshots are now derived from the already-collected direct
    call snapshots inside semantic-product build, avoiding a second redundant
    snapshot traversal in that hot path.
  - fallback to `validator.bridgePathChoiceSnapshotForSemanticProduct()` is
    preserved when direct-call collection is not enabled.
- `P2-03` parity coverage added:
  - `tests/unit/test_ir_pipeline_backends_registry.cpp`:
    `compile pipeline direct and bridge collector merge keeps output-order parity`
    compares combined collector output order against direct-only and
    bridge-only runs.
- `P2-04` implemented in `src/semantics/SemanticsValidatorSnapshotLocals.cpp` +
  `src/semantics/SemanticsValidatorSnapshots.cpp`:
  - query snapshot collection now runs once through
    `ensureQuerySnapshotFactCaches()` and populates cached outputs for:
    `queryCallTypeSnapshotForTesting`, `queryBindingSnapshotForTesting`,
    `queryResultTypeSnapshotForTesting`, `queryFactSnapshotForSemanticProduct`,
    and `queryReceiverBindingSnapshotForTesting`.
- `P2-05` parity/guard coverage added:
  - `tests/unit/test_ir_pipeline_backends_graph_contexts.h`:
    `semantic snapshot shared traversal keeps query fact and receiver ordering keys`
    now also guards cached shared traversal wiring for query-call-type,
    query-binding, and query-result outputs.
- Additional traversal-churn follow-up implemented:
  - call + try snapshot collection now runs once through
    `ensureCallAndTrySnapshotFactCaches()` and populates cached outputs for:
    `callBindingSnapshotForTesting` and `tryValueSnapshotForTesting`.
  - guard coverage:
    `semantic snapshot shared traversal keeps call and try ordering keys`
    in `tests/unit/test_ir_pipeline_backends_graph_contexts.h`.

## Next Pair Candidate (post query + call/try merges)

Likely next merge candidate:

- `methodCallTargetSnapshotForSemanticProduct`
- `queryReceiverBindingSnapshotForTesting`

Both run `forEachLocalAwareSnapshotCall(...)` and perform receiver-binding
inference on overlapping call-site traversals.
