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

void pushToggleCheckBox(uint64_t checkBox) {
  recordScenario("check:" + std::to_string(checkBox));
  Action a;
  a.kind = Action::Kind::Toggle;
  a.handle = checkBox;
  state().actions.push_back(std::move(a));
}

void pushSelectPopupItem(uint64_t popup, int32_t index) {
  recordScenario("popup:" + std::to_string(popup) + ":" + std::to_string(index));
  Action a;
  a.kind = Action::Kind::Select;
  a.handle = popup;
  a.command = index;
  state().actions.push_back(std::move(a));
}

int32_t fontSize(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr ? v->fontSize : 0;
}

bool previewScrollLock() { return state().scrollLock; }

bool checkBoxChecked(uint64_t checkBox) {
  const Control *c = control(checkBox);
  return c != nullptr && c->kind == Control::Kind::CheckBox && c->checked;
}

int32_t popupSelected(uint64_t popup) {
  const Control *c = control(popup);
  return c != nullptr && c->kind == Control::Kind::Popup ? c->selected : -1;
}

namespace {

std::string leafOutline(const Control &c) {
  switch (c.kind) {
  case Control::Kind::Label:
    return "label " + quote(c.text);
  case Control::Kind::CheckBox:
    return "checkbox " + quote(c.text) + (c.checked ? " [checked]" : "");
  case Control::Kind::Popup: {
    std::string items;
    for (size_t i = 0; i < c.items.size(); ++i) {
      items += (i == 0 ? "" : "|") + c.items[i];
    }
    return "popup [" + items + "] selected=" + std::to_string(c.selected);
  }
  default:
    return "";
  }
}

void describeControl(uint64_t handle, const std::string &indent, const std::string &prefix,
                     std::vector<std::string> &out) {
  const Control *c = control(handle);
  if (c == nullptr) {
    out.push_back(indent + prefix + "(none)");
  } else if (c->kind == Control::Kind::Form) {
    out.push_back(indent + prefix + "form");
    for (const Control::Row &row : c->rows) {
      switch (row.kind) {
      case Control::Row::Kind::Heading:
        out.push_back(indent + "  heading " + quote(row.label));
        break;
      case Control::Row::Kind::Labelled:
        describeControl(row.control, indent + "  ", "row " + quote(row.label) + ": ", out);
        break;
      case Control::Row::Kind::Plain:
        describeControl(row.control, indent + "  ", "control: ", out);
        break;
      }
    }
  } else if (c->kind == Control::Kind::Tabs) {
    out.push_back(indent + prefix + "tabs selected=" + std::to_string(c->selected));
    for (const Control::Page &page : c->pages) {
      out.push_back(indent + "  page " + quote(page.title) + ":");
      describeControl(page.content, indent + "    ", "", out);
    }
  } else {
    out.push_back(indent + prefix + leafOutline(*c));
  }
}

} // namespace

std::vector<std::string> controlOutline(uint64_t root) {
  std::vector<std::string> out;
  describeControl(root, "", "", out);
  return out;
}

} // namespace primec::ui::headless
