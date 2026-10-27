#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "tests/unit/ui/CompiledEditorMirror.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

int countCalls(const std::string &prefix) {
  int count = 0;
  for (const auto &entry : headless::callLog()) {
    count += entry.rfind(prefix, 0) == 0 ? 1 : 0;
  }
  return count;
}

// Runs the editor program once against the scripted headless backend.
void runEditor() {
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  compiled_editor::Mirror mirror;
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
  mirror.expectSame(result.exitCode);
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

namespace {
std::string writeScratch(const std::string &name, const std::string &contents) {
  const std::string path = scratchPath(name);
  std::ofstream(path, std::ios::binary) << contents;
  return path;
}

// Opens `source` in the editor and saves it under a new name; returns the copy's path.
std::string copyThroughEditor(const std::string &source, const std::string &copyName) {
  const std::string copy = scratchPath(copyName);
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(source);
  headless::pushCommand(CommandSaveAs);
  headless::pushSavePanelAnswer(copy);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  return copy;
}
} // namespace

TEST_CASE("acceptance: unicode text round-trips byte for byte through the editor") {
  const std::string fixture = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/ui/unicode_sample.txt";
  const std::string original = readFile(fixture);
  REQUIRE(original.size() > 200);
  // The fixture really holds each script family as multi-byte UTF-8.
  for (const char *needle : {"caf\xC3\xA9", "\xE6\x97\xA5\xE6\x9C\xAC", "\xF0\x9F\x99\x82", "\xCC\x81",
                             "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D", "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7"}) {
    CHECK_MESSAGE(original.find(needle) != std::string::npos, needle);
  }
  CHECK(readFile(copyThroughEditor(fixture, "unicode_copy.txt")) == original);
}

TEST_CASE("acceptance: line endings and a missing final newline survive the editor") {
  const std::string crlf = "one\r\ntwo\r\n\xE6\x97\xA5\r\n";
  CHECK(readFile(copyThroughEditor(writeScratch("crlf.txt", crlf), "crlf_copy.txt")) == crlf);
  const std::string noNewline = "no final newline \xF0\x9F\x99\x82";
  CHECK(readFile(copyThroughEditor(writeScratch("tail.txt", noNewline), "tail_copy.txt")) == noNewline);
  CHECK(readFile(copyThroughEditor(writeScratch("empty.txt", ""), "empty_copy.txt")).empty());
}

TEST_CASE("acceptance: typed unicode is saved exactly") {
  const std::string path = scratchPath("typed_unicode.txt");
  const std::string text = "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD e\xCC\x81 \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D";
  headless::reset();
  headless::pushTypeText(0, text);
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(path);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(path) == text);
}

TEST_CASE("acceptance: a file that is not valid UTF-8 is refused with the error shown") {
  const std::string bad = writeScratch("bad_utf8.txt", "ok \xC3\x28 broken");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(bad);
  headless::pushAlertAnswer(0);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  bool shown = false;
  for (const auto &entry : headless::callLog()) {
    shown = shown || (entry.rfind("ps_ui_alert(\"The file could not be opened.\", \"", 0) == 0 &&
                      entry.find("not valid UTF-8") != std::string::npos);
  }
  CHECK(shown);
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
}

