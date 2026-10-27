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
#include <map>
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
constexpr int32_t CommandPreview = 10;

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
// menus (3 to 10), so the second tab's window and text view are 11 and 12.
constexpr uint64_t SecondWindow = 15;
constexpr uint64_t SecondView = 16;
// Opening a markdown file builds the toolbar first (its handle and the handles of its three menus).
constexpr uint64_t MarkdownSecondWindow = 19;

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
  CHECK(countCalls("ps_ui_window_add_tab(1, 15)") == 1);
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
  headless::pushCommand(CommandNew); // window 17, view 18
  headless::pushTypeText(18, "C");
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
  headless::pushCloseWindow(17);
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
  CHECK(countCalls("ps_ui_text_view_load_file(16, ") == 1);
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
  headless::pushCloseWindow(11);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineNumbersVisible(2) == false);
  CHECK(headless::lineNumbersVisible(2));
  CHECK(headless::lineNumbersVisible(SecondView));
  CHECK(headless::menuItemChecked(7));

  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(7);
  headless::pushCommand(7);
  headless::pushCloseWindow(11);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineNumbersVisible(2));
  CHECK_FALSE(headless::lineNumbersVisible(SecondView));
  CHECK_FALSE(headless::menuItemChecked(7));
}

TEST_CASE("editor starts with the line number bar when the preference says so") {
  headless::reset();
  headless::setBoolPref("lineNumbers", true);
  headless::pushCommand(1);
  headless::pushCloseWindow(11);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::lineNumbersVisible(2));
  CHECK(headless::lineNumbersVisible(SecondView));  // a new tab follows the current setting
  CHECK(headless::menuItemChecked(7));
}

TEST_CASE("editor keeps the line number choice for the next launch") {
  headless::reset();
  headless::pushCommand(7);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::boolPref("lineNumbers", false));
  CHECK(headless::lineNumbersVisible(2));
}

// The settings window (command 8): handles in creation order after the first window, its text view and the
// eight menus: the window is 11, then the General form 12 with the restore check box 13, the Editor form 14
// with the line numbers 14, wrapping 15 and the font size pop-up 16, the Markdown form 17 with the preview
// 18 and the scroll lock 19 check boxes, and the tabs 20.
namespace {
constexpr uint64_t SettingsWindow = 15;
constexpr uint64_t RestoreBox = 17;
constexpr uint64_t NumbersBox = 19;
constexpr uint64_t WrapBox = 20;
constexpr uint64_t SizePopup = 21;
constexpr uint64_t PreviewBox = 23;
constexpr uint64_t ScrollPopup = 24;
constexpr uint64_t SettingsTabs = 25;
// The Files tab: the form 22, the hint 23, the file type list 24, "Make Default" 25.
constexpr uint64_t FileTypeList = 28;
constexpr uint64_t MakeDefaultButton = 29;
constexpr uint64_t TabAfterSettings = 30; // a New command after the settings window: window 30, view 31
constexpr int32_t CommandSettings = 8;

// The file types of the headless backend (examples/apps/text_editor/file_types.txt) and the buttons of the Files tab.
struct HeadlessFileType {
  const char *name;
  const char *extensions;
};
const std::vector<HeadlessFileType> HeadlessFileTypes = {
    {"Plain text", "txt,text"},        {"Markdown", "md,markdown"},
    {"PrimeStruct source", "prime"},   {"JSON", "json"},
    {"YAML", "yml,yaml"},              {"Configuration", "toml,ini,cfg,conf"},
    {"C and C++", "c,h,cc,cpp,hpp,cxx"}, {"Shell scripts", "sh,bash,zsh"},
    {"Python", "py"},                  {"Web scripts and styles", "js,css"},
    {"Logs", "log"},
};

// A row of the Files list: a check mark when the chosen application is the default, then the type and the default.
std::string fileTypeLine(const HeadlessFileType &type, const std::string &defaultName, bool chosenIsDefault) {
  std::string line = chosenIsDefault ? "\xE2\x9C\x93 " : "  ";
  line += type.name;
  line += " (.";
  for (const char *c = type.extensions; *c != '\0'; ++c) {
    line += *c == ',' ? std::string(", .") : std::string(1, *c);
  }
  return line + ") - " + (defaultName.empty() ? std::string("no default") : defaultName);
}

// The outline lines of the Files page. `defaults` maps a type's name to the display name of its default application;
// `selected` is the list's selection ("1,2").
std::vector<std::string> filesPage(const std::string &applicationName, const std::map<std::string, std::string> &defaults,
                                   const std::string &selected) {  // applicationName: the app that shows a check mark
  std::string items;
  for (size_t i = 0; i < HeadlessFileTypes.size(); ++i) {
    const auto found = defaults.find(HeadlessFileTypes[i].name);
    const std::string name = found != defaults.end() ? found->second : std::string();
    items += (i == 0 ? "" : "|") + fileTypeLine(HeadlessFileTypes[i], name, !name.empty() && name == applicationName);
  }
  return {"  page \"Files\":",
          "    form",
          "      heading \"File types\"",
          "      control: label \"Select file types, then press Make Default. macOS asks you to confirm each type.\"",
          "      control: list [" + items + "] selected=" + selected,
          "      control: button \"Make Default\""};
}

std::vector<std::string> settingsOutline(const std::vector<std::string> &base, const std::string &applicationName,
                                         const std::map<std::string, std::string> &defaults = {},
                                         const std::string &selected = "") {
  std::vector<std::string> outline = base;
  const auto files = filesPage(applicationName, defaults, selected);
  outline.insert(outline.end(), files.begin(), files.end());
  return outline;
}
} // namespace

