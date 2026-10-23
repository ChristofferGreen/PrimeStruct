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

TEST_CASE("headless tab groups hold windows in tab order and select the newest tab") {
  startApp();
  const uint64_t first = ps_ui_window_create("One", 100, 100);
  const uint64_t second = ps_ui_window_create("Two", 100, 100);
  const uint64_t third = ps_ui_window_create("Three", 100, 100);
  REQUIRE(ps_ui_window_show(first));
  CHECK(ps_ui_active_window() == first);
  CHECK(headless::tabGroup(first).empty());
  CHECK(ps_ui_window_add_tab(first, second));
  CHECK(ps_ui_active_window() == second);
  CHECK(ps_ui_window_add_tab(second, third));
  const std::vector<uint64_t> group{first, second, third};
  CHECK(headless::tabGroup(first) == group);
  CHECK(headless::tabGroup(third) == group);
  // Rejected: the same window, a window that already has a group, bad handles.
  CHECK_FALSE(ps_ui_window_add_tab(first, first));
  CHECK_FALSE(ps_ui_window_add_tab(first, third));
  CHECK_FALSE(ps_ui_window_add_tab(first, 999));
  CHECK_FALSE(ps_ui_window_add_tab(999, first));
}

TEST_CASE("headless windows enumerate in creation order and the active window follows closes") {
  startApp();
  CHECK(ps_ui_active_window() == 0);
  CHECK(ps_ui_window_count() == 0);
  const uint64_t a = ps_ui_window_create("A", 100, 100);
  const uint64_t b = ps_ui_window_create("B", 100, 100);
  const uint64_t c = ps_ui_window_create("C", 100, 100);
  REQUIRE(ps_ui_window_add_tab(a, b));
  REQUIRE(ps_ui_window_add_tab(b, c));
  CHECK(ps_ui_window_count() == 3);
  CHECK(ps_ui_window_at(0) == a);
  CHECK(ps_ui_window_at(2) == c);
  CHECK(ps_ui_window_at(3) == 0);
  CHECK(ps_ui_window_at(-1) == 0);
  CHECK(ps_ui_active_window() == c);
  REQUIRE(ps_ui_window_close(c));
  CHECK(ps_ui_window_count() == 2);
  CHECK(ps_ui_active_window() == b);
  CHECK(headless::tabGroup(a) == std::vector<uint64_t>{a, b});
  headless::pushActivateWindow(a);
  headless::pushQuit();
  REQUIRE(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
  CHECK(ps_ui_active_window() == a);
  REQUIRE(ps_ui_window_close(a));
  REQUIRE(ps_ui_window_close(b));
  CHECK(ps_ui_window_count() == 0);
  CHECK(ps_ui_active_window() == 0);
}

TEST_CASE("headless tabs can be counted, indexed and selected") {
  startApp();
  const uint64_t a = ps_ui_window_create("A", 100, 100);
  const uint64_t b = ps_ui_window_create("B", 100, 100);
  const uint64_t c = ps_ui_window_create("C", 100, 100);
  // A window in no group is its own single tab.
  CHECK(ps_ui_window_tab_count(a) == 1);
  CHECK(ps_ui_window_tab_at(a, 0) == a);
  CHECK(ps_ui_window_tab_at(a, 1) == 0);
  REQUIRE(ps_ui_window_add_tab(a, b));
  REQUIRE(ps_ui_window_add_tab(a, c));
  for (const uint64_t member : {a, b, c}) {
    CHECK(ps_ui_window_tab_count(member) == 3);
    CHECK(ps_ui_window_tab_at(member, 0) == a);
    CHECK(ps_ui_window_tab_at(member, 2) == c);
  }
  CHECK(ps_ui_window_tab_at(b, 3) == 0);
  CHECK(ps_ui_window_tab_at(b, -1) == 0);
  CHECK(ps_ui_active_window() == c);
  CHECK(ps_ui_window_select(a));
  CHECK(ps_ui_active_window() == a);
  REQUIRE(ps_ui_window_close(b));
  CHECK(ps_ui_window_tab_count(a) == 2);
  CHECK(ps_ui_window_tab_at(a, 1) == c);
  CHECK_FALSE(ps_ui_window_select(b));
  CHECK(ps_ui_window_tab_count(b) == 0);
  CHECK(ps_ui_window_tab_at(999, 0) == 0);
}

TEST_CASE("headless windows keep their own content and document path") {
  startApp();
  const uint64_t a = ps_ui_window_create("A", 100, 100);
  const uint64_t b = ps_ui_window_create("B", 100, 100);
  const uint64_t viewA = ps_ui_text_view_create();
  const uint64_t viewB = ps_ui_text_view_create();
  REQUIRE(ps_ui_window_set_content(a, viewA));
  REQUIRE(ps_ui_window_set_content(b, viewB));
  CHECK(ps_ui_window_content(a) == viewA);
  CHECK(ps_ui_window_content(b) == viewB);
  CHECK(ps_ui_window_content(999) == 0);
  CHECK_FALSE(ps_ui_window_has_path(a));
  CHECK(std::string(ps_ui_window_path(a)).empty());
  CHECK(ps_ui_window_set_path(a, "/docs/a.txt"));
  CHECK(ps_ui_window_set_path(b, "/docs/b.txt"));
  CHECK(ps_ui_window_has_path(a));
  CHECK(std::string(ps_ui_window_path(a)) == "/docs/a.txt");
  CHECK(std::string(ps_ui_window_path(b)) == "/docs/b.txt");
  CHECK(headless::windowPath(b) == "/docs/b.txt");
  CHECK(ps_ui_window_set_path(a, ""));
  CHECK_FALSE(ps_ui_window_has_path(a));
  CHECK_FALSE(ps_ui_window_set_path(999, "x"));
  CHECK_FALSE(ps_ui_window_has_path(999));
  REQUIRE(ps_ui_window_close(b));
  CHECK(ps_ui_window_content(b) == 0);
  CHECK(std::string(ps_ui_window_path(b)).empty());
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


TEST_CASE("a text view shows or hides its line number bar") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  CHECK_FALSE(headless::lineNumbersVisible(view));
  CHECK(ps_ui_text_view_set_line_numbers(view, true));
  CHECK(headless::lineNumbersVisible(view));
  CHECK_FALSE(headless::styleRuns(view).size() != 0);
  CHECK(ps_ui_text_view_is_modified(view) == false);  // presentation only
  CHECK(ps_ui_text_view_set_line_numbers(view, false));
  CHECK_FALSE(headless::lineNumbersVisible(view));
  CHECK_FALSE(ps_ui_text_view_set_line_numbers(999, true));
  CHECK(logContains("ps_ui_text_view_set_line_numbers(" + std::to_string(view) + ", true) -> true"));
  CHECK(logContains("ps_ui_text_view_set_line_numbers(999, true) -> false"));
}

TEST_CASE("menu items carry a check mark that follows ps_ui_menu_set_checked") {
  startApp();
  const uint64_t menu = ps_ui_menu_create("View");
  REQUIRE(ps_ui_menu_add_item(menu, "Show Line Numbers", "cmd+shift+l", 7));
  REQUIRE(ps_ui_menu_add_item(menu, "Other", "", 8));
  REQUIRE(ps_ui_menu_bar_add(menu));
  CHECK_FALSE(headless::menuItemChecked(7));
  CHECK(ps_ui_menu_set_checked(menu, 7, true));
  CHECK(headless::menuItemChecked(7));
  CHECK_FALSE(headless::menuItemChecked(8));
  const std::vector<std::string> checked{"View>Show Line Numbers (cmd+shift+l) #7 [checked]", "View>Other #8"};
  CHECK(headless::menuBarItems() == checked);
  CHECK(ps_ui_menu_set_checked(menu, 7, false));
  CHECK_FALSE(headless::menuItemChecked(7));
  CHECK_FALSE(ps_ui_menu_set_checked(menu, 99, true));  // no item with that id
  CHECK_FALSE(ps_ui_menu_set_checked(999, 7, true));    // no such menu
}

TEST_CASE("boolean preferences keep their value and answer the fallback when unset") {
  startApp();
  CHECK(ps_ui_pref_get_bool("lineNumbers", true));
  CHECK_FALSE(ps_ui_pref_get_bool("lineNumbers", false));
  CHECK(ps_ui_pref_set_bool("lineNumbers", true));
  CHECK(ps_ui_pref_get_bool("lineNumbers", false));
  CHECK(headless::boolPref("lineNumbers", false));
  CHECK(ps_ui_pref_set_bool("lineNumbers", false));
  CHECK_FALSE(ps_ui_pref_get_bool("lineNumbers", true));
  CHECK_FALSE(ps_ui_pref_set_bool("", true));
  CHECK_FALSE(ps_ui_pref_set_bool(nullptr, true));
  headless::setBoolPref("preset", true);
  CHECK(ps_ui_pref_get_bool("preset", false));
  CHECK(headless::pushedScenario() == "pref:preset=1");
}


TEST_CASE("a text view wraps long lines until told not to") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  CHECK(headless::lineWrapping(view));
  CHECK(ps_ui_text_view_set_wrapping(view, false));
  CHECK_FALSE(headless::lineWrapping(view));
  CHECK(ps_ui_text_view_set_wrapping(view, true));
  CHECK(headless::lineWrapping(view));
  CHECK_FALSE(ps_ui_text_view_set_wrapping(999, false));
}

