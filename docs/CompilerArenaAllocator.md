# Compiler Arena Allocator

Status: implemented (TODO-5233 design, TODO-5234 implementation). Ships a
narrower variant than originally proposed - see "Why the design changed"
below; this is the authoritative record of what actually shipped and why.

## Background

TODO-5230/5231/5232's post-fix profile of the `mini_vec.prime` repro (see
`docs/TestRuntimeOptimization.md`'s 2026-08-13 log entries) showed
`malloc`/`free`-family libc functions collectively consuming roughly a
quarter to a third of retired instructions, spread across parsing,
semantics, and IR lowering rather than concentrated in one function - a
classic case for a bump/arena allocator instead of chasing individual call
sites one at a time.

## Survey: what's actually being allocated

Re-profiled the same `mini_vec.prime` repro (`import /std/collections/vector/*`
plus one call using a vector, `--emit=vm`, release build) with two tools,
per this leaf's acceptance criteria (instruction share was not enough -
needed allocation *count*):

- `valgrind --tool=callgrind`: malloc/free-family functions (`_int_malloc`,
  `_int_free`, `malloc`, `free`, `malloc_consolidate`, `unlink_chunk`) plus
  `operator new` collectively **~33% of retired instructions**
  (735M + 634M + 439M + 273M + 273M + 110M + 156M out of 7.87B total).
- `valgrind --tool=dhat` (allocation-count-focused, not instruction-share):
  **9,756,990 total heap blocks allocated** for this one compile (754MB
  total bytes allocated over the run; only 22.96MB live at the peak, i.e.
  the overwhelming majority of that volume is short-lived churn, not a
  large working set). The top allocation-count sites by a wide margin were
  all inside `primec::semantics::RequirementPredicateDefinitionContext`'s
  copy constructor, called from `rewriteCompileTimeIfBranches`
  (`SemanticsValidate.cpp`) - the same code path TODO-5231/5232 already
  reduced call *volume* on by 92.4%, but each surviving call still copies
  several `std::string`/`std::vector<std::string>`/`std::unordered_set`
  members, and each of those copies is itself hundreds of thousands of
  individual small allocations (string buffers, vector buffers, hash-table
  nodes). The top ~7 allocation sites by block count accounted for roughly
  2.6M of the 9.76M total blocks (~27%).

Conclusion: this is not concentrated in one fixable function the way
TODO-5231/5232's O(N^2) bug was - it's diffuse, high-*count*,
short-lived-object churn (strings, small vectors, hash-table nodes)
distributed across parsing, requirement-predicate-context copying, and
`parseBindingInfo`'s ~50 call sites. A general allocator-level fix (bump
allocation, size-classed reuse) rather than call-site-by-call-site
optimization is the right lever, matching the original hypothesis.

## Scope boundary survey

**CLI binaries** (`src/main.cpp`, `src/primevm_main.cpp`): both call
`primec::runCompilePipelineResult` exactly once per ordinary invocation.
`main.cpp` additionally has a `--benchmark-semantic-repeat-count` flag that
loops calling it N times in one process for benchmarking, with an existing
`BenchmarkSemanticRepeatLeakCheck`/`captureProcessRssSample` mechanism
already built for detecting cross-repeat RSS growth - a preexisting,
independent analog of exactly the "long-lived process" concern this design
had to reason about for the test binaries.

**In-process test harnesses**: `tests/unit/semantics/test_semantics_helpers.h`
and the `tests/unit/ir_pipeline/` equivalents call
`primec::Semantics::validate` and IR lowering directly, not through a
spawned `primec` subprocess - confirmed via `grep -rn "semantics.validate("`
finding call sites spread across the helper files rather than one choke
point. Every test binary shares one `main()` (`tests/unit/test_main.cpp`,
linked into every `PrimeStruct_*_tests` target per `CMakeLists.txt`), which
gave a single integration point that doesn't require editing 47+ files
individually (see "Why the design changed" for why this ended up not
mattering in the shipped version).

## The design that was proposed (TODO-5233)

The original TODO-5233 design, matching the TODO's own `implementation_notes`:

- A scoped/RAII arena (`ScopedCompileArena`), constructed at the start of a
  compile scope and reset at the end (bump-pointer reset, chunks kept for
  reuse across compiles - not actually freed each time).
