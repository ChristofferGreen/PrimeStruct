#pragma once

#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildRunner.h"

#include <string>
#include <vector>

namespace primec {

// The declarations and the cache of function steps (docs/BuildSystemPlan.md section 4 and 8); the compile and the run
// of the definition are `BuildFunctionStepRunner` (BuildFunctionStep.h).

enum class FunctionStepCheck { Failed, UpToDate, NeedsRun };

// What the runner settles about a function step before it compiles anything: every declared input is a file of the
// project, every declared output is a place inside the output directory (`resolveStepOutputPath`), and, with a cache,
// whether the step is up to date: the entry of the step has the same key (compiler, file, definition, inputs, outputs,
// output directory), every file the previous compile read and every input still has its hash, and every output exists
// with its hash. `cacheFile` and `key` are set when `caching` and are what `finishFunctionStep` writes. On Failed the
// problem is in `diagnostics`.
FunctionStepCheck checkFunctionStep(const BuildStep &step,
                                    const BuildRunSettings &settings,
                                    bool caching,
                                    const std::string &compilerHash,
                                    std::string &cacheFile,
                                    std::string &key,
                                    std::string &diagnostics);

// After the definition returned 0: every declared output must exist (a step that did not write one fails with the
// path), and with a cache the entry is written, depending on `files` (what the compile read) and the inputs. False,
// with the problem in `diagnostics`, when an output is missing.
bool finishFunctionStep(const BuildStep &step,
                        const BuildRunSettings &settings,
                        bool caching,
                        const std::string &cacheFile,
                        const std::string &key,
                        const std::vector<std::string> &files,
                        std::string &diagnostics);

} // namespace primec
