# VM Design

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **implementation note**.

## VM Design
- **Instruction set:** stack-based ops covering control flow, stack manipulation, memory/pointer access, IO, and
  explicit conversions. No implicit conversions; opcodes mirror the canonical language surface.
- **PSIR opcode set (v20, VM/native):** `PushI32`, `PushI64`, `PushF32`, `PushF64`, `PushArgc`, `LoadLocal`,
  `StoreLocal`,
  `AddressOfLocal`, `LoadIndirect`, `StoreIndirect`, `Dup`, `Pop`, `AddI32`, `SubI32`, `MulI32`, `DivI32`, `NegI32`,
  `AddI64`, `SubI64`, `MulI64`, `DivI64`, `DivU64`, `NegI64`, `AddF32`, `SubF32`, `MulF32`, `DivF32`, `NegF32`,
  `AddF64`, `SubF64`, `MulF64`, `DivF64`, `NegF64`, `CmpEqI32`, `CmpNeI32`, `CmpLtI32`, `CmpLeI32`, `CmpGtI32`,
  `CmpGeI32`, `CmpEqI64`, `CmpNeI64`, `CmpLtI64`, `CmpLeI64`, `CmpGtI64`, `CmpGeI64`, `CmpLtU64`, `CmpLeU64`,
  `CmpGtU64`, `CmpGeU64`, `CmpEqF32`, `CmpNeF32`, `CmpLtF32`, `CmpLeF32`, `CmpGtF32`, `CmpGeF32`, `CmpEqF64`,
  `CmpNeF64`, `CmpLtF64`, `CmpLeF64`, `CmpGtF64`, `CmpGeF64`, `ConvertI32ToF32`, `ConvertI32ToF64`,
  `ConvertI64ToF32`, `ConvertI64ToF64`, `ConvertU64ToF32`, `ConvertU64ToF64`, `ConvertF32ToI32`, `ConvertF32ToI64`,
  `ConvertF32ToU64`, `ConvertF64ToI32`, `ConvertF64ToI64`, `ConvertF64ToU64`, `ConvertF32ToF64`, `ConvertF64ToF32`,
  `JumpIfZero`, `Jump`, `ReturnVoid`, `ReturnI32`, `ReturnI64`, `ReturnF32`, `ReturnF64`, `PrintI32`, `PrintI64`,
  `PrintU64`, `PrintString`, `PrintArgv`, `PrintArgvUnsafe`, `LoadStringByte`, `FileOpenRead`, `FileOpenWrite`,
  `FileOpenAppend`, `FileReadByte`, `FileClose`, `FileFlush`, `FileWriteI32`, `FileWriteI64`, `FileWriteU64`,
  `FileWriteString`, `FileWriteByte`, `FileWriteNewline`, `PrintStringDynamic`, `Call`, `CallVoid`, `HeapAlloc`,
  `HeapFree`, `HeapRealloc`.
- **Call-opcode status:** `Call` and `CallVoid` are serialized/validated and execute in both VM and native backends with
  frame/call-stack semantics. Lowering inlines small source-level definitions and emits call opcodes for the rest
  (recursive and larger functions), including recursion up to the VM's call-depth limit.
  The arguments written at a call are lowered before the inlined callee starts, so a call to the same definition
  or template instantiation inside one of them (`twice(twice(x))`, `some<String>(f(some<String>(g())))`) is not
  recursion; a definition inlined into its own body is still rejected (`does not support recursive calls`).
  Only a definition with a real-call shape recurses: its parameters are scalars, structs outside the collections, a
  builtin `vector<T>` taken by `mut`, read only, `copy` or `move`, or a `mut`, `Reference` or `Pointer` of those (a
  `Reference<vector<T>>` too). A vector parameter whose element the lowerer cannot copy or destroy keeps the definition inline,
  so its recursion is rejected with the parameter named.
  A lifecycle helper (`Create`, `Destroy`, `Copy`, `Move`) is expanded inline where a value is made, copied or
  destroyed; when its expansion reaches the same helper again (a `Function` box whose closure holds a `Function` of
  its own type) the second reach is a real call of the helper, lowered once as a function whose receiver and
  `Reference<Self>` argument are addresses. Only such a reentry reserves a lifecycle helper; once reserved, its
  later expansions are calls too, and every other lifecycle helper stays inline.
- **GLSL note:** GLSL/SPIR-V emission routes through canonical IR (`glsl-ir`/`spirv-ir`) and `IrValidationTarget::Glsl`;
  these modes emit backend output directly without requiring PSIR serialization.