TEST_CASE("a submenu hangs off its parent and can be rebuilt") {
  startApp();
  const uint64_t file = ps_ui_menu_create("File");
  const uint64_t recent = ps_ui_menu_create("Open Recent");
  REQUIRE(ps_ui_menu_add_item(file, "Open...", "cmd+o", 2));
  CHECK(ps_ui_menu_add_submenu(file, "Open Recent", recent));
  CHECK_FALSE(ps_ui_menu_add_submenu(file, "Again", recent));      // already attached
  CHECK_FALSE(ps_ui_menu_add_submenu(file, "Itself", file));       // not into itself
  CHECK_FALSE(ps_ui_menu_add_submenu(file, "Bad", 999));
  REQUIRE(ps_ui_menu_add_item(recent, "a.txt", "", 200));
  REQUIRE(ps_ui_menu_bar_add(file));
  const std::vector<std::string> filled{"File>Open... (cmd+o)", "File>Open Recent>", "File>Open Recent>a.txt"};
  CHECK(headless::menuBarOutline() == filled);
  CHECK(ps_ui_menu_set_checked(recent, 200, true));
  CHECK(headless::menuItemChecked(200));
  CHECK(ps_ui_menu_clear(recent));
  const std::vector<std::string> emptied{"File>Open... (cmd+o)", "File>Open Recent>"};
  CHECK(headless::menuBarOutline() == emptied);
  CHECK_FALSE(ps_ui_menu_clear(999));
}

