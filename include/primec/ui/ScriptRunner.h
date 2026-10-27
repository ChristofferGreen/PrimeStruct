#pragma once

// Runs a PrimeScript on a worker thread for a UI backend's "Run" (docs/NativeUiPlan.md section 18). The interpreter is
// the embedding facade (primec::embed::ScriptEngine: parser, semantics, lowerer and VM), which is large, so it lives in
// its own dynamic library (libps_script) that a backend loads with scriptRunnerApi() on the first run and never at
// launch. The library exports the three C functions below; the backends call them through the table.

#include <cstddef>

extern "C" {

typedef struct PsScriptRun PsScriptRun;
// Output of the script in program order: fd 1 is stdout, fd 2 is stderr and the runner's own last lines (a diagnostic,
// "[exit code N]", "[stopped]"). Called on the worker thread; `chunk` is valid during the call.
typedef void (*PsScriptOutput)(int fd, const char *chunk, size_t size, void *user);
// Called once on the worker thread after the last output: status 0 the script ran (`exitCode` is main's result),
// 1 it did not compile or failed while running, 2 it was stopped.
typedef void (*PsScriptFinished)(int status, int exitCode, void *user);

// Starts compiling and running `text` (named `name` in diagnostics and as the base of relative imports). `stdlibDir` is
// the directory holding `std/` ("" or NULL: the engine looks for it itself). Returns NULL when the thread cannot start.
PsScriptRun *ps_script_start(const char *name, const char *text, const char *stdlibDir, PsScriptOutput output,
                             PsScriptFinished finished, void *user);
// Asks the run to stop (it fails with status 2 soon after; a no-op once finished).
void ps_script_stop(PsScriptRun *run);
// Stops the run if it still goes, waits for the worker to end and frees it. No callback runs after it returns.
void ps_script_release(PsScriptRun *run);
}

namespace primec::ui {

struct ScriptRunnerApi {
  PsScriptRun *(*start)(const char *, const char *, const char *, PsScriptOutput, PsScriptFinished, void *) = nullptr;
  void (*stop)(PsScriptRun *) = nullptr;
  void (*release)(PsScriptRun *) = nullptr;
};

// The runner library's functions, loading it on the first call (the PRIMESTRUCT_SCRIPT_RUNNER environment variable,
// then `libps_script.dylib` beside the library or executable this code is in, then the build tree's copy). nullptr when
// it cannot be loaded; `scriptRunnerError()` then says why.
const ScriptRunnerApi *scriptRunnerApi();
const char *scriptRunnerError();

} // namespace primec::ui
