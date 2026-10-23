#include "NativeUiHeadlessState.h"

#include "primec/ui/NativeUiHeadless.h"
#include "primec/ui/NativeUiRecentFiles.h"
#include "primec/ui/NativeUiStandardItems.h"

#include <algorithm>
#include <string>
#include <vector>

// The control surface of the headless backend: queueing user actions and scripted answers, presetting
// preferences and recents, and reading back the widget tree, the menus and the call log.
namespace primec::ui::headless {
using namespace detail;

// Text in a scenario entry: ';' separates entries and newlines end a line, so they travel as %XX.
std::string encodeScenarioText(const std::string &text) {
  static const char *Digits = "0123456789ABCDEF";
  std::string out;
  for (const unsigned char c : text) {
    if (c == '%' || c == ';' || c == ',' || c == '\n' || c == '\r') {
      out += '%';
      out += Digits[c >> 4];
      out += Digits[c & 15];
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

void recordScenario(const std::string &entry) {
  std::string &scenario = state().scenario;
  scenario += scenario.empty() ? entry : ";" + entry;
}

void reset() { state() = State{}; }
std::string pushedScenario() { return state().scenario; }
void pushCommand(int32_t commandId) {
  Action a;
  a.kind = Action::Kind::Command;
  a.command = commandId;
  recordScenario("cmd:" + std::to_string(commandId));
  state().actions.push_back(std::move(a));
}
void pushTypeText(uint64_t widget, std::string text) {
  Action a;
  a.kind = Action::Kind::TypeText;
  a.handle = widget;
  recordScenario("type:" + std::to_string(widget) + ":" + encodeScenarioText(text));
  a.text = std::move(text);
  state().actions.push_back(std::move(a));
}
void pushCloseWindow(uint64_t window) {
  Action a;
  a.kind = Action::Kind::CloseWindow;
  a.handle = window;
  recordScenario("close:" + std::to_string(window));
  state().actions.push_back(std::move(a));
}
void pushQuit() {
  recordScenario("quit");
  state().actions.push_back(Action{});
}
void pushActivateWindow(uint64_t window) {
  Action a;
  a.kind = Action::Kind::Activate;
  a.handle = window;
  recordScenario("activate:" + std::to_string(window));
  state().actions.push_back(std::move(a));
}

std::vector<uint64_t> tabGroup(uint64_t window) {
  const auto it = state().windows.find(window);
  std::vector<uint64_t> members;
  if (it == state().windows.end() || it->second.group == 0) {
    return members;
  }
  for (uint64_t member : state().tabGroups[it->second.group]) {
    if (liveWindow(member) != nullptr) {
      members.push_back(member);
    }
  }
  return members;
}

std::string windowPath(uint64_t window) {
  const auto it = state().windows.find(window);
  return it == state().windows.end() ? std::string() : it->second.path;
}
void pushOpenPanelAnswer(std::string path) {
  recordScenario("open:" + encodeScenarioText(path));
  state().openAnswers.push_back(std::move(path));
}
void pushSavePanelAnswer(std::string path) {
  recordScenario("save:" + encodeScenarioText(path));
  state().saveAnswers.push_back(std::move(path));
}
void pushAlertAnswer(int32_t button) {
  recordScenario("alert:" + std::to_string(button));
  state().alertAnswers.push_back(button);
}
const std::vector<std::string> &callLog() { return state().log; }

WindowState windowState(uint64_t window) {
  auto it = state().windows.find(window);
  return it == state().windows.end() ? WindowState{} : it->second.state;
}

std::string previewPage(uint64_t window) {
  auto it = state().windows.find(window);
  return it == state().windows.end() ? std::string() : it->second.previewPage;
}

bool isMonospace(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr && v->monospace;
}

bool lineNumbersVisible(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr && v->lineNumbers;
}

bool menuItemChecked(int32_t commandId) {
  for (const auto &entry : state().menus) {
    for (const MenuEntry &item : entry.second.entries) {
      if (item.kind == MenuEntry::Kind::Item && item.id == commandId && entryChecked(item)) {
        return true;
      }
    }
  }
  return false;
}

void setBoolPref(const std::string &key, bool value) {
  recordScenario("pref:" + encodeScenarioText(key) + "=" + (value ? "1" : "0"));
  state().prefs[key] = value;
}

void setIntPref(const std::string &key, int32_t value) {
  recordScenario("pint:" + encodeScenarioText(key) + "=" + std::to_string(value));
  state().intPrefs[key] = value;
}

int32_t intPref(const std::string &key, int32_t fallback) {
  const auto it = state().intPrefs.find(key);
  return it != state().intPrefs.end() ? it->second : fallback;
}

void setPrefList(const std::string &key, const std::vector<std::string> &items) {
  std::string joined;
  for (const std::string &item : items) {
    joined += (joined.empty() ? "" : ",") + encodeScenarioText(item);
  }
  recordScenario("plist:" + encodeScenarioText(key) + "=" + joined);
  state().listPrefs[key] = items;
}

std::vector<std::string> prefList(const std::string &key) {
  const auto it = state().listPrefs.find(key);
  return it != state().listPrefs.end() ? it->second : std::vector<std::string>{};
}

bool boolPref(const std::string &key, bool fallback) {
  const auto it = state().prefs.find(key);
  return it != state().prefs.end() ? it->second : fallback;
}

std::vector<StyleRun> styleRuns(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr ? v->styles : std::vector<StyleRun>{};
}

namespace {

void outlineOf(const std::string &prefix, const Menu &m, std::vector<std::string> &items) {
  for (const MenuEntry &entry : m.entries) {
    std::string title = entry.title;
    std::string shortcut = entry.shortcut;
    if (entry.kind == MenuEntry::Kind::Separator) {
      items.push_back(prefix + "-");
      continue;
    }
    if (entry.kind == MenuEntry::Kind::Submenu) {
      items.push_back(prefix + entry.title + ">");
      if (const Menu *child = menu(entry.child)) {
        outlineOf(prefix + entry.title + ">", *child, items);
      }
      continue;
    }
    if (entry.kind == MenuEntry::Kind::Standard) {
      const StandardItemInfo info = standardItemInfo(entry.id);
      title = info.title;
      shortcut = info.shortcut;
    }
    items.push_back(prefix + title + (shortcut.empty() ? "" : " (" + shortcut + ")"));
  }
}

void itemsOf(const std::string &prefix, const Menu &m, std::vector<std::string> &items) {
  for (const MenuEntry &entry : m.entries) {
    switch (entry.kind) {
    case MenuEntry::Kind::Item:
      items.push_back(prefix + entry.title + (entry.shortcut.empty() ? "" : " (" + entry.shortcut + ")") + " #" +
                      std::to_string(entry.id) + (entryChecked(entry) ? " [checked]" : ""));
      break;
    case MenuEntry::Kind::Separator:
      items.push_back(prefix + "-");
      break;
    case MenuEntry::Kind::Standard:
      items.push_back(prefix + "standard " + std::to_string(entry.id));
      break;
    case MenuEntry::Kind::Submenu:
      items.push_back(prefix + entry.title + ">");
      if (const Menu *child = menu(entry.child)) {
        itemsOf(prefix + entry.title + ">", *child, items);
      }
      break;
    }
  }
}

} // namespace

std::vector<std::string> menuBarOutline() {
  std::vector<std::string> items;
  for (uint64_t handle : state().menuBar) {
    if (const Menu *m = menu(handle)) {
      outlineOf(m->title + ">", *m, items);
    }
  }
  return items;
}

std::vector<std::string> menuBarItems() {
  std::vector<std::string> items;
  for (uint64_t handle : state().menuBar) {
    if (const Menu *m = menu(handle)) {
      itemsOf(m->title + ">", *m, items);
    }
  }
  return items;
}

bool lineWrapping(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr && v->wrapping;
}

void pushRecentFile(const std::string &path) {
  recordScenario("recent:" + encodeScenarioText(path));
  std::vector<std::string> &recents = state().recents;
  recents.erase(std::remove(recents.begin(), recents.end(), path), recents.end());
  recents.insert(recents.begin(), path);
  if (recents.size() > 10) {
    recents.resize(10);
  }
}

std::vector<std::string> recentFiles() { return state().recents; }

} // namespace primec::ui::headless
