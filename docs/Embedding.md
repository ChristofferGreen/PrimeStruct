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

## Tests

Suites live in `tests/unit/embed/`: `script_engine`, `diagnostics`, `bytecode`
(round trip, determinism, truncation/corruption fuzzing), `threads`, `host_calls` (hand-built IR, VM
and binding API), `host_language` (`[host]` source declarations, backend rejection, offline bytecode), and
`runtime_only` (a separate binary linking only `primec_embed_runtime_lib`). The
fixture programs are in `embed_fixture_programs.h`; their pinned bytecode is
regenerated with
`PRIMESTRUCT_EMBED_REGEN_FIXTURES=<repo>/tests/unit/embed/embed_fixture_bytecode.h
build-release/PrimeStruct_embed_tests --test-case="*regenerates*"`.

`loadBytecode` is hardened against corrupt input (counts are bounded by the
remaining bytes, found by the corruption fuzzing), but it validates structure,
not behavior: do not run bytecode from an untrusted source.
