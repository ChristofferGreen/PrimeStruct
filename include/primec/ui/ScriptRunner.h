#pragma once

// Runs a PrimeScript for a UI backend's "Run" (docs/NativeUiPlan.md section 18). The script runs in its own process:
// the PrimeScriptRunner executable (the development runner `primestruct_app` with its `--script-host` mode: the
// compiler, the interpreter and the native UI bindings) reads the text from its standard input and runs it, so a
// script that opens windows is an app of its own, a script that does not end is stopped by ending its process, and a
// crash cannot take the editor down. The backend reads the process's output (stdout and stderr, in program order) on a
// worker thread. The functions are plain C so a backend needs nothing else.

#include <cstddef>

extern "C" {

typedef struct PsScriptRun PsScriptRun;
// Output of the script in program order (fd is 1: the process's stdout and stderr share one pipe) and the runner's own
// last lines (a diagnostic, "[exit code N]", "[stopped]"). Called on the worker thread; `chunk` is valid during the call.
typedef void (*PsScriptOutput)(int fd, const char *chunk, size_t size, void *user);
// Called once on the worker thread after the last output: status 0 the script ran (its own trailer says how it ended),
// 1 it did not start or its process failed, 2 it was stopped.
typedef void (*PsScriptFinished)(int status, int exitCode, void *user);

// Starts the runner and sends it `text` (named `name` in diagnostics and as the base of relative imports). `stdlibDir` is
// the directory holding `std/` ("" or NULL: the runner looks for it itself). Returns NULL when the runner cannot be found
// or started; ps_script_last_error() then says why.
PsScriptRun *ps_script_start(const char *name, const char *text, const char *stdlibDir, PsScriptOutput output,
                             PsScriptFinished finished, void *user);
// Asks the run to stop (its process is ended; a no-op once finished).
void ps_script_stop(PsScriptRun *run);
// Stops the run if it still goes, waits for the worker to end and frees it. No callback runs after it returns.
void ps_script_release(PsScriptRun *run);
// Why the last ps_script_start returned NULL.
const char *ps_script_last_error(void);
}
