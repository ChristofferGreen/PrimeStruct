#pragma once

/* PrimeStruct native UI C ABI, version 0 (docs/NativeUiPlan.md section 4).
 *
 * Plain C types only. Handles are opaque non-zero uint64_t values; 0 means
 * "none" or failure. Text in and out is UTF-8. Every function must be called on
 * the thread that called ps_ui_init (the main thread on macOS); a call from
 * another thread fails like any other bad call. Functions never abort: a bad
 * handle, a call before ps_ui_init, or a call after the window closed returns
 * 0 / false / "" and changes nothing.
 *
 * Text returned as `const char *` is owned by the library and valid until the
 * next call that returns text (ps_ui_text_view_get_text, ps_ui_open_panel,
 * ps_ui_save_panel, ps_ui_last_error). Copy it before then.
 *
 * Versioning: additions append functions; changing an existing signature bumps
 * ps_ui_abi_version().
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_UI_ABI_VERSION 0

/* ps_ui_platform() results. */
enum {
  PS_UI_PLATFORM_MACOS = 1,
  PS_UI_PLATFORM_WINDOWS = 2,
  PS_UI_PLATFORM_LINUX = 3,
  PS_UI_PLATFORM_IOS = 4,
  PS_UI_PLATFORM_HEADLESS = 5
};

/* ps_ui_wait_event() results. */
enum {
  PS_UI_EVENT_NONE = 0,
  /* A menu item or shortcut fired; ps_ui_event_command() is its command id. */
  PS_UI_EVENT_COMMAND = 1,
  /* The user asked to close ps_ui_event_window(); the program decides whether
   * to call ps_ui_window_close. */
  PS_UI_EVENT_WINDOW_CLOSE_REQUESTED = 2,
  /* The text of ps_ui_event_widget() (inside ps_ui_event_window()) changed. */
  PS_UI_EVENT_TEXT_CHANGED = 3,
  /* The platform asked the app to quit (Cmd+Q, last window gone, ...). */
  PS_UI_EVENT_QUIT_REQUESTED = 4
};

/* Standard menu items for ps_ui_menu_add_standard. They map to the platform's
 * own actions, so they work in the focused native widget without program code. */
enum {
  PS_UI_STANDARD_UNDO = 1,
  PS_UI_STANDARD_REDO = 2,
  PS_UI_STANDARD_CUT = 3,
  PS_UI_STANDARD_COPY = 4,
  PS_UI_STANDARD_PASTE = 5,
  PS_UI_STANDARD_SELECT_ALL = 6,
  PS_UI_STANDARD_FIND = 7,
  PS_UI_STANDARD_QUIT = 8,
  PS_UI_STANDARD_ABOUT = 9,
  PS_UI_STANDARD_HIDE = 10,
  PS_UI_STANDARD_HIDE_OTHERS = 11,
  PS_UI_STANDARD_MINIMIZE = 12,
  PS_UI_STANDARD_ZOOM = 13,
  PS_UI_STANDARD_BRING_ALL_TO_FRONT = 14,
  PS_UI_STANDARD_FULL_SCREEN = 15,
  PS_UI_STANDARD_HELP = 16,
  PS_UI_STANDARD_SHOW_TAB_BAR = 17,
  PS_UI_STANDARD_MERGE_ALL_WINDOWS = 18,
  PS_UI_STANDARD_MOVE_TAB_TO_NEW_WINDOW = 19,
  PS_UI_STANDARD_NEXT_TAB = 20,
  PS_UI_STANDARD_PREVIOUS_TAB = 21
};

/* Roles of a menu in the menu bar for ps_ui_menu_bar_add_role. */
enum {
  /* Takes the place of the platform's default application menu (first). */
  PS_UI_MENU_ROLE_APP = 1,
  /* The platform lists the open windows in it. */
  PS_UI_MENU_ROLE_WINDOW = 2,
  /* The platform adds its help search to it. */
  PS_UI_MENU_ROLE_HELP = 3
};

/* App ------------------------------------------------------------------ */

/* The ABI version this build implements (PS_UI_ABI_VERSION). */
int32_t ps_ui_abi_version(void);
/* Starts the app on the calling thread. False if already started or the
 * backend cannot start. */
bool ps_ui_init(const char *appName);
/* Ends the app: later waits return PS_UI_EVENT_QUIT_REQUESTED. */
void ps_ui_quit(void);
int32_t ps_ui_platform(void);

