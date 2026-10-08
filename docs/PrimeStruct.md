# PrimeStruct Plan

PrimeStruct is built around a simple idea: program meaning comes from two primitives—**definitions** (potential) and
**executions** (actual). Both map to a single canonical **Envelope** in the AST; executions are envelopes with an
implicit empty body. Compile-time transforms rewrite the surface into a small canonical core. That core is what we
target to C++, GLSL, and the PrimeStruct VM. It also keeps semantics deterministic and leaves room for future tooling
and visual editors.

At a glance:
- One canonical envelope model underlies both definitions and executions.
- Surface syntax is convenience only; transforms rewrite it into a smaller deterministic core.
- Backends target the shared canonical representation rather than reinterpreting surface syntax.

Related design notes:
- `docs/MemoryCapabilities.md` sketches a capability-checked memory-safety direction where ownership, references,
  effects, escape control, and unsafe raw-address authority share one inherited-authority model.
- `docs/SafeArrayExtentViews.md` records a non-normative design direction for known array extents, runtime
  preconditions, slices, and borrowed views inspired by Nagle's safe-array proposal for C. Its
  `require<...>` / `require(...)` phase split is now the language direction recorded below; the
  remaining extent and view surfaces are still design notes until their TODO leaves land.

## Specification index

This file is the entry point. The specification text lives in `docs/spec/`, split along the original section
boundaries without edits to the text itself; `scripts/check_spec_docs.py` (ctest `PrimeStruct_spec_docs`) checks that
every part is listed here and that relative links and anchors resolve. Classifications: **normative** (language
rules), **normative (draft)** (rules still marked draft), **design direction** (planned, not fully implemented),
**implementation note** (current compiler/VM behavior), **examples**, **roadmap / history** (phases, risks, next steps).

| Part | Classification | Lines |
| --- | --- | --- |
| [Source Pipeline and Language Levels](spec/source-pipeline.md) | normative | 82 |
| [Planned Type-Resolution Graph](spec/type-resolution-graph.md) | design direction | 1198 |
| [Planned Semantics-to-Lowering Boundary](spec/semantics-lowering-boundary.md) | design direction | 779 |
| [Project Phases, Charter, and Risk Log](spec/project-phases.md) | roadmap / history | 182 |
| [Goals and Proposed Architecture](spec/architecture.md) | implementation note | 198 |
| [Language Design Highlights](spec/language-core.md) | normative | 310 |
| [Backend Type Support](spec/backend-type-support.md) | implementation note | 111 |
| [Transforms and Surface Syntax](spec/transforms.md) | normative (draft) | 395 |
| [Host Functions and Core Library Surface](spec/host-and-core-library.md) | normative (draft) | 213 |
| [Standard Library Reference](spec/stdlib-reference.md) | normative (draft) | 364 |
| [Error Handling and File I/O](spec/errors-and-file-io.md) | normative (draft) | 182 |
| [Runtime Stack Model](spec/runtime-model.md) | implementation note | 114 |
| [Type System v1](spec/type-system.md) | normative (draft) | 647 |
| [Move/Copy/Destroy, Optional Values, and Lambdas](spec/value-lifecycle.md) | normative | 172 |
| [Literals and Composite Construction](spec/literals-and-composite-construction.md) | normative | 403 |
| [Pointers and References](spec/pointers-and-references.md) | normative | 255 |
| [Strings, Text and Slices](spec/strings-and-views.md) | design direction | 195 |
| [VM Design](spec/vm-design.md) | implementation note | 99 |
| [Examples (sketch)](spec/examples.md) | examples | 109 |
| [Integration Points and Tooling](spec/integration-and-tooling.md) | implementation note / roadmap | 109 |
| [Dependencies, Risks, and Next Steps](spec/roadmap.md) | roadmap / history | 33 |
