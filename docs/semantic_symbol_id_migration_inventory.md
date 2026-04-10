# Semantic SymbolId Migration Inventory

This inventory tracks semantic-product fact families that still retain
transitional string shadow fields alongside `SymbolId` fields.

Goal: complete one small, code-affecting leaf per family so each slice can be
implemented, parity-tested, and landed independently.

## Completed foundations
- Deterministic compilation-local interning and merge hooks landed (`P3-01` to
  `P3-03`).
- Initial key migrations and first fact-family migrations landed (`P3-04` to
  `P3-07`, `P3-10` to `P3-16`).
- First transitional shadow-field removal landed (`P3-08`): bridge-path
  `chosenPath` now resolves via `chosenPathId` only.
- Direct-call resolved-path shadow removal landed (`P3-17`): direct-call target
  paths now resolve via `resolvedPathId` only.
- Callable-summary full-path shadow removal landed (`P3-20`): callable summaries
  now resolve paths via `fullPathId` only, and lowerer semantic-product
  validation rejects missing/invalid callable-summary path IDs.
- Binding-fact resolved-path shadow removal landed (`P3-21`): binding facts now
  resolve paths via `resolvedPathId` only, and lowerer semantic-product
  validation rejects invalid binding-fact resolved-path IDs.
- Return-fact definition-path shadow removal landed (`P3-22`): return facts now
  resolve definition paths via `definitionPathId` only, and lowerer
  semantic-product validation rejects invalid return-fact definition-path IDs.
- Local-auto initializer-path shadow removal landed (`P3-23`): local-auto facts
  now resolve initializer paths via `initializerResolvedPathId` only, and
  lowerer semantic-product validation rejects invalid local-auto initializer
  path IDs.
- Query-fact resolved-path shadow removal landed (`P3-24`): query facts now
  resolve paths via `resolvedPathId` only, and lowerer semantic-product
  validation rejects invalid query-fact resolved-path IDs.

## Remaining families and next one-leaf follow-ups

| Family | Current state | Next leaf follow-up |
| --- | --- | --- |
| `method_call_targets` | IDs + string shadows | `P3-18`: remove `resolvedPath` shadow and require `resolvedPathId` on semantic-product paths. |
| `bridge_path_choices` | IDs + remaining string shadows | `P3-19`: remove `helperName` shadow and consume `helperNameId` only on semantic-product paths. |
| `try_facts` | IDs + string shadows | `P3-25`: remove `operandResolvedPath` shadow and require `operandResolvedPathId` in semantic-product consumers. |
| `on_error_facts` | IDs + string shadows | `P3-26`: remove `handlerPath` shadow and require `handlerPathId` in semantic-product consumers. |

## Out of scope for this queue
- `definitions`, `executions`, `type_metadata`, and `struct_field_metadata` are
  not currently part of the hot-path SymbolId migration queue.
