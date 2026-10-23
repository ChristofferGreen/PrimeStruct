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

bool ps_ui_preview_set_scroll_lock(bool on) {
  const std::string call = "ps_ui_preview_set_scroll_lock(" + flag(on) + ")";
  const bool ok = usable();
  if (ok) {
    state().scrollLock = on;
  }
  return loggedFlag(call, ok);
}

} // extern "C"
