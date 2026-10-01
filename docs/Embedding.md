# Embedding PrimeStruct in a C++ host

Two libraries, one API (`primec/embed/Script.h`):

| Library | Contains | Use when |
| --- | --- | --- |
| `primec_embed_lib` | full compiler + VM (`ScriptEngine`, `Script`) | desktop/server hosts that compile source at run time |
| `primec_embed_runtime_lib` | VM + IR (de)serialization/validation only (`Script`) | hosts that run precompiled bytecode, notably iOS (no JIT, no on-device compiler) |

The runtime-only test binary is ~435 KB (release, unstripped) and contains no
parser, semantics, or lowerer symbols.

## Using it from CMake

```sh
cmake --install build-release --prefix /opt/primestruct
cmake -S examples/embed -B build -DCMAKE_PREFIX_PATH=/opt/primestruct
```

```cmake
find_package(PrimeStruct REQUIRED)
target_link_libraries(host PRIVATE PrimeStruct::embed)          # compile + run
target_link_libraries(host PRIVATE PrimeStruct::embed_runtime)  # bytecode only
```

The install carries the stdlib at `<prefix>/share/primestruct/stdlib`. The engine
finds it via `ScriptEngine::setStdlibPath`, then `$PRIMESTRUCT_STDLIB`, then the
prefix baked in at configure time, then the source tree it was built from. If
you relocate the install, call `setStdlibPath` or set the environment variable.
`examples/embed/` has a full-compile host (`embed_example`) and a runtime-only
host (`embed_bytecode_runner`, ~200 KB); CTest `PrimeStruct_embed_install_package`
installs, builds and runs both against the installed package.

## Compile and run

```cpp
#include "primec/embed/ScriptEngine.h"

primec::embed::ScriptEngine engine;
auto script = engine.compileSource("/hello.prime", R"(
[return<int>]
main() {
  return(7i32)
}
)");
if (!script.valid()) { /* script.diagnostics() */ }
auto result = script.run();   // result.ok, result.exitCode == 7
```

Errors are returned as data (`ScriptResult::diagnostics`); the API never exits
the process or writes to stdout/stderr.

## Calling script functions from the host

Declare the functions the host will call, then call them by name:

```cpp
engine.exportFunction<double(int32_t, double)>("scale");
engine.exportFunction<int32_t(int32_t, int32_t)>("add");
auto script = engine.compileSource("/lib.prime", source);

auto r = script.call<double>("scale", 3, 2.5);   // r.ok, r.value == 7.5, r.diagnostics
auto s = script.call<int32_t>("add", 40, 2);
```

Types are `int32_t`, `int64_t`, `uint64_t`, `float`, `double`, `bool` (and `void`
results). A mismatch with the declaration, an unknown name, or a missing host
binding is an error value in `CallResult`, never undefined behavior. Each export
compiles its own entry (a generated wrapper that fetches arguments and reports
the result through reserved `__psarg_*` / `__psret_*` host functions), so exports
add compile time but calls are plain VM runs. A script with exports may omit
`main`. `saveBytecode` then writes a `PSBN` bundle (all modules plus signatures)
that `loadBytecode` restores in the runtime-only library, which can `call` the
exports without a compiler. `Script::call` is safe to use from several threads
on one `Script`.

String arguments (`std::string_view`, `std::string`, `const char *`) are copied into
the call: the VM cannot create strings, so each call runs against a copy of the
export's module whose string table has the arguments appended (the original
module is never modified). The VM represents strings as table indices, so inside
the script an argument string can be measured (`text.count()`), printed, and
forwarded to host functions, but not indexed (`text.at(i)` needs a string whose
bytes are known at compile time). String results are not supported.

## Calling the host from a script

Bind C++ callables by name; signatures come from the callable's primitive
parameter types (`int32_t`, `int64_t`, `uint64_t`, `float`, `double`, `bool`;
`void` or one of those as the result):