TEST_CASE("settings window shows the preferences in tabs and is not a document") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const std::vector<std::string> expected{
      "tabs selected=0",
      "  page \"General\":",
      "    form",
      "      heading \"Startup\"",
      "      control: checkbox \"Reopen the files of the last session\" [checked]",
      "  page \"Editor\":",
      "    form",
      "      heading \"Text\"",
      "      control: checkbox \"Show line numbers\"",
      "      control: checkbox \"Wrap long lines\" [checked]",
      "      row \"Font size\": popup [11|12|13|14|16|18|20|24] selected=2",
      "  page \"Markdown\":",
      "    form",
      "      heading \"Preview\"",
      "      control: checkbox \"Show the preview when a markdown file opens\"",
      "      row \"Scroll source and preview\": popup [Independently|Linearly|By block|With the cursor] selected=2",
  };
  CHECK(headless::controlOutline(SettingsTabs) == settingsOutline(expected, "PrimeScroll"));
  const auto settings = headless::windowState(SettingsWindow);
  CHECK(settings.title == "Settings");
  CHECK(settings.style == 1);
  // It goes with the last document.
  CHECK(settings.closed);
  CHECK(headless::windowState(EditorWindow).closed);
}

TEST_CASE("settings window opens once and a second Settings command brings it forward") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_window_create(\"Settings\"") == 1);
  CHECK(countCalls("ps_ui_window_show(15)") == 2);
}

TEST_CASE("settings window can be closed and opened again") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(SettingsWindow);
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(SettingsWindow).closed);
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(countCalls("ps_ui_window_create(\"Settings\"") == 2);
}

TEST_CASE("settings toggles line numbers and wrapping in every tab at once") {
  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(CommandSettings);
  headless::pushToggleCheckBox(NumbersBox + 2); // the second tab took the handles 10 and 11
  headless::pushToggleCheckBox(WrapBox + 2);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::boolPref("lineNumbers", false));
  CHECK_FALSE(headless::boolPref("wrapLines", true));
  CHECK(headless::lineNumbersVisible(2));
  CHECK(headless::lineNumbersVisible(SecondView));
  CHECK_FALSE(headless::lineWrapping(2));
  CHECK_FALSE(headless::lineWrapping(SecondView));
  // The View menu follows the check boxes.
  CHECK(headless::menuItemChecked(7));
  CHECK_FALSE(headless::menuItemChecked(9));
}

TEST_CASE("the View menu and the settings window stay in step") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCommand(7);
  headless::pushCommand(9);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::checkBoxChecked(NumbersBox));
  CHECK_FALSE(headless::checkBoxChecked(WrapBox));
  CHECK(headless::lineNumbersVisible(2));
  CHECK_FALSE(headless::lineWrapping(2));
}

