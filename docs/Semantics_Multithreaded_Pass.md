# Multithreaded Semantics Pass Investigation

Note: this investigation doc is now historical context. The canonical
implementation baseline for Group 15 P4 starts in
`docs/Semantics_Multithread_Design.md`.

Goal: parallelize semantic validation across root functions while keeping
diagnostics deterministic and identical to single-threaded mode.

## Current Constraints
- The `SemanticsValidator` holds mutable state (`currentDefinitionPath_`,
  `entryArgsName_`, `error_`, local binding caches) and assumes a single
  traversal order.
- Multiple passes build shared maps (`definitionMap_`, `structNames_`,
  `returnKinds_`) that are mutated during inference and validation.
- Diagnostics are emitted as a single `error_` string, so the first error
  wins and order depends on traversal.

## Parallelization Strategy (Proposed)
Split the semantics pipeline into serial and parallel phases:

Serial phases:
- Build definition/import maps and aliases.
- Infer unknown return kinds (requires reading definitions and updating
  `returnKinds_`).
- Validate struct layouts (depends on fully populated struct metadata).

Parallel phases (after serial setup):
- Validate definitions: one task per definition.
- Validate executions: one task per execution envelope (if needed).

Each task should operate on immutable program data and a per-task context
that owns its local state. Shared maps are read-only at this stage.

## Deterministic Diagnostics
Introduce a `Diagnostics` container that collects errors per task. Each
diagnostic should carry a stable sort key:
- Definition order index (based on source order in `Program::definitions`).
- Statement index and expression path within the definition.
- Source span start (file index + offset), when spans exist.
- Error code or message as a tie-breaker.

At the end of the parallel phase:
- Merge and stable-sort diagnostics by the key above.
- Emit the first error (or full list when `--emit-diagnostics` is enabled).

## Required Refactors
- Replace the `error_` string with a `Diagnostics` sink.
- Move mutable per-definition fields (current path, locals, scopes) into a
  `DefinitionValidationContext` passed to each task.
- Make helper routines read-only or accept explicit context instead of
  mutating `SemanticsValidator` fields.
- Ensure all shared maps are fully built before the parallel phase begins.

## Threading Model
- Start with a bounded worker pool sized by `--semantics-threads` (default 1).
- Maintain a simple work queue of definitions/executions.
- Use `std::thread` + `std::mutex` + `std::condition_variable`; avoid
  task-stealing to keep ordering assumptions simple.
- Keep diagnostics emission strictly after all tasks complete.

## Test Plan (Future Work)
- Add a regression test that runs validation with `--semantics-threads=1`
  and `--semantics-threads=4` on a multi-definition program and asserts
  identical diagnostics (error count and first error message).
- Add a stress test with randomized definition order to ensure stable
  sorting and no data races.

## Risks
- Subtle shared-state leaks via helper methods.
- Dependency between definition validation and execution validation.
- Error ordering changes when source spans are missing.

## Milestone Proposal
1. Refactor diagnostics and per-task context (single-threaded).
2. Introduce the thread pool with `--semantics-threads` (still default 1).
3. Enable parallel validation behind a flag and add deterministic tests.