/* Events ---------------------------------------------------------------- */

/* Blocks until something the program should handle happens and returns its
 * kind. The ps_ui_event_* accessors read that event until the next wait; fields
 * that do not apply to the kind read as 0. Typing, selection, scrolling, copy,
 * paste and undo are handled by the native widget and never reach the program. */
int32_t ps_ui_wait_event(void);
uint64_t ps_ui_event_window(void);
uint64_t ps_ui_event_widget(void);
int32_t ps_ui_event_command(void);

/* Window ---------------------------------------------------------------- */

uint64_t ps_ui_window_create(const char *title, int32_t width, int32_t height);
bool ps_ui_window_set_title(uint64_t window, const char *title);
/* Makes `widget` (a text view) the window's content view. */
bool ps_ui_window_set_content(uint64_t window, uint64_t widget);
/* Shows the platform's unsaved-changes marker. */
bool ps_ui_window_set_edited(uint64_t window, bool edited);
bool ps_ui_window_show(uint64_t window);
/* Closes the window and invalidates its handle. */
bool ps_ui_window_close(uint64_t window);

/* Tabs and documents ------------------------------------------------------ */

/* Joins `tab` to the native tab group of `host` (the tabs of one window on
 * macOS) and makes it the active tab. False when either window is bad, they are
 * the same, or `tab` is already in a group. */
bool ps_ui_window_add_tab(uint64_t host, uint64_t tab);
/* The window the user is working in (the key window, or the selected tab of its
 * group); 0 when no window is open. Menu commands act on it. */
uint64_t ps_ui_active_window(void);
/* The tabs of the group `window` belongs to, in the order the tab bar shows them
 * (dragging reorders them): their number (1 for a window in no group, 0 for a bad
 * handle) and the index-th of them (0 when out of range). */
int32_t ps_ui_window_tab_count(uint64_t window);
uint64_t ps_ui_window_tab_at(uint64_t window, int32_t index);
/* Selects the window's tab and makes it the active window. */
bool ps_ui_window_select(uint64_t window);
/* The text view set with ps_ui_window_set_content; 0 for none or a bad handle. */
uint64_t ps_ui_window_content(uint64_t window);
/* Open windows (tabs count separately), and the index-th of them in creation
 * order; 0 for an index out of range. Closing a window removes it. */
int32_t ps_ui_window_count(void);
uint64_t ps_ui_window_at(int32_t index);
/* The file a window's document is saved to; "" when it has none. The backend
 * only stores it, so programs keep per-document state without string handling. */
bool ps_ui_window_set_path(uint64_t window, const char *path);
const char *ps_ui_window_path(uint64_t window);
bool ps_ui_window_has_path(uint64_t window);

/* Text view -------------------------------------------------------------- */

uint64_t ps_ui_text_view_create(void);
/* The view's text; "" for a bad handle. See the lifetime note above. */
const char *ps_ui_text_view_get_text(uint64_t view);
/* Replaces the text. Programmatic changes do not mark the view modified and do
 * not produce PS_UI_EVENT_TEXT_CHANGED. */
bool ps_ui_text_view_set_text(uint64_t view, const char *text);
bool ps_ui_text_view_set_monospace(uint64_t view, bool monospace);
/* True once the user changed the text since creation or the last clear. */
bool ps_ui_text_view_is_modified(uint64_t view);
bool ps_ui_text_view_clear_modified(uint64_t view);
/* Shows or hides a bar of line numbers to the left of the text (one number per
 * paragraph; wrapped lines are not numbered). Presentation only: it never marks the
 * view modified or produces events. Views start without the bar. */
bool ps_ui_text_view_set_line_numbers(uint64_t view, bool visible);

/* Styled text ------------------------------------------------------------ */

/* Style flags for ps_ui_text_view_add_style. */
enum { PS_UI_STYLE_BOLD = 1, PS_UI_STYLE_ITALIC = 2 };

/* Removes every style run, restoring the view's base font and text colour. */
bool ps_ui_text_view_clear_styles(uint64_t view);
/* Styles the UTF-8 byte range [startByte, endByte) of the view's text with the
 * colour 0xRRGGBB and PS_UI_STYLE_* flags. Both ends must lie on code point
 * boundaries inside the text, and the range must not be empty; otherwise nothing
 * changes and the call returns false. Where runs overlap, the later call wins.
 * Styling is presentation only: it never marks the view modified, produces
 * PS_UI_EVENT_TEXT_CHANGED or enters the undo stack. Replacing the text
 * (set_text, load_file) drops all runs; typing keeps the runs of the text around
 * it, so programs restyle after PS_UI_EVENT_TEXT_CHANGED. */