TEST_CASE("recent files keep the latest first, drop repeats and stop at ten") {
  startApp();
  CHECK(ps_ui_recent_add("/a/one.txt"));
  CHECK(ps_ui_recent_add("/a/two.txt"));
  CHECK(ps_ui_recent_add("/a/one.txt"));
  CHECK(std::string(ps_ui_recent_path(0)) == "/a/one.txt");
  CHECK(std::string(ps_ui_recent_path(1)) == "/a/two.txt");
  CHECK(std::string(ps_ui_recent_path(2)).empty());
  CHECK(std::string(ps_ui_recent_path(-1)).empty());
  CHECK_FALSE(ps_ui_recent_add(""));
  CHECK_FALSE(ps_ui_recent_add(nullptr));
  for (int i = 0; i < 12; ++i) {
    CHECK(ps_ui_recent_add(("/b/file" + std::to_string(i) + ".txt").c_str()));
  }
  CHECK(headless::recentFiles().size() == 10);
  CHECK(std::string(ps_ui_recent_path(0)) == "/b/file11.txt");
  CHECK(std::string(ps_ui_recent_path(9)) == "/b/file2.txt");
  CHECK(ps_ui_recent_clear());
  CHECK(headless::recentFiles().empty());
}

TEST_CASE("recent file items are titled with the file name and numbered from the first command id") {
  startApp();
  const uint64_t menu = ps_ui_menu_create("Open Recent");
  REQUIRE(ps_ui_menu_bar_add(menu));
  ps_ui_recent_add("/x/notes.txt");
  ps_ui_recent_add("/y/todo.md");
  ps_ui_recent_add("/z/other.txt");
  CHECK(ps_ui_menu_add_recent_files(menu, 200, 2) == 2);
  const std::vector<std::string> two{"Open Recent>other.txt #200", "Open Recent>todo.md #201"};
  CHECK(headless::menuBarItems() == two);
  CHECK(ps_ui_menu_clear(menu));
  CHECK(ps_ui_menu_add_recent_files(menu, 300, 10) == 3);
  CHECK(ps_ui_menu_add_recent_files(menu, 300, 0) == 0);
  CHECK(ps_ui_menu_add_recent_files(999, 300, 10) == 0);

  // Two files with one name get their folder added.
  ps_ui_recent_clear();
  ps_ui_recent_add("/x/a.txt");
  ps_ui_recent_add("/y/a.txt");
  CHECK(ps_ui_menu_clear(menu));
  CHECK(ps_ui_menu_add_recent_files(menu, 1, 10) == 2);
  const std::vector<std::string> clash{"Open Recent>a.txt \xE2\x80\x94 y #1", "Open Recent>a.txt \xE2\x80\x94 x #2"};
  CHECK(headless::menuBarItems() == clash);
}