TEST_CASE("settings changes the font size of every tab and of new tabs") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushSelectPopupItem(SizePopup, 6);
  headless::pushCommand(1);
  headless::pushCloseWindow(TabAfterSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::intPref("fontSize", 13) == 20);
  CHECK(headless::fontSize(2) == 20);
  CHECK(headless::fontSize(TabAfterSettings + 1) == 20);
  CHECK(headless::popupSelected(SizePopup) == 6);
}

TEST_CASE("the first launch uses the default preferences") {
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::lineNumbersVisible(2));
  CHECK(headless::lineWrapping(2));
  CHECK(headless::fontSize(2) == 13);
  CHECK(headless::previewScrollMode() == 2);
  CHECK_FALSE(headless::menuItemChecked(7));
  CHECK(headless::menuItemChecked(9));
}

TEST_CASE("the saved font size and scroll mode apply at launch") {
  headless::reset();
  headless::setIntPref("fontSize", 18);
  headless::setIntPref("scrollMode", 3);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::fontSize(2) == 18);
  CHECK(headless::previewScrollMode() == 3);
}

TEST_CASE("settings chooses how the preview scrolls with the source") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushSelectPopupItem(ScrollPopup, 1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::intPref("scrollMode", 2) == 1);
  CHECK(headless::previewScrollMode() == 1);
  CHECK(headless::popupSelected(ScrollPopup) == 1);

  headless::reset();
  headless::setIntPref("scrollMode", 3);
  headless::pushCommand(CommandSettings);
  headless::pushSelectPopupItem(ScrollPopup, 0);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::intPref("scrollMode", 2) == 0);
  CHECK(headless::previewScrollMode() == 0);
}

TEST_CASE("restoring the last session can be switched off") {
  const std::string kept = writeScratch("session_kept.txt", "kept");
  headless::reset();
  headless::setPrefList("session.files", {kept});
  headless::pushCommand(CommandSettings);
  headless::pushToggleCheckBox(RestoreBox);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::boolPref("restoreSession", true));
  CHECK(headless::windowState(EditorWindow).title == kept);

  // A launch with the preference off leaves the saved session alone: the window stays untitled.
  headless::reset();
  headless::setPrefList("session.files", {kept});
  headless::setBoolPref("restoreSession", false);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).title == "Untitled");
}

TEST_CASE("a markdown file opens with its preview when the preference is on") {
  const std::string note = writeScratch("preview_note.md", "# Title\n");
  const std::string plain = writeScratch("preview_plain.txt", "text\n");
  headless::reset();
  headless::setBoolPref("previewMarkdown", true);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(note);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(plain);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).preview);
  CHECK_FALSE(headless::windowState(SecondWindow).preview);

  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(note);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
}

TEST_CASE("settings turns the markdown preview preference on and off") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushToggleCheckBox(PreviewBox);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::boolPref("previewMarkdown", false));
  CHECK(headless::checkBoxChecked(PreviewBox));
}

TEST_CASE("the app ends after the last document closes with the settings window open") {
  headless::reset();
  headless::pushCommand(1);
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(EditorWindow);
  headless::pushCloseWindow(SecondWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).closed);
  CHECK(headless::windowState(SecondWindow).closed);
  CHECK(headless::windowState(17).closed); // the settings window, after the second tab's two handles
}

TEST_CASE("commands that arrive while the settings window is active act on a document") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCommand(1);
  headless::pushCloseWindow(TabAfterSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_window_add_tab(") == 1);
  CHECK(headless::windowState(SettingsWindow).style == 1);
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
  CHECK(countCalls("ps_ui_window_select(15)") == 1);
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

TEST_CASE("editor brings back the preview of the tabs that had it on at the last shutdown") {
  const std::string a = writeScratch("session_preview_a.md", "# a");
  const std::string b = writeScratch("session_preview_b.txt", "b");
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::setPrefList("session.previews", {"1", "0"});
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).preview);
  CHECK_FALSE(headless::windowState(SecondWindow).preview);

  // The other way round, and a session saved before previews were remembered has none on.
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::setPrefList("session.previews", {"0", "1"});
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
  CHECK(headless::windowState(SecondWindow).preview);
  headless::reset();
  headless::setPrefList("session.files", {a});
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
}

