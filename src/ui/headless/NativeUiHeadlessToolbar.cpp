#include "NativeUiHeadlessState.h"

#include "primec/ui/MarkdownEdit.h"

#include <string>
#include <utility>

// The toolbar, the preview modes and the markdown edits of the headless backend (docs/NativeUiPlan.md section 17):
// the part of the C ABI in primec/ui/NativeUi.h that a markdown window's toolbar needs. The widget tree, the call log
// and the helpers are NativeUiHeadlessState.h.

using namespace primec::ui::headless;
using namespace primec::ui::headless::detail;

namespace {

std::vector<std::string> splitSegments(const char *titles) {
  std::vector<std::string> out;
  std::string current;
  for (const char *p = titles != nullptr ? titles : ""; *p != '\0'; ++p) {
    if (*p == '|') {
      out.push_back(current);
      current.clear();
    } else {
      current += *p;
    }
  }
  out.push_back(current);
  return out;
}

} // namespace

namespace primec::ui::headless {

std::pair<int32_t, int32_t> selection(uint64_t view) {
  const TextView *v = textView(view);
  return v != nullptr ? std::pair<int32_t, int32_t>{static_cast<int32_t>(v->selectionStart),
                                                    static_cast<int32_t>(v->selectionEnd)}
                      : std::pair<int32_t, int32_t>{0, 0};
}

std::vector<std::string> toolbarOutline(uint64_t window) {
  std::vector<std::string> lines;
  const auto it = state().windows.find(window);
  if (it == state().windows.end()) {
    return lines;
  }
  const Toolbar *bar = toolbar(it->second.state.toolbar);
  if (bar == nullptr) {
    return lines;
  }
  for (const ToolbarItem &item : bar->items) {
    const std::string symbol = item.symbol.empty() ? "" : " [" + item.symbol + "]";
    switch (item.kind) {
    case ToolbarItem::Kind::Separator:
      lines.push_back("-");
      break;
    case ToolbarItem::Kind::Button:
      lines.push_back("button " + item.title + symbol + " #" + std::to_string(item.id));
      break;
    case ToolbarItem::Kind::Segments: {
      std::string joined;
      for (const std::string &segment : item.segments) {
        joined += (joined.empty() ? "" : "|") + segment;
      }
      const int32_t selected = it->second.state.toolbarSelected;
      const bool mine = selected >= item.id && selected < item.id + static_cast<int32_t>(item.segments.size());
      lines.push_back("segments " + joined + " #" + std::to_string(item.id) +
                      (mine ? " [selected " + std::to_string(selected - item.id) + "]" : ""));
      break;
    }
    case ToolbarItem::Kind::Menu: {
      lines.push_back("menu " + item.title + symbol);
      if (const Menu *m = menu(item.menu)) {
        std::vector<std::string> entries;
        for (const MenuEntry &entry : m->entries) {
          if (entry.kind == MenuEntry::Kind::Separator) {
            entries.push_back("  -");
          } else if (entry.kind == MenuEntry::Kind::Item) {
            entries.push_back("  " + entry.title + " #" + std::to_string(entry.id));
          } else if (entry.kind == MenuEntry::Kind::Submenu) {
            entries.push_back("  " + entry.title + ">");
          }
        }
        lines.insert(lines.end(), entries.begin(), entries.end());
      }
      break;
    }
    }
  }
  return lines;
}

} // namespace primec::ui::headless

uint64_t ps_ui_toolbar_create(void) {
  const std::string call = "ps_ui_toolbar_create()";
  if (!usable()) {
    return loggedHandle(call, 0);
  }
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  s.toolbars[handle];
  return loggedHandle(call, handle);
}

bool ps_ui_toolbar_add_button(uint64_t handle, const char *title, const char *symbol, int32_t commandId) {
  const std::string call = "ps_ui_toolbar_add_button(" + std::to_string(handle) + ", " + quote(title) + ", " +
                           quote(symbol) + ", " + std::to_string(commandId) + ")";
  Toolbar *bar = usable() ? toolbar(handle) : nullptr;
  if (bar != nullptr) {
    ToolbarItem item;
    item.kind = ToolbarItem::Kind::Button;
    item.title = title != nullptr ? title : "";
    item.symbol = symbol != nullptr ? symbol : "";
    item.id = commandId;
    bar->items.push_back(std::move(item));
  }
  return loggedFlag(call, bar != nullptr);
}

bool ps_ui_toolbar_add_segments(uint64_t handle, const char *titles, int32_t firstCommandId) {
  const std::string call = "ps_ui_toolbar_add_segments(" + std::to_string(handle) + ", " + quote(titles) + ", " +
                           std::to_string(firstCommandId) + ")";
  Toolbar *bar = usable() ? toolbar(handle) : nullptr;
  const bool ok = bar != nullptr && titles != nullptr && *titles != '\0';
  if (ok) {
    ToolbarItem item;
    item.kind = ToolbarItem::Kind::Segments;
    item.segments = splitSegments(titles);
    item.id = firstCommandId;
    bar->items.push_back(std::move(item));
  }
  return loggedFlag(call, ok);
}

