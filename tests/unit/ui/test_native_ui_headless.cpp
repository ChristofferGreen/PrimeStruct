#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace headless = primec::ui::headless;

TEST_SUITE_BEGIN("primestruct.ui.headless");

namespace {
// Fresh backend with the app started.
void startApp() {
  headless::reset();
  REQUIRE(ps_ui_init("Test"));
}

bool logContains(const std::string &line) {
  for (const auto &entry : headless::callLog()) {
    if (entry == line) {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("headless backend reports its version and platform") {
  startApp();
  CHECK(ps_ui_abi_version() == PS_UI_ABI_VERSION);
  CHECK(ps_ui_platform() == PS_UI_PLATFORM_HEADLESS);
}

TEST_CASE("headless backend starts once and rejects calls before init") {
  headless::reset();
  CHECK(ps_ui_window_create("Early", 100, 100) == 0);
  CHECK(ps_ui_text_view_create() == 0);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_NONE);
  CHECK(ps_ui_init("Test"));
  CHECK_FALSE(ps_ui_init("Again"));
  CHECK(ps_ui_window_create("Late", 100, 100) != 0);
}

TEST_CASE("headless backend calls from another thread fail") {
  startApp();
  uint64_t foreignWindow = 1;
  std::thread other([&] { foreignWindow = ps_ui_window_create("Foreign", 100, 100); });
  other.join();
  CHECK(foreignWindow == 0);
  CHECK(ps_ui_window_create("Own", 100, 100) != 0);
}

TEST_CASE("headless windows hold a text view and track title and edited state") {
  startApp();
  const uint64_t window = ps_ui_window_create("Untitled", 720, 480);
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(window != 0);
  REQUIRE(view != 0);
  CHECK(window != view);
  CHECK(ps_ui_window_set_content(window, view));
  CHECK(ps_ui_window_set_title(window, "notes.txt"));
  CHECK(ps_ui_window_set_edited(window, true));
  CHECK(ps_ui_window_show(window));
  const auto state = headless::windowState(window);
  CHECK(state.title == "notes.txt");
  CHECK(state.width == 720);
  CHECK(state.height == 480);
  CHECK(state.content == view);
  CHECK(state.edited);
  CHECK(state.shown);
  CHECK_FALSE(state.closed);
}

TEST_CASE("headless windows reject bad sizes handles and shared content") {
  startApp();
  CHECK(ps_ui_window_create("Flat", 0, 100) == 0);
  CHECK(ps_ui_window_create("Thin", 100, -1) == 0);
  const uint64_t first = ps_ui_window_create("First", 100, 100);
  const uint64_t second = ps_ui_window_create("Second", 100, 100);
  const uint64_t view = ps_ui_text_view_create();
  CHECK_FALSE(ps_ui_window_set_content(first, 999));
  CHECK_FALSE(ps_ui_window_set_content(999, view));
  CHECK_FALSE(ps_ui_window_set_content(first, second));
  CHECK(ps_ui_window_set_content(first, view));
  CHECK_FALSE(ps_ui_window_set_content(second, view));
  CHECK_FALSE(ps_ui_window_set_title(999, "x"));
  CHECK_FALSE(ps_ui_window_show(0));
}

TEST_CASE("headless closed windows invalidate their handle") {
  startApp();
  const uint64_t window = ps_ui_window_create("Doomed", 100, 100);
  CHECK(ps_ui_window_close(window));
  CHECK_FALSE(ps_ui_window_close(window));
  CHECK_FALSE(ps_ui_window_show(window));
  CHECK_FALSE(ps_ui_window_set_title(window, "Zombie"));
  const auto state = headless::windowState(window);
  CHECK(state.closed);
  CHECK(state.title == "Doomed");
}

TEST_CASE("headless text views store text and flags") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  CHECK(std::string(ps_ui_text_view_get_text(view)).empty());
  CHECK(ps_ui_text_view_set_text(view, "héllo\nworld"));
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "héllo\nworld");
  CHECK_FALSE(ps_ui_text_view_is_modified(view));
  CHECK_FALSE(headless::isMonospace(view));
  CHECK(ps_ui_text_view_set_monospace(view, true));
  CHECK(headless::isMonospace(view));
  CHECK(std::string(ps_ui_text_view_get_text(999)).empty());
  CHECK_FALSE(ps_ui_text_view_set_text(999, "x"));
  CHECK_FALSE(ps_ui_text_view_is_modified(999));
  CHECK_FALSE(ps_ui_text_view_clear_modified(999));
}

namespace {
std::string scratchFile(const std::string &name, const std::string &contents) {
  const std::filesystem::path path = primec::testing::testScratchDir("native_ui_headless") / name;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << contents;
  return path.string();
}

std::string readScratchFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("headless text views load UTF-8 files without marking them modified") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  const std::string path = scratchFile("load.txt", "h\xC3\xA9llo\nw\xC3\xB6rld \xF0\x9F\x99\x82\n");
  REQUIRE(ps_ui_text_view_set_text(view, "old"));
  headless::pushTypeText(view, "!");
  REQUIRE(ps_ui_wait_event() == PS_UI_EVENT_TEXT_CHANGED);
  REQUIRE(ps_ui_text_view_is_modified(view));
  CHECK(ps_ui_text_view_load_file(view, path.c_str()));
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "h\xC3\xA9llo\nw\xC3\xB6rld \xF0\x9F\x99\x82\n");
  CHECK_FALSE(ps_ui_text_view_is_modified(view));
  CHECK(std::string(ps_ui_last_error()).empty());
}

