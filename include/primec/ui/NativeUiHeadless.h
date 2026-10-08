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
#include <vector>

namespace primec::ui::headless {

// Forgets all state (widgets, queued actions, the call log) and lets
// ps_ui_init run again. Handles start again from 1.
void reset();

// User actions, replayed in order by ps_ui_wait_event. When none is left,
// ps_ui_wait_event returns PS_UI_EVENT_QUIT_REQUESTED so scripted programs end.
void pushCommand(int32_t commandId);
// Types `text` at the end of a text view: the view becomes modified and a
// PS_UI_EVENT_TEXT_CHANGED event is delivered. widget 0 means the first live
// text view; an action whose target is gone by replay time is skipped.
void pushTypeText(uint64_t widget, std::string text);
void pushCloseWindow(uint64_t window);
void pushQuit();

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
};
WindowState windowState(uint64_t window);
bool isMonospace(uint64_t textView);
// "File>Save (cmd+s) #3", "File>-" for a separator, "File>standard 4" for a
// standard item; only menus added to the menu bar are listed, in bar order.
std::vector<std::string> menuBarItems();
// The menu bar as the user would read it, in bar order (an application-role menu
// first): "File>Save As... (cmd+shift+s)", "File>-" for a separator. Standard
// items show the title and shortcut the platform backends give them
// (include/primec/ui/NativeUiStandardItems.h).
std::vector<std::string> menuBarOutline();

} // namespace primec::ui::headless
