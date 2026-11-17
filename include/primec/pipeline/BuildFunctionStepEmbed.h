#pragma once

#include "primec/pipeline/BuildFunctionStep.h"

#include <memory>
#include <string>

namespace primec {

// The function-step runner of `primec`: the compile and the VM of the embedding library (primec_embed_lib). A step is
// compiled with its definition as the entry under an effect ceiling of `file_read`, `heap_alloc`, `io_out`, `io_err`
// and the task effects (no `file_write`, no network, no `build_run`), and runs with the host calls of `/std/build_step`
// bound: `ps_build_write_output` writes a file inside the output directory (and faults with `build step <name> writes
// outside the output directory: <path>` otherwise) and `ps_build_project_dir` names the project. What the definition
// prints is captured and reported only when the step fails. Tasks that the definition spawns run on threads.
std::unique_ptr<BuildFunctionStepRunner> makeEmbeddedFunctionStepRunner();

// For `primec build --dry-run`: adds the import closure of every function step of the export `graphText` (the files of the
// project the compile of the step read, relative to the project, sorted; the stdlib belongs to the compiler and is not
// listed) as `"closure"` to its node, by compiling each step with `runner`. False, with the compile diagnostics of the
// failing step in `diagnostics`, when a step does not compile. A graph without function steps comes back unchanged.
bool addFunctionStepClosures(BuildFunctionStepRunner &runner,
                             const std::string &graphText,
                             const std::string &projectDir,
                             const std::string &outDir,
                             std::string &annotated,
                             std::string &diagnostics);

} // namespace primec
