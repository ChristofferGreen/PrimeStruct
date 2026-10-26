// A scripted run of the headless native UI backend, written as text so the VM test and the
// compiled program (headless_log_dump.cpp) queue the same user actions:
//
//   cmd:7;type:0: more;close:1;save:/out/file.txt;open:/in/file.txt;alert:1;quit
//
// Entries are separated by ';'. `cmd:N` is a menu command, `type:W:text` types into window
// handle W, `close:W` asks to close it, `save:path` and `open:path` answer the next save and open
// panel, `alert:N` answers the next alert, `activate:W` makes window W the active one and `quit`
// asks the app to quit, `check:H` clicks check box H and `popup:H:I` chooses item I of pop-up H, `pref:key=1` presets a boolean preference and `select:H:S:E` selects bytes S to E of text view H, `recent:path` notes a recent file, `pint:key=3` and `plist:key=a,b` preset an integer and a list preference. Text may carry %XX escapes (for ';', '%' and line breaks).
#pragma once

#include "primec/ui/NativeUiHeadless.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

inline std::string decodeScenarioText(const std::string &text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 2 < text.size()) {
      out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else {
      out += text[i];
    }
  }
  return out;
}

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
                             second == std::string::npos ? "" : decodeScenarioText(rest.substr(second + 1)));
    } else if (key == "select") {
      const size_t second = rest.find(':');
      const size_t third = second == std::string::npos ? second : rest.find(':', second + 1);
      if (third != std::string::npos) {
        headless::pushSelectText(static_cast<uint64_t>(std::atoll(rest.substr(0, second).c_str())),
                                 std::atoi(rest.substr(second + 1, third - second - 1).c_str()),
                                 std::atoi(rest.substr(third + 1).c_str()));
      }
    } else if (key == "close") {
      headless::pushCloseWindow(static_cast<uint64_t>(std::atoll(rest.c_str())));
    } else if (key == "save") {
      headless::pushSavePanelAnswer(decodeScenarioText(rest));
    } else if (key == "open") {
      headless::pushOpenPanelAnswer(decodeScenarioText(rest));
    } else if (key == "alert") {
      headless::pushAlertAnswer(std::atoi(rest.c_str()));
    } else if (key == "pint") {
      const size_t equals = rest.rfind('=');
      headless::setIntPref(decodeScenarioText(rest.substr(0, equals)),
                           equals == std::string::npos ? 0 : std::atoi(rest.substr(equals + 1).c_str()));
    } else if (key == "plist") {
      const size_t equals = rest.rfind('=');
      std::vector<std::string> items;
      const std::string joined = equals == std::string::npos ? "" : rest.substr(equals + 1);
      for (size_t from = 0; from < joined.size();) {
        size_t comma = joined.find(',', from);
        comma = comma == std::string::npos ? joined.size() : comma;
        items.push_back(decodeScenarioText(joined.substr(from, comma - from)));
        from = comma + 1;
      }
      headless::setPrefList(decodeScenarioText(rest.substr(0, equals)), items);
    } else if (key == "recent") {
      headless::pushRecentFile(decodeScenarioText(rest));
    } else if (key == "pref") {
      const size_t equals = rest.rfind('=');
      headless::setBoolPref(decodeScenarioText(rest.substr(0, equals)),
                            equals != std::string::npos && rest.substr(equals + 1) == "1");
    } else if (key == "check") {
      headless::pushToggleCheckBox(static_cast<uint64_t>(std::atoll(rest.c_str())));
    } else if (key == "popup") {
      const size_t second = rest.find(':');
      headless::pushSelectPopupItem(static_cast<uint64_t>(std::atoll(rest.substr(0, second).c_str())),
                                    second == std::string::npos ? 0 : std::atoi(rest.substr(second + 1).c_str()));
    } else if (key == "rows") {
      const size_t second = rest.find(':');
      std::vector<int32_t> rows;
      const std::string joined = second == std::string::npos ? "" : rest.substr(second + 1);
      for (size_t from = 0; from < joined.size();) {
        size_t comma = joined.find(',', from);
        comma = comma == std::string::npos ? joined.size() : comma;
        rows.push_back(std::atoi(joined.substr(from, comma - from).c_str()));
        from = comma + 1;
      }
      headless::pushSelectListRows(static_cast<uint64_t>(std::atoll(rest.substr(0, second).c_str())), rows);
    } else if (key == "click") {
      headless::pushClickButton(static_cast<uint64_t>(std::atoll(rest.c_str())));
    } else if (key == "openfile") {
      headless::pushOpenFile(decodeScenarioText(rest));
    } else if (key == "activate") {
      headless::pushActivateWindow(static_cast<uint64_t>(std::atoll(rest.c_str())));
    } else if (key == "quit") {
      headless::pushQuit();
    }
  }
}
