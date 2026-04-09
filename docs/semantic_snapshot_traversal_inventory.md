# Semantic Snapshot Traversal Inventory

## Scope

This note inventories snapshot collectors in
`src/semantics/SemanticsValidatorSnapshots.cpp` by traversal shape, then
selects the first merge target for Group 15 `P2-02`.

## Traversal Shapes

| Shape ID | Traversal skeleton | Primary helpers | Snapshot collectors |
|---|---|---|---|
| `S1` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferQuerySnapshotData(...)` | `queryCallTypeSnapshotForTesting`, `queryBindingSnapshotForTesting`, `queryResultTypeSnapshotForTesting`, `queryFactSnapshotForSemanticProduct`, `queryReceiverBindingSnapshotForTesting` |
| `S2` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferCallSnapshotData(...)` | `callBindingSnapshotForTesting` |
| `S3` | `forEachLocalAwareSnapshotCall(...)` over definition call expressions with local-binding context | `inferTrySnapshotData(...)` | `tryValueSnapshotForTesting` (`tryFactSnapshotForSemanticProduct` forwards to this) |
| `S4` | Manual recursive AST walk over `definitions.parameters`, `definitions.statements`, `definitions.returnExpr`, `executions.arguments`, `executions.bodyArguments` | `resolveCalleePath(...)` | `directCallTargetSnapshotForSemanticProduct`, `bridgePathChoiceSnapshotForSemanticProduct` |
| `S5` | Flat definition/execution loops (no call-expression recursion) | `buildDefinitionValidationState(...)`, `buildExecutionValidationState(...)` | `returnResolutionSnapshotForTesting`, `returnFactSnapshotForSemanticProduct`, `callableSummarySnapshotForSemanticProduct`, `typeMetadataSnapshotForSemanticProduct`, `onErrorSnapshotForTesting`, `validationContextSnapshotForTesting` |
| `S6` | Definition + statement-field loops (struct/enums/binding walks) | `resolveStructFieldBinding(...)`, local binding parsing/inference helpers | `structFieldMetadataSnapshotForSemanticProduct`, `bindingFactSnapshotForSemanticProduct`, `localAutoBindingSnapshotForTesting` (`localAutoFactSnapshotForSemanticProduct` forwards to this) |

## Highest-Overlap Pair Selection

Selected pair for `P2-02`:

1. `directCallTargetSnapshotForSemanticProduct`
2. `bridgePathChoiceSnapshotForSemanticProduct`

### Why this pair first

- They duplicate the same recursive walk shape (`S4`) over the same AST
  surfaces:
  - `directCallTargetSnapshotForSemanticProduct`: lines `603-642`
  - `bridgePathChoiceSnapshotForSemanticProduct`: lines `737-778`
- Both gate on the same non-method call predicate and derive from
  `resolveCalleePath(...)`; the bridge collector only adds one extra filter
  (`collectionBridgeChoiceFromResolvedPath(...)`).
- Both apply the same initial ordering prefix (`scopePath`, `sourceLine`,
  `sourceColumn`) before collector-specific tiebreakers.

This makes the pair low-risk for a first shared-traversal merge with clear
output-order parity checks in `P2-03`.

## Next Pair Candidate (for `P2-04`)

Likely second merge candidate after `S4`:

- `queryFactSnapshotForSemanticProduct`
- `queryReceiverBindingSnapshotForTesting`

Both run `forEachLocalAwareSnapshotCall(...)` + `inferQuerySnapshotData(...)`
and differ only in projection/filtering of the same `QuerySnapshotData` payload.
