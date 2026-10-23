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
  compiled_editor::Mirror mirror;
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
  mirror.expectSame(result.exitCode);
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
  // The whole menu structure is asserted by the editor acceptance gate.
  const auto items = headless::menuBarItems();
  CHECK(std::find(items.begin(), items.end(), "File>Save As... (cmd+shift+s) #4") != items.end());
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

TEST_CASE("editor opens the file named on the command line") {
  const std::string in = writeScratch("argument.txt", "from the command line\n");
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  const Script script = compileEditor();
  compiled_editor::Mirror mirror;
  const auto result = script.run({in});
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode, {in});
  CHECK(result.exitCode == 0);
  const auto window = headless::windowState(EditorWindow);
  CHECK(window.title == in);
  CHECK_FALSE(window.edited);
  CHECK(countCalls("ps_ui_text_view_load_file(2, ") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor reports a command line file that cannot be opened") {
  headless::reset();
  headless::pushAlertAnswer(0);
  headless::pushCloseWindow(EditorWindow);
  const Script script = compileEditor();
  compiled_editor::Mirror mirror;
  const auto result = script.run({"/definitely/not/here.txt"});
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode, {"/definitely/not/here.txt"});
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
  CHECK(countCalls("ps_ui_alert(\"The file could not be opened.\"") == 1);
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

// The editor creates its first window and text view (handles 1 and 2) and then six
// menus (3 to 9), so the second tab's window and text view are 10 and 11.
constexpr uint64_t SecondWindow = 10;
constexpr uint64_t SecondView = 11;

TEST_CASE("editor new opens a second tab in the same tab group") {
  headless::reset();
  headless::pushCommand(CommandNew);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::tabGroup(EditorWindow).empty()); // both closed again
  const auto second = headless::windowState(SecondWindow);
  CHECK(second.exists);
  CHECK(second.title == "Untitled");
  CHECK(second.content == SecondView);
  CHECK(second.closed);
  CHECK(headless::isMonospace(SecondView));
  CHECK(countCalls("ps_ui_window_add_tab(1, 10)") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor tabs keep their own paths and save independently") {
  const std::string pathA = scratchPath("tab_a.txt");
  const std::string pathB = scratchPath("tab_b.txt");
  headless::reset();
  headless::pushTypeText(EditorWindow + 1, "A");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(pathA);
  headless::pushCommand(CommandNew);
  headless::pushTypeText(SecondView, "B");
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(pathB);
  headless::pushActivateWindow(EditorWindow);
  headless::pushTypeText(EditorWindow + 1, "2");
  headless::pushCommand(CommandSave);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(readFile(pathA) == "A2");
  CHECK(readFile(pathB) == "B");
  CHECK(headless::windowPath(EditorWindow) == pathA);
  CHECK(headless::windowPath(SecondWindow) == pathB);
  CHECK(headless::windowState(EditorWindow).title == pathA);
  CHECK(headless::windowState(SecondWindow).title == pathB);
  CHECK(countCalls("ps_ui_save_panel(") == 2); // the third save reused tab A's path
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor Command-number selects that tab and Command-9 the last") {
  const std::string pathA = scratchPath("number_a.txt");
  const std::string pathB = scratchPath("number_b.txt");
  const std::string pathC = scratchPath("number_c.txt");
  constexpr int32_t CommandTab = 100;
  headless::reset();
  headless::pushTypeText(EditorWindow + 1, "A");
  headless::pushCommand(CommandNew);
  headless::pushTypeText(SecondView, "B");
  headless::pushCommand(CommandNew); // window 12, view 13
  headless::pushTypeText(13, "C");
  headless::pushCommand(CommandTab + 1); // Command-1: the first tab
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(pathA);
  headless::pushCommand(CommandTab + 2); // Command-2: the second tab
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(pathB);
  headless::pushCommand(CommandTab + 9); // Command-9: the last tab
  headless::pushCommand(CommandSave);
  headless::pushSavePanelAnswer(pathC);
  headless::pushCommand(CommandTab + 5); // no fifth tab: nothing changes
  headless::pushCloseWindow(EditorWindow);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(12);
  runEditor();
  CHECK(readFile(pathA) == "A");
  CHECK(readFile(pathB) == "B");
  CHECK(readFile(pathC) == "C");
  CHECK(countCalls("ps_ui_window_select(") == 3);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor open reuses an untouched untitled tab and otherwise opens a new one") {
  const std::string first = writeScratch("reuse_first.txt", "first");
  const std::string second = writeScratch("reuse_second.txt", "second");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(first);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(second);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == first);
  CHECK(headless::windowPath(SecondWindow) == second);
  CHECK(countCalls("ps_ui_window_add_tab(") == 1);
  CHECK(countCalls("ps_ui_text_view_load_file(2, ") == 1);
  CHECK(countCalls("ps_ui_text_view_load_file(11, ") == 1);
}

