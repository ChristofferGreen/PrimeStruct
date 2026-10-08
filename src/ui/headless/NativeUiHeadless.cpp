#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"
#include "primec/ui/NativeUiStandardItems.h"

#include <cerrno>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace primec::ui::headless {
namespace {

struct MenuEntry {
  enum class Kind { Item, Separator, Standard } kind = Kind::Item;
  std::string title;
  std::string shortcut;
  int32_t id = 0;
};

struct Menu {
  std::string title;
  std::vector<MenuEntry> entries;
  bool inBar = false;
};

struct TextView {
  std::string text;
  bool monospace = false;
  bool modified = false;
  uint64_t window = 0;
  std::vector<StyleRun> styles;
};

struct Window {
  WindowState state;
};

struct Action {
  enum class Kind { Command, TypeText, CloseWindow, Quit } kind = Kind::Quit;
  int32_t command = 0;
  uint64_t handle = 0;
  std::string text;
};

struct State {
  bool initialized = false;
  bool quitting = false;
  std::thread::id owner;
  uint64_t nextHandle = 1;
  std::map<uint64_t, Window> windows;
  std::map<uint64_t, TextView> textViews;
  std::map<uint64_t, Menu> menus;
  std::vector<uint64_t> menuBar;
  std::deque<Action> actions;
  std::deque<std::string> openAnswers;
  std::deque<std::string> saveAnswers;
  std::deque<int32_t> alertAnswers;
  int32_t eventKind = PS_UI_EVENT_NONE;
  uint64_t eventWindow = 0;
  uint64_t eventWidget = 0;
  int32_t eventCommand = 0;
  std::string returnedText;
  std::string lastError;
  bool panelChosen = false;
  std::vector<std::string> log;
};

// True when `text` is well-formed UTF-8 (no overlongs, surrogates or values past U+10FFFF).
bool validUtf8(const std::string &text) {
  size_t i = 0;
  const size_t size = text.size();
  auto at = [&](size_t index) { return static_cast<unsigned char>(text[index]); };
  while (i < size) {
    const unsigned char lead = at(i);
    size_t extra = 0;
    uint32_t value = 0;
    if (lead < 0x80) {
      ++i;
      continue;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
      extra = 1;
      value = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      extra = 2;
      value = lead & 0x0Fu;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      extra = 3;
      value = lead & 0x07u;
    } else {
      return false;
    }
    if (i + extra >= size) {
      return false;
    }
    for (size_t k = 1; k <= extra; ++k) {
      const unsigned char next = at(i + k);
      if ((next & 0xC0u) != 0x80u) {
        return false;
      }
      value = (value << 6) | (next & 0x3Fu);
    }
    if ((extra == 2 && value < 0x800) || (extra == 3 && value < 0x10000) || value > 0x10FFFF ||
        (value >= 0xD800 && value <= 0xDFFF)) {
      return false;
    }
    i += extra + 1;
  }
  return true;
}

State &state() {
  static State instance;
  return instance;
}

std::string quote(const char *text) {
  std::string out = "\"";
  for (const char *p = text ? text : ""; *p != '\0'; ++p) {
    switch (*p) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    default:
      out += *p;
    }
  }
  return out + "\"";
}

std::string quote(const std::string &text) { return quote(text.c_str()); }

std::string flag(bool value) { return value ? "true" : "false"; }

const char *eventName(int32_t kind) {
  switch (kind) {
  case PS_UI_EVENT_COMMAND:
    return "command";
  case PS_UI_EVENT_WINDOW_CLOSE_REQUESTED:
    return "window_close_requested";
  case PS_UI_EVENT_TEXT_CHANGED:
    return "text_changed";
  case PS_UI_EVENT_QUIT_REQUESTED:
    return "quit_requested";
  default:
    return "none";
  }
}

// Logs one call and passes its result through.
template <class T> T logged(const std::string &call, T result, const std::string &shown) {
  state().log.push_back(call + " -> " + shown);
  return result;
}

bool loggedFlag(const std::string &call, bool result) { return logged(call, result, flag(result)); }

uint64_t loggedHandle(const std::string &call, uint64_t result) {
  return logged(call, result, std::to_string(result));
}

// True when the app is running and the call comes from its thread.
bool usable() {
  const State &s = state();
  return s.initialized && s.owner == std::this_thread::get_id();
}

Window *liveWindow(uint64_t handle) {
  auto it = state().windows.find(handle);
  return it != state().windows.end() && !it->second.state.closed ? &it->second : nullptr;
}

TextView *textView(uint64_t handle) {
  auto it = state().textViews.find(handle);
  return it != state().textViews.end() ? &it->second : nullptr;
}

