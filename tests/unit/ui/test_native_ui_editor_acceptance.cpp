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

// The editor acceptance gate (docs/todo.md, TODO-5552 .. TODO-5554). It runs in
// ctest as PrimeStruct_native_ui_editor_acceptance together with the
// primestruct.ui.editor scenarios, which already cover cancelled dialogs, failing
// open and save (the error is surfaced in an alert), and the dirty-flag close
// prompt with each answer. This suite adds the file round trips.

namespace headless = primec::ui::headless;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.ui.editor_acceptance");

namespace {
constexpr int32_t CommandOpen = 2;
constexpr int32_t CommandSave = 3;
constexpr int32_t CommandSaveAs = 4;
constexpr uint64_t EditorWindow = 1;

std::string scratchPath(const std::string &name) {
  const std::filesystem::path path = primec::testing::testScratchDir("native_ui_editor_acceptance") / name;
  std::filesystem::create_directories(path.parent_path());
  std::filesystem::remove(path);
  return path.string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Runs the editor program once against the scripted headless backend.
void runEditor() {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
}
} // namespace

TEST_CASE("acceptance: open, edit, save and reopen keep the bytes") {
  const std::string path = scratchPath("note.txt");

  headless::reset();
  headless::pushTypeText(0, "alpha");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(path);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  REQUIRE(readFile(path) == "alpha");

  // Reopen, edit, save in place (no save panel), close without a prompt.
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  headless::pushTypeText(0, "-beta");
  headless::pushCommand(CommandSave);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(path) == "alpha-beta");
  CHECK(headless::windowState(EditorWindow).title == path);
  CHECK_FALSE(headless::windowState(EditorWindow).edited);
  for (const auto &entry : headless::callLog()) {
    CHECK(entry.rfind("ps_ui_save_panel(", 0) != 0);
    CHECK(entry.rfind("ps_ui_alert(", 0) != 0);
  }

  // Open and close without edits: nothing is written and nothing is asked.
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(path) == "alpha-beta");
  for (const auto &entry : headless::callLog()) {
    CHECK(entry.rfind("ps_ui_text_view_save_file(", 0) != 0);
    CHECK(entry.rfind("ps_ui_alert(", 0) != 0);
  }
}

TEST_CASE("acceptance: save as writes an equal copy and later saves go to it") {
  const std::string original = scratchPath("original.txt");
  const std::string copy = scratchPath("copy.txt");

  headless::reset();
  headless::pushTypeText(0, "line one\nline two\n");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(original);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  REQUIRE(readFile(original) == "line one\nline two\n");

  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(original);
  headless::pushCommand(CommandSaveAs);
  headless::pushSavePanelAnswer(copy);
  headless::pushTypeText(0, "three\n");
  headless::pushCommand(CommandSave);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(original) == "line one\nline two\n");
  CHECK(readFile(copy) == "line one\nline two\nthree\n");
  CHECK(headless::windowState(EditorWindow).title == copy);
}