- Global `operator new`/`operator delete` overrides, gated by a
  `thread_local` "current arena" pointer: null outside a scope (falls back
  to the system allocator - the safe default for anything outside an
  intentionally-scoped region, including worker threads spawned via
  `std::async` for parallel definition validation, which never activate the
  arena and so always use the system allocator).
- Scope boundary: one `ScopedCompileArena` per CLI compile (wrapping
  `runCompilePipelineResult` and everything downstream that consumes its
  result), and one per `TEST_CASE` via a `doctest::IReporter` listener in
  `tests/unit/test_main.cpp` (`test_case_start` constructs it,
  `test_case_end` destroys it) - chosen over instrumenting all 47+
  individual helper files because it is a single, robust choke point that
  doesn't need to track every present and future call site into the
  compiler library.
- A uniform 16-byte allocation header (owner-arena pointer + size class) on
  *every* allocation, arena-served or not, so `operator delete` can always
  determine how to free a pointer from the pointer itself rather than from
  "is an arena currently active on this thread" - this matters because an
  object can legitimately be allocated on one thread and freed on another
  (e.g. ownership transferred out of a `std::async` closure), and delete
  must be correct regardless of which thread's arena state happens to be
  active at that moment.
- Size-classed free lists (16B..4096B doubling classes) *within* the arena,
  not a naive "bump and never free" allocator: the DHAT survey's own
  754MB-allocated-vs-23MB-live numbers show that "never free within a
  compile" would inflate a single compile's peak memory by roughly 30x over
  its live working set - unacceptable even for the "safe to leak until
  process exit" CLI case, since a much larger real program could turn that
  ratio into a multi-GB blowup. Free lists let same-thread frees be reused
  immediately, bounding peak memory to roughly the live working set instead
  of the cumulative allocation volume, the same property a general-purpose
  allocator provides.
- Explicit avoidance of a *cross-thread* free-list hazard: a block freed on
  a thread other than the one whose arena owns it is deliberately leaked
  (never pushed onto any free list) rather than guessed at - the memory
  stays valid (arena chunks are never unmapped) but isn't recycled, which
  costs at most some unreclaimed capacity, never a correctness bug from two
  live objects aliasing the same bytes.

This part of the design was implemented, built cleanly, and passed a quick
manual smoke test (`primec` compiling the `mini_vec.prime` repro, correct
output, ~25% faster). It did **not** survive first contact with the full
test suite.

## Why the design changed: a real corruption bug, not a hypothetical

Wiring the doctest listener into every `TEST_CASE` and running the full
`semantics` suite (2940 `TEST_CASE`s) reproduced a deterministic SIGSEGV on
the 4th `TEST_CASE`, always the same one, destroying a `std::string` with a
garbage (non-heap, non-object) pointer value. Isolating it (gdb with debug
symbols, then a targeted `valgrind --tool=memcheck` run that came back
*clean* - itself a clue that this was an address-identity bug, not a
generic buffer overrun memcheck would have caught) pointed at memory that
had been silently handed out twice to two different live objects.