TEST_CASE("headless text views report files they cannot load") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_text_view_set_text(view, "keep"));
  CHECK_FALSE(ps_ui_text_view_load_file(view, "/definitely/not/here.txt"));
  CHECK(std::string(ps_ui_last_error()).find("cannot open /definitely/not/here.txt") == 0);
  const std::string bad = scratchFile("bad.txt", "ok \xC3\x28 broken");
  CHECK_FALSE(ps_ui_text_view_load_file(view, bad.c_str()));
  CHECK(std::string(ps_ui_last_error()).find("not valid UTF-8") != std::string::npos);
  const std::string truncated = scratchFile("truncated.txt", "cut \xE2\x82");
  CHECK_FALSE(ps_ui_text_view_load_file(view, truncated.c_str()));
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "keep");
  CHECK_FALSE(ps_ui_text_view_load_file(999, bad.c_str()));
  CHECK(std::string(ps_ui_last_error()) == "invalid text view");
}

TEST_CASE("headless text views save their text and report write failures") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_text_view_set_text(view, "line one\nline two"));
  const std::string path = scratchFile("save.txt", "previous contents that are longer");
  CHECK(ps_ui_text_view_save_file(view, path.c_str()));
  CHECK(readScratchFile(path) == "line one\nline two");
  CHECK_FALSE(ps_ui_text_view_save_file(view, "/definitely/not/here/out.txt"));
  CHECK(std::string(ps_ui_last_error()).find("cannot write /definitely/not/here/out.txt") == 0);
  CHECK_FALSE(ps_ui_text_view_save_file(999, path.c_str()));
}

TEST_CASE("headless typing modifies a view and delivers a text changed event") {
  startApp();
  const uint64_t window = ps_ui_window_create("Typing", 100, 100);
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_window_set_content(window, view));
  REQUIRE(ps_ui_text_view_set_text(view, "abc"));
  headless::pushTypeText(view, "def");
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_TEXT_CHANGED);
  CHECK(ps_ui_event_window() == window);
  CHECK(ps_ui_event_widget() == view);
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "abcdef");
  CHECK(ps_ui_text_view_is_modified(view));
  CHECK(ps_ui_text_view_clear_modified(view));
  CHECK_FALSE(ps_ui_text_view_is_modified(view));
}

TEST_CASE("headless typing at widget zero targets the first text view") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  headless::pushTypeText(0, "hi");
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_TEXT_CHANGED);
  CHECK(ps_ui_event_widget() == view);
  CHECK(ps_ui_event_window() == 0);
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "hi");
}