TEST_CASE("a window remembers its frame under an autosave name") {
  startApp();
  const uint64_t window = ps_ui_window_create("W", 100, 100);
  CHECK(headless::windowState(window).autosaveName.empty());
  CHECK(ps_ui_window_set_autosave_name(window, "Main"));
  CHECK(headless::windowState(window).autosaveName == "Main");
  CHECK_FALSE(ps_ui_window_set_autosave_name(window, ""));
  CHECK_FALSE(ps_ui_window_set_autosave_name(window, nullptr));
  CHECK_FALSE(ps_ui_window_set_autosave_name(999, "Other"));
}

TEST_CASE("a window shows a markdown preview of its text view") {
  startApp();
  const uint64_t window = ps_ui_window_create("W", 100, 100);
  CHECK_FALSE(ps_ui_window_has_preview(window));
  CHECK_FALSE(ps_ui_window_set_preview(window, true)); // no text view yet
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_window_set_content(window, view));
  CHECK(ps_ui_text_view_set_text(view, "# Title\n\nbody"));
  CHECK_FALSE(ps_ui_window_refresh_preview(window)); // the preview is off
  CHECK(headless::previewPage(window).empty());

  CHECK(ps_ui_window_set_preview(window, true));
  CHECK(ps_ui_window_has_preview(window));
  CHECK(headless::windowState(window).preview);
  CHECK(headless::previewPage(window).find("<h1>Title</h1>") != std::string::npos);
  CHECK(headless::previewPage(window).find("<!DOCTYPE html>") == 0);

  // The page follows the text when it is refreshed, not before.
  CHECK(ps_ui_text_view_set_text(view, "## Next"));
  CHECK(headless::previewPage(window).find("<h2>") == std::string::npos);
  CHECK(ps_ui_window_refresh_preview(window));
  CHECK(headless::previewPage(window).find("<h2>Next</h2>") != std::string::npos);
  CHECK(headless::windowState(window).previewRenders == 2);

  // Turning it on again changes nothing; turning it off drops the page.
  CHECK(ps_ui_window_set_preview(window, true));
  CHECK(headless::windowState(window).previewRenders == 2);
  CHECK(ps_ui_window_set_preview(window, false));
  CHECK_FALSE(ps_ui_window_has_preview(window));
  CHECK(headless::previewPage(window).empty());
  CHECK_FALSE(ps_ui_window_refresh_preview(window));
  CHECK(ps_ui_window_set_preview(window, false)); // already off

  CHECK_FALSE(ps_ui_window_has_preview(999));
  CHECK_FALSE(ps_ui_window_set_preview(999, true));
  CHECK_FALSE(ps_ui_window_refresh_preview(999));
}