bool ps_ui_text_view_add_style(uint64_t view, int32_t startByte, int32_t endByte, int32_t rgb, int32_t flags);
/* Replaces the view's style runs with syntax highlighting for the language of
 * the file name `path` (by extension: PrimeStruct .prime, C/C++/Objective-C
 * .c .h .cc .cpp .hpp .m .mm ..., Python .py, JSON .json, Markdown .md,
 * JavaScript/TypeScript, Rust, Go, Java, Kotlin, Swift, shell, SQL and CMake;
 * the full table is in primec/ui/SyntaxHighlight.h); an
 * unknown extension or "" just clears the styles, as does text over 2 MiB.
 * Programs call it after loading or saving and on PS_UI_EVENT_TEXT_CHANGED. */
bool ps_ui_text_view_highlight(uint64_t view, const char *path);

/* Menus ------------------------------------------------------------------ */

uint64_t ps_ui_menu_create(const char *title);
/* `shortcut` is portable text such as "cmd+s" or "cmd+shift+s" ("" for none);
 * `cmd` is Command on macOS and Ctrl elsewhere. Choosing the item produces
 * PS_UI_EVENT_COMMAND with `commandId`. */
bool ps_ui_menu_add_item(uint64_t menu, const char *title, const char *shortcut, int32_t commandId);
bool ps_ui_menu_add_separator(uint64_t menu);
bool ps_ui_menu_add_standard(uint64_t menu, int32_t standardId);
/* Puts or removes a check mark on the items of `menu` added with `commandId`
 * (ps_ui_menu_add_item). False for a bad menu or when no item has that id. */
bool ps_ui_menu_set_checked(uint64_t menu, int32_t commandId, bool checked);
/* Appends the menu to the app's menu bar. */
bool ps_ui_menu_bar_add(uint64_t menu);
/* Appends the menu to the menu bar with a platform role (PS_UI_MENU_ROLE_*).
 * The application menu replaces the default one and comes first. False for a
 * bad menu or role, or a menu that is already in the bar. */
bool ps_ui_menu_bar_add_role(uint64_t menu, int32_t role);

/* Text files -------------------------------------------------------------- */

/* Replaces the view's text with the UTF-8 file at `path`, the way a native
 * editor opens a document: the backend reads the file itself (no program-side
 * string is built), the view is not modified afterwards, and large files do not
 * pass through the program. False when the file cannot be read or is not valid
 * UTF-8; the view is left unchanged and ps_ui_last_error() says why. */
bool ps_ui_text_view_load_file(uint64_t view, const char *path);
/* Writes the view's text to `path` as UTF-8 (replacing the file). It does not
 * clear the modified flag; the program does that with ps_ui_text_view_clear_modified
 * once it considers the text saved. False on failure, see ps_ui_last_error(). */
bool ps_ui_text_view_save_file(uint64_t view, const char *path);
/* Describes the last failed call of the file functions; "" when none has
 * failed. See the lifetime note above (valid until the next text-returning call
 * or file function). */
const char *ps_ui_last_error(void);

/* Preferences ---------------------------------------------------------------- */

/* A boolean the app keeps between runs under `key` (NSUserDefaults on macOS):
 * its stored value, or `fallback` when none was stored. */
bool ps_ui_pref_get_bool(const char *key, bool fallback);
/* Stores the boolean under `key`. False for an empty key or before ps_ui_init. */
bool ps_ui_pref_set_bool(const char *key, bool value);

/* Dialogs ---------------------------------------------------------------- */

/* The chosen path, or "" when cancelled. See the lifetime note above. */
const char *ps_ui_open_panel(const char *title);
const char *ps_ui_save_panel(const char *title, const char *suggestedName);
/* True when the last open/save panel returned a path, false when it was
 * cancelled. Programs use it instead of testing the returned text for
 * emptiness. */
bool ps_ui_panel_chosen(void);
/* Shows an alert with the given buttons (titles separated by '\n'; the first
 * is the default) and returns the zero-based index of the button pressed, or
 * -1 for a bad call. */
int32_t ps_ui_alert(const char *message, const char *detail, const char *buttons);

#ifdef __cplusplus
}
#endif
