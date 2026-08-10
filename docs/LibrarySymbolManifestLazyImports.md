# Library Symbol Manifests and Lazy Import Expansion

Status: plan, not started. This is the architecture-level fix for the
per-import validation cost documented in `docs/TestRuntimeOptimization.md`
and TODO-4743 (`docs/todo.md`) - both landed real, bounded wins against the
*symptom* (diffuse per-call resolution cost) but concluded the remaining
cost is proportional to how many definitions get parsed and validated per
import, not a fixable per-call inefficiency. This doc addresses the actual
cause.

## The Problem, Verified

- `appendStdlibModuleSources` (`src/CompilePipeline.cpp`) resolves a
  wildcard/module-root import (e.g. `import /std/image/*`,
  `import /std/gfx/experimental/*`) to a `.prime` file via the existing
  `std/modules.psmeta` manifest, then splices that **entire file's source
  text** into the compilation unit before a single `Lexer`/`Parser`/
  `Semantics::validate()` pass runs over the whole combined blob. This is
  confirmed via direct code reading, not inferred.
- Measured with `--benchmark-semantic-phase-counters`: `import
  /std/image/*` with a totally unused, empty `main()` visits **4,885
  calls** and produces **26,862 facts** during validation - work spent
  entirely on definitions the program never references at all. Wall-clock
  cost for this case is ~10-12s as of 2026-08-10 (down from an original
  ~34.85s baseline via TODO-4742/4743's per-call optimizations, but still
  far from proportional to what the program actually does - which is
  nothing).
- This is not a fresh discovery. TODO-4735 (closed,
  `docs/todo_finished.md`) already investigated caching/pruning options
  for exactly this cost and concluded: "there is no AST-level module
  boundary anywhere in the pipeline - 'the stdlib portion of the AST' is
  not a thing that exists independently of a specific test's expanded
  source, so there is nothing at the AST layer to cache and later
  re-attach." The existing `std/modules.psmeta` manifest only maps a
  module root (e.g. `/std/image`) to **one whole source file** - it has
  no concept of the symbols inside that file, so it cannot support
  partial inclusion.
- TODO-4743 (`docs/todo.md`, still open) profiled the same symptom's
  per-call cost with real `callgrind` instrumentation on 2026-08-10 and
  found it diffuse across ~40 small functions in the compat-path/soa-path
  string-building and lookup machinery, landed a real fix (a
  validator-lifetime-persistent cache for `hasDefinitionFamilyPath`,
  ~31-38% further improvement), and confirmed the residual cost scales
  with *how many definitions get validated*, not with any single
  remaining inefficiency - reducing the call volume itself is the only
  remaining lever.

## Goal

