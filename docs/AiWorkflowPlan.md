# AI Workflow: Plan

Classification: **design direction**. Nothing here is implemented yet. The first filed leaves are TODO-5841,
TODO-5843 and TODO-5842 (namespaces as envelopes, and summaries); the other slices in section 9 become `docs/todo.md`
entries as the open decisions in section 10 are made.

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
| Summary text, stale flag | `[summary]` (section 6.2), metadata (section 4.3) |
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
- **Header:** entering a box shows a report at the top of the window: its summary, the effects used anywhere inside
  it (with the definitions responsible), and its purity and trust levels.
- **Rendering:** not HTML. The view is described in a small display language (section 4.2) and drawn natively.
- **Startup:** the view and the compiler work behind it load only when the view is first opened, so the editor's
  launch time does not change.

### 4.1 Opening a project

Decided:

- **File > Open PrimeStruct Project…** opens a folder, and the folder is the project.
- **The first version watches the folder** for changes made outside the editor (an AI editing the files on disk).
  A change rebuilds the outline and reloads open tabs that have no unsaved edits; a tab with unsaved edits is marked
  as changed on disk and is not overwritten. The watcher starts when a project is opened, never at launch.

Proposed, not yet decided:

- **Contents:** every `.prime` file under the folder; the folder is an import root (as `--import-path`).
- **Entry point:** `main.prime` at the root when present; otherwise the project is a library and the outline covers
  its public definitions. A project file is added only when a real project needs more.
- **Window:** a project window shows the box view first; files open as tabs from it. Recent projects join
  File > Open Recent and a project window is restored like tabs are.
- **Refresh on save:** saving a file in the editor rebuilds the outline too.

Needed first: a choose-folder call in the UI ABI (`openPanel` picks files only), a directory listing in the stdlib
(none found in `/std/os` or `/std/file`), a folder-watch call and event in the UI ABI, and the display view of
section 4.2.

### 4.2 The display language

Decided: the box view is not drawn in a web view. PrimeStruct code turns the outline and the active lens into a
simple JSON display description, and a renderer draws it with native components or in a Metal view.

Proposed shape:

- **Vocabulary:** box, text, badge, color token, an id for clicks, children. No scripts, no styles beyond tokens.
- **Layout in PrimeStruct, once:** a shared step turns the hierarchy into rectangles, so a renderer only draws
  rectangles and text and reports clicks by id. A second renderer then costs little.
- **Tests:** the description is deterministic JSON, so the view is snapshot-tested without a window.

The renderer is open (section 10). Today the native UI has no label, box or canvas widget, and `/std/scene` has no
text. Three routes:

| Route | Text | Cost |
| --- | --- | --- |
| One native view per box and label | the platform's | many views; zooming between levels is awkward |
| One native canvas view that draws the description with the platform's 2D and text drawing | the platform's | one new widget in the UI ABI |
| A Metal view over `/std/scene` | has to be built | a scene widget plus text rendering |

### 4.3 Project metadata

`ProjectRoot/.ps/metadata.json` holds facts about a project that are not in its source. It is committed with the
project, written with sorted keys so it merges and diffs cleanly, and carries a format version.

Per definition path:

- **Reviewed:** the canonical-form hash of the definition when a person marked it reviewed, and the date. A real
  change to the definition clears the mark; a respelling does not.
- **Summary stamp:** the canonical-form hash of the definition when its summary text last changed (section 6.2),
  recorded automatically when the outline is rebuilt.

The hash covers a definition's own signature and body, not its callees or its template instances, and names the
hash scheme, so a compiler upgrade that changes lowering does not silently clear every review.

## 5. Lenses

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
| Reviewed | A person marked it reviewed and it has not changed since (section 4.3) |
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

A combined filter rather than a color: boxes that are impure, not reviewed, and have trust level None or Tested.
This is the list of code a person should read themselves. Reviewed is tracked next to the machine evidence, not
instead of it: a box can be both tested and unreviewed.

### 5.7 Structure

From the outline alone:

- **Dead code:** not reachable from the entry point or from any public definition.
- **Duplicates:** definitions whose bodies have the same canonical-form hash.
- **Size:** box area follows code size.

### 5.8 Run time

Filled in by running the program from the editor; held in memory only and gone when the app closes.

- **Profile:** instructions and allocations per box.
- **Replay:** boxes light up as a recorded run moves through them.
- **Faults:** a fault is shown as a path through boxes.

## 6. Language changes

### 6.1 Commas separate arguments

Today a comma between digits is a digit separator, so `add(1,2)` is `add(12)` and the error is an argument count
mismatch. Decision: a comma always separates. No `.prime` file under `stdlib/` or `examples/` uses a digit comma.

The digit separator becomes `_` (`1_000_000`, `0xFF_FFu64`), between digits only, in the integer part of integer
and float literals. A dot was rejected because it collides with float literals: `1.000` already means 1.0.

### 6.2 Summaries

A `[summary("...")]` transform on a definition carries the text shown on its box. It is an ordinary transform, so
the lexer still drops comments.

```
[summary("A tab's text, file path and saved state.") struct]
Document() { ... }
```

- A summary is a claim, not a checked fact, and the view draws the two differently. Usage examples go in the
  summary; they are not run.
- A summary is **stale** when the definition's canonical-form hash differs from its summary stamp (section 4.3).
  The outline lists stale and missing summaries, so a periodic AI pass rewrites only those.
