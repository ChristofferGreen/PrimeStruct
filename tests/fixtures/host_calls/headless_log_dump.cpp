// Linked into a compiled native UI program together with the headless backend: queues the
// scripted user actions of the HEADLESS_SCENARIO environment variable (see scenario.h) before the
// program starts, and prints the backend's call log when it ends, so a test can compare it with a
// VM run of the same scenario.
#include "primec/ui/NativeUiHeadless.h"

#include "scenario.h"

#include <cstdio>
#include <cstdlib>

namespace {
struct LogDumper {
  LogDumper() {
    // Touch the backend first: its state is destroyed after this object, so the log is
    // still there when the destructor runs.
    (void)primec::ui::headless::callLog();
    if (const char *scenario = std::getenv("HEADLESS_SCENARIO")) {
      applyHeadlessScenario(scenario);
    }
  }
  ~LogDumper() {
    for (const std::string &line : primec::ui::headless::callLog()) {
      std::printf("%s\n", line.c_str());
    }
  }
};
const LogDumper dumper;
} // namespace
