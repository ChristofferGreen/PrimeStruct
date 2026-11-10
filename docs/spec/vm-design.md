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
- **GLSL note:** GLSL/SPIR-V emission routes through canonical IR (`glsl-ir`/`spirv-ir`) and `IrValidationTarget::Glsl`;
  these modes emit backend output directly without requiring PSIR serialization.
- **PSIR versioning:** current portable IR is PSIR v34 (v24 to v34 are listed in `include/primec/ir/Ir.h`'s opcode
  comments and the finished TODO log; v34 appends `TaskSpawn` and `TaskWait` (see Tasks) and is the first version
  that also loads the previous one: a v33 module has the same layout and cannot hold the task opcodes, which the
  loader refuses in it, and it is written back as v34; v33 appends `Trap` and numbers local addresses across all live frames (see
  Frames & stack); v32 appends the `bytes` host value kind (`Pointer<u8>` host parameters); v31 retires run-time string indices (a host function can no longer return `string`; same module layout); v30 appends the `text` host value kind; v29 appends the `u8` host value kind; v28 adds `HeapAllocBytes`, `LoadU8`, `StoreU8` and `ZextU8`). Before that, PSIR v23 (adds a per-function `parameter_count` field on top of v22’s
  source-unit/file identity in source-map metadata, v21’s `HeapRealloc`, v20’s `FileReadByte` and
  `HeapFree`, v19’s per-instruction source-map metadata keyed by debug ID, v18’s instruction debug IDs, v17’s local
  debug slots, v16’s function-call opcodes `Call`/`CallVoid`, v15’s execution metadata, v14’s float return opcodes,
  v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`,
  `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** VM/native execution starts at the entry frame and pushes/pops frames for `Call`/`CallVoid`; each
  frame stores locals in 16-byte slots while the operand stack stores raw `u64` values interpreted by opcode (ints,
  floats as bits, and indices). A local address (`AddressOfLocal`) is `(base + index) * 16`, where a frame's `base` is
  its caller's base plus the caller's local count (the entry frame's base is 0): the live frames' locals form one
  numbering, so an address of a caller's local names the same slot in every callee, which reads and writes it through
  `LoadIndirect`/`StoreIndirect`. An address beyond the live frames faults (`invalid indirect address in IR`).
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
  (`VmTaskScheduler` in `src/runtime/VmExecutionKernel.cpp`) and switches only at a spawn, a wait and a task's end.
  The default schedule is eager: a spawn runs the new task until it ends or blocks, then its spawner continues, so a
  program prints exactly what it prints with each spawned call run inline; a blocked task yields to the
  lowest-numbered runnable task (tasks are numbered in spawn order, the main task is 0). `--vm-schedule-seed <n>`
  (`primec --emit=vm`, `primevm`) or `PRIMESTRUCT_VM_SCHEDULE_SEED` picks the next runnable task at every switch point
  with a splitmix64 generator seeded by `n`, so the same seed gives the same run; tests run several seeds and expect
  the same output to show a program is free of races. When no task can run the VM faults with `deadlock: every task
  is waiting` (exit code 3); the main task's return ends the program, and a `Trap` in any task ends it with its code.
  Only the vm target and serialized bytecode accept the task opcodes; validation also rejects a `TaskWait` whose
  operand is not a `TaskSpawn` result on every path (through locals and `Dup`). A module with task opcodes runs on the
  step kernel (the flat loop and the native JIT decline it), and a debug session faults on them (`tasks run only in a
  plain VM run, not in a debug session`, TODO-5837). Lowering for the vm target and serialized bytecode turns
  `[Task<T>] t{[spawn] f(args)}` into the arguments, evaluated in the spawner, and `TaskSpawn f` stored in `t`, when
  every parameter of `f` is a plain scalar and its result a scalar or void (f becomes a real call even when it is a
  tiny leaf); every read of `t` (`wait(t)`, a name in a multi-wait tuple) is `LoadLocal t; TaskWait`. Any other spawned
  call (a `mut` or `copy mut` parameter, a `String`, struct or collection parameter, a struct or sum result, a method)
  runs inline, which is a legal schedule, and `--opt-report` says so: `lowering: spawn of /f runs inline: <reason>`.
  Every other backend runs every spawned call inline; a `Task<void>` spawn there is the call as a statement.
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
  machine addresses instead: a block of up to 8 KiB (header included) comes from 256 KiB chunks that every size class
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
  them. They manage text blocks over byte regions: 256 KiB chunks of equal blocks in size classes (four per doubling from
  32 bytes to 64 KiB), a byte region of its own for text over 64 KiB, an optional byte limit (the runtime fault
  `string memory limit exceeded`) and statistics. A chunk whose blocks are all free is returned except one kept for
  reuse. `Pointer<u8>` is a valid `[host]` parameter and result type (a `u64` byte address in the slot); outside the
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
  checked on every access (inside a live region, same generation); regions are reused best fit and merged like slot
  runs, and a reuse takes a new generation, so an address from before a free faults. `HeapFree` accepts both kinds;
  `HeapRealloc` of a byte region counts bytes and copies the leading bytes, while a realloc of address 0 still
  allocates slots (a byte container starts with `HeapAllocBytes`). Byte memory and `ZextU8` run on the step kernel and the
  flat loop declines modules that use the three memory opcodes; the native JIT and the native, wasm and GLSL
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
  into a local), and turns chains of three or more `local == constant` tests into one table lookup. Debug sessions and any module the analysis rejects use the
  checked step kernel. Both produce the same results, output and fault messages; `PRIMEVM_KERNEL=step` forces the step
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
  runtime error in a called function still returns 3 to its caller (TODO-5744).
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
  body. A struct result (any struct but a sum or a collection record, with or without lifecycle helpers) is returned
  through a block of the caller's frame whose address is the last argument: the callee's `return` moves or copies the
  value there (as it would into the inlined call's result) and returns the address. A called function therefore never
  returns an address of its own frame, which is gone once it returns: what it returns came in as a parameter. IR
  validation rejects a function that a `Call` reaches and that returns an address derived from its own
  `AddressOfLocal`. A definition that stays inline at every call site has no function in the module: the module holds
  the entry and the bodies of real calls only. A `Function` parameter taken by
  `copy` or `move` is bound (copied or moved) by the call site too, but belongs to the callee: its body holds the drop
  flag and destroys the closure on every exit unless it moves it on, as the inlined body does. Shapes that stay inline:
  `copy` and `move` parameters of other types, args-pack, `string`, collection, `File` and template-typed parameters;
  sum results (`Result`, `Maybe`);
  definitions with `on_error`, `require`, `try`/`?` or a generated tail value; `/std/collections/` definitions and
  methods of their records; lifecycle helpers; and the `call` of a closure with a `[ref]` capture
  (docs/spec/value-lifecycle.md, Borrowing).
  The other targets keep the scalar ABI: a definition with scalar parameters and result and two or more call sites (or
  a recursive one) is a real call, a read-only method passing its receiver as slots rebuilt in the callee's frame. On
  the scalar targets that run `Trap` (`cpp`/`exe`, x86_64 native) a `never` result counts as void, so `panic` is one
  function there too instead of a copy of its body at every call site.
  Measured for the web examples under `--emit=optcpp` in docs/OptimizingBackendsPlan.md (section 9, Real calls for web
  apps): `examples/apps/todo_web` went from 12.7 MB of C++ to under 1.4 MB.