TEST_CASE("headless events replay in order and end with a quit request") {
  startApp();
  const uint64_t window = ps_ui_window_create("Events", 100, 100);
  headless::pushCommand(7);
  headless::pushCloseWindow(window);
  headless::pushCommand(9);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_COMMAND);
  CHECK(ps_ui_event_command() == 7);
  CHECK(ps_ui_event_window() == 0);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_WINDOW_CLOSE_REQUESTED);
  CHECK(ps_ui_event_window() == window);
  CHECK(ps_ui_event_command() == 0);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_COMMAND);
  CHECK(ps_ui_event_command() == 9);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
}

TEST_CASE("headless actions aimed at missing targets are skipped") {
  startApp();
  const uint64_t window = ps_ui_window_create("Gone", 100, 100);
  REQUIRE(ps_ui_window_close(window));
  headless::pushCloseWindow(window);
  headless::pushTypeText(0, "nobody home");
  headless::pushCommand(3);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_COMMAND);
  CHECK(ps_ui_event_command() == 3);
}

TEST_CASE("headless quit ends the event stream") {
  startApp();
  headless::pushCommand(1);
  ps_ui_quit();
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
  headless::pushQuit();
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
}

TEST_CASE("headless menus list their items in the menu bar") {
  startApp();
  const uint64_t file = ps_ui_menu_create("File");
  const uint64_t edit = ps_ui_menu_create("Edit");
  REQUIRE(file != 0);
  CHECK(ps_ui_menu_add_item(file, "Save", "cmd+s", 3));
  CHECK(ps_ui_menu_add_item(file, "Close", "", 4));
  CHECK(ps_ui_menu_add_separator(file));
  CHECK(ps_ui_menu_add_standard(file, PS_UI_STANDARD_QUIT));
  CHECK(ps_ui_menu_add_standard(edit, PS_UI_STANDARD_UNDO));
  CHECK(headless::menuBarItems().empty());
  CHECK(ps_ui_menu_bar_add(file));
  CHECK(ps_ui_menu_bar_add(edit));
  CHECK_FALSE(ps_ui_menu_bar_add(file));
  const std::vector<std::string> expected{"File>Save (cmd+s) #3", "File>Close #4", "File>-", "File>standard 8",
                                          "Edit>standard 1"};
  CHECK(headless::menuBarItems() == expected);
}

TEST_CASE("headless menus reject bad handles and standard ids") {
  startApp();
  const uint64_t menu = ps_ui_menu_create("File");
  CHECK_FALSE(ps_ui_menu_add_item(999, "x", "", 1));
  CHECK_FALSE(ps_ui_menu_add_separator(0));
  CHECK_FALSE(ps_ui_menu_add_standard(menu, 0));
  CHECK_FALSE(ps_ui_menu_add_standard(menu, 99));
  CHECK_FALSE(ps_ui_menu_bar_add(999));
  CHECK(headless::menuBarItems().empty());
}

TEST_CASE("headless text views keep style runs in call order and validate their ranges") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  // "h\xC3\xA9llo \xE6\x97\xA5": bytes 0 h, 1-2 e-acute, 3 l, 4 l, 5 o, 6 space, 7-9 the CJK character.
  REQUIRE(ps_ui_text_view_set_text(view, "h\xC3\xA9llo \xE6\x97\xA5"));
  CHECK(ps_ui_text_view_add_style(view, 0, 6, 0xFF0000, PS_UI_STYLE_BOLD));
  CHECK(ps_ui_text_view_add_style(view, 7, 10, 0x00FF00, PS_UI_STYLE_BOLD | PS_UI_STYLE_ITALIC));
  CHECK(ps_ui_text_view_add_style(view, 3, 5, 0x0000FF, 0));
  const std::vector<headless::StyleRun> runs{{0, 6, 0xFF0000, 1}, {7, 10, 0x00FF00, 3}, {3, 5, 0x0000FF, 0}};
  CHECK(headless::styleRuns(view) == runs);
  // Rejected: empty, reversed, past the end, negative, and inside a code point.
  CHECK_FALSE(ps_ui_text_view_add_style(view, 3, 3, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, 5, 3, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, 0, 11, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, -1, 3, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, 2, 4, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, 0, 8, 1, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(999, 0, 1, 1, 0));
  CHECK(headless::styleRuns(view).size() == 3);
  // Styling is presentation only.
  CHECK_FALSE(ps_ui_text_view_is_modified(view));
  CHECK(ps_ui_text_view_clear_styles(view));
  CHECK(headless::styleRuns(view).empty());
  CHECK_FALSE(ps_ui_text_view_clear_styles(999));
}