TEST_CASE("editor remembers which tabs show their preview") {
  const std::string a = writeScratch("session_remember_a.md", "# a");
  const std::string b = writeScratch("session_remember_b.md", "# b");
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::pushCommand(CommandPreview); // the first tab is the active one: its preview goes on
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  // Closing the second tab saved the session with the first tab alone, and its preview on; the last tab's
  // closing leaves the session as it was.
  CHECK(headless::prefList("session.files") == std::vector<std::string>{a});
  CHECK(headless::prefList("session.previews") == std::vector<std::string>{"1"});
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

// The markdown preview: View > Show Markdown Preview splits the active tab, and the page follows its text.
TEST_CASE("editor shows a markdown preview of the active tab and turns it off again") {
  const std::string md = writeScratch("preview.md", "# Heading\n\ntext");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandPreview);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).preview);
  CHECK(headless::previewPage(EditorWindow).find(">Heading</h1>") != std::string::npos);
  CHECK(countCalls("ps_ui_menu_follow_preview(") == 1);

  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandPreview);
  headless::pushCommand(CommandPreview);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
  CHECK(headless::previewPage(EditorWindow).empty());
}

TEST_CASE("editor refreshes the preview as the text changes") {
  const std::string md = writeScratch("preview_typing.md", "# Heading");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandPreview);
  headless::pushTypeText(0, "\n\nmore words");
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::previewPage(EditorWindow).find(">more words</p>") != std::string::npos);
  CHECK(headless::windowState(EditorWindow).previewRenders == 2);
}

TEST_CASE("editor previews only the tabs that asked for it") {
  const std::string md = writeScratch("preview_tabs.md", "# One");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandNew);
  headless::pushCommand(CommandPreview);
  headless::pushTypeText(2, " typed in the first tab");
  headless::pushCloseWindow(MarkdownSecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
  CHECK(headless::windowState(EditorWindow).previewRenders == 0);
  CHECK(headless::windowState(MarkdownSecondWindow).preview);
  CHECK(headless::windowState(MarkdownSecondWindow).previewRenders == 1);
}

TEST_CASE("editor refreshes a preview when a file opens into the tab") {
  const std::string first = writeScratch("preview_open_a.md", "# First");
  const std::string second = writeScratch("preview_open_b.md", "# Second");
  headless::reset();
  headless::pushCommand(CommandPreview);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(first);
  headless::pushCloseWindow(EditorWindow);
  headless::pushCloseWindow(SecondWindow);
  runEditor();
  // The untouched first tab took the file and its preview, already on, shows it.
  CHECK(headless::windowState(EditorWindow).preview);
  CHECK(headless::previewPage(EditorWindow).find(">First</h1>") != std::string::npos);
  (void)second;
}

// A base outline of the other three pages, for the Files tests (the first test pins them whole).
namespace {
std::vector<std::string> otherPages() {
  return {
      "tabs selected=0",
      "  page \"General\":",
      "    form",
      "      heading \"Startup\"",
      "      control: checkbox \"Reopen the files of the last session\" [checked]",
      "  page \"Editor\":",
      "    form",
      "      heading \"Text\"",
      "      control: checkbox \"Show line numbers\"",
      "      control: checkbox \"Wrap long lines\" [checked]",
      "      row \"Font size\": popup [11|12|13|14|16|18|20|24] selected=2",
      "  page \"Markdown\":",
      "    form",
      "      heading \"Preview\"",
      "      control: checkbox \"Show the preview when a markdown file opens\"",
      "      row \"Scroll source and preview\": popup [Independently|Linearly|By block|With the cursor] selected=2",
  };
}
} // namespace