TEST_CASE("acceptance: the menu bar has the standard macOS structure and shortcuts") {
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const std::vector<std::string> expected{
      "PrimeScroll>About",
      "PrimeScroll>-",
      "PrimeScroll>Settings... (cmd+,)",
      "PrimeScroll>-",
      "PrimeScroll>Hide (cmd+h)",
      "PrimeScroll>Hide Others (cmd+alt+h)",
      "PrimeScroll>-",
      "PrimeScroll>Quit (cmd+q)",
      "File>New Tab (cmd+t)",
      "File>New (cmd+n)",
      "File>Open... (cmd+o)",
      "File>Open Recent>",
      "File>Open Recent>Clear Menu",
      "File>-",
      "File>Save (cmd+s)",
      "File>Save As... (cmd+shift+s)",
      "File>-",
      "File>Close (cmd+w)",
      "Edit>Undo (cmd+z)",
      "Edit>Redo (cmd+shift+z)",
      "Edit>-",
      "Edit>Cut (cmd+x)",
      "Edit>Copy (cmd+c)",
      "Edit>Paste (cmd+v)",
      "Edit>Select All (cmd+a)",
      "Edit>-",
      "Edit>Find... (cmd+f)",
      "Format>Bold (cmd+b)",
      "Format>Italic (cmd+i)",
      "Format>Strikethrough (cmd+shift+x)",
      "Format>Code (cmd+e)",
      "Format>-",
      "Format>Link (cmd+k)",
      "Format>Image",
      "Format>-",
      "Format>Heading>",
      "Format>Heading>Heading 1",
      "Format>Heading>Heading 2",
      "Format>Heading>Heading 3",
      "Format>Heading>-",
      "Format>Heading>Paragraph",
      "Format>Bulleted List",
      "Format>Numbered List",
      "Format>Task List",
      "Format>Quote",
      "Format>Code Block",
      "Format>Horizontal Rule",
      "Format>-",
      "Format>Table>",
      "Format>Table>Insert 2 by 2",
      "Format>Table>Insert 3 by 3",
      "Format>Table>Insert 4 by 3",
      "Format>Table>Insert 5 by 4",
      "Format>Table>-",
      "Format>Table>Format Table",
      "Format>Table>Add Row",
      "Format>Table>Add Column",
      "Format>Table>Delete Row",
      "Format>Table>Delete Column",
      "Format>Table>-",
      "Format>Table>Align Left",
      "Format>Table>Align Center",
      "Format>Table>Align Right",
      "Run>Run (cmd+r)",
      "Run>Stop (cmd+.)",
      "Run>-",
      "Run>Show Output (cmd+alt+o)",
      "View>Enter Full Screen (cmd+ctrl+f)",
      "View>Show Tab Bar",
      "View>Show Line Numbers (cmd+shift+l)",
      "View>Wrap Lines",
      "View>Show Markdown Preview (cmd+shift+m)",
      "View>Editor (cmd+alt+1)",
      "View>Split View (cmd+alt+2)",
      "View>Preview (cmd+alt+3)",
      "View>Scroll Preview>",
      "View>Scroll Preview>Independently",
      "View>Scroll Preview>Linearly",
      "View>Scroll Preview>By Block",
      "View>Scroll Preview>With the Cursor",
      "Window>Minimize (cmd+m)",
      "Window>Zoom",
      "Window>-",
      "Window>Show Next Tab (cmd+shift+])",
      "Window>Show Previous Tab (cmd+shift+[)",
      "Window>Move Tab to New Window",
      "Window>Merge All Windows",
      "Window>-",
      "Window>Show Tab 1 (cmd+1)",
      "Window>Show Tab 2 (cmd+2)",
      "Window>Show Tab 3 (cmd+3)",
      "Window>Show Tab 4 (cmd+4)",
      "Window>Show Tab 5 (cmd+5)",
      "Window>Show Tab 6 (cmd+6)",
      "Window>Show Tab 7 (cmd+7)",
      "Window>Show Tab 8 (cmd+8)",
      "Window>Show Last Tab (cmd+9)",
      "Window>-",
      "Window>Bring All to Front",
      "Help>Help (cmd+?)",
  };
  CHECK(headless::menuBarOutline() == expected);
  // The application menu is registered with its role (it replaces the default one).
  CHECK(countCalls("ps_ui_menu_bar_add_role(") == 3);
  // The editor's commands carry the ids the program dispatches on.
  const auto items = headless::menuBarItems();
  for (const char *item : {"File>New (cmd+n) #1", "File>Open... (cmd+o) #2", "File>Save (cmd+s) #3",
                           "File>Save As... (cmd+shift+s) #4", "File>Close (cmd+w) #5",
                           "View>Show Line Numbers (cmd+shift+l) #7", "View>Wrap Lines #9 [checked]",
                           "File>Open Recent>Clear Menu #210",
                           "PrimeScroll>Settings... (cmd+,) #8"}) {
    CHECK_MESSAGE(std::find(items.begin(), items.end(), item) != items.end(), item);
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
