// A scripted run of the headless native UI backend, written as text so the VM test and the
// compiled program (headless_log_dump.cpp) queue the same user actions:
//
//   cmd:7;type:0: more;close:1;save:/out/file.txt;open:/in/file.txt;alert:1;quit
//
// Entries are separated by ';'. `cmd:N` is a menu command, `type:W:text` types into window
// handle W, `close:W` asks to close it, `save:path` and `open:path` answer the next save and open
// panel, `alert:N` answers the next alert and `quit` asks the app to quit.
#pragma once

#include "primec/ui/NativeUiHeadless.h"

#include <cstdlib>
#include <string>
#include <string_view>

inline void applyHeadlessScenario(std::string_view scenario) {
  namespace headless = primec::ui::headless;
  size_t start = 0;
  while (start <= scenario.size()) {
    size_t end = scenario.find(';', start);
    if (end == std::string_view::npos) {
      end = scenario.size();
    }
    const std::string entry(scenario.substr(start, end - start));
    start = end + 1;
    if (entry.empty()) {
      continue;
    }
    const size_t colon = entry.find(':');
    const std::string key = entry.substr(0, colon);
    const std::string rest = colon == std::string::npos ? "" : entry.substr(colon + 1);
    if (key == "cmd") {
      headless::pushCommand(std::atoi(rest.c_str()));
    } else if (key == "type") {
      const size_t second = rest.find(':');
      headless::pushTypeText(static_cast<uint64_t>(std::atoll(rest.substr(0, second).c_str())),
                             second == std::string::npos ? "" : rest.substr(second + 1));
    } else if (key == "close") {
      headless::pushCloseWindow(static_cast<uint64_t>(std::atoll(rest.c_str())));
    } else if (key == "save") {
      headless::pushSavePanelAnswer(rest);
    } else if (key == "open") {
      headless::pushOpenPanelAnswer(rest);
    } else if (key == "alert") {
      headless::pushAlertAnswer(std::atoi(rest.c_str()));
    } else if (key == "quit") {
      headless::pushQuit();
    }
  }
}
