#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <thread>

// The contract of the native UI C ABI (include/primec/ui/NativeUi.h) for every backend, written against the C calls
// only (docs/NativeUiPlan.md section 9, Backend contract). The scenarios need no event loop and open no visible
// window. The only backend-specific code of this file is `ContractBackend`; a backend gets the same suite by giving
// that class its own setUp, tearDown and callFromSecondThread.
TEST_SUITE_BEGIN("primestruct.ui.contract");

namespace {

namespace headless = primec::ui::headless;

// What a backend supplies to run the contract: a fresh app that has not started, and the reaction to a call from a
// thread other than the one that started it.
class ContractBackend {
public:
  // A backend with no state of an earlier scenario: ps_ui_init has not been called, the recent files list is in
  // memory (PRIMESTRUCT_UI_RECENT, the way the platform backends keep it in tests) and empty.
  void setUp() {
    headless::reset();
    ::setenv("PRIMESTRUCT_UI_RECENT", "", 1);
  }

  // Ends the app the scenario started and forgets what it made.
  void tearDown() {
    ps_ui_quit();
    headless::reset();
    ::unsetenv("PRIMESTRUCT_UI_RECENT");
  }

  // Runs `call` on a second thread and returns the fault the backend reported for it ("ui call off the main thread:
  // <function>"), or "" when it reported none. The headless backend reports to a hook; a backend that ends the process
  // (exit code 3) runs the call in a child process and reads the message from its stderr.
  std::string callFromSecondThread(const std::function<void()> &call) {
    std::string message;
    headless::setFaultHook([&message](const std::string &fault) { message = fault; });
    std::thread other(call);
    other.join();
    headless::setFaultHook({});
    return message;
  }
};

// A started app for the length of a scenario.
struct ContractApp {
  ContractBackend backend;
  ContractApp() {
    backend.setUp();
    REQUIRE(ps_ui_init("Contract"));
  }
  ~ContractApp() {
    backend.tearDown();
  }
};

std::string scratchFile(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("native_ui_contract");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readAll(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// UTF-8 of every length: one byte, two (e acute), four (a smiling face) and three (two CJK characters).
const char *Utf8Text = "a\xC3\xA9\xF0\x9F\x98\x80z \xE6\x97\xA5\xE6\x9C\xAC";

} // namespace

// ps_ui_abi_version, ps_ui_init, ps_ui_platform
TEST_CASE("contract: the ABI is version 0 and ps_ui_init starts the app once") {
  ContractBackend backend;
  backend.setUp();
  CHECK(ps_ui_abi_version() == 0);
  CHECK(PS_UI_ABI_VERSION == 0);
  // Before ps_ui_init a call fails like a bad handle.
  CHECK(ps_ui_window_create("Early", 100, 100) == 0);
  CHECK(ps_ui_text_view_create() == 0);
  CHECK(ps_ui_init("Contract"));
  CHECK_FALSE(ps_ui_init("Contract again"));
  const int32_t platform = ps_ui_platform();
  CHECK(platform >= PS_UI_PLATFORM_MACOS);
  CHECK(platform <= PS_UI_PLATFORM_HEADLESS);
  backend.tearDown();
}

// ps_ui_supports, ps_ui_widget_watch, ps_ui_watch_first_frame
TEST_CASE(
    "contract: ps_ui_supports answers for each feature and the feature's calls agree with it") {
  ContractApp app;
  const bool inputEvents = ps_ui_supports(PS_UI_FEATURE_INPUT_EVENTS);
  const bool firstFrame = ps_ui_supports(PS_UI_FEATURE_FIRST_FRAME);
  CHECK_FALSE(ps_ui_supports(0));
  CHECK_FALSE(ps_ui_supports(3));
  CHECK_FALSE(ps_ui_supports(-1));
  CHECK_FALSE(ps_ui_supports(1000));
  // A backend without a feature fails the calls of that feature (docs/NativeUiPlan.md section 4).
  const uint64_t button = ps_ui_button_create("Go");
  REQUIRE(button != 0);
  CHECK(ps_ui_widget_watch(button, PS_UI_WATCH_HOVER) == inputEvents);
  CHECK_FALSE(ps_ui_widget_watch(button, 1 << 10));
  CHECK_FALSE(ps_ui_widget_watch(0, PS_UI_WATCH_HOVER));
  CHECK(ps_ui_watch_first_frame() == firstFrame);
}

// ps_ui_window_create, ps_ui_text_view_create, ps_ui_menu_create, ps_ui_checkbox_create, ps_ui_popup_create,
// ps_ui_label_create, ps_ui_window_set_content, ps_ui_form_create, ps_ui_form_add_row
TEST_CASE("contract: creating a widget gives a non-zero handle and a bad parent gives 0 or false") {
  ContractApp app;
  const uint64_t window = ps_ui_window_create("Window", 320, 200);
  const uint64_t view = ps_ui_text_view_create();
  const uint64_t menu = ps_ui_menu_create("File");
  const uint64_t checkbox = ps_ui_checkbox_create("Wrap", true);
  const uint64_t popup = ps_ui_popup_create();
  const uint64_t label = ps_ui_label_create("Name");
  const uint64_t form = ps_ui_form_create();
  for (const uint64_t handle : {window, view, menu, checkbox, popup, label, form}) {
    CHECK(handle != 0);
  }
  CHECK(window != view);
  CHECK(view != menu);
  CHECK(checkbox != popup);
  // Sizes that are not positive make no window.
  CHECK(ps_ui_window_create("Flat", 0, 100) == 0);
  CHECK(ps_ui_window_create("Thin", 100, -5) == 0);
  // A bad handle in either place fails and changes nothing.
  CHECK_FALSE(ps_ui_window_set_content(0, view));
  CHECK_FALSE(ps_ui_window_set_content(window, 0));
  CHECK_FALSE(ps_ui_window_set_content(window, menu));
  CHECK(ps_ui_window_set_content(window, view));
  CHECK(ps_ui_window_content(window) == view);
  CHECK_FALSE(ps_ui_form_add_row(0, "Name", label));
  CHECK_FALSE(ps_ui_form_add_row(form, "Name", 0));
  CHECK(ps_ui_form_add_row(form, "Name", label));
  // A control that a form holds does not go to another.
  const uint64_t other = ps_ui_form_create();
  CHECK_FALSE(ps_ui_form_add_row(other, "Name", label));
  // The state of the controls reads back.
  CHECK(ps_ui_checkbox_checked(checkbox));
  CHECK(ps_ui_checkbox_set_checked(checkbox, false));
  CHECK_FALSE(ps_ui_checkbox_checked(checkbox));
  CHECK(ps_ui_popup_selected(popup) == -1);
  CHECK(ps_ui_popup_add_item(popup, "One"));
  CHECK(ps_ui_popup_add_item(popup, "Two"));
  CHECK(ps_ui_popup_selected(popup) == 0);
  CHECK(ps_ui_popup_select(popup, 1));
  CHECK(ps_ui_popup_selected(popup) == 1);
  CHECK_FALSE(ps_ui_popup_select(popup, 2));
  CHECK_FALSE(ps_ui_checkbox_checked(0));
  CHECK(ps_ui_popup_selected(0) == -1);
}

// ps_ui_text_view_set_text, ps_ui_text_view_get_text, ps_ui_text_view_is_modified
TEST_CASE("contract: set_text and get_text round trip UTF-8 and the empty string") {
  ContractApp app;
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(view != 0);
  CHECK(std::string(ps_ui_text_view_get_text(view)).empty());
  CHECK(ps_ui_text_view_set_text(view, Utf8Text));
  CHECK(std::string(ps_ui_text_view_get_text(view)) == Utf8Text);
  CHECK(ps_ui_text_view_set_text(view, "line one\nline two\n"));
  CHECK(std::string(ps_ui_text_view_get_text(view)) == "line one\nline two\n");
  CHECK(ps_ui_text_view_set_text(view, ""));
  CHECK(std::string(ps_ui_text_view_get_text(view)).empty());
  // A program's own change is not the user's.
  CHECK_FALSE(ps_ui_text_view_is_modified(view));
  // A bad handle fails and reads as "".
  CHECK_FALSE(ps_ui_text_view_set_text(0, "x"));
  CHECK_FALSE(ps_ui_text_view_set_text(view + 1000, "x"));
  CHECK(std::string(ps_ui_text_view_get_text(0)).empty());
}

// ps_ui_text_view_add_style, ps_ui_text_view_clear_styles
TEST_CASE("contract: add_style takes byte ranges on code point boundaries only") {
  ContractApp app;
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(view != 0);
  // "a" 1 byte, "e acute" bytes 1-2, "smiling face" bytes 3-6, "z" byte 7.
  REQUIRE(ps_ui_text_view_set_text(view, "a\xC3\xA9\xF0\x9F\x98\x80z"));
  CHECK(ps_ui_text_view_add_style(view, 0, 1, 0xFF0000, PS_UI_STYLE_BOLD));
  CHECK(ps_ui_text_view_add_style(view, 1, 3, 0x00FF00, PS_UI_STYLE_ITALIC));
  CHECK(ps_ui_text_view_add_style(view, 3, 7, 0x0000FF, PS_UI_STYLE_BOLD | PS_UI_STYLE_ITALIC));
  CHECK(ps_ui_text_view_add_style(view, 0, 8, 0x808080, 0));
  CHECK_FALSE(ps_ui_text_view_add_style(view, 2, 3, 0, 0));  // inside e acute
  CHECK_FALSE(ps_ui_text_view_add_style(view, 1, 2, 0, 0));  // ends inside e acute
  CHECK_FALSE(ps_ui_text_view_add_style(view, 4, 7, 0, 0));  // starts inside the smiling face
  CHECK_FALSE(ps_ui_text_view_add_style(view, 3, 5, 0, 0));  // ends inside the smiling face
  CHECK_FALSE(ps_ui_text_view_add_style(view, 3, 3, 0, 0));  // empty
  CHECK_FALSE(ps_ui_text_view_add_style(view, 4, 3, 0, 0));  // backwards
  CHECK_FALSE(ps_ui_text_view_add_style(view, -1, 2, 0, 0)); // before the text
  CHECK_FALSE(ps_ui_text_view_add_style(view, 0, 9, 0, 0));  // past the end
  CHECK_FALSE(ps_ui_text_view_add_style(0, 0, 1, 0, 0));     // a bad handle
  CHECK(ps_ui_text_view_clear_styles(view));
  CHECK_FALSE(ps_ui_text_view_clear_styles(0));
}

// ps_ui_text_view_save_file, ps_ui_text_view_load_file, ps_ui_last_error
TEST_CASE(
    "contract: save_file and load_file round trip a file and last_error says why one failed") {
  ContractApp app;
  const uint64_t writer = ps_ui_text_view_create();
  const uint64_t reader = ps_ui_text_view_create();
  REQUIRE(writer != 0);
  REQUIRE(reader != 0);
  CHECK(std::string(ps_ui_last_error()).empty()); // nothing has failed yet
  const std::string path = scratchFile("round_trip.txt");
  REQUIRE(ps_ui_text_view_set_text(writer, Utf8Text));
  CHECK(ps_ui_text_view_save_file(writer, path.c_str()));
  CHECK(readAll(path) == Utf8Text);
  CHECK(ps_ui_text_view_load_file(reader, path.c_str()));
  CHECK(std::string(ps_ui_text_view_get_text(reader)) == Utf8Text);
  CHECK_FALSE(ps_ui_text_view_is_modified(reader));

  // A missing file fails, leaves the view as it was and is explained.
  const std::string missing = scratchFile("no_such_dir/missing.txt");
  CHECK_FALSE(ps_ui_text_view_load_file(reader, missing.c_str()));
  CHECK_FALSE(std::string(ps_ui_last_error()).empty());
  CHECK(std::string(ps_ui_text_view_get_text(reader)) == Utf8Text);
  // So does a write into a folder that does not exist.
  CHECK_FALSE(ps_ui_text_view_save_file(writer, missing.c_str()));
  CHECK_FALSE(std::string(ps_ui_last_error()).empty());

  // A file that is not UTF-8 is refused and the view keeps its text.
  const std::string binary = scratchFile("not_utf8.bin");
  {
    std::ofstream file(binary, std::ios::binary);
    file << "ab\xFF\xFE" << "cd";
  }
  CHECK_FALSE(ps_ui_text_view_load_file(reader, binary.c_str()));
  CHECK_FALSE(std::string(ps_ui_last_error()).empty());
  CHECK(std::string(ps_ui_text_view_get_text(reader)) == Utf8Text);
  // A bad handle fails.
  CHECK_FALSE(ps_ui_text_view_load_file(0, path.c_str()));
  CHECK_FALSE(ps_ui_text_view_save_file(0, path.c_str()));
}

// ps_ui_pref_get_bool, ps_ui_pref_set_bool, ps_ui_pref_get_int, ps_ui_pref_set_int, ps_ui_pref_list_clear,
// ps_ui_pref_list_add, ps_ui_pref_list_count, ps_ui_pref_list_get
TEST_CASE("contract: preferences round trip booleans, integers and lists") {
  ContractApp app;
  const char *flagKey = "contract.test.flag";
  const char *numberKey = "contract.test.number";
  const char *listKey = "contract.test.list";
  CHECK(ps_ui_pref_set_bool(flagKey, true));
  CHECK(ps_ui_pref_get_bool(flagKey, false));
  CHECK(ps_ui_pref_set_bool(flagKey, false));
  CHECK_FALSE(ps_ui_pref_get_bool(flagKey, true));
  CHECK(ps_ui_pref_set_int(numberKey, -42));
  CHECK(ps_ui_pref_get_int(numberKey, 7) == -42);
  CHECK(ps_ui_pref_set_int(numberKey, 2147483647));
  CHECK(ps_ui_pref_get_int(numberKey, 7) == 2147483647);

  CHECK(ps_ui_pref_list_clear(listKey));
  CHECK(ps_ui_pref_list_count(listKey) == 0);
  CHECK(ps_ui_pref_list_add(listKey, "first"));
  CHECK(ps_ui_pref_list_add(listKey, Utf8Text));
  CHECK(ps_ui_pref_list_add(listKey, ""));
  CHECK(ps_ui_pref_list_count(listKey) == 3);
  CHECK(std::string(ps_ui_pref_list_get(listKey, 0)) == "first");
  CHECK(std::string(ps_ui_pref_list_get(listKey, 1)) == Utf8Text);
  CHECK(std::string(ps_ui_pref_list_get(listKey, 2)).empty());
  CHECK(std::string(ps_ui_pref_list_get(listKey, 3)).empty());
  CHECK(std::string(ps_ui_pref_list_get(listKey, -1)).empty());
  CHECK(ps_ui_pref_list_clear(listKey));
  CHECK(ps_ui_pref_list_count(listKey) == 0);

  // An empty key is refused and a key never stored reads as the fallback.
  CHECK_FALSE(ps_ui_pref_set_bool("", true));
  CHECK_FALSE(ps_ui_pref_set_int("", 1));
  CHECK(ps_ui_pref_get_bool("contract.test.never_stored", true));
  CHECK_FALSE(ps_ui_pref_get_bool("contract.test.never_stored", false));
  CHECK(ps_ui_pref_get_int("contract.test.never_stored", 99) == 99);
  ps_ui_pref_set_bool(flagKey, false);
  ps_ui_pref_set_int(numberKey, 0);
}

// ps_ui_recent_add, ps_ui_recent_path, ps_ui_recent_clear
TEST_CASE("contract: recent files list the newest first and clear") {
  ContractApp app;
  CHECK(ps_ui_recent_clear());
  CHECK(std::string(ps_ui_recent_path(0)).empty());
  CHECK(ps_ui_recent_add("/contract/a.txt"));
  CHECK(ps_ui_recent_add("/contract/b.txt"));
  CHECK(std::string(ps_ui_recent_path(0)) == "/contract/b.txt");
  CHECK(std::string(ps_ui_recent_path(1)) == "/contract/a.txt");
  CHECK(std::string(ps_ui_recent_path(2)).empty());
  // A file that is noted again moves to the front instead of appearing twice.
  CHECK(ps_ui_recent_add("/contract/a.txt"));
  CHECK(std::string(ps_ui_recent_path(0)) == "/contract/a.txt");
  CHECK(std::string(ps_ui_recent_path(1)) == "/contract/b.txt");
  CHECK(std::string(ps_ui_recent_path(2)).empty());
  CHECK_FALSE(ps_ui_recent_add(""));
  CHECK(std::string(ps_ui_recent_path(-1)).empty());
  CHECK(ps_ui_recent_clear());
  CHECK(std::string(ps_ui_recent_path(0)).empty());
}

// every function of the ABI, shown with ps_ui_window_create and ps_ui_init
TEST_CASE("contract: a call from a thread other than the one that started the app is a fault") {
  ContractApp app;
  uint64_t foreignWindow = 1;
  bool foreignInit = true;
  const std::string windowFault = app.backend.callFromSecondThread(
      [&foreignWindow] { foreignWindow = ps_ui_window_create("Foreign", 100, 100); });
  CHECK(windowFault == "ui call off the main thread: ps_ui_window_create");
  CHECK(foreignWindow == 0);
  const std::string initFault =
      app.backend.callFromSecondThread([&foreignInit] { foreignInit = ps_ui_init("Second"); });
  CHECK(initFault == "ui call off the main thread: ps_ui_init");
  CHECK_FALSE(foreignInit);
  // The app's own thread is unaffected.
  CHECK(ps_ui_window_create("Own", 100, 100) != 0);
}

TEST_SUITE_END();
