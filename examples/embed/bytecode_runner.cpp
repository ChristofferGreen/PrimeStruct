// Runtime-only host: links just the VM (PrimeStruct::embed_runtime) and runs
// bytecode produced offline with `primec --emit=ir script.prime -o script.psir`.
//
//   bytecode_runner script.psir [args...]
#include "primec/embed/Script.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: bytecode_runner <script.psir> [args...]\n";
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "cannot open " << argv[1] << "\n";
    return 2;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  const primec::embed::Script script = primec::embed::Script::loadBytecode(bytes, argv[1]);
  if (!script.valid()) {
    std::cerr << script.diagnostics() << "\n";
    return 1;
  }
  const std::vector<std::string> args(argv + 2, argv + argc);
  const primec::embed::ScriptResult result = script.run(args);
  if (!result.ok) {
    std::cerr << result.diagnostics << "\n";
    return 1;
  }
  std::cout << "exit code " << result.exitCode << "\n";
  return 0;
}
