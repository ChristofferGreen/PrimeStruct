# ADR 0001: Backend Boundary Is Canonical IR

- Status: Accepted
- Date: 2026-03-06
- Deciders: PrimeStruct compiler maintainers

## Context

PrimeStruct historically had mixed backend entry paths:
- some emit modes used direct AST-backed emission paths,
- newer emit modes used the shared `IrBackend` path after lowering and IR
  validation.

That split made backend behavior harder to reason about because backend-specific
diagnostics, validation, and feature parity depended on which emission path a
mode used.

## Decision

All code generation backends must consume canonical `IrModule` only.

Policy details:
- Frontend stages (`import` expansion, text transforms, parsing, semantic
  transforms, semantic validation) produce canonical AST and then lower once to
  IR.
- Emission stages (`vm`, `native`, `ir`, `wasm`, `glsl`, `spirv`, `cpp`, `exe`,
  and corresponding `*-ir` forms) run through the `IrBackend` abstraction.
- AST-direct backend emission paths are not allowed for production emit modes.
- Backend-specific legality checks must be expressed as IR validation targets,
  not AST-only checks.

## Consequences

Positive:
- One deterministic lowering boundary for all backends.
- Shared validation/inlining/diagnostic behavior across emit modes.
- Fewer backend parity regressions caused by divergent frontend/backend seams.

Costs:
- Backend bring-up requires IR emitter scaffolding before a mode can ship.
- Some backend-specific sugar handling must move earlier into transforms or IR
  lowering.

## Guardrails

- New emit modes must register as `IrBackend` implementations.
- Any proposal for AST-direct emission requires a new ADR that explicitly
  supersedes this decision.

## Follow-up status (2026-03-11)

- Completed: production `primec --emit` aliases (`cpp`, `exe`, `glsl`,
  `spirv`) now resolve to canonical `IrBackend` kinds before backend lookup.
- Completed: `primec` no longer contains production-only AST-precheck
  branches for `cpp`/`exe`; lowering/validation/emit diagnostics now flow
  through backend diagnostics uniformly.
