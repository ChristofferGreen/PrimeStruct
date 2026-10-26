#pragma once

// The state the headless native UI backend (src/ui/headless/) shares between its translation units:
// the widget tree, the queued actions, the call log, and the helpers every ABI function uses. The ABI
// itself is NativeUiHeadless.cpp, the control surface of primec/ui/NativeUiHeadless.h is
// NativeUiHeadlessControl.cpp. One instance exists per process (`state()`).

#include "primec/ui/Markdown.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"

#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace primec::ui::headless::detail {

struct MenuEntry {
  enum class Kind { Item, Separator, Standard, Submenu } kind = Kind::Item;
  std::string title;
  std::string shortcut;
  int32_t id = 0;
  bool checked = false;
  uint64_t child = 0; // the submenu of a Submenu entry
  bool followsPreview = false; // the check mark shows whether the active window has a preview
};

struct Menu {
  std::string title;
  std::vector<MenuEntry> entries;
  bool inBar = false;
};

// A label, check box, pop-up, form or tabs (the controls of a settings window).
struct Control {
  enum class Kind { Label, CheckBox, Popup, Form, Tabs, Button, List } kind = Kind::Label;
  std::string text;                // a label's text, a check box's title
  bool checked = false;            // a check box
  std::vector<std::string> items;  // a pop-up's or a list's items
  std::set<int32_t> picked;        // a list's selected rows
  int32_t selected = -1;           // a pop-up's chosen item, the page tabs show
  struct Row {
    enum class Kind { Heading, Labelled, Plain } kind = Kind::Plain;
    std::string label;
    uint64_t control = 0;
  };
  std::vector<Row> rows;           // a form's rows
  struct Page {
    std::string title;
    uint64_t content = 0;
  };
  std::vector<Page> pages;         // tabs' pages
  uint64_t parent = 0;             // the form or tabs that holds it
};

// A toolbar: items in order; a segmented control is one item with several segments.
struct ToolbarItem {
  enum class Kind { Button, Segments, Menu, Separator } kind = Kind::Button;
  std::string title;
  std::string symbol;
  int32_t id = 0;                 // a button's command, a segmented control's first command
  std::vector<std::string> segments;
  uint64_t menu = 0;
};

struct Toolbar {
  std::vector<ToolbarItem> items;
};

struct TextView {
  std::string text;
  bool monospace = false;
  bool lineNumbers = false;
  bool wrapping = true;
  bool modified = false;
  int32_t fontSize = 0; // set with ps_ui_text_view_set_font_size
  size_t selectionStart = 0; // byte offsets; a caret when equal
  size_t selectionEnd = 0;
  bool markdownEditing = false; // ps_ui_text_view_set_markdown_editing
  uint64_t window = 0;
  std::vector<StyleRun> styles;
};

struct Window {
  WindowState state;
  std::string previewPage;
  std::string path;
  uint64_t group = 0; // tab group id; 0 when the window is not in a tab group
};

struct Action {
  enum class Kind { Command, TypeText, CloseWindow, Quit, Activate, Toggle, Select, Click, OpenFile, Rows, Selection, TextChanged } kind = Kind::Quit;
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
  std::map<uint64_t, std::vector<uint64_t>> tabGroups; // group id -> windows in tab order
  uint64_t nextGroup = 1;
  uint64_t activeWindow = 0;
  std::map<uint64_t, TextView> textViews;
  std::map<uint64_t, Control> controls;
  int32_t scrollMode = 2; // the preview's scroll mode (PS_UI_SCROLL_BLOCKS)
  std::map<uint64_t, Menu> menus;
  std::map<uint64_t, Toolbar> toolbars;
  std::vector<uint64_t> menuBar;
  std::deque<Action> actions;
  std::deque<std::string> openAnswers;
  std::deque<std::string> saveAnswers;
  std::deque<int32_t> alertAnswers;
  std::map<std::string, std::string> defaultApps;   // extension -> the application that opens it by default
  std::map<std::string, bool> prefs;
  std::map<std::string, int32_t> intPrefs;
  std::map<std::string, std::vector<std::string>> listPrefs;
  std::vector<std::string> recents; // most recent first
  std::string scenario; // every push, in the text form of tests/fixtures/host_calls/scenario.h
  int32_t eventKind = PS_UI_EVENT_NONE;
  uint64_t eventWindow = 0;
  uint64_t eventWidget = 0;
  int32_t eventCommand = 0;
  std::string eventText;
  std::string returnedText;
  std::string lastError;
  bool panelChosen = false;
  std::vector<std::string> log;
};