bool ps_ui_toolbar_add_menu(uint64_t handle, const char *title, const char *symbol, uint64_t menuHandle) {
  const std::string call = "ps_ui_toolbar_add_menu(" + std::to_string(handle) + ", " + quote(title) + ", " +
                           quote(symbol) + ", " + std::to_string(menuHandle) + ")";
  Toolbar *bar = usable() ? toolbar(handle) : nullptr;
  const Menu *m = usable() ? menu(menuHandle) : nullptr;
  const bool ok = bar != nullptr && m != nullptr && !m->inBar;
  if (ok) {
    ToolbarItem item;
    item.kind = ToolbarItem::Kind::Menu;
    item.title = title != nullptr ? title : "";
    item.symbol = symbol != nullptr ? symbol : "";
    item.menu = menuHandle;
    bar->items.push_back(std::move(item));
  }
  return loggedFlag(call, ok);
}

bool ps_ui_toolbar_add_separator(uint64_t handle) {
  const std::string call = "ps_ui_toolbar_add_separator(" + std::to_string(handle) + ")";
  Toolbar *bar = usable() ? toolbar(handle) : nullptr;
  if (bar != nullptr) {
    ToolbarItem item;
    item.kind = ToolbarItem::Kind::Separator;
    bar->items.push_back(std::move(item));
  }
  return loggedFlag(call, bar != nullptr);
}

bool ps_ui_window_set_toolbar(uint64_t window, uint64_t handle) {
  const std::string call = "ps_ui_window_set_toolbar(" + std::to_string(window) + ", " + std::to_string(handle) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  const bool ok = w != nullptr && (handle == 0 || toolbar(handle) != nullptr);
  if (ok) {
    w->state.toolbar = handle;
    w->state.toolbarSelected = -1;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_window_toolbar_select(uint64_t window, int32_t commandId) {
  const std::string call =
      "ps_ui_window_toolbar_select(" + std::to_string(window) + ", " + std::to_string(commandId) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  const Toolbar *bar = w != nullptr ? toolbar(w->state.toolbar) : nullptr;
  bool found = false;
  if (bar != nullptr) {
    for (const ToolbarItem &item : bar->items) {
      found = found || (item.kind == ToolbarItem::Kind::Segments && commandId >= item.id &&
                        commandId < item.id + static_cast<int32_t>(item.segments.size()));
    }
  }
  if (found) {
    w->state.toolbarSelected = commandId;
  }
  return loggedFlag(call, found);
}

bool ps_ui_window_set_preview_mode(uint64_t window, int32_t mode) {
  const std::string call = "ps_ui_window_set_preview_mode(" + std::to_string(window) + ", " + std::to_string(mode) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  TextView *view = w != nullptr ? textView(w->state.content) : nullptr;
  const bool ok = w != nullptr && mode >= PS_UI_PREVIEW_TEXT && mode <= PS_UI_PREVIEW_ONLY &&
                  (view != nullptr || mode == PS_UI_PREVIEW_TEXT);
  if (ok) {
    if (mode != PS_UI_PREVIEW_TEXT && !w->state.preview) {
      w->state.preview = true;
      renderPreview(*w, *view);
    } else if (mode == PS_UI_PREVIEW_TEXT) {
      w->state.preview = false;
      w->previewPage.clear();
    }
    w->state.previewMode = mode;
  }
  return loggedFlag(call, ok);
}

int32_t ps_ui_window_preview_mode(uint64_t window) {
  const std::string call = "ps_ui_window_preview_mode(" + std::to_string(window) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  const int32_t mode = w != nullptr ? w->state.previewMode : 0;
  return logged(call, mode, std::to_string(mode));
}

bool ps_ui_text_view_set_markdown_editing(uint64_t view, bool on) {
  const std::string call = "ps_ui_text_view_set_markdown_editing(" + std::to_string(view) + ", " + flag(on) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  if (v != nullptr) {
    v->markdownEditing = on;
  }
  return loggedFlag(call, v != nullptr);
}

bool ps_ui_text_view_markdown_edit(uint64_t view, int32_t operation, int32_t a, int32_t b) {
  const std::string call = "ps_ui_text_view_markdown_edit(" + std::to_string(view) + ", " + std::to_string(operation) +
                           ", " + std::to_string(a) + ", " + std::to_string(b) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  bool ok = v != nullptr && operation >= PS_UI_MARKDOWN_BOLD && operation <= PS_UI_MARKDOWN_TABLE_PREVIOUS_CELL;
  if (ok) {
    const auto result = primec::ui::markdownEdit(v->text, v->selectionStart, v->selectionEnd,
                                                 static_cast<primec::ui::MarkdownEditOp>(operation), a, b);
    ok = result.applied;
    if (ok) {
      v->text = primec::ui::applyMarkdownEdit(v->text, result);
      v->selectionStart = result.selectionStart;
      v->selectionEnd = result.selectionEnd;
      v->modified = true;
      Action changed;
      changed.kind = Action::Kind::TextChanged;
      changed.handle = view;
      state().actions.push_front(std::move(changed)); // the user's change is reported before later actions
    }
  }
  return loggedFlag(call, ok);
}
