#include "NativeUiHeadlessState.h"

#include <string>
#include <utility>

// The controls of the headless backend (labels, check boxes, pop-ups, forms and tabs), the settings window
// style, the font size of a text view and the preview's scroll lock: the part of the C ABI in
// primec/ui/NativeUi.h that builds a settings window. The widget tree, the call log and the helpers are
// NativeUiHeadlessState.h; the rest of the ABI is NativeUiHeadless.cpp.

using namespace primec::ui::headless;
using namespace primec::ui::headless::detail;

namespace {

uint64_t createControl(const std::string &call, Control::Kind kind, std::string text) {
  if (!usable()) {
    return loggedHandle(call, 0);
  }
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  Control &created = s.controls[handle];
  created.kind = kind;
  created.text = std::move(text);
  return loggedHandle(call, handle);
}

// A control that may be put into a container: it exists and nothing holds it yet.
Control *freeControl(uint64_t handle) {
  Control *c = control(handle);
  return c != nullptr && c->parent == 0 && windowOfControl(handle) == 0 ? c : nullptr;
}

Control *controlOfKind(uint64_t handle, Control::Kind kind) {
  Control *c = usable() ? control(handle) : nullptr;
  return c != nullptr && c->kind == kind ? c : nullptr;
}

} // namespace

