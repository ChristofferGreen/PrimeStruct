// Linked into a compiled native UI program together with the headless backend: prints the
// backend's call log when the program ends, so a test can compare it with a VM run.
#include "primec/ui/NativeUiHeadless.h"

#include <cstdio>

namespace {
struct LogDumper {
  LogDumper() {
    // Touch the backend first: its state is destroyed after this object, so the log is
    // still there when the destructor runs.
    (void)primec::ui::headless::callLog();
    // The user actions of the hello_window scenario in the test.
    primec::ui::headless::pushCommand(7);
    primec::ui::headless::pushTypeText(0, " more");
    primec::ui::headless::pushCommand(3);
    primec::ui::headless::pushCloseWindow(1);
  }
  ~LogDumper() {
    for (const std::string &line : primec::ui::headless::callLog()) {
      std::printf("%s\n", line.c_str());
    }
  }
};
const LogDumper dumper;
} // namespace