TEST_CASE("settings makes the app the default for the selected file types") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushSelectListRows(FileTypeList, {1, 2}); // Markdown, PrimeStruct source
  headless::pushClickButton(MakeDefaultButton);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::defaultApp("md") == "/Headless/PrimeScroll.app");
  CHECK(headless::defaultApp("markdown") == "/Headless/PrimeScroll.app");
  CHECK(headless::defaultApp("prime") == "/Headless/PrimeScroll.app");
  CHECK(headless::defaultApp("txt").empty());
  CHECK(headless::defaultApp("json").empty());
  CHECK(headless::controlOutline(SettingsTabs) ==
        settingsOutline(otherPages(), "PrimeScroll", {{"Markdown", "PrimeScroll"}, {"PrimeStruct source", "PrimeScroll"}},
                        "1,2"));
}

TEST_CASE("Make Default asks the system once, for every selected type together") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushSelectListRows(FileTypeList, {0, 1, 2});
  headless::pushClickButton(MakeDefaultButton);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_set_default_app(") == 1);
  CHECK(countCalls("ps_ui_set_default_app(\"txt,text,md,markdown,prime\"") == 1);
}

TEST_CASE("selecting file types alone changes nothing; the button applies the selection") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushSelectListRows(FileTypeList, {1});
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_set_default_app(") == 0);
  CHECK(headless::listSelection(FileTypeList) == std::vector<int32_t>{1});

  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushClickButton(MakeDefaultButton); // nothing selected
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_set_default_app(") == 0);
  CHECK(headless::defaultApp("md").empty());
}

TEST_CASE("settings makes the app the default for all file types when every row is selected") {
  headless::reset();
  headless::pushCommand(CommandSettings);
  std::vector<int32_t> rows;
  std::string joined;
  for (size_t i = 0; i < HeadlessFileTypes.size(); ++i) {
    rows.push_back(static_cast<int32_t>(i));
    joined += (i == 0 ? "" : ",") + std::to_string(i);
  }
  headless::pushSelectListRows(FileTypeList, rows);
  headless::pushClickButton(MakeDefaultButton);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  std::map<std::string, std::string> defaults;
  for (const auto &type : HeadlessFileTypes) {
    defaults[type.name] = "PrimeScroll";
    std::string list = type.extensions;
    for (size_t from = 0; from <= list.size();) {
      size_t comma = list.find(',', from);
      comma = comma == std::string::npos ? list.size() : comma;
      CHECK_MESSAGE(headless::defaultApp(list.substr(from, comma - from)) == "/Headless/PrimeScroll.app", list);
      from = comma + 1;
    }
  }
  CHECK(headless::controlOutline(SettingsTabs) == settingsOutline(otherPages(), "PrimeScroll", defaults, joined));
}

// The launch time must not change: nothing of the file associations is asked before the settings window is built.
TEST_CASE("the file types are not looked up before the settings window opens") {
  const char *calls[] = {"ps_ui_document_type_", "ps_ui_default_app(", "ps_ui_set_default_app(", "ps_ui_app_path(", "ps_ui_button_", "ps_ui_list_"};
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  for (const char *call : calls) {
    CHECK_MESSAGE(countCalls(call) == 0, call);
  }
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_document_type_count(") >= 1);
  CHECK(countCalls("ps_ui_default_app(") >= 1);
}

TEST_CASE("a file the system asks to open opens in the untitled tab") {
  const std::string a = writeScratch("request_a.txt", "a");
  headless::reset();
  headless::pushOpenFile(a);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).title == a);
  CHECK(headless::windowPath(EditorWindow) == a);
  REQUIRE(headless::recentFiles().size() == 1);
  CHECK(headless::recentFiles()[0] == a);
  CHECK(countCalls("ps_ui_window_create(\"Untitled\"") == 1);
}

TEST_CASE("a file that is already open is selected instead of opened again") {
  const std::string a = writeScratch("request_same_a.txt", "a");
  const std::string b = writeScratch("request_same_b.txt", "b");
  headless::reset();
  headless::pushOpenFile(a);
  headless::pushOpenFile(a);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_window_create(\"Untitled\"") == 1);

  // Another file gets a tab of its own, and asking for the first again selects the first tab.
  headless::reset();
  headless::pushOpenFile(a);
  headless::pushOpenFile(b);
  headless::pushOpenFile(a);
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(countCalls("ps_ui_window_create(\"Untitled\"") == 2);
  CHECK(headless::windowPath(SecondWindow) == b);
  CHECK(countCalls("ps_ui_window_select(1)") >= 1);
}