Menu *menu(uint64_t handle) {
  auto it = state().menus.find(handle);
  return it != state().menus.end() ? &it->second : nullptr;
}

void setEvent(int32_t kind, uint64_t window = 0, uint64_t widget = 0, int32_t command = 0) {
  State &s = state();
  s.eventKind = kind;
  s.eventWindow = window;
  s.eventWidget = widget;
  s.eventCommand = command;
}

// Applies queued actions until one produces an event. Returns false when the
// queue ran dry.
bool deliverNextEvent() {
  State &s = state();
  while (!s.actions.empty()) {
    const Action action = std::move(s.actions.front());
    s.actions.pop_front();
    switch (action.kind) {
    case Action::Kind::Command:
      setEvent(PS_UI_EVENT_COMMAND, 0, 0, action.command);
      return true;
    case Action::Kind::Quit:
      setEvent(PS_UI_EVENT_QUIT_REQUESTED);
      return true;
    case Action::Kind::CloseWindow:
      if (liveWindow(action.handle) != nullptr) {
        setEvent(PS_UI_EVENT_WINDOW_CLOSE_REQUESTED, action.handle);
        return true;
      }
      break;
    case Action::Kind::TypeText: {
      uint64_t handle = action.handle;
      if (handle == 0 && !s.textViews.empty()) {
        handle = s.textViews.begin()->first;
      }
      TextView *view = textView(handle);
      if (view != nullptr) {
        view->text += action.text;
        view->modified = true;
        setEvent(PS_UI_EVENT_TEXT_CHANGED, view->window, handle);
        return true;
      }
      break;
    }
    }
  }
  return false;
}

} // namespace

void reset() { state() = State{}; }
void pushCommand(int32_t commandId) {
  Action a;
  a.kind = Action::Kind::Command;
  a.command = commandId;
  state().actions.push_back(std::move(a));
}
void pushTypeText(uint64_t widget, std::string text) {
  Action a;
  a.kind = Action::Kind::TypeText;
  a.handle = widget;
  a.text = std::move(text);
  state().actions.push_back(std::move(a));
}
void pushCloseWindow(uint64_t window) {
  Action a;
  a.kind = Action::Kind::CloseWindow;
  a.handle = window;
  state().actions.push_back(std::move(a));
}
void pushQuit() { state().actions.push_back(Action{}); }
void pushOpenPanelAnswer(std::string path) { state().openAnswers.push_back(std::move(path)); }
void pushSavePanelAnswer(std::string path) { state().saveAnswers.push_back(std::move(path)); }
void pushAlertAnswer(int32_t button) { state().alertAnswers.push_back(button); }
const std::vector<std::string> &callLog() { return state().log; }

WindowState windowState(uint64_t window) {
  auto it = state().windows.find(window);
  return it == state().windows.end() ? WindowState{} : it->second.state;
}

bool isMonospace(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr && v->monospace;
}

std::vector<StyleRun> styleRuns(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr ? v->styles : std::vector<StyleRun>{};
}

std::vector<std::string> menuBarOutline() {
  std::vector<std::string> items;
  for (uint64_t handle : state().menuBar) {
    const Menu *m = menu(handle);
    if (m == nullptr) {
      continue;
    }
    for (const MenuEntry &entry : m->entries) {
      std::string title = entry.title;
      std::string shortcut = entry.shortcut;
      if (entry.kind == MenuEntry::Kind::Separator) {
        items.push_back(m->title + ">-");
        continue;
      }
      if (entry.kind == MenuEntry::Kind::Standard) {
        const StandardItemInfo info = standardItemInfo(entry.id);
        title = info.title;
        shortcut = info.shortcut;
      }
      items.push_back(m->title + ">" + title + (shortcut.empty() ? "" : " (" + shortcut + ")"));
    }
  }
  return items;
}

std::vector<std::string> menuBarItems() {
  std::vector<std::string> items;
  for (uint64_t handle : state().menuBar) {
    const Menu *m = menu(handle);
    if (m == nullptr) {
      continue;
    }
    for (const MenuEntry &entry : m->entries) {
      switch (entry.kind) {
      case MenuEntry::Kind::Item:
        items.push_back(m->title + ">" + entry.title + (entry.shortcut.empty() ? "" : " (" + entry.shortcut + ")") +
                        " #" + std::to_string(entry.id));
        break;
      case MenuEntry::Kind::Separator:
        items.push_back(m->title + ">-");
        break;
      case MenuEntry::Kind::Standard:
        items.push_back(m->title + ">standard " + std::to_string(entry.id));
        break;
      }
    }
  }
  return items;
}

} // namespace primec::ui::headless

