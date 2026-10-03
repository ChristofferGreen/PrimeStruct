# Optimizing Native Backend and VM: Plan

Status: proposal (2026-10-03). Nothing in this document is implemented yet.
It records what the direct native backend (`--emit=native`) and the PrimeScript
VM (`--emit=vm`, `primevm`) do today, why they are slow, and a phased plan to
turn the shared IR path into an optimizing compiler with GCC-style `-O` levels
and individually selectable passes. The C++-emitting path (`--emit=exe`,
`--emit=cpp`) is out of scope except as the correctness and speed baseline.

## 1. Where the code is today

### 1.1 The IR the two backends share

- PSIR (`include/primec/ir/Ir.h`, schema v26) is a linear stack machine: one
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

See section 7; filled in from this machine (Linux x86_64, 4 cores) after a
release build.

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
                stack IR (PSIR v26)  ----validate----
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
  after loading. `.psir` files produced at any `-O` level remain v26.
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
  code, and (for `--debug-trace`) the trace where the level allows it.
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

Each numbered item is intended to become one `TODO-XXXX` leaf in
`docs/todo.md` with its own scope, acceptance and stop rule when the plan is
approved. Order within a phase is a dependency order unless stated.

### Phase 0: measurement and control surface (no behaviour change)

0.1 Add `OptimizationOptions` to `Options`: level, explicit pass enables and
    disables, `verifyEachPass`, `report`. CLI: `-O0`..`-O3`, `--opt-pass`,
    `--no-opt-pass`, `--opt-list`, `--opt-report`, `--opt-verify-each`, for
    `primec` and `primevm`. Default stays `-O0` until Phase 5.
0.2 Pass manager and `irOptimizationPassManifest()`; `optimizeIrModule()`
    inserted into `prepareIrModule` after validation, re-validating after
    the pipeline (and after every pass under `--opt-verify-each`). Add a
    stack-balance check to `validateIrModule` so broken passes fail early.
0.3 `--dump-stage=ir-lowered` and `--dump-stage=ir-optimized` printing the
    real `IrModule` (text, deterministic), and an `--opt-report` listing per
    pass: instructions before/after, time. Fix the `--help` default text.
0.4 Benchmark harness: time `--emit=native` on Linux x86_64 too, add a
    `primestruct_vm` entry, and an `-O` matrix (`BENCH_OPT_LEVELS`). Add
    baseline entries for vm and native at `-O0` from this machine.
0.5 Differential harness: a test helper that compiles one source at every
    level for `vm` and `native`, plus `exe` as a second oracle, and compares
    stdout and exit code. Run the existing compile-run corpus through it
    (sharded; one level set per shard). Add a deterministic random stack-IR
    generator (seeded) whose programs run through the `-O0` VM and the
    optimized VM.

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
2.2 `copy-prop`, `cse`, algebraic simplification on the register form.
2.3 Loop analysis and `licm`; `tail-self-loop`.
2.4 General `inline` with budgets; interaction with the lowerer's
    two-call-sites heuristic measured and tuned.
2.5 Stackification: lower the register form back to PSIR for vm, cpp, wasm,
    glsl (expression-tree scheduling; multi-use values become locals).
    Round-trip property tests: lower -> optimize(no passes) -> stackify is a
    semantic no-op on the whole corpus.
2.6 Differential corpus green at `-O2` on vm; benchmark report for vm.

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

## 6. Constraints, risks, and how each is handled

- **Exact semantics.** Every pure opcode's semantics is defined once (the
  VM's implementation) and the folder calls the same functions. Float folding
  uses the host's IEEE operations exactly as the VM does; `NaN` comparisons
  follow the VM. Differential tests at every level are the gate.
- **`AddressOfLocal` aliasing.** v1 rule: if a function contains any
  `AddressOfLocal`, every local with index >= the smallest addressed index is
  pinned (addresses only grow via `+ k*IrSlotBytes`). If a pointer value can
  flow into `StoreIndirect`/`LoadIndirect` from outside the function (heap
  addresses are tagged, frame addresses are not), treat all locals as pinned.
  Later refinement: the lowerer emits a per-local extent table (struct slot
  count) in a schema bump so only the addressed aggregate is pinned.
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

Filled in from `build-release` on Linux x86_64. Each number is the mean of the
runs noted; see `docs/todo_log.md` for the raw commands when the TODOs are
created.

| Benchmark | C `-O3` | `--emit=exe` (C++ `-O3`) | `--emit=native` (today) | `--emit=vm` (today) |
| --- | --- | --- | --- | --- |
| aggregate (N=5,000,000) | see below | see below | see below | see below |
| json_scan (20,000 scans) | see below | see below | see below | see below |
| json_parse | see below | see below | see below | see below |

Targets for acceptance of the whole programme (to be confirmed against the
measured baseline):

- native `-O2`: within 2x of `--emit=exe` on all three benchmarks; `-O3`
  within 1.5x on aggregate.
- vm `-O2` with the Phase 4 kernel: at least 4x faster than today's VM on
  all three benchmarks.
- Compile time of `--emit=native -O2` for the 100,000-line compile-speed
  source within 1.5x of `-O0`.

## 8. Open decisions for the maintainer

1. Default level after Phase 5: `-O2` for native and vm (proposed), or keep
   `-O0` and opt in.
2. Scope of Phase 3: register-form codegen (proposed) versus peepholes on the
   template emitter only (cheaper, caps out around 2x).
3. Whether a VM JIT that reuses the Phase 3 codegen is wanted later.
4. Whether a PSIR v27 (local extents, shift/bitwise opcodes) is acceptable, or
   the schema must stay frozen for this work.
5. Access to a macOS arm64 machine for Phase 3.6 validation.