Importing a library should cost proportionally to what a program actually
uses from it, not to the library's total size. Concretely: `import
/std/image/*` in a program that calls 2 of that module's ~40 functions
should only parse and validate the transitive closure reachable from those
2 calls - not all ~40. Target: sub-1-second cost for a stdlib-heavy,
usage-light program (down from today's ~3-12s for gfx/image-style
imports), with proportionally larger wins for programs that use even less
of a large library.

## Design

### Component 1: Per-module symbol manifest

Each stdlib module gets a manifest declaring every top-level symbol
(functions, structs, and their associated methods) it exports and where
to find it - which source file, and enough position information (e.g. a
byte offset and length, or line range) to slice out that exact symbol's
source text without re-parsing the whole file.

**Generated, not hand-authored.** `primec` already parses `.prime` files;
a small pre-pass that walks a module's top-level definitions and emits
the symbol table is cheap to build and cannot drift out of sync with the
source the way a hand-maintained JSON file would. Treat manifest
generation as a build-time (or first-compile, cached) step, not something
library authors write by hand.

This extends (does not replace) the existing `std/modules.psmeta`
file-level manifest - either as new fields alongside the existing
`root`/`source_file` entries, or a new sibling manifest file per module,
whichever proves cleaner once Step 0 characterizes the actual `.psmeta`
format's extensibility.

### Component 2: Lazy, iterative import expansion

Replace today's one-shot "splice the whole file, then parse and validate
once" with an iterative resolve loop - the same algorithm a static linker
uses to resolve undefined symbols from an archive:

1. Parse the user's own source first (no stdlib text spliced in yet).
2. Collect unresolved call/type names whose resolution would come from an
   active wildcard/module-root import.
3. For each unresolved name, consult that import's module manifest, slice
   in **that one symbol's** source text (and only that one).
4. Re-scan the newly-expanded source for unresolved names introduced by
   what was just added (a symbol just pulled in may itself call other
   symbols in the same or a different imported module).
5. Repeat steps 3-4 until a fixed point (no new unresolved names that
   resolve to a manifested symbol).
6. Validate/lower the resulting - now minimal - expanded source through
   the existing pipeline unchanged.

The validation/lowering stages themselves do not need to change; only the
text-assembly step feeding them does.

## Non-Goals (first phase)

- Imports that already name one specific file rather than a module root
  or wildcard (e.g. `import /std/collections/vector`) are not the primary
  target - they already reasonably scope to one file's worth of text. The
  wildcard (`import /std/gfx/experimental/*`) and module-root forms are
  the ones unconditionally pulling in everything today, and are what this
  plan targets first.
- Not extending manifest-based lazy expansion to user-authored (non-`/std`)
  libraries in the first phase. Scope to `/std/*`, matching
  `modules.psmeta`'s own current scope; revisit once this is proven out.
- Not changing validation behavior or diagnostics for symbols that ARE
  referenced - this is purely about not parsing/validating symbols that
  are never referenced from the compiled program, not a change to what a
  referenced symbol's own validation looks like.

## Risks

- **Dependency discovery requires parsing, not just a lookup table.** A
  manifest entry says "symbol X lives here"; it does not say "symbol X
  also needs symbols Y and Z." The fixed-point loop is not optional -
  there is no shortcut that avoids parsing a newly-included symbol's body
  to discover what it itself calls.
- **Struct/method association.** Need to confirm, by reading how
  struct-associated helper functions are actually represented in
  `program.definitions` today (full paths, receiver-typed first
  parameters, etc. - this needs direct investigation, not assumption),
  that a struct and its methods are captured coherently as either one
  manifest entry or cleanly-linked separate entries. Getting this wrong
  risks pulling in a struct without its constructor, or vice versa.
- **Diagnostic quality on genuine unknowns.** If a name doesn't resolve to
  any manifested symbol, the user needs a clear "unknown symbol in
  imported library X" diagnostic, not a confusing downstream parse error
  from what looks like an incomplete program.
- **Cycles.** Mutually-referencing symbols (A calls B, B calls A) need an
  explicit "currently expanding" set to avoid infinite recursion in the
  fixed-point loop - standard for any linker-style symbol resolver, but
  must be built in from the start, not discovered via a hang.
- **Corpus assumptions about whole-module inclusion.** Some existing
  tests may incidentally depend on the *entire* module being present
  (e.g. a test checking that two unrelated stdlib definitions don't
  collide). The differential-testing discipline that made the
  compat-path-resolution consolidation succeed
  (`docs/CompatPathResolutionConsolidation.md`) - an opt-in env-gated
  comparison between old (whole-file) and new (lazy) resolved output
  across the full corpus, with every observed divergence individually
  triaged - is the required verification method here too, not a
  from-scratch full-suite pass/fail diff.
- **This plan rhymes with, but is not the same as, TODO-4735's rejected
  options.** TODO-4735 declined both "AST-level module linking" and
  "reachability-pruned lazy validation" as oversized for a bounded leaf.
  This plan is closest to the first option, scoped specifically to
  library imports (not general program-wide reachability pruning across
  the user's own code), with the manifest as the missing piece that makes
  it tractable. If Step 0 finds this still needs to become general
  program-wide reachability pruning to work, that is a signal to stop and
  re-scope rather than silently expanding this plan's boundaries.

## Plan / Phases

### Phase 0 - Characterize (no production-code changes)

- Catalog every distinct wildcard/module-root import path used across
  `tests/` and `stdlib/`, and for each, measure the actual
  referenced-symbol-count vs. total-symbol-count ratio (via
  `--benchmark-semantic-phase-counters` or a dedicated instrumentation
  pass). This sizes the real-world benefit per module and flags any
  module where the ratio is already near 1:1 (a diminishing-returns case
  not worth manifesting first).
- Read how struct-associated helper functions are represented in
  `program.definitions` (full-path naming, receiver-typed parameters) to
  resolve the struct/method association risk above before designing the
  manifest's entry granularity.
- Confirm the `std/modules.psmeta` format's extensibility (can it grow
  symbol-level fields, or does it need a sibling file) by reading its
  full parser, not just the entry point already found in
  `CompilePipeline.cpp`.
- Deliverable: a findings section appended to this document, plus a
  decided manifest file format and entry granularity.

### Phase 1 - Manifest generator

- Build the generator (walks a module's top-level definitions via the
  existing parser, emits the symbol table with source-slice locations).
- Differential-check the generator itself: for every symbol the generator
  claims exists at location X, confirm re-parsing that exact slice
  produces the same definition the whole-file parse does.
- Generate manifests for the modules Phase 0 flagged as highest-value
  first (the largest modules with the lowest used-symbol ratio -
  `/std/image`, `/std/gfx/experimental` are the two already measured as
  expensive this session).

### Phase 2 - Lazy expansion, opt-in

- Implement the iterative fixed-point expansion algorithm as an opt-in
  path (env var or CLI flag), with today's whole-file-splice behavior
  kept as the default.
- Build the permanent differential harness: compare old (whole-file) vs.
  new (lazy) resolved call paths and validation outcomes across the full
  semantics/ir_pipeline/compile_run corpora, following the exact pattern
  `docs/CompatPathResolutionConsolidation.md`'s Step 1 already proved out
  for a structurally similar problem.
- Triage every divergence the harness reports individually - each is
  either a genuine bug this plan should fix, or an intentional behavior
  change that needs its own pinning test, per the same discipline.

### Phase 3 - Flip the default

- Once the differential harness reports zero unintended divergence across
  all three corpora, make lazy expansion the default path.
- Keep the whole-file path available behind a flag for at least one full
  session/release as an escape hatch, then remove it once confidence is
  established.
- Re-measure the gfx/image cases from this doc's "Problem, Verified"
  section and confirm the sub-1-second target.

### Phase 4 - Extend beyond `/std/*` (only if Phase 3 proves out)

- Revisit the "stdlib only" non-goal once the mechanism is proven; assess
  whether user-authored library imports would benefit the same way.

## Findings

Completed as TODO-5223 (Phase 0 characterization, no production-code
changes). All measurements below via
`--benchmark-semantic-phase-counters`, `--dump-stage ast`, and direct
source reading against the `build-release/primec` binary as of
2026-08-10.

### Per-module usage ratios

Measured `validation.calls_visited` / `semantic_product_build.facts_produced`
for a wildcard import of each module with a totally unused `main()`
(worst case: zero usage, full cost), plus each module's approximate
source size:

| module | lines | calls_visited (unused) | facts_produced (unused) |
|---|---|---|---|
| `/std/image/*` | 2,736 | 4,885 | 26,862 |
| `/std/gfx/*` | ~660 | 3,125 | 13,034 |
| `/std/gfx/experimental/*` | ~650 | 3,113 | 12,973 |
| `/std/collections/*` | 5,913 | 78 | 593 |
| `/std/math/*` | 942 | 0 | 5 |

`/std/math/*` and (to a lesser extent) `/std/collections/*` are **not**
next-priority candidates despite `collections` being the largest module
by line count - see "Existing prior art" below for why math is already
near-zero, and collections' own wildcard resolution already has a
narrower special case (`skipExperimentalCollectionsInBaseWildcard`,
`CompilePipeline.cpp`) that keeps its unused-import cost low relative to
its size. **`/std/image/*` and `/std/gfx/*`/`/std/gfx/experimental/*`
remain the highest-value targets** - both because they're the most
expensive per this table and because a real usage sample (see below)
shows the used/total ratio for a typical test is extremely lopsided (a
handful of functions/types out of hundreds of transitively-included
definitions).

`--dump-stage ast` on an unused `import /std/image/*` shows **784 total
definitions** get parsed into the AST (image.prime's own definitions plus
whatever it transitively imports itself). A representative real test
(`test_compile_run_native_backend_core_image_io_png_16bit_interlaced.cpp`
and siblings) references on the order of a handful of `Image`/`ImageError`
symbols directly - the used-to-total ratio is in the low single-digit
percent for typical `compile_run` test usage, confirming the Goal
section's "usage-light, size-heavy" framing is realistic, not a
worst-case cherry-pick.

### Existing prior art: the math wildcard's hardcoded skip-or-include hack

`CompilePipeline.cpp`'s `shouldSkipMathWildcardStdlibModule` /
`sourceReferencesNonBuiltinMathSymbols` already implement a coarse version
of this plan's idea for exactly one module (`/std/math/*`): a lexer-level
scan of the compilation unit's own source (before stdlib splicing) checks
whether any token matches a **hardcoded list of math stdlib surface names**
(`MathStdlibSurfaceNames`, `isMathBuiltinOrConstantName`); if none match,
the entire math module is skipped rather than spliced in at all. This
directly validates that conditional module inclusion based on actual
usage is already a load-bearing production technique in this codebase,
not a speculative idea - but it also demonstrates the exact anti-pattern
this plan's manifest design avoids: the surface-name list is **hand-written
and module-specific**, so it silently goes stale the moment `math.prime`
gains a new public symbol nobody remembers to add to the list, and the
technique only works as a binary all-or-nothing switch (skip everything or
include everything), not partial/symbol-level inclusion. This plan
generalizes the validated idea (skip work for what isn't used) while fixing
both weaknesses (auto-generated from source, partial/lazy rather than
binary).

### Struct/method representation - resolved, simpler than the plan worried

Read `--dump-stage ast` output directly for `/std/image/*`: a struct like
`ImageError` and each of its associated "methods" (`why`, `status`,
`read_unsupported`, `result<T>`, etc.) are **already independent top-level
entries in `program.definitions`**, distinguished only by a shared
`fullPath` prefix (`/std/image/ImageError`, `/std/image/ImageError/why`,
`/std/image/ImageError/status`, ...) - there is no special AST-level
"struct with attached methods" grouping to account for. This resolves the
risk flagged in the Design section as simpler than feared: **the manifest
needs no special struct/method entry type** - every definition (bare
function, struct declaration, or struct-associated method) is manifested
uniformly, one entry per `fullPath`, exactly like any other symbol. The
lazy-expansion algorithm pulls in `ImageError`'s struct declaration only
when something actually needs its field layout (used as a type anywhere -
not just when explicitly constructed) and pulls in each method
independently, only when that specific method is actually called.

**One correction to the Design section's wording**: the fixed-point loop
must scan for unresolved **type names**, not just unresolved **call
names**. A program can reference `[ImageError]` as a parameter or local
binding type without ever calling any of its methods, and the struct's own
field-layout definition still needs to be pulled in for type-checking to
work at all. "Unresolved name" in the Design section's steps 2-4 should be
read as covering both call targets and type references from here on.

### Manifest format decision

Read the full `readStdlibModuleManifest` parser
(`CompilePipeline.cpp:397-465`): `std/modules.psmeta` is a small, strict,
hand-rolled `[module]`-block INI-style text format (`key = value` pairs,
`#` comments, explicit "unknown key" rejection for anything not
recognized). Critically, **most stdlib modules don't have any entry in it
at all** - `modules.psmeta` today only contains 2 entries total (`/std/gfx`
and `/std/gfx/experimental`, both narrow file-location overrides for
module roots whose import path doesn't map cleanly onto a directory scan).
Every other module (including `/std/image`, `/std/collections`,
`/std/math`) resolves via the **default directory-scan path**: the import
root maps to a directory, and `appendStdlibModuleSources` recursively
walks it, splicing in every `.prime` file found (with one narrow
special-cased exclusion for `/std/collections`'s experimental-prefixed
files).

**Decision: do not grow the central `std/modules.psmeta` file.** Centralizing
per-symbol tables for every stdlib module into one shared file would (a)
make an already-small, easy-to-read file balloon to thousands of lines
covering unrelated modules, and (b) require every module's manifest
regeneration to touch and re-diff the same shared file, which is
unnecessary merge-conflict and review-noise risk for a purely mechanical,
auto-generated artifact. Instead: **a new, per-module sibling manifest
file, colocated with each module's own source** (e.g.
`stdlib/std/image/image.psmeta` next to `image.prime`, following the
existing `.psmeta` extension convention for consistency). Keep the same
simple `key = value` block-based text format `modules.psmeta` already
uses (no need to introduce a JSON dependency for a format this codebase
already has a working hand-rolled parser for) - one `[symbol]` block per
manifested definition, with `path`, `source_file`, and enough position
information (a byte offset + length pair is simplest and avoids
line/column recomputation edge cases around multi-line definitions) to
slice the exact source text for that one symbol.

### Scope confirmation for Phase 1

Per the measurements above, Phase 1 (TODO-5224) should generate manifests
for `/std/image` and `/std/gfx`/`/std/gfx/experimental` first, exactly as
originally planned - the data confirms these are the highest-value
targets and the struct/method representation question that could have
complicated the generator's design turns out to need no special handling.
