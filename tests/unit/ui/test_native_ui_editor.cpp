#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace headless = primec::ui::headless;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.ui.editor");

namespace {
// Command ids of examples/apps/text_editor/main.prime.
constexpr int32_t CommandNew = 1;
constexpr int32_t CommandOpen = 2;
constexpr int32_t CommandSave = 3;
constexpr int32_t CommandSaveAs = 4;

// Handles in creation order: the window, then its text view.
constexpr uint64_t EditorWindow = 1;

Script compileEditor() {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}

std::string scratchPath(const std::string &name) {
  const std::filesystem::path path = primec::testing::testScratchDir("native_ui_editor") / name;
  std::filesystem::create_directories(path.parent_path());
  std::filesystem::remove(path);
  return path.string();
}

std::string writeScratch(const std::string &name, const std::string &contents) {
  const std::string path = scratchPath(name);
  std::ofstream(path, std::ios::binary) << contents;
  return path;
}

bool exists(const std::string &path) { return std::filesystem::exists(path); }

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int countCalls(const std::string &prefix) {
  int count = 0;
  for (const auto &entry : headless::callLog()) {
    if (entry.rfind(prefix, 0) == 0) {
      ++count;
    }
  }
  return count;
}

void runEditor() {
  const Script script = compileEditor();
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
}
} // namespace

TEST_CASE("editor builds its window, menus and standard edit items") {
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const auto window = headless::windowState(EditorWindow);
  CHECK(window.title == "Untitled");
  CHECK(window.closed);
  CHECK(countCalls("ps_ui_window_show(1)") == 1);
  CHECK(headless::isMonospace(2));
  const std::vector<std::string> expected{
      "File>New (cmd+n) #1",         "File>Open... (cmd+o) #2", "File>-",
      "File>Save (cmd+s) #3",        "File>Save As... (cmd+shift+s) #4",
      "File>-",                      "File>Close (cmd+w) #5",   "Edit>standard 1",
      "Edit>standard 2",             "Edit>-",                  "Edit>standard 3",
      "Edit>standard 4",             "Edit>standard 5",         "Edit>standard 6",
      "Edit>-",                      "Edit>standard 7"};
  CHECK(headless::menuBarItems() == expected);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor types then saves to the chosen path") {
  const std::string out = scratchPath("typed.txt");
  headless::reset();
  headless::pushTypeText(0, "hello");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(out);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(out) == "hello");
  const auto window = headless::windowState(EditorWindow);
  CHECK(window.title == out);
  CHECK_FALSE(window.edited);
  CHECK(window.closed);
  CHECK(countCalls("ps_ui_alert(") == 0);
  CHECK(countCalls("ps_ui_save_panel(") == 1);
}

TEST_CASE("editor saves again without asking for a path") {
  const std::string out = scratchPath("again.txt");
  headless::reset();
  headless::pushTypeText(0, "a");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(out);
  headless::pushTypeText(0, "b");
  headless::pushCommand(CommandSave);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(out) == "ab");
  CHECK(countCalls("ps_ui_save_panel(") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor save as picks a new path and keeps it for later saves") {
  const std::string first = scratchPath("first.txt");
  const std::string second = scratchPath("second.txt");
  headless::reset();
  headless::pushTypeText(0, "a");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(first);
  headless::pushTypeText(0, "b");
  headless::pushCommand(CommandSaveAs);
  headless::pushSavePanelAnswer(second);
  headless::pushTypeText(0, "c");
  headless::pushCommand(CommandSave);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(first) == "a");
  CHECK(readFile(second) == "abc");
  CHECK(headless::windowState(EditorWindow).title == second);
  CHECK(countCalls("ps_ui_save_panel(") == 2);
}

TEST_CASE("editor save does nothing when the save panel is cancelled") {
  headless::reset();
  headless::pushTypeText(0, "draft");
  headless::pushCommand(CommandSave);
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_text_view_save_file(") == 0);
  const auto window = headless::windowState(EditorWindow);
  CHECK(window.title == "Untitled");
  CHECK(window.closed);
}

TEST_CASE("editor opens an existing file") {
  const std::string in = writeScratch("existing.txt", "h\xC3\xA9llo\nworld\n");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(in);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const auto window = headless::windowState(EditorWindow);
  CHECK(window.title == in);
  CHECK_FALSE(window.edited);
  CHECK(window.closed);
  CHECK(countCalls("ps_ui_text_view_load_file(2, ") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor reports a file that cannot be opened and keeps its document") {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer("/definitely/not/here.txt");
  headless::pushAlertAnswer(0);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
  CHECK(countCalls("ps_ui_alert(\"The file could not be opened.\", \"cannot open /definitely/not/here.txt") == 1);
  CHECK(headless::windowState(EditorWindow).closed);
}

TEST_CASE("editor open ignores a cancelled open panel") {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_text_view_load_file(") == 0);
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
}

TEST_CASE("editor close with unsaved text saves when the user chooses Save") {
  const std::string out = scratchPath("close_save.txt");
  headless::reset();
  headless::pushTypeText(0, "keep me");
  headless::pushCloseWindow(EditorWindow);
  headless::pushAlertAnswer(0);
  headless::pushSavePanelAnswer(out);
  runEditor();
  CHECK(readFile(out) == "keep me");
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(countCalls("ps_ui_alert(") == 1);
}

TEST_CASE("editor close with unsaved text discards it when the user chooses Don't Save") {
  const std::string out = scratchPath("close_discard.txt");
  headless::reset();
  headless::pushTypeText(0, "throw away");
  headless::pushCloseWindow(EditorWindow);
  headless::pushAlertAnswer(1);
  headless::pushSavePanelAnswer(out);
  runEditor();
  CHECK_FALSE(exists(out));
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(countCalls("ps_ui_text_view_save_file(") == 0);
}

TEST_CASE("editor close with unsaved text stays open when the user chooses Cancel") {
  headless::reset();
  headless::pushTypeText(0, "still here");
  headless::pushCloseWindow(EditorWindow);
  headless::pushAlertAnswer(2);
  headless::pushCloseWindow(EditorWindow);
  headless::pushAlertAnswer(1);
  runEditor();
  CHECK(countCalls("ps_ui_alert(") == 2);
  CHECK(countCalls("ps_ui_window_close(") == 1);
  CHECK(headless::windowState(EditorWindow).closed);
}

TEST_CASE("editor new clears the document after asking about unsaved text") {
  const std::string out = scratchPath("before_new.txt");
  headless::reset();
  headless::pushTypeText(0, "old");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(out);
  headless::pushTypeText(0, "!");
  headless::pushCommand(CommandNew);
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(out) == "old");
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
  CHECK_FALSE(headless::windowState(EditorWindow).edited);
  CHECK(countCalls("ps_ui_text_view_set_text(2, \"\")") == 1);
}

TEST_CASE("editor reports a file that cannot be saved") {
  headless::reset();
  headless::pushTypeText(0, "x");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer("/definitely/not/here/out.txt");
  headless::pushAlertAnswer(0);
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_alert(\"The file could not be saved.\", \"cannot write /definitely/not/here/out.txt") == 1);
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
  CHECK(headless::windowState(EditorWindow).closed);
}
