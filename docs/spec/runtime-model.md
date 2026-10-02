# Runtime Stack Model

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note**.

## Runtime Stack Model
- **Frames:** VM and native execution both support dynamic call frames for `Call`/`CallVoid` when callable IR opcodes
  are present. Lowering still inlines source-level definition calls, so entry-lowered source programs typically use one
  frame unless callable IR is emitted directly. Locals live in fixed 16-byte slots; `location(...)` yields a byte offset
  into this slot space and `dereference` uses `LoadIndirect`/`StoreIndirect`.
- **Deterministic evaluation:** arguments evaluate left-to-right; boolean `and`/`or` short-circuit; `return(value)`
  unwinds the current definition. In value blocks, `return(value)` exits the block and yields its value. Implicit
  `return(void)` fires if a definition body reaches the end.
- **Indirect alignment:** indirect addresses must be 16-byte aligned; misaligned dereferences are VM runtime errors.
- **Transform boundaries:** text/semantic rewrites decide where bodies inline; IR lowering preserves left-to-right
  argument evaluation inside the active frame.
- **Resource handles:** handles live inside frame slots as opaque values; lifetimes follow lexical scope.
- **Tail execution:** lowering marks tail-position calls in metadata; backends may reuse frames when the tail flag is
  set. VM/native currently ignore the hint; GPU backends may require tail-safe forms for determinism.
- **Effect annotations:** purity by default; explicit `[effects(...)]` opt-ins. Effects are validated during lowering;
  runtime enforcement is limited to builtin checks.

### Execution Metadata
- **Scheduling scope:** queue/thread selection stays host-driven; there are no stack- or runner-specific annotations
  yet, so executions inherit the embedding runtime’s default placement. Lowering writes `scheduling_scope = 0` in PSIR
  metadata until explicit scheduling transforms exist.
- **Effects & capabilities:** lowering records `effect_mask` and `capability_mask` in PSIR metadata. If no
  `capabilities` transform is supplied, `capability_mask` mirrors the active effects for the definition; explicit
  capability lists narrow the mask.
- **Instrumentation:** `instrumentation_flags` are reserved for tracing/source-map hooks. Bit 0 (`tail_execution`) is
  set when the final statement is `return(def_call(...))` or (for `void` definitions) a final `def_call(...)`. Backends
  may treat this as a tail-call hint; no backend currently requires it.

### Native Allocator & Scheduler (IR Optimization Path)
- **Pipeline shape:** stack-form IR is lowered to block-local virtual registers, then processed in deterministic order:
  liveness intervals, linear-scan allocation, spill/reload insertion, block-local scheduling, and verifier checks.
- **Linear-scan allocator design:** each virtual register interval uses one `startPosition`/`endPosition` pair derived
  from liveness ranges; intervals are processed sorted by `(start, end, virtual_register)`.
- **Register assignment policy:** free physical registers are kept sorted ascending, so allocation picks the lowest
  available register first.
- **Spill heuristic:** only `SpillFarthestEnd` is supported. When pressure exceeds available physical registers, the
  active interval with the farthest end point is the spill candidate; ties spill the higher virtual register ID first.
- **Spill insertion contract:** spill/reload insertion consumes allocator assignments and emits deterministic edge-safe
  reloads/spills; verifier passes reject assignment/edge mismatches before lifting back to stack-form IR.
- **Scheduler design:** scheduling runs per basic block and preserves control-flow edge metadata. A dependency DAG is
  built from def-use edges plus barrier ordering constraints.
- **Scheduler barriers:** control flow, calls, returns, prints, file I/O, `StoreLocal`, and indirect loads/stores are
  treated as barriers to preserve observable behavior and memory ordering.
- **Scheduler heuristic:** ready instructions are chosen by highest latency score first (division > multiply >
  add/sub/compare > default), with a +2 penalty for each spilled use/def register; ties pick lower original instruction
  index.
- **Verification contract:** the schedule/allocation verifier checks one-to-one instruction mapping, dependency
  ordering, barrier ordering, register range validity, and branch-edge compatibility.
- **Debug dump format:** allocator/scheduler outcomes are tracked through the native emitter instrumentation dump format
  `native_emitter_debug_v1`, emitted by `formatNativeEmitterDebugDump(...)`.
  - `[instrumentation]` reports totals and sorted per-function counters (`function[i].*`), ordered by `(functionIndex,
    functionName)`.
  - `[optimization]` reports pre/post totals and `applied` (`1` when an optimization comparison was recorded, else `0`).
- **Debug dump example (optimization applied):**
  ```text
  native_emitter_debug_v1
  [instrumentation]
  total_instruction_count=4
  total_value_stack_push_count=3
  total_value_stack_pop_count=2
  total_spill_count=1
  total_reload_count=1
  function_count=1
  function[0].index=0
  function[0].name=/main
  function[0].instruction_total=4
  function[0].value_stack_push_count=3
  function[0].value_stack_pop_count=2
  function[0].spill_count=1
  function[0].reload_count=1
  [optimization]
  applied=1
  instruction_total_before=4
  instruction_total_after=3
  value_stack_push_count_before=3
  value_stack_push_count_after=2
  value_stack_pop_count_before=2
  value_stack_pop_count_after=2
  spill_count_before=1
  spill_count_after=0
  reload_count_before=1
  reload_count_after=0
  ```
- **Debug dump example (defaults):**
  ```text
  native_emitter_debug_v1
  [instrumentation]
  total_instruction_count=2
  total_value_stack_push_count=1
  total_value_stack_pop_count=1
  total_spill_count=1
  total_reload_count=1
  function_count=1
  function[0].index=0
  function[0].name=/main
  function[0].instruction_total=2
  function[0].value_stack_push_count=1
  function[0].value_stack_pop_count=1
  function[0].spill_count=1
  function[0].reload_count=1
  [optimization]
  applied=0
  instruction_total_before=0
  instruction_total_after=0
  value_stack_push_count_before=0
  value_stack_push_count_after=0
  value_stack_pop_count_before=0
  value_stack_pop_count_after=0
  spill_count_before=0
  spill_count_after=0
  reload_count_before=0
  reload_count_after=0
  ```