// True when `text` is well-formed UTF-8 (no overlongs, surrogates or values past U+10FFFF).
inline bool validUtf8(const std::string &text) {
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

inline State &state() {
  static State instance;
  return instance;
}

inline std::string quote(const char *text) {
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

inline std::string quote(const std::string &text) { return quote(text.c_str()); }

inline std::string flag(bool value) { return value ? "true" : "false"; }

// The file types the headless "app" declares: the same list as examples/apps/text_editor/file_types.txt (a test keeps
// them equal).
struct DocumentType {
  const char *name;
  const char *extensions;
};

inline const std::vector<DocumentType> &documentTypes() {
  static const std::vector<DocumentType> types = {
      {"Plain text", "txt,text"},
      {"Markdown", "md,markdown"},
      {"PrimeStruct source", "prime"},
      {"JSON", "json"},
      {"YAML", "yml,yaml"},
      {"Configuration", "toml,ini,cfg,conf"},
      {"C and C++", "c,h,cc,cpp,hpp,cxx"},
      {"Shell scripts", "sh,bash,zsh"},
      {"Python", "py"},
      {"Web scripts and styles", "js,css"},
      {"Logs", "log"},
  };
  return types;
}

inline const char *HeadlessAppPath = "/Headless/PrimeScroll.app";

inline const char *eventName(int32_t kind) {
  switch (kind) {
  case PS_UI_EVENT_COMMAND:
    return "command";
  case PS_UI_EVENT_WINDOW_CLOSE_REQUESTED:
    return "window_close_requested";
  case PS_UI_EVENT_TEXT_CHANGED:
    return "text_changed";
  case PS_UI_EVENT_QUIT_REQUESTED:
    return "quit_requested";
  case PS_UI_EVENT_CONTROL_CHANGED:
    return "control_changed";
  case PS_UI_EVENT_OPEN_FILE:
    return "open_file";
  default:
    return "none";
  }
}

// Logs one call and passes its result through.
template <class T> T logged(const std::string &call, T result, const std::string &shown) {
  state().log.push_back(call + " -> " + shown);
  return result;
}

inline bool loggedFlag(const std::string &call, bool result) { return logged(call, result, flag(result)); }

inline uint64_t loggedHandle(const std::string &call, uint64_t result) {
  return logged(call, result, std::to_string(result));
}

// True when the app is running and the call comes from its thread.
inline bool usable() {
  const State &s = state();
  return s.initialized && s.owner == std::this_thread::get_id();
}

inline Window *liveWindow(uint64_t handle) {
  auto it = state().windows.find(handle);
  return it != state().windows.end() && !it->second.state.closed ? &it->second : nullptr;
}

inline TextView *textView(uint64_t handle) {
  auto it = state().textViews.find(handle);
  return it != state().textViews.end() ? &it->second : nullptr;
}

inline Toolbar *toolbar(uint64_t handle) {
  auto it = state().toolbars.find(handle);
  return it != state().toolbars.end() ? &it->second : nullptr;
}

inline Menu *menu(uint64_t handle) {
  auto it = state().menus.find(handle);
  return it != state().menus.end() ? &it->second : nullptr;
}

inline Control *control(uint64_t handle) {
  auto it = state().controls.find(handle);
  return it != state().controls.end() ? &it->second : nullptr;
}

// The window whose content holds the control (directly or through forms and tabs); 0 when none.
inline uint64_t windowOfControl(uint64_t handle) {
  uint64_t root = handle;
  for (int depth = 0; depth < 64; ++depth) {
    const Control *c = control(root);
    if (c == nullptr || c->parent == 0) {
      break;
    }
    root = c->parent;
  }
  for (const auto &entry : state().windows) {
    if (!entry.second.state.closed && entry.second.state.content == root) {
      return entry.first;
    }
  }
  return 0;
}

inline bool activeWindowHasPreview() {
  const Window *w = liveWindow(state().activeWindow);
  return w != nullptr && w->state.preview;
}

inline bool entryChecked(const MenuEntry &entry) {
  return entry.checked || (entry.followsPreview && activeWindowHasPreview());
}

inline void renderPreview(Window &w, const TextView &view) {
  w.previewPage = primec::ui::markdownPreviewPage(view.text);
  ++w.state.previewRenders;
}

inline void setEvent(int32_t kind, uint64_t window = 0, uint64_t widget = 0, int32_t command = 0,
                     std::string text = std::string()) {
  State &s = state();
  s.eventKind = kind;
  s.eventText = std::move(text);
  s.eventWindow = window;
  s.eventWidget = widget;
  s.eventCommand = command;
}

// Applies queued actions until one produces an event. Returns false when the
// queue ran dry.
inline bool deliverNextEvent() {
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
    case Action::Kind::Activate:
      if (liveWindow(action.handle) != nullptr) {
        s.activeWindow = action.handle;
      }
      break;
    case Action::Kind::CloseWindow:
      if (liveWindow(action.handle) != nullptr) {
        setEvent(PS_UI_EVENT_WINDOW_CLOSE_REQUESTED, action.handle);
        return true;
      }
      break;
    case Action::Kind::Toggle: {
      Control *box = control(action.handle);
      if (box != nullptr && box->kind == Control::Kind::CheckBox) {
        box->checked = !box->checked;
        setEvent(PS_UI_EVENT_CONTROL_CHANGED, windowOfControl(action.handle), action.handle);
        return true;
      }
      break;
    }
    case Action::Kind::Select: {
      Control *popup = control(action.handle);
      if (popup != nullptr && popup->kind == Control::Kind::Popup && action.command >= 0 &&
          static_cast<size_t>(action.command) < popup->items.size()) {
        popup->selected = action.command;
        setEvent(PS_UI_EVENT_CONTROL_CHANGED, windowOfControl(action.handle), action.handle);
        return true;
      }
      break;
    }
    case Action::Kind::Click: {
      const Control *button = control(action.handle);
      if (button != nullptr && button->kind == Control::Kind::Button) {
        setEvent(PS_UI_EVENT_CONTROL_CHANGED, windowOfControl(action.handle), action.handle);
        return true;
      }
      break;
    }
    case Action::Kind::OpenFile:
      setEvent(PS_UI_EVENT_OPEN_FILE, 0, 0, 0, action.text);
      return true;
    case Action::Kind::Rows: {
      Control *list = control(action.handle);
      if (list != nullptr && list->kind == Control::Kind::List) {
        list->picked.clear();
        for (size_t from = 0; from < action.text.size();) {
          size_t comma = action.text.find(',', from);
          comma = comma == std::string::npos ? action.text.size() : comma;
          const int32_t row = std::atoi(action.text.substr(from, comma - from).c_str());
          if (row >= 0 && static_cast<size_t>(row) < list->items.size()) {
            list->picked.insert(row);
          }
          from = comma + 1;
        }
        setEvent(PS_UI_EVENT_CONTROL_CHANGED, windowOfControl(action.handle), action.handle);
        return true;
      }
      break;
    }
    case Action::Kind::Selection: {
      TextView *view = textView(action.handle);
      if (view != nullptr) {
        const size_t size = view->text.size();
        view->selectionStart = std::min(static_cast<size_t>(action.command), size);
        view->selectionEnd = std::min(static_cast<size_t>(std::atoll(action.text.c_str())), size);
      }
      break;
    }
    case Action::Kind::TextChanged: {
      const TextView *view = textView(action.handle);
      if (view != nullptr) {
        setEvent(PS_UI_EVENT_TEXT_CHANGED, view->window, action.handle);
        return true;
      }
      break;
    }
    case Action::Kind::TypeText: {
      uint64_t handle = action.handle;
      if (handle == 0 && !s.textViews.empty()) {
        handle = s.textViews.begin()->first;
      }
      TextView *view = textView(handle);
      if (view != nullptr) {
        view->text += action.text;
        view->selectionStart = view->selectionEnd = view->text.size();
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

} // namespace primec::ui::headless::detail
