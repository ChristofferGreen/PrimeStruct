# Repository Memories

This file stores durable session-derived facts that are useful in later work. Keep it short, factual, and easy to diff.

## Active Memories

- `experimental-vector-negative-index-guard`: `/std/collections/experimental_vector/*`
  checked access now rejects negative indices via `vectorCheckIndex` (same
  bounds-error path as high indices), and `vectorAtUnsafe` now also rejects
  negative indices explicitly, so negative indices no longer fall through to
  invalid pointer arithmetic/indirect-address traps.
- `experimental-vector-push-growth-overflow-contract`:
  `/std/collections/experimental_vector/vectorPush` now diverts capacity-
  doubling overflow into the shared runtime-contract path instead of letting
  the doubled capacity wrap or continue into unchecked growth.
- `map-bare-access-canonical-helper-contract`: bare-root
  `contains(values, key)`, `at(values, key)`, and `at_unsafe(values, key)`
  on builtin `map<K, V>` now reject with canonical
  `unknown call target: /std/collections/map/*` diagnostics unless a
  canonical helper is imported or explicitly defined, and bare
  `values.at(...)` / `values.at_unsafe(...)` method sugar now follows the
  same-path reject contract instead of falling back to compiler-owned
  access behavior; the C++ emitter now matches that direct-call contract for
  bare `at` / `at_unsafe` calls instead of compiling them through the old
  builtin access fallback.
- `template-monomorph-core-utilities-header`:
  shared TemplateMonomorph core utility helpers now live in
  `src/semantics/TemplateMonomorphCoreUtilities.h` and are included by
  `TemplateMonomorph.cpp`, covering builtin type and collection
  classification plus overload path and import utility helpers.

### template-monomorph-final-orchestration-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Final monomorphization orchestration now routes import-alias setup and definition/execution rewrite passes through `src/semantics/TemplateMonomorphFinalOrchestration.h`.
- Evidence: Group 9 extracted `buildImportAliases(...)`, definition rewrite coordination, and execution rewrite coordination out of `monomorphizeTemplates(...)` into dedicated helper functions.

### template-monomorph-source-definition-setup-header
- Updated: 2026-03-26
- Tags: semantics, template-monomorph, refactor
- Fact: Source-definition collation, helper-overload family expansion, and template-root entry-path validation for monomorphization now run via `src/semantics/TemplateMonomorphSourceDefinitionSetup.h`.
- Evidence: Group 9 extracted the monomorph setup block from `monomorphizeTemplates(...)` into `initializeTemplateMonomorphSourceDefinitions(...)`.

## Maintenance Notes

- Keep entries sorted by slug within the section.
- Delete wrong entries instead of leaving contradictory facts behind.
- Prefer updating an existing entry over adding a near-duplicate.
- Avoid copying obvious facts from `AGENTS.md` or canonical design docs unless the shorter memory adds unique operational value.