TEST_CASE("a file the system asks to open goes to a document while the settings window is active") {
  const std::string a = writeScratch("request_settings.txt", "a");
  headless::reset();
  headless::pushCommand(CommandSettings);
  headless::pushOpenFile(a);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowPath(EditorWindow) == a);
  CHECK(headless::windowPath(SettingsWindow).empty());
}

// The markdown toolbar (docs/NativeUiPlan.md section 17): shown for markdown windows only, with the view modes and
// the format commands; the Format menu offers the same commands.
namespace {
// Command ids of the markdown toolbar.
constexpr int32_t CommandMode = 30;   // + 0 editor, 1 split, 2 preview
constexpr int32_t CommandFormat = 40; // + the operation of ps_ui_text_view_markdown_edit
constexpr int32_t CommandHeading = 70; // + the level
constexpr int32_t CommandTable = 80;  // + 0 ... 3, the sizes
constexpr int32_t CommandAlign = 90;  // + 1 left, 2 center, 3 right
constexpr uint64_t EditorView = 2;

std::string viewText(uint64_t view) { return ps_ui_text_view_get_text(view); }

// Opens `path` in the first tab, runs the queued commands and closes it without saving.
void runMarkdownEditor(const std::string &path, const std::vector<int32_t> &commands) {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  for (const int32_t command : commands) {
    headless::pushCommand(command);
  }
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
}
} // namespace

TEST_CASE("editor shows the markdown toolbar for markdown windows only") {
  const std::string md = writeScratch("toolbar_shown.md", "# t");
  const std::string markdown = writeScratch("toolbar_shown.markdown", "# t");
  const std::string txt = writeScratch("toolbar_hidden.txt", "t");
  runMarkdownEditor(md, {});
  CHECK(headless::windowState(EditorWindow).toolbar != 0);
  runMarkdownEditor(markdown, {});
  CHECK(headless::windowState(EditorWindow).toolbar != 0);
  runMarkdownEditor(txt, {});
  CHECK(headless::windowState(EditorWindow).toolbar == 0);
  CHECK(headless::toolbarOutline(EditorWindow).empty());
  // The toolbar is not built until a window wants it.
  CHECK(countCalls("ps_ui_toolbar_create(") == 0);

  // A text file with its preview on has it too, and loses it again with the preview.
  runMarkdownEditor(txt, {CommandPreview});
  CHECK(headless::windowState(EditorWindow).toolbar != 0);
  runMarkdownEditor(txt, {CommandPreview, CommandPreview});
  CHECK(headless::windowState(EditorWindow).toolbar == 0);

  // An untitled window has none, and Save As to a markdown name gives it one.
  headless::reset();
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).toolbar == 0);
  CHECK(countCalls("ps_ui_toolbar_create(") == 0);
  headless::reset();
  headless::pushTypeText(EditorView, "text");
  headless::pushCommand(CommandSaveAs);
  headless::pushSavePanelAnswer(scratchPath("toolbar_saved_as.md"));
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).toolbar != 0);
}

TEST_CASE("editor's toolbar offers the view modes and the markdown formats") {
  const std::string md = writeScratch("toolbar_outline.md", "# t");
  runMarkdownEditor(md, {});
  const std::vector<std::string> outline{"segments Editor|Split|Preview #30 [selected 0]",
                                         "-",
                                         "button Bold [bold] #40",
                                         "button Italic [italic] #41",
                                         "button Strikethrough [strikethrough] #42",
                                         "button Code [chevron.left.forwardslash.chevron.right] #43",
                                         "button Link [link] #44",
                                         "button Image [photo] #45",
                                         "-",
                                         "menu Heading [textformat.size]",
                                         "  Heading 1 #71",
                                         "  Heading 2 #72",
                                         "  Heading 3 #73",
                                         "  -",
                                         "  Paragraph #70",
                                         "menu Blocks [list.bullet]",
                                         "  Bulleted List #47",
                                         "  Numbered List #48",
                                         "  Task List #49",
                                         "  -",
                                         "  Quote #50",
                                         "  Code Block #51",
                                         "  Horizontal Rule #52",
                                         "-",
                                         "menu Table [tablecells]",
                                         "  Insert 2 by 2 #80",
                                         "  Insert 3 by 3 #81",
                                         "  Insert 4 by 3 #82",
                                         "  Insert 5 by 4 #83",
                                         "  -",
                                         "  Format Table #54",
                                         "  Add Row #55",
                                         "  Add Column #56",
                                         "  Delete Row #57",
                                         "  Delete Column #58",
                                         "  -",
                                         "  Align Left #91",
                                         "  Align Center #92",
                                         "  Align Right #93"};
  CHECK(headless::toolbarOutline(EditorWindow) == outline);
  // One toolbar serves every markdown window.
  CHECK(countCalls("ps_ui_toolbar_create(") == 1);
}

