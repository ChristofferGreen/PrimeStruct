// The script runner library (libps_script): see include/primec/ui/ScriptRunner.h.

#include "primec/ui/ScriptRunner.h"

#include "primec/embed/ScriptEngine.h"

#include <atomic>
#include <pthread.h>
#include <string>

struct PsScriptRun {
  std::string name;
  std::string text;
  std::string stdlibDir;
  PsScriptOutput output = nullptr;
  PsScriptFinished finished = nullptr;
  void *user = nullptr;
  std::atomic<bool> stop{false};
  pthread_t thread{};
  bool started = false;
};

namespace {

void emit(PsScriptRun &run, int fd, const std::string &text) {
  if (!text.empty() && run.output != nullptr) {
    run.output(fd, text.data(), text.size(), run.user);
  }
}

void execute(PsScriptRun &run) {
  primec::embed::ScriptEngine engine;
  if (!run.stdlibDir.empty()) {
    engine.setStdlibPath(run.stdlibDir);
  }
  const primec::embed::Script script = engine.compileSource(run.name, run.text);
  int status = 0;
  int exitCode = 0;
  if (!script.valid()) {
    status = 1;
    emit(run, 2, script.diagnostics() + "\n");
  } else {
    primec::embed::RunOptions options;
    options.cancel = &run.stop;
    options.output = [&run](int fd, std::string_view chunk) {
      if (run.output != nullptr) {
        run.output(fd, chunk.data(), chunk.size(), run.user);
      }
    };
    const primec::embed::ScriptResult result = script.run({}, options);
    if (result.ok) {
      exitCode = result.exitCode;
      emit(run, 2, "[exit code " + std::to_string(exitCode) + "]\n");
    } else if (run.stop.load()) {
      status = 2;
      emit(run, 2, "[stopped]\n");
    } else {
      status = 1;
      emit(run, 2, result.diagnostics + "\n");
    }
  }
  if (run.finished != nullptr) {
    run.finished(status, exitCode, run.user);
  }
}

void *threadMain(void *argument) {
  execute(*static_cast<PsScriptRun *>(argument));
  return nullptr;
}

} // namespace

extern "C" {

PsScriptRun *ps_script_start(const char *name, const char *text, const char *stdlibDir, PsScriptOutput output,
                             PsScriptFinished finished, void *user) {
  auto *run = new PsScriptRun;
  run->name = name != nullptr ? name : "script.prime";
  run->text = text != nullptr ? text : "";
  run->stdlibDir = stdlibDir != nullptr ? stdlibDir : "";
  run->output = output;
  run->finished = finished;
  run->user = user;
  // The compiler recurses deeply on big sources: a worker gets a generous stack (a secondary thread's default is small).
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, 64u * 1024u * 1024u);
  const int created = pthread_create(&run->thread, &attributes, threadMain, run);
  pthread_attr_destroy(&attributes);
  if (created != 0) {
    delete run;
    return nullptr;
  }
  run->started = true;
  return run;
}

void ps_script_stop(PsScriptRun *run) {
  if (run != nullptr) {
    run->stop.store(true);
  }
}

void ps_script_release(PsScriptRun *run) {
  if (run == nullptr) {
    return;
  }
  run->stop.store(true);
  if (run->started) {
    pthread_join(run->thread, nullptr);
  }
  delete run;
}

} // extern "C"
