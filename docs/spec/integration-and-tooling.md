# Integration Points and Tooling

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note / roadmap**.

## Integration Points
- **Build system:** extend CMake/tooling to run `primec`, track dependency graphs, and support incremental builds.
- **Testing:** unit/looped regression suites verify backend parity (C++, VM, GLSL).
- **Diagnostics:** metrics/logs land under `diagnostics/PrimeStruct/*`. Effect annotations drive error messaging.

### Diagnostics & Tooling Roadmap
- **Phase 0 (now):** standardize diagnostic records (`severity`, `code`, `message`, `notes`, source span), include
  canonical call paths, and ensure CLI output is deterministic. Establish a stable JSON diagnostic export
  (`--emit-diagnostics`) for tooling/tests.
- **Phase 1 (source maps):** attach token spans to AST, keep span provenance through text/semantic transforms, and emit
  IR-to-source maps for VM/native/GLSL. Require every diagnostic to carry at least one source span.
- **Phase 2 (incremental):** content-addressed caches for AST/IR, dependency graph tracking for imports, and
  invalidation rules per transform phase. Add `--watch` to reuse caches and stream diagnostics.
- **Phase 3 (IDE/LSP):** go-to-definition, completion, and signature help using the same symbol tables as the compiler.
  Provide diagnostics in LSP format plus an editor adapter.
- **Phase 4 (runtime):** VM/native stack traces mapped via source maps, crash reports emitted with IR/AST hashes, and
  opt-in runtime tracing for effect/capability usage.

### VM Debug Event Ordering
- `VmDebugSession` hook callbacks are emitted in one total order with a monotonically increasing `sequence` value that
  starts at `0` on each `start(...)`.
- Per instruction, hooks fire in this order: `beforeInstruction` -> zero or more in-instruction call events (`callPush`
  / `callPop`) -> `afterInstruction`.
- Faulted instructions emit `beforeInstruction` and then `fault`; they do not emit `afterInstruction`.
- For identical IR + entry arguments + control flow (`step`/`continue` decisions), the emitted hook event stream is
  deterministic and replayable.
- `primevm --debug-json` emits one JSON object per line (`version`, `event`, and event-specific fields). Hook records
  include `sequence` and `snapshot`; stop records include `reason` and a terminal snapshot.
- `primevm --debug-json-snapshots=stop|all` adds `snapshot_payload` on demand; `stop` limits payloads to `stop` events,
  while `all` includes payloads on session/hook/stop events.
- `primevm --debug-trace <path>` records deterministic VM events to a file (NDJSON) including hook `sequence` ordering,
  snapshots, and snapshot payloads; repeated runs over identical inputs must produce byte-identical trace logs.
- `primevm --debug-replay <trace>` restores a deterministic checkpoint from a captured trace and emits a single
  `replay_checkpoint` record (`target_sequence`, resolved `checkpoint_sequence`, event/reason, snapshot,
  snapshot_payload).
- `primevm --debug-replay-sequence <n>` selects the latest checkpoint with `sequence <= n` for time-travel; without an
  explicit sequence, replay uses the terminal checkpoint and mirrors traced exit codes for terminal `stop`/`Exit`
  checkpoints.

### VM IR Breakpoints
- `VmDebugSession` supports IR-level breakpoints keyed by `(functionIndex, instructionPointer)` through `addBreakpoint`,
  `removeBreakpoint`, and `clearBreakpoints`.
- `resolveSourceBreakpoints(...)` maps source locations (`line`, optional `column`, optional source-unit/file identity)
  to executable IR breakpoint locations via canonical source-map entries; line-only requests reject ambiguous
  multi-column matches with explicit diagnostics, and source-unit filtering disambiguates identical positions in
  primary/imported files.
- `VmDebugSession::addSourceBreakpoint(line, column, resolvedCount, ..., sourceUnit)` resolves and installs all matching
  IR breakpoints in one call; `resolvedCount` reports how many executable locations were mapped.
- Breakpoints are checked in `continueExecution` before instruction execution and stop with reason `Breakpoint`.
- Continuing from a breakpoint resumes past that same location once (prevents immediate repeat-stops at the same
  `(functionIndex, instructionPointer)`), then normal breakpoint checks resume.

### VM Fault Stack Traces
- `VmDebugSession` fault diagnostics append a deterministic mapped stack trace when source-map metadata is available.
- Stack frames are listed from faulting frame to caller frames and include function path, instruction pointer,
  instruction debug ID, and mapped source span (`source_unit:line:column` when available, otherwise `line:column`) with
  provenance.
- Caller-frame mapping reports call-site instructions (the `Call`/`CallVoid` slot) rather than the post-call instruction
  pointer.

### VM Debug Adapter (MVP)
- `VmDebugAdapter` (`primec/VmDebugAdapter.h`) translates VM debug-session behavior into debugger-facing primitives:
  - execution control: `launch`, `continueExecution`, `step`, `pause`
  - debugger queries: `threads`, `stackTrace`, `scopes`, `variables`
  - breakpoints: `setInstructionBreakpoints`, `setSourceBreakpoints`
- Stack-frame responses use source-map metadata when available (`sourceUnit`, `line`, `column`, provenance) and keep the
  same call-site mapping behavior used by VM fault stack traces.
- Adapter calls append deterministic transcript lines through `transcript()` to support protocol transcript tests and
  replay checks.
- Snapshot payloads now carry per-frame locals (`frame_locals`) so `scopes/variables` return concrete local values for
  non-top frames as well as the active frame.
- `primevm --debug-dap` adds stdio JSON-RPC (`Content-Length`) framing and a request router for `initialize`, `launch`,
  `setBreakpoints`, `setInstructionBreakpoints`, `threads`, `stackTrace`, `scopes`, `variables`, `continue`, `next`,
  `pause`, and `disconnect`.

### Semantics Parallelism (Investigation)
We plan to parallelize semantic validation across root functions using a
deterministic diagnostics pipeline. See
`docs/Semantics_Multithread_Design.md` for the canonical phase boundaries,
ownership model, and rollout plan.

### IDE/LSP Integration Plan
Goals:
- Deliver a first-class IDE/LSP experience powered by the compiler front end.
- Keep diagnostics and symbol resolution deterministic across CLI and LSP.
- Preserve transform provenance so hovers and completions map to source.

Scope (MVP):
- `primec-lsp` process that reuses the parser, transform pipeline, and semantic resolver.
- Diagnostics wired through the JSON export (`--emit-diagnostics`) for identical output.
- Core language intelligence: hover, go-to-definition, signature help, and completion.
- Workspace symbol search driven by import dependency graphs.

Architecture:
- Maintain a long-lived compiler service with per-file token/AST caches.
- Track a dependency graph keyed by canonical paths; invalidate in topological order.
- Store per-definition symbol tables and a reverse reference index for navigation.
- Use source maps (Phase 1) to map transformed diagnostics back to surface text.

Milestones:
- M1: LSP diagnostics and document symbols (requires Phase 0).
- M2: go-to-definition/hover/signature help with stable symbol tables.
- M3: completion and workspace symbols with incremental cache invalidation (Phase 2).
- M4: rename/code actions backed by reference indexing and stable IDs.

Out of scope (initial):
- Full editor integration packaging (for example VS Code extension manifests and launch profile templates).
- GPU-specific editor features beyond diagnostics and symbol navigation.
