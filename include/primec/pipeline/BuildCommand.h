#pragma once

#include <string>
#include <vector>

namespace primec {

// `primec build [--dry-run] [dir] [options]` (docs/spec/integration-and-tooling.md, Build command). The command is
// the ordinary compile of `<dir>/make.prime` under `--profile build`, run on the VM with the stdlib entry
// `/std/build_runner/run`, so this plan is the argument list and source text that compile starts from.
struct BuildCommandPlan {
  // Whether `--dry-run` was given: print the graph instead of running it.
  bool dryRun = false;
  // The project directory the command was given (default `.`).
  std::string projectDir;
  // `--out-dir` (default `<dir>/out`): where the compile steps write; `--jobs` (0: the core count): how many run at
  // once. Neither is passed on to the compile of the script.
  std::string outDir;
  int jobs = 0;
  // `--stage <n>` (internal: the runner starts the dry run once per stage): the zero-based stage of `Graph.run` the
  // script stops at. The script receives it as its first program argument. Default 0.
  int stage = 0;
  // The options after the directory, except `--dry-run`, `--jobs` and `--out-dir`.
  std::vector<std::string> forwarded;
  // The path of the script, `<dir>/make.prime`.
  std::string scriptPath;
  // The arguments of the compile, starting with the program name, in the form `parseOptions` takes.
  std::vector<std::string> arguments;
  // The text of the script with the import of the runner module appended after its last line, so that every line
  // number of a diagnostic is a line of `make.prime`.
  std::string source;
};

// Plans `primec build` from the arguments after the word `build`. Returns false and sets `error` when the directory
// has no `make.prime` or the script cannot be read.
bool planBuildCommand(const std::vector<std::string> &buildArguments,
                      BuildCommandPlan &plan,
                      std::string &error);

} // namespace primec