extern "C" {

uint64_t ps_ui_label_create(const char *text) {
  return createControl("ps_ui_label_create(" + quote(text) + ")", Control::Kind::Label, text != nullptr ? text : "");
}

bool ps_ui_label_set_text(uint64_t label, const char *text) {
  const std::string call = "ps_ui_label_set_text(" + std::to_string(label) + ", " + quote(text) + ")";
  Control *c = controlOfKind(label, Control::Kind::Label);
  const bool ok = c != nullptr && text != nullptr;
  if (ok) {
    c->text = text;
  }
  return loggedFlag(call, ok);
}

uint64_t ps_ui_checkbox_create(const char *title, bool checked) {
  const uint64_t handle = createControl("ps_ui_checkbox_create(" + quote(title) + ", " + flag(checked) + ")",
                                        Control::Kind::CheckBox, title != nullptr ? title : "");
  if (handle != 0) {
    state().controls[handle].checked = checked;
  }
  return handle;
}

bool ps_ui_checkbox_set_checked(uint64_t checkbox, bool checked) {
  const std::string call = "ps_ui_checkbox_set_checked(" + std::to_string(checkbox) + ", " + flag(checked) + ")";
  Control *c = controlOfKind(checkbox, Control::Kind::CheckBox);
  if (c != nullptr) {
    c->checked = checked;
  }
  return loggedFlag(call, c != nullptr);
}

bool ps_ui_checkbox_checked(uint64_t checkbox) {
  const Control *c = controlOfKind(checkbox, Control::Kind::CheckBox);
  return loggedFlag("ps_ui_checkbox_checked(" + std::to_string(checkbox) + ")", c != nullptr && c->checked);
}

uint64_t ps_ui_button_create(const char *title) {
  return createControl("ps_ui_button_create(" + quote(title) + ")", Control::Kind::Button, title != nullptr ? title : "");
}

bool ps_ui_button_set_title(uint64_t button, const char *title) {
  const std::string call = "ps_ui_button_set_title(" + std::to_string(button) + ", " + quote(title) + ")";
  Control *c = controlOfKind(button, Control::Kind::Button);
  const bool ok = c != nullptr && title != nullptr;
  if (ok) {
    c->text = title;
  }
  return loggedFlag(call, ok);
}

uint64_t ps_ui_popup_create(void) { return createControl("ps_ui_popup_create()", Control::Kind::Popup, ""); }

bool ps_ui_popup_add_item(uint64_t popup, const char *title) {
  const std::string call = "ps_ui_popup_add_item(" + std::to_string(popup) + ", " + quote(title) + ")";
  Control *c = controlOfKind(popup, Control::Kind::Popup);
  const bool ok = c != nullptr && title != nullptr;
  if (ok) {
    c->items.push_back(title);
    if (c->selected < 0) {
      c->selected = 0; // the first item is chosen, as in a platform pop-up
    }
  }
  return loggedFlag(call, ok);
}

bool ps_ui_popup_select(uint64_t popup, int32_t index) {
  const std::string call = "ps_ui_popup_select(" + std::to_string(popup) + ", " + std::to_string(index) + ")";
  Control *c = controlOfKind(popup, Control::Kind::Popup);
  const bool ok = c != nullptr && index >= 0 && static_cast<size_t>(index) < c->items.size();
  if (ok) {
    c->selected = index;
  }
  return loggedFlag(call, ok);
}

int32_t ps_ui_popup_selected(uint64_t popup) {
  const Control *c = controlOfKind(popup, Control::Kind::Popup);
  const int32_t selected = c != nullptr ? c->selected : -1;
  return logged("ps_ui_popup_selected(" + std::to_string(popup) + ")", selected, std::to_string(selected));
}

uint64_t ps_ui_form_create(void) { return createControl("ps_ui_form_create()", Control::Kind::Form, ""); }

bool ps_ui_form_add_heading(uint64_t form, const char *title) {
  const std::string call = "ps_ui_form_add_heading(" + std::to_string(form) + ", " + quote(title) + ")";
  Control *f = controlOfKind(form, Control::Kind::Form);
  const bool ok = f != nullptr && title != nullptr;
  if (ok) {
    Control::Row row;
    row.kind = Control::Row::Kind::Heading;
    row.label = title;
    f->rows.push_back(std::move(row));
  }
  return loggedFlag(call, ok);
}

bool ps_ui_form_add_row(uint64_t form, const char *label, uint64_t controlHandle) {
  const std::string call = "ps_ui_form_add_row(" + std::to_string(form) + ", " + quote(label) + ", " +
                           std::to_string(controlHandle) + ")";
  Control *f = controlOfKind(form, Control::Kind::Form);
  Control *child = f != nullptr && controlHandle != form ? freeControl(controlHandle) : nullptr;
  const bool ok = child != nullptr && label != nullptr;
  if (ok) {
    Control::Row row;
    row.kind = Control::Row::Kind::Labelled;
    row.label = label;
    row.control = controlHandle;
    f->rows.push_back(std::move(row));
    child->parent = form;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_form_add_control(uint64_t form, uint64_t controlHandle) {
  const std::string call = "ps_ui_form_add_control(" + std::to_string(form) + ", " + std::to_string(controlHandle) + ")";
  Control *f = controlOfKind(form, Control::Kind::Form);
  Control *child = f != nullptr && controlHandle != form ? freeControl(controlHandle) : nullptr;
  if (child != nullptr) {
    Control::Row row;
    row.kind = Control::Row::Kind::Plain;
    row.control = controlHandle;
    f->rows.push_back(std::move(row));
    child->parent = form;
  }
  return loggedFlag(call, child != nullptr);
}

uint64_t ps_ui_tabs_create(void) { return createControl("ps_ui_tabs_create()", Control::Kind::Tabs, ""); }

bool ps_ui_tabs_add(uint64_t tabs, const char *title, uint64_t content) {
  const std::string call = "ps_ui_tabs_add(" + std::to_string(tabs) + ", " + quote(title) + ", " +
                           std::to_string(content) + ")";
  Control *t = controlOfKind(tabs, Control::Kind::Tabs);
  Control *page = t != nullptr && content != tabs ? freeControl(content) : nullptr;
  const bool ok = page != nullptr && title != nullptr;
  if (ok) {
    Control::Page added;
    added.title = title;
    added.content = content;
    t->pages.push_back(std::move(added));
    page->parent = tabs;
    if (t->selected < 0) {
      t->selected = 0;
    }
  }
  return loggedFlag(call, ok);
}

bool ps_ui_tabs_select(uint64_t tabs, int32_t index) {
  const std::string call = "ps_ui_tabs_select(" + std::to_string(tabs) + ", " + std::to_string(index) + ")";
  Control *t = controlOfKind(tabs, Control::Kind::Tabs);
  const bool ok = t != nullptr && index >= 0 && static_cast<size_t>(index) < t->pages.size();
  if (ok) {
    t->selected = index;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_window_set_style(uint64_t window, int32_t style) {
  const std::string call = "ps_ui_window_set_style(" + std::to_string(window) + ", " + std::to_string(style) + ")";
  Window *w = usable() ? liveWindow(window) : nullptr;
  // A settings window is never a tab, so a window already in a tab group stays a document.
  const bool ok = w != nullptr && (style == PS_UI_WINDOW_DOCUMENT || (style == PS_UI_WINDOW_SETTINGS && w->group == 0));
  if (ok) {
    w->state.style = style;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_text_view_set_font_size(uint64_t view, int32_t points) {
  const std::string call = "ps_ui_text_view_set_font_size(" + std::to_string(view) + ", " + std::to_string(points) + ")";
  TextView *v = usable() ? textView(view) : nullptr;
  const bool ok = v != nullptr && points >= 6 && points <= 96;
  if (ok) {
    v->fontSize = points;
  }
  return loggedFlag(call, ok);
}

bool ps_ui_preview_set_scroll_mode(int32_t mode) {
  const std::string call = "ps_ui_preview_set_scroll_mode(" + std::to_string(mode) + ")";
  const bool ok = usable() && mode >= PS_UI_SCROLL_OFF && mode <= PS_UI_SCROLL_CURSOR;
  if (ok) {
    state().scrollMode = mode;
  }
  return loggedFlag(call, ok);
}

uint64_t ps_ui_list_create(void) { return createControl("ps_ui_list_create()", Control::Kind::List, ""); }

bool ps_ui_list_add_item(uint64_t list, const char *text) {
  const std::string call = "ps_ui_list_add_item(" + std::to_string(list) + ", " + quote(text) + ")";
  Control *c = controlOfKind(list, Control::Kind::List);
  const bool ok = c != nullptr && text != nullptr;
  if (ok) {
    c->items.push_back(text);
  }
  return loggedFlag(call, ok);
}

bool ps_ui_list_set_item(uint64_t list, int32_t index, const char *text) {
  const std::string call = "ps_ui_list_set_item(" + std::to_string(list) + ", " + std::to_string(index) + ", " + quote(text) + ")";
  Control *c = controlOfKind(list, Control::Kind::List);
  const bool ok = c != nullptr && text != nullptr && index >= 0 && static_cast<size_t>(index) < c->items.size();
  if (ok) {
    c->items[static_cast<size_t>(index)] = text;
  }
  return loggedFlag(call, ok);
}

int32_t ps_ui_list_item_count(uint64_t list) {
  const Control *c = controlOfKind(list, Control::Kind::List);
  const int32_t count = c != nullptr ? static_cast<int32_t>(c->items.size()) : 0;
  return logged("ps_ui_list_item_count(" + std::to_string(list) + ")", count, std::to_string(count));
}

bool ps_ui_list_set_selected(uint64_t list, int32_t index, bool selected) {
  const std::string call =
      "ps_ui_list_set_selected(" + std::to_string(list) + ", " + std::to_string(index) + ", " + flag(selected) + ")";
  Control *c = controlOfKind(list, Control::Kind::List);
  const bool ok = c != nullptr && index >= 0 && static_cast<size_t>(index) < c->items.size();
  if (ok) {
    if (selected) {
      c->picked.insert(index);
    } else {
      c->picked.erase(index);
    }
  }
  return loggedFlag(call, ok);
}

bool ps_ui_list_is_selected(uint64_t list, int32_t index) {
  const Control *c = controlOfKind(list, Control::Kind::List);
  return loggedFlag("ps_ui_list_is_selected(" + std::to_string(list) + ", " + std::to_string(index) + ")",
                    c != nullptr && c->picked.count(index) != 0);
}

int32_t ps_ui_document_type_count(void) {
  const int32_t count = static_cast<int32_t>(documentTypes().size());
  return logged("ps_ui_document_type_count()", count, std::to_string(count));
}

const char *ps_ui_document_type_name(int32_t index) {
  State &s = state();
  const auto &types = documentTypes();
  s.returnedText = index >= 0 && static_cast<size_t>(index) < types.size() ? types[static_cast<size_t>(index)].name : "";
  return logged("ps_ui_document_type_name(" + std::to_string(index) + ")", s.returnedText.c_str(), quote(s.returnedText));
}

const char *ps_ui_document_type_extensions(int32_t index) {
  State &s = state();
  const auto &types = documentTypes();
  s.returnedText = index >= 0 && static_cast<size_t>(index) < types.size() ? types[static_cast<size_t>(index)].extensions : "";
  return logged("ps_ui_document_type_extensions(" + std::to_string(index) + ")", s.returnedText.c_str(),
                quote(s.returnedText));
}

const char *ps_ui_app_path(void) {
  State &s = state();
  s.returnedText = HeadlessAppPath;
  return logged("ps_ui_app_path()", s.returnedText.c_str(), quote(s.returnedText));
}

extern "C++" {
namespace {
// "MD" and ".md" are the extension "md".
std::string normalExtension(const char *extension) {
  std::string name = extension != nullptr ? extension : "";
  if (!name.empty() && name.front() == '.') {
    name.erase(0, 1);
  }
  for (char &c : name) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return name;
}
} // namespace
}

const char *ps_ui_default_app(const char *extension) {
  State &s = state();
  const auto found = s.defaultApps.find(normalExtension(extension));
  s.returnedText = found != s.defaultApps.end() ? found->second : "";
  return logged("ps_ui_default_app(" + quote(extension) + ")", s.returnedText.c_str(), quote(s.returnedText));
}

bool ps_ui_set_default_app(const char *extensions, const char *appPath) {
  const std::string call = "ps_ui_set_default_app(" + quote(extensions) + ", " + quote(appPath) + ")";
  State &s = state();
  const std::string list = extensions != nullptr ? extensions : "";
  const std::string app = appPath != nullptr && *appPath != '\0' ? appPath : HeadlessAppPath;
  bool any = false;
  for (size_t from = 0; usable() && from <= list.size();) {
    size_t comma = list.find(',', from);
    comma = comma == std::string::npos ? list.size() : comma;
    const std::string extension = normalExtension(list.substr(from, comma - from).c_str());
    if (!extension.empty()) {
      s.defaultApps[extension] = app;
      any = true;
    }
    from = comma + 1;
  }
  return loggedFlag(call, any);
}

const char *ps_ui_open_application_panel(const char *title) {
  State &s = state();
  s.returnedText.clear();
  if (usable() && !s.appAnswers.empty()) {
    s.returnedText = std::move(s.appAnswers.front());
    s.appAnswers.pop_front();
  }
  s.panelChosen = !s.returnedText.empty();
  return logged("ps_ui_open_application_panel(" + quote(title) + ")", s.returnedText.c_str(), quote(s.returnedText));
}

} // extern "C"
