#pragma once

// Control surface of the headless native UI backend (src/ui/headless/). The
// backend implements the whole C ABI in memory: it keeps the widget tree,
// replays a scripted list of user actions through ps_ui_wait_event, answers
// dialogs from scripted answers, and records every ABI call.
//
// One backend instance exists per process and, like the ABI, belongs to a
// single thread. Call `reset` before each scenario.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace primec::ui::headless {

// Forgets all state (widgets, queued actions, the call log) and lets
// ps_ui_init run again. Handles start again from 1.
void reset();
// Every push since the last reset, as the text form `applyHeadlessScenario` replays
// (tests/fixtures/host_calls/scenario.h): lets a test run the same scenario in a compiled program.
std::string pushedScenario();

// User actions, replayed in order by ps_ui_wait_event. When none is left,
// ps_ui_wait_event returns PS_UI_EVENT_QUIT_REQUESTED so scripted programs end.
void pushCommand(int32_t commandId);
// Types `text` at the end of a text view: the view becomes modified and a
// PS_UI_EVENT_TEXT_CHANGED event is delivered. widget 0 means the first live
// text view; an action whose target is gone by replay time is skipped.
void pushTypeText(uint64_t widget, std::string text);
void pushCloseWindow(uint64_t window);
void pushQuit();
// Makes `window` the active (key) window when replayed; no event is delivered.
void pushActivateWindow(uint64_t window);
// The user clicks a check box (it flips) or chooses item `index` of a pop-up; either delivers
// PS_UI_EVENT_CONTROL_CHANGED (recorded in pushedScenario as `check:H` and `popup:H:I`). An action whose control
// is gone or of another kind by replay time is skipped.
void pushToggleCheckBox(uint64_t checkBox);
void pushSelectPopupItem(uint64_t popup, int32_t index);
// The user clicks a button (PS_UI_EVENT_CONTROL_CHANGED, `click:H`), or the system asks the app to open a file
// (PS_UI_EVENT_OPEN_FILE, `openfile:PATH`). The application the headless "system" opens a file extension with after ps_ui_set_default_app ("" when none).
void pushClickButton(uint64_t button);
// The user selects exactly these rows of a list (PS_UI_EVENT_CONTROL_CHANGED, `rows:H:1,3`; an empty list clears the
// selection), and the rows that are selected, ascending.
void pushSelectListRows(uint64_t list, std::vector<int32_t> rows);
std::vector<int32_t> listSelection(uint64_t list);
void pushOpenFile(std::string path);
std::string defaultApp(const std::string &extension);
// The open windows of the tab group `window` belongs to, in tab order; empty when it is in none.
std::vector<uint64_t> tabGroup(uint64_t window);
// The document path stored with a window ("" when none or the handle is bad).
std::string windowPath(uint64_t window);

// Answers for the next dialogs of each kind, consumed in order. With none left
// the panels answer "" (cancelled) and alerts answer 0 (the default button).
void pushOpenPanelAnswer(std::string path);
void pushSavePanelAnswer(std::string path);
void pushAlertAnswer(int32_t button);

// One line per ABI call, in order, such as
//   ps_ui_window_create("Untitled", 720, 480) -> 2
// Strings are quoted with \" \\ \n escapes; booleans print true/false.
const std::vector<std::string> &callLog();

