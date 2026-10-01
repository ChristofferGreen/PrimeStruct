// Minimal C++ host embedding PrimeStruct.
//
//   embed_example                  built-in demo (below)
//   embed_example script.prime ... compile and run a script file with args
//
// Demo steps (the script declares host functions that this program implements):
//   1. compile a script from a string,
//   2. run it with arguments,
//   3. save it as bytecode and run the bytecode (what an iOS app would ship),
//   4. show that script errors come back as data.
#include "primec/embed/ScriptEngine.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
const char *Source = R"(
// Provided by the C++ host (see the bind calls in main below).
[host return<void>]
host_log([i32] value) {
}

[host return<int>]
host_add([i32] a, [i32] b) {
}

[return<int>]
main([array<string>] args) {
  [mut] total{0i32}
  [mut] i{1i32}
  while(i <= 10i32) {
    total = host_add(total, i)
    host_log(total)
    i = i + 1i32
  }
  return(total + args.count())
}
)";
} // namespace

int main(int argc, char **argv) {
  primec::embed::ScriptEngine engine;

  // File mode: behaves like a tiny `primevm`.
  if (argc > 1) {
    const primec::embed::Script file = engine.compileFile(argv[1]);
    if (!file.valid()) {
      std::cerr << file.diagnostics();
      return 1;
    }
    const primec::embed::ScriptResult fileResult = file.run(std::vector<std::string>(argv + 2, argv + argc));
    if (!fileResult.ok) {
      std::cerr << fileResult.diagnostics << "\n";
      return 1;
    }
    std::cout << "exit code " << fileResult.exitCode << "\n";
    return 0;
  }

  // 1. Compile, then provide the host functions the script declares. Types are
  //    deduced from the lambdas; they must match the script's declarations.
  primec::embed::Script script = engine.compileSource("/example.prime", Source);
  if (!script.valid()) {
    std::cerr << "compile failed:\n" << script.diagnostics();
    return 1;
  }
  std::vector<int> logged;
  script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
  script.bind("host_log", [&logged](int32_t value) { logged.push_back(value); });

  // 2. Run. argv is the script name plus the arguments given here.
  const primec::embed::ScriptResult result = script.run({"first", "second"});
  std::cout << "run:      ok=" << result.ok << " exitCode=" << result.exitCode << "\n";
  std::cout << "logged:   " << logged.size() << " host_log calls, last=" << logged.back() << "\n";

  // 3. Bytecode round trip. A runtime-only host (links just the VM) can do the
  //    loadBytecode half without any compiler code.
  std::vector<uint8_t> bytes;
  std::string error;
  if (!script.saveBytecode(bytes, error)) {
    std::cerr << "save failed: " << error << "\n";
    return 1;
  }
  primec::embed::Script loaded = primec::embed::Script::loadBytecode(bytes);
  loaded.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
  loaded.bind("host_log", [](int32_t) {});
  const primec::embed::ScriptResult again = loaded.run({"first", "second"});
  std::cout << "bytecode: " << bytes.size() << " bytes, exitCode=" << again.exitCode << "\n";

  // 4. Errors are data, not crashes or stderr noise.
  const primec::embed::Script broken =
      engine.compileSource("/broken.prime", "[return<int>]\nmain() {\n  return(nope())\n}\n");
  std::cout << "broken:   valid=" << broken.valid() << "\n" << broken.diagnostics();

  return result.ok && again.ok ? 0 : 1;
}
