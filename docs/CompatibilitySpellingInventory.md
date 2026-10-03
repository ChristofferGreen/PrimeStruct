# Compatibility spelling inventory

Inventory of the `Legacy` / `Compatibility` / `Removed` / `Retired` identifiers in `src/` and `include/`
(1,795 occurrences on 2026-10-03; the counts in the issue that opened this were for a looser pattern).
It records, per family, what spelling the helpers deal with and what the compiler does with that spelling
today, using `docs/CollectionHelperTargets.md` (the parity matrix) and direct `primec` probes. No code
changes came with it.

How to recount: `grep -rhoE '\b[A-Za-z_]*(Legacy|Compatibility|Removed|Retired)[A-Za-z_0-9]*\b' src include | sort | uniq -c | sort -rn`.

| family | occurrences | top identifiers | spelling it handles | today's behavior | verdict |
| --- | --- | --- | --- | --- | --- |
| std-namespaced / canonical vector compatibility helpers | 363 | `isStdNamespacedVectorCompatibilityHelperPath` (68), `canonicalVectorCompatibilityHelperPathOrFallback` (58), `allowsArrayVectorCompatibilitySuffix` (26) | `/std/collections/vector/<helper>` and bare helper calls that must resolve to the stdlib vector helpers | accepted (`ok` for method, bare, canonical rows) | keep: these are the canonical routing, only misnamed "compatibility" |
| compatibility fallback and diagnostics plumbing | 265 | `SemanticsValidatorInferCollectionCompatibilityInternal` (136 include lines), `TemplateMonomorphCollectionCompatibilityPaths` (33), `CompatibilityRewrite` (18), `shouldAllowStdAccessCompatibilityFallback` (17) | mostly header names and the fallback that lets `/std/...` access helpers resolve | accepted | rename candidate, not deletion |
| legacy SoA helper path canonicalizers | 291 | `isLegacyOrCanonicalSoaHelperPath` (127), `canonicalizeLegacySoaRefHelperPath` (32), `canonicalizeLegacySoaGetHelperPath` (31) | `/soa_vector/<helper>`, `/to_aos`, `/to_aos_ref`, `/get`-style root spellings mapped to `/std/collections/soa/...` | the parity matrix covers only canonical SoA spellings (all `ok`); legacy spellings are not pinned | **needs probes** before any deletion (child leaf) |
| removed vector / array method spellings | 235 | `explicitRemovedMethodPath` (76), `isRemovedVectorCompatibilityHelper` (21), `explicitRemovedCollectionMethodPath` (16) | `/vector/count(v)`, `/array/count(v)`, `v./vector/count()`, `v./array/count()` | rejected: `unknown call target: /vector/count`, `unknown method: /vector/count` (probed) | the helpers exist to emit the removal diagnostic; deletable only by accepting the generic unknown-target message (child leaf) |
| legacy experimental vector compatibility paths | 128 | `isLegacyExperimentalVectorCompatibilityPath` (31), `...TypePath` (24), `legacyExperimentalVectorCompatibilityPrefix` (22) | `/std/collections/experimental_vector/...` spellings | accepted for the experimental vector family used by bridge/bench code | keep until the experimental vector files are retired (AGENTS.md lists them as bridge code) |
| removed key/value (map) compatibility helpers | 116 | `removedKeyValueCompatibilityPath` (26), `isRemovedKeyValueCompatibilityHelper` (21) | `/map/count(m)` | rejected for the call form (`unknown call target: /map/count`), accepted for `m./map/count()` and `m.count()` (probed) | split verdict: call-form rejection is diagnostic-only, the method form is live (child leaf covers the former) |
| retired maybe / mutable helpers | 23 | `maybeFailRetiredMaybeMutableHelperForType` (8) | removed mutable `Maybe` helpers | diagnostic only | small; fold into the removed-spelling leaf |
| benchmark shadows | 24 | `benchmarkSemanticGraphLocalAutoLegacyKeyShadow` (8) | benchmark flags only | benchmark-only | out of scope |
| other | 350 | `LegacyCollectionBranchCounters` (7), `Removed` (8), assorted | mixed | mixed | none |

Child leaves: TODO-5424 (removed vector/array/map call-form spellings), TODO-5425 (legacy SoA helper paths).
