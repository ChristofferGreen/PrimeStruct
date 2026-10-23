#include "NativeUiHeadlessState.h"

#include "primec/ui/Markdown.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"
#include "primec/ui/NativeUiRecentFiles.h"
#include "primec/ui/NativeUiStandardItems.h"
#include "primec/ui/SyntaxHighlight.h"

#include <algorithm>
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

// ---------------------------------------------------------------------------
// The C ABI.

using namespace primec::ui::headless;
using namespace primec::ui::headless::detail;

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
    state().activeWindow = window;
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
    State &s = state();
    if (s.activeWindow == window) {
      // Like the platform: the neighbour tab, else the newest window still open.
      s.activeWindow = 0;
      const std::vector<uint64_t> neighbours = primec::ui::headless::tabGroup(window);
      if (!neighbours.empty()) {
        s.activeWindow = neighbours.back();
      } else {
        for (const auto &[handle, other] : s.windows) {
          if (!other.state.closed) {
            s.activeWindow = handle;
          }
        }
      }
    }
  }
  return loggedFlag(call, w != nullptr);
}

uint64_t ps_ui_active_window(void) {
  const State &s = state();
  const uint64_t handle = usable() && liveWindow(s.activeWindow) != nullptr ? s.activeWindow : 0;
  return loggedHandle("ps_ui_active_window()", handle);
}

int32_t ps_ui_window_tab_count(uint64_t window) {
  int32_t count = 0;
  if (usable() && liveWindow(window) != nullptr) {
    const std::vector<uint64_t> group = primec::ui::headless::tabGroup(window);
    count = group.empty() ? 1 : static_cast<int32_t>(group.size());
  }
  return logged("ps_ui_window_tab_count(" + std::to_string(window) + ")", count, std::to_string(count));
}

uint64_t ps_ui_window_tab_at(uint64_t window, int32_t index) {
  uint64_t found = 0;
  if (usable() && liveWindow(window) != nullptr && index >= 0) {
    std::vector<uint64_t> group = primec::ui::headless::tabGroup(window);
    if (group.empty()) {
      group.push_back(window);
    }
    found = static_cast<size_t>(index) < group.size() ? group[static_cast<size_t>(index)] : 0;
  }
  return loggedHandle("ps_ui_window_tab_at(" + std::to_string(window) + ", " + std::to_string(index) + ")", found);
}

bool ps_ui_window_select(uint64_t window) {
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    state().activeWindow = window;
  }
  return loggedFlag("ps_ui_window_select(" + std::to_string(window) + ")", w != nullptr);
}

uint64_t ps_ui_window_content(uint64_t window) {
  const Window *w = usable() ? liveWindow(window) : nullptr;
  return loggedHandle("ps_ui_window_content(" + std::to_string(window) + ")", w != nullptr ? w->state.content : 0);
}

int32_t ps_ui_window_count(void) {
  int32_t count = 0;
  if (usable()) {
    for (const auto &entry : state().windows) {
      count += entry.second.state.closed ? 0 : 1;
    }
  }
  return logged("ps_ui_window_count()", count, std::to_string(count));
}

uint64_t ps_ui_window_at(int32_t index) {
  uint64_t found = 0;
  if (usable() && index >= 0) {
    int32_t seen = 0;
    for (const auto &entry : state().windows) {
      if (!entry.second.state.closed && seen++ == index) {
        found = entry.first;
        break;
      }
    }
  }
  return loggedHandle("ps_ui_window_at(" + std::to_string(index) + ")", found);
}

