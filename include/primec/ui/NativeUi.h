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
 * ps_ui_save_panel, ps_ui_last_error, ps_ui_recent_path, ps_ui_pref_list_get). Copy it before then.
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
  PS_UI_EVENT_QUIT_REQUESTED = 4,
  /* The user changed a control, ps_ui_event_widget() (inside ps_ui_event_window()): toggled a check box
   * or chose another item of a pop-up. Read the new state with the control's accessor. */
  PS_UI_EVENT_CONTROL_CHANGED = 5,
  /* The system asks the app to open a file (a double click in the Finder, Open With, a file dropped on the app's
   * icon): its path is ps_ui_event_text(). */
  PS_UI_EVENT_OPEN_FILE = 6
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

/* Styles of a window for ps_ui_window_set_style. */
enum {
  /* A document window: it can join a tab group and be resized (the default). */
  PS_UI_WINDOW_DOCUMENT = 0,
  /* A settings window: never a tab, sized to fit its content, no minimize or zoom. */
  PS_UI_WINDOW_SETTINGS = 1
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
/* The text of the event: the path of a PS_UI_EVENT_OPEN_FILE; "" for the other kinds. See the lifetime note
 * above. */
const char *ps_ui_event_text(void);

/* Window ---------------------------------------------------------------- */

uint64_t ps_ui_window_create(const char *title, int32_t width, int32_t height);
bool ps_ui_window_set_title(uint64_t window, const char *title);
/* Makes `widget` the window's content view: a text view, or a control that nothing holds yet (usually a
 * form or tabs, see Controls and forms). */
bool ps_ui_window_set_content(uint64_t window, uint64_t widget);
/* Shows the platform's unsaved-changes marker. */
bool ps_ui_window_set_edited(uint64_t window, bool edited);
bool ps_ui_window_show(uint64_t window);
/* Remembers the window's position and size between runs under `name` and restores a saved frame
 * now (NSWindow frame autosave on macOS). Use one name per window that should come back where the
 * user left it; false for a bad window, an empty name or a name another window already uses. */
bool ps_ui_window_set_autosave_name(uint64_t window, const char *name);
/* Sets a PS_UI_WINDOW_* style. False for a bad window or style, or for a settings style on a window that is
 * already in a tab group. */
bool ps_ui_window_set_style(uint64_t window, int32_t style);
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

/* Controls and forms ------------------------------------------------------ */

/* The pieces of a settings window. Every function returns a handle (0 for a failed create) or a flag like the
 * rest of the ABI. A control is held by one form or tabs, or is the content of a window; putting one where
 * another already holds it fails. When the user changes a check box or a pop-up the program gets
 * PS_UI_EVENT_CONTROL_CHANGED; changes the program makes itself produce no event. A backend without controls
 * fails every call in this section like a bad handle. */

uint64_t ps_ui_label_create(const char *text);
bool ps_ui_label_set_text(uint64_t label, const char *text);

uint64_t ps_ui_checkbox_create(const char *title, bool checked);
bool ps_ui_checkbox_set_checked(uint64_t checkbox, bool checked);
bool ps_ui_checkbox_checked(uint64_t checkbox);

/* A pop-up button: add its items in order, the first one is chosen at first. */
uint64_t ps_ui_popup_create(void);
bool ps_ui_popup_add_item(uint64_t popup, const char *title);
bool ps_ui_popup_select(uint64_t popup, int32_t index);
/* The chosen item's index, -1 for a pop-up without items or a bad handle. */
int32_t ps_ui_popup_selected(uint64_t popup);

/* A form lays its rows out in a column: a bold heading, a row with a label to the left of a control (the
 * labels line up, the controls line up), or a control alone in the controls' column (a check box). */
uint64_t ps_ui_form_create(void);
bool ps_ui_form_add_heading(uint64_t form, const char *title);
bool ps_ui_form_add_row(uint64_t form, const char *label, uint64_t control);
bool ps_ui_form_add_control(uint64_t form, uint64_t control);

/* Tabs show one page at a time under a row of tab titles; the first page added is shown. */
uint64_t ps_ui_tabs_create(void);
bool ps_ui_tabs_add(uint64_t tabs, const char *title, uint64_t content);
bool ps_ui_tabs_select(uint64_t tabs, int32_t index);

/* Markdown preview -------------------------------------------------------- */

/* Turns the window's preview pane on or off. With it on the window shows its text view and, beside it,
 * the markdown of that text rendered as a page (primec::ui::markdownPreviewPage); turning it off puts
 * the text view back alone. The backend builds the preview only when it is turned on. False for a bad
 * handle, or for turning it on in a window with no text view as its content; turning it on while it is
 * on succeeds and changes nothing. */
bool ps_ui_window_set_preview(uint64_t window, bool on);
bool ps_ui_window_has_preview(uint64_t window);
/* Renders the preview again from the text view's current text. False when the window has no preview.
 * A backend may coalesce refreshes that follow each other closely. */
bool ps_ui_window_refresh_preview(uint64_t window);
/* Makes the check mark of the items of `menu` with `commandId` show whether the active window has a
 * preview, instead of the state ps_ui_menu_set_checked gave them. False for a bad menu or an unknown
 * command id. */
bool ps_ui_menu_follow_preview(uint64_t menu, int32_t commandId);
/* How the source and the page of a preview scroll together: PS_UI_SCROLL_OFF (independently),
 * PS_UI_SCROLL_LINEAR (the same fraction of what each can scroll), PS_UI_SCROLL_BLOCKS (the same source line,
 * by the page's blocks; the default) or PS_UI_SCROLL_CURSOR (the page follows the text cursor). False for any
 * other value. */
enum {
  PS_UI_SCROLL_OFF = 0,
  PS_UI_SCROLL_LINEAR = 1,
  PS_UI_SCROLL_BLOCKS = 2,
  PS_UI_SCROLL_CURSOR = 3,
};
bool ps_ui_preview_set_scroll_mode(int32_t mode);

/* How much of the window the preview takes: PS_UI_PREVIEW_TEXT (the text view alone), PS_UI_PREVIEW_SPLIT (the text view and
 * the page, what ps_ui_window_set_preview(window, true) gives) or PS_UI_PREVIEW_ONLY (the page alone; the text view stays
 * alive and keeps its text and selection). False for a bad handle, another value, or a mode other than TEXT in a window
 * with no text view as its content. ps_ui_window_has_preview is true in SPLIT and ONLY; turning the preview off with
 * ps_ui_window_set_preview(window, false) is TEXT. */
enum {
  PS_UI_PREVIEW_TEXT = 0,
  PS_UI_PREVIEW_SPLIT = 1,
  PS_UI_PREVIEW_ONLY = 2,
};
bool ps_ui_window_set_preview_mode(uint64_t window, int32_t mode);
/* The window's mode (PS_UI_PREVIEW_*); 0 for a bad handle. */
int32_t ps_ui_window_preview_mode(uint64_t window);

/* Toolbar ------------------------------------------------------------------ */

/* A toolbar is a row of items above a window's content. Every item is a command: choosing it produces
 * PS_UI_EVENT_COMMAND with its command id, like a menu item, so a program handles one command from the toolbar, the
 * menu and a shortcut alike. A toolbar belongs to the window it is set on; it is built once and may be set on, and
 * hidden from, any number of windows. `symbol` names a platform icon (SF Symbols on macOS, such as "bold" or
 * "list.bullet"); the title is the tooltip and the fallback when the platform has no such icon. */
uint64_t ps_ui_toolbar_create(void);
bool ps_ui_toolbar_add_button(uint64_t toolbar, const char *title, const char *symbol, int32_t commandId);
/* A segmented control: `titles` is a list separated by '|', and the segments produce the commands `firstCommandId`,
 * `firstCommandId + 1`, ... One segment is selected at a time (see ps_ui_window_toolbar_select). */
bool ps_ui_toolbar_add_segments(uint64_t toolbar, const char *titles, int32_t firstCommandId);
/* A button that opens `menu` (a menu made with ps_ui_menu_create that is not in the menu bar); its items produce their
 * command ids. */
bool ps_ui_toolbar_add_menu(uint64_t toolbar, const char *title, const char *symbol, uint64_t menu);
bool ps_ui_toolbar_add_separator(uint64_t toolbar);
/* Shows `toolbar` on the window, or hides the window's toolbar with 0. False for a bad window or toolbar. */
bool ps_ui_window_set_toolbar(uint64_t window, uint64_t toolbar);
/* Selects the segment with `commandId` in the window's toolbar (it shows selected; no event). False when the window has
 * no toolbar or no segment has that command. */
bool ps_ui_window_toolbar_select(uint64_t window, int32_t commandId);

/* Running scripts ------------------------------------------------------------ */

/* Compiles and runs PrimeScript `text` (named `name` in diagnostics and as the base of its imports) on the embedded
 * interpreter, on a worker thread, and shows what it prints in the window's output pane: the pane is cleared and shown
 * when the run starts, the output (stdout and stderr in program order) is appended as it arrives, and the run ends with
 * a line saying how ("[exit code N]", the compile or runtime diagnostic, or "[stopped]"). The script takes no
 * arguments and has no host functions. When the run ends PS_UI_EVENT_COMMAND arrives with `endCommand` and the
 * window's handle, so a program can re-enable its Run button. False for a bad window, a run already going on the
 * window, or when the interpreter cannot be loaded (the pane then says why). The interpreter library is loaded on the
 * first run, never at launch. */
bool ps_ui_script_run(uint64_t window, const char *name, const char *text, int32_t endCommand);
/* Asks the window's run to stop; it ends soon after with "[stopped]". False when none is running. */
bool ps_ui_script_stop(uint64_t window);
/* True from ps_ui_script_run until the end command has been delivered. */
bool ps_ui_script_running(uint64_t window);
/* Shows or hides the output pane under the window's content (the text kept); a run shows it. False for a bad window. */
bool ps_ui_window_set_output_visible(uint64_t window, bool visible);
bool ps_ui_window_output_visible(uint64_t window);

/* Text view -------------------------------------------------------------- */

uint64_t ps_ui_text_view_create(void);
/* The view's text; "" for a bad handle. See the lifetime note above. */
const char *ps_ui_text_view_get_text(uint64_t view);
/* Replaces the text. Programmatic changes do not mark the view modified and do
 * not produce PS_UI_EVENT_TEXT_CHANGED. */
bool ps_ui_text_view_set_text(uint64_t view, const char *text);
bool ps_ui_text_view_set_monospace(uint64_t view, bool monospace);
/* The size of the view's text in points (6 to 96); false outside that. */
bool ps_ui_text_view_set_font_size(uint64_t view, int32_t points);
/* Markdown editing (primec/ui/MarkdownEdit.h): applies `operation` (PS_UI_MARKDOWN_*) to the text and the selection of
 * the view as one undoable edit, the way a markdown editor's buttons do. `a` and `b` are the operation's arguments (the
 * heading level, the table's columns and rows, the alignment). The change is the user's: the view is modified and
 * PS_UI_EVENT_TEXT_CHANGED is produced. False for a bad handle or operation, or when the operation does not apply
 * (a table operation with the cursor outside a table). */
enum {
  PS_UI_MARKDOWN_BOLD = 0,
  PS_UI_MARKDOWN_ITALIC = 1,
  PS_UI_MARKDOWN_STRIKE = 2,
  PS_UI_MARKDOWN_CODE = 3,
  PS_UI_MARKDOWN_LINK = 4,
  PS_UI_MARKDOWN_IMAGE = 5,
  PS_UI_MARKDOWN_HEADING = 6,
  PS_UI_MARKDOWN_BULLETS = 7,
  PS_UI_MARKDOWN_NUMBERS = 8,
  PS_UI_MARKDOWN_TASKS = 9,
  PS_UI_MARKDOWN_QUOTE = 10,
  PS_UI_MARKDOWN_CODE_BLOCK = 11,
  PS_UI_MARKDOWN_RULE = 12,
  PS_UI_MARKDOWN_TABLE_INSERT = 13,
  PS_UI_MARKDOWN_TABLE_FORMAT = 14,
  PS_UI_MARKDOWN_TABLE_ADD_ROW = 15,
  PS_UI_MARKDOWN_TABLE_ADD_COLUMN = 16,
  PS_UI_MARKDOWN_TABLE_DELETE_ROW = 17,
  PS_UI_MARKDOWN_TABLE_DELETE_COLUMN = 18,
  PS_UI_MARKDOWN_TABLE_ALIGN = 19,
  PS_UI_MARKDOWN_TABLE_NEXT_CELL = 20,
  PS_UI_MARKDOWN_TABLE_PREVIOUS_CELL = 21,
};
bool ps_ui_text_view_markdown_edit(uint64_t view, int32_t operation, int32_t a, int32_t b);
/* With it on, Tab and Shift-Tab in the view move between the cells of a markdown table when the cursor is in one (and
 * are ordinary otherwise). Off by default. */
bool ps_ui_text_view_set_markdown_editing(uint64_t view, bool on);
/* True once the user changed the text since creation or the last clear. */
bool ps_ui_text_view_is_modified(uint64_t view);
bool ps_ui_text_view_clear_modified(uint64_t view);
/* Shows or hides a bar of line numbers to the left of the text (one number per
 * paragraph; wrapped lines are not numbered). Presentation only: it never marks the
 * view modified or produces events. Views start without the bar. */
bool ps_ui_text_view_set_line_numbers(uint64_t view, bool visible);
/* Wraps long lines at the view's width (the default) or lets them run to the right with a
 * horizontal scroll bar. Presentation only. */
bool ps_ui_text_view_set_wrapping(uint64_t view, bool wrap);

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
/* Adds an item titled `title` that opens `submenu` (a menu made with ps_ui_menu_create that is not
 * in the menu bar and not a submenu already). False for bad handles. */
bool ps_ui_menu_add_submenu(uint64_t menu, const char *title, uint64_t submenu);
/* Removes every item of the menu (to rebuild a dynamic submenu such as the recent files). */
bool ps_ui_menu_clear(uint64_t menu);
/* Appends one item per recent file (see ps_ui_recent_add), most recent first, at most `maxItems`,
 * titled with the file name (the folder name is added when two files share a name). The items
 * produce PS_UI_EVENT_COMMAND with `firstCommandId + index`; the program opens
 * ps_ui_recent_path(index). Returns how many items it added. */
int32_t ps_ui_menu_add_recent_files(uint64_t menu, int32_t firstCommandId, int32_t maxItems);

/* Recent files --------------------------------------------------------------- */

/* Notes `path` as the most recently used file (the platform's recent documents list, which also
 * survives between runs). */
bool ps_ui_recent_add(const char *path);
/* The index-th recent file, most recent first; "" when there are fewer. See the lifetime note
 * above (valid until the next text-returning call). */
const char *ps_ui_recent_path(int32_t index);
bool ps_ui_recent_clear(void);
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
/* The same for an integer. */
int32_t ps_ui_pref_get_int(const char *key, int32_t fallback);
bool ps_ui_pref_set_int(const char *key, int32_t value);
/* A list of texts kept under `key`: empty it, append to it, its length and its index-th item ("" when
 * out of range; see the lifetime note above). Programs keep what they had open this way. */
bool ps_ui_pref_list_clear(const char *key);
bool ps_ui_pref_list_add(const char *key, const char *text);
int32_t ps_ui_pref_list_count(const char *key);
const char *ps_ui_pref_list_get(const char *key, int32_t index);

/* Buttons ---------------------------------------------------------------- */

/* A push button for a settings window; held by a form (or the content of a window) like the other controls.
 * The user's click produces PS_UI_EVENT_CONTROL_CHANGED with the button as ps_ui_event_widget(). */
uint64_t ps_ui_button_create(const char *title);
bool ps_ui_button_set_title(uint64_t button, const char *title);

/* Lists ------------------------------------------------------------------ */

/* A list of text rows of which any number can be selected (the user's click, shift and command click, select
 * all), for a settings window; held by a form (or the content of a window) like the other controls. The user's
 * change of the selection produces PS_UI_EVENT_CONTROL_CHANGED with the list as ps_ui_event_widget(); the
 * program's own changes produce none. Rows are numbered from 0 in the order they were added. */
uint64_t ps_ui_list_create(void);
bool ps_ui_list_add_item(uint64_t list, const char *text);
bool ps_ui_list_set_item(uint64_t list, int32_t index, const char *text);
int32_t ps_ui_list_item_count(uint64_t list);
bool ps_ui_list_set_selected(uint64_t list, int32_t index, bool selected);
bool ps_ui_list_is_selected(uint64_t list, int32_t index);

/* File types and the default application ------------------------------------ */

/* The kinds of file the app declares it can open (the document types of its Info.plist on macOS): a name for
 * the group ("Markdown") and its extensions without dots, separated by commas ("md,markdown"). Index out of
 * range reads as "". See the lifetime note above. */
int32_t ps_ui_document_type_count(void);
const char *ps_ui_document_type_name(int32_t index);
const char *ps_ui_document_type_extensions(int32_t index);
/* The path of the app (its bundle on macOS); "" when the program does not run from one. */
const char *ps_ui_app_path(void);
/* The path of the application the system opens files with `extension` with ("md" or ".md"); "" when none or
 * unknown. */
const char *ps_ui_default_app(const char *extension);
/* Makes the application at `appPath` ("" or NULL: this app) the system's default for every extension of the
 * comma separated list. False when the list is empty or the system refuses (the user may be asked to confirm). */
bool ps_ui_set_default_app(const char *extensions, const char *appPath);

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
