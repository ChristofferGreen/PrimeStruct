# AI Workflow: Plan

Classification: **design direction**. Nothing here is implemented or scheduled yet; the slices in section 9 become
`docs/todo.md` entries once the open decisions in section 10 are made.

## 1. Goal

PrimeStruct should suit a workflow where an AI writes most of the code and a person directs and reviews it. The
person no longer reads to learn how the code works; they read to decide whether to trust it. That is four questions:

1. **What exists?** The components of a project and their public surface.
2. **What changed?** Since the last AI turn or commit.
3. **What can it do?** Files, network, global state, tasks.
4. **What is the evidence that it is right?** Tests, contracts, proofs.

The plan has three parts that share one data source:

- a compact, machine-readable **outline** of a project that the compiler emits (section 3),
- a **box view** in PrimeScroll that draws the outline and colors it through lenses (sections 4 and 5),
- **language and tooling changes** that make AI-written code safer to write and faster to review (sections 6 to 8).

The AI reads the same outline the person sees. It learns a project's shape from signatures instead of bodies, and it
gets lint findings as diagnostics, so it corrects them before the person sees the code.

## 2. What exists today

- **Slash paths and namespaces** (`/std/ui/native/...`) give every definition a place in a hierarchy.
- **Effects** (`[effects(net_io, file_write)]`) are checked in semantics. A definition without `[effects]` gets the
  default set (`io_out`). Only effects a callee *declares* are required of its caller; the transitive rule is
  deferred (`docs/spec/language-core.md`, Callee effects).
- **`public` / `private`** on definitions and fields.
- **Four language levels** (0.Concrete to 3.Surface) with deterministic lowering, and `--dump-stage` for `ast`,
  `ast-semantic`, `semantic-product` and the IR stages.
- **`--dump-stage semantic-product`** prints definitions with source positions, resolved call targets, active effects
  per callable, struct fields and binding facts. It is complete but verbose: about 45 lines for a 6-line program.
- **`--emit-diagnostics`** gives JSON diagnostics.
- **`require<...>`** (compile time) and **`require(...)`** (runtime-capable) are the recorded direction for
  preconditions (`docs/spec/type-resolution-graph.md`).
- **The VM** is deterministic, has per-instruction debug hooks and source maps.
- **PrimeScroll** has a web view (the markdown preview) and the embedded compiler (the script runner).

What is missing:

- Comments are dropped by the lexer, so a definition has no summary a tool can show.
- User projects have no test construct, no postconditions, no formatter and no style checker.
- `--dump-stage` cannot be limited to one definition.
- PrimeScroll itself is one file with about 183 definitions in the root namespace. A box view is only as good as the
  namespace structure of the project it shows.

## 3. The outline

`primec --emit=outline` writes one JSON document for the whole program after semantic validation. Per definition:

| Field | Source |
| --- | --- |
| Full path, kind (function, struct, enum, method), visibility | semantic product |
| Signature with concrete or template types | semantic product |
| Summary line | doc comment (section 6.2) |
| Source unit and span | provenance ledger |
| Declared effects, used effects, transitive effects | semantic product plus a call-graph closure |
| Purity level (section 5.1) | derived |
| Callees and callers by path | `direct_call_targets` and method-call facts |
| Contracts (`require`, `ensure`) as source text | AST |
| Tests that reach it, instruction coverage | coverage run (section 5.2), optional input |
| Trust level (section 5.3) | derived |
| Raisable-construct count (section 5.4) | lint |
| Canonical-form hash | canonical AST, for the change lens |

Rules:

- Output is deterministic: definitions sorted by path, keys in a fixed order.
- The format carries a version number.
- A namespace node aggregates its children: worst purity, lowest trust, summed counts.
- The outline is the only interface between the compiler and the view. The box view does not parse source.

## 4. The box view

A view in PrimeScroll for a `.prime` project.

- **Hierarchy:** namespace, then struct, then definition. Boxes nest.
- **Box face:** name, summary line, signature for a leaf, effect badges ("touches the network", "writes files",
  "writes global state"), a public or private mark.
- **Navigation:** clicking a box opens it; clicking a leaf opens the source at its span.
- **Zoom on a leaf:** signature, surface body, body with inferred types shown inline, full lowered form
  (section 7).
- **No call-edge drawing in the first version.** Callers and callees are lists on the selected box. Graph layout is
  the expensive part and is often unreadable.
- **Rendering:** HTML in the existing web view, written with `/std/html`, fed by the outline from the embedded
  compiler. This needs no new drawing stack.
- **Startup:** the view and the compiler work behind it load only when the view is first opened, so the editor's
  launch time does not change.

## 5. Lenses

A lens colors every box by one property. One lens is active at a time, because a box carries one color legibly. Every
color has a tooltip that names the definition responsible, so a red namespace leads to the one function that makes
it red.

### 5.1 Purity

| Level | Meaning |
| --- | --- |
| Pure | No effects and no mutable borrows; the result depends only on the arguments |
| Allocates | `heap_alloc` only |
| Mutates its arguments | `mut` or reference parameters it writes through |
| Mutates global state | `global_write` |
| Touches the world | file, network, task, GPU |

- The level comes from what the body *uses*, closed over the call graph, not from what it declares. This separates
  "pure" from "not annotated" and covers the deferred transitive rule.
- A `[pure]` transform lets an author assert the first level; the compiler rejects a body that breaks it. This lets
  the person tell the AI "this module stays pure" and have it enforced.

### 5.2 Coverage