bool ps_ui_window_add_tab(uint64_t host, uint64_t tab) {
  const std::string call = "ps_ui_window_add_tab(" + std::to_string(host) + ", " + std::to_string(tab) + ")";
  State &s = state();
  Window *h = usable() ? liveWindow(host) : nullptr;
  Window *t = usable() ? liveWindow(tab) : nullptr;
  const bool ok = h != nullptr && t != nullptr && host != tab && t->group == 0;
  if (ok) {
    if (h->group == 0) {
      h->group = s.nextGroup++;
      s.tabGroups[h->group].push_back(host);
    }
    t->group = h->group;
    s.tabGroups[h->group].push_back(tab);
    s.activeWindow = tab;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_window_set_path(uint64_t window, const char *path) {
  const std::string call = "ps_ui_window_set_path(" + std::to_string(window) + ", " + quote(path) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  if (w != nullptr) {
    w->path = path != nullptr ? path : "";
  }
  return loggedFlag(call, w != nullptr);
}

const char *ps_ui_window_path(uint64_t window) {
  State &s = state();
  const Window *w = usable() ? liveWindow(window) : nullptr;
  s.returnedText = w != nullptr ? w->path : "";
  return logged("ps_ui_window_path(" + std::to_string(window) + ")", s.returnedText.c_str(), quote(s.returnedText));
}

bool ps_ui_window_has_path(uint64_t window) {
  const Window *w = usable() ? liveWindow(window) : nullptr;
  return loggedFlag("ps_ui_window_has_path(" + std::to_string(window) + ")", w != nullptr && !w->path.empty());
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

bool ps_ui_text_view_highlight(uint64_t view, const char *path) {
  const std::string call = "ps_ui_text_view_highlight(" + std::to_string(view) + ", " + quote(path) + ")";
  const bool ok = usable() && textView(view) != nullptr && primec::ui::applyHighlight(view, path);
  return loggedFlag(call, ok);
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

bool ps_ui_text_view_set_line_numbers(uint64_t view, bool visible) {
  const std::string call = "ps_ui_text_view_set_line_numbers(" + std::to_string(view) + ", " + flag(visible) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->lineNumbers = visible;
  }
  return loggedFlag(call, v != nullptr);
}

bool ps_ui_text_view_set_wrapping(uint64_t view, bool wrap) {
  const std::string call = "ps_ui_text_view_set_wrapping(" + std::to_string(view) + ", " + flag(wrap) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->wrapping = wrap;
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

bool ps_ui_menu_set_checked(uint64_t menuHandle, int32_t commandId, bool checked) {
  const std::string call = "ps_ui_menu_set_checked(" + std::to_string(menuHandle) + ", " + std::to_string(commandId) +
                           ", " + flag(checked) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  bool found = false;
  if (m != nullptr) {
    for (MenuEntry &entry : m->entries) {
      if (entry.kind == MenuEntry::Kind::Item && entry.id == commandId) {
        entry.checked = checked;
        found = true;
      }
    }
  }
  return loggedFlag(call, found);
}

bool ps_ui_menu_add_submenu(uint64_t menuHandle, const char *title, uint64_t submenuHandle) {
  const std::string call = "ps_ui_menu_add_submenu(" + std::to_string(menuHandle) + ", " + quote(title) + ", " +
                           std::to_string(submenuHandle) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  Menu *child = usable() ? menu(submenuHandle) : nullptr;
  bool attached = false;
  for (const auto &entry : state().menus) {
    for (const MenuEntry &item : entry.second.entries) {
      attached = attached || (item.kind == MenuEntry::Kind::Submenu && item.child == submenuHandle);
    }
  }
  const bool ok = m != nullptr && child != nullptr && menuHandle != submenuHandle && !child->inBar && !attached;
  if (ok) {
    MenuEntry entry;
    entry.kind = MenuEntry::Kind::Submenu;
    entry.title = title ? title : "";
    entry.child = submenuHandle;
    m->entries.push_back(std::move(entry));
  }
  return loggedFlag(call, ok);
}

bool ps_ui_menu_clear(uint64_t menuHandle) {
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  if (m != nullptr) {
    m->entries.clear();
  }
  return loggedFlag("ps_ui_menu_clear(" + std::to_string(menuHandle) + ")", m != nullptr);
}

int32_t ps_ui_menu_add_recent_files(uint64_t menuHandle, int32_t firstCommandId, int32_t maxItems) {
  const std::string call = "ps_ui_menu_add_recent_files(" + std::to_string(menuHandle) + ", " +
                           std::to_string(firstCommandId) + ", " + std::to_string(maxItems) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  int32_t added = 0;
  if (m != nullptr && maxItems > 0) {
    const std::vector<std::string> &recents = state().recents;
    for (size_t i = 0; i < recents.size() && static_cast<int32_t>(i) < maxItems; ++i) {
      MenuEntry entry;
      entry.title = primec::ui::recentFileTitle(recents, i);
      entry.id = firstCommandId + static_cast<int32_t>(i);
      m->entries.push_back(std::move(entry));
      ++added;
    }
  }
  return logged(call, added, std::to_string(added));
}

bool ps_ui_recent_add(const char *path) {
  const std::string call = "ps_ui_recent_add(" + quote(path) + ")";
  const bool ok = usable() && path != nullptr && *path != '\0';
  if (ok) {
    std::vector<std::string> &recents = state().recents;
    recents.erase(std::remove(recents.begin(), recents.end(), std::string(path)), recents.end());
    recents.insert(recents.begin(), path);
    if (recents.size() > 10) {
      recents.resize(10);
    }
  }
  return loggedFlag(call, ok);
}

const char *ps_ui_recent_path(int32_t index) {
  State &s = state();
  s.returnedText = usable() && index >= 0 && static_cast<size_t>(index) < s.recents.size()
                       ? s.recents[static_cast<size_t>(index)]
                       : std::string();
  s.log.push_back("ps_ui_recent_path(" + std::to_string(index) + ") -> " + quote(s.returnedText));
  return s.returnedText.c_str();
}

bool ps_ui_recent_clear(void) {
  if (usable()) {
    state().recents.clear();
  }
  return loggedFlag("ps_ui_recent_clear()", usable());
}

bool ps_ui_window_set_autosave_name(uint64_t window, const char *name) {
  const std::string call = "ps_ui_window_set_autosave_name(" + std::to_string(window) + ", " + quote(name) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  const bool ok = w != nullptr && name != nullptr && *name != '\0';
  if (ok) {
    w->state.autosaveName = name;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_window_set_preview(uint64_t window, bool on) {
  const std::string call = "ps_ui_window_set_preview(" + std::to_string(window) + ", " + flag(on) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  TextView *view = w != nullptr ? textView(w->state.content) : nullptr;
  const bool ok = w != nullptr && (view != nullptr || !on);
  if (ok && on && !w->state.preview) {
    w->state.preview = true;
    renderPreview(*w, *view);
  } else if (ok && !on) {
    w->state.preview = false;
    w->previewPage.clear();
  }
  return loggedFlag(call, ok);
}

bool ps_ui_window_has_preview(uint64_t window) {
  const std::string call = "ps_ui_window_has_preview(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  return loggedFlag(call, w != nullptr && w->state.preview);
}

bool ps_ui_window_refresh_preview(uint64_t window) {
  const std::string call = "ps_ui_window_refresh_preview(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  TextView *view = w != nullptr ? textView(w->state.content) : nullptr;
  const bool ok = w != nullptr && w->state.preview && view != nullptr;
  if (ok) {
    renderPreview(*w, *view);
  }
  return loggedFlag(call, ok);
}

bool ps_ui_menu_follow_preview(uint64_t menuHandle, int32_t commandId) {
  const std::string call = "ps_ui_menu_follow_preview(" + std::to_string(menuHandle) + ", " +
                           std::to_string(commandId) + ")";
  Menu *m = usable() ? menu(menuHandle) : nullptr;
  bool found = false;
  if (m != nullptr) {
    for (MenuEntry &entry : m->entries) {
      if (entry.kind == MenuEntry::Kind::Item && entry.id == commandId) {
        entry.followsPreview = true;
        found = true;
      }
    }
  }
  return loggedFlag(call, found);
}

int32_t ps_ui_pref_get_int(const char *key, int32_t fallback) {
  const std::string call = "ps_ui_pref_get_int(" + quote(key) + ", " + std::to_string(fallback) + ")";
  const int32_t value = usable() && key != nullptr && *key != '\0' ? intPref(key, fallback) : fallback;
  return logged(call, value, std::to_string(value));
}

bool ps_ui_pref_set_int(const char *key, int32_t value) {
  const std::string call = "ps_ui_pref_set_int(" + quote(key) + ", " + std::to_string(value) + ")";
  const bool ok = usable() && key != nullptr && *key != '\0';
  if (ok) {
    state().intPrefs[key] = value;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_pref_list_clear(const char *key) {
  const std::string call = "ps_ui_pref_list_clear(" + quote(key) + ")";
  const bool ok = usable() && key != nullptr && *key != '\0';
  if (ok) {
    state().listPrefs[key].clear();
  }
  return loggedFlag(call, ok);
}

bool ps_ui_pref_list_add(const char *key, const char *text) {
  const std::string call = "ps_ui_pref_list_add(" + quote(key) + ", " + quote(text) + ")";
  const bool ok = usable() && key != nullptr && *key != '\0' && text != nullptr;
  if (ok) {
    state().listPrefs[key].push_back(text);
  }
  return loggedFlag(call, ok);
}

int32_t ps_ui_pref_list_count(const char *key) {
  const std::string call = "ps_ui_pref_list_count(" + quote(key) + ")";
  const auto it = usable() && key != nullptr ? state().listPrefs.find(key) : state().listPrefs.end();
  const int32_t count = it != state().listPrefs.end() ? static_cast<int32_t>(it->second.size()) : 0;
  return logged(call, count, std::to_string(count));
}

const char *ps_ui_pref_list_get(const char *key, int32_t index) {
  State &s = state();
  const auto it = usable() && key != nullptr ? s.listPrefs.find(key) : s.listPrefs.end();
  s.returnedText = it != s.listPrefs.end() && index >= 0 && static_cast<size_t>(index) < it->second.size()
                       ? it->second[static_cast<size_t>(index)]
                       : std::string();
  s.log.push_back("ps_ui_pref_list_get(" + quote(key) + ", " + std::to_string(index) + ") -> " + quote(s.returnedText));
  return s.returnedText.c_str();
}

bool ps_ui_pref_get_bool(const char *key, bool fallback) {
  const std::string call = "ps_ui_pref_get_bool(" + quote(key) + ", " + flag(fallback) + ")";
  if (!usable() || key == nullptr || *key == '\0') {
    return loggedFlag(call, fallback);
  }
  return loggedFlag(call, boolPref(key, fallback));
}

bool ps_ui_pref_set_bool(const char *key, bool value) {
  const std::string call = "ps_ui_pref_set_bool(" + quote(key) + ", " + flag(value) + ")";
  const bool ok = usable() && key != nullptr && *key != '\0';
  if (ok) {
    state().prefs[key] = value;
  }
  return loggedFlag(call, ok);
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
  const bool valid = standardId >= PS_UI_STANDARD_UNDO && standardId <= PS_UI_STANDARD_PREVIOUS_TAB;
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