TEST_CASE("editor closes a failed open's new tab again") {
  const std::string first = writeScratch("fail_first.txt", "first");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(first);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer("/definitely/not/here.txt");
  headless::pushAlertAnswer(0);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(SecondWindow).closed);
  CHECK(headless::windowPath(EditorWindow) == first);
  CHECK(countCalls("ps_ui_alert(\"The file could not be opened.\"") == 1);
}

TEST_CASE("editor closing one tab asks about that tab only") {
  headless::reset();
  headless::pushCommand(CommandNew);
  headless::pushTypeText(SecondView, "dirty second");
  headless::pushCloseWindow(EditorWindow); // the first tab is clean: closes without asking
  headless::pushCloseWindow(SecondWindow);
  headless::pushAlertAnswer(1);
  runEditor();
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(headless::windowState(SecondWindow).closed);
  CHECK(countCalls("ps_ui_alert(") == 1);
}

TEST_CASE("editor quit asks about every dirty tab and Cancel stops it") {
  headless::reset();
  headless::pushTypeText(EditorWindow + 1, "x");
  headless::pushCommand(CommandNew);
  headless::pushTypeText(SecondView, "y");
  headless::pushQuit();
  headless::pushAlertAnswer(1); // first tab: Don't Save
  headless::pushAlertAnswer(2); // second tab: Cancel, the quit stops
  headless::pushAlertAnswer(1); // quit again once the queue ran dry: Don't Save
  runEditor();
  CHECK(countCalls("ps_ui_alert(") == 3);
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(headless::windowState(SecondWindow).closed);
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


// Handles: the first window is 1 with text view 2, the menus follow, the second tab's window is 9
// with text view 11. Commands: 1 New, 7 View > Show Line Numbers, 8 Start with Line Numbers.
TEST_CASE("editor toggles the line number bar in every tab from the View menu") {
  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(7);
  headless::pushCloseWindow(10);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineNumbersVisible(2) == false);
  CHECK(headless::lineNumbersVisible(2));
  CHECK(headless::lineNumbersVisible(11));
  CHECK(headless::menuItemChecked(7));

  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(7);
  headless::pushCommand(7);
  headless::pushCloseWindow(10);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineNumbersVisible(2));
  CHECK_FALSE(headless::lineNumbersVisible(11));
  CHECK_FALSE(headless::menuItemChecked(7));
}

TEST_CASE("editor starts with the line number bar when the preference says so") {
  headless::reset();
  headless::setBoolPref("lineNumbers", true);
  headless::pushCommand(1);
  headless::pushCloseWindow(10);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::lineNumbersVisible(2));
  CHECK(headless::lineNumbersVisible(11));  // a new tab follows the current setting
  CHECK(headless::menuItemChecked(7));
  CHECK(headless::menuItemChecked(8));
}

TEST_CASE("editor remembers the start-up choice without changing the open tabs") {
  headless::reset();
  headless::pushCommand(8);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::boolPref("lineNumbers", false));
  CHECK_FALSE(headless::lineNumbersVisible(2));
  CHECK(headless::menuItemChecked(8));
  CHECK_FALSE(headless::menuItemChecked(7));

  headless::reset();
  headless::setBoolPref("lineNumbers", true);
  headless::pushCommand(8);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::boolPref("lineNumbers", true));
  CHECK(headless::lineNumbersVisible(2));
  CHECK_FALSE(headless::menuItemChecked(8));
}


// Commands: 9 View > Wrap Lines, 200 to 209 the recent files, 210 Clear Menu.
TEST_CASE("editor wraps lines until View > Wrap Lines is switched off, in every tab") {
  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(9);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineWrapping(2));
  CHECK_FALSE(headless::lineWrapping(SecondView));
  CHECK_FALSE(headless::menuItemChecked(9));

  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(9);
  headless::pushCommand(9);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::lineWrapping(2));
  CHECK(headless::lineWrapping(SecondView));
  CHECK(headless::menuItemChecked(9));
}

TEST_CASE("editor lists the recent files under File > Open Recent") {
  const std::string a = writeScratch("recent_a.txt", "aaa");
  const std::string b = writeScratch("recent_b.txt", "bbb");
  headless::reset();
  headless::pushRecentFile(a);
  headless::pushRecentFile(b);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const auto items = headless::menuBarItems();
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>recent_b.txt #200") != items.end());
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>recent_a.txt #201") != items.end());
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>Clear Menu #210") != items.end());
}