- **A test construct for user code:** a `[test]` transform on a definition marks it as a test; `primec --test` runs
  them on the VM.
- **Measurement:** instruction hit counts from the VM's debug hooks, mapped to definitions through the source map.
  No instrumentation pass.
- **Shown per box:** the share of instructions reached, and the list of tests that reach it. The list matters more
  than the number, because a test can execute code without asserting anything about it.

### 5.3 Trust

A ladder, so the lens is useful before any proof support exists:

| Level | Evidence |
| --- | --- |
| None | It compiles |
| Tested | Reached by at least one test |
| Contracted | Has `require` or `ensure` contracts, checked at run time |
| Property-tested | Contracts exercised with generated inputs |
| Proved | Contracts discharged statically |

- `ensure(...)` is the postcondition form next to `require(...)`. Contracts are part of a definition's public
  signature and appear on the box face.
- A proved box states its scope: which properties, and how many unproved callees it assumes.
- Static proof is the last step and applies first to pure definitions over fixed-width integers and bounded arrays,
  by emitting verification conditions from the level-0 form to an existing solver. This is far off; the first three
  levels need no prover.

### 5.4 Level

- Colors boxes that deliberately drop below surface syntax: explicit canonical calls, `Pointer`, `unsafe`,
  hand-written template arguments. This is where careful reading is needed.
- Shows the count of raisable constructs (section 8) with a fix-all action.

### 5.5 Change

- Compares canonical-form hashes against a base (the last commit by default), so a respelling shows as no change
  and a one-character edit that flips an inferred type shows as a change.
- Distinguishes, from loudest to quietest: contract changed, effects changed, signature changed, body changed,
  re-instantiated because of a change elsewhere.

### 5.6 The review queue

A combined filter rather than a color: changed boxes that are impure and have trust level None or Tested. This is the
list of code a person should read themselves.

## 6. Language changes

### 6.1 Commas separate arguments

Today a comma between digits is a digit separator, so `add(1,2)` is `add(12)` and the error is an argument count
mismatch. Decision: a comma always separates. No `.prime` file under `stdlib/` or `examples/` uses a digit comma.

The digit separator becomes `_` (`1_000_000`, `0xFF_FFu64`), between digits only, in the integer part of integer
and float literals. A dot was rejected because it collides with float literals: `1.000` already means 1.0.

### 6.2 Doc comments

A comment form the lexer keeps and attaches to the next definition or field (`///`, or a `[doc("...")]` transform).
The first line is the summary on the box face. A lint can require one on every public definition.

### 6.3 `[pure]`, `[test]`, `ensure(...)`

As described in sections 5.1, 5.2 and 5.3.

### 6.4 Explicit boundaries

A project setting: public signatures and struct fields carry explicit types; bodies may infer. Box faces stay
precise and inference cannot silently change a public type.

## 7. Lowered view

- `--dump-stage` gains a filter by definition path, so the editor asks for one definition.
- PrimeScroll shows the lowered form of the definition under the cursor in a side pane, or as a toggle.
- For a templated definition the pane shows one concrete instance, chosen from the instances the program has.
- Inline inferred types (the step between surface and lowered) need per-expression source spans through the
  transforms; this depends on the source-map phase in `docs/spec/integration-and-tooling.md`.

## 8. Formatter and raise lint

- **Formatter:** one canonical surface spelling (transform placement, method or free call, spacing), so diffs and
  reading are uniform whoever wrote the code.
- **Raise lint:** a construct is unnecessarily low-level when its surface spelling lowers to the identical canonical
  form. The lint rewrites a candidate, lowers both and compares, so every finding has a safe automatic fix.
  Examples: `0i32` for `0`, `plus(a, b)` for `a + b`, `at(values, i)` for `values[i]`, the canonical `if(...)` call,
  a binding type that inference gives anyway, template arguments that are inferred anyway.
- **Exemptions:** explicit types on public signatures and fields (section 6.4), and files classed as bridge or
  substrate code in `docs/CodeExamples.md`.
- **Delivery:** findings come through `--emit-diagnostics`, so an AI fixes them before the person reviews.

## 9. Slices, in order

Each is one or more TODO leaves when scheduled.

1. **Comma fix** (6.1). Small and independent; removes a trap every AI falls into.
2. **Doc comments** (6.2).
3. **Outline v1** (section 3): paths, kinds, visibility, signatures, summaries, spans, declared and transitive
   effects, callees.
4. **Box view v1** (section 4): hierarchy, box faces with effect badges, click to source.
5. **Purity lens** (5.1), then `[pure]`.
6. **Change lens** (5.5) and canonical hashes in the outline.
7. **Lowered view** (section 7), definition-filtered dump first.
8. **Raise lint** and **level lens** (section 8, 5.4).
9. **`[test]` and the coverage lens** (5.2).
10. **`ensure(...)` and the trust lens** (5.3) up to Contracted; review queue (5.6).
11. **Formatter** (section 8).
12. Later: property testing, static proof, call-edge drawing, instructing the AI from a box.

Slices 1 to 4 give a usable first result: a box view of a project with "touches the network" badges.

## 10. Open decisions

- **Doc comment spelling:** `///` or `[doc("...")]`.
- **Project structure:** whether namespaces are enough or a project or module manifest sits above them. PrimeScroll
  would need namespaces before its own box view is useful.
- **Whether a contract change is a breaking change** for callers.
- **Whether the box view stays read-only** or later becomes the place to direct the AI ("change this component").