- The toolchain ships a default `summary.md`: instructions for an AI on how to write a summary. A project can
  replace it.
- A namespace carries a summary the same way, because a namespace is a definition (section 6.5):
  `[namespace summary("Text buffers and cursors.")] model { ... }`. TODO-5842 makes `summary` a checked transform.

### 6.3 `[pure]`, `[test]`, `ensure(...)`

As described in sections 5.1, 5.2 and 5.3.

### 6.4 Explicit boundaries

A project setting: public signatures and struct fields carry explicit types; bodies may infer. Box faces stay
precise and inference cannot silently change a public type.

### 6.5 Namespaces are envelopes

Every construct is an envelope, `[transforms] name<templates>(parameters) { body }`. `namespace name { ... }` is
not: it is a keyword with its own grammar production (`namespace_decl`) and parser function, and it leaves nothing
in the AST, so it cannot carry a transform list.

The envelope form already works (checked on the VM, 2026-11-10):

```
[return<void>]
demo() {
  [struct] Point() { [public i32] x{3} }
  [return<i32>] one() { return(7) }
  [return<i32>] two() { return(one()) }
}
```

`/demo/one`, `/demo/two` and `/demo/Point` exist, siblings call each other by bare name, `import /demo/*` works,
containers nest, and a slash-path name (`/a/b() { ... }`) works. After parsing, the members are definitions with
slash paths and the container is an empty definition at `/demo`; that is already the canonical form a namespace
needs. Three things are missing:

| Gap | Today |
| --- | --- |
| The container is a real function | `demo()` can be called and lowers to an empty function |
| It cannot be opened twice | "duplicate definition: /demo"; the stdlib opens `namespace std` in 50 files |
| Members of an untagged container can be read as struct helpers | `demo() { one() { return(7) } }` gives `/demo/one` an implicit `this` |

Proposal: **`namespace` is a transform.**

```
[namespace summary("Text buffers and cursors.")]
model {
  [struct] Document() { ... }
  [return<i32>] line_count([Document] doc) { ... }
}
```

- A `[namespace]` definition's body holds only definitions. It has no parameters, template list or `return`, is
  not callable and not a type, gives its members no implicit `this`, and lowers to nothing.
- Definitions with `namespace` at one path are one namespace; their transform lists are joined.
- It carries any transform, `summary` first. What member-affecting transforms mean on it is open (section 10).
- A path has one envelope that owns its transforms. A `[namespace]` block may share a path with a struct, enum or
  function, as `namespace P { ... }` next to `[struct] P` does today, but then it is `namespace` alone and the
  transforms go on the other definition.
- `namespace model { ... }` stays as surface sugar for `[namespace] model { ... }`, like `if(...) { ... }` for the
  canonical `if` call. A transform list goes on the envelope spelling; the keyword form takes none.
- The AST gets no new node and the canonical dump keeps its shape: `[namespace ...] /model() { }` plus the members.

TODO-5841 adds the transform, TODO-5843 turns the keyword form into sugar and removes `namespace_decl` from the
grammar. After both, the top-level items are `import`, definitions and executions. `import` is the remaining form
that is not an envelope; it is resolved on the source text before parsing and is out of scope here.

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
2. **Namespaces as envelopes** (6.5) and **`[summary]`** (6.2): TODO-5841, TODO-5843, TODO-5842.
3. **Outline v1** (section 3): paths, kinds, visibility, signatures, summaries, spans, declared and transitive
   effects, callees.
4. **Open PrimeStruct Project** (4.1) and **box view v1** (4, 4.2): hierarchy, box faces with effect badges, the
   header report, click to source.
5. **Canonical hashes** in the outline, the **structure lens** (5.7) and **project metadata** with the Reviewed
   mark (4.3).
6. **Purity lens** (5.1), then `[pure]`.
7. **Change lens** (5.5).
8. **Lowered view** (section 7), definition-filtered dump first.
9. **Raise lint** and **level lens** (section 8, 5.4).
10. **`[test]` and the coverage lens** (5.2).
11. **`ensure(...)` and the trust lens** (5.3) up to Contracted; review queue (5.6).
12. **Formatter** (section 8).
13. Later: run-time lenses (5.8), property testing, static proof, call-edge drawing, instructing the AI from a box.

Slices 1 to 4 give a usable first result: a box view of a project with "touches the network" badges.

## 10. Open decisions

- **Renderer for the display language** (4.2): native views, one native canvas view, or a Metal view.
- **Whether the keyword form `namespace name { ... }` stays** as sugar for good or is retired later (about 150
  blocks in the stdlib and examples, more in tests).
- **What other transforms mean on a namespace.** TODO-5841 carries them without applying them to members.
  `[effects(...)] namespace` as an upper bound for its members would be the namespace rule below.
- **Namespace rules:** the compiler checks each call against its caller's effects, but nothing stops an edit from
  adding an effect to a definition and to its callers. Whether a project can state "nothing under this namespace
  may use `net_io`" is open; the header report (section 4) makes such a change visible either way.
- **Project structure:** whether namespaces are enough or a project or module manifest sits above them. PrimeScroll
  would need namespaces before its own box view is useful.
- **Whether a contract change is a breaking change** for callers.
- **Whether the box view stays read-only** or later becomes the place to direct the AI ("change this component").