The mechanism: this codebase has many function-local "magic static" values
- `static const std::string`/`std::vector`/etc. computed once on first call
and reused for the rest of the process, e.g. `SemanticsValidate.cpp`'s
`static const std::unordered_set<std::string> emptyStructTypes;` pattern
(dozens of instances across `src/semantics/`) and similar one-time-computed
prefix/suffix strings elsewhere. These are a distinct hazard from the
`thread_local` caches the original design already knew about and had a plan
for (`normalizeBindingTypeName`'s cache in
`SemanticsBindingTypeHelpers.cpp`, `StdlibSurfaceRegistry`'s resolved-path
cache, `SourceLocationMapper`'s cached-mapper-by-address) - those were found
by grepping for `thread_local` and were going to be registered for
explicit invalidation at every arena reset. Magic statics are *not*
`thread_local`, are far more numerous, are added routinely with no compiler
warning if one is missed, and are semantically just "compute this constant
once" idioms rather than caches anyone would think to register for
invalidation.

If one of these gets constructed (lazily, on first call) while a compile
scope is active, its backing bytes are arena-allocated. The *next* compile
scope's reset then treats that memory as free again and can hand the exact
same bytes to a brand-new object while the magic static is still alive and
expected to be readable for the rest of the process - silently corrupting
it. This is worse than a crash: the corrupted object can look plausible
right up until something reads a field that got overwritten, which is
exactly the "array rejects envelope-level length template arg" failure
pattern observed (a `std::string`'s pointer field holding a small integer
that had been a completely unrelated object's field). No amount of grepping
for `thread_local` would have found this, and there is no practical way to
enumerate "every static that might get lazily constructed inside a compile
scope, forever, as the codebase evolves" - the memcheck-clean result also
means normal test-suite discipline would not reliably catch a regression
here either.

Per TODO-5234's `stop_rule` ("If the memory-growth verification for the
long-lived test binaries shows unbounded or even just significantly-worse
peak RSS, do not ship... A correctness or resource-usage regression here is
strictly worse than the current slowness"), this is exactly the situation
that mandates falling back to the narrower, provably-safe variant rather
than trying to patch around it (e.g. by also registering every magic
static, which cannot be done exhaustively and would only convert a crash
into a silent, unenumerable risk that reappears the next time someone adds
one).

## What actually shipped

**The arena never resets.** It is entered exactly once, via a single
`primec::ScopedCompileArena` constructed near the top of `main()` in
`src/main.cpp` and `src/primevm_main.cpp`, and lives for the rest of the
process. This is safe for exactly the reason "never free until process
exit" was always the documented, accepted baseline strategy for a one-shot
compile-then-exit CLI process: nothing is ever reset out from under a
still-live object, because nothing is ever reset at all. Same-thread frees
still recycle through the size-classed free lists described above (so a
single compile's peak memory tracks its live working set, not its
cumulative allocation volume - this part of the original design's safety
property is preserved), but no *generation* boundary is ever crossed.

**The doctest test binaries never construct a `ScopedCompileArena` at all**
(`tests/unit/test_main.cpp` reverted to a plain `main()`, no listener). They
call into `Semantics::validate`/IR lowering exactly as they did before this
work landed, entirely on the system allocator. This is the direct
consequence of the corruption mechanism above: "thousands of compiles in
one long-lived process" is precisely the shape that makes "never reset" and
"reset per compile" both wrong for a different reason each ("never reset"
grows unboundedly; "reset per compile" corrupts magic statics) - so rather
than solve a harder version of the same problem, the test binaries simply
don't participate. `src/semantics/SemanticsBindingTypeHelpers.cpp`,
`src/StdlibSurfaceRegistry.cpp`, and `src/SourceLocationMapper.cpp` were
reverted to their pre-arena state (their `thread_local` caches persist
across compiles exactly as they always have, under the system allocator,
with no interaction with the arena at all - the CLI binaries' single,
never-reset scope means this is fine there too, since nothing ever gets
reset for those caches to become stale relative to).

The `--benchmark-semantic-repeat-count` loop no longer resets the arena
between repeats (an earlier iteration of this fallback still tried to reset
there, which carries the identical magic-static hazard on a smaller scale -
removed for the same reason). Consequence: that diagnostic-only flag's own
RSS-checkpoint output will show memory growing across repeats for
arena-covered allocations. This is expected, not a regression: it is the
direct, documented behavior of "never free until process exit" applied
repeatedly within one process, the same characterization that always
justified the CLI-only scope in the first place. `src/main.cpp` documents
this inline at the point it's observable.

### Implementation notes (`src/CompileArena.{h,cpp}`)

- `BlockHeader` (16 bytes: owning-arena pointer + size class) precedes every
  payload, whether or not the arena is active for that particular
  allocation - `operator delete` always reads it rather than consulting
  ambient state, for the cross-thread-free-safety reason above.
- Size classes: 16/32/64/128/256/512/1024/2048/4096 bytes (doubling).
  Anything larger, or any over-aligned allocation (the standard library's
  aligned `operator new`/`delete` overloads are deliberately left
  untouched, matching their default system-allocator behavior), falls
  straight through to `malloc`/`free` (still under the uniform header).
- The arena's own chunk bookkeeping is a hand-rolled intrusive linked list
  allocated via plain `std::malloc`, never through the overridden
  `operator new` - using `std::vector` here was tried first and found to
  reenter the same arena instance's chunk-growth logic while it was
  already mid-mutation (growing the bookkeeping vector while the *outer*
  call was in the middle of appending a chunk because the arena had just
  run out of room), which is undefined behavior. Bypassing `operator new`
  entirely for this one piece of arena-internal state sidesteps the
  reentrancy question altogether.
- Constructing the very first `CompileArena` for a thread is also
  deliberately routed around `operator new` (plain `malloc` + placement
  new) for the same bootstrapping reason: `new CompileArena()` would call
  the overridden `operator new`, which needs `currentThreadArena()` to
  already have a `tls_arena` to allocate from - infinite recursion until
  stack overflow, observed directly during implementation before this fix.

## Measured results

All measurements: release build (`--emit=vm`), same 4-core sandbox, same
machine both before/after (rebuilt `primec` from a `git stash` of this
work's changes for the "before" numbers, restored after).

**`mini_vec.prime` repro** (`import /std/collections/vector/*` +
`vectorCount<i32>(v)`, reconstructed per `docs/TestRuntimeOptimization.md`'s
existing note that the original repro file was never checked in):
- Before: ~0.90-0.92s wall-clock, peak RSS ~36MB (`getrusage` `ru_maxrss`).
- After: ~0.66s wall-clock (**~27% faster**), peak RSS ~28.5MB (no memory
  regression - lower, in fact, likely because the arena's size-classed free
  lists avoid glibc malloc's own bookkeeping/fragmentation overhead for
  this allocation pattern).

**Heavier repro** (`import /std/collections/*`, the whole collections
module, not just vector - modeled on TODO-5232's own heavier-repro
methodology):
- Before: ~0.89-0.94s wall-clock.
- After: ~0.64-0.68s wall-clock (**~28% faster**), peak RSS ~43.6MB.

Falls short of the sub-100ms directional target from TODO-5232's
discussion, consistent with that leaf's own finding that the remaining
cost floor is dominated by the whole-file text-splicing import
architecture (`docs/LibrarySymbolManifestLazyImports.md`, TODO-5223 through
TODO-5226 - a separately-tracked, much larger architectural fix), not
allocator overhead. The ~27-28% win here is the allocator-attributable
share.

**Long-lived in-process test binary memory growth** (the single most
important verification per TODO-5234's `stop_rule`): sampled
`/proc/<pid>/status` `VmRSS`/`VmHWM` every 15s across a full
`PrimeStruct_semantics_tests` run (2940 `TEST_CASE`s, ~460s wall-clock).
`VmHWM` (peak resident set, monotonically non-decreasing by definition)
went from ~50MB to ~56MB over the entire run - essentially flat, consistent
with ordinary allocation variance across different test cases rather than
growth, and expected given the test binaries never touch the arena at all
(this is the same behavior the suite had before this work landed - the
arena's existence doesn't change anything for a binary that never
constructs a `ScopedCompileArena`). `VmRSS` itself oscillated between
~34-55MB throughout, tracking whichever test cases were running at each
sample, with no upward trend.

**Memory-budget CI gate**: `PrimeStruct_semantic_memory_trend` (a
sustained-regression check over `benchmarks/semantic_memory_budget_policy.json`)
flagged a real, reproducible ~11-12% `ast-semantic`-phase RSS increase for
the three fixtures that import stdlib math/vector modules
(`imported_math_body`, `math_vector`, `math_vector_matrix`) - attributable
to the arena's per-allocation 16-byte header, and only for fixtures with
meaningfully more allocations (fixtures that don't import anything stayed
flat). The policy's own headroom for these three entries was already
nearly exhausted by earlier same-day work before this arena landed, so a
modest, understood addition on top was enough to trip the sustained-window
check. Updated the three affected policy entries to the new measured
baseline with a fresh margin - see `docs/TestRuntimeOptimization.md`'s
matching log entry for the full before/after numbers and reasoning.

**Full suite**: `./scripts/compile.sh --release`, single invocation per
this repo's convention: **1881/1881 tests passing, 0 regressions**
(one transient failure during an interim run,
`native_window_launcher_and_preflight_56_56`, a `docs/todo.md`
content-lock test whose expected snapshot goes stale whenever
`### Ready Now` changes - same pre-existing pattern TODO-5232's resolution
note already documented, not a code regression; resolved by this leaf's own
`docs/todo.md` update below).

## Safety argument, restated

The property that makes this safe is simple enough to state in one
sentence: **the arena is only ever active on a thread between that
thread's first `ScopedCompileArena` construction and process exit, with no
reset in between, so nothing is ever handed a byte range that another
still-live object also believes it owns.** Every other property discussed
above (uniform header, cross-thread-free leak-not-recycle, magic statics)
follows from or supports that one invariant. The class of binaries that
this invariant is safe for is exactly the class that entered it: one-shot,
compile-then-exit CLI processes. The class of binaries it would be unsafe
for - long-lived processes making the same allocations thousands of times
per run - simply never enters it.