TEST_CASE("a menu item's check mark can follow the active window's preview") {
  startApp();
  const uint64_t menu = ps_ui_menu_create("View");
  REQUIRE(ps_ui_menu_add_item(menu, "Preview", "", 10));
  REQUIRE(ps_ui_menu_bar_add(menu));
  CHECK_FALSE(ps_ui_menu_follow_preview(menu, 11));
  CHECK_FALSE(ps_ui_menu_follow_preview(999, 10));
  CHECK(ps_ui_menu_follow_preview(menu, 10));
  const auto windowWithText = [] {
    const uint64_t window = ps_ui_window_create("W", 100, 100);
    REQUIRE(ps_ui_window_set_content(window, ps_ui_text_view_create()));
    return window;
  };
  const uint64_t first = windowWithText();
  const uint64_t second = windowWithText();
  CHECK(ps_ui_window_select(first));
  CHECK_FALSE(headless::menuItemChecked(10));
  CHECK(ps_ui_window_set_preview(first, true));
  CHECK(headless::menuItemChecked(10));
  CHECK(ps_ui_window_select(second));
  CHECK_FALSE(headless::menuItemChecked(10));
  CHECK(ps_ui_window_select(first));
  CHECK(headless::menuItemChecked(10));
  const auto items = headless::menuBarItems();
  CHECK(items == std::vector<std::string>{"View>Preview #10 [checked]"});
  CHECK(ps_ui_window_set_preview(first, false));
  CHECK_FALSE(headless::menuItemChecked(10));
}

TEST_CASE("controls need a started app and bad handles fail") {
  headless::reset();
  CHECK(ps_ui_label_create("x") == 0);
  CHECK(ps_ui_checkbox_create("x", true) == 0);
  CHECK(ps_ui_popup_create() == 0);
  CHECK(ps_ui_form_create() == 0);
  CHECK(ps_ui_tabs_create() == 0);
  CHECK_FALSE(ps_ui_preview_set_scroll_lock(false));
  REQUIRE(ps_ui_init("Test"));
  CHECK_FALSE(ps_ui_label_set_text(999, "x"));
  CHECK_FALSE(ps_ui_checkbox_checked(999));
  CHECK(ps_ui_popup_selected(999) == -1);
  CHECK_FALSE(ps_ui_form_add_heading(999, "x"));
  CHECK_FALSE(ps_ui_tabs_select(999, 0));
}

