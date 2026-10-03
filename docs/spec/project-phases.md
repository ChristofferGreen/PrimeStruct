# Project Phases, Charter, and Risk Log

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **roadmap / history**.

### Language ethos (v1)
- **Simplified and coherent C:** keep the core small, explicit, and close to how the machine behaves when it matters.
- **Sane subset of C++:** keep value types, structs, and explicit control flow, but avoid implicit conversions,
  surprising overload rules, or hidden allocations.
- **Python spirit for ergonomics:** readable defaults and small conveniences (e.g., optional separators, concise
  literals), without sacrificing determinism or making meaning depend on tooling.
- **Ease of understanding first:** prefer features that are easy to explain and reason about, and reject features that
  add power without clarity.
- **Concrete rules:** explicit envelopes, explicit conversions, explicit effects, immutable-by-default bindings, and
  deterministic evaluation order.

## Phase 0 — Scope & Acceptance Gates (must precede implementation)
- **Charter:** capture exactly which language primitives, transforms, and effect rules belong in PrimeStruct, and list
  anything explicitly deferred to later phases.
- **Success criteria:** define measurable gates (parser coverage, IR validation, backend round-trips, sample programs)
- **Ownership map:** assign leads for parser, IR/envelope system, first backend, and test infrastructure, plus
  security/runtime reviewers.
- **Integration plan:** describe how the compiler/test suite slots into the build (targets, CI loops, feature flags,
  artifact publishing).
- **Risk log:** record open questions (borrow checker, capability taxonomy, GPU backend constraints) and
  mitigation/rollback strategies.
- **Exit:** only after this phase is reviewed/approved do parser/IR/backend implementations begin; the conformance suite
  derives from the frozen charter instead of chasing a moving target.

## Project Charter (v1 target)
- **Language core:** envelope syntax (definitions + executions), slash paths, namespaces, imports (source + namespace
  exposure), binding initializers, return annotations, effects annotations, and deterministic canonicalization rules.
- **Transform pipeline:** ordered text transforms, ordered semantic transforms, explicit `text(...)` / `semantic(...)`
  grouping, and auto-deduction for registered transforms.
- **Determinism:** stable diagnostics, fixed evaluation order, stable IR emission, and canonical string normalization
  across backends.
- **IR:** PSIR serialization with versioning, explicit opcode list, and a stable module layout shared by all backends.
- **Backends:** C++ emitter and VM bytecode are required targets; GLSL/SPIR-V is a supported optional target with
  explicit documented constraints.
- **Standard library:** a documented core subset (math + collections + IO primitives) with a per-backend support
  matrix.
- **Tooling:** `primec`, `primevm`, `--dump-stage`, and snapshot-style tests for parser/IR/diagnostics.
- **Change control:** any feature outside this charter requires a docs update plus an explicit acceptance gate.

## Risk Log (Phase 0)
Each risk lists a mitigation and a concrete fallback so the project can keep moving if the
open question is unresolved.

- Risk: Borrow checker deferred in v1.
  Impact: Resource safety rules vary by backend, leading to unsound or inconsistent behavior.
  Mitigation: Document explicit resource rules and keep borrow checks out of v1.
  Fallback: Keep borrow-checking claims removed until a design lands.
- Risk: Capability taxonomy is not finalized.
  Impact: Effects/capabilities drift between docs, diagnostics, and runtime logging.
  Mitigation: Define a minimal capability list (IO + memory + GPU) and lock it for v1.
  Fallback: Treat capabilities as diagnostic-only metadata until taxonomy stabilizes.
- Risk: GPU backend constraints are underspecified.
  Impact: GLSL/SPIR-V output may accept unsupported effects or types.
  Mitigation: Publish an explicit GPU support matrix and reject unsupported effects/types early.
  Fallback: Mark GPU backend as experimental and limit to math-only + POD structs.
- Risk: IR/PSIR versioning slips.
  Impact: Backends diverge on opcode interpretation or serialized layouts.
  Mitigation: Freeze v1 opcode set, add migration notes, and version the serialized header.
  Fallback: Reject unknown IR versions and require recompile in tooling.
