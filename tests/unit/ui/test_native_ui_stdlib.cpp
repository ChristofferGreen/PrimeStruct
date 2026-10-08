#include "primec/embed/ScriptEngine.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

namespace headless = primec::ui::headless;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.ui.stdlib");

namespace {
Script compileExample() {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/native_ui/hello_window.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}

Script compileSource(const std::string &source) {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  Script script = engine.compileSource("/native_ui_stdlib_test.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}
} // namespace

TEST_CASE("native ui example builds its window and menu before the loop") {
  const Script script = compileExample();
  headless::reset();
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
  const std::vector<std::string> expected{
      "ps_ui_init(\"Hello\") -> true",
      "ps_ui_window_create(\"Untitled\", 720, 480) -> 1",
      "ps_ui_text_view_create() -> 2",
      "ps_ui_window_set_content(1, 2) -> true",
      "ps_ui_text_view_set_text(2, \"Hello, native UI\") -> true",
      "ps_ui_menu_create(\"File\") -> 3",
      "ps_ui_menu_add_item(3, \"Save\", \"cmd+s\", 7) -> true",
      "ps_ui_menu_bar_add(3) -> true",
      "ps_ui_window_show(1) -> true",
      "ps_ui_wait_event() -> quit_requested",
  };
  CHECK(headless::callLog() == expected);
  const auto window = headless::windowState(1);
  CHECK(window.shown);
  CHECK(window.content == 2);
  CHECK(headless::menuBarItems() == std::vector<std::string>{"File>Save (cmd+s) #7"});
}

TEST_CASE("native ui example handles commands typing and close requests with pick") {
  const Script script = compileExample();
  headless::reset();
  headless::pushCommand(7);
  headless::pushTypeText(0, " more");
  headless::pushCommand(3);
  headless::pushCloseWindow(1);
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 10);
  const auto window = headless::windowState(1);
  CHECK(window.edited);
  CHECK(window.closed);
  const auto &log = headless::callLog();
  CHECK(log.back() == "ps_ui_window_close(1) -> true");
}

TEST_CASE("native ui quit request ends the loop without closing the window") {
  const Script script = compileExample();
  headless::reset();
  headless::pushCommand(5);
  headless::pushQuit();
  headless::pushCommand(100);
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 5);
  CHECK_FALSE(headless::windowState(1).closed);
}

TEST_CASE("native ui app wraps dialogs and text views") {
  const Script script = compileSource(R"(
import /std/ui/native/*

[return<int>]
main() {
  [App] app{start_app("Dialogs")}
  [TextView] view{app.textView()}
  view.setText(app.openPanel("Open"))
  [Window] window{app.window(app.savePanel("Save As", "a.txt"), 100i32, 100i32)}
  window.setContent(view)
  return(app.alert("Discard changes?", "Unsaved text", "Save\nDiscard"))
}
)");
  headless::reset();
  headless::pushOpenPanelAnswer("/docs/a.txt");
  headless::pushSavePanelAnswer("/out/b.txt");
  headless::pushAlertAnswer(1);
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 1);
  CHECK(headless::windowState(2).title == "/out/b.txt");
  CHECK(headless::callLog().at(3) == "ps_ui_text_view_set_text(1, \"/docs/a.txt\") -> true");
}

TEST_CASE("native ui menus offer the standard edit items") {
  const Script script = compileSource(R"(
import /std/ui/native/*

[return<int>]
main() {
  [App] app{start_app("Menus")}
  [Menu] edit{app.menu("Edit")}
  edit.undo()
  edit.redo()
  edit.separator()
  edit.cut()
  edit.copy()
  edit.paste()
  edit.selectAll()
  edit.find()
  edit.addToBar()
  [Menu] file{app.menu("File")}
  file.quit()
  file.about()
  file.addToBar()
  return(0i32)
}
)");
  headless::reset();
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  const std::vector<std::string> expected{"Edit>standard 1", "Edit>standard 2", "Edit>-",        "Edit>standard 3",
                                          "Edit>standard 4", "Edit>standard 5", "Edit>standard 6", "Edit>standard 7",
                                          "File>standard 8", "File>standard 9"};
  CHECK(headless::menuBarItems() == expected);
}

TEST_CASE("native ui start reports a backend that cannot start") {
  const Script script = compileSource(R"(
import /std/ui/native/*

[return<int>]
main() {
  [App] first{start_app("One")}
  [App] second{start_app("Two")}
  if(second.started) {
    return(2i32)
  }
  if(first.started) {
    return(1i32)
  }
  return(0i32)
}
)");
  headless::reset();
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 1);
}