TEST_CASE("labels, check boxes and pop-ups hold their state") {
  startApp();
  const uint64_t label = ps_ui_label_create("Font size");
  const uint64_t box = ps_ui_checkbox_create("Wrap long lines", true);
  const uint64_t popup = ps_ui_popup_create();
  REQUIRE(label != 0);
  REQUIRE(box != 0);
  REQUIRE(popup != 0);
  CHECK(ps_ui_label_set_text(label, "Size"));
  CHECK(headless::controlOutline(label) == std::vector<std::string>{"label \"Size\""});
  CHECK(ps_ui_checkbox_checked(box));
  CHECK(ps_ui_checkbox_set_checked(box, false));
  CHECK_FALSE(ps_ui_checkbox_checked(box));
  CHECK_FALSE(headless::checkBoxChecked(box));

  // A pop-up has no chosen item until it has an item; the first one added is chosen.
  CHECK(ps_ui_popup_selected(popup) == -1);
  CHECK(ps_ui_popup_add_item(popup, "12"));
  CHECK(ps_ui_popup_add_item(popup, "14"));
  CHECK(ps_ui_popup_selected(popup) == 0);
  CHECK(ps_ui_popup_select(popup, 1));
  CHECK(ps_ui_popup_selected(popup) == 1);
  CHECK_FALSE(ps_ui_popup_select(popup, 2));
  CHECK_FALSE(ps_ui_popup_select(popup, -1));
  CHECK(ps_ui_popup_selected(popup) == 1);
  CHECK(headless::controlOutline(popup) == std::vector<std::string>{"popup [12|14] selected=1"});

  // The wrong kind of handle and a missing text fail.
  CHECK_FALSE(ps_ui_checkbox_set_checked(label, true));
  CHECK_FALSE(ps_ui_label_set_text(box, "x"));
  CHECK_FALSE(ps_ui_popup_add_item(box, "x"));
  CHECK_FALSE(ps_ui_popup_add_item(popup, nullptr));
  CHECK(ps_ui_popup_selected(box) == -1);
}

TEST_CASE("forms and tabs hold controls and a window shows them") {
  startApp();
  const uint64_t form = ps_ui_form_create();
  const uint64_t size = ps_ui_popup_create();
  ps_ui_popup_add_item(size, "12");
  ps_ui_popup_add_item(size, "14");
  const uint64_t wrap = ps_ui_checkbox_create("Wrap", true);
  const uint64_t hint = ps_ui_label_create("Applies to every tab");
  CHECK(ps_ui_form_add_heading(form, "Editor"));
  CHECK(ps_ui_form_add_row(form, "Font size", size));
  CHECK(ps_ui_form_add_control(form, wrap));
  CHECK(ps_ui_form_add_control(form, hint));
  const std::vector<std::string> formLines{"form",
                                           "  heading \"Editor\"",
                                           "  row \"Font size\": popup [12|14] selected=0",
                                           "  control: checkbox \"Wrap\" [checked]",
                                           "  control: label \"Applies to every tab\""};
  CHECK(headless::controlOutline(form) == formLines);

  // A control has one holder, a form cannot hold itself, and only a form takes rows.
  const uint64_t other = ps_ui_form_create();
  CHECK_FALSE(ps_ui_form_add_control(other, wrap));
  CHECK_FALSE(ps_ui_form_add_row(other, "Again", size));
  CHECK_FALSE(ps_ui_form_add_control(form, form));
  CHECK_FALSE(ps_ui_form_add_control(size, ps_ui_label_create("x")));
  CHECK_FALSE(ps_ui_form_add_row(form, nullptr, ps_ui_label_create("y")));
  CHECK(headless::controlOutline(form) == formLines);

  const uint64_t tabs = ps_ui_tabs_create();
  CHECK(ps_ui_tabs_add(tabs, "Editor", form));
  CHECK(ps_ui_tabs_add(tabs, "Markdown", other));
  CHECK_FALSE(ps_ui_tabs_add(tabs, "Again", form));
  CHECK_FALSE(ps_ui_tabs_add(tabs, "Itself", tabs));
  CHECK_FALSE(ps_ui_tabs_add(form, "Not tabs", ps_ui_form_create()));
  std::vector<std::string> outline = headless::controlOutline(tabs);
  CHECK(outline.front() == "tabs selected=0");
  CHECK(outline[1] == "  page \"Editor\":");
  CHECK(outline[2] == "    form");
  CHECK(outline[outline.size() - 2] == "  page \"Markdown\":");
  CHECK(outline.back() == "    form");
  CHECK(ps_ui_tabs_select(tabs, 1));
  CHECK_FALSE(ps_ui_tabs_select(tabs, 2));
  CHECK_FALSE(ps_ui_tabs_select(tabs, -1));
  CHECK(headless::controlOutline(tabs).front() == "tabs selected=1");

  // A window takes the tabs as its content, once, and not a control that something holds.
  const uint64_t window = ps_ui_window_create("Settings", 400, 300);
  const uint64_t second = ps_ui_window_create("Other", 400, 300);
  CHECK_FALSE(ps_ui_window_set_content(window, form)); // the tabs hold it
  CHECK(ps_ui_window_set_content(window, tabs));
  CHECK(ps_ui_window_content(window) == tabs);
  CHECK(headless::windowState(window).content == tabs);
  CHECK_FALSE(ps_ui_window_set_content(second, tabs));
  CHECK(ps_ui_window_set_content(window, tabs)); // the same content again
  CHECK_FALSE(ps_ui_form_add_control(ps_ui_form_create(), tabs));
}

