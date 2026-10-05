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
- **PSIR versioning:** current portable IR is PSIR v23 (adds a per-function `parameter_count` field on top of v22’s
  source-unit/file identity in source-map metadata, v21’s `HeapRealloc`, v20’s `FileReadByte` and
  `HeapFree`, v19’s per-instruction source-map metadata keyed by debug ID, v18’s instruction debug IDs, v17’s local
  debug slots, v16’s function-call opcodes `Call`/`CallVoid`, v15’s execution metadata, v14’s float return opcodes,
  v13’s float arithmetic/compare/convert opcodes, and v12’s struct field visibility/static metadata, `LoadStringByte`,
  `PrintArgvUnsafe`, `PrintArgv`, `PushArgc`, pointer helpers, `ReturnVoid`, and print opcode upgrades).
- **Frames & stack:** VM/native execution starts at the entry frame and pushes/pops frames for `Call`/`CallVoid`; each
  frame stores locals in 16-byte slots while the operand stack stores raw `u64` values interpreted by opcode (ints,
  floats as bits, and indices). Indirect addresses are byte offsets into the active frame’s local slot space and must be
  16-byte aligned.
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
- **VM-owned dynamic strings (implemented: VM heap, `LoadStringByteDynamic`, embed string arguments and results):** a string value is a `u64`. Before the VM heap existed it could only index the immutable module table; this section records the model that is now implemented, without changing how module-table strings behave.
  - *Index space.* A string value stays a `u64`. Values with bit 63 clear index the module table exactly as today.
    Values with bit 63 set are *dynamic*: `0x8000_0000_0000_0000 | generation << 32 | slot` (31-bit generation, 32-bit
    slot). The tag is independent of the module's table size, so existing indices and `.psir` files keep their meaning.
  - *One lookup.* Every VM string access (`LoadStringLength`, `LoadStringByte`, `PrintString`, `PrintStringDynamic`,
    the file-open/`FileWriteString*` opcodes, host-call string arguments) goes through one helper,
    `resolveVmString(module, heap, index)`, which returns the string or faults: index past the table, dynamic slot out of
    range, or a generation that no longer matches the slot (use after release).
  - *Heap.* `VmStringHeap` is a slot vector with a free list and a per-slot generation. It belongs to one run
    (`Vm::execute`, one debug session): it is created empty, and destroyed with the run. There is no GC and no per-string
    free opcode in the first version, so strings made during a run live until the run ends; embedders that call into a
    script repeatedly start a new run per call, so memory does not grow across calls. A release opcode can be added
    later without changing the index format (generations already catch stale uses).
  - *Creation.* Only host calls create strings in the first version: a binding whose return kind is `String` returns
    its text (`VmHostBinding` gains a string-returning invoke form) and the VM pushes the new dynamic index. Script code
    cannot concatenate or build strings; that is a separate language feature.
  - *Indexing.* `LoadStringByte` carries the string index as an immediate (compile-time literal), so it cannot read a
    dynamic string. A new opcode `LoadStringByteDynamic` is appended after `CallHost`: it pops the byte position and the
    string index (both from the stack) and pushes the byte, with the same bounds fault as `LoadStringByte`.
  - *Backends.* Native, wasm, glsl/spirv and C++ emission do not support dynamic strings: the opcode table
    (`include/primec/ir/IrOpcodeTable.h`) marks `LoadStringByteDynamic` as VM-only (all target flags 0), so the validator
    rejects it for those targets with the existing "unsupported opcode for <target> target" diagnostic. Programs that
    only use literal strings are unaffected.
  - *PSIR.* Appending an opcode changes the serialized format's opcode range, so `IrSchemaVersion` goes 25 -> 26 and
    version 25 files are rejected (the supported range is exactly the current version; done in TODO-5364). Host import return kind
    `String` is already encoded; no module layout change.
  - *IR sketch* (`host_name()` returns a string, the script returns its first byte):

    ```
    CallHost 0            ; host_name() -> dynamic string index on the stack
    Dup
    LoadStringLength      ; resolveVmString: length of the dynamic string
    Pop
    PushI32 0             ; byte position
    LoadStringByteDynamic ; pops position, then index -> byte
    ReturnI32
    ```
  - *Touched sites (estimate).* VM: `VmExecutionKernel.cpp` (2 table lookups + the new opcode), `VmIoHelpers.cpp` (1 lookup
    used by print/file ops), `VmExecution.cpp` (host-call string argument and string return, 2 sites), `VmHost.h/.cpp`
    (string-returning binding), the debug session (shares the kernel, nothing extra), plus `Ir.h`, the opcode table,
    validator, serializer and version constant. Lowerer/semantics: choosing `LoadStringByteDynamic` for `text.at(i)` when
    the receiver is not a literal-backed string. Embed: `Script::call` argument/result strings and removal of the
    `__psarg_string` special case. Roughly 15 source files, none of them large changes.
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
