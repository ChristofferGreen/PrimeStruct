# PrimeStruct Graphics API Design

Status: Locked (v1 Core Contract)
Last updated: 2026-03-05

This document defines the locked cross-backend graphics API contract for the
spinning-cube program family. The goal is to keep one portable language surface,
apply profile-gated validation at compile time, and avoid backend-specific
language namespaces in v1.

## Scope
- Covers the PrimeStruct language-facing graphics contract only.
- Defines locked Core/API constraints and profile-gating rules.
- Establishes conformance hooks that tests must exercise.

## v1 Principles
- One core graphics language surface under `/std/gfx/*`.
- No backend extension namespace in the language surface for v1.
- Profile differences are enforced with compile-time gating and deterministic
diagnostics.
- Browser, native desktop, and macOS Metal targets share one simulation/data
contract; only host/shader artifacts vary by profile.

## Locked Core Surface (v1)
The locked core object model names are:
- `Buffer<T>`
- `Texture2D<T>`
- `Sampler`
- `ShaderModule`
- `Pipeline`
- `BindGroupLayout`
- `BindGroup`
- `CommandEncoder`
- `RenderPass`
- `Queue`
- `Swapchain`

These names are stable contract anchors. Concrete implementation and lowering
work for these names is staged by follow-up TODO items.

## Locked Profile Set (v1)
The graphics profiles tracked by this contract are:
- `wasm-browser`
- `native-desktop`
- `metal-osx`

Current compiler flags may expose profile selectors differently (for example,
`--wasm-profile browser` with alias `wasm-browser`). The contract profile names
above remain the canonical design identifiers.

## Locked Constraints and Conformance Hooks

| ID | Locked Constraint | Conformance Hook |
| --- | --- | --- |
| `GFX-CORE-API-NAMESPACE` | Graphics API surface is rooted at `/std/gfx/*` and does not use backend-specific language namespaces. | `graphics_api_contract_doc_linked_constraints` (doc lock + unsupported backend emit check) |
| `GFX-CORE-SURFACE-V1` | The v1 core object model names listed in this document are locked. | `graphics_api_contract_doc_linked_constraints` (doc lock checks) |
| `GFX-CORE-NO-EXT-NS` | `/std/gfx/ext/*` is forbidden in v1. | `graphics_api_contract_doc_linked_constraints` (compile diagnostic check) |
| `GFX-PROFILE-IDENTIFIERS` | Profile identifiers are locked to `wasm-browser`, `native-desktop`, `metal-osx`. | `graphics_api_contract_doc_linked_constraints` (profile value rejection check) |
| `GFX-PROFILE-GATING` | Unsupported profile features fail at compile time before backend emission. | `graphics_api_contract_doc_linked_constraints` (wasm-browser reject check) |
| `GFX-DIAG-PROFILE-CONTEXT` | Profile-gated failures must include deterministic profile/backend context in diagnostics. | `graphics_api_contract_doc_linked_constraints` (diagnostic content check) |

## Non-goals (v1)
- No `/std/gfx/ext/*` backend extension namespace.
- No requirement for parity with advanced backend-specific features.
- No expansion to additional backend profile names without contract update and
conformance tests.

## Change Control
Any change to this contract requires:
1. Update this document (including locked IDs or rationale).
2. Update doc-linked conformance tests for every affected locked constraint.
3. Update `docs/todo.md` with explicit follow-up work if implementation is
split across multiple PRs.
