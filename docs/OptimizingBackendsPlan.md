# Optimizing Native Backend and VM: Plan

Status: in progress (2026-10-03). Implemented so far: the `-O` and `--opt-*` flags (default `-O2` for runs and
executables), the pass manager and manifest, the `ir-lowered`/`ir-optimized` dumps, shared CFG utilities and opcode
semantics, local escape analysis, five passes (see `docs/spec/source-pipeline.md` for the user-facing description),
the `optexe`/`optcpp` emit kinds (section 9.1), the flat VM kernel with fused instructions (Phase 4), and register
locals plus deferred operands in the x86_64 native emitter (Phase 3, first steps). Not started: the register form,
a full register-allocating native code generator, and arm64 versions of the native steps.

This document records what the direct native backend (`--emit=native`) and the PrimeScript
VM (`--emit=vm`, `primevm`) do today, why they are slow, and a phased plan to
turn the shared IR path into an optimizing compiler with GCC-style `-O` levels
and individually selectable passes. The C++-emitting path (`--emit=exe`,
`--emit=cpp`) is out of scope except as the correctness and speed baseline.

## 1. Where the code is today

### 1.1 The IR the two backends share

- PSIR (`include/primec/ir/Ir.h`, schema v27) is a linear stack machine: one
  `IrFunction` per callable, a flat `std::vector<IrInstruction>` of
  `{opcode, imm, debugId}`, 16-byte local slots, raw `u64` operand stack
  values. Control flow is `Jump`/`JumpIfZero` to instruction indices.
- All integer opcodes operate on the full 64-bit slot in every backend. `PushI32`
  sign-extends; `AddI32` is a 64-bit add (VM: `VmKernelBoundary.cpp`; native:
  `NativeEmitterInternalsX64Arithmetic.h` header comment). This is the
  semantics any constant folder must reproduce bit for bit.
- Calls: the lowerer emits real `Call`/`CallVoid` for definitions with two or
  more call sites or any recursion (`IrLowererRecursionAnalysis.cpp`,
  `kMinCallSitesForRealCall = 2`); everything else is inlined at lowering.
  Arguments travel on the operand stack and the callee prologue pops them into
  locals 0..N-1 (`IrLowererLowerStatementsCallsStage.cpp:258-261`).
- `AddressOfLocal` yields a byte offset into the frame's slot space, and struct
  field access is lowered as `AddressOfLocal(base) + k*IrSlotBytes`
  (`NativeEmitterInternalsX64Core.h`, comment above `emitLoadLocalToReg`).
  Any local can therefore be reached from the address of another; this is the
  single hardest constraint for register promotion.
- `IrValidation.cpp` checks opcode ranges, jump targets, string indices and
  effect masks. It does **not** check stack balance; only the virtual-register
  lowering and the native emitter's `computeMaxStackDepth` do.
- `prepareIrModule` (`src/ir/IrPreparation.cpp`) is the only place all
  backends pass through: lower, validate, optional `--ir-inline`, re-validate,
  release AST bodies. That is where a target-independent optimizer belongs.

### 1.2 The direct native backend

`src/native_emitter/`: an `X64Emitter` (Linux/x86_64, ELF) and `Arm64Emitter`
(macOS/arm64, Mach-O) behind one template driver,
`emitNativeFunctions` (`NativeEmitterFunctionEmit.cpp`). It is a template
expander: each IR opcode becomes a fixed machine sequence that pops its
operands from a memory value stack (`r15`/`x28`) and pushes the result back.

The only optimization is a one-register top-of-stack cache (`r14`/`x26`,
`NativeEmitterOptions::enableRegisterCache`, always on, no CLI control). It is
flushed at every branch target, every `Jump`, every `Call`/`CallVoid`.

Consequences, visible in the emitted code for the benchmark loops:

- Every local read and write is a memory access (`[rbp - frameSize + idx*16]`).
  A loop body like `assign(sum, plus(sum, value))` is load, load, add, store
  through memory, with the value stack in between.
- Every constant is a 10-byte `mov r64, imm64`.
- Compare + branch is `cmp; setcc; movzx; push` followed by
  `pop; test; jz` instead of `cmp; jcc`.
- Floats round-trip GPR -> XMM -> GPR on every operation (`emitFloatBinaryOp`).
- Division is always `cqo; idiv` even by constants; multiplication by constants
  is never strength-reduced; there is no `lea`, no immediate-operand forms.
- The entry function reserves a fixed 1MB value stack regardless of need.
- `parameterCount` is known but arguments still go through the memory stack.

Concretely, `benchmarks/aggregate.prime`'s loop body
(`assign(sum, plus(sum, value))`, `assign(sumsq, plus(sumsq, multiply(value,
value)))`, `assign(value, plus(value, 1i64))`, plus the `repeat` counter) is
emitted today as about 70 x86_64 instructions per iteration, of which this is
the first statement (objdump of the `--emit=native` output on this machine):

```text
mov    -0x100078(%rbp),%rax      ; LoadLocal sum
mov    %rax,%r14
mov    -0x100058(%rbp),%rax      ; LoadLocal value
sub    $0x10,%r15                ; spill cached sum to the memory value stack
mov    %r14,0x8(%r15)
mov    %rax,%r14
mov    %r14,%rcx                 ; pop value
mov    0x8(%r15),%rax            ; reload sum
add    $0x10,%r15
add    %rcx,%rax                 ; AddI64
mov    %rax,%r14
mov    %r14,%rax
mov    %rax,%r14
sub    $0x10,%r15                ; (value stack traffic for the StoreIndirect-style result)
mov    %r14,0x8(%r15)
mov    %rax,%r14
mov    %r14,%rax
mov    %rax,-0x100078(%rbp)      ; StoreLocal sum
mov    0x8(%r15),%rax
add    $0x10,%r15
```

An optimizing backend would emit one `add` with both operands in registers.
The loop test (`repeat` counter against zero) is `movabs $0,%rax; ...; cmp;
setg; movzbq; mov; mov; test; je`, where `cmp $0; jle` suffices.

There is also a complete but **unwired** register pipeline in `src/ir/`:
`IrVirtualRegisterLowering` (stack IR -> basic blocks with block-local virtual
registers for operand-stack values), `Liveness`, linear-scan `Allocator`,
`SpillInsertion`, `Scheduler`, `Verifier`, and a lift back to stack IR.
`docs/spec/runtime-model.md` ("Native Allocator & Scheduler") describes it as
the native optimization path, but no production code calls any of it; only the
`ir.pipeline.serialization` tests do. Important limits of that scaffold:

- Only operand-stack values become virtual registers. `LoadLocal`/`StoreLocal`
  stay as memory instructions and `StoreLocal` is a scheduler barrier, so the
  thing that dominates the benchmark loops (locals) is never in a register.
- Its output is lifted straight back to stack IR, so the allocator's decisions
  never reach the emitter.

### 1.3 The VM

`src/runtime/VmExecutionKernel.cpp` `stepImpl` executes one instruction per
call. Per instruction it currently:

1. Re-derives `frames.back()`, `fn`, `locals`, `ip` from the frame vector.
2. Calls `handleSharedVmControlFlowOpcode` (a separate switch in another TU)
   which returns `NotHandled` for ~95% of instructions.
3. Runs the main `switch`; arithmetic falls to `default:`, calls
   `isPureNumericOpcode` (another switch), then `handleVmNumericOpcode`, then
   `executePureNumericOpcode` (another switch).
4. Does `std::vector::push_back`/`pop_back` on the operand stack with a
   `stack.size()` underflow check on every pop, even though validation could
   guarantee balance statically.
5. On `Call`, heap-allocates a fresh `std::vector<uint64_t>` of locals.
6. On `LoadIndirect`/`StoreIndirect` to the VM heap, linearly scans every
   allocation ever made (`VmHeapHelpers.cpp:35-45`) to find the owner. Vector
   and map code pays O(live allocations) per element access.
7. Dispatches print/file/host opcodes through virtual calls on `VmKernelHost`.

None of this is wrong, and the debug session (`VmDebugSession`) depends on the
single-step kernel and byte-identical traces, so a faster kernel must coexist
with the stepping one rather than replace it.

### 1.4 Tooling gaps that matter for this work

- `scripts/benchmark.sh` times native only on Darwin arm64 (`RUN_NATIVE`), and
  never times the VM. There is no runtime perf gate for either on Linux.
- `--dump-stage=ir` prints the AST through `IrPrinter`, not the lowered
  `IrModule`; the only way to see real IR is `--emit=ir` (binary `.psir`).
- There is no generic differential harness comparing vm / native / exe output
  for the same program; math conformance and a few feature suites do it ad hoc
  (`test_compile_run_math_conformance_helpers.h`).
- `test_ir_pipeline_serialization_control_flow_native_backend.h` and the
  `native_backend.core` / `math_numeric` compile-run suites are compiled out on
  Linux, so the x86_64 emitter has less coverage than the arm64 one.
- Default `--emit` is `native` (`OptionsParser.cpp:457`); `--help` says `exe`.

### 1.5 Baseline measurements

See section 7 (Linux x86_64, 4 cores, release build).

## 2. Goals and non-goals

Goals:

1. `--emit=native` becomes an optimizing compiler: a target-independent
   middle end on the shared IR plus a register-allocating native code
   generator for x86_64 and arm64.
2. The VM benefits from the same IR-level optimizations (it consumes the
   optimized IR) and additionally gets interpreter-level optimizations
   (dispatch, operand stack, locals, heap addressing, fused instructions).
3. GCC-style levels `-O0`, `-O1`, `-O2`, `-O3` (and `-Os` later) plus
   per-pass control `--opt-pass=<name>` / `--no-opt-pass=<name>` /
   `--opt-list` / `--opt-report`, for both `primec` and `primevm`.
4. Bit-exact behaviour across levels and backends, verified by differential
   tests, and deterministic output (same input, same flags, same bytes).

Non-goals (for this plan):

- Changing the C++ emitter or its `clang++ -O3` path.
- A JIT inside the VM (listed as a later option only).
- New language features or PSIR opcodes, except where section 6 explicitly
  allows a schema bump.

## 3. Architecture

```
            Program AST + SemanticProgram
                       |
                 IrLowerer (unchanged)
                       |
                stack IR (PSIR v27)  ----validate----
                       |
          +------------+-----------------+
          |  IR optimizer (new, target-  |   <- Phase 1 + 2, all backends
          |  independent, level-driven)  |
          |   1. stack-IR passes         |
          |   2. CFG/register-form passes|
          |   3. stackify back to PSIR   |
          +------------+-----------------+
                       |            \
             optimized stack IR      \  register-form IR (kept for native)
            /     |      |      \      \
          vm    cpp    wasm    glsl     native codegen (new, Phase 3)
          |                              regalloc + isel + layout
   VM pre-decode (Phase 4):              x86_64 / arm64 encoders (existing
   fused ops, flat stack,                 low-level emitters reused)
   O(1) heap, single dispatch
```

Key decisions:

- **PSIR stays the interchange format.** Optimizations never add opcodes to
  PSIR in Phases 1-4; the VM's fused instructions are an in-memory form built
  after loading. `.psir` files produced at any `-O` level remain v27.
- **One middle end, two consumers.** The register-form IR is produced from
  stack IR by extending the existing `IrVirtualRegisterLowering` (basic blocks
  and operand-stack vregs already exist; add promoted locals, edge moves, and
  an escape rule for `AddressOfLocal`). The native backend consumes the
  register form directly; every other backend gets it stackified back to PSIR.
- **Pass manager with a manifest**, mirroring `semanticValidationPassManifest()`
  and `irPreparationPhaseManifest()`: every pass has a stable name, the levels
  it is on by default, the backends it is valid for, and the analyses it
  invalidates. The manifest is what `--opt-list` prints and what tests
  enumerate.
- **The VM at `-O0` is the reference semantics.** Every pass is checked by
  running the program through the unoptimized VM and comparing output, exit
  code, and (for `--debug-trace`) the trace where the level allows it. The
  test runner that does this for every program and configuration is
  described in section 8.
- **Division and effects are barriers.** `Div*` can fault (VM) or trap
  (native `idiv`) so it is never removed, hoisted, or folded unless the divisor
  is a provably nonzero constant. Print, file, heap, host-call and
  `StoreIndirect`/`LoadIndirect` keep program order.

## 4. Optimization levels and pass catalogue

Pass names are the strings accepted by `--opt-pass`/`--no-opt-pass`.

| Level | Meaning | Passes on by default |
| --- | --- | --- |
| `-O0` | Compile fast, exact source maps, debugger-friendly. Today's behaviour. | none |
| `-O1` | Cheap, always-safe cleanups on stack IR. | `cfg-simplify`, `const-fold`, `peephole`, `dead-store`, `dce`, `inline-leaf` |
| `-O2` | Register-form optimizations and the register-allocating native codegen. | `-O1` + `mem2reg`, `copy-prop`, `cse`, `licm`, `inline`, `tail-self-loop`, `native-regalloc`, `native-isel`, `native-layout`, `vm-fuse` |
| `-O3` | Spend compile time for speed. | `-O2` + `inline-aggressive`, `unroll`, `schedule`, `vm-fuse-aggressive` |
| `-Os` | (later) `-O2` with inlining budget reduced and `unroll` off. | |

Pass catalogue:

- `cfg-simplify`: remove unreachable blocks, thread `Jump -> Jump`, drop
  jumps to the next instruction, fold `JumpIfZero` on constants, merge blocks.
