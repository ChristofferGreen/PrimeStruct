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