TEST_CASE("editor's view modes switch between the text, both and the preview alone") {
  const std::string md = writeScratch("toolbar_modes.md", "# t");
  runMarkdownEditor(md, {CommandMode + 1});
  CHECK(headless::windowState(EditorWindow).previewMode == PS_UI_PREVIEW_SPLIT);
  CHECK(headless::toolbarOutline(EditorWindow).front() == "segments Editor|Split|Preview #30 [selected 1]");
  runMarkdownEditor(md, {CommandMode + 2});
  CHECK(headless::windowState(EditorWindow).previewMode == PS_UI_PREVIEW_ONLY);
  CHECK(headless::windowState(EditorWindow).preview);
  CHECK(headless::toolbarOutline(EditorWindow).front() == "segments Editor|Split|Preview #30 [selected 2]");
  runMarkdownEditor(md, {CommandMode + 2, CommandMode});
  CHECK(headless::windowState(EditorWindow).previewMode == PS_UI_PREVIEW_TEXT);
  CHECK_FALSE(headless::windowState(EditorWindow).preview);
  // The markdown file keeps its toolbar in the text mode.
  CHECK(headless::windowState(EditorWindow).toolbar != 0);
}

TEST_CASE("editor's format commands change the selected text") {
  const std::string md = writeScratch("toolbar_format.md", "hello world");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushSelectText(EditorView, 0, 5);
  headless::pushCommand(CommandFormat);       // bold
  headless::pushCommand(CommandHeading + 2);  // a level 2 heading
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(viewText(EditorView) == "## **hello** world");
  CHECK(headless::windowState(EditorWindow).edited);

  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushSelectText(EditorView, 6, 11);
  headless::pushCommand(CommandFormat + 1);   // italic
  headless::pushCommand(CommandFormat + 7);   // a bulleted list
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(viewText(EditorView) == "- hello *world*");
}

TEST_CASE("editor inserts and edits tables with the Table menu") {
  const std::string md = writeScratch("toolbar_table.md", "");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandTable);           // 2 by 2
  headless::pushCommand(CommandFormat + 15);     // add a row
  headless::pushCommand(CommandAlign + 3);       // align the first column right
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  const std::string table = viewText(EditorView);
  CHECK(table.find("| Column 1 | Column 2 |") != std::string::npos);
  CHECK(table.find("| -------: | -------- |") != std::string::npos);
  // Header, delimiter row and 2 + 1 body rows.
  CHECK(std::count(table.begin(), table.end(), '\n') >= 4);

  // A table command outside a table does nothing.
  const std::string plain = writeScratch("toolbar_not_table.md", "text");
  runMarkdownEditor(plain, {CommandFormat + 14, CommandFormat + 15});
  CHECK(viewText(EditorView) == "text");
}

TEST_CASE("editor's format command in the preview-only mode shows the text again") {
  const std::string md = writeScratch("toolbar_only_format.md", "word");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(CommandMode + 2);
  headless::pushSelectText(EditorView, 0, 4);
  headless::pushCommand(CommandFormat);
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(viewText(EditorView) == "**word**");
  CHECK(headless::windowState(EditorWindow).previewMode == PS_UI_PREVIEW_SPLIT);
}