// ---------------------------------------------------------------------------
// The C ABI.

using namespace primec::ui::headless;

extern "C" {

int32_t ps_ui_abi_version(void) {
  return logged("ps_ui_abi_version()", PS_UI_ABI_VERSION, std::to_string(PS_UI_ABI_VERSION));
}

bool ps_ui_init(const char *appName) {
  State &s = state();
  const std::string call = "ps_ui_init(" + quote(appName) + ")";
  if (s.initialized) {
    return loggedFlag(call, false);
  }
  s.initialized = true;
  s.owner = std::this_thread::get_id();
  return loggedFlag(call, true);
}

void ps_ui_quit(void) {
  const bool ok = usable();
  if (ok) {
    state().quitting = true;
  }
  logged("ps_ui_quit()", 0, ok ? "ok" : "failed");
}

int32_t ps_ui_platform(void) { return logged("ps_ui_platform()", PS_UI_PLATFORM_HEADLESS, "headless"); }

int32_t ps_ui_wait_event(void) {
  State &s = state();
  if (!usable()) {
    return logged("ps_ui_wait_event()", static_cast<int32_t>(PS_UI_EVENT_NONE), "none");
  }
  if (s.quitting || !deliverNextEvent()) {
    setEvent(PS_UI_EVENT_QUIT_REQUESTED);
  }
  return logged("ps_ui_wait_event()", s.eventKind, eventName(s.eventKind));
}

uint64_t ps_ui_event_window(void) {
  return loggedHandle("ps_ui_event_window()", usable() ? state().eventWindow : 0);
}

uint64_t ps_ui_event_widget(void) {
  return loggedHandle("ps_ui_event_widget()", usable() ? state().eventWidget : 0);
}

int32_t ps_ui_event_command(void) {
  const int32_t value = usable() ? state().eventCommand : 0;
  return logged("ps_ui_event_command()", value, std::to_string(value));
}

uint64_t ps_ui_window_create(const char *title, int32_t width, int32_t height) {
  const std::string call =
      "ps_ui_window_create(" + quote(title) + ", " + std::to_string(width) + ", " + std::to_string(height) + ")";
  if (!usable() || width <= 0 || height <= 0) {
    return loggedHandle(call, 0);
  }
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  Window &window = s.windows[handle];
  window.state.exists = true;
  window.state.title = title ? title : "";
  window.state.width = width;
  window.state.height = height;
  return loggedHandle(call, handle);
}

bool ps_ui_window_set_title(uint64_t window, const char *title) {
  const std::string call = "ps_ui_window_set_title(" + std::to_string(window) + ", " + quote(title) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->state.title = title ? title : "";
  }
  return loggedFlag(call, w != nullptr);
}

bool ps_ui_window_set_content(uint64_t window, uint64_t widget) {
  const std::string call =
      "ps_ui_window_set_content(" + std::to_string(window) + ", " + std::to_string(widget) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  TextView *view = usable() ? textView(widget) : nullptr;
  if (w == nullptr || view == nullptr || (view->window != 0 && view->window != window)) {
    return loggedFlag(call, false);
  }
  if (w->state.content != 0 && w->state.content != widget) {
    if (TextView *old = textView(w->state.content)) {
      old->window = 0;
    }
  }
  w->state.content = widget;
  view->window = window;
  return loggedFlag(call, true);
}

bool ps_ui_window_set_edited(uint64_t window, bool edited) {
  const std::string call = "ps_ui_window_set_edited(" + std::to_string(window) + ", " + flag(edited) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->state.edited = edited;
  }
  return loggedFlag(call, w != nullptr);
}

bool ps_ui_window_show(uint64_t window) {
  const std::string call = "ps_ui_window_show(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->state.shown = true;
  }
  return loggedFlag(call, w != nullptr);
}

bool ps_ui_window_close(uint64_t window) {
  const std::string call = "ps_ui_window_close(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->state.closed = true;
    w->state.shown = false;
    if (TextView *content = textView(w->state.content)) {
      content->window = 0;
    }
  }
  return loggedFlag(call, w != nullptr);
}

uint64_t ps_ui_text_view_create(void) {
  if (!usable()) {
    return loggedHandle("ps_ui_text_view_create()", 0);
  }
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  s.textViews[handle];
  return loggedHandle("ps_ui_text_view_create()", handle);
}

