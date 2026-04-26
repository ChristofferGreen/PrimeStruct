# Semantic SymbolId Migration Inventory

This inventory records the completed semantic-product fact-family migration from
transitional string shadow fields to `SymbolId` fields.

Goal: preserve the landed one-leaf-per-family history so future shadow-field or
fact-family changes start from a fresh TODO instead of reopening the completed
hot-path migration queue implicitly.

## Completed foundations
- Deterministic compilation-local interning and merge hooks landed (`P3-01` to
  `P3-03`).
- Initial key migrations and first fact-family migrations landed (`P3-04` to
  `P3-07`, `P3-10` to `P3-16`).
- First transitional shadow-field removal landed (`P3-08`): bridge-path
  `chosenPath` now resolves via `chosenPathId` only.
- Direct-call resolved-path shadow removal landed (`P3-17`): direct-call target
  paths now resolve via `resolvedPathId` only.
- Method-call resolved-path shadow removal landed (`P3-18`): method-call
  targets now resolve via `resolvedPathId` only, and lowerer semantic-product
  validation rejects invalid method-call resolved-path IDs.
- Bridge helper-name shadow removal landed (`P3-19`): bridge-path choices now
  resolve helper names via `helperNameId` only, and lowerer semantic-product
  validation rejects invalid bridge helper-name IDs.
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
- Try-fact operand-path shadow removal landed (`P3-25`): try facts now resolve
  operand paths via `operandResolvedPathId` only, and lowerer semantic-product
  validation rejects invalid try-fact operand-path IDs.
- On-error handler-path shadow removal landed (`P3-26`): on-error facts now
  resolve handlers via `handlerPathId` only, and lowerer semantic-product
  validation rejects invalid on-error handler-path IDs.

## Inactive Follow-Up Status

None. The current hot-path SymbolId migration family list is fully landed.
Add a concrete SymbolId migration TODO before adding another fact family to this
inventory or reintroducing transitional string shadow fields.

## Out of scope for this queue
- `definitions`, `executions`, `type_metadata`, and `struct_field_metadata` are
  not currently part of the hot-path SymbolId migration queue.
