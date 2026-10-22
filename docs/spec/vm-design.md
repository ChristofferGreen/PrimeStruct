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
- **PSIR versioning:** current portable IR is PSIR v31 (v24 to v31 are listed in `include/primec/ir/Ir.h`'s opcode
  comments and the finished TODO log; v31 retires run-time string indices (a host function can no longer return `string`; same module layout); v30 appends the `text` host value kind; v29 appends the `u8` host value kind; v28 adds `HeapAllocBytes`, `LoadU8`, `StoreU8` and `ZextU8`). Before that, PSIR v23 (adds a per-function `parameter_count` field on top of v22’s
  source-unit/file identity in source-map metadata, v21’s `HeapRealloc`, v20’s `FileReadByte` and
  `HeapFree`, v19’s per-instruction source-map metadata keyed by debug ID, v18’s instruction debug IDs, v17’s local
  debug slots, v16’s function-call opcodes `Call`/`CallVoid`, v15’s execution metadata, v14’s float return opcodes,
  v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`,
  `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** VM/native execution starts at the entry frame and pushes/pops frames for `Call`/`CallVoid`; each
  frame stores locals in 16-byte slots while the operand stack stores raw `u64` values interpreted by opcode (ints,
  floats as bits, and indices). Indirect addresses are byte offsets into the active frame’s local slot space and must be
  16-byte aligned.
- **Heap:** `HeapAlloc`/`HeapRealloc` return heap addresses `bit 63 | generation << 48 | slot * 16` (generation in bits
  48-62, slot offset in bits 0-47; zero slots allocate nothing and return 0). A live allocation never moves, so an
  address a program has seen stays valid until the program frees or reallocates that allocation; `HeapRealloc` returns
  a new allocation holding the old one's leading values and frees the old one. Freed slots are zeroed and reused by
  later allocations (best fit among free runs, adjacent runs merged, a free run at the end of the heap extended), and a
  reuse gives the slots the next generation, so an address from before the free still faults (`invalid indirect
  address`, `invalid heap free address`, `invalid heap realloc address`) instead of reaching the new object; the
  generation wraps after 32,768 reuses of the same slot. Slots never allocated before carry generation 0. The VM, the
  debugger, the native execution tier and the IR-to-C++ and optexe runtimes share one implementation
  (`include/primec/runtime/VmHeapCore.h`), so all of them produce the same addresses.
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
  reuse. `Pointer<u8>` is a valid `[host]` parameter and result type (a `u64` byte address in the slot).
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
  backends do not support them yet (the validator says so; TODO-5574). The optimized C++ emitters (`optcpp`,
  `optexe`) run them: their generated runtime pastes `VmHeapCore` (`VmHeapCoreSource`) and the string heap
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
  messages and exit code 3; `INT64_MIN / -1` wraps as in the VM. Modules importing host functions are interpreted.
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
  runtime faults (stack underflow, invalid addresses) surface as `VM error:` with exit code 3.
- **Deployment target:** the VM serves as the sandboxed runtime for user-supplied scripts (e.g., on iOS) where native
  code generation is unavailable. Effect masks and capabilities enforce per-platform restrictions.
