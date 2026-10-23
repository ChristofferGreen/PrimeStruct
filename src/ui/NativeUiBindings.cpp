#include "primec/ui/NativeUiBindings.h"

#include "primec/ui/NativeUi.h"

#include <string>
#include <string_view>

namespace primec::ui {
namespace {

std::string text(std::string_view value) { return std::string(value); }
std::string text(embed::HostTextView value) { return std::string(value.value); }

// Calls `bindOne(name, callable)` once per ABI function. Strings arrive as
// string_view (fixed labels) or HostTextView (program-built text); they are copied to NUL-terminated
// text for the C functions. Text results return as HostText, which the VM turns into a String.
template <class Binder> void forEachBinding(Binder &&bindOne) {
  bindOne("ps_ui_abi_version", [] { return ps_ui_abi_version(); });
  bindOne("ps_ui_init", [](std::string_view appName) { return ps_ui_init(text(appName).c_str()); });
  bindOne("ps_ui_quit", [] { ps_ui_quit(); });
  bindOne("ps_ui_platform", [] { return ps_ui_platform(); });

  bindOne("ps_ui_wait_event", [] { return ps_ui_wait_event(); });
  bindOne("ps_ui_event_window", [] { return ps_ui_event_window(); });
  bindOne("ps_ui_event_widget", [] { return ps_ui_event_widget(); });
  bindOne("ps_ui_event_command", [] { return ps_ui_event_command(); });

  bindOne("ps_ui_window_create", [](std::string_view title, int32_t width, int32_t height) {
    return ps_ui_window_create(text(title).c_str(), width, height);
  });
  bindOne("ps_ui_window_set_title", [](uint64_t window, embed::HostTextView title) {
    return ps_ui_window_set_title(window, text(title).c_str());
  });
  bindOne("ps_ui_window_set_autosave_name", [](uint64_t window, std::string_view name) {
    return ps_ui_window_set_autosave_name(window, text(name).c_str());
  });
  bindOne("ps_ui_window_set_content",
          [](uint64_t window, uint64_t widget) { return ps_ui_window_set_content(window, widget); });
  bindOne("ps_ui_window_set_edited",
          [](uint64_t window, bool edited) { return ps_ui_window_set_edited(window, edited); });
  bindOne("ps_ui_window_show", [](uint64_t window) { return ps_ui_window_show(window); });
  bindOne("ps_ui_window_close", [](uint64_t window) { return ps_ui_window_close(window); });

  bindOne("ps_ui_text_view_create", [] { return ps_ui_text_view_create(); });
  bindOne("ps_ui_text_view_get_text",
          [](uint64_t view) { return embed::HostText(ps_ui_text_view_get_text(view)); });
  bindOne("ps_ui_text_view_set_text", [](uint64_t view, embed::HostTextView value) {
    return ps_ui_text_view_set_text(view, text(value).c_str());
  });
  bindOne("ps_ui_text_view_set_monospace",
          [](uint64_t view, bool monospace) { return ps_ui_text_view_set_monospace(view, monospace); });
  bindOne("ps_ui_text_view_set_line_numbers",
          [](uint64_t view, bool visible) { return ps_ui_text_view_set_line_numbers(view, visible); });
  bindOne("ps_ui_text_view_set_wrapping",
          [](uint64_t view, bool wrap) { return ps_ui_text_view_set_wrapping(view, wrap); });
  bindOne("ps_ui_text_view_is_modified", [](uint64_t view) { return ps_ui_text_view_is_modified(view); });
  bindOne("ps_ui_text_view_clear_modified", [](uint64_t view) { return ps_ui_text_view_clear_modified(view); });

  bindOne("ps_ui_text_view_load_file", [](uint64_t view, embed::HostTextView path) {
    return ps_ui_text_view_load_file(view, text(path).c_str());
  });
  bindOne("ps_ui_text_view_save_file", [](uint64_t view, embed::HostTextView path) {
    return ps_ui_text_view_save_file(view, text(path).c_str());
  });
  bindOne("ps_ui_window_add_tab", [](uint64_t host, uint64_t tab) { return ps_ui_window_add_tab(host, tab); });
  bindOne("ps_ui_active_window", [] { return ps_ui_active_window(); });
  bindOne("ps_ui_window_tab_count", [](uint64_t window) { return ps_ui_window_tab_count(window); });
  bindOne("ps_ui_window_tab_at", [](uint64_t window, int32_t index) { return ps_ui_window_tab_at(window, index); });
  bindOne("ps_ui_window_select", [](uint64_t window) { return ps_ui_window_select(window); });
  bindOne("ps_ui_window_content", [](uint64_t window) { return ps_ui_window_content(window); });
  bindOne("ps_ui_window_count", [] { return ps_ui_window_count(); });
  bindOne("ps_ui_window_at", [](int32_t index) { return ps_ui_window_at(index); });
  bindOne("ps_ui_window_set_path", [](uint64_t window, embed::HostTextView path) {
    return ps_ui_window_set_path(window, text(path).c_str());
  });
  bindOne("ps_ui_window_path", [](uint64_t window) { return embed::HostText(ps_ui_window_path(window)); });
  bindOne("ps_ui_window_has_path", [](uint64_t window) { return ps_ui_window_has_path(window); });
  bindOne("ps_ui_text_view_clear_styles", [](uint64_t view) { return ps_ui_text_view_clear_styles(view); });
  bindOne("ps_ui_text_view_add_style", [](uint64_t view, int32_t start, int32_t end, int32_t rgb, int32_t flags) {
    return ps_ui_text_view_add_style(view, start, end, rgb, flags);
  });
  bindOne("ps_ui_text_view_highlight", [](uint64_t view, embed::HostTextView path) {
    return ps_ui_text_view_highlight(view, text(path).c_str());
  });
  bindOne("ps_ui_last_error", [] { return embed::HostText(ps_ui_last_error()); });

  bindOne("ps_ui_menu_create", [](std::string_view title) { return ps_ui_menu_create(text(title).c_str()); });
  bindOne("ps_ui_menu_add_item",
          [](uint64_t menu, std::string_view title, std::string_view shortcut, int32_t commandId) {
            return ps_ui_menu_add_item(menu, text(title).c_str(), text(shortcut).c_str(), commandId);
          });
  bindOne("ps_ui_menu_set_checked", [](uint64_t menu, int32_t commandId, bool checked) {
    return ps_ui_menu_set_checked(menu, commandId, checked);
  });
  bindOne("ps_ui_menu_add_submenu", [](uint64_t menu, std::string_view title, uint64_t submenu) {
    return ps_ui_menu_add_submenu(menu, text(title).c_str(), submenu);
  });
  bindOne("ps_ui_menu_clear", [](uint64_t menu) { return ps_ui_menu_clear(menu); });
  bindOne("ps_ui_menu_add_recent_files", [](uint64_t menu, int32_t firstCommandId, int32_t maxItems) {
    return ps_ui_menu_add_recent_files(menu, firstCommandId, maxItems);
  });
  bindOne("ps_ui_recent_add", [](embed::HostTextView path) { return ps_ui_recent_add(text(path).c_str()); });
  bindOne("ps_ui_recent_path", [](int32_t index) { return embed::HostText(ps_ui_recent_path(index)); });
  bindOne("ps_ui_recent_clear", [] { return ps_ui_recent_clear(); });
  bindOne("ps_ui_pref_get_int", [](std::string_view key, int32_t fallback) {
    return ps_ui_pref_get_int(text(key).c_str(), fallback);
  });
  bindOne("ps_ui_pref_set_int", [](std::string_view key, int32_t value) {
    return ps_ui_pref_set_int(text(key).c_str(), value);
  });
  bindOne("ps_ui_pref_list_clear", [](std::string_view key) { return ps_ui_pref_list_clear(text(key).c_str()); });
  bindOne("ps_ui_pref_list_add", [](std::string_view key, embed::HostTextView item) {
    return ps_ui_pref_list_add(text(key).c_str(), text(item).c_str());
  });
  bindOne("ps_ui_pref_list_count", [](std::string_view key) { return ps_ui_pref_list_count(text(key).c_str()); });
  bindOne("ps_ui_pref_list_get", [](std::string_view key, int32_t index) {
    return embed::HostText(ps_ui_pref_list_get(text(key).c_str(), index));
  });
  bindOne("ps_ui_pref_get_bool", [](std::string_view key, bool fallback) {
    return ps_ui_pref_get_bool(text(key).c_str(), fallback);
  });
  bindOne("ps_ui_pref_set_bool", [](std::string_view key, bool value) {
    return ps_ui_pref_set_bool(text(key).c_str(), value);
  });
  bindOne("ps_ui_menu_add_separator", [](uint64_t menu) { return ps_ui_menu_add_separator(menu); });
  bindOne("ps_ui_menu_add_standard",
          [](uint64_t menu, int32_t standardId) { return ps_ui_menu_add_standard(menu, standardId); });
  bindOne("ps_ui_menu_bar_add", [](uint64_t menu) { return ps_ui_menu_bar_add(menu); });
  bindOne("ps_ui_menu_bar_add_role",
          [](uint64_t menu, int32_t role) { return ps_ui_menu_bar_add_role(menu, role); });

  bindOne("ps_ui_open_panel", [](std::string_view title) { return embed::HostText(ps_ui_open_panel(text(title).c_str())); });
  bindOne("ps_ui_save_panel", [](std::string_view title, std::string_view suggestedName) {
    return embed::HostText(ps_ui_save_panel(text(title).c_str(), text(suggestedName).c_str()));
  });
  bindOne("ps_ui_panel_chosen", [] { return ps_ui_panel_chosen(); });
  bindOne("ps_ui_alert",[](std::string_view message, embed::HostTextView detail, std::string_view buttons) {
    return ps_ui_alert(text(message).c_str(), text(detail).c_str(), text(buttons).c_str());
  });
}

const char *typeName(embed::HostType type) {
  switch (type) {
  case embed::HostType::Void:
    return "void";
  case embed::HostType::I32:
    return "i32";
  case embed::HostType::I64:
    return "i64";
  case embed::HostType::U64:
    return "u64";
  case embed::HostType::F32:
    return "f32";
  case embed::HostType::F64:
    return "f64";
  case embed::HostType::Bool:
    return "bool";
  case embed::HostType::String:
    return "string";
  case embed::HostType::U8:
    return "u8";
  case embed::HostType::Text:
    return "text";
  }
  return "?";
}

} // namespace

void bindNativeUi(embed::ScriptEngine &engine) {
  forEachBinding([&engine](const char *name, auto callable) { engine.bind(name, std::move(callable)); });
}

void bindNativeUi(embed::Script &script) {
  forEachBinding([&script](const char *name, auto callable) { script.bind(name, std::move(callable)); });
}

std::vector<std::string> nativeUiBindingSignatures() {
  embed::HostBindings bindings;
  forEachBinding([&bindings](const char *name, auto callable) { bindings.bind(name, std::move(callable)); });
  std::vector<std::string> signatures;
  for (const auto &entry : bindings.entries()) {
    std::string line = entry.name + "(";
    for (size_t i = 0; i < entry.parameters.size(); ++i) {
      line += (i == 0 ? "" : ", ") + std::string(typeName(entry.parameters[i]));
    }
    signatures.push_back(line + ") -> " + typeName(entry.returnType));
  }
  return signatures;
}

} // namespace primec::ui
