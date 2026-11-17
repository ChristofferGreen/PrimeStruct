#pragma once

#include "primec/pipeline/BuildCommand.h"

#include <string>
#include <vector>

namespace primec {

// The graph of a project with its subprojects (docs/BuildSystemPlan.md section 7). Starts one `primec build
// --dry-run --raw` per script (a separate VM run: the project's own, then the `make.prime` of every folder some script
// named with `b.subproject`, each once, in a deterministic order) and merges the exports with `mergeBuildGraphJson`.
// A subproject runs under the effect ceiling of the project without `build_run`, so `Graph.run` there is a diagnostic.
// Diagnostics of a script are the ones of its compile; a missing `make.prime`, a folder outside the project and an
// inclusion cycle (naming the scripts) are described in `diagnostics` with the prefix `primec build: `.
// Returns 0 and the JSON text in `graphText` (the export of the project script unchanged when it names no subproject),
// or the failing exit code.
int resolveBuildGraph(const BuildCommandPlan &plan,
                      const std::string &selfPath,
                      int stage,
                      std::string &graphText,
                      std::string &diagnostics);

// The option list a subproject script runs with: `forwarded` plus an `--allow-effects` that is the project's list (or the
// build profile's) without `build_run`.
std::vector<std::string> subprojectForwardedOptions(const std::vector<std::string> &forwarded);

} // namespace primec