// Inspection of the in-memory widget tree (false / "" / 0 for a bad handle).
struct WindowState {
  bool exists = false;
  std::string title;
  int32_t width = 0;
  int32_t height = 0;
  uint64_t content = 0;
  bool edited = false;
  bool shown = false;
  bool closed = false;
  std::string autosaveName; // set with ps_ui_window_set_autosave_name
  bool preview = false;     // the preview pane is on (ps_ui_window_set_preview)
  int32_t previewRenders = 0; // how many times the preview page was built
  int32_t previewMode = 0;    // PS_UI_PREVIEW_TEXT, _SPLIT or _ONLY
  uint64_t toolbar = 0;       // the toolbar shown on the window (0 for none)
  int32_t toolbarSelected = -1; // the command of the selected segment of the toolbar (-1 for none)
  int32_t style = 0;          // the PS_UI_WINDOW_* style (ps_ui_window_set_style)
  bool outputVisible = false; // the output pane under the content (ps_ui_window_set_output_visible, a script run)
  bool scriptRunning = false; // ps_ui_script_run until its end command is delivered
  std::string output;         // what the output pane shows
};
WindowState windowState(uint64_t window);
// The page the window's preview shows (primec::ui::markdownPreviewPage of the text view's text when it
// was last rendered); "" when the window has no preview.
std::string previewPage(uint64_t window);
// The user selects bytes [start, end) of a text view (no event; `select:H:S:E` in pushedScenario), and the selection
// as {start, end}. Typing moves the caret to the end of the text.
void pushSelectText(uint64_t textView, int32_t start, int32_t end);
std::pair<int32_t, int32_t> selection(uint64_t textView);
// The toolbar shown on the window, one line per item: "button Bold [bold] #20", "segments Editor|Split|Preview #1
// [selected 2]", "menu Heading [textformat]" followed by its items indented ("  Heading 1 #31"), "-" for a separator.
// Empty when the window has none.
std::vector<std::string> toolbarOutline(uint64_t window);
bool isMonospace(uint64_t textView);
// True when the line number bar of the text view is shown.
bool lineNumbersVisible(uint64_t textView);
// True when the text view wraps long lines (the default).
bool lineWrapping(uint64_t textView);
// The point size set with ps_ui_text_view_set_font_size (0 when never set).
int32_t fontSize(uint64_t textView);
// How the preview's source and page scroll together (PS_UI_SCROLL_*; blocks by default).
int32_t previewScrollMode();
bool checkBoxChecked(uint64_t checkBox);
// The chosen item of a pop-up (-1 for none or a bad handle).
int32_t popupSelected(uint64_t popup);
// The controls under `root` as indented lines, such as
//   tabs selected=0
//     page "General":
//       form
//         heading "Startup"
//         row "Font size": popup [12|14] selected=1
//         control: checkbox "Wrap" [checked]
std::vector<std::string> controlOutline(uint64_t root);
// Notes a recent file the way the program does (recorded in pushedScenario as `recent:path`); the
// list, most recent first.
void pushRecentFile(const std::string &path);
std::vector<std::string> recentFiles();
// True when some menu item added with `commandId` carries a check mark.
bool menuItemChecked(int32_t commandId);
// The boolean preferences: the program reads and writes them through ps_ui_pref_*; a test sets a
// starting value before the run (recorded in pushedScenario as `pref:key=0|1`) and reads it back.
void setBoolPref(const std::string &key, bool value);
bool boolPref(const std::string &key, bool fallback);
// The integer and list preferences the same way (`pint:key=3` and `plist:key=a,b` in pushedScenario).
void setIntPref(const std::string &key, int32_t value);
int32_t intPref(const std::string &key, int32_t fallback);
void setPrefList(const std::string &key, const std::vector<std::string> &items);
std::vector<std::string> prefList(const std::string &key);
// One styled byte range of a text view, in call order (see ps_ui_text_view_add_style).
struct StyleRun {
  int32_t start = 0;
  int32_t end = 0;
  int32_t rgb = 0;
  int32_t flags = 0;
  bool operator==(const StyleRun &other) const = default;
};
std::vector<StyleRun> styleRuns(uint64_t textView);
// "File>Save (cmd+s) #3", "File>-" for a separator, "File>standard 4" for a
// standard item, "File>Open Recent>" for a submenu followed by its items
// ("File>Open Recent>a.txt #200"); only menus added to the menu bar are listed, in bar order.
std::vector<std::string> menuBarItems();
// The menu bar as the user would read it, in bar order (an application-role menu
// first): "File>Save As... (cmd+shift+s)", "File>-" for a separator. Standard
// items show the title and shortcut the platform backends give them
// (include/primec/ui/NativeUiStandardItems.h).
std::vector<std::string> menuBarOutline();

} // namespace primec::ui::headless
