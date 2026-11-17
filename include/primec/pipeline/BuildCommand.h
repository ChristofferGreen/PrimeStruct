#pragma once

#include "primec/pipeline/BuildOptions.h"

#include <string>
#include <vector>

namespace primec {

// `primec build [--dry-run] [dir] [options]` (docs/spec/integration-and-tooling.md, Build command). The command is
// the ordinary compile of `<dir>/make.prime` under `--profile build`, run on the VM with the stdlib entry
// `/std/build_runner/run`, so this plan is the argument list and source text that compile starts from.
struct BuildCommandPlan {
  // Whether `--dry-run` was given: print the graph instead of running it.
  bool dryRun = false;
  // `--raw` (internal, with `--dry-run`): print the export of this script alone. Without it the dry run also runs the
  // scripts of its subprojects and prints the merged graph (include/primec/pipeline/BuildSubprojects.h).
  bool raw = false;
  // The project directory the command was given (default `.`).
  std::string projectDir;
  // `--out-dir` (default `<dir>/out`): where the compile steps write; `--jobs` (0: the core count): how many run at
  // once. Neither is passed on to the compile of the script.
  std::string outDir;
  int jobs = 0;
  // `--stage <n>` (internal: the runner starts the dry run once per stage): the zero-based stage of `Graph.run` the
  // script stops at. The script receives it as its first program argument. Default 0.
  int stage = 0;
  // `--option name=value` (repeatable), sorted by name: the values of the options the scripts declare with `Build.option`.
  // They go to every script of the build (the project's, its subprojects' and each stage) as program arguments after the
  // stage, so they are not part of the script cache key; the script sees them, the graph carries them.
  std::vector<BuildOptionRequest> options;
  // `--list-options`: print the options the scripts declare (types, defaults, descriptions) instead of building.
  bool listOptions = false;
  // The options after the directory, except `--dry-run`, `--jobs`, `--out-dir`, `--option` and `--list-options`.
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
