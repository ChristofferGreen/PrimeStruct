#pragma once

#include "primec/pipeline/BuildGraphJson.h"

#include <memory>
#include <string>
#include <vector>

namespace primec {

struct BuildRunSettings;

// What `primec build` needs to run a function step (docs/BuildSystemPlan.md section 4, `Build.step` of /std/build): the
// compile of the step's file and the run of its definition on a VM thread. The runner of `runBuildGraph` lives in the
// IR library and the compile in the embedding library above it, so `primec` hands an implementation to the runner
// (`BuildRunSettings::functionSteps`); a build without one reports that function steps are not supported.
class BuildFunctionStepRunner {
public:
  // A compiled step, ready to run. Compiling is not thread safe (the compile pipeline keeps process-wide state), so the
  // runner compiles one step at a time on the thread that called it and runs the compiled steps on worker threads.
  class Prepared {
  public:
    virtual ~Prepared() = default;
  };

  virtual ~BuildFunctionStepRunner() = default;

  // Compiles the file `step.source` with the definition `step.function` as the entry, under the effect ceiling of
  // function steps. On success returns the compiled step and, in `files`, the files the compile read (absolute, sorted:
  // the file, its imports and the stdlib); on failure returns null and the compile diagnostics in `diagnostics`.
  virtual std::unique_ptr<Prepared> prepare(const BuildStep &step,
                                            const BuildRunSettings &settings,
                                            std::vector<std::string> &files,
                                            std::string &diagnostics) = 0;

  // Runs a compiled step (callable from several threads at once, each with its own step). True when the definition
  // returned 0; otherwise false with the fault or the exit code in `diagnostics`, followed by what it printed.
  virtual bool run(Prepared &prepared,
                   const BuildStep &step,
                   const BuildRunSettings &settings,
                   std::string &diagnostics) = 0;
};

} // namespace primec