const char *ps_ui_text_view_get_text(uint64_t view) {
  State &s = state();
  const TextView *v = usable() ? textView(view) : nullptr;
  s.returnedText = v != nullptr ? v->text : "";
  return logged("ps_ui_text_view_get_text(" + std::to_string(view) + ")", s.returnedText.c_str(),
                quote(s.returnedText));
}

bool ps_ui_text_view_set_text(uint64_t view, const char *text) {
  const std::string call = "ps_ui_text_view_set_text(" + std::to_string(view) + ", " + quote(text) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->text = text ? text : "";
    v->modified = false;
    v->styles.clear();
  }
  return loggedFlag(call, v != nullptr);
}

bool ps_ui_text_view_clear_styles(uint64_t view) {
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->styles.clear();
  }
  return loggedFlag("ps_ui_text_view_clear_styles(" + std::to_string(view) + ")", v != nullptr);
}

bool ps_ui_text_view_add_style(uint64_t view, int32_t startByte, int32_t endByte, int32_t rgb, int32_t flags) {
  const std::string call = "ps_ui_text_view_add_style(" + std::to_string(view) + ", " + std::to_string(startByte) +
                           ", " + std::to_string(endByte) + ", " + std::to_string(rgb) + ", " +
                           std::to_string(flags) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  bool ok = false;
  if (v != nullptr && startByte >= 0 && endByte > startByte && static_cast<size_t>(endByte) <= v->text.size()) {
    // A code point boundary is the end of the text or a byte that is not a continuation byte.
    auto boundary = [&](int32_t at) {
      return static_cast<size_t>(at) == v->text.size() || (static_cast<unsigned char>(v->text[at]) & 0xC0) != 0x80;
    };
    ok = boundary(startByte) && boundary(endByte);
    if (ok) {
      v->styles.push_back(StyleRun{startByte, endByte, rgb, flags});
    }
  }
  return loggedFlag(call, ok);
}

bool ps_ui_text_view_set_monospace(uint64_t view, bool monospace) {
  const std::string call = "ps_ui_text_view_set_monospace(" + std::to_string(view) + ", " + flag(monospace) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->monospace = monospace;
  }
  return loggedFlag(call, v != nullptr);
}

bool ps_ui_text_view_is_modified(uint64_t view) {
  const TextView *v = usable() ? textView(view) : nullptr;
  return loggedFlag("ps_ui_text_view_is_modified(" + std::to_string(view) + ")", v != nullptr && v->modified);
}

bool ps_ui_text_view_clear_modified(uint64_t view) {
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->modified = false;
  }
  return loggedFlag("ps_ui_text_view_clear_modified(" + std::to_string(view) + ")", v != nullptr);
}

uint64_t ps_ui_menu_create(const char *title) {
  const std::string call = "ps_ui_menu_create(" + quote(title) + ")";
  if (!usable()) {
    return loggedHandle(call, 0);
  }
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  s.menus[handle].title = title ? title : "";
  return loggedHandle(call, handle);
}

bool ps_ui_menu_add_item(uint64_t menuHandle, const char *title, const char *shortcut, int32_t commandId) {
  const std::string call = "ps_ui_menu_add_item(" + std::to_string(menuHandle) + ", " + quote(title) + ", " +
                           quote(shortcut) + ", " + std::to_string(commandId) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  if (m != nullptr) {
    MenuEntry entry;
    entry.title = title ? title : "";
    entry.shortcut = shortcut ? shortcut : "";
    entry.id = commandId;
    m->entries.push_back(std::move(entry));
  }
  return loggedFlag(call, m != nullptr);
}

bool ps_ui_menu_add_separator(uint64_t menuHandle) {
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  if (m != nullptr) {
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::Separator;
    m->entries.push_back(std::move(entry));
  }
  return loggedFlag("ps_ui_menu_add_separator(" + std::to_string(menuHandle) + ")", m != nullptr);
}

bool ps_ui_menu_add_standard(uint64_t menuHandle, int32_t standardId) {
  const std::string call =
      "ps_ui_menu_add_standard(" + std::to_string(menuHandle) + ", " + std::to_string(standardId) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  const bool valid = standardId >= PS_UI_STANDARD_UNDO && standardId <= PS_UI_STANDARD_HELP;
  if (m != nullptr && valid) {
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::Standard;
    entry.id = standardId;
    m->entries.push_back(std::move(entry));
  }
  return loggedFlag(call, m != nullptr && valid);
}

bool ps_ui_menu_bar_add(uint64_t menuHandle) {
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  const bool ok = m != nullptr && !m->inBar;
  if (ok) {
    m->inBar = true;
    state().menuBar.push_back(menuHandle);
  }
  return loggedFlag("ps_ui_menu_bar_add(" + std::to_string(menuHandle) + ")", ok);
}

