# Embedding PrimeStruct in a C++ host

Two libraries, one API (`primec/embed/Script.h`):

| Library | Contains | Use when |
| --- | --- | --- |
| `primec_embed_lib` | full compiler + VM (`ScriptEngine`, `Script`) | desktop/server hosts that compile source at run time |
| `primec_embed_runtime_lib` | VM + IR (de)serialization/validation only (`Script`) | hosts that run precompiled bytecode, notably iOS (no JIT, no on-device compiler) |

The runtime-only test binary is ~435 KB (release, unstripped) and contains no
parser, semantics, or lowerer symbols.

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
(round trip, determinism, truncation/corruption fuzzing), `threads`, and
`runtime_only` (a separate binary linking only `primec_embed_runtime_lib`). The
fixture programs are in `embed_fixture_programs.h`; their pinned bytecode is
regenerated with
`PRIMESTRUCT_EMBED_REGEN_FIXTURES=<repo>/tests/unit/embed/embed_fixture_bytecode.h
build-release/PrimeStruct_embed_tests --test-case="*regenerates*"`.

`loadBytecode` is hardened against corrupt input (counts are bounded by the
remaining bytes, found by the corruption fuzzing), but it validates structure,
not behavior: do not run bytecode from an untrusted source.
