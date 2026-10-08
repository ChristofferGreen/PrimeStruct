#include "primec/ui/NativeUiBindings.h"

#include "primec/ui/NativeUi.h"

#include <string>
#include <string_view>

namespace primec::ui {
namespace {

std::string text(std::string_view value) { return std::string(value); }

// Calls `bindOne(name, callable)` once per ABI function. Strings arrive as
// string_view; they are copied to NUL-terminated text for the C functions.
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
  bindOne("ps_ui_window_set_title", [](uint64_t window, std::string_view title) {
    return ps_ui_window_set_title(window, text(title).c_str());
  });
  bindOne("ps_ui_window_set_content",
          [](uint64_t window, uint64_t widget) { return ps_ui_window_set_content(window, widget); });
  bindOne("ps_ui_window_set_edited",
          [](uint64_t window, bool edited) { return ps_ui_window_set_edited(window, edited); });
  bindOne("ps_ui_window_show", [](uint64_t window) { return ps_ui_window_show(window); });
  bindOne("ps_ui_window_close", [](uint64_t window) { return ps_ui_window_close(window); });

  bindOne("ps_ui_text_view_create", [] { return ps_ui_text_view_create(); });
  bindOne("ps_ui_text_view_get_text",
          [](uint64_t view) { return std::string(ps_ui_text_view_get_text(view)); });
  bindOne("ps_ui_text_view_set_text", [](uint64_t view, std::string_view value) {
    return ps_ui_text_view_set_text(view, text(value).c_str());
  });
  bindOne("ps_ui_text_view_set_monospace",
          [](uint64_t view, bool monospace) { return ps_ui_text_view_set_monospace(view, monospace); });
  bindOne("ps_ui_text_view_is_modified", [](uint64_t view) { return ps_ui_text_view_is_modified(view); });
  bindOne("ps_ui_text_view_clear_modified", [](uint64_t view) { return ps_ui_text_view_clear_modified(view); });

  bindOne("ps_ui_menu_create", [](std::string_view title) { return ps_ui_menu_create(text(title).c_str()); });
  bindOne("ps_ui_menu_add_item",
          [](uint64_t menu, std::string_view title, std::string_view shortcut, int32_t commandId) {
            return ps_ui_menu_add_item(menu, text(title).c_str(), text(shortcut).c_str(), commandId);
          });
  bindOne("ps_ui_menu_add_separator", [](uint64_t menu) { return ps_ui_menu_add_separator(menu); });
  bindOne("ps_ui_menu_add_standard",
          [](uint64_t menu, int32_t standardId) { return ps_ui_menu_add_standard(menu, standardId); });
  bindOne("ps_ui_menu_bar_add", [](uint64_t menu) { return ps_ui_menu_bar_add(menu); });

  bindOne("ps_ui_open_panel", [](std::string_view title) { return std::string(ps_ui_open_panel(text(title).c_str())); });
  bindOne("ps_ui_save_panel", [](std::string_view title, std::string_view suggestedName) {
    return std::string(ps_ui_save_panel(text(title).c_str(), text(suggestedName).c_str()));
  });
  bindOne("ps_ui_alert", [](std::string_view message, std::string_view detail, std::string_view buttons) {
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
