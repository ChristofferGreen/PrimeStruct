# Collection routing exceptions (lowerer)

`docs/CollectionHelperTargets.md` is generated and records the collection
call targets the compiler publishes. Three predicates in
`src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp` decide how the
lowerer treats a semantic-product method-call target. They were considered
for a (family x helper x receiver kind -> routing) table (TODO-5387) and are
kept as predicates because each one reads state that a table row cannot
carry. Family identity itself now goes through
`collection_helpers::CollectionFamily` (TODO-5386).

| predicate | why it is not a table row |
| --- | --- |
| `allowsReceiverResolvedVectorMetadataFallback` | Keyed on the spelled method path of the concrete call expression (`isCollectionVectorMetadataMethodPath(describeMethodCallExpr(callExpr))`), not on a family or helper: it also decides whether a missing semantic target is an error. |
| `directTargetKeepsSyntheticCollectionFallback` | Depends on the call's simple name, whether the published direct target is blocked as a synthetic fallback, and whether the target is a key/value constructor path; the inputs are per-call and per-definition, not per family. |
| `routesExplicitVectorCountMethodThroughArgsPackCount` | Needs the receiver's declared type text from the semantic program (an `args<...>` pack wraps the array `count` target), which exists only at the call site. |

Rule: add a new predicate of this kind only with a row in this table; if the
decision depends only on (family, helper, receiver kind), put it in the
family registry instead.

## Reachability evidence for the vector method-call branches

`IrLowererLegacyCollectionBranchCounters` (enabled with
`PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS=1`, optionally
`..._LOG_FILE=<path>` to aggregate across processes) counts hits on the
legacy collection-vector branches in `IrLowererSetupTypeMethodCallResolution.cpp`.
A whole-suite `ctest` run on 2026-10-03 recorded, summed per process maxima:

| counter | processes with hits | verdict |
| --- | --- | --- |
| `collection_vector_metadata_method_path_hits` | 381 | live; the fallback above stays |
| `collection_vector_owner_path_hits` (both sites) | 293 | live; stays |
| `..._target_path_site_hits` / `..._fallback_resolved_hits` | 293 / 293 | live; the direct lookup resolves every time |
| `..._receiver_type_site_hits` | 293 | live; stays |
| `..._fallback_nullptr_hits` | 0 | counter deleted; the `return nullptr` it marked is kept because the suite is not proof for user programs |
| struct-slot-layout vector/soa, uninitialized-struct duplicate, divergence log | 0 (no callers) | counters and recorders deleted |

Non-zero branches stay and are documented here; do not delete one without a
parity-matrix run showing the spelling is no longer accepted.