TEST_CASE("the user changes controls and the program is told which") {
  startApp();
  const uint64_t window = ps_ui_window_create("Settings", 400, 300);
  const uint64_t form = ps_ui_form_create();
  const uint64_t box = ps_ui_checkbox_create("Wrap", false);
  const uint64_t popup = ps_ui_popup_create();
  ps_ui_popup_add_item(popup, "12");
  ps_ui_popup_add_item(popup, "14");
  ps_ui_popup_add_item(popup, "16");
  REQUIRE(ps_ui_form_add_control(form, box));
  REQUIRE(ps_ui_form_add_row(form, "Size", popup));
  REQUIRE(ps_ui_window_set_content(window, form));
  headless::pushToggleCheckBox(box);
  headless::pushSelectPopupItem(popup, 2);
  headless::pushSelectPopupItem(popup, 9); // no such item: skipped
  headless::pushToggleCheckBox(popup);     // not a check box: skipped
  headless::pushToggleCheckBox(box);
  const std::string b = std::to_string(box);
  const std::string p = std::to_string(popup);
  CHECK(headless::pushedScenario() == "check:" + b + ";popup:" + p + ":2;popup:" + p + ":9;check:" + p + ";check:" + b);

  CHECK(ps_ui_wait_event() == PS_UI_EVENT_CONTROL_CHANGED);
  CHECK(ps_ui_event_widget() == box);
  CHECK(ps_ui_event_window() == window);
  CHECK(ps_ui_checkbox_checked(box));
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_CONTROL_CHANGED);
  CHECK(ps_ui_event_widget() == popup);
  CHECK(ps_ui_event_window() == window);
  CHECK(ps_ui_popup_selected(popup) == 2);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_CONTROL_CHANGED);
  CHECK(ps_ui_event_widget() == box);
  CHECK_FALSE(ps_ui_checkbox_checked(box));
  CHECK(ps_ui_popup_selected(popup) == 2);
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
  CHECK(logContains("ps_ui_wait_event() -> control_changed"));

  // The program's own changes make no event.
  CHECK(ps_ui_checkbox_set_checked(box, true));
  CHECK(ps_ui_popup_select(popup, 0));
  CHECK(ps_ui_wait_event() == PS_UI_EVENT_QUIT_REQUESTED);
}

TEST_CASE("a settings window is never a tab") {
  startApp();
  const uint64_t host = ps_ui_window_create("Document", 100, 100);
  const uint64_t settings = ps_ui_window_create("Settings", 100, 100);
  CHECK(headless::windowState(settings).style == PS_UI_WINDOW_DOCUMENT);
  CHECK(ps_ui_window_set_style(settings, PS_UI_WINDOW_SETTINGS));
  CHECK(headless::windowState(settings).style == PS_UI_WINDOW_SETTINGS);
  CHECK_FALSE(ps_ui_window_add_tab(host, settings));
  CHECK_FALSE(ps_ui_window_add_tab(settings, host));
  CHECK(ps_ui_window_tab_count(settings) == 1);
  CHECK_FALSE(ps_ui_window_set_style(settings, 5));
  CHECK_FALSE(ps_ui_window_set_style(999, PS_UI_WINDOW_SETTINGS));

  // A window already in a tab group stays a document.
  const uint64_t tab = ps_ui_window_create("Tab", 100, 100);
  CHECK(ps_ui_window_add_tab(host, tab));
  CHECK_FALSE(ps_ui_window_set_style(tab, PS_UI_WINDOW_SETTINGS));
  CHECK(ps_ui_window_set_style(settings, PS_UI_WINDOW_DOCUMENT));
  CHECK(ps_ui_window_add_tab(host, settings));
}