bool ps_ui_menu_bar_add_role(uint64_t menuHandle, int32_t role) {
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  const bool ok = m != nullptr && !m->inBar && role >= PS_UI_MENU_ROLE_APP && role <= PS_UI_MENU_ROLE_HELP;
  if (ok) {
    m->inBar = true;
    auto &bar = state().menuBar;
    bar.insert(role == PS_UI_MENU_ROLE_APP ? bar.begin() : bar.end(), menuHandle);
  }
  return loggedFlag("ps_ui_menu_bar_add_role(" + std::to_string(menuHandle) + ", " + std::to_string(role) + ")", ok);
}

bool ps_ui_text_view_load_file(uint64_t view, const char *path) {
  State &s = state();
  const std::string call = "ps_ui_text_view_load_file(" + std::to_string(view) + ", " + quote(path) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v == nullptr) {
    s.lastError = "invalid text view";
    return loggedFlag(call, false);
  }
  std::ifstream file(path != nullptr ? path : "", std::ios::binary);
  if (!file) {
    s.lastError = std::string("cannot open ") + (path != nullptr ? path : "") + ": " + std::strerror(errno);
    return loggedFlag(call, false);
  }
  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (!validUtf8(contents)) {
    s.lastError = std::string(path != nullptr ? path : "") + " is not valid UTF-8 text";
    return loggedFlag(call, false);
  }
  v->text = std::move(contents);
  v->modified = false;
  v->styles.clear();
  return loggedFlag(call, true);
}

bool ps_ui_text_view_save_file(uint64_t view, const char *path) {
  State &s = state();
  const std::string call = "ps_ui_text_view_save_file(" + std::to_string(view) + ", " + quote(path) + ")";
  const TextView *v = usable() ? textView(view) : nullptr;
  if (v == nullptr) {
    s.lastError = "invalid text view";
    return loggedFlag(call, false);
  }
  std::ofstream file(path != nullptr ? path : "", std::ios::binary | std::ios::trunc);
  if (!file) {
    s.lastError = std::string("cannot write ") + (path != nullptr ? path : "") + ": " + std::strerror(errno);
    return loggedFlag(call, false);
  }
  file.write(v->text.data(), static_cast<std::streamsize>(v->text.size()));
  file.close();
  if (!file) {
    s.lastError = std::string("cannot write ") + (path != nullptr ? path : "") + ": " + std::strerror(errno);
    return loggedFlag(call, false);
  }
  return loggedFlag(call, true);
}

const char *ps_ui_last_error(void) {
  State &s = state();
  return logged("ps_ui_last_error()", s.lastError.c_str(), quote(s.lastError));
}

const char *ps_ui_open_panel(const char *title) {
  State &s = state();
  s.returnedText.clear();
  if (usable() && !s.openAnswers.empty()) {
    s.returnedText = std::move(s.openAnswers.front());
    s.openAnswers.pop_front();
  }
  s.panelChosen = !s.returnedText.empty();
  return logged("ps_ui_open_panel(" + quote(title) + ")", s.returnedText.c_str(), quote(s.returnedText));
}

const char *ps_ui_save_panel(const char *title, const char *suggestedName) {
  State &s = state();
  s.returnedText.clear();
  if (usable() && !s.saveAnswers.empty()) {
    s.returnedText = std::move(s.saveAnswers.front());
    s.saveAnswers.pop_front();
  }
  s.panelChosen = !s.returnedText.empty();
  return logged("ps_ui_save_panel(" + quote(title) + ", " + quote(suggestedName) + ")", s.returnedText.c_str(),
                quote(s.returnedText));
}

bool ps_ui_panel_chosen(void) {
  return loggedFlag("ps_ui_panel_chosen()", state().panelChosen);
}

int32_t ps_ui_alert(const char *message, const char *detail, const char *buttons) {
  State &s = state();
  const std::string call = "ps_ui_alert(" + quote(message) + ", " + quote(detail) + ", " + quote(buttons) + ")";
  if (!usable()) {
    return logged(call, -1, "-1");
  }
  int32_t buttonCount = 1;
  for (const char *p = buttons ? buttons : ""; *p != '\0'; ++p) {
    buttonCount += *p == '\n' ? 1 : 0;
  }
  int32_t answer = 0;
  if (!s.alertAnswers.empty()) {
    answer = s.alertAnswers.front();
    s.alertAnswers.pop_front();
  }
  if (answer < 0 || answer >= buttonCount) {
    answer = 0;
  }
  return logged(call, answer, std::to_string(answer));
}

} // extern "C"
