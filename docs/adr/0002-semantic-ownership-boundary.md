# ADR 0002: Semantic Ownership Boundary for Graph Facts

- Status: Accepted
- Date: 2026-04-19
- Deciders: PrimeStruct compiler maintainers

## Context

PrimeStruct's semantic pipeline now has three different kinds of state:
- the type-resolution graph and graph-backed inference facts used while
  validation converges,
- validator-local scratch data used to validate and publish facts, and
- the published semantic product consumed by lowering and backend entrypoints.

That separation had become blurry around graph-local `auto` inference. The
validator kept legacy shadow copies of local/query/`try(...)`/direct-call/
method-call state on the main production validator object so benchmark A/B
experiments could compare older shapes against the new graph-backed facts.

Keeping those legacy shadows adjacent to production validator state made the
ownership boundary harder to reason about and encouraged future production
fallbacks to depend on benchmark-only copies.

## Decision

The semantic ownership boundary is locked as follows:
- The type-resolution graph owns convergence-time local/query/`try(...)`,
  direct-call, and method-call facts while validation is running.
- Validator scratch owns only transient caches, memoization, and publication
  staging needed to validate the current program and publish semantic facts.
- `SemanticProgram` owns the stable published fact surface consumed after
  validation.
- Production lowering, `primec`, `primevm`, and compile-pipeline entrypoints
  must consume published semantic-product facts for covered families instead of
  consulting validator-local shadow copies.
- Any remaining legacy graph-local shadow copies are benchmark-only comparison
  artifacts. They must live behind explicit benchmark-only state and must not
  influence production diagnostics, lowering, or semantic-product publication.

Covered fact families for this decision:
- local `auto` binding facts
- graph-backed query facts
- graph-backed `try(...)` facts
- direct-call targets
- method-call targets

## Consequences

Positive:
- Default production validation carries only authoritative graph facts plus
  transient validator scratch.
- Lowering/publication ownership is easier to audit because benchmark-only
  copies are no longer peers of the production state.
- Future semantic-product cutover work has a clear rule for deleting fallback
  paths instead of preserving them as silent side channels.

Costs:
- Benchmark A/B experiments must route through explicit benchmark-only storage
  and cannot assume the production validator keeps legacy shapes alive by
  default.
- New comparison experiments need to justify their benchmark-only state instead
  of attaching it directly to production-path structures.

## Guardrails

- Do not add new production-path consumers of validator-local legacy shadow
  maps for covered fact families.
- If a benchmark needs legacy comparisons, isolate that state behind an
  explicit benchmark-only holder and keep the data flow one-way from the
  authoritative graph facts.
- When a fact family becomes semantic-product authoritative, production
  lowerer/backends must delete or quarantine any remaining validator-local
  fallback for that family in the same slice or an explicit follow-up TODO.

## Follow-up status (2026-04-19)

- Completed: graph-local legacy shadow storage now lives behind explicit
  benchmark-only state and is allocated only when the benchmark comparison
  flags are enabled.
- Completed: default production validation no longer carries those legacy
  shadow maps on the main validator object.