TEST_CASE("a text view has a font size and the preview's scroll lock can be switched") {
  startApp();
  const uint64_t view = ps_ui_text_view_create();
  CHECK(headless::fontSize(view) == 0);
  CHECK(ps_ui_text_view_set_font_size(view, 13));
  CHECK(headless::fontSize(view) == 13);
  CHECK(ps_ui_text_view_set_font_size(view, 6));
  CHECK(ps_ui_text_view_set_font_size(view, 96));
  CHECK_FALSE(ps_ui_text_view_set_font_size(view, 5));
  CHECK_FALSE(ps_ui_text_view_set_font_size(view, 97));
  CHECK_FALSE(ps_ui_text_view_set_font_size(999, 12));
  CHECK(headless::fontSize(view) == 96);

  CHECK(headless::previewScrollLock());
  CHECK(ps_ui_preview_set_scroll_lock(false));
  CHECK_FALSE(headless::previewScrollLock());
  CHECK(ps_ui_preview_set_scroll_lock(true));
  CHECK(headless::previewScrollLock());
}

TEST_CASE("integer preferences keep their value and answer the fallback when unset") {
  startApp();
  CHECK(ps_ui_pref_get_int("session.active", 7) == 7);
  CHECK(ps_ui_pref_set_int("session.active", 3));
  CHECK(ps_ui_pref_get_int("session.active", 7) == 3);
  CHECK(headless::intPref("session.active", 0) == 3);
  CHECK(ps_ui_pref_set_int("session.active", -2));
  CHECK(ps_ui_pref_get_int("session.active", 7) == -2);
  CHECK_FALSE(ps_ui_pref_set_int("", 1));
  CHECK(ps_ui_pref_get_int(nullptr, 5) == 5);
  headless::setIntPref("preset", 12);
  CHECK(ps_ui_pref_get_int("preset", 0) == 12);
  CHECK(headless::pushedScenario() == "pint:preset=12");
}

TEST_CASE("list preferences keep their items in order") {
  startApp();
  CHECK(ps_ui_pref_list_count("files") == 0);
  CHECK(ps_ui_pref_list_add("files", "/a/one.txt"));
  CHECK(ps_ui_pref_list_add("files", "/a/two.txt"));
  CHECK(ps_ui_pref_list_count("files") == 2);
  CHECK(std::string(ps_ui_pref_list_get("files", 0)) == "/a/one.txt");
  CHECK(std::string(ps_ui_pref_list_get("files", 1)) == "/a/two.txt");
  CHECK(std::string(ps_ui_pref_list_get("files", 2)).empty());
  CHECK(std::string(ps_ui_pref_list_get("files", -1)).empty());
  CHECK(std::string(ps_ui_pref_list_get("other", 0)).empty());
  CHECK(ps_ui_pref_list_clear("files"));
  CHECK(ps_ui_pref_list_count("files") == 0);
  CHECK_FALSE(ps_ui_pref_list_add("", "x"));
  CHECK_FALSE(ps_ui_pref_list_add("files", nullptr));
  CHECK_FALSE(ps_ui_pref_list_clear(nullptr));
  headless::setPrefList("plain", {"x", "y,z"});
  CHECK(headless::prefList("plain") == std::vector<std::string>{"x", "y,z"});
  CHECK(headless::pushedScenario() == "plist:plain=x,y%2Cz");
}
