#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"
#include "primec/ui/NativeUiShortcut.h"
#include "primec/ui/NativeUiStandardItems.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

// The one shortcut parser (include/primec/ui/NativeUiShortcut.h) that every backend maps from, and the headless
// backend's use of it (docs/NativeUiPlan.md section 4).
TEST_SUITE_BEGIN("primestruct.ui.shortcut");

namespace {

using primec::ui::ParsedShortcut;
using primec::ui::parseShortcut;

// The flags as text, so a failing case shows what was parsed: P (cmd), S (shift), A (alt), C (ctrl).
std::string flagsOf(const ParsedShortcut &parsed) {
  return std::string(parsed.primary ? "P" : "") + (parsed.shift ? "S" : "") +
         (parsed.alt ? "A" : "") + (parsed.control ? "C" : "");
}

struct Expected {
  const char *text;
  const char *flags;
  const char *key;
};

void checkAll(const std::vector<Expected> &cases) {
  for (const Expected &expected : cases) {
    const ParsedShortcut parsed = parseShortcut(expected.text);
    INFO("shortcut \"" << expected.text << "\"");
    CHECK(parsed.valid);
    CHECK(flagsOf(parsed) == expected.flags);
    CHECK(parsed.key == expected.key);
  }
}

} // namespace

// Every shortcut the stdlib and the examples write (grep '"cmd+' stdlib examples).
TEST_CASE("parseShortcut reads the shortcuts of the repo's apps and stdlib") {
  checkAll({
      {"cmd+s", "P", "s"},        {"cmd+shift+s", "PS", "s"}, {"cmd+w", "P", "w"},
      {"cmd+t", "P", "t"},        {"cmd+shift+x", "PS", "x"}, {"cmd+shift+m", "PS", "m"},
      {"cmd+shift+l", "PS", "l"}, {"cmd+r", "P", "r"},        {"cmd+o", "P", "o"},
      {"cmd+n", "P", "n"},        {"cmd+k", "P", "k"},        {"cmd+i", "P", "i"},
      {"cmd+e", "P", "e"},        {"cmd+b", "P", "b"},        {"cmd+alt+o", "PA", "o"},
      {"cmd+alt+1", "PA", "1"},   {"cmd+alt+2", "PA", "2"},   {"cmd+alt+3", "PA", "3"},
      {"cmd+1", "P", "1"},        {"cmd+5", "P", "5"},        {"cmd+9", "P", "9"},
      {"cmd+.", "P", "."},        {"cmd+,", "P", ","},
  });
}

TEST_CASE("parseShortcut reads the shortcuts of the standard menu items") {
  for (int32_t id = PS_UI_STANDARD_UNDO; id <= PS_UI_STANDARD_PREVIOUS_TAB; ++id) {
    const primec::ui::StandardItemInfo info = primec::ui::standardItemInfo(id);
    REQUIRE(info.shortcut != nullptr);
    INFO("standard item " << id << " shortcut \"" << info.shortcut << "\"");
    CHECK(parseShortcut(info.shortcut).valid);
  }
  checkAll({{"cmd+z", "P", "z"},
            {"cmd+shift+z", "PS", "z"},
            {"cmd+alt+h", "PA", "h"},
            {"cmd+ctrl+f", "PC", "f"},
            {"cmd+?", "P", "?"},
            {"cmd+shift+]", "PS", "]"},
            {"cmd+shift+[", "PS", "["}});
}

TEST_CASE("parseShortcut reads ctrl, alt, opt and shift as the modifiers they name") {
  checkAll({{"ctrl+c", "C", "c"},
            {"alt+x", "A", "x"},
            {"opt+x", "A", "x"},
            {"shift+a", "S", "a"},
            {"cmd+ctrl+alt+shift+q", "PSAC", "q"},
            {"shift+cmd+q", "PS", "q"},
            {"q", "", "q"}});
}

TEST_CASE("parseShortcut ignores case and keeps one spelling for a key") {
  checkAll({{"CMD+Shift+S", "PS", "s"}, {"Cmd+Return", "P", "return"}, {"OPT+F5", "A", "f5"}});
}

TEST_CASE("parseShortcut reads named keys") {
  checkAll({{"cmd+return", "P", "return"},
            {"cmd+enter", "P", "return"},
            {"tab", "", "tab"},
            {"shift+tab", "S", "tab"},
            {"escape", "", "escape"},
            {"cmd+esc", "P", "escape"},
            {"cmd+space", "P", "space"},
            {"delete", "", "delete"},
            {"cmd+backspace", "P", "backspace"},
            {"cmd+left", "P", "left"},
            {"cmd+right", "P", "right"},
            {"cmd+up", "P", "up"},
            {"cmd+down", "P", "down"},
            {"home", "", "home"},
            {"end", "", "end"},
            {"cmd+pageup", "P", "pageup"},
            {"cmd+pagedown", "P", "pagedown"},
            {"f1", "", "f1"},
            {"f5", "", "f5"},
            {"cmd+f12", "P", "f12"}});
}

TEST_CASE("parseShortcut: an empty text is no shortcut and is valid") {
  const ParsedShortcut empty = parseShortcut("");
  CHECK(empty.valid);
  CHECK(empty.key.empty());
  CHECK(flagsOf(empty).empty());
  const ParsedShortcut none = parseShortcut(nullptr);
  CHECK(none.valid);
  CHECK(none.key.empty());
}

TEST_CASE("parseShortcut refuses text that is not a shortcut") {
  for (const char *text : {"cmd",
                           "cmd+",
                           "+s",
                           "cmd++s",
                           "cmd+shift",
                           "ctrl+",
                           "hyper+s",
                           "cmd+hyper+s",
                           "cmd+foo",
                           "cmd+f0",
                           "cmd+f13",
                           "cmd+f05",
                           "cmd+ab",
                           "s+cmd",
                           "cmd+s+t",
                           "cmd+ s",
                           " ",
                           "cmd+\x01",
                           "cmd+\xC3\xA9"}) {
    INFO("shortcut \"" << text << "\"");
    const ParsedShortcut parsed = parseShortcut(text);
    CHECK_FALSE(parsed.valid);
    CHECK(parsed.key.empty());
    CHECK(flagsOf(parsed).empty());
  }
}

// The headless backend adds an item with a bad shortcut without one, as the AppKit backend does (a menu command never
// fails for its shortcut), and keeps the shortcut of a valid one as written.
TEST_CASE("the headless backend lists a bad shortcut as none and a valid one as written") {
  namespace headless = primec::ui::headless;
  headless::reset();
  REQUIRE(ps_ui_init("Shortcuts"));
  const uint64_t menu = ps_ui_menu_create("File");
  REQUIRE(menu != 0);
  CHECK(ps_ui_menu_add_item(menu, "Save", "cmd+s", 1));
  CHECK(ps_ui_menu_add_item(menu, "Save As", "cmd+shift+s", 2));
  CHECK(ps_ui_menu_add_item(menu, "Typo", "cmd+hyper+t", 3));
  CHECK(ps_ui_menu_add_item(menu, "No key", "cmd+", 4));
  CHECK(ps_ui_menu_add_item(menu, "None", "", 5));
  CHECK(ps_ui_menu_bar_add(menu));
  const std::vector<std::string> expected = {"File>Save (cmd+s) #1",
                                             "File>Save As (cmd+shift+s) #2",
                                             "File>Typo #3",
                                             "File>No key #4",
                                             "File>None #5"};
  CHECK(headless::menuBarItems() == expected);
  headless::reset();
}

TEST_SUITE_END();