- Risk: Struct layout guarantees are ambiguous.
  Impact: ABI/layout mismatches across native/VM/GPU.
  Mitigation: Require layout manifests and add conformance tests per backend.
  Fallback: Document layout as backend-defined and disallow cross-backend struct reuse.
- Risk: Default effect policy changes.
  Impact: Behavior differs between CLI defaults and docs.
  Mitigation: Keep defaults centralized in `Options` and tests covering defaults.
  Fallback: Require explicit `[effects]` in examples and templates.
- Risk: Execution emission is unclear.
  Impact: Executions parse but are ignored by some backends.
  Mitigation: Decide and document whether executions are lowered per backend.
  Fallback: Treat executions as diagnostics-only until a backend implements them.
- Risk: Stdlib surface drifts across backends.
  Impact: Code runs on one backend but fails on another.
  Mitigation: Maintain a per-backend stdlib support table + tests.
  Fallback: Provide a "core subset" and gate all samples to it.

## Deferred Features (not in v1)
- Borrow checker and lifetime enforcement beyond basic effects gating.
- Full capability taxonomy and policy-driven capability enforcement (beyond documented effects).
- Tooling integrations beyond basic CLI workflows.
- Backend support for software numeric envelopes (`integer`/`decimal`/`complex`) and mixed-mode numeric ops.
- Placement transforms (`stack`/`heap`/`buffer`) and placement-driven layout guarantees.
- Recursive struct layouts and cross-module layout stability guarantees.
- JIT, chunk caching, or dynamic recompilation tooling.
- IDE/LSP integration and editor tooling.
- Standard library packaging/version negotiation beyond a single in-tree reference set.
- `tools/PrimeStructc` feature parity with the main compiler and template codegen (PrimeStructc stays a minimal subset;
  template codegen and import version selection are explicitly out of scope for v1).
- Tail-call or tail-execution optimization guarantees across all backends.

## Phase 1 — Minimal Compiler That Emits an Executable
Goal: a tiny end-to-end compiler path that turns a single PrimeStruct source file into a runnable native executable.
This is the smallest vertical slice that proves parsing, IR, a backend, and a host toolchain handoff.

### Acceptance criteria
- A single-file PrimeStruct program with one entry definition compiles to a native executable on macOS (initial target).
- The compiler can:
  - Parse a subset of the Envelope syntax (definitions + executions).
  - Build a canonical AST for that subset.
  - Lower to a minimal IR (calls, literals, return).
  - Emit C++ and invoke a host compiler to produce an executable.
- The produced executable runs and returns a deterministic exit code.

### Example source and expected IR (sketch)
PrimeStruct:
```
[return<i32>]
main() {
  return(42i32)
}
```

Expected IR (shape only):
```
module {
  def main(): i32 {
    return 42
  }
}
```

### Compiler driver behavior
- `primec --emit=cpp input.prime -o hello.cpp`
- `primec --emit=exe input.prime -o hello`
  - Uses the C++ emitter plus the host toolchain (initially `clang++`).
  - Bundles a minimal runtime shim that maps `main` to `int main()`.
- `primec --emit=optcpp input.prime -o hello.cpp` and `primec --emit=optexe input.prime -o hello`
  - Emit structured C++ from the shared IR instead of re-implementing the stack machine: each operand-stack depth and
    local is a C++ variable, each basic block a label, jumps are `goto`, calls are C++ calls. The host compiler does
    register allocation; `-O<n>` also selects its optimization level (default `-O2`, `-ffp-contract=off`).
  - Matches the VM's output, exit code and `VM error:` fault text; `CallHost` (VM host imports) is rejected with a
    diagnostic. `exe`/`cpp` stay unchanged and serve as an independent oracle.
- `primec --emit=native input.prime -o hello`
  - Emits a self-contained macOS/arm64 executable directly (no external linker).
  - Lowers through the portable IR that also feeds the VM/network path.
  - Current subset: fixed-width integer/bool/float literals (`i32`, `i64`, `u64`, `f32`, `f64`), locals + assign, basic
    arithmetic/comparisons (signed/unsigned integers plus floats), boolean ops (`and`/`or`/`not`), explicit conversions
    via `T{value}` for `i32/i64/u64/bool/f32/f64`, abs/sign/min/max/clamp/saturate, `if`, `print`, `print_line`,
    `print_error`, and `print_line_error` for integer/bool or string literals/bindings, and pointer/reference helpers
    (`location`, `dereference`, `Reference`) in a single entry definition.