TEST_CASE("editor adds a file it opened or saved to the recent files") {
  const std::string opened = writeScratch("recent_opened.txt", "text");
  const std::string saved = scratchPath("recent_saved.txt");
  headless::reset();
  headless::pushCommand(2);
  headless::pushOpenPanelAnswer(opened);
  headless::pushCommand(1);
  headless::pushTypeText(SecondView, "new text");
  headless::pushCommand(3);
  headless::pushSavePanelAnswer(saved);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const auto recent = headless::recentFiles();
  REQUIRE(recent.size() == 2);
  CHECK(recent[0] == saved);
  CHECK(recent[1] == opened);
  const auto items = headless::menuBarItems();
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>recent_saved.txt #200") != items.end());
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>recent_opened.txt #201") != items.end());
}

TEST_CASE("editor reopens a recent file from the menu and can clear the list") {
  const std::string b = writeScratch("recent_again.txt", "again");
  headless::reset();
  headless::pushRecentFile(b);
  headless::pushCommand(200);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == b);
  CHECK(headless::windowState(EditorWindow).title == b);
  CHECK(countCalls("ps_ui_text_view_load_file(2, ") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);

  headless::reset();
  headless::pushRecentFile(b);
  headless::pushCommand(210);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::recentFiles().empty());
  const auto items = headless::menuBarItems();
  for (const std::string &item : items) {
    CHECK(item.rfind("File>Open Recent>recent_again.txt", 0) != 0);
  }
  CHECK(std::find(items.begin(), items.end(), "File>Open Recent>Clear Menu #210") != items.end());
}


// The session: the open files in tab order and the active tab come back at the next launch.
TEST_CASE("editor remembers its open files when it quits") {
  const std::string a = writeScratch("session_a.txt", "a");
  const std::string b = writeScratch("session_b.txt", "b");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(a);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(b);
  headless::pushQuit();
  runEditor();
  CHECK(headless::prefList("session.files") == std::vector<std::string>{a, b});
  CHECK(headless::intPref("session.active", -1) == 1);
}

TEST_CASE("editor brings back the last session's tabs and selects the active one") {
  const std::string a = writeScratch("restore_a.txt", "a");
  const std::string b = writeScratch("restore_b.txt", "b");
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::setIntPref("session.active", 1);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == a);
  CHECK(headless::windowPath(SecondWindow) == b);
  CHECK(headless::windowState(SecondWindow).title == b);
  CHECK(countCalls("ps_ui_window_select(10)") == 1);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor skips a remembered file that is gone") {
  const std::string b = writeScratch("restore_only.txt", "b");
  headless::reset();
  headless::setPrefList("session.files", {"/definitely/not/here.txt", b});
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == b);
  CHECK_FALSE(headless::windowState(SecondWindow).exists);
  CHECK(countCalls("ps_ui_alert(") == 0);
}

TEST_CASE("editor opens only the file named on the command line, not the last session") {
  const std::string a = writeScratch("restore_not_a.txt", "a");
  const std::string b = writeScratch("restore_named.txt", "b");
  headless::reset();
  headless::setPrefList("session.files", {a});
  headless::pushCloseWindow(EditorWindow);
  const Script script = compileEditor();
  compiled_editor::Mirror mirror;
  const auto result = script.run({b});
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode, {b});
  CHECK(headless::windowPath(EditorWindow) == b);
  CHECK_FALSE(headless::windowState(SecondWindow).exists);
}

TEST_CASE("editor keeps the session when the last tab is closed, as the app ends like on quit") {
  const std::string a = writeScratch("restore_closed.txt", "a");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(a);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == a);
  CHECK(headless::prefList("session.files") == std::vector<std::string>{a});

  // A session that was restored and closed straight away is still there for the next launch.
  headless::reset();
  headless::setPrefList("session.files", {a});
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::prefList("session.files") == std::vector<std::string>{a});
}

TEST_CASE("editor drops a closed tab from the session but keeps the last one") {
  const std::string a = writeScratch("restore_keep_a.txt", "a");
  const std::string b = writeScratch("restore_keep_b.txt", "b");
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::prefList("session.files") == std::vector<std::string>{a});
}

TEST_CASE("editor asks to remember the first window's size and position") {
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).autosaveName == "PrimeScroll");
  CHECK_FALSE(headless::windowState(SecondWindow).exists);
}