TEST_CASE("headless style runs go away when the text is replaced but survive typing") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_text_view_set_text(view, "abc"));
  REQUIRE(ps_ui_text_view_add_style(view, 0, 3, 0x123456, 0));
  headless::pushTypeText(view, "d");
  REQUIRE(ps_ui_wait_event() == PS_UI_EVENT_TEXT_CHANGED);
  CHECK(headless::styleRuns(view).size() == 1);
  REQUIRE(ps_ui_text_view_set_text(view, "xyz"));
  CHECK(headless::styleRuns(view).empty());
}

TEST_CASE("headless dialogs answer from the script and then default") {
  startApp();
  headless::pushOpenPanelAnswer("/docs/a.txt");
  headless::pushSavePanelAnswer("/docs/b.txt");
  headless::pushAlertAnswer(2);
  CHECK(std::string(ps_ui_open_panel("Open")) == "/docs/a.txt");
  CHECK(ps_ui_panel_chosen());
  CHECK(std::string(ps_ui_open_panel("Open")).empty());
  CHECK_FALSE(ps_ui_panel_chosen());
  CHECK(std::string(ps_ui_save_panel("Save", "Untitled.txt")) == "/docs/b.txt");
  CHECK(ps_ui_panel_chosen());
  CHECK(std::string(ps_ui_save_panel("Save", "Untitled.txt")).empty());
  CHECK_FALSE(ps_ui_panel_chosen());
  CHECK(ps_ui_alert("Save changes?", "Unsaved text", "Save\nDiscard\nCancel") == 2);
  CHECK(ps_ui_alert("Save changes?", "Unsaved text", "Save\nDiscard\nCancel") == 0);
}

TEST_CASE("headless alerts clamp answers outside the offered buttons") {
  startApp();
  headless::pushAlertAnswer(5);
  headless::pushAlertAnswer(-3);
  CHECK(ps_ui_alert("m", "d", "OK") == 0);
  CHECK(ps_ui_alert("m", "d", "OK\nCancel") == 0);
}

TEST_CASE("headless backend records every call in order") {
  startApp();
  const uint64_t window = ps_ui_window_create("Say \"hi\"", 720, 480);
  const uint64_t view = ps_ui_text_view_create();
  ps_ui_window_set_content(window, view);
  ps_ui_text_view_set_text(view, "a\nb");
  ps_ui_text_view_get_text(view);
  ps_ui_window_show(window);
  ps_ui_window_set_edited(window, true);
  ps_ui_window_close(404);
  headless::pushCommand(5);
  ps_ui_wait_event();
  ps_ui_event_command();
  const std::vector<std::string> expected{
      "ps_ui_init(\"Test\") -> true",
      "ps_ui_window_create(\"Say \\\"hi\\\"\", 720, 480) -> 1",
      "ps_ui_text_view_create() -> 2",
      "ps_ui_window_set_content(1, 2) -> true",
      "ps_ui_text_view_set_text(2, \"a\\nb\") -> true",
      "ps_ui_text_view_get_text(2) -> \"a\\nb\"",
      "ps_ui_window_show(1) -> true",
      "ps_ui_window_set_edited(1, true) -> true",
      "ps_ui_window_close(404) -> false",
      "ps_ui_wait_event() -> command",
      "ps_ui_event_command() -> 5",
  };
  CHECK(headless::callLog() == expected);
}

TEST_CASE("headless reset forgets everything and restarts handles") {
  startApp();
  const uint64_t first = ps_ui_window_create("One", 100, 100);
  headless::pushCommand(1);
  headless::reset();
  CHECK(headless::callLog().empty());
  REQUIRE(ps_ui_init("Again"));
  CHECK(ps_ui_window_create("Two", 100, 100) == first);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
  CHECK(logContains("ps_ui_init(\"Again\") -> true"));
}