- **PSIR versioning:** current portable IR is PSIR v40 (v24 to v40 are listed in `include/primec/ir/Ir.h`'s opcode
  comments and the finished TODO log). v40 appends `FmaF32` and `FmaF64` (see Fused multiply-add). Migration: the
  module layout is unchanged, so a v33 to v39 module still loads (it cannot hold the opcodes) and a writer always
  writes v40; the loader refuses an opcode newer than the module's version. v39 marks string literal pushes, as the
  string literal paragraph below describes. v38 (TODO-5922, the IR link) appends one section after the host imports: a
  count and, for each function that is an import (`IrFunction::isImport`: a name and a parameter count, no
  instructions, the name of the library that defines it), its index and library name; and an entry index of
  `0xFFFFFFFF` means no entry point (a library has none). Migration: a v33 to v37 module has no such section, so the
  loader reads it as a module with every function defined and an entry index in range, and a writer always writes
  v38, so a loaded older module is written back with an empty import section; the opcodes are unchanged. A module
  that still has an import fails validation (`IR function <name> is an import of library <lib> that was not
  linked`). v37 appends `ConditionWait` and `ConditionNotify` (see Conditions) and the
  effect mask bit of `channel`. Migration: the module layout is unchanged, so a v33 to v36 module still loads and
  is written back as v37; the loader refuses an opcode newer than the module's version, and a v37 module does not
  load in an older build. v36 appends `MutexLock` and `MutexUnlock` (see Mutexes) and the effect mask bit of
  `lock`. v35 appends the five atomic opcodes (see Atomics). v34 appends `TaskSpawn` and
  `TaskWait` (see Tasks) and is the first version
  that also loads the previous one: a v33 module has the same layout and cannot hold the task opcodes, which the
  loader refuses in it; v33 appends `Trap` and numbers local addresses across all live frames (see
  Frames & stack); v32 appends the `bytes` host value kind (`Pointer<u8>` host parameters); v31 retires run-time string indices (a host function can no longer return `string`; same module layout); v30 appends the `text` host value kind; v29 appends the `u8` host value kind; v28 adds `HeapAllocBytes`, `LoadU8`, `StoreU8` and `ZextU8`). Before that, PSIR v23 (adds a per-function `parameter_count` field on top of v22’s
  source-unit/file identity in source-map metadata, v21’s `HeapRealloc`, v20’s `FileReadByte` and
  `HeapFree`, v19’s per-instruction source-map metadata keyed by debug ID, v18’s instruction debug IDs, v17’s local
  debug slots, v16’s function-call opcodes `Call`/`CallVoid`, v15’s execution metadata, v14’s float return opcodes,
  v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`,
  `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** VM/native execution starts at the entry frame and pushes/pops frames for `Call`/`CallVoid`; each
  frame stores locals in 16-byte slots while the operand stack stores raw `u64` values interpreted by opcode (ints,
  floats as bits, and indices). A local address (`AddressOfLocal`) is `(base + index + 1) * 16`, where a frame's `base` is
  its caller's base plus the caller's local count (the entry frame's base is 0): the live frames' locals form one
  numbering that starts at the second slot, so address 0 (a null reference or pointer) names no local, so an address of a caller's local names the same slot in every callee, which reads and writes it through
  `LoadIndirect`/`StoreIndirect`. An address below the first slot (0) or beyond the live frames faults (`invalid indirect address in IR`). The simple
  C++ emitters, Wasm and GLSL keep their own local numbering from 0 and do not fault on address 0.
  Indirect addresses must be 16-byte aligned. The step kernel resolves a local address against the current frame first
  and then the frames below it; the fast kernel and the optimized C++ emitter keep the same numbering (the fast kernel's
  locals arena is laid out that way, and generated C++ registers each addressable frame). The arm64 native backend
  uses machine addresses: a local's address is its place in its frame on the machine stack, and a caller's frame stays
  in place while its callees run, so it too names the same slot in every frame. The x86_64 native backend and the Linux
  JIT still resolve local addresses against the current frame (TODO-5752); the JIT refuses a function that takes a
  local address and calls.
- **Tasks (PSIR v34):** `TaskSpawn f` pops function `f`'s parameters, in order, and starts a task: a call stack of
  its own whose operand stack starts with them, which `f`'s first instructions store as its parameters. It pushes the
  task handle (tagged bits 48-62, a generation in bits 32-47, the task's slot in bits 0-31; never a heap or local
  address). `TaskWait` pops a handle, waits until that task has ended and pushes its result (0 when `f` returns
  nothing); the handle is consumed, so a second wait faults (`task handle already waited`), as does a value that is
  not a handle (`invalid task handle in IR`). Each call stack numbers its own locals from 0, so a local address does
  not cross to another task; all tasks share the heap. The VM runs every task on its one OS thread
  (`VmTaskScheduler` in `src/runtime/VmTaskScheduler.cpp`) and switches only at a spawn, a wait and a task's end.
  The default schedule is eager: a spawn runs the new task until it ends or blocks, then its spawner continues, so a
  program prints exactly what it prints with each spawned call run inline; a blocked task yields to the
  lowest-numbered runnable task (tasks are numbered in spawn order, the main task is 0). `--vm-schedule-seed <n>`
  (`primec --emit=vm`, `primevm`) or `PRIMESTRUCT_VM_SCHEDULE_SEED` picks the next runnable task at every switch point
  with a splitmix64 generator seeded by `n`, so the same seed gives the same run; tests run several seeds and expect
  the same output to show a program is free of races. When no task can run the VM faults with `deadlock: every task
  is waiting` (exit code 3); the main task's return ends the program, and a `Trap` in any task ends it with its code.
  The vm and optimized C++ targets and serialized bytecode accept the task opcodes; validation also rejects a
  `TaskWait` whose operand is not a `TaskSpawn` result on every path (through locals and `Dup`). A module with task opcodes runs under that
  scheduler with every task on the flat loop (Execution kernels), the native JIT declines it, and a debug session runs them on the same
  scheduler one instruction at a time (below). Lowering for the vm target, serialized bytecode and the
  optimized C++ emitter turns
  `[Task<T>] t{[spawn] f(args)}` into the arguments, evaluated in the spawner, and `TaskSpawn f` stored in `t`, when
  every parameter of `f` is a plain scalar and its result a scalar or void (f becomes a real call even when it is a
  tiny leaf); every read of `t` (`wait(t)`, a name in a multi-wait tuple) is `LoadLocal t; TaskWait`.
  A parameter or result that is a `String`, another struct outside the collections, or a closure (a `Function`,
  borrowed or `copy`/`move`) crosses in one heap block per spawn (`LowerSetupStageState::TaskSpawnSite`), since a
  local address does not cross: the spawner binds each struct argument in a hidden local of its own scope (a copy
  of a named value, a temporary taken over), allocates the block (`HeapAlloc`: the result's slots, then each struct
  argument's slots, which alias the hidden local), and passes the block addresses as the task's parameters, the
  block's start last as the result address of the address ABI. While the task may run, the drop flag of a lent
  argument is off; an owned (`copy`, `move`) closure is destroyed by the task. With a struct result `t` is a struct
  local of the spawner's frame whose drop flag starts off, and a read of `t` is `TaskWait`, a copy of the result
  slots from the block into `t`, the drop flags of `t` and of the lent arguments turned on, and `HeapFree` of the
  block; `wait(t)` then reads that place, so a binding or call argument copies from it and `t` destroys its value
  when its scope ends. What the task allocated for the result (a `String`'s bytes) is freed by the spawner; on
  `optexe` that free is queued to the task's arena. A builtin `vector<T>` is a pointer to a 5-slot record of the frame
  that made it (the buffer is on the heap), and such a record cannot cross to another call stack: the spawner binds a
  vector argument in a hidden `mut` local of its scope (a copy of the name or place, or a fresh vector), copies the
  record into the block and passes the block address; the hidden local's drop flag is off while the task may read the
  vector (lent), and for a `copy` or `move` parameter for good, since the task's body destroys it. A vector result
  is returned the same way: the body copies the record of the returned local into the result block and clears the
  local's drop flag, and the wait moves the record into a frame record of the waiter, whose binding owns the vector
  (`wait(t)` is a fresh vector). A `copy` or `move` parameter of a scalar is the task's own slot
  (the spawner evaluates the argument), and one of a struct is the block's copy, which the task destroys. The stdlib `Vector` and `Map` are
  structs, so they cross like any struct: the spawner binds the argument in a hidden struct local (its `Copy` helper
  copies the buffer), copies the slots into the block, and the task owns the copy when the parameter is `copy` or
  `move` (a `copy mut` one is its own value, not a reference to a place of the spawner), and a result is moved to the
  waiter as for a struct. The builtin `map<K, V>` is the stdlib storage struct declared with `key_value_type` (two `Vector` fields whose
  `Copy` and `Destroy` helpers own the buffers), so a map parameter (borrowed, `copy` or `move`) and a map result cross
  the same way. A sum crosses like a struct too, with one owner decided: the hidden local the spawner binds is a
  deep copy of a named sum (`emitActiveSumPayloadCopyFromSumPtrs`), the sum `move(name)` hands over, or a fresh value;
  the spawner's flag for it is off while the task runs (a borrowed parameter, restored at the wait) or for good (a
  `copy` or `move` parameter, whose body destroys the active payload through a drop entry with the sum definition,
  `registerRealCallOwnedParameters`), and a sum result is moved to the waiter as for a struct (TODO-5847). Any other
  spawned call (a `mut` parameter, a `string`, a vector result that is not a `mut` local the task built, a sum
  argument that is not a name, `move` of a name or a fresh value, a callee with `on_error`, `require` or `try`, a
  method) runs inline, which is a legal schedule, and
  `--opt-report` says so: `lowering: spawn of /f runs inline: <reason>`.
  Every other backend runs every spawned call inline; a `Task<void>` spawn there is the call as a statement.
- **Atomics (PSIR v35):** five opcodes work on one slot through its address, all sequentially consistent. The
  immediate is the width, 32 or 64 (anything else is invalid IR); the address is pushed first.

  | Opcode | Pops | Pushes |
  | --- | --- | --- |
  | `AtomicLoad` | address | the value |
  | `AtomicStore` | address, value | nothing |
  | `AtomicAdd` | address, delta | the value before the addition, which wraps |
  | `AtomicExchange` | address, value | the value before the store |
  | `AtomicCompareExchange` | address, expected, desired | 1 when the slot held `expected` and now holds `desired`, else 0 |

  The address is a slot, or an 8-byte aligned address of a byte region whose 8 bytes lie in one live region (the owner
  count inside a `SharedString`'s text block): the cell is those 8 bytes as a native-endian 64-bit integer. An
  unaligned byte address faults with `unaligned atomic address in IR` and one whose 8 bytes are not in a live region
  with `invalid atomic address in IR`, on the VM and in `optexe`. Every byte region starts at a multiple of 8 and
  occupies a multiple of 8 bytes of the heap (it is exactly as long as it was asked to be, and the rest is not
  addressable).

  A 32-bit operation works on the low half of the slot's value and leaves the slot sign-extended, like every `i32`. Under `--vm-threads`
  they are real sequentially consistent atomics across the task threads (Tasks on threads, below).
  The source of these opcodes is `/std/sync` (docs/spec/stdlib-reference.md, Shared state): its `[host]`
  definitions named `/std/sync/__psatomic_*` are not host imports; the lowerer turns a call of one into its opcode,
  so `hits.add(1i64)` on an `Atomic<i64>` is

  ```
  LoadLocal local 6
  LoadLocal local 8
  AtomicAdd i64
  ```

  with the cell's address in local 6. The VM's tasks share one thread, so it reads and writes the slot plainly,
  with the address checks of `LoadIndirect`; under a schedule seed each atomic operation is a switch point, so a
  load and a later store of one task can have another task's operation between them, and without a seed it is
  none. `optexe` uses `std::atomic_ref<uint64_t>` on the slot (the compiler's `__atomic_*` builtins where the
  library has no `atomic_ref`); that runtime is pasted into the program only when the module has an atomic opcode,
  so other programs are unchanged. The arm64 native emitter (TODO-5950) emits LSE instructions on the 8-byte cell:
  `ldar`, `stlr`, `swpal`, `ldaddal` and `casal`, all sequentially consistent; a 32-bit operation computes in 64 bits
  on sign-extended operands (an add through a `casal` loop) so the cell stays sign-extended, and the byte-region
  cell form (the owner count of a `SharedString`) is the same instruction on the aligned address (native has no
  region table, so an unaligned or dead cell is not checked). IR validation accepts the opcodes for the VM,
  serialized IR, the C++ emitters and arm64 native and rejects them for Wasm and GLSL: `atomic operations are not
  supported by the wasm backend` (`wasm-browser`, `glsl`; native on a host that is not arm64 macOS). The effect mask bit
  of `atomic` is `1 << 13`.
- **Mutexes (PSIR v36):** `MutexLock` and `MutexUnlock` each pop the address of one heap slot, the lock, and push
  nothing (immediate 0). The slot holds 0 while the lock is free. `MutexLock` takes a free lock, waits for a held
  one, and faults with `mutex locked again by the task that holds it` when the caller holds it; `MutexUnlock`
  frees it and faults with `mutex unlocked by a task that does not hold it` otherwise. Under
  `--vm-threads` the lock blocks the OS thread and a wait that can never finish is the deadlock fault (Tasks on
  threads, below). Their source is
  `Mutex<T>.locked` of `/std/sync` (docs/spec/stdlib-reference.md, Shared state), through the `[host]`
  definitions `/std/sync/__psmutex_lock` and `__psmutex_unlock`, which the lowerer turns into the opcodes as it
  does for atomics: `locked` is

  ```
  LoadLocal <cell>
  MutexLock
  ... the body, called with a reference to the value ...
  LoadLocal <cell>
  MutexUnlock
  ```

  where the unlock is the `Destroy` of a local that holds the lock, so it runs on every way out of the body.
  On the VM the slot holds the holder's task number plus one. Both opcodes are switch points under a schedule
  seed; a task that finds the lock held is blocked on it and runs again when it is freed, and blocked tasks count
  for `deadlock: every task is waiting`. The flat loop hands a task to the scheduler at each mutex opcode (Tasks). `optexe` stores the holder's thread state address in the slot: taking a free lock and freeing one that
  nobody waits for is one atomic operation on the slot, and a thread that finds the lock held sleeps on a
  condition variable of the program that an unlock signals (`OptCppMutexRuntime` in
  `src/backend/IrToOptCppRuntime.h`, pasted only into a module with mutex opcodes, so other programs are
  unchanged). A thread that sleeps for a lock counts as waiting (Tasks on threads, below), so a wait that can
  never finish is the VM's `deadlock: every task is waiting` there too. arm64 native (TODO-5951) keeps the holder's thread token (the thread pointer, TPIDRRO_EL0 without its low
  bits) in the slot: taking a free lock is one `casal`, freeing one nobody sleeps for a store-release; a contended
  thread sleeps on a program-wide `pthread_cond_t` behind a `pthread_mutex_t` in the data segment, with the live,
  blocked and round counts of `optexe`'s `OptCppWaitRuntime`, so a wait that can never finish is `deadlock: every
  task is waiting` there too (`src/native_emitter/NativeEmitterInternalsArm64Sync.h`; the entry function of a module
  with a lock or condition opcode writes the static initializers of the gate and the condition variable, and its
  tasks count themselves as live and signal when they end). IR validation accepts the opcodes for the VM, serialized
  IR, the C++ emitters and arm64 native and rejects them for Wasm and GLSL: `mutex operations are not supported by
  the wasm backend` (`wasm-browser`, `glsl`; native on a host that is not arm64 macOS). The effect mask bit of `lock` is `1 << 14`.
- **Fused multiply-add (PSIR v40):** `FmaF32` and `FmaF64` pop `c`, then `b`, then `a` and push `a * b + c` rounded
  once, the result of C's `fmaf` and `fma` (`std::fma`); the product is not rounded before the addition, so the result
  can differ from `MulF32` then `AddF32` (and the `F64` forms), which round twice (immediate 0, no fault). The `fma`
  builtin lowers to one of them. They have three operands, so they are outside the one- and two-operand pure opcodes of
  `IrPureSemantics.h`; `evalFmaOpcode` there is their value, which the VM kernels (step and flat loop) and the constant
  folder call, so a folded and a run `fma` agree bit for bit (a NaN result is never folded). Each backend:

  | backend | form | rounding |
  | --- | --- | --- |
  | VM (step kernel, flat loop), constant folding | `std::fma` | once |
  | `optcpp`, `optexe`, `exe` (the C++ emitters) | `std::fma` (the host's fused instruction; the host compiler runs with `-ffp-contract=off`) | once |
  | arm64 native (Mach-O), stack and register-allocated functions | `FMADD`; a function that has the opcode keeps the stack path | once |
  | GLSL | `fma()` (the narrowed f64 path computes it on f32 payloads like the other f64 operations) | once |
  | Wasm (WASI and browser profiles) | WebAssembly has no fused instruction: a call of a helper function the module carries (`FmaF32`: the exact f64 product and a round-to-odd f64 sum, then one demotion; `FmaF64`: a soft-float on the bit patterns with an exact 128-bit product, an aligned add with a sticky bit and one round to nearest even, subnormals, infinities and NaN included) | once (slower than a fused instruction) |
  | x86_64 native (ELF) | the opcode becomes `StoreLocal t; Mul; LoadLocal t; Add` before emission (`expandFusedMultiplyAdd`, `primec/ir/IrFmaExpansion.h`) | twice, until FMA3 `vfmadd231` lands |
  | the Linux x86_64 JIT | not inline and not bridged, so a module with the opcode stays on the interpreter | once |

- **Conditions (PSIR v37):** `ConditionWait` and `ConditionNotify` let a task that holds a lock wait until another
  task has changed what the lock guards. A condition is the address of one heap slot. `ConditionWait` pops the
  address of a lock the task holds (on top) and the address of a condition, frees the lock and leaves the task
  waiting until the next `ConditionNotify` on that condition; it does not take the lock again, the `MutexLock`
  that follows it does. `ConditionNotify` pops the address of a condition and wakes every task that waits on it.
  Both push nothing (immediate 0). A wait by a task that does not hold the lock faults with `condition wait by a
  task that does not hold the lock`. A notify wakes only the tasks that already wait, so a waiter checks what it
  waits for in a loop with the lock held, and the notifier changes it with the lock held. Their source is
  `Channel<T>` and `RwLock<T>` of `/std/sync` (docs/spec/stdlib-reference.md, Shared state), through the `[host]` definitions
  `/std/sync/__pscondition_wait` and `__pscondition_notify`; a wait in `receive` is

  ```
  LoadLocal <condition>
  LoadLocal <cell>
  ConditionWait
  LoadLocal <cell>
  MutexLock
  ```

  The third `[host]` definition of a channel, `/std/sync/__pschannel_send_closed`, is no opcode: the lowerer
  writes the runtime error `send on a closed channel` (a line on `stderr` and exit code 3, as for an array index
  out of bounds). On the VM a waiting task is blocked on the condition's address, and a notify makes the tasks
  blocked on it runnable; the slot's value is not used. A wait and a notify are switch points under a schedule
  seed, and a task that waits counts for `deadlock: every task is waiting`. Under `--vm-threads` a wait sleeps the
  OS thread until a notify that comes after it (Tasks on threads, below). Where one call stack runs (a module
  without tasks) a notify does nothing and a wait is that deadlock. The flat loop hands a task to the scheduler at each
  condition opcode (Tasks). On `optexe` the slot counts the notifies: a wait reads the count and
  frees the lock with the program's wait gate held, then sleeps on the program's condition variable until the
  count has changed, and a notify adds one and signals when a thread sleeps (`OptCppConditionRuntime` in
  `src/backend/IrToOptCppRuntime.h`, pasted only into a module with condition opcodes, so other programs are
  unchanged). A thread that sleeps for a notify counts as waiting (Tasks on threads, below). arm64 native sleeps a condition
  waiter on the same program-wide condition variable (the slot counts the notifies). IR validation accepts the
  opcodes for the VM, serialized IR, the C++ emitters and arm64 native and rejects them for Wasm and GLSL before it
  looks at the atomic and mutex opcodes of the same module: `channel operations are not supported by the wasm
  backend` (`wasm-browser`, `glsl`; native on a host that is not arm64 macOS). The effect mask bit of `channel` is `1 << 15`.
- **Tasks in a debug session (`VmDebugSession`, `--debug-json`, `--debug-dap`):** a program that spawns tasks runs in
  a debug session on the deterministic scheduler above (eager, or the seed of `setScheduleSeed` / `--vm-schedule-seed`),
  one instruction or one scheduler opcode per step, with the same results, output and exit code as a plain run. A debug
  session never uses threads mode. Every task is a thread of the session: the main task is thread 1 and the task
  numbered `n` is thread `n + 1`; `VmDebugSession::threads()` and DAP `threads` list the main task and every task that
  has not ended (a task ends when its call returns, so an eager run lists a spawned task only while it is the one that
  runs or is waiting), named `main` or `task <n>: <function>`. A stop (breakpoint, step, pause or fault) is in the
  task that ran the instruction, `currentThreadId()` and the `threadId` of the DAP `stopped` event name it, and every
  task is stopped with it. `stackTrace` takes a thread id and answers that task's call stack; a frame id and a
  variables reference name their thread (the frame ids of thread 1 are the plain ordinals, as for a program without
  tasks, and thread `t` adds `(t - 1) * 2^20`). A step runs one instruction of the task that runs next, so stepping a
  `TaskSpawn` lands at the first instruction of the spawned task under the eager schedule. Breakpoints are by
  instruction and apply to every task. A program without tasks takes none of this path, and its DAP and `--debug-json`
  output is unchanged. `--debug-json` and `--debug-trace` record each step with the snapshot of the task that ran it;
  replaying such a trace (`--debug-replay`) is not covered.
- **Tasks on threads (the VM, `--vm-threads`):** `primevm --vm-threads` and `primec --emit=vm --vm-threads` (the
  embedder's `Vm::setTaskThreads`) run each `TaskSpawn` on an OS thread of its own instead of under the deterministic
  scheduler; the default stays the deterministic scheduler and threads are opt-in (decided 2026-11-18), so without the
  flag nothing changes, and a program without tasks runs as before (its heap is the first
  arena, with today's addresses). The calling thread is the main task. A spawned task is a detached pthread with an
  8 MiB stack that runs the step kernel on a call stack of its own (`VmThreadedTasks` in
  `src/runtime/VmThreadedTasks.cpp`; the flat loop is not used until TODO-5902) and numbers its locals from 0, as
  every task does. The task table (slots with generations, the handle format of Tasks above) and the flags below sit
  behind one mutex; `TaskWait` consumes the handle, sleeps on one condition variable until the task's function has
  returned and reads the result, with the faults of the deterministic scheduler (`invalid task handle in IR`, `task
  handle already waited`). A thread is started per live task (`too many live tasks` past a million, and
  `cannot start a task thread: <reason>` when the system refuses one), so a program that has thousands of tasks live
  at once is bounded by the operating system. **Heaps:** the VM allocates through `VmHeapArenas`
  (`include/primec/runtime/VmHeapArenas.h`, shared with `optexe`): the main task uses arena 0 and each task thread
  an arena its spawner acquired, found by the arena number in address bits 41 to 46, so a block a task allocates has
  a non-zero arena number and an arena whose task ended is used again. The first spawn moves arena 0's storage into
  its address-space reservation while the main task is the only thread, so every address stays valid. A block a task
  returns (a `String`, a vector, a struct) stays valid after the task ended and is freed by the waiter through the
  owning arena's remote-free queue. **Host calls:** a `print`, a file operation and every embedder host function take
  one global lock, so a line is written by one thread at a time (the output sink needs no lock of its own) and a host
  function never runs twice at once; a host function that blocks therefore blocks every task's host calls. An
  embedder refines this per function (`VmHostBinding::threading`, `Script::setHostThreading`): `Reentrant` runs
  without the lock, so it may be entered by several threads at once and a blocking call blocks only its own task;
  `MainThreadOnly` (the native UI) runs on the thread that started the run and faults with `<name> called off the
  main thread` when a spawned task calls it; the default `Serialized` is the lock above. Without `--vm-threads` the
  flag has no effect. The `__pstext_*` functions the VM answers itself run without the lock, on the calling
  thread's own string heap. **Faults and cancel:** a fault, a `Trap` or the main task's return sets a stop flag that
  every thread polls every 256 steps and that wakes every waiter; the first of them is the run's outcome (its fault
  message, or the trap's or the return's value), and the run returns once every task thread ended, so the process
  survives and a task never hangs the run (a thread inside a blocking host call ends when that call returns). A task
  nobody waited for is stopped when the main task returns. `Vm::setCancelFlag` is polled too. The mode does not use
  the schedule seed, takes no JIT (`primevm` skips the native tier). The embed API offers it as `RunOptions::threads` (TODO-5899): a fault or `RunOptions::cancel` stops every thread, `Script::run` returns once all are joined, and a script without task opcodes starts no thread. Tasks run in parallel, so the
  order of what two live tasks print is not fixed.
  **Atomics, mutexes, conditions and deadlock (TODO-5897):** the atomic opcodes run on the slot through `std::atomic_ref`,
  sequentially consistent (a 32-bit add or compare-exchange is a compare-exchange loop on the sign-extended slot), so
  `Atomic<T>` and the owner count of a `SharedString` are real atomics across tasks. A lock slot holds 0 while free
  and the holder's task number otherwise (the main task is 1, spawned tasks count up from 2); a free lock is taken
  with one compare-exchange and a held one parks the thread on the run's one condition variable until an unlock wakes
  it (an unlock with nobody parked is one compare-exchange and takes no mutex). A condition is only an address: the run
  keeps a notify count for each condition some task waits on, a wait registers and frees its lock with the run's mutex
  held and sleeps until the count changes, and a notify with nobody waiting does nothing, so no wake-up is lost and the
  slot is never written. `Channel<T>`, `Mutex<T>` and `SharedString` work unchanged on these. The faults of the
  deterministic scheduler are the same (`mutex locked again by the task that holds it`, `mutex unlocked by a task
  that does not hold it`, `condition wait by a task that does not hold the lock`). Every thread that waits (in
  `TaskWait`, for a lock or for a notify) counts itself waiting; every lock release, notify and task end clears the
  count and wakes the waiters, which count themselves again only if they still have to wait. The thread that makes
  the count reach the number of live threads (the spawned tasks that have not returned, plus the main task) is the
  last that could have changed anything, so the run faults with `deadlock: every task is waiting` (exit code 3), as
  `optexe` does; a thread in a host call or spinning on an atomic counts as running, so a program that waits for a
  host function or an atomic is never reported as deadlocked. The fault stops and joins every thread like any other.
  `primevm --vm-threads --vm-repeat <n>` (primevm only) compiles the program once and runs the module `n` times, each
  on a fresh VM; the runs' output follows one another and the exit code is the runs' common one, and a run that exits
  otherwise ends the process with exit code 1 and a `vm-repeat: run <k> exited <x> but run 0 exited <y>` line. The
  concurrent corpus of `tests/vm_threads_corpus.txt` uses it so the front end is paid once per program (TODO-6030).
- **IR link (`primevm --link`; TODO-5922; docs/ModulesAndLibrariesPlan.md, section 9):** `primevm <program> --link
  <module.psir>` (repeatable; `--link=<path>` too) runs a program merged with serialized IR modules. The program is a
  source file or a `.psir` file; a `.psir` input is loaded and run without compiling. `linkIrModule`
  (`primec/ir/IrLinker.h`) merges each module into the one before it and keeps the program's entry. Functions merge by
  name: an import (`IrFunction::isImport`) resolves to the definition, an import nobody defines yet stays one, and a
  name defined twice is `duplicate definition of <name>`, except a generic instantiation (a path segment
  `<template>__t<hex digits>`, which the compiler derives from the template arguments): two definitions of one are one
  copy when their instructions are the same (a function, host import or string operand compared by the name or text it
  names, not by its index) and `instantiation <name> differs between the modules` otherwise; a name declared with two
  parameter counts is an error too.
  Struct layouts merge by name and must be equal (`struct layout of <name> differs between the modules`); host imports
  merge by name and signature (`host import <name> has two signatures`). The library's string table is appended and
  its `Call`, `CallVoid`, `TaskSpawn`, `CallHost` and string operands are rewritten by the operand classes of
  `primec/ir/IrOperandTable.h` (one row per opcode, checked by `scripts/check_ir_opcode_table.py`: an opcode without a
  class fails the check and a `static_assert`). Debug ids of the library shift past the program's. After the last
  module, an import that is left is `undefined symbol <name> (library <lib>)` and the result is validated for the VM;
  any link error is a VM error with exit code 3 and the program does not run. **String literals (TODO-6031):** a string
  literal is a `PushI32` or `PushI64` of an index into the string table, and lowering marks each one
  (`IrInstruction::isStringLiteral`; in the serialized form the opcode bytes `0xFE` and `0xFF` stand for a marked `PushI32` and `PushI64`, since the opcode table already uses values above 127; PSIR v39), so the linker
  shifts exactly the marked pushes, wherever the string flows afterwards: a library function that prints a string it
  received as a parameter, or returns a literal, links like any other. The mark is a field in the padding of the
  24-byte `IrInstruction`, not an opcode, so the VM kernels, the fused `PushI32` sequences and every emitter see the
  push they always saw and ignore the mark; passes that build a new instruction (`InstructionRewriter::replace`) drop
  it and the constant folders do not treat a marked push as a constant. A byte after the import section of a v39
  module (`IrModule::stringPushesMarked`) says that every string push is marked. A module of v33 to v38 has no marks:
  it loads, runs and relinks as a program, but as a library with strings it is refused with `the library was written
  before IR schema v39 and does not mark its string literals; rebuild it` (the file name leads the message).
- **Synchronization record and replay (the VM, `--vm-record-sync`, `--vm-replay-sync`; TODO-5901):** a `--vm-threads`
  run is not repeatable by itself, so `primevm --vm-threads --vm-record-sync <path>` (also `primec --emit=vm`, the
  embedder's `Vm::setSyncRecordPath` and `RunOptions::syncRecord`) writes the order in which the run's tasks
  synchronized, and `primevm --vm-replay-sync <path>` (`Vm::setSyncReplayPath`, `RunOptions::syncReplay`) runs the same
  program on the deterministic scheduler in that order, so a threaded run can be repeated, stepped and inspected. A
  record needs `--vm-threads` (`--vm-record-sync records a --vm-threads run and needs --vm-threads`), a replay cannot be
  combined with `--vm-threads` or a record, and a replay ignores `--vm-schedule-seed`. Without either flag nothing
  changes: the recorder is a null pointer that the threaded primitives test, and the scheduler's replay check is one
  null test per scheduler opcode. **What is recorded:** one event for each `TaskSpawn`, `TaskWait` (when it completes),
  `MutexLock` (when the lock is taken), `MutexUnlock`, `ConditionWait` (when it registers and frees its lock),
  `ConditionNotify` and every atomic operation (load, store, add, exchange, compare-exchange), each with the number of
  the task that did it (the main task is 1, spawned tasks count up from 2 in the order of their spawn events, as lock
  slots hold them). `Channel<T>` and `Mutex<T>` are stdlib code over these opcodes, so a channel operation is the lock,
  condition and atomic events it is made of. Host input and output, string and heap operations and plain loads and
  stores are not recorded; a program that reads from the outside is replayed only if the host calls return the same
  bytes. **Ordering point:** the events are appended to one buffer under one mutex (`SyncLogRecorder`,
  `src/runtime/VmSyncLog.h`). A lock event is appended after the lock was taken and an unlock event before it is
  released, so the log has the unlock before the next holder's lock; a condition wait is appended with the run's mutex
  held before its lock is freed and a notify with the mutex held, a spawn with the task numbered and a wait when the
  task has ended. An atomic operation is the one place the recorder serializes tasks: it holds the record mutex across
  the operation itself, so the log orders atomic operations as they ran (a recorded run therefore does not exhibit the
  races between two atomic operations that an unrecorded one does; a race between a lock and an atomic, or between
  plain accesses, is not hidden). The buffer is written in 64 KiB blocks and a faulted run still writes the events up
  to its fault. A module without task opcodes records a log with no events. **Format (version 1, little-endian):** a
  24-byte header (the bytes `PSSYNCLG`, the `u32` format version 1, a `u32` of 0, and a `u64` fingerprint), the events
  as 8-byte records (`u32` task number, `u8` kind, three zero bytes), and a trailer (a record of kind 0, then a `u64`
  count of the events). Kinds: 1 spawn, 2 wait, 3 lock, 4 unlock, 5 condition wait, 6 condition notify, 7 atomic load,
  8 atomic store, 9 atomic add, 10 atomic exchange, 11 atomic compare-exchange. The fingerprint is the FNV-1a hash of the
  serialized IR of the module (`serializeIr`), so a record is refused for another program and for the same program
  compiled with other options (another `-O` level changes the IR): `the sync record <path> was recorded from a
  different program (or the same program compiled with other options)`. A file that is not a record (`is not a sync
  record`), has another format version (`has format version <n> (this VM reads version 1)`), is cut short (`is truncated`)
  or holds a kind that is not an event (`holds an unknown event kind <n>`) is refused the same way, before the program
  starts. A change of the format raises the version, and a reader refuses a version it does not know. **Replay:** the
  deterministic scheduler lets a task run a scheduler opcode (spawn, wait, lock, unlock, condition wait, notify or an
  atomic operation) only when the next event of the record is that task's and has that kind; a task that reaches one
  out of turn is parked until the record gets to it, and the task whose turn it becomes is released after each event.
  Between two events a task runs on, so what a task prints between its own events appears in the order the tasks run,
  not in the order of the threaded run: output is reproduced when it is ordered by the synchronization (inside a lock,
  or after a channel receive), not when two tasks print without any. A replay that does what the record does not have
  fails with `sync replay diverged at event <n> of <total> (task <t> <kind>): <what>`; when no task can run and the
  record has events left, the same diagnostic, and when the record is used up and every task waits, the deadlock fault
  of the run that was recorded.
- **Tasks on threads (arm64 native, PSIR v34):** the direct Mach-O emitter runs each `TaskSpawn` on a pthread
  (TODO-5942; `src/native_emitter/NativeEmitterInternalsArm64Tasks.h`). The spawner allocates a task block on the
  heap (result, `pthread_t`, argc, argv, then the parameters; the handle is its address), fills it from the value
  stack and calls the spawn routine of the callee, which starts a pthread with an 8 MiB stack (`pthread_create`,
  bound from libSystem like any imported function) at a thread entry of its own per callee: it saves x19-x30, gives
  the task a 1 MiB value stack below them, pushes the parameters as a caller would, calls the function and stores
  x0. A spawn target never takes register arguments. `TaskWait` checks the handle's block header (`task handle
  already waited` for a freed block, `invalid task handle in IR` otherwise; a freed block that was reused reads as a
  handle), joins the thread, reads the result and frees the block. A module with `TaskSpawn` allocates from
  per-thread allocator states (below); a module without it emits the code it did before. A fault leaves
  through the exit system call, which ends every thread. Atomics, mutexes and conditions on native are described in their own entries (Atomics, Mutexes, Conditions).
  **Heaps of native threads (TODO-5800):** each thread has an allocator state of its own (the chunk cursor and end,
  one free-list head per size class, a remote-free head and a pool link). The main thread keeps the data segment's;
  a spawned thread takes one from a pool (mmap'd pages that are never unmapped, since a block can outlive its
  thread) when it starts, records it in a pthread key and gives it back when it ends, so a later thread continues
  with the chunks and free lists of an earlier one. The running thread's state is the data segment's when
  TPIDRRO_EL0 equals the main thread's (recorded by the entry function, which also creates the key) and the key's
  value otherwise. A pooled chunk is mapped aligned to its 256 KiB (twice the size is mapped and the excess unmapped)
  and starts with its owner's state, so the owner of a block is found by masking its address; `HeapFree` of a block
  of another thread's chunk marks it free and pushes it onto the owner's remote-free list with `casal`, which the
  owner takes back with one `swpal` and sorts into its size classes when the free list of the class it needs is
  empty, so a double free across threads is `invalid heap free address: the block is already free` as on one thread.
  Only taking a state from the pool and giving it back take the lock word. Large blocks (their own mapping) are
  unmapped by whoever frees them. Four tasks that allocate and free 2,000,000 small blocks each ran 70 times faster
  than with the one lock of TODO-5942 (0.022 s against 1.55 s), and one task takes the time it took (0.067 s).
- **Tasks on threads (`optexe`, `optcpp`):** the optimized C++ emitter runs each `TaskSpawn` on an OS thread of its
  own (`OptCppTaskRuntime` in `src/backend/IrToOptCppRuntime.h`, pasted into the program only when the module has
  task opcodes, so a program without tasks is unchanged). The spawner copies the arguments into a task block,
  acquires a heap arena for the task and starts a pthread with an 8 MiB stack (the main thread's size; a secondary
  thread's default is 512 KiB on macOS, too little for 4096 calls); the thread builds its own `Rt` (call depth,
  local-address frames, the arena's heap and string heap) and calls the function. `TaskWait` sleeps until the
  task's function has returned, joins the thread, copies the result out and frees the task block. Handles, their checks and the fault messages are the VM's (a
  table of slots with generations behind one mutex). A block a task allocated stays valid after the task ends, so
  a task can return a heap or string heap address and the waiter frees it (the free is queued to the owning
  arena). Tasks run in parallel, so the order of what two live tasks print is not fixed; a line (a print with its
  newline) is written in one call and is never mixed with another task's. A fault on any thread flushes `stdout`,
  writes the VM's message and leaves with `_Exit(3)` while the other threads still run (only the first fault
  reports); a `Trap` and the entry function's return leave through `_Exit` the same way in a program with tasks.
  A wait that can never finish faults with the VM's `deadlock: every task is waiting` (exit code 3). The
  program counts the threads that run its code and the ones that wait (`OptCppWaitRuntime`, pasted into a module
  with task, mutex or condition opcodes): a thread waits in `TaskWait` for a task that still runs, in `MutexLock`
  for a lock another thread holds or in `ConditionWait` for a notify, all on one condition variable that an
  unlock, a notify and the end of a task signal. A signal clears
  the count and every waiter that still has to wait counts itself again, so the thread that makes the count reach
  the number of running threads is the last one that could have changed anything, and it faults. A thread inside
  a host call (a read, an `accept`) counts as running. There is no thread pool: each live task is one thread, and
  threads past the 64 heap arenas share the last one (`VmHeapArenas`).
- **Heap:** `HeapAlloc`/`HeapRealloc` return heap addresses `bit 63 | generation << 48 | slot * 16` (generation in bits
  48-62, slot offset in bits 0-47; zero slots allocate nothing and return 0). A live allocation never moves, so an
  address a program has seen stays valid until the program frees or reallocates that allocation; `HeapRealloc` returns
  a new allocation holding the old one's leading values and frees the old one. Freed slots are zeroed and reused by
  later allocations (best fit among free runs, adjacent runs merged, a free run at the end of the heap extended), and a
  reuse gives the slots the next generation, so an address from before the free still faults (`invalid indirect
  address`, `invalid heap free address`, `invalid heap realloc address`) instead of reaching the new object; the
  generation wraps after 32,768 reuses of the same slot. Slots never allocated before carry generation 0. A
  `HeapRealloc` that grows an allocation whose next slots are free (a free run long enough, or the end of the heap)
  grows it where it is: the values stay, and all its slots take a new generation, so the old address faults as after a
  moving realloc. The slot storage is reserved ahead (a million slots, then doubling) from zeroed memory that is
  written only where a program stores a value or a free clears one, so a growing heap is not copied and the slots of
  an allocation that are never written cost no memory. The VM, the
  debugger, the native execution tier and the IR-to-C++ and optexe runtimes share one implementation
  (`include/primec/runtime/VmHeapCore.h`), so all of them produce the same addresses. The arm64 native backend uses
  machine addresses instead: a block of up to 32 KiB (header included) comes from 256 KiB chunks that every size class
  shares (exact sizes up to 128 bytes, then four classes per doubling), a freed one goes on its class's free list and
  is handed out again zeroed, and a larger block has a mapping of its own; a double free or a realloc of a freed block
  prints `invalid heap free address` / `invalid heap realloc address` and exits with code 3
  (`src/native_emitter/NativeEmitterInternalsArm64Heap.h`). The x86_64 native backend maps every block on its own.
- **Host text (PSIR v30):** `IrHostValueKind::Text` is owned text at the host boundary. A `[host]` definition may name
  the stdlib `String` as a parameter or result type. A `String` argument passes its data block: the VM gives the host
  a pointer to the NUL-terminated bytes without a copy (a module-table string literal is accepted in the same
  position). A `String` result is text the host returns; the VM copies it into a fresh string-heap block and the
  call builds the `String` (length, capacity, data, owning) from it, using the VM-answered `__pstext_length` and
  `__pstext_usable`. `__pstext_argument(index)` returns the entry argument at `index` the same way, and `argumentText(index)` wraps it. The optimized C++ emitters pass a `String` argument as the NUL-terminated `const char *` of its data block and copy a `const char *` result into their own string heap, and answer the `__pstext_*` functions in the generated program; the simple C++ emitters and the native backend reject text host values until they implement the boundary.
- **String heap (TODO-5539):** the VM answers the host functions named `__pstext_*` itself, so a program needs no binding for
  them. They manage text blocks over byte regions: chunks of equal blocks in size classes (four per doubling from
  32 bytes to 64 KiB; a class's first chunk is 4 KiB or one block, each further chunk as large as the class's chunks
  together, up to 256 KiB), a byte region of its own for text over 64 KiB, an optional byte limit over the chunks and
  large blocks held (the runtime fault `string memory limit exceeded`, after free chunks were returned) and
  statistics. Of the chunks whose blocks are all free each size class keeps one, the smaller of two, and the heap
  keeps at most 1 MiB of them, returning the largest first; the rest are returned at once. `Pointer<u8>` is a valid `[host]` parameter and result type (a `u64` byte address in the slot); outside the
  runtime's own `__pstext_*` functions a `Pointer<u8>` parameter is the `bytes` host value kind (PSIR v32, see
  docs/spec/host-and-core-library.md, Byte buffers).
- **Byte regions (PSIR v28):** `HeapAllocBytes` pops a byte count and pushes the address of a zeroed region of that many
  bytes (0 for a count of 0); `LoadU8` pops an address and pushes the byte there, zero-extended; `StoreU8` pops a value
  and then an address, stores the value's low 8 bits and pushes the stored byte (as `StoreIndirect` pushes its value);
  `ZextU8` pushes the low 8 bits of a slot zero-extended, the wrap step after `u8` arithmetic as `SextI32` is for
  `i32`. A byte address is `bit 63 | generation << 48 | bit 47 | byte offset`: the offset counts bytes into one byte
  arena, so `address + i` names byte `i`, and bit 47 puts the offset far beyond any slot index, which is why a slot
  access to a byte address (`slot access to a byte region`) and a byte access to a slot, frame or unknown address
  (`byte access to a slot region`, `invalid byte address`) fault instead of reading the wrong memory. A region is
  checked on every access (inside a live region, same generation) without writing anything: a table holds one state
  per 64 bytes, the generation of the one live region whose bytes lie there and the range they fill, so an access
  costs one load and compare, and only bytes that share 64 bytes with another region ask the map of live regions.
  Regions are reused best fit and merged like slot runs, and a reuse takes a new generation, so an address from before
  a free faults. `HeapFree` accepts both kinds;
  `HeapRealloc` of a byte region counts bytes and copies the leading bytes, while a realloc of address 0 still
  allocates slots (a byte container starts with `HeapAllocBytes`). Byte memory and `ZextU8` run on both kernels (the flat loop
  since TODO-5793); the native JIT and the native, wasm and GLSL
  backends do not support them yet (the validator says so). The C++ emitters run them: `optcpp` and `optexe` always, and
  `cpp` and `exe` for a module that uses them (the optimized emitter writes it). The optimized emitters their generated runtime pastes `VmHeapCore` (`VmHeapCoreSource`) and the string heap
  (`VmTextHeapSource`, from `VmTextHeapBody.inc`), so byte addresses, faults and `stringMemory()` figures match the
  VM. `VmHeapCore` holds both arenas, so the debugger and the C++ runtimes that paste it share the same addresses.
- **Native execution tier:** on Linux x86_64, `primevm` at `-O2` (its default) runs a module as native code in its own
  process (`include/primec/backend/NativeJit.h`) with the VM's observable behavior. Integer, f64 and f32
  arithmetic (f32 results zero-extended as in the VM), comparisons, branches, calls, returns, prints of numbers and module strings, argc, string bytes and lengths, and
  indirect loads and stores are machine code; addresses keep the VM's values (frame addresses are byte offsets into the
  current frame, heap addresses are tagged slot offsets) and are translated on access. Heap allocation, freeing and
  reallocation, files, prints of argv and dynamic strings, dynamic string bytes and the float-to-i32/u64 and u64-to-f32
  conversions call into the runtime (`src/runtime/VmNativeJitHost.cpp`), which runs them with the VM's own
  handlers; the heap the runtime keeps uses the VM's addresses and the code reads and writes it directly. The VM's
  runtime faults (division by zero, string index out of bounds, invalid string index, unaligned or invalid indirect
  address, heap misuse, I/O handler errors, more than 4096 frames, a missing return) are reported with the VM's
  messages and exit code 3, and a `Trap` ends the run with its code as the VM's does; `INT64_MIN / -1` wraps as in the VM. Modules importing host functions are interpreted.
  `PRIMEVM_JIT=0` (or `PRIMEVM_KERNEL=step`, `-O0`/`-O1`, any debug mode) keeps the interpreter, and `--opt-report`
  prints `execution_tier=native` or `execution_tier=interpreter reason=...`.
- **Float comparisons:** `CmpEq/Ne/Lt/Le/Gt/GeF32/F64` follow IEEE 754: when either operand is NaN, every comparison is
  false except `CmpNe`, which is true. The VM, native (x86_64 and arm64), Wasm and C++ backends all agree; the
  `nan_comparisons` program-matrix case checks it.
- **Execution kernels:** plain runs (`primevm`, `--emit=vm`, embedded scripts) use a flat loop when every function
  passes the shared CFG analysis (consistent stack depths, valid jump targets, balanced returns, an entry without
  parameters): it keeps the instruction pointer, operand-stack pointer and locals pointer in registers, fuses common
  sequences inside a basic block into single instructions (including three-address forms that store a result straight
  into a local), and turns chains of three or more `local == constant` tests into one table lookup. A module with tasks runs under the task scheduler with each task's call stack (operand stack, locals arena, frames) on the
  flat loop: the loop runs a task until it ends or reaches a task, atomic, mutex or condition opcode, where it hands the
  call stack back with the instruction pointer at that opcode; the scheduler runs the opcode (the schedule is the one
  of the step kernel, so a seeded run prints what it printed) and resumes a task. The task opcodes are accepted only in
  that mode; a run that stops the flat loop (`PRIMEVM_KERNEL=step`, a cancel flag, a module the analysis rejects) runs
  every task on the step kernel. Measured on arm64 (instructions retired, 200000 `String`s of 77 bytes built and dropped
  in one function): in a spawned task 87.8e9 on the step kernel, 8.22e9 on the flat loop, against 8.16e9 for the same
  loop in a program without tasks (the compile step is about 2.0e9 of each). Debug sessions and any module the analysis
  rejects use
  the checked step kernel; a `Trap` ends the flat loop with its code as it ends the step kernel. Both produce the same results, output and fault messages; `PRIMEVM_KERNEL=step` forces the step
  kernel for comparison (see `docs/OptimizingBackendsPlan.md`, Phase 4).
- **Module layout:** `IrModule` bundles functions, string table, and struct layouts; lowering emits entry instructions
  plus reachable non-entry callable function bodies so function names/metadata and executable IR survive serialization.
  VM/native execution starts from `entryIndex`; recursive source functions lower to callable IR and run on every
  backend (the program matrix has a recursive `fib` case).
- **Strings & IO:** string values are indices into the module string table; `PrintString`/`LoadStringByte` read from it.
  File operations use OS descriptors stored as `i64` values and must be explicitly closed or they close on scope end via
  lowering.
- **No run-time string indices (PSIR v31):** a string value is only an index into the module string table, resolved
  by `resolveVmString(module, index)` (`include/primec/runtime/VmStringLookup.h`), which faults with `invalid string
  index in IR` for anything else. The earlier VM-owned dynamic strings (bit-63 indices into a per-run `VmStringHeap`,
  created by host calls that returned `string` and by embed string arguments) are gone: run-time text is the stdlib
  `String`, a block of the string heap below, and crosses the host boundary as `Text`. A host function that returns
  `string` is refused when the bindings are verified and when the call runs, `[host return<string>]` is a compile
  error, and `LoadStringByteDynamic` keeps its meaning for module-table strings only (the string index comes from the
  stack instead of the instruction, for example a `string` parameter or an entry-independent computed index).
- **Memory/GC:** there is no GC in the VM today. Arrays are inline locals with
  count metadata plus contiguous element slots. VM/native vector locals use a
  heap-backed `count/capacity/data_ptr` record; push/reserve growth reallocates
  that backing storage and preserves existing elements up to the current
  `1024` local dynamic-capacity limit. No
  reference counting is performed.
- **Errors:** guard rails emit errors by printing to stderr and returning error codes (e.g., bounds checks), while VM
  runtime faults (stack underflow, invalid addresses) surface as `VM error:` with exit code 3. Inside a function that
  lowering calls rather than inlines, a guard rail ends with `Trap 3` instead of a return, which ends the program with
  exit code 3 from that frame; `panic` traps the same way. `Trap` (PSIR v33) runs on the VM, serialized bytecode,
  the C++ emitters (`cpp`/`exe` and `optcpp`/`optexe`: `exit` after flushing the output), `--emit=native` (the exit
  syscall: `exit` on macOS arm64, `exit_group` on Linux x86_64; output is already written) and the Linux x86_64 JIT
  (which hands the code back to `primevm` as the program's result). The Wasm and GLSL targets never receive it; there a
  runtime error in a called function still returns 3 to its caller (TODO-5744). **Runtime faults on every backend:**
  integer division by zero (`i32`, `i64`, `u64`) is the runtime fault `division by zero in IR`: the VM, the C++
  emitters (`cpp`/`exe` and `optcpp`/`optexe`) and the Linux x86_64 JIT write `VM error: division by zero in IR` to
  stderr and exit with code 3, keeping the output printed before it. The simple C++ emitter exits 3 for all its
  runtime faults (stack underflow, invalid or unaligned address, invalid string index, string index out of bounds,
  heap misuse, host function misuse), after flushing the buffered output, with the VM's message. `INT64_MIN / -1` and
  `INT32_MIN / -1` wrap on the VM and the C++ emitters. Float division never faults (an infinity or NaN). Native arm64 division by zero is the same fault (an inline zero check before `sdiv`/`udiv`); native x86_64 is
  still a hardware result (SIGFPE; TODO-6051). **Recursion limits (backend
  specific, measured on macOS arm64 with the default 8 MiB stack, a recursive `i32` function):** the VM, the flat
  loop and `optexe` allow 4096 frames and past that fault with `VM error: VM call stack overflow` and exit code 3
  (`depth(4096)` of the measured function already faults); the Linux x86_64 JIT does the same.
  `--emit=native` and `--emit=exe` keep the frames on the process stack and have no depth check: native ran 8000 and
  failed at 20000, `exe` ran 20000 and failed at 50000, both ending with SIGSEGV (exit status 139) and, for `exe`,
  without the buffered output. A program needing more than 4096 levels is therefore portable to neither the VM nor
  `optexe`; Wasm and GLSL were not measured (no Wasm runtime on the measuring machine).
- **Deployment target:** the VM serves as the sandboxed runtime for user-supplied scripts (e.g., on iOS) where native
  code generation is unavailable. Effect masks and capabilities enforce per-platform restrictions.

- Real calls: the VM, the optimizing C++ emitters (`optcpp`, `optexe`) and the arm64 native backend lower a definition
  once as a function and call it from every call site when its body is more than a tiny leaf (more than two statements
  or more than 12 expression nodes; a tiny leaf stays inline unless it is recursive) and its shape allows it (below).
  The first call site that meets such a definition reserves it, so every definition the program reaches is decided,
  not only those with two static call sites. Arguments pass by the address ABI: the call site binds the parameters
  exactly as an inlined call would, in its own frame, and passes each parameter local's value - a scalar, or the
  address of a struct, of a `mut` place or of a referenced value; the receiver passes as its address, `mut` or not. A
  local address names the same slot in every frame (Frames & stack), so writes through these addresses reach the
  caller. A temporary argument is destroyed after the call, where an inlined call destroys it at the end of the callee's
  body. A struct or sum result (any struct but a collection record, with or without lifecycle helpers, and a sum: `Result`, `Maybe` or a user `[sum]`; a spawned call also returns a builtin `map`) is returned
  through a block of the caller's frame whose address is the last argument: the callee's `return` moves or copies the
  value there (as it would into the inlined call's result) and returns the address. A sum is built in the callee's frame
  (or, with `Result` and `Maybe`, constructed there) and moves into the block slot for slot, so the payload's owner is the
  caller from then on; a sum result nobody binds is not destroyed, as for an inlined call. A called function therefore never
  returns an address of its own frame, which is gone once it returns: what it returns came in as a parameter. IR
  validation rejects a function that a `Call` reaches and that returns an address derived from its own
  `AddressOfLocal`. A definition that stays inline at every call site has no function in the module: the module holds
  the entry and the bodies of real calls only. A scalar or struct parameter taken by
  `copy` or `move` is bound (copied or moved) by the call site too, but belongs to the callee: a scalar arrives as the
  value of the call site's copy (a `copy mut` scalar is the body's own slot, not an address), a struct as the copy's
  address, and the body holds the drop flag and destroys the struct on every exit unless it moves it on, as the
  inlined body does. A builtin `vector<T>` parameter (`mut`, read only, or `Reference<vector<T>>`; element a scalar, a `String`, a struct or
  a vector) passes the address of the vector's record, which sits in the caller's frame: the callee reads and writes that
  record, so a push past capacity in the callee (it reallocates the buffer and updates the record) is seen by the caller,
  and assigning to a `mut` vector parameter replaces the caller's record in place. No copy is made for a borrowed vector and the caller stays the
  owner; a temporary vector argument of a read-only parameter is freed by the call site after the call. The callee's parameter local takes its
  element layout (`LocalInfo::vectorStructElementSlotCount`) and, for `mut`, the element lifecycle path
  (`vectorElementLifecyclePath`) from the parameter type (`registerRealCallOwnedParameters`). A `copy` parameter gets a copy
  the call site makes in its frame (a place; a fresh vector or `move(x)` is handed over as it is) and a `move` parameter
  the vector handed over; the call site binds it and registers no drop, and the body owns it like a struct `copy`
  parameter: a drop flag and a drop entry (`registerRealCallOwnedParameters`), so it is freed once, on every exit
  unless the body moves it on (a spawned call passes it owned the same way).
  Shapes that stay inline for a direct call (a spawned call takes owned sums and builtin maps, above):
  `copy` and `move` parameters of sums, args-pack, collection,
  `File` and template-typed parameters; the packed `Result` of a program that does not import `/std/result` (its result text is
  not a scalar type);
  definitions with `on_error`, `require`, `try`/`?` or a generated tail value; `/std/collections/` definitions and
  methods of their records; lifecycle helpers; and the `call` of a closure with a `[ref]` capture
  (docs/spec/value-lifecycle.md, Borrowing). A `string` parameter is a real call only on the VM and the optimizing C++
  emitters, which read a module string by a computed index, and only when the body uses it as `count()` or as the
  argument of `__pstext_literal` (`String.append`): the call passes the string's module-table index and the body reads
  a runtime string, where any other use (indexing expands to a branch per table string) stays inline.
  `String.append` costs the same as inlined: its length check and `growFor`'s capacity check are inlined into the callers
  and the C++ emitters call the bulk move (`__pstext_move`) directly instead of through the by-name host dispatch.
  The other targets keep the scalar ABI: a definition with scalar parameters and result and two or more call sites (or
  a recursive one) is a real call, a read-only method passing its receiver as slots rebuilt in the callee's frame. On
  the scalar targets that run `Trap` (`cpp`/`exe`, x86_64 native) a `never` result counts as void, so `panic` is one
  function there too instead of a copy of its body at every call site.
  Measured for the web examples under `--emit=optcpp` in docs/OptimizingBackendsPlan.md (section 9, Real calls for web
  apps): `examples/apps/todo_web` went from 12.7 MB of C++ to under 1.4 MB.
- Shared exit paths: a body (the entry or a real call's function) that holds two or more `return` statements (counted
  syntactically, nested blocks included) shares the destruction of its owning locals between them. A `return` whose
  live scope entries include a drop entry (an owning struct, sum or builtin vector local) stores its value in a local
  of the body (one per return opcode), jumps to a cleanup chain and does not expand the destruction itself. The chains
  are emitted once, after the body (`SharedReturnExits`, `include/primec/ir_lowerer/IrLowererSharedReturnExits.h`): the
  chain of one exit is its live entries last declared first, so an exit shares the chain of every exit whose live
  entries are a prefix of its own, and each chain node is the destruction of one entry (the same drop-flag check, flag
  clear and destroy helpers that a scope's own exit emits) followed by its parent, ending in the load of the stored value
  and the return. The drop flags still decide at run time what each node destroys, so every exit destroys exactly the
  locals live at it, in the same order, once. A body with one `return`, an exit with no drop entry live (nothing but
  file handles, or nothing), the exits of an inlined callee (they already jump to the call's exit), `break` and
  `continue`, and the `?` exit keep the inline cleanup, so their IR is unchanged. Measured on the one-body form of
  `tests/fixtures/build/build_graph_one_body.prime` (a `Graph` and seven checks in one `run_checks`): 51,665 lines of
  `--emit=optcpp` C++ for `run_checks` became 10,967, with the same output and heap statistics on the VM and optexe.
  A drop flag is only set where its local is declared, so a scope entry must be live (on the scope stack) only on
  the paths that ran the declaration: each arm of a `pick` statement and of a `pick` expression is a scope of its own (its owning locals are
  destroyed where the arm ends; in the expression form, after the arm's value is computed or copied into the
  result and the local the arm ends with has been taken over), because arm locals left in the enclosing scope were read at the exits of the arms
  after it, an uninitialized flag on the native backend, which does not zero its frames.
- Struct copies: a struct of fewer than 16 slots is copied by one load/store sequence per slot. A struct of 16 slots
  or more (a `Request` is about 23) is copied by a loop over chunks of 8 slots through advancing copies of the two
  addresses, then the remaining slots in line (`emitStructCopyFromPtrs`, `StructCopyLoopMinSlots` and
  `StructCopyLoopUnrollSlots` in `include/primec/ir_lowerer/IrLowererFlowHelpers.h`); the loop takes three
  temporary locals per copy site. Every backend sees ordinary `Jump`/`JumpIfZero` IR.
