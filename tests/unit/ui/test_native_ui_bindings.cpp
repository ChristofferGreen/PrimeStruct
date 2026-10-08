#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <string>
#include <vector>

namespace headless = primec::ui::headless;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.ui.bindings");

namespace {
const char *EditorScript = R"(
[host return<i32>]
ps_ui_abi_version() {
}
[host return<bool>]
ps_ui_init([string] appName) {
}
[host return<void>]
ps_ui_quit() {
}
[host return<i32>]
ps_ui_platform() {
}
[host return<i32>]
ps_ui_wait_event() {
}
[host return<u64>]
ps_ui_event_window() {
}
[host return<u64>]
ps_ui_event_widget() {
}
[host return<i32>]
ps_ui_event_command() {
}
[host return<u64>]
ps_ui_window_create([string] title, [i32] width, [i32] height) {
}
[host return<bool>]
ps_ui_window_set_title([u64] window, [string] title) {
}
[host return<bool>]
ps_ui_window_set_content([u64] window, [u64] widget) {
}
[host return<bool>]
ps_ui_window_set_edited([u64] window, [bool] edited) {
}
[host return<bool>]
ps_ui_window_show([u64] window) {
}
[host return<bool>]
ps_ui_window_close([u64] window) {
}
[host return<u64>]
ps_ui_text_view_create() {
}
[host return<string>]
ps_ui_text_view_get_text([u64] view) {
}
[host return<bool>]
ps_ui_text_view_set_text([u64] view, [string] text) {
}
[host return<bool>]
ps_ui_text_view_set_monospace([u64] view, [bool] monospace) {
}
[host return<bool>]
ps_ui_text_view_is_modified([u64] view) {
}
[host return<bool>]
ps_ui_text_view_clear_modified([u64] view) {
}
[host return<u64>]
ps_ui_menu_create([string] title) {
}
[host return<bool>]
ps_ui_menu_add_item([u64] menu, [string] title, [string] shortcut, [i32] commandId) {
}
[host return<bool>]
ps_ui_menu_add_separator([u64] menu) {
}
[host return<bool>]
ps_ui_menu_add_standard([u64] menu, [i32] standardId) {
}
[host return<bool>]
ps_ui_menu_bar_add([u64] menu) {
}
[host return<string>]
ps_ui_open_panel([string] title) {
}
[host return<string>]
ps_ui_save_panel([string] title, [string] suggestedName) {
}
[host return<i32>]
ps_ui_alert([string] message, [string] detail, [string] buttons) {
}

[return<int>]
score([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[return<int>]
main() {
  [bool] started{ps_ui_init("Scripted")}
  [u64] window{ps_ui_window_create("Untitled", 720i32, 480i32)}
  [u64] text{ps_ui_text_view_create()}
  [u64] menu{ps_ui_menu_create("File")}
  [mut] ok{score(started)}
  ok = ok + score(ps_ui_abi_version() == 0i32)
  ok = ok + score(ps_ui_platform() == 5i32)
  ok = ok + score(ps_ui_window_set_content(window, text))
  ok = ok + score(ps_ui_text_view_set_monospace(text, true))
  ok = ok + score(ps_ui_menu_add_item(menu, "Save", "cmd+s", 7i32))
  ok = ok + score(ps_ui_menu_add_separator(menu))
  ok = ok + score(ps_ui_menu_add_standard(menu, 4i32))
  ok = ok + score(ps_ui_menu_bar_add(menu))
  ok = ok + score(ps_ui_window_show(window))
  [string] picked{ps_ui_open_panel("Open")}
  ok = ok + score(ps_ui_text_view_set_text(text, picked))
  ok = ok + score(ps_ui_text_view_clear_modified(text))
  [string] saved{ps_ui_save_panel("Save As", "Untitled.txt")}
  ok = ok + score(ps_ui_window_set_title(window, saved))
  ok = ok + score(ps_ui_alert("Save changes?", "Unsaved text", "Save\nDiscard\nCancel") == 1i32)
  [mut] commands{0i32}
  [mut] running{true}
  while(running) {
    [i32] kind{ps_ui_wait_event()}
    if(kind == 1i32) {
      commands = commands + ps_ui_event_command()
    } else {
      if(kind == 2i32) {
        ok = ok + score(ps_ui_window_close(ps_ui_event_window()))
      } else {
        if(kind == 3i32) {
          ok = ok + score(ps_ui_window_set_title(ps_ui_event_window(), ps_ui_text_view_get_text(ps_ui_event_widget())))
          ok = ok + score(ps_ui_window_set_edited(ps_ui_event_window(), ps_ui_text_view_is_modified(ps_ui_event_widget())))
        } else {
          running = false
        }
      }
    }
  }
  ps_ui_quit()
  return(ok * 1000i32 + commands)
}
)";
} // namespace

TEST_CASE("native ui bindings cover every ABI function once") {
  const auto signatures = primec::ui::nativeUiBindingSignatures();
  CHECK(signatures.size() == 32);
  for (const char *expected : {"ps_ui_init(string) -> bool", "ps_ui_window_create(string, i32, i32) -> u64",
                               "ps_ui_text_view_get_text(u64) -> string", "ps_ui_window_set_edited(u64, bool) -> bool",
                               "ps_ui_menu_add_item(u64, string, string, i32) -> bool",
                               "ps_ui_alert(string, string, string) -> i32", "ps_ui_quit() -> void",
                               "ps_ui_panel_chosen() -> bool"}) {
    CHECK_MESSAGE(std::find(signatures.begin(), signatures.end(), expected) != signatures.end(), expected);
  }
  std::vector<std::string> names;
  for (const auto &signature : signatures) {
    names.push_back(signature.substr(0, signature.find('(')));
  }
  std::sort(names.begin(), names.end());
  CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());
}

TEST_CASE("a script drives the headless backend through bound host functions") {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const auto script = engine.compileSource("/native_ui_editor.prime", EditorScript);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string error;
  REQUIRE_MESSAGE(script.checkHostBindings(error), error);

  headless::reset();
  headless::pushOpenPanelAnswer("/docs/a.txt");
  headless::pushSavePanelAnswer("/out/b.txt");
  headless::pushAlertAnswer(1);
  headless::pushCommand(10);
  headless::pushTypeText(0, "!");
  headless::pushCommand(5);
  headless::pushCloseWindow(1);
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  // 14 successful calls before the loop plus 3 in it, and commands 10 + 5.
  CHECK(result.exitCode == 17015);

  const auto window = headless::windowState(1);
  CHECK(window.closed);
  CHECK(window.title == "/docs/a.txt!");
  CHECK(window.edited);
  CHECK(headless::isMonospace(2));
  const std::vector<std::string> menu{"File>Save (cmd+s) #7", "File>-", "File>standard 4"};
  CHECK(headless::menuBarItems() == menu);
  CHECK(headless::callLog().front() == "ps_ui_init(\"Scripted\") -> true");
  CHECK(headless::callLog().back() == "ps_ui_quit() -> ok");
}

TEST_CASE("bindings can be attached to one script instead of the engine") {
  ScriptEngine engine;
  auto script = engine.compileSource("/native_ui_editor_script.prime", EditorScript);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string error;
  CHECK_FALSE(script.checkHostBindings(error));
  primec::ui::bindNativeUi(script);
  CHECK_MESSAGE(script.checkHostBindings(error), error);
  headless::reset();
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode / 1000 >= 12);
}