- `primec --emit=ir input.prime -o module.psir`
  - Emits serialized PSIR bytecode after semantic validation (no execution).
  - Output is written as `.psir` and includes a PSIR header/version tag.
- `primec --emit=glsl input.prime -o module.glsl`
  - Routes through canonical IR into `IrToGlslEmitter`.
  - Runs `IrValidationTarget::Glsl` before emission to reject unsupported GLSL IR shapes.
- `primec --emit=spirv input.prime -o module.spv`
  - Emits SPIR-V by first generating GLSL and invoking `glslangValidator` or `glslc` (compute stage).
  - Runs `IrValidationTarget::Glsl` before GLSL generation.
  - Requires `glslangValidator` or `glslc` on `PATH`.
- `primec --emit=wasm input.prime -o module`
  - Routes through canonical IR into `WasmEmitter`.
  - Runs `IrValidationTarget::Wasm` or `IrValidationTarget::WasmBrowser` before emission based on `--wasm-profile`
    (`wasi` default, `browser` optional).
  - Rejects unsupported opcode/effect/capability combinations during `ir-validate` before backend emission.
  - When `-o` is omitted, output defaults to `<input-stem>.wasm`.
- `primevm input.prime --entry /main -- <args>`
  - Runs the source via the PrimeStruct VM (equivalent to `primec --emit=vm`). `--entry` defaults to `/main` if omitted.
  - `--debug-json` streams VM debug events as NDJSON to stdout (`session_start`, hook events, and `stop` records with
    snapshots).
  - `--debug-json-snapshots=none|stop|all` adds on-demand `snapshot_payload` fields (`instruction_pointer`,
    `call_stack`, `frame_locals`, `current_frame_locals`, `operand_stack`) to debug-json events.
  - `--debug-trace <path>` writes a deterministic VM event log (NDJSON) to the given file path using the same event
    schema family as debug-json (`session_start`, hook events, and `stop`).
  - `--debug-replay <trace>` replays a trace file generated by `--debug-trace` and emits a single `replay_checkpoint`
    NDJSON event to stdout containing the restored snapshot + snapshot payload.
  - `--debug-replay-sequence <n>` time-travels to the latest checkpoint with `sequence <= n` (without this flag, replay
    restores the terminal checkpoint from the trace).
  - `--debug-dap` runs a stdio DAP endpoint using `Content-Length` framing and routes debugger requests to
    `VmDebugAdapter`.
- `--ir-inline`
  - Enables the optional IR inlining optimization pass after IR validation and before VM/native/IR output.
- Defaults: if `--emit` and `-o` are omitted, `primec input.prime` uses `--emit=native` and writes the output using the
  input filename stem (still under `--out-dir`).
- All generated outputs land in the current directory (configurable by `--out-dir`).

### Wasm backend limits (current)
- `WASM-LIMIT-MEM-ON-DEMAND`: the emitter allocates linear memory only when the lowered IR uses WASI runtime opcodes
  (`PushArgc`, `Print*`, `File*`). Pure compute/control-flow modules emit no memory section.
- `WASM-LIMIT-MEM-SINGLE`: when memory is present, the module uses exactly one linear memory (`memory index 0`) sized
  for runtime scratch space and literal data segments; no additional memories are emitted.
- `WASM-LIMIT-IMPORTS-WASI`: imported host calls are limited to `wasi_snapshot_preview1` and the fixed symbol set
  `{fd_write, fd_read, args_sizes_get, args_get, path_open, fd_close, fd_sync}` selected on demand from IR opcode usage.
- `WASM-LIMIT-PROFILE-BROWSER`: `--wasm-profile=browser` rejects all non-zero effect/capability masks and rejects
  WASI-only opcodes (`PushArgc`, `Print*`, `File*`) during IR validation.
- `WASM-LIMIT-PROFILE-WASI-ALLOWLIST`: `--wasm-profile=wasi` accepts only the explicit Wasm opcode/effect/capability
  allowlist; unsupported IR operations fail with deterministic `ir-validate` diagnostics.