```cpp
script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
engine.bind("log_value", [](int32_t v) { /* ... */ });   // applies to every compiled script
```

Scripts declare the host functions they call with `[host]` definitions (see
"Host functions (embedding)" in `docs/PrimeStruct.md`):

```prime
[host return<int>]
host_add([i32] a, [i32] b) {
}
```

These lower to the `CallHost` IR opcode and the module's host import table. `Script::requiredHostFunctions()` lists what a script needs
and `checkHostBindings()` reports anything missing or mismatched; `run()` does
the same check first and returns a diagnostic without executing anything.
Host functions that throw are reported as errors. Only the VM and `--emit=ir`
bytecode support host calls (native/C++/wasm/GLSL emission rejects them); VM
debug sessions and `primevm` have no bindings. Offline bytecode that declares
host functions loads in the runtime-only library, which then needs the same
`bind` calls before `run`.

## Precompiled bytecode

```cpp
std::vector<uint8_t> bytes; std::string error;
script.saveBytecode(bytes, error);                       // full library
auto loaded = primec::embed::Script::loadBytecode(bytes); // runtime-only OK
```

Offline from the command line: `primec --emit=ir main.prime -o main.psir`
writes the same serialized module; `Script::loadBytecode` loads it.

Loaded modules are validated for the VM target; corrupt, truncated, or
wrong-version bytes produce an invalid `Script` with a diagnostic.

## Bytecode compatibility

Bytecode is the serialized `IrModule` (magic `RISP`, then a format version).
Ship bytecode and runtime built from the same IR version; a version mismatch is
rejected at load. `tests/unit/embed/embed_fixture_bytecode.h` pins the current
format and the full-library test fails when it drifts.

## Lifetime and threading

- The embed API does not use the CLI's `ScopedCompileArena`: compiling and running
  use the system allocator, so compiled `Script`s own ordinary heap memory and can
  outlive the engine that produced them (or be copied, moved, and run on another
  thread). Repeated compiles, failed compiles, `run`, and `call` do not grow
  memory (`primestruct.embed.lifetime` checks resident-set growth over thousands
  of iterations).
- A compiled `Script` shares its immutable modules between copies. `run`,
  `call`, `requiredHostFunctions`, `checkHostBindings`, and `saveBytecode` are
  safe to call concurrently on one `Script`. `bind` mutates that `Script`'s own
  binding table and must not run concurrently with its other methods; bind before
  sharing, or give each thread its own copy.
- Independent `ScriptEngine`s may compile on different threads at once, and a
  compile yields the same bytecode no matter what was compiled before or
  alongside it. One `ScriptEngine` is not safe to mutate (`addImportPath`,
  `exportFunction`, `bind`) while another thread compiles with it.
- Host callbacks run on the thread that called `run`/`call`; the engine adds no
  locking around them.
- ThreadSanitizer smoke: configure with `-DPRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON`
  and run `PrimeStruct_embed_tsan_smoke` (two engines compiling and running
  concurrently, one script shared by several threads); it is clean.

## Tests

Suites live in `tests/unit/embed/`: `script_engine`, `diagnostics`, `exports`, `bytecode`
(round trip, determinism, truncation/corruption fuzzing), `threads`, `lifetime`, `host_calls` (hand-built IR, VM
and binding API), `host_language` (`[host]` source declarations, backend rejection, offline bytecode), and
`runtime_only` (a separate binary linking only `primec_embed_runtime_lib`). The
fixture programs are in `embed_fixture_programs.h`; their pinned bytecode is
regenerated with
`PRIMESTRUCT_EMBED_REGEN_FIXTURES=<repo>/tests/unit/embed/embed_fixture_bytecode.h
build-release/PrimeStruct_embed_tests --test-case="*regenerates*"`.

`loadBytecode` is hardened against corrupt input (counts are bounded by the
remaining bytes, found by the corruption fuzzing), but it validates structure,
not behavior: do not run bytecode from an untrusted source.
