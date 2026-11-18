#pragma once

#include "primec/pipeline/BuildCommand.h"
#include "primec/pipeline/BuildGraphJson.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace primec {

class BuildFunctionStepRunner;

struct BuildRunSettings {
  // The compiler to start for each step (the running `primec`).
  std::string primecPath;
  // The project directory: steps run with it as the working directory, so a step's relative source, import path
  // and link argument are relative to the project.
  std::string projectDir;
  // Where the outputs go (an absolute path, or relative to the project directory).
  std::string outDir;
  // Where the cache files of the steps live (`ProjectRoot/.ps/cache`, an absolute path). A step whose output
  // exists and whose key and input hashes are unchanged is not run (see BuildCache.h). Empty: no cache.
  std::string cacheDir;
  // At most this many steps run at once; values below 1 mean 1.
  int jobs = 1;
  // Compiles and runs the function steps of the graph (BuildFunctionStep.h); null: a graph with one is refused.
  BuildFunctionStepRunner *functionSteps = nullptr;
};

// The command line of one compile step, starting with the compiler path.
std::vector<std::string> buildStepArguments(const BuildStep &step,
                                            const BuildRunSettings &settings);

// Runs every compile step of `graph`, in parallel up to `settings.jobs`. Prints one `built <id>` line per finished
// step (`up to date <id>` for a step the cache skipped) to `out` and, for a failed step, its name and the diagnostics of the compiler to `err`. A failure stops the
// steps that have not started. A graph with a problem runs nothing. Library nodes build like any step, and the steps that link them wait for them (BuildLibraryLinks.h). Returns 0 when
// every step succeeded and 1 otherwise.
int runBuildGraph(const BuildGraphData &graph,
                  const BuildRunSettings &settings,
                  std::ostream &out,
                  std::ostream &err);

// `primec build` without `--dry-run`: starts `selfPath build --dry-run` for the graph of the script (so the script's
// diagnostics are the ones of an ordinary compile), parses it and runs it. Returns the exit code of the command.
int runBuildCommand(const BuildCommandPlan &plan,
                    const std::string &selfPath,
                    std::ostream &out,
                    std::ostream &err,
                    BuildFunctionStepRunner *functionSteps = nullptr);

// The path of the running executable, or `argv0` when the system does not say.
std::string selfExecutablePath(const std::string &argv0);

// Runs `arguments` (the first is the program) with `workingDir` as the working directory (empty: unchanged). The
// standard output goes to `capturedOut`, the standard error to `capturedErr`. Returns the exit status, or -1 when the
// program could not be started or did not exit normally.
int runProgramCaptured(const std::vector<std::string> &arguments,
                       const std::string &workingDir,
                       std::string &capturedOut,
                       std::string &capturedErr);

} // namespace primec