- `const-fold`: fold `Push*; Push*; <pure op>` and conversions using the exact
  VM semantics (64-bit wrap, sign-extension rules, `ReturnI32` truncation,
  IEEE float ops in the host's default rounding). Never folds `Div*` by zero.
- `peephole`: `Dup; Pop` elimination, `StoreLocal x; LoadLocal x` to
  `Dup; StoreLocal x` (when `x` has no address taken), `StoreIndirect; Pop`
  shape cleanup, `PushI64` of an i32-range value to `PushI32`, repeated
  `LoadLocal x; LoadLocal x` to `LoadLocal x; Dup`.
- `dead-store` / `dce`: remove stores to locals never read before the next
  store or function exit, and pure instructions whose results are only
  popped. Needs the local-escape analysis (section 6).
- `inline-leaf`: today's `--ir-inline` (`IrInliner.cpp`), promoted to a pass.
- `inline` / `inline-aggressive`: inline callees with control flow (relocate
  jump targets, renumber locals) under a size budget per call site and per
  caller; `-O3` raises the budget. Recursion is never inlined.
- `mem2reg`: promote locals whose address is never taken to virtual
  registers in the register form; introduce edge moves at block boundaries.
- `copy-prop`, `cse`: block-local copy propagation and common-subexpression
  elimination over pure ops (identical opcode and operands).
- `licm`: natural-loop detection (from `repeat`/`while`/`for` lowering) and
  hoisting of loop-invariant pure, non-trapping instructions.
- `tail-self-loop`: turn a self tail call marked by `InstrumentationTailExecution`
  into a jump to the function entry (frame reuse).
- `unroll`: fully unroll `repeat(k)` for small constant `k` and small bodies;
  peel otherwise. `-O3` only.
- `schedule`: the existing block-local list scheduler, re-targeted at the
  register form. Expected low value on out-of-order cores; kept selectable.
- `native-regalloc`: linear scan over the register form with register
  classes (GPR, XMM/V), fixed-register constraints (`idiv`, syscall clobbers),
  call-crossing live ranges, spill slots in the frame. Builds on
  `IrVirtualRegisterAllocator.cpp`.
- `native-isel`: compare-and-branch fusion, immediate operands, `lea`, shifts
  for power-of-two multiply, `test` for compare-with-zero, floats kept in
  XMM/V registers, 32-bit `mov` for small constants, materialize booleans
  only when consumed as values.
- `native-layout`: block ordering by fallthrough, loop rotation, short branch
  encodings, value-stack frame sized from the computed maximum depth.
- `vm-fuse` / `vm-fuse-aggressive`: VM-internal superinstructions produced at
  module load from profiled patterns (`LoadLocal a; LoadLocal b; Add; StoreLocal
  a`, `LoadLocal; PushI32; Cmp*; JumpIfZero`, local increment, `Dup; StoreLocal`).
  Disabled under debug sessions and traces.

## 5. Phases

Each numbered item becomes one `TODO-XXXX` leaf in `docs/todo.md` with its
own scope, acceptance and stop rule. Order within a phase is a dependency
order unless stated. Leaves filed so far (2026-10-03) cover Phase 0, the
analysis prerequisites, and the `optexe` slice; the optimizer passes of
Phases 1-2 and the native code generator of Phase 3 are not filed yet and
should be split into leaves once `optexe` has validated the register form.

| Plan item | Leaf |
| --- | --- |
| 0.1 flags | TODO-5460 |
| 0.2 pass manager | TODO-5461 |
| 0.3 IR dumps | TODO-5462 (`ir-lowered`), TODO-5461 (`ir-optimized`) |
| 0.4 benchmarks | TODO-5463 |
| 0.5 matrix runner | TODO-5464 (VM output sink), TODO-5465 (runner), TODO-5466 (migration) |
| 1.1 shared CFG, stack-balance check | TODO-5467, TODO-5468 |
| 1.3 shared pure-opcode semantics | TODO-5469 |
| 1.4 local escape analysis | TODO-5470 |
| 2.1 register form with promoted locals | TODO-5471 (done: test-only `promoteLocals` lowering, local-form verifier, liveness; no consumer yet) |
| 2.1b / section 9 `optexe` | TODO-5472..5475 (done), TODO-5476 (matrix, benchmarks) |
| found while testing | TODO-5477 (i32 overflow semantics), TODO-5478 (front-end cost on huge functions, deferred) |

### Phase 0: measurement and control surface (no behaviour change)

0.1 Add `OptimizationOptions` to `Options`: level, explicit pass enables and
    disables, `verifyEachPass`, `report`. CLI: `-O0`..`-O3`, `--opt-pass`,
    `--no-opt-pass`, `--opt-list`, `--opt-report`, `--opt-verify-each`, for
    `primec` and `primevm`. Default stays `-O0` until Phase 5.
0.2 Pass manager and `irOptimizationPassManifest()`; `optimizeIrModule()`
    inserted into `prepareIrModule` after validation, re-validating after
    the pipeline (and after every pass under `--opt-verify-each`). Add a
    stack-balance check to `validateIrModule` so broken passes fail early
    (done, TODO-5468: it runs the shared `IrCfg` analysis for every target).
0.3 `--dump-stage=ir-lowered` and `--dump-stage=ir-optimized` printing the
    real `IrModule` (text, deterministic), and an `--opt-report` listing per
    pass: instructions before/after, time. Fix the `--help` default text.
0.4 Benchmark harness: time `--emit=native` on Linux x86_64 too, add a
    `primestruct_vm` entry, and an `-O` matrix (`BENCH_OPT_LEVELS`). Add
    baseline entries for vm and native at `-O0` from this machine.
0.5 Program matrix test runner (section 8): `ProgramMatrix.h`, the VM output
    sink, one hand-ported suite, then the migration script and the
    deduplicated `programs/` tree. Add the seeded random stack-IR generator
    whose modules run through the `-O0` VM and every other config.
0.6 Fix the benchmark speed oracle: `benchmarks/README.md` and the baseline
    JSON treat `--emit=exe` as the fast path; record that the C/C++ reference
    programs are the baseline and `exe` is a correctness oracle (section 7).

### Phase 1: stack-IR passes (`-O1`), all backends

1.1 Shared CFG utilities (`IrCfg`): block leaders, successor edges,
    reachability, stack depth per block. Factored out of
    `IrVirtualRegisterLowering.cpp` and `computeMaxStackDepth`.
1.2 `cfg-simplify`.
1.3 `const-fold` with an exact per-opcode semantics table shared with the VM
    (`VmKernelBoundary.cpp` becomes the single source of truth for pure ops).
1.4 Local escape analysis (which locals may be addressed, per function) and
    `dead-store`, `dce`, `peephole`.
1.5 `inline-leaf` as a manifest pass; `--ir-inline` kept as an alias.
1.6 Golden IR-dump tests per pass; differential corpus green at `-O1`.

### Phase 2: register-form middle end (`-O2`, target-independent)

2.1 Extend the register form: promote non-escaping locals to virtual
    registers, edge moves for locals and stack values, per-function
    "pinned local" set. Verifier updated.
2.1b First consumer: a register-form C++ emitter for `--emit=exe`/`cpp` at
    `-O1` and above (section 9). One C++ scalar per virtual register and
    per promoted local, labelled basic blocks with `goto`, pinned locals in
    a `uint64_t frame[]`. The host C++ compiler then supplies register
    allocation and instruction selection, which makes this the cheapest way
    to prove the register form on the whole corpus and gives the speed
    oracle section 7 lacks. The existing `switch (pc)` emitter stays as the
    `-O0` form.
2.2 `copy-prop`, `cse`, algebraic simplification on the register form.
2.3 Loop analysis and `licm`; `tail-self-loop`.
2.4 General `inline` with budgets; interaction with the lowerer's
    two-call-sites heuristic measured and tuned.
2.5 Stackification: lower the register form back to PSIR for vm, cpp, wasm,
    glsl (expression-tree scheduling; multi-use values become locals).
    Round-trip property tests: lower -> optimize(no passes) -> stackify is a
    semantic no-op on the whole corpus.
2.6 Differential corpus green at `-O2` on vm; benchmark report for vm.

Status (2026-10-03): `copy-prop` is implemented on the stack form (TODO-5480): a forward must-analysis of "local `t`
holds the value of local `a`" over the CFG, killed by writes to either side, skipped for functions that take local
addresses; `dead-store` then drops the unread copies. Two peepholes came from the lowered json_parse loop:
`cmp; push 0; ne` on a comparison result (the `a && b` shape) and a constant pushed into a join-point
`JumpIfZero`, which now jumps straight to the outcome. Instruction counts at `-O2`: aggregate 45 -> 39, json_scan
146 -> 103, json_parse 374 -> 281. `cse`, `licm`, `inline` and `tail-self-loop` are not started.

### Phase 3: optimizing native code generator

3.1 New codegen path from the register form, behind `-O2`; `-O0`/`-O1` keep
    the template emitter. Shared driver stays templated over
    `X64Emitter`/`Arm64Emitter`, reusing the existing low-level encoders.
3.2 `native-regalloc`: register classes, fixed constraints, call clobbers,
    spill slots; pinned locals keep their 16-byte frame slots so
    `AddressOfLocal` arithmetic is unchanged.
3.3 `native-isel`.
3.4 Register-based internal calling convention for `Call`/`CallVoid` (args in
    registers up to N, rest on the value stack), callee-saved set, no cache
    flush; entry value-stack size from `computeMaxStackDepth`.
3.5 `native-layout` and short branches.
3.6 arm64 parity for 3.1-3.5. Can only be built here for x86_64; arm64 must be
    validated on a macOS machine before the default flips.
3.7 Enable the Linux-excluded native suites where the exclusion reason no
    longer holds, so x86_64 gets the same coverage as arm64.

Status (2026-10-03): the template emitter's biggest costs are gone on x86_64 without a new code generator, in three
steps, all on from `-O1` (`NativeEmitterOptions::promoteLocals` / `deferOperands`, set by the native backend from the
IR level; the arm64 emitter is untouched):

1. Register-resident locals (`NativeEmitterPromotion.h`): the most used locals that no memory access can reach
   (`IrLocalEscape`; uses in loops count ten times per nesting level) live in rsi, rdi, r8, r10 and r11 (plus r12 and r13 when no
   function reads argc/argv) for the whole function. Measured dead ends on json_parse (about 20 hot state variables):
   taking rbx and r9 from the operand cache for the promotion pool did not help, and `LoadStringByte` straight into a
   cache register cut instructions by 8% (163M to 149M) without changing the run time, which is bound by
   loop-carried values that stay in memory; ranking by static use count put rarely executed locals ahead of the loop-carried ones, so uses now halve per
   conditional arm (a forward branch that is not a loop test), which cut json_parse from 149.5M to 121.1M instructions
   (optexe: 116M) with the same output but no change in run time: the run time follows branch mispredictions
   (cachegrind branch simulation: native 34M branches and 2.7M mispredicts, 17 ms; optexe 26M and 1.8M, 9.8 ms, about
   15 cycles per mispredict in both), so register assignment is not the lever for json_parse; if-converting
   `if (c == K) x += 1` chains into branchless code is. Sharing registers between locals whose live ranges never meet (per-instruction backward liveness over the
   candidates, spills and reloads restricted to the live ones) left json_parse at 149.3M instructions against 149.5M
   because its hot state variables are live at the same time, so the gap to optexe is register pressure that needs
   more than seven pool registers or a real allocator with spilling, not better packing. Instructions whose templates clobber those registers (printing, file and heap operations, string table
   lookups, calls) get the locals written to their frame slots before and reloaded after.
2. Deferred operands: the emitter tracks the top of the operand stack at compile time. Constants and register locals
   are pushed lazily, `add`/`sub`/`mul`/compare/`neg`/`dup`/`pop` work on registers and immediates directly, and the
   rbx/r9/r14 cache registers hold computed values; operands reach the memory stack only at branch targets, in front of
   jumps and calls, before opcodes that clobber the cache registers, and when more than three are live. A
   comparison feeding `JumpIfZero` becomes one `cmp` and `jcc`, and `local = local OP (local|constant)` becomes one
   instruction on the register.
3. A fix found on the way: the print scratch area was placed relative to `rbp` instead of the frame bottom, so a
   print in a called function overwrote that function's locals and a print under a deep operand stack overwrote
   operands (`print_in_callee`, `print_with_deep_stack`).

Verification: the full release gate (every native compile-run suite) passes with both on;
`primestruct.ir.native_codegen` compares 60 random programs, the fused-form grid, calls and recursion, deep operand
stacks, aliased locals and prints against the VM in plain and optimized mode; `scripts/differential_opt_check.py
--native` checks native `-O0` against native `-O2` and the VM over the corpus: 505 programs identical, 336 rejected by the
native-profile lowering, and nine that differ from the VM by design (VM-only faults and host imports, listed in the
script). Checking it also found a native bug that predates this work: `HeapRealloc` kept the new size in rcx across the
mmap syscall, which clobbers rcx, so every shrinking realloc overran the new block (eight corpus programs segfaulted);
fixed, with `heap_realloc` as the regression.

### Phase 4: VM interpreter engineering (independent of IR passes)

4.1 Fast kernel: one dispatch switch (computed goto where the compiler
    supports it), frame state in locals, operand stack as a raw pointer into
    a buffer sized by precomputed maximum depth, no per-op underflow checks
    (validation guarantees balance), contiguous locals arena with frame base.
    The debug stepper keeps the checked path; both must agree on every
    corpus program.
4.2 O(1) heap addressing: direct slot index with an owner table instead of
    scanning allocations; keep the scan for fault diagnostics.
4.3 `vm-fuse`: in-memory pre-decode into fused instructions; pattern list
    chosen from static instruction-pair counts over the benchmark and corpus
    IR (`--opt-report` prints them). Off under `--debug-*`.
4.4 Out-of-line fault paths and host dispatch (print/file via function
    pointers set once per run rather than virtual calls per instruction).

Status (2026-10-03): 4.1 and 4.3 are implemented in `src/runtime/VmFastKernel.cpp` (TODO-5479). `executeVmKernel`
(plain runs; never debug sessions) first asks the fast loop to take the module. It accepts a module when every
function passes `buildIrCfg`, every reachable return leaves the caller's stack balanced and the entry takes no
parameters; otherwise the step kernel runs it unchanged. The loop keeps `ip`, the operand-stack pointer and the
locals pointer in registers, sizes the stack from the CFG's maximum depth, uses one locals arena, and fuses
sequences inside a basic block into one instruction (`LoadLocal; Push; Cmp; JumpIfZero`,
`LoadLocal; Push; Add; StoreLocal`, `Dup; StoreLocal; Pop`, compare-and-branch, and similar; the pairs came from
an opcode-pair histogram of json_parse). Fused forms do not change results or fault order: all are fault-free except the
string-byte forms (`LoadLocal; LoadStringByte [; StoreLocal]`), which fault on a bad index before storing or popping,
exactly where the original instruction would. Module-table strings resolve inline in the loop.
`PRIMEVM_KERNEL=step` forces the step kernel for comparisons, and
`scripts/differential_opt_check.py --baseline-kernel step` runs the 850-program corpus against it at -O0 and -O2
(equal stdout, stderr and exit code); `primestruct.ir.vm_fast_kernel` runs random programs, every fault, calls and
recursion, files and a fused-form grid through both kernels. 4.2 (O(1) heap addressing in the VM) and 4.4 are open.

Measured wall time of `primevm` including the 13-70 ms compile (seconds):

| program | before | fast loop | + fused forms | + fused forms, -O2 |
| --- | --- | --- | --- | --- |
| aggregate | 1.25 | 0.24 | 0.10 | 0.095 |
| json_scan | 1.31 | 0.30 | 0.17 | 0.10 |
| json_parse | 2.23 | 0.48 | 0.30 | 0.23 |

### Phase 5: defaults, docs, gates

5.1 Flip defaults: `-O2` for `--emit=native` and `--emit=vm`/`primevm`,
    `-O0` whenever a debug session, `--debug-trace`, or `--debug-replay` is
    active. Document the rule.
5.2 Replace the stale "Native Allocator & Scheduler" section in
    `docs/spec/runtime-model.md` and the stale "lowering still inlines all
    calls" statements in `vm-design.md`, `architecture.md`,
    `backend-type-support.md`; document levels and passes in the integration
    doc and README.
5.3 Perf gates: baseline JSON entries per backend and level; regression
    ratio checks in `scripts/check_benchmark_report.py`.

Status (2026-10-03): 5.2 is partly done: the stale "lowering never emits Call/CallVoid" statements in `Ir.h`,
`architecture.md`, `runtime-model.md` and `vm-design.md` are corrected (lowering emits real calls for functions it does
not inline), and the "Native Allocator & Scheduler" section now says the `IrVirtualRegister*` pipeline is not wired into
any backend. `backend-type-support.md` and a levels/passes page in the integration doc are still open. 5.1 is done. `parseOptions` picks `-O2` when no level is given for `primevm` and for
`--emit=vm`, `native`, `optexe` and `optcpp`; dumps, debug sessions (`--debug-json`, `--debug-dap`, `--debug-trace`,
`--debug-replay`) and the other emit kinds stay at `-O0`, and an explicit `-O` always wins. The full release gate
passes with it, including every native compile-run suite on x86_64. `scripts/benchmark_backends.py` records the
numbers below (median of 3; executable rows list run time and, in brackets, the compile time):

| config | aggregate | json_parse | json_scan |
| --- | --- | --- | --- |
| vm step kernel -O0 | 1.27 s | 2.22 s | 1.45 s |
| vm -O0 (flat loop) | 103 ms | 272 ms | 161 ms |
| vm -O2 | 94 ms | 234 ms | 99 ms |
| native -O0 (template expansion) | 30 ms (+11) | 50 ms (+111) | 33 ms (+23) |
| native -O2 (register locals, deferred operands) | 4.2 ms (+11) | 22 ms (+71) | 11 ms (+22) |
| optexe -O2 | 2.0 ms (+618) | 10 ms (+602) | 9.2 ms (+584) |
| exe (old C++ emitter, clang -O0) | 4.14 s (+341) | 6.95 s (+450) | 4.06 s (+404) |
| C reference, cc -O3 | 4.8 ms (+115) | 8.3 ms (+66) | 3.5 ms (+55) |

Before the Phase 3 work below, native -O2 was 22 to 32 ms (4x to 15x behind C) because its template expansion kept
every local and every operand in memory; the IR passes barely moved it.

## 6. Constraints, risks, and how each is handled

- **Exact semantics.** Every pure opcode's semantics is defined once (the
  VM's implementation) and the folder calls the same functions. Float folding
  uses the host's IEEE operations exactly as the VM does; `NaN` comparisons
  follow the VM. Differential tests at every level are the gate.
- **`AddressOfLocal` aliasing.** Implemented in `IrLocalEscape.h`. A pointer to a slot of a function's own frame can
  only come from an `AddressOfLocal` in that function, but user code may add negative byte offsets
  (`plus(location(v), 8i32)` is legal and the VM resolves whichever slot the sum names), so the v1 rule is: if a
  function takes any local's address, every slot of that function is pinned; otherwise nothing is, except the slot a
  `FileReadByte` writes. The analysis records the lowest addressed slot so a later pass that can prove offsets are
  non-negative constants can relax the rule. Measured effect: scalar loops (the benchmarks) pin nothing; functions
  using structs or `location(...)` pin everything.
- **Determinism.** Passes iterate in instruction, block, or function index
  order; no unordered containers decide output; the manifest fixes pass
  order; `--opt-report` output is sorted.
- **Debugging and source maps.** Passes keep the `debugId` of surviving
  instructions and drop entries for removed ones. `-O0` remains exact. Debug
  sessions force `-O0` and no `vm-fuse` so traces stay byte-identical.
- **Effects.** Function `effectMask` is unchanged by optimization; inlining
  does not widen a caller's mask beyond what the lowerer already computed
  (it uses transitive effects).
- **PSIR stability.** No schema change in Phases 0-4. A possible v27 later:
  per-local extent metadata (for better pinning) and shift/bitwise opcodes
  (for strength reduction that reaches the VM too). Each would follow the
  existing versioning rule.
- **Build cost and test time.** New passes live in `primec_ir_lib`; native
  codegen in `primec_backend_emitters_lib`; VM kernel in `primec_runtime_lib`.
  New doctest shards registered in `cmake/PrimeStructManaged*.cmake` with the
  inventory script. Differential shards are compile-run tests and need the
  usual 10-case sharding.
- **Platform coverage.** Only x86_64 can be built and run in this environment.
  arm64 changes are mirrored but need a macOS run before any default flip.
- **GLSL/SPIR-V.** Those targets reject `Call`; the inliner must not create
  calls (it only removes them) and the manifest marks passes valid per
  `IrValidationTarget`.

## 7. Baseline numbers (this machine)

Measured 2026-10-03 on Linux x86_64 (4 cores), `build-release`, mean of 3
runs, wall clock of the whole process. The `vm` column is `primec --emit=vm`
and so includes compile time; the compile-only time (`--emit=ir`) is listed so
the pure interpretation time can be read off.

| Benchmark | C `-O3` | C++ `-O3` | `--emit=exe` | `--emit=native` | `--emit=vm` | compile only |
| --- | --- | --- | --- | --- | --- | --- |
| aggregate (N=5,000,000) | 3.8 ms | 4.0 ms | 3484 ms | 28.8 ms | 1086 ms | 8.6 ms |
| json_scan (20,000 scans) | 2.9 ms | 3.3 ms | 3449 ms | 30.7 ms | 1308 ms | 20.6 ms |
| json_parse | 9.1 ms | 8.1 ms | 5814 ms | 43.4 ms | 1795 ms | 56.8 ms |

Reading the table:

- The direct native output is 7x to 11x slower than C `-O3` on these loops.
  That gap is almost entirely the memory value stack and memory-resident
  locals shown in section 1.2; there is no algorithmic difference.
- The VM is roughly 40x slower than today's native output and about 300x
  slower than C. The dispatch structure in section 1.3 explains most of it.
- `--emit=exe` is **not** a speed baseline. `IrToCppEmitter` emits a C++
  program that re-implements the stack machine (`psEnsureStack`,
  `psResolveHeapSlot`, a `std::deque` operand stack), and
  `compileCppExecutable` builds it with `clang++ -O0`
  (`src/support/ExternalTooling.cpp:42`), deliberately, for compile speed.
  Compiling the same generated source at `-O2` by hand gives:

  | Benchmark | exe at `-O0` (run / compile) | same source at `-O2` (run / compile) | C `-O3` |
  | --- | --- | --- | --- |
  | aggregate | 4091 ms / 5.4 s | 609 ms / 1.4 s | 3.8 ms |
  | json_scan | 4189 ms / 0.8 s | 738 ms / 1.8 s | 2.9 ms |
  | json_parse | 6791 ms / 0.8 s | 1753 ms / 3.9 s | 9.1 ms |

  So 4x to 7x of the gap is the `-O0` flag and the remaining 160x to 250x
  is the switch-dispatch stack machine, which clang cannot turn back into
  loops. The first compile of aggregate at `-O0` includes a cold start.
  `exe` stays a correctness oracle; the C and C++ reference programs in
  `benchmarks/` are the speed baseline.

Targets for acceptance of the whole programme:

- native `-O2`: within 2x of C `-O3` on all three benchmarks; `-O3` within
  1.5x on aggregate.
- vm `-O2` with the Phase 4 kernel: at least 4x faster than today's VM on
  all three benchmarks (interpretation time, compile time excluded).
- Compile time of `--emit=native -O2` for the 100,000-line compile-speed
  source within 1.5x of `-O0`.

## 8. Testing strategy: one program, many execution forms

### 8.1 What exists

- `tests/unit/compile_run/` holds about 3,570 doctest cases. 962 are in `vm/`
  and 927 in `native_backend/`. Each case embeds a `.prime` source as a raw
  string, writes it to scratch, builds a `./primec --emit=<kind> ...` shell
  command by hand, and checks an exit code or a redirected stdout file.
- The backend is hard-coded per file and per case. 472 of the 818 distinct VM
  test programs also appear verbatim in the native suite: the same program,
  two files, two command lines, two expectations.
- Cross-backend checks are hand-rolled per feature: math conformance uses
  `exe` as the oracle and compares `vm` (and `native` only under an Apple
  arm64 `#if`); reflection runtime tests spell out all three backends inline;
  the vector/map conformance helpers have their own loops.
- Every case spawns at least one `primec` process (full parse, semantics and
  lowering) and, for native, the produced binary; `exe` cases additionally
  run `clang++`, which is why those shards have 900-second timeouts and a
  content-addressed compile cache.
- CTest registration (`addPrimeStructManagedDoctestSuite`,
  `scripts/check_test_registration.py`) needs a statically enumerable case
  list per binary and filter: shards are `--first/--last` ranges that must
  tile the real case count exactly. A matrix design must therefore keep "one
  program = one doctest case" or change the guard.
- Most cases fit two mechanical shapes: 731 VM cases are "run, check exit
  code" (447 also redirect stdout), and 362 native cases are "compile, run,
  check exit code". The rest check diagnostics, files, or multi-step output.

### 8.2 Design

Separate **what** a test asserts (a program and its expectation) from **how**
it is executed (a backend configuration), and let one declaration run under
every applicable configuration.

```cpp
// include/primec/testing/ProgramMatrix.h (new)
struct ProgramCase {
  std::string_view name;
  std::string_view source;        // .prime text; "{scratch}" is substituted per run
  ProgramExpectation expected;    // exitCode(n) | stdoutIs(text) | stdoutMatches(oracle) | compileError(substr)
  ProgramRequirements requires;   // effects/features: heapAlloc, fileIo, hostCalls, dynamicStrings, lambdas...
  std::vector<std::string> programArgs;
};

struct ExecutionConfig {
  std::string_view backend;       // "vm", "native", "exe", "wasm"
  OptimizationLevel level;        // O0..O3
  std::vector<std::string> passOn, passOff;   // --opt-pass / --no-opt-pass
  bool fusedVm = false;           // Phase 4 vm-fuse on/off
};

// Runs `program` under every config that its requirements and the host allow,
// checks the explicit expectation when present, and checks that every config
// agrees with the oracle config (vm, O0) on stdout and exit code.
void runProgramMatrix(const ProgramCase &program, std::span<const ExecutionConfig> configs);
```

- **One doctest case per program.** `PROGRAM_CASE(...)` expands to exactly
  one `TEST_CASE` whose body calls `runProgramMatrix` with the suite's config
  set; configurations become `SUBCASE`s so a failure names the config. Case
  counts stay static, so the registration guard and sharding are unchanged.
- **Oracle, not just literals.** The `-O0` VM is the reference. A program
  with a literal expectation is checked against it; every other config is
  checked for parity with the oracle (stdout, exit code). Float formatting
  differences use the allowlist mechanism math conformance already has.
- **Requirements, not `#if`.** Backend and platform availability is decided
  at run time: `native` unavailable on this host, `CallHost`/dynamic strings
  VM-only, lambdas rejected on vm/native, `exe` needing a C++ compiler. A
  skipped config is logged by name, never silent, so coverage is visible.
- **Compile once in process.** The runner calls `runCompilePipeline` and
  `prepareIrModule` once at `-O0`, then for each config copies the
  `IrModule`, runs `optimizeIrModule(level, passes)`, and executes:
  - `vm`: in process through `Vm::execute`, with program output captured
    through an output sink on the VM host (today prints go straight to
    `::write`/`fwrite` in `VmIoHelpers.cpp`; the sink is also what embedders
    want). The fused kernel is the same entry point with a flag.
  - `native`: `NativeEmitter::emitExecutable` to scratch, then spawn the
    binary (milliseconds; no `primec` process).
  - `exe`: only in explicitly opted-in oracle suites, keeping the existing
    compile cache; never in the default matrix.
  - `wasm` (optional): through the existing `runWasmMainViaNode` when Node
    is present.
  A six-config matrix then costs about what two process spawns cost today,
  so the 5-second per-case guardrail holds.
- **Config sets per suite.** Each suite picks its default set (for example
  `{vm O0, vm O1, vm O2, vm O2 fused, native O0, native O2}`); a
  `PRIMESTRUCT_TEST_MATRIX=vm:O2,native:O3` environment variable narrows it
  for local debugging only and is not used in CTest registrations, so the
  tiling guard needs no change.
- **Pass-specific tests stay separate.** A pass's own behaviour is checked by
  golden `--dump-stage=ir-optimized` text (deterministic) in
  `tests/unit/ir_pipeline/optimizer/`, one `TEST_CASE` per pass per shape,
  plus `--opt-verify-each` runs. The matrix proves the program still behaves;
  the golden test proves the pass did what it claims.
- **Differential fuzzing** (Phase 0.5) uses the same runner with generated
  stack-IR modules instead of sources: a seeded generator produces balanced,
  validated IR; `-O0` VM is the oracle; a fixed number of `TEST_CASE`s each
  cover a seed range so the count is static.
- **Benchmarks reuse the configs.** `scripts/benchmark.sh` iterates the same
  `ExecutionConfig` names (`BENCH_CONFIGS=vm:O0,vm:O2,native:O2`) so the perf
  gate and the correctness matrix describe configurations the same way.

Status (2026-10-03): the runner exists as `tests/unit/program_matrix/program_matrix.h` (TODO-5465). It differs from the
in-process design above in one respect: every config runs through the built `primevm`/`primec` (as the compile-run
suites do), so no VM output sink is needed (TODO-5464 is deferred). A `ProgramCase` carries the source, the expected exit
code and stdout, per-family stderr, extra flags and skip/only lists; `runProgramMatrix` runs it on `vm-step-O0`, `vm-O0`,
`vm-O2`, `native-O0`, `native-O2` and, with `PRIMESTRUCT_MATRIX_CONFIGS=all`, `optexe-O2` and `exe`. With no expected exit
code the configs must agree with the first (the checked VM step kernel). `primestruct.program_matrix.control` holds the 22
mechanical cases of the native control suite and `primestruct.program_matrix.optimizer` eight programs with the shapes the
optimizer and the optimizing backends work on (short-circuit conditions, loops, calls, recursion, strings, floats, heap
collections, output inside loops); all pass on all seven configs. The migration of the duplicated vm/native cases is
TODO-5466. Other differential checks that already exist: `scripts/differential_opt_check.py` (850-program corpus against
optexe, native and the step kernel), `primestruct.ir.vm_fast_kernel`, `primestruct.ir.optexe` and
`primestruct.ir.native_codegen` (IR-level programs against the VM).

### 8.3 Migrating the existing tests

1. Land `ProgramMatrix.h` and port one small suite by hand (for example
   `native_backend.control`, 31 cases) to validate the API and the capture
   path. Register it with the normal sharding and inventory steps.
2. Write `scripts/migrate_compile_run_cases.py` that recognises the two
   mechanical shapes above (exit-code-only, stdout-redirect, compile+run) and
   rewrites them to `PROGRAM_CASE` declarations, keeping case names and order
   so shard ranges stay valid. It reports every case it could not convert;
   those keep their current form and are converted by hand or left as is.
3. Deduplicate: the 472 programs present in both `vm/` and `native_backend/`
   become one `PROGRAM_CASE` each under a backend-neutral
   `tests/unit/compile_run/programs/` tree with one matrix run covering both
   backends. The script emits a count reconciliation (old vm + old native -
   duplicates = new) so no program is lost, and the suite files, shards, and
   `tests/TEST_INVENTORY.md` are regenerated in the same change.
4. Replace the per-feature cross-backend loops (math conformance,
   reflection runtime, vector/map conformance) with `runProgramMatrix` and
   their existing allowlists; delete the Apple-only `#if` guards in favour of
   requirements.
5. Leave diagnostics tests (compile errors, dump comparisons) and
   file-system side-effect tests alone unless they are runnable programs; the
   matrix is for programs that execute.

Expected outcome: every runnable program in the corpus runs at every level on
every available backend by default, new programs are written once, and the
total test wall time goes down because the dominant cost (a `primec` process
per case) is paid once per program instead of once per backend.

## 9. Starting with the C++ emitter

`IrToCppEmitter` (`src/backend/IrToCppEmitter*.cpp`, about 1,600 lines) emits
a stack machine in C++: every function is `while (true) { switch (pc) { case
N: ... } }`, one `case` per IR instruction, with the operand stack in a
`std::deque`-backed `PsStack`, locals in a `std::vector<uint64_t>`, a
`psEnsureStack` underflow guard before every pop, and functions split into
1,024-instruction chunks so clang can compile the switch. The `pc` variable is
loop-carried and the deque is opaque, so clang cannot recover the loop
structure: even built at `-O2` the output is still 160x to 250x slower than C
on the benchmarks (section 7), and `exe` itself builds at `-O0`.

Doing the register form here first is attractive:

- It is the same `IrModule -> CFG -> virtual registers -> emit` pipeline the
  native backend needs, minus register allocation and instruction selection,
  which clang does. Each basic block becomes a label, each pure instruction
  becomes `uint64_t vN = <expr>;`, `JumpIfZero` becomes `if (vK == 0) goto
  L_M;`, promoted locals are C++ scalars, pinned locals stay in a
  `uint64_t frame[N]` so `AddressOfLocal` arithmetic and `LoadIndirect` keep
  their byte-offset semantics. No `pc`, no operand stack, no underflow guards
  (validation proves balance), no chunking (clang handles large structured
  functions; a fallback to the `-O0` emitter above a size threshold is cheap).
- It validates `mem2reg`, the escape rule, edge moves, and the pass pipeline
  on the whole compile-run corpus with an independent consumer before any
  native register allocator exists. Bugs in the register form show up as
  `exe` parity failures in the matrix.
- It produces the missing speed oracle: how fast this IR can run with a
  mature backend. If `exe -O2` still trails the C reference, the gap is IR
  quality (Phases 1-2); if it matches, the remaining native gap is codegen
  (Phase 3).
- The print, file, heap and string helpers in the generated preamble are
  reusable as functions taking values instead of popping a stack. The heap
  owner scan (`psResolveHeapSlot`) needs the same O(1) fix as the VM.

Things to settle while doing it:

- Semantics parity. The current C++ emitter computes `AddI64` as
  `int64_t left + right` and `DivI64` as `left / right`, which is undefined
  behaviour on overflow and on zero or `INT64_MIN / -1`, while the VM wraps
  in `uint64_t` and faults on a zero divisor. The register-form emitter should
  use the shared pure-opcode semantics (Phase 1.3) so all four execution
  forms agree bit for bit, and divisor checks must stay explicit.
- `i32` opcodes. `emitBinaryI32` narrows to 32 bits where the VM and native
  operate on the 64-bit slot; the differential matrix will expose any
  program where that differs, and the shared semantics table decides which
  is right.
- Chunking. Functions above a size threshold fall back to the `-O0` emitter
  rather than splitting labelled code across functions.

### 9.1 A separate `optexe` emit kind (recommended)

Add the register-form emitter as new emit kinds next to the old ones rather
than changing `exe`/`cpp` in place:

- `optexe` (compile) and `optcpp` (source only), following the existing
  user-facing / `-ir` split: `optexe-ir` and `optcpp-ir` are the registered
  backends, `resolveIrBackendEmitKind` maps the short names. `optcpp` is
  worth having because reading the generated source is the main way to debug
  an emitter.
- `exe` and `cpp` stay exactly as they are, so they remain an independent
  oracle for the matrix runner (section 8): same IR, two unrelated C++
  generators must agree. When `optexe` has run the corpus clean at every level,
  the alias flip is one line in `resolveIrBackendEmitKind`, and the old
  emitter can then be deleted or kept as `-O0`.
- `optexe` always uses the register form; the `-O` level only selects which
  middle-end passes run before it. The host compile flag is a separate
  decision: `exe` uses `-O0` for compile speed, `optexe` should default to
  `-O2` (measured above: 1.4 to 4 s compile versus 0.8 to 5 s at `-O0` for the
  old output) with a pass-through override.
- Registration touches: `EmitKind.cpp` (list, usage text, alias map),
  `IrBackends.cpp` (backend class, registry array size), `IrBackendProfiles.cpp`,
  `OptionsParser.cpp` (default output name), the backend-architecture and
  registry tests that enumerate backends, `README.md`, and the spec index.
  No change to `prepareIrModule`'s contract.
- Unsupported constructs (for example `CallHost`, which is VM-only) fail with
  a clear diagnostic rather than falling back per function; the old emitter's
  calling convention (shared `PsStack&`, `sp`) differs, so mixing forms inside
  one module is not worth the adapter.
- Risk to measure early: clang `-O2` time and memory on very large
  goto-style functions. The old emitter chunks at 1,024 instructions to stay
  compilable (TODO-4747 started from a 15MB generated file). Run `optexe` on
  the 100,000-line compile-speed source and keep the existing 12 s baseline
  gate; above a size threshold use `__attribute__((optnone))`/`-O1` for that
  function instead of splitting it.

#### Implementation status and measurements (2026-10-03)

`optexe` and `optcpp` are implemented in `src/backend/IrToOptCppEmitter.cpp` with its runtime text in
`IrToOptCppRuntime.h`. Deviation from the plan above: there is no register form. The emitter works on the stack
form directly, using the shared CFG (`IrCfg.h`) for per-block stack depths: stack depth *d* is the C++ variable
`s<d>`, locals are `l<n>`, blocks are labels, jumps are `goto`. Because depths are proven consistent at joins no
moves are needed, and the host compiler does the register allocation. Functions with `AddressOfLocal`,
`LoadIndirect` or `StoreIndirect` use a `uint64_t frame[N]` array (the VM resolves indirect addresses against the
current frame even when the address came from the caller, so any indirect access needs one). The heap keeps the
VM's address encoding and fault text but resolves addresses through an owner table in constant time.

Coverage: every opcode except `CallHost` (host imports; rejected with a diagnostic naming opcode and function).
The host compile flag is `-O<n>` from the command line (default `-O2`) with `-ffp-contract=off`.

Verification: pure-opcode results over ~6,000 edge-case operand pairs, 60 random programs (plain and IR-optimized),
heap/indirect, string/argv/file and fault tests all compare with the VM (`primestruct.ir.optexe`), and
`scripts/differential_opt_check.py --optexe` runs the 850-program corpus: 514 programs compile and match the VM's
stdout, stderr and exit code (the remaining 336 are compile-error cases or rejected earlier by the native-profile
lowering; none is rejected by the emitter). One systematic difference is accepted and canonicalized in tests: the
sign and payload of a NaN produced by arithmetic is unspecified (the host compiler folds `inf - inf` to the positive
NaN, x86 hardware at run time yields the negative one).

Run time on this machine (seconds; `exe` is the old emitter at `clang++ -O0`):

| program | vm -O0 | vm -O2 | exe | optexe -O2 |
| --- | --- | --- | --- | --- |
| aggregate | 1.25 | 0.95 | 4.12 | 0.0045 |
| json_scan | 1.31 | 1.06 | 4.18 | 0.0116 |
| json_parse | 2.23 | 1.95 | 6.94 | 0.0122 |

These benchmarks have constant inputs, so clang folds much of the work at compile time; the table shows the
ceiling, not what data-dependent programs will see. TODO-5463/5476 add non-foldable rows.

Compile time: for an `n`-statement `main` of arithmetic and branches, clang on the optexe source takes
0.8-1.0 s at n=2,000 and 1.2-1.4 s at n=8,000 at every `-O` level (1.1 MB of C++), against 2.8 s and 8.9 s for the
old emitter at `-O0`. The cost that grows is `primec` itself, before any emitter runs: 3.3 s, 6.5 s, 17 s and 84 s at
n=2,000, 4,000, 8,000 and 20,000 when this was first measured; after removing a discarded second fact collection and
memoizing the name classifiers and indexing query facts by source position in the lowerer it is 1.1 s, 2.4 s and 5.4 s at n=2,000, 4,000 and 8,000 (TODO-5478). The reproducer is a `main` of lines like
`assign(total, plus(multiply(total, 3i32), K))`, with an `if` on every seventh line.

Resolved (TODO-5477): the backends used to disagree on i32 overflow. For `total = 2147483647i32; total = total + 1i32`
the VM kept the 64-bit slot (so `total < 0` was false), `exe` wrapped in `int32_t`, and `native` printed 2147483648.
The language rule is now wrapping i32: lowering emits the pure `SextI32` opcode (schema v27) after user-level i32
add/sub/mul/div/negate (and increment, decrement, abs, pow), and every backend implements it, so the reproducer prints
`-2147483648 1 0 1 -2147483648 -2147483648` on vm, native, optexe and exe (program-matrix cases `i32_wrap_*`).

Suggested order: Phase 0.1-0.3 (flags, pass manager, dumps), Phase 1.1
(shared CFG), then this emitter (done), then the matrix runner (0.5) to run the corpus through it. Phase 3
(register form, native code generator) is the next consumer of the middle end.

## 10. Decisions taken

- Default level is `-O2` for `--emit=native`, `--emit=vm`, `--emit=optexe`/`optcpp` and
  `primevm`, `-O0` under any debug session or trace, for dumps and for the other emit kinds
  (2026-10-03, implemented).
- The `optexe` C++ emitter is the first consumer of the middle end
  (section 9); it works on the stack form with the shared CFG, no register form (2026-10-03).
- This work was developed on the branch `claude/native-instruction-optimization-rc4qu6`
  (2026-10-03, maintainer decision). Later the same day the maintainer asked for the branch to be
  rebased onto `master` and pushed there, and since then finished work lands on `master` directly
  per `AGENTS.md`; the branch only mirrors `master`.

## 11. Open decisions for the maintainer

1. Scope of Phase 3: register-form codegen (proposed) versus peepholes on the
   template emitter only (cheaper, caps out around 2x).
2. Whether a VM JIT that reuses the Phase 3 codegen is wanted later.
3. Whether a PSIR v27 (local extents, shift/bitwise opcodes) is acceptable, or
   the schema must stay frozen for this work.
4. Access to a macOS arm64 machine for Phase 3.6 validation.
