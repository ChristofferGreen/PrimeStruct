// Running scripts on the headless backend (ps_ui_script_*, docs/NativeUiPlan.md section 18). The script runs on a
// worker thread of the runner library; its output is collected and handed over only when no user action is left, so a
// scenario is deterministic: the queued actions (a Stop, say) all happen before the script's output and end arrive.

#include "NativeUiHeadlessState.h"

#include "primec/ui/ScriptRunner.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace primec::ui::headless::detail {

struct ScriptSession {
  PsScriptRun *run = nullptr;
  int32_t endCommand = 0;
  std::mutex mutex;
  std::condition_variable changed;
  std::string pending;
  bool finished = false;

  ~ScriptSession() {
    if (run != nullptr) {
      ps_script_release(run);
    }
  }
};

namespace {

void onOutput(int, const char *chunk, size_t size, void *user) {
  auto *session = static_cast<ScriptSession *>(user);
  std::lock_guard<std::mutex> lock(session->mutex);
  session->pending.append(chunk, size);
}

void onFinished(int, int, void *user) {
  auto *session = static_cast<ScriptSession *>(user);
  {
    std::lock_guard<std::mutex> lock(session->mutex);
    session->finished = true;
  }
  session->changed.notify_all();
}

} // namespace

bool deliverScriptEvent() {
  for (auto &entry : state().windows) {
    Window &w = entry.second;
    if (w.script == nullptr || w.state.closed) {
      continue;
    }
    ScriptSession &session = *w.script;
    std::unique_lock<std::mutex> lock(session.mutex);
    // A script that never ends must not hang a test forever: stop it after a while.
    if (!session.changed.wait_for(lock, std::chrono::seconds(20), [&] { return session.finished; })) {
      lock.unlock();
      ps_script_stop(session.run);
      lock.lock();
      session.changed.wait(lock, [&] { return session.finished; });
    }
    w.state.output += session.pending;
    session.pending.clear();
    const int32_t command = session.endCommand;
    lock.unlock();
    w.script.reset(); // joins the worker
    w.state.scriptRunning = false;
    setEvent(PS_UI_EVENT_COMMAND, entry.first, 0, command);
    return true;
  }
  return false;
}

} // namespace primec::ui::headless::detail

using namespace primec::ui::headless::detail;

bool ps_ui_script_run(uint64_t window, const char *name, const char *text, int32_t endCommand) {
  const std::string call = "ps_ui_script_run(" + std::to_string(window) + ", " + quote(name) + ", " + quote(text) +
                           ", " + std::to_string(endCommand) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w == nullptr || w->script != nullptr) {
    return loggedFlag(call, false);
  }
  w->state.output.clear();
  w->state.outputVisible = true;
  auto session = std::make_shared<ScriptSession>();
  session->endCommand = endCommand;
  session->run = ps_script_start(name != nullptr ? name : "script.prime", text != nullptr ? text : "", "", onOutput,
                                 onFinished, session.get());
  if (session->run == nullptr) {
    w->state.output = std::string("The script could not be started: ") + ps_script_last_error() + "\n";
    return loggedFlag(call, false);
  }
  w->script = std::move(session);
  w->state.scriptRunning = true;
  return loggedFlag(call, true);
}

bool ps_ui_script_stop(uint64_t window) {
  const std::string call = "ps_ui_script_stop(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  const bool running = w != nullptr && w->script != nullptr;
  if (running) {
    ps_script_stop(w->script->run);
  }
  return loggedFlag(call, running);
}

bool ps_ui_script_running(uint64_t window) {
  const std::string call = "ps_ui_script_running(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  return loggedFlag(call, w != nullptr && w->state.scriptRunning);
}

bool ps_ui_window_set_output_visible(uint64_t window, bool visible) {
  const std::string call = "ps_ui_window_set_output_visible(" + std::to_string(window) + ", " + flag(visible) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->state.outputVisible = visible;
  }
  return loggedFlag(call, w != nullptr);
}

bool ps_ui_window_output_visible(uint64_t window) {
  const std::string call = "ps_ui_window_output_visible(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  return loggedFlag(call, w != nullptr && w->state.outputVisible);
}