TEST_CASE("editor remembers the preview mode of every tab") {
  const std::string a = writeScratch("session_mode_a.md", "# a");
  const std::string b = writeScratch("session_mode_b.md", "# b");
  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::pushCommand(CommandMode + 2); // the first tab is the active one
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::prefList("session.previews") == std::vector<std::string>{"2"});

  headless::reset();
  headless::setPrefList("session.files", {a, b});
  headless::setPrefList("session.previews", {"2", "1"});
  headless::pushCloseWindow(SecondWindow);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
  CHECK(headless::windowState(EditorWindow).previewMode == PS_UI_PREVIEW_ONLY);
  CHECK(headless::windowState(SecondWindow).previewMode == PS_UI_PREVIEW_SPLIT);
  CHECK(headless::toolbarOutline(EditorWindow).front() == "segments Editor|Split|Preview #30 [selected 2]");
  CHECK(headless::toolbarOutline(SecondWindow).front() == "segments Editor|Split|Preview #30 [selected 1]");
}

// Running scripts (docs/NativeUiPlan.md section 18): a PrimeStruct file gets a toolbar with Run and Stop, and Run shows
// the output under the editor.
namespace {
constexpr int32_t CommandRun = 120;
constexpr int32_t CommandStop = 121;
constexpr int32_t CommandOutput = 122;

// Like runMarkdownEditor, and lets a running script end (its output arrives) before the window closes.
void runScriptEditor(const std::string &path, const std::vector<int32_t> &commands) {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  for (const int32_t command : commands) {
    headless::pushCommand(command);
  }
  headless::pushWaitScript();
  headless::pushAlertAnswer(1);
  headless::pushCloseWindow(EditorWindow);
  runEditor();
}

const char *HelloScript = "[return<i32> effects(io_out)]\nmain() {\n  print_line(\"hello from the editor\"utf8)\n"
                          "  return(3i32)\n}\n";
const char *ForeverScript = "[return<i32>]\nmain() {\n  [mut] n{0i32}\n  while(true) {\n    n = n + 1i32\n  }\n"
                            "  return(n)\n}\n";
} // namespace

TEST_CASE("editor shows a Run toolbar for PrimeStruct files only") {
  const std::string script = writeScratch("run_toolbar.prime", HelloScript);
  const std::string txt = writeScratch("run_toolbar.txt", "t");
  runMarkdownEditor(script, {});
  const std::vector<std::string> outline = {"button Run [play.fill] #120",
                                            "button Stop [stop.fill] #121",
                                            "-",
                                            "button Output [rectangle.bottomhalf.inset.filled] #122"};
  CHECK(headless::toolbarOutline(EditorWindow) == outline);
  runMarkdownEditor(txt, {});
  CHECK(headless::toolbarOutline(EditorWindow).empty());
}

TEST_CASE("editor's Run shows the script's output under the text") {
  const std::string script = writeScratch("run_output.prime", HelloScript);
  runScriptEditor(script, {CommandRun});
  const auto state = headless::windowState(EditorWindow);
  CHECK(state.outputVisible);
  CHECK(state.output == "hello from the editor\n[exit code 3]\n");
  CHECK_FALSE(state.scriptRunning);
}

TEST_CASE("editor's Run reports a broken script") {
  const std::string script =
      writeScratch("run_broken.prime", "[return<i32>]\nmain() {\n  return(missing_helper())\n}\n");
  runScriptEditor(script, {CommandRun});
  const std::string output = headless::windowState(EditorWindow).output;
  CHECK(output.find("missing_helper") != std::string::npos);
}

TEST_CASE("editor's Stop ends a script that does not end and Show Output toggles the pane") {
  const std::string script = writeScratch("run_forever.prime", ForeverScript);
  runScriptEditor(script, {CommandRun, CommandStop});
  CHECK(headless::windowState(EditorWindow).output == "[stopped]\n");
  runMarkdownEditor(script, {CommandOutput});
  CHECK(headless::windowState(EditorWindow).outputVisible);
  runMarkdownEditor(script, {CommandOutput, CommandOutput});
  CHECK_FALSE(headless::windowState(EditorWindow).outputVisible);
}
