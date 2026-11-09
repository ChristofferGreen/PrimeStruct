# Native UI: Plan

Status: in progress (2026-10-08). The C ABI header, the headless backend and the engine bindings exist
(`include/primec/ui/`, `src/ui/`; TODO-5528); the rest is tracked as the TODO slices in section 11.

PrimeStruct programs get desktop GUIs built from **platform-native widgets**: an `NSTextView` on macOS, an `EDIT`
control or RichEdit on Windows, a `GtkTextView` on Linux. The first target is macOS and the first example is a plain
text editor. This plan is separate from `docs/Graphics_API_Design.md`, whose `/std/scene` graph stays the path for
custom-drawn UI (games, canvases, the web host); native widgets never go through the scene
renderer.

## 1. Startup time comes first

Most macOS editors are slow to open because of what they load before the first window: a browser engine or a JVM,
plugin and extension hosts, indexers, document-restoration and iCloud lookups, or a compiler. The editor's first
requirement is to open as fast as a native app can. Every design choice below is checked against that, and launch time
is measured, not assumed.

**Budget.** On a warm launch, the PrimeStruct editor shows its window within 5 ms of a pure Objective-C++ editor with
the same UI (the reference build in TODO-5535), and shows the text of a 1 MB file within one more frame.

**Measured so far** (Linux, release build, 2026-10-08; wall time of a whole process run):

| Path | Small program (21 KB bytecode) | PNG decoder (12.7 MB bytecode) |
| --- | --- | --- |
| Empty C program | 3.5 ms | — |
| Precompiled bytecode on the runtime-only VM (406 KB executable) | 4.5 ms | 80 ms |
| Compile the source at launch, then run | 68 ms | 3.7 s |

**Measured on the Mac, 2026-10-22** (`scripts/measure_app_startup.sh`, retired since for `scripts/measure_startup.sh`; 10 runs of the editor in the AppKit backend's
snapshot mode, so wall time to the first laid-out and drawn window including the snapshot itself, and the peak
resident set; not the 5 ms budget measurement, which is TODO-5535):

| PrimeScroll build | Bundle executable | Bytecode | No file: mean / peak RSS | Opening the 3-language showcase file: mean / peak RSS |
| --- | --- | --- | --- | --- |
| Bytecode bundle (runtime-only VM + `app.psir`) | 1.28 MB | 1.16 MB | 1526 ms / 123.5 MB | 1091 ms / 126.4 MB |
| Native bundle (`optexe` + AppKit backend) | 0.29 MB | none | 912 to 967 ms / 112 to 116 MB | 1057 ms / 122.4 MB |

**Where the memory goes.** A minimal Objective-C++ app (one window, one `NSTextView`, 20 run loop turns, no
PrimeStruct code) peaks at 90 MB resident; the native `hello_window` in snapshot mode peaks at 110 MB and PrimeScroll
at 112 MB. So about 90 MB is AppKit and TextKit themselves, snapshot mode adds about 20 MB for rendering the PNG, and
everything PrimeStruct adds on top of hello_window is about 2 MB (the editor's own widgets, the string heap and the
highlighter's runs). There is no PrimeStruct-owned memory to trim here; the 120 MB is the platform's text stack.

The native build is 5 times smaller on disk and uses about 10 MB less memory; the snapshot wall time is dominated by
AppKit's own window creation and layout, and the run-to-run noise (the first bytecode series includes cold caches) is
larger than the VM's own cost, so the launch harness of TODO-5535 is still the way to compare against the budget.

So the VM itself costs about 1 ms; compiling at launch is never acceptable; and bytecode size is the cost that
matters. The large program's size comes from lowering inlining almost every call, which TODO-5536 addresses.

**Where a launch goes** (2026-11-09, TODO-5603; raw data in `docs/todo_log.md` under TODO-5603). The user reported
launches that often take 3 to 5 s. `scripts/measure_startup.sh` loads a probe (`tools/startup_probe/startup_probe.c`)
into the app with `DYLD_INSERT_LIBRARIES` and times each phase from the process start: dyld done (`main`), `ps_ui_init`
returned (`app_init`), the program's first `ps_ui_wait_event` (`loop`), the first window display and the first drawn
text. The earlier figure of about 900 ms (`scripts/measure_app_startup.sh`) was mostly the snapshot mode's own idle
wait of 12 slices of 50 ms before it writes the PNG; the probe leaves that out. Medians, snapshot mode, M3 Pro,
macOS 27.0.1, load average about 10:

| Program | main | app_init | loop | first text |
| --- | --- | --- | --- | --- |
| Reference Objective-C++ editor (one window and text view) | 13 | 92 | 150 | 192 ms |
| PrimeScroll, master 06268bdc7 (0.79 MB executable), no file | 26 | 113 | 167 | 217 ms |
| PrimeScroll, installed build of 2026-10-27 (3.6 MB), no file | 32 | 121 | 175 | 220 ms |
| PrimeScroll master, a 50 KB `.prime` file as argument | 28 | 117 | 241 | 279 ms |
| PrimeScroll master, a 3-file session with a markdown preview | 22 | 109 | 559 | 254 ms (window shown mid-restore) |

On a warm launch with no document PrimeScroll is within the run-to-run noise (about 30 ms) of a plain AppKit editor:
both load about 1270 images (AppKit's closure on macOS 27 includes WebKit whether or not a program uses it; the editor
adds 6), and the program's own work before its event loop (registering every handler, building 120 menu items) is
under 2 ms of the 50 ms between `app_init` and `loop`; the rest is `ps_ui_window_create` (35 ms) and the text view.
The handler dispatch of 2cead3370 made no measurable difference against its loop version (179 against 213 ms, noise).
The causes of the slow launches, by what they cost when they happen:

1. **The first launch after every install (macOS, triggered by the workflow).** The kernel holds the exec of code with
   a cdhash it has not seen until syspolicyd finishes a Gatekeeper assessment: a notarization-ticket lookup over the
   network (150 ms on a warm connection; its request timeout is 3 s) and an XProtect scan. Ad-hoc signed, every build
   is new code: +320 to 500 ms for the executable and +170 to 250 ms more for a new `libps_ui_appkit.dylib`, the same
   for a 50 KB plain C program; the first launch of the master bundle above started `main` at 428 ms against 26 ms.
   With a slow or cold network each lookup can approach its 3 s timeout. TODO-5755.
   Since TODO-5755 `scripts/bundle_macos_app.sh` ends with `scripts/warm_macos_app.sh`, which launches the bundle once
   in snapshot mode (nothing on screen) and starts its script runner, so the assessment is paid at bundling: the next
   direct launch then starts `main` at 17 to 22 ms against 28 to 38 ms for the launches after it (a bundle that was
   not warmed: 383 ms). The verdict belongs to the file, not to the code hash alone: a moved bundle keeps it (20 ms),
   one re-signed in place almost does (45 ms), but a copy (`ditto`, as the install does) is assessed again (357 ms).
   So an install that copies the bundle runs `scripts/warm_macos_app.sh` on the installed copy after its last
   `codesign` (a warmed installed copy started at 22 ms). Ad-hoc signing the same bytes gives the same code hash, so a
   dylib whose bytes did not change keeps its hash without a check in the script.
2. **Load from build and test gates (machine, not PrimeScroll).** At load averages of 85 to 126 with 2.8 GB swapped,
   the reference editor took 0.47 to 1.06 s to its first text instead of 0.19 s, and a launch that had to page AppKit
   back in (3916 major faults) took 1.17 s to its first window. TODO-5757.
3. **Session restore before the first frame (PrimeScroll).** `restore_session` runs after `first.show()` but before
   `app.run`, and it also runs when Finder opens a file (a document arrives as an open-file event, not an argument).
   Each restored tab costs `ps_ui_window_add_tab` 80 to 130 ms (tab-group layout), the markdown and script toolbars
   about 30 to 60 ms per `ps_ui_window_set_toolbar`, and a markdown preview 95 to 250 ms (WebKit); with three files
   the event loop starts 390 to 450 ms later, and the requested file then opens in a new tab (+100 ms). Opening one
   `.prime` file at launch spends 61 ms building the script toolbar. TODO-5753 and TODO-5754.
   Since TODO-5753 the restore runs after the first frame (`onFirstFrame`), and a file Finder opens at launch goes
   into the first window with the session's tabs behind it. Same session, 7 snapshot runs each, load about 10:
   before, loop 621 and first text 273 ms (window shown mid-restore); after, loop 158, first text 211, the first
   frame event 239 and the restore done at 613 ms (the 455 ms of `add_tab`, `set_toolbar` and `set_preview_mode` now
   follow the frame); no session, after: loop 146, first text 187 ms.
   Since TODO-5754 the restored tabs open behind the selected one (`ps_ui_window_add_background_tab`): AppKit inserts
   them into the tab group (about 3 ms a tab, the first about 18 ms with the tab bar, against 20 to 30 ms and 65 to
   80 ms for `addTabbedWindow:ordered:`), does not lay them out, and builds a tab's toolbar and preview page only when
   it is first shown. The same session (interleaved, 3 rounds of 7 runs, load 40 to 77): before, loop 494 and first
   text 231 ms; after, loop 174, first text 212, the restore done at 297 ms, with 21 to 36 ms in all for the two
   background tabs and 5 ms for the preview (was 76 to 83). The first `NSToolbar` of a process costs 40 to 70 ms
   whatever it holds (measured empty, without images, in each toolbar style, and as a titlebar accessory view
   instead), so a `.prime` or markdown file opened at launch still spends about 50 ms in `set_toolbar` before the
   first frame (TODO-5821).
4. **AppKit's own launch in a real (regular) launch.** The unified log of the user's launches shows 275 to 440 ms
   between Launch Services check-in and `finishLaunching` returning, against 90 to 110 ms for all of `ps_ui_init` in
   snapshot mode: AppIntents registration that linkd rejects four times for an ad-hoc bundle, Writing Tools and
   Generative Models availability, a Spotlight donation for the recent document, and the AutoFill helper process
   (`SafariPlatformSupport.Helper`) that starts about 0.5 s in. TODO-5756.
   Since TODO-5756 `ps_ui_init` registers defaults that turn off window restoration (`ApplePersistenceIgnoreState`,
   `NSQuitAlwaysKeepsWindows`), the AutoFill heuristics and AutoFill menu items (`NSAutoFillHeuristicsEnabled`,
   `NSEnableTextInputContextBasedAutoFill`, `NSAutoFillSystemInsertMenuEnabled`) and the Writing Tools items AppKit
   adds to menus (`NSMenuDoesNotAutomaticallyInsertWritingToolsItems`); being registered defaults, a person's own
   setting still wins. Text views have no completion while typing, no inline predictions and no Writing Tools; spelling,
   undo, find and the program's menus are unchanged. The system's recent-document entry (`noteNewRecentDocumentURL`,
   the document controller and a Spotlight donation) of a document opened at launch waits for the first frame. In
   snapshot mode (no recent documents, no restoration) launches are unchanged within noise (first text 220 against
   210 ms, 3 interleaved rounds of 9); the effect is on a real launch, which a person measures (TODO-5603).
5. **Process start.** Launch Services to exec is about 20 ms, then amfid checks the ad-hoc executable and dylib on every
   launch, and dyld takes 20 to 35 ms; the executable's size (0.8 MB now, 3.6 MB installed) makes no measurable
   difference warm. Not PrimeScroll-specific.

The seven user launches still in the unified log (2026-11-07 to 11-09) checked in with Launch Services 61 to 207 ms
after the launch request, and the two with a full trace showed the window active about 0.5 and 0.9 s after it (load
near 100 at the time). The launch at login, by "reopen windows when logging back in", took 5.6 s to check in: the
login window starts the app stopped at a background role. None of the logged launches was a first launch after an
install, which is when cause 1 applies; the steps for measuring the real Launch Services path are in TODO-5603.

**Rules for the app path:**
- Ship precompiled bytecode only. The runner links `primec_embed_runtime_lib` (no compiler) into one executable, with
  the bytecode embedded in it or next to it; compiling at launch exists only for development.
- Keep the bytecode small and cheap to load (TODO-5536): size-oriented lowering for apps, and load and validate
  functions lazily instead of decoding the whole module up front.
- Link only Foundation and AppKit; no Swift runtime, no Metal or QuartzCore unless a feature needs them, no extra
  dylibs (the system frameworks come from the dyld shared cache).
- Show the window before doing anything else, then load the file. Put the text in the text storage in one batch;
  very large files show their first screen first.
- Use a plain `NSWindow`, not the `NSDocument` architecture (autosave, versions, file coordination and iCloud work at
  open time), and turn off window state restoration.
- Use TextKit 2 (`NSTextLayoutManager`), which lays out only the visible part of the text.
- Start services that spin up helper processes (continuous spell and grammar checking, text completion, data detectors)
  after the first frame, or leave them off by default.
- No child processes, no network or iCloud lookups and no open panel during launch.

**How it is measured.** `scripts/measure_startup.sh` times the phases of a launch with its probe (snapshot mode, or a
person's Launch Services launch with `--open`), the ps_ui calls before the event loop and after it until the program
settles, the peak resident set, and the distinct images dyld loads (about 1164 for PrimeScroll on macOS 27.0.1; the
earlier count of about 1270 included dyld's lines for deferred initializers). CTest
`PrimeStruct_native_ui_startup_guard` (TODO-5603) builds hello_window and PrimeScroll natively and fails when either
loads more than 4 images that a plain AppKit program (`tools/startup_probe/reference_app.m`) does not: today
`libps_ui_appkit.dylib` and the menu bar's MenuBarClientCore and sometimes CampoUIServices; one more framework link
adds about 3 (AVKit adds AVKit, AVKitCore and PIP). It counts images, not milliseconds, which move by 30 ms or more
between runs and with the machine's load. The rest of the harness (cold launches, window-list polling, a reference
editor with the same UI, installed editors for comparison) is TODO-5535.

## 2. Principles

- **Native first.** Each widget is the platform's own control, so it brings the platform's editing, selection,
  clipboard, undo, input methods, accessibility, spell checking and look. PrimeStruct code never draws a native
  widget.
- **Best effort per platform.** The API describes what an app wants (a menu bar, a text view, a save prompt). Each
  platform does as much of it as it can; where a platform has no equivalent, the call is a documented no-op or the
  nearest native form (for example, an iOS app has no menu bar, so its menus become a toolbar or are omitted). The
  coverage matrix in section 9 is the contract.
- **The program owns the loop.** `main` creates the app, then loops on `waitEvent()` until it decides to quit. State
  lives in ordinary locals, and events arrive as a sum type handled with `pick`; no callbacks are needed.
- **One C ABI, many hosts.** Every platform implements the same small C interface (section 4). Programs reach it
  through `[host]` functions; the runner binds them. The same program runs against the macOS backend, a headless test
  backend, and later Windows and Linux backends.
- **VM first.** Host calls run only on the VM today (`docs/spec/host-and-core-library.md`), so the first runner embeds
  the VM (`docs/Embedding.md`). Compiled backends calling the ABI directly come later (section 10).
- **Testable without a screen.** A headless backend implements the whole ABI in memory: it replays a scripted list of
  user actions and records every widget call, so the editor's behavior is golden-tested on Linux CI. Only the thin
  platform backends need manual checks on their OS.

## 3. Architecture

```
  app.prime ──[host] calls──▶ runner (embeds primec VM, binds ps_ui_* by name)
                                   │
                                   ▼
                        ps_ui C ABI (include/primec/ui/NativeUi.h)
                 ┌─────────────────┼──────────────────┬───────────────┐
                 ▼                 ▼                  ▼               ▼
         macOS: AppKit       headless (tests)    Windows: Win32   Linux: GTK 4
        (Objective-C++)     (C++, scripted)       (later)          (later)
```

- `include/primec/ui/NativeUi.h`: the C ABI, plain C types only (`uint64_t` handles, `int32_t`, UTF-8 `const char *`).
- `src/ui/headless/`: the headless backend, built everywhere.
- `src/ui/appkit/`: the macOS backend, built only when `APPLE`.
- `src/ui/NativeUiBindings.cpp`: registers every ABI function with a `primec::embed::ScriptEngine` under its `[host]`
  name, so any runner binds the whole surface in one call.
- `tools/primestruct_app/`: the runner. In development it compiles `app.prime` at launch with `primec_embed_lib`;
  a packaged app ships precompiled bytecode (`saveBytecode`) and links only `primec_embed_runtime_lib`.
- `stdlib/std/ui/native/`: the PrimeStruct surface: `[host]` declarations plus `App`, `Window`, `TextView`, `Menu`
  and the `AppEvent` sum.

## 4. The C ABI (version 0)

Handles are opaque non-zero `uint64_t`; `0` means none or failure. Text in and out is UTF-8; programs pass `string`
or `String` and receive `String` ([Strings, Text and Slices](spec/strings-and-views.md)). Every function
is callable only on the thread that called `ps_ui_init`, the main thread on macOS.

| Group | Functions |
| --- | --- |
| App | `ps_ui_init(appName)`, `ps_ui_quit()`, `ps_ui_platform() -> i32` (macOS, Windows, Linux, iOS, headless) |
| Events | `ps_ui_wait_event() -> i32 kind`, `ps_ui_event_window() -> u64`, `ps_ui_event_widget() -> u64`, `ps_ui_event_command() -> i32` |
| Window | `ps_ui_window_create(title, width, height) -> u64`, `_set_title`, `_set_content(window, widget)`, `_set_edited(window, bool)`, `_set_autosave_name(window, name)` (the window's frame is remembered between runs under that name and restored at once; `NSWindow` frame autosave), `_show`, `_close` |
| Text view | `ps_ui_text_view_create() -> u64`, `_get_text -> String`, `_set_text`, `_set_monospace(bool)`, `_set_wrapping(bool)` (wrap long lines at the view's width, the default, or scroll horizontally), `_set_line_numbers(bool)` (a bar of line numbers to the left of the text, one per paragraph, wrapped lines carry none; presentation only), `_is_modified -> bool`, `_clear_modified` |
| Menus | `ps_ui_menu_create(title) -> u64`, `_add_item(menu, title, shortcut, commandId)`, `_add_separator`, `_add_standard(menu, standardId)`, `_set_checked(menu, commandId, bool)` (a check mark on the items with that command id), `_add_submenu(menu, title, submenu)` (an item that opens another menu), `_clear(menu)` (remove every item, to rebuild a dynamic submenu), `_add_recent_files(menu, firstCommandId, maxItems) -> i32` (one item per recent file, titled with its file name and the folder name when two share a name), `ps_ui_menu_bar_add(menu)`, `ps_ui_menu_bar_add_role(menu, role)` (application menu replaces the platform default, Window lists the windows, Help). Standard items 1-16 (Undo ... Help, including Hide, Hide Others, Minimize, Zoom, Bring All to Front, Enter Full Screen) take their titles and shortcuts from `include/primec/ui/NativeUiStandardItems.h`, shared with the headless backend so `headless::menuBarOutline()` shows what the platform menus hold |
| Tabs and documents | `ps_ui_window_add_tab(host, tab)` (native tab group, the tab selected; AppKit inserts it into the host's `NSWindowTabGroup` right after the host), `ps_ui_window_add_background_tab(host, tab)` (the same place, neither shown nor selected: a backend lays it out and builds its toolbar and preview page when it is first selected; for a restored session), `ps_ui_active_window()`, `ps_ui_window_tab_count/tab_at(window, index)` (tab-bar order) and `ps_ui_window_select(window)` (what Command-1 to Command-9 use), `ps_ui_window_content(window)`, `ps_ui_window_count()`, `ps_ui_window_at(index)`, and a per-window document path (`ps_ui_window_set_path/path/has_path`, stored by the backend so programs keep per-document state without handling strings). Standard items 17-21: Show Tab Bar, Merge All Windows, Move Tab to New Window, Show Next/Previous Tab. Menu commands carry no window: programs act on the active window |
| Styled text | `ps_ui_text_view_clear_styles(view) -> bool`, `ps_ui_text_view_add_style(view, startByte, endByte, rgb, flags) -> bool` (UTF-8 byte ranges on code point boundaries, `rgb` is 0xRRGGBB, flags `PS_UI_STYLE_BOLD`/`PS_UI_STYLE_ITALIC`; later runs win; presentation only: no modified flag, no text-changed event, no undo entry; replacing the text drops the runs, typing keeps them so programs restyle on text-changed). AppKit applies them as text storage attributes with undo registration off), `ps_ui_text_view_highlight(view, path) -> bool` (restyles the text with the syntax highlighting of the file name's extension: PrimeStruct, C/C++/Objective-C, Python, JSON, Markdown, JavaScript/TypeScript, Rust, Go, Java, Kotlin, Swift, shell, SQL, CMake, with keyword, type, string, comment, number, function-name, annotation and variable colours; the lexers are C++ in `src/ui/SyntaxHighlight.cpp` because programs cannot scan strings yet) |
| Text files | `ps_ui_text_view_load_file(view, path) -> bool`, `ps_ui_text_view_save_file(view, path) -> bool`, `ps_ui_last_error() -> String` (the backend reads and writes UTF-8 files itself, so no program-side text is built and a large file never passes through the program) |
| Recent files | `ps_ui_recent_add(path)`, `ps_ui_recent_path(index) -> String` (most recent first), `ps_ui_recent_clear()` (the app's own defaults list `ps.recent.files` on macOS, kept between runs and across app updates; `PRIMESTRUCT_UI_RECENT=/a:/b` and every snapshot run use an in-memory list instead; the headless backend keeps the list in memory, preset with `headless::pushRecentFile`, at most 10 entries) |
| Preferences | `ps_ui_pref_get_bool(key, fallback) -> bool`, `ps_ui_pref_set_bool(key, value) -> bool` (and `_get_int/_set_int`, plus lists of texts: `ps_ui_pref_list_clear/add/count/get(key, ...)`; booleans the app keeps between runs: `NSUserDefaults` on macOS under `ps.pref.<key>`; `PRIMESTRUCT_UI_PREFS=key=1,other=0` and every snapshot run answer from the environment instead and never touch the user's defaults; the headless backend keeps them in memory, preset with `headless::setBoolPref`) |
| Controls and forms | `ps_ui_label_create(text)`, `_set_text`; `ps_ui_checkbox_create(title, checked)`, `_set_checked`, `_checked`; `ps_ui_popup_create()`, `_add_item(popup, title)`, `_select(popup, index)`, `_selected(popup) -> i32` (the first item added is chosen); `ps_ui_form_create()`, `_add_heading(form, title)`, `_add_row(form, label, control)` (the labels line up in one column, the controls in the next), `_add_control(form, control)` (a control alone in the controls' column, such as a check box); `ps_ui_tabs_create()`, `_add(tabs, title, content)`, `_select(tabs, index)` (the first page added is shown). A control is held by one form or tabs, or is a window's content (`ps_ui_window_set_content` takes any of them). The user's click on a check box or choice in a pop-up is `PS_UI_EVENT_CONTROL_CHANGED` with the control as the event's widget and its window; the program's own changes make no event. `ps_ui_window_set_style(window, PS_UI_WINDOW_SETTINGS)` makes a window that is never a tab and fits its content. `ps_ui_text_view_set_font_size(view, points)` (6 to 96). The headless backend keeps all of it in memory, answers scripted user actions (`headless::pushToggleCheckBox`, `pushSelectPopupItem`; `check:H` and `popup:H:I` in a scenario) and shows a control tree as text (`headless::controlOutline(root)`); the AppKit backend answers the controls with TODO-5593 |
| Markdown preview | `ps_ui_window_set_preview(window, bool)` (the window shows its text view and, beside it, the markdown of that text rendered as a page; the backend builds the preview only when it is turned on; false for a bad window or one with no text view; on while on is a no-op), `ps_ui_window_has_preview(window) -> bool`, `ps_ui_window_refresh_preview(window) -> bool` (render again from the current text; false without a preview; a backend may coalesce refreshes), `ps_ui_menu_follow_preview(menu, commandId) -> bool` (the items' check mark shows whether the active window has a preview, so it stays right when the user clicks another tab). The headless backend records the page (`headless::previewPage(window)`) and how often it was built (`WindowState::previewRenders`) |
| Toolbar and markdown edits | `ps_ui_toolbar_create/add_button/add_segments/add_menu/add_separator` (every item is a command: a click is `PS_UI_EVENT_COMMAND`; segments are `Editor|Split|Preview` with consecutive command ids), `ps_ui_window_set_toolbar(window, toolbar)` (0 hides), `ps_ui_window_toolbar_select(window, commandId)`, `ps_ui_window_set_preview_mode/preview_mode` (`PS_UI_PREVIEW_TEXT/SPLIT/ONLY`; `set_preview` is TEXT or SPLIT), `ps_ui_text_view_markdown_edit(view, PS_UI_MARKDOWN_*, a, b)` (one change through `primec::ui::markdownEdit`, modified, `PS_UI_EVENT_TEXT_CHANGED`) and `ps_ui_text_view_set_markdown_editing` (Tab moves between table cells). The headless backend keeps `headless::toolbarOutline(window)`, the selection (`headless::pushSelectText`, `selection`, `select:H:S:E` in scenarios) and applies the edits; AppKit builds an `NSToolbar` per window (SF Symbols, an `NSToolbarItemGroup` for the segments, `NSMenuToolbarItem` for the menus), hides the text scroll view in the preview-only mode, applies an edit through `shouldChangeText`/`didChangeText` (one undo) and tells the program about undo and redo with a text-changed event |
| Pointer and keys (feature 1) | `ps_ui_supports(PS_UI_FEATURE_INPUT_EVENTS) -> bool`; `ps_ui_widget_watch(widget, mask) -> bool` asks for the `PS_UI_WATCH_*` events of a control or text view (1 hover, 2 press, 4 release, 8 key down, 16 key up: the bits of a `/std/web` control's events; the mask replaces the earlier one, 0 stops them); the kinds `pointer_enter` (7), `pointer_leave` (8), `mouse_down` (9), `mouse_up` (10), `key_down` (11), `key_up` (12) arrive for watched widgets only, with the widget and its window; `ps_ui_event_button() -> i32` (-1 none, 0 primary, 1 secondary, 2 middle), `_x`, `_y` (points from the widget's top left corner; a text view's visible part), `_modifiers` (1 shift, 2 control, 4 alt, 8 meta), `_click_count` (2 for the second press of a double click), `_key` (a code point, or a named key below 0 as `/std/ui/events` `Key`; function keys are not reported), `_repeat -> bool`. A release goes to the widget the press went to. A text view keeps the keys it uses (typing, editing and moving keys, its shortcuts) and reports only the others (Escape, Command combinations it has no binding for), and a key up only after it reported the key down. The headless backend replays `headless::pushPointerEnter/Leave`, `pushMouseDown/Up`, `pushKeyDown/Up` (`enter:H:X:Y:M`, `down:H:B:X:Y:M:C`, `keydown:H:K:M:R` ... in scenarios, `--enter`, `--down`, `--keydown` ... in `native_ui_headless_runner`), delivers an action only when its event is watched, and models a text view's keys: a character, Enter or Tab without Control and Command is typed, Escape and Command keys are reported, the rest kept |
| First frame (feature 2) | `ps_ui_supports(PS_UI_FEATURE_FIRST_FRAME) -> bool`; `ps_ui_watch_first_frame() -> bool` asks for `first_frame` (13): it arrives once, after the first window the program showed has been drawn (at the next wait when that has happened), with that window as `ps_ui_event_window()`; events that came before the drawing (a file the system asks the app to open at launch) are delivered first. False when it came already or the backend lacks the feature. AppKit observes the main run loop, after Core Animation's commit, as it goes to sleep or leaves (the first display is often on the way out of the first pump) and delivers once the window's views no longer need display, or after 0.5 s from a timer (a window that is not drawn soon, as in a launch into the background). The headless backend delivers it at the first wait after the first `ps_ui_window_show`, ahead of the scripted actions, or where the scenario places it (`headless::pushFirstFrame`, `frame`) |
| Dialogs | `ps_ui_open_panel(title) -> String`, `ps_ui_save_panel(title, suggestedName) -> String` (empty means cancelled), `ps_ui_panel_chosen() -> bool` (whether the last panel returned a path), `ps_ui_alert(message, detail, buttons) -> i32` |

- **Events.** `ps_ui_wait_event` blocks in the platform's run loop until something the program should handle happens,
  then returns its kind; the accessors read the fields of that event until the next wait. Kinds: `command` (a menu
  item or shortcut, with its command id), `window_close_requested`, `text_changed`, `quit_requested`,
  `control_changed`, `open_file`, and the pointer and key kinds of the widgets the program watches. Everything else
  (typing, selection, scrolling, copy and paste, undo) is handled by the native widget and never reaches the
  program.
- **Shortcuts** are written portably (`"cmd+s"`, `"cmd+shift+s"`); `cmd` means Command on macOS and Ctrl elsewhere.
- **Standard items** (`undo`, `redo`, `cut`, `copy`, `paste`, `select_all`, `find`, `quit`, `about`) map to the
  platform's own actions (the AppKit responder chain), so they work in the focused native widget without program code.
- **As built.** `include/primec/ui/NativeUi.h` is the authority for exact signatures: flags are C `bool`, a text result
  is a `const char *` valid until the next text-returning call, `ps_ui_alert` takes button titles separated by `\n` and
  returns the pressed index, and `ps_ui_init` returns false when the app is already running. The headless backend
  (`primec/ui/NativeUiHeadless.h`) ends a scripted run with `quit_requested` once its action list is empty.
  `primec/ui/NativeUiBindings.h` binds all 130 functions into a `ScriptEngine` or `Script` under their ABI names. The
  three text-file functions (TODO-5546) let the editor open and save before owned `String` exists (TODO-5530).
- **Versioning.** `ps_ui_abi_version() -> i32` returns 0 for this table. Additions append functions; a change to an
  existing signature bumps the version. An addition a version-0 program could notice comes behind a feature that
  `ps_ui_supports(feature) -> bool` reports: the pointer and key events (`PS_UI_FEATURE_INPUT_EVENTS`, TODO-5725)
  reach a program only for the widgets it watches, so a program that never calls `ps_ui_widget_watch` gets exactly
  the version-0 events, and every version-0 function is unchanged; the first frame (`PS_UI_FEATURE_FIRST_FRAME`,
  TODO-5753) reaches only a program that called `ps_ui_watch_first_frame`. A backend without the feature answers false and
  fails every watch.

## 5. The PrimeStruct surface

```prime
import /std/ui/native/*
import /std/file/*

[effects(io_out file_read file_write) return<int>]
main() {
  [App] app{App.start("Editor"utf8)}
  [Window] window{app.window("Untitled"utf8, 720i32, 480i32)}
  [TextView] text{app.textView()}
  window.setContent(text)
  [Menu] file{app.menu("File"utf8)}
  file.item("Save"utf8, "cmd+s"utf8, SaveCommand)
  window.show()
  [bool mut] running{true}
  while(running) {
    pick(app.waitEvent()) {
      command(id) { if(id == SaveCommand) { save(window, text) } }
      windowCloseRequested(w) { running = false }
      textChanged(w) { window.setEdited(true) }
      quitRequested { running = false }
    }
  }
  return(0i32)
}
```

The names follow `docs/CodeExamples.md`. The surface as built (TODO-5529) is in `docs/spec/stdlib-reference.md`
("Native UI"): `start_app(name)` replaces `App.start(name)` (a type-qualified static call does not resolve through
an import yet), and `AppEvent` lists `quitRequested` first because a default sum value needs a unit first variant.

**Handlers (TODO-5709).** Controls also take handler closures, so a program need not write the loop above
(docs/WebAppsPlan.md, principle 7). An `AppHandlers<State>` (stdlib/std/ui/native/handlers.prime) holds the app's
state and a table of handlers registered once, at setup; `app.run(handlers)` loops on `ps_ui_wait_event` and calls
the handler of each event until the app quits. Each handler is a `Function<void, Reference<State>, Event>`: its own
`mut` captures keep what only it needs, the shared state comes in as the parameter, and the event is a
`/std/ui/events` struct, the same types a `/std/web` control takes. The table maps the version-0 events back to
their handlers; the pointer and key handlers (TODO-5725) use the events of `PS_UI_FEATURE_INPUT_EVENTS`, and
registering one watches its widget (`ps_ui_widget_watch` with the events that have handlers), so the platform tracks
only widgets with such handlers.

| Event (ABI v0) | Registration | The handler gets |
| --- | --- | --- |
| `command` | `handlers.command(handler)` returns a `Command`, added with `menu.commandItem(title, shortcut, command)`, `toolbar.commandButton(title, symbol, command)` (`menu.setCommandChecked(command, bool)`, `menu.followPreviewCommand(command)`, `window.selectToolbarCommand(command)`, `window.runScriptCommand(name, text, command)` for the end of a script run) | a `PointerEvent` of the primary button, no position, `clickCount` 1 |
| `command`, one of a row | `handlers.commandRange(count, handler)` returns a `CommandRange` (`first`, `count`) of consecutive ids: `toolbar.commandSegments(titles, range)`, `menu.addRecentFileCommands(range)` (one recent file per command), or one of them, `range.at(offset)`, as a `Command` | the offset of the chosen command in the range, an `i32` |
| `control_changed` of a `Button` | `handlers.onClick(button, handler)` | the same `PointerEvent` |
| `control_changed` of a `CheckBox`, `Popup`, `ItemList` | `onCheckBoxChange`, `onPopupChange`, `onListChange(control, handler)` | a `ChangeEvent`: `checked`, or `index` (the first selected row of a list, -1 for none), read from the control |
| `control_changed` of a control made after setup | `onClickOf`, `onCheckBoxChangeOf`, `onPopupChangeOf`, `onListChangeOf(find, handler)`: `find`, a `Function<u64, Reference<State>>`, gives the control's handle from the state when an event comes (0 while there is none) | as above |
| `window_close_requested` | `onWindowClose(handler)`, for every window | the `Window`; without a handler the window closes |
| `text_changed` | `onTextChange(handler)` | the `Window` |
| `open_file` | `onOpenFile(handler)` | the path, a `String` |
| `quit_requested` | `onQuit(handler)`, a `Function<bool, Reference<State>>` | nothing; `true` lets `run` return |
| every event | `afterEvent(handler)`, a `Function<bool, Reference<State>>`, after the event's handler (or none) | nothing; `false` ends `run` |
| `pointer_enter` (feature 1) | `onMouseOver(widget, handler)`, any control or text view as a `Widget` (`button.widget()`, `text.widget()`) | a `PointerEvent`: no button, the position, the modifiers, `clickCount` 0 |
| `mouse_down`, `mouse_up` | `onMouseDown`, `onMouseUp(widget, handler)` | a `PointerEvent`: the button, the position, the modifiers, the click count |
| `key_down`, `key_up` | `onKeyDown`, `onKeyUp(widget, handler)`, while the widget has focus | a `KeyEvent`: the key, the modifiers, `isRepeat`; a text view's handlers see only the keys it does not keep |
| `first_frame` (feature 2) | `onFirstFrame(handler)`, a `Function<void, Reference<State>, Window>`; registering one makes `run` ask for the event, and a backend without the feature has the handler called before the first wait | the first window the app showed, drawn; once |

- **Ids stay inside.** `command` and `commandRange` hand out ids from `first_command_id()` (1000000) up, in the order
  of registration, above the ids programs pick for `waitEvent`; app code holds the `Command` or `CommandRange`. One
  `Command` in a menu and a toolbar calls one closure, so its `mut` captures are shared between the two. The controls
  that take consecutive ids (toolbar segments, recent files) take a range, whose one handler gets the offset; so does a
  family of items that differ by a number (Show Tab 1 to 9). A command id without a handler (a hand-picked one) is
  dropped by `run`, as is a control change of a control without one.
- **Controls made later.** Handlers are registered once, at setup (principle 7), but a window built on first use
  (PrimeScroll's settings window) has no controls yet then. The `...Of` registrations take a `find` closure instead of
  the control: `run` asks it for the control's handle when a control changes, so the state holds the handles and the
  handlers stay registered once.
- **After every event.** `afterEvent(handler)` runs after each event, whether a handler took it or not. It is where an
  app brings what its handlers changed on screen once (PrimeScroll: the toolbars follow the windows, the session is
  saved after a command, Open Recent is rebuilt), so each handler does its own work and sets a flag in the state.
- **When `run` returns.** On a request to quit that no quit handler refuses, and after a close request (handled or
  not) when no window is open any more (`ps_ui_window_count() == 0`). With an `afterEvent` handler, the app decides:
  `run` returns when it gives false (and still on an unrefused quit request), and the window count is not asked.
- **`waitEvent` stays.** `run` reads the same events, so a program keeps its own loop or uses handlers; one program
  uses one of the two. PrimeScroll moved to handlers (TODO-5710, below).
- **As built, and why so.** The table and the state live in a generic struct, not in `App`, `Button` or `Menu`: those
  are `u64` handles with nowhere to keep closures, and there is no global state, so registration goes through the one
  object that knows `State`; this is the shape of `WebApp<State>` (`state` field, controls registered on it). The
  per-control `onChange` names and `commandItem`/`commandButton` are separate methods, not overloads of `onChange`,
  `item` and `button`: same-arity overloads of struct methods do not resolve yet (TODO-5735). `run` reads the ABI
  event kinds itself instead of `pick`ing an `AppEvent` (TODO-5736). The handlers of one type share one vector and
  `run` calls each vector from one place. The arm64 native backend makes struct methods and closure calls real calls
  (TODO-5740); a target with the scalar ABI (x86_64 native, TODO-5752) inlines them and a `Function` call with every
  closure of its type at each call site, so this keeps an app's handlers compiled once there.

```prime
[AppHandlers<Greeter> mut] handlers{AppHandlers<Greeter>{[state] Greeter{[button] hello}}}
[mut] sayHello{[mut said{0i32}]([Reference<Greeter>] greeter, [PointerEvent] event) {
  said = said + 1i32
  greeter.say(greeting(said))
}}
handlers.onClick(hello, sayHello)
window.show()
app.run(handlers)
```

`examples/native_ui/hello_button.prime` is the whole program; `tests/fixtures/ui/handlers.prime` uses every row of the
table. `PrimeStruct_cpp_native_ui` runs both on the VM against the headless backend and compiled with `optexe`, with
the same call logs.

## 6. The text editor example

`examples/apps/text_editor/main.prime` is a single-window plain-text editor:

- **File menu:** New, Open… (⌘O), Save (⌘S), Save As… (⇧⌘S), Close (⌘W). **Edit menu:** the standard items.
- The window title is the file name (or `Untitled`), and the window shows the platform's unsaved-changes marker once
  the text changes.
- Closing or quitting with unsaved changes asks Save / Don't Save / Cancel.
- Open and Save read and write UTF-8 files; a read or write error is shown in an alert instead of quitting.
- The text view uses the system monospace font; everything else about editing comes from the native control.
- **Line numbers:** View > Show Line Numbers (⇧⌘L) shows or hides a bar of line numbers to the left of the text in every
  tab (new tabs follow the current setting; the item carries a check mark). The choice is the `lineNumbers` preference (stored with
  `ps_ui_pref_set_bool`, off until chosen), also a check box of the settings window (section 15). The AppKit bar is an `NSRulerView` that numbers the TextKit 2 layout fragments of the visible
  part only.
- **Wrapping:** View > Wrap Lines (check-marked, on by default) wraps long lines at the window width or lets them
  run to the right with a horizontal scroll bar, in every tab; new tabs follow the current setting.
- **Session:** the first window is named for frame autosave, so its position and size come back. The files the
  tabs hold (in tab-bar order) and the active tab are kept as the list `session.files` and the integer
  `session.active`, saved after every command and before the tabs close on quit; the next launch reopens them one
  tab each and selects the active one (a file that is gone is skipped, a file named on the command line replaces the
  restore, and closing the last tab leaves the session as it was, since the app ends then like it does on quit; closing
  one of several tabs drops it from the session). The restore waits for the first frame (`onFirstFrame`, TODO-5753):
  the empty first window is drawn first, then the tabs come back. A file the system asks the app to open at launch (a
  double click in the Finder) arrives before the first frame and opens in the first window; the session's tabs then
  open behind it, without that file twice, and it stays selected. Until the restore has run the session is not saved
  over.
- **Open Recent:** File > Open Recent lists up to ten recently used files (opened or saved by the editor, or given on
  the command line), most recent first, with a Clear Menu item. The submenu is rebuilt after every open, save and
  clear with `Menu.clear()` and `Menu.addRecentFileCommands(range)`; choosing a file opens it like Open does.
- **Dispatch (TODO-5710).** The editor runs on handlers (section 5): its state is an `Editor` (the app, the menus and
  toolbars that follow the documents, the settings window, the commands as `EditorCommands`), every menu item and
  toolbar button is a `handlers.command` or one of a `handlers.commandRange` (scroll modes, view modes, format
  operations, headings, tables, alignments, Show Tab 1 to 9, recent files), the settings window's controls, built on
  first use, are found through `onCheckBoxChangeOf` and its siblings, and one `afterEvent` handler does what follows
  every event (the preferences shown again, Open Recent rebuilt, the toolbars synced, the session saved after a
  command, the tabs closed after Quit, the app ended with the last document). The ids come from `AppHandlers`
  (1000000 up) instead of the editor's own numbers; the call logs are otherwise those of the `waitEvent` loop, without
  the queries it made for nothing (the active window before each wait, the settings-window check before an event that
  did not use it).

## 7. Runtime gaps and how this plan closes them

| Gap | Resolution |
| --- | --- |
| Programs cannot build, slice or own text; run-time strings are never freed during a run | Owned `String`, the `string` text view and slices ([Strings, Text and Slices](spec/strings-and-views.md), TODO-5537 to TODO-5542); whole-file text read and write as `String` (TODO-5530). Text from `getText()` is then a `String` freed when dropped, so a long editing session does not grow memory. |
| Host calls are VM-only | The runner embeds the VM; compiled backends follow in TODO-5534. |
| Each `Script::call` is a fresh run | The program owns the loop inside one long `main` run, so its state persists. |
| No callbacks | Events are values (`AppEvent` sum) returned by `waitEvent()`. |
| Native file I/O bugs (TODO-5514) | Not on the VM path; fixed separately. |

## 8. Packaging

- CMake: `PRIMESTRUCT_BUILD_NATIVE_UI` (default ON on `APPLE`) builds the AppKit backend and `primestruct_app`.
- `scripts/bundle_macos_app.sh <app.prime> <Name>` makes `Name.app`: the runner, compiled bytecode, the stdlib if
  needed, an `Info.plist`, and an ad-hoc signature for local runs. Notarization is out of scope.

## 9. Platform coverage (contract)

| Feature | macOS (AppKit) | Headless | Windows (Win32) | Linux (GTK 4) | iOS (UIKit) |
| --- | --- | --- | --- | --- | --- |
| Window, title, edited marker | full | full | title only (marker drawn as `*` in the title) | title only (`*`) | one window; title in the navigation bar |
| Multi-line text view | `NSTextView` | in-memory | RichEdit | `GtkTextView` | `UITextView` |
| Menu bar + shortcuts | full | full | window menu + accelerators | `GtkPopoverMenuBar` + accelerators | none (commands via toolbar) |
| Standard edit items | responder chain | recorded | `WM_*` messages | GTK actions | system edit menu |
| Open/Save panels | `NSOpenPanel`/`NSSavePanel` | scripted | common dialogs | `GtkFileDialog` | document picker |
| Alerts | `NSAlert` | scripted | `MessageBox` | `GtkAlertDialog` | `UIAlertController` |
| Pointer hover, press, release on a watched widget (feature 1) | `NSTrackingArea` added by the watch; presses read off the pumped events | scripted (`enter`, `down`, `up`) | `WM_MOUSEMOVE`/`TrackMouseEvent`, `WM_*BUTTON*` | `GtkEventControllerMotion`, `GtkGestureClick` | touches (no hover) |
| Keys of the focused watched widget (feature 1) | key events read off the pump; a text view's unused commands through its delegate | scripted (`keydown`, `keyup`), a modelled text view | `WM_KEYDOWN`/`WM_KEYUP` | `GtkEventControllerKey` | hardware keyboard `pressesBegan` |

Planned columns are design intent; only macOS and headless are in the first slices.

## 10. Later

- **Compiled programs.** Let the C++ emitter (`--emit=exe`/`optexe`) call the ABI directly through `extern "C"`, then
  the native backend through a platform shim; programs stop needing the VM runner (TODO-5534).
- **More widgets:** label, button, single-line field, checkbox, list/table, split view, toolbar, status bar, layout
  stacks; each added to the ABI and the coverage matrix together.
- **Second and third platforms** (TODO-5533).

**Goal (2026-10-21): PrimeScroll compiled to native code through the C++ emitter, implemented properly.**
`primec --emit=cpp` and `--emit=native` currently reject every `[host]` call (verified on hello_window and the editor),
and the editor depends on host-returned text (panel results, error text, per-tab paths), which the compiled backends
cannot hold until the string work exists. So the order is: bytes (TODO-5537), real slices (TODO-5538), owned `String`
(TODO-5539), the `string` view (TODO-5540), host functions with `String`/`string` (TODO-5541), compiled-backend parity
(TODO-5542), then `[host]` lowering in the C++ emitter (TODO-5534), a native app build path (TODO-5559), the editor
compiling and matching its VM scenarios (TODO-5560), and the native bundle and install (TODO-5561). No VM-only
shortcut is taken for text on the way.

Status: the build path exists. `scripts/build_native_app.sh app.prime out` compiles a program with
`primec --emit=optexe` and links the AppKit backend (`--backend headless` links the scripted test backend), so the
result is a native executable with no VM or compiler in it; `PrimeStruct_cpp_native_ui` replays hello_window and the
editor's scenarios on the VM and as compiled programs with identical call logs, and
`PrimeStruct_native_ui_compiled_app` snapshots the compiled AppKit builds. Every scenario of the editor gate
(section 13: the `primestruct.ui.editor` and `primestruct.ui.editor_acceptance` suites, highlight samples included)
also runs on the compiled editor linked with the headless backend (`tests/unit/ui/CompiledEditorMirror.h`), and the
call log, exit code and files must equal the VM run's.

**Direct native build (primec's own backend, macOS/arm64).** `primec --emit=native app.prime -o out --link-arg
path/libps_ui_appkit.dylib` writes the Mach-O itself, with no C++ compiler and no linker: the AppKit backend is built as
`libps_ui_appkit.dylib` (target `primec_ui_appkit_dylib`), the image loads it (`LC_LOAD_DYLIB`), binds the program's
`[host]` functions to it by flat lookup at launch, and declares the SDK it was built with (`LC_BUILD_VERSION`,
`LC_UUID`) so AppKit draws the current look. `scripts/build_native_app.sh --direct` copies the dylib next to the output
and loads it through `@executable_path`; `PrimeStruct_native_ui_compiled_app` builds hello_window this way and checks
that it draws a window of the same size as the C++ build. The editor compiles this way too and opens a highlighted
Unicode file in snapshot mode. Every editor scenario (section 13) also runs on the editor built this way, linked with
`libps_ui_headless.dylib` (the headless backend and the call-log fixture), and must leave the VM's call log, exit code
and files. `scripts/bundle_macos_app.sh --native` builds PrimeScroll from it: `Contents/MacOS` holds the native
executable (about 0.5 MB) and the AppKit dylib, both ad-hoc signed, and `codesign --verify --deep --strict` passes.

## 11. TODO slices

| TODO | Slice | Verifiable here |
| --- | --- | --- |
| TODO-5528 (done) | C ABI header, headless backend, engine bindings, unit tests | yes |
| TODO-5529 | `/std/ui/native` surface and `AppEvent`, VM compile-run tests on the headless backend | yes |
| TODO-5530 | Read and write whole files as `String` (after the string slices) | yes |
| TODO-5531 | AppKit backend, `primestruct_app` runner, CMake and `.app` bundling | builds and runs on a Mac only |
| TODO-5532 | Text editor example with headless golden scenarios | yes (macOS run by hand) |
| TODO-5533 | Windows and Linux backends | per platform |
| TODO-5534 | Compiled backends call the ABI | yes for the C++ emitter |
| TODO-5535 | Launch-time harness and the reference Objective-C++ editor | on a Mac only |
| TODO-5536 | Small, lazily loaded bytecode for apps | yes |

## 13. Editor acceptance gate

"The text editor is acceptable" means CTest `PrimeStruct_native_ui_editor_acceptance` passes (it runs the doctest
suites `primestruct.ui.editor` and `primestruct.ui.editor_acceptance` of `PrimeStruct_native_ui_tests` against the
headless backend, so it is part of `./scripts/compile.sh --release` on every platform), plus the Apple-only
`PrimeStruct_native_ui_appkit_smoke`. Requirement to test:

| # | Requirement | Where it is checked |
| --- | --- | --- |
| 1 | Syntax highlighting for PrimeStruct, C/C++, Python, JSON, Markdown | `test_native_ui_editor_highlight.cpp`: one sample per language (`tests/fixtures/ui/highlight/`) opened in the editor, expected runs by slice, run order and boundaries, extension mapping, unknown extensions unstyled, restyle after typing; `test_native_ui_headless.cpp` for the style ABI itself (validation, replace vs typing) |
| 2 | Unicode: load, display, save | `test_native_ui_editor_acceptance.cpp`: byte-exact round trips of `tests/fixtures/ui/unicode_sample.txt` (accents, CJK, emoji, combining marks, RTL), CRLF, no final newline, empty file, typed text, invalid UTF-8 refused with the error shown; grapheme-cluster cursor and selection come from `NSTextView` (section 12); display in the smoke picture below |
| 3 | Saving and loading | `test_native_ui_editor.cpp` (type then save, save as, cancelled save and open panels, open, failing open and save with the error in an alert, close/new with each prompt answer) and `test_native_ui_editor_acceptance.cpp` (open, edit, save, reopen equality; save-as copy) |
| 4 | Standard menus and shortcuts | `acceptance: the menu bar has the standard macOS structure and shortcuts`: the exact `headless::menuBarOutline()` of App, File, Edit, View, Window and Help, and the command ids |
| 5 | Screenshots | `PrimeStruct_native_ui_appkit_smoke` bundles the editor and checks two drawn PNGs, the highlighted Unicode file and the File menu open (hooks in section 12); the menu structure itself is requirement 4 |

Highlighted languages and their extensions (one asserted fixture each in `test_native_ui_editor_highlight.cpp`):
PrimeStruct `.prime .ps`; C/C++/Objective-C `.c .h .cc .cpp .cxx .hpp .hh .hxx .m .mm`; Python `.py .pyw`; JSON `.json`;
Markdown `.md .markdown`; JavaScript/TypeScript `.js .mjs .cjs .jsx .ts .tsx .mts .cts`; Rust `.rs`; Go `.go`; Java
`.java`; Kotlin `.kt .kts`; Swift `.swift`; shell `.sh .bash .zsh .ksh`; SQL `.sql`; CMake `.cmake`, `CMakeLists.txt`;
HTML/XML `.html .htm .xhtml .xml .svg .plist`; CSS `.css`; YAML `.yml .yaml`; TOML `.toml`; INI `.ini .cfg .conf`.

The editor is called PrimeScroll. It keeps one native tab per document (`test_native_ui_editor.cpp`: new tab, independent
saves, open into a reused or new tab, closing and quitting with per-tab prompts; `test_native_ui_editor_highlight.cpp`:
each tab styled for its own file); the AppKit smoke test also runs it with two extra tabs.

Hand checks on a Mac (typing, shortcuts, real panels, cursor stepping over grapheme clusters) stay in TODO-5545.

## 12. macOS backend as built (TODO-5531)

- `src/ui/appkit/NativeUiAppKit.mm` implements the whole ABI with a plain `NSWindow` (no `NSDocument`, no state
  restoration), `NSScrollView` + `NSTextView` (TextKit 2 default stack), `NSMenu` with key equivalents and
  responder-chain standard items, `NSOpenPanel`/`NSSavePanel` and `NSAlert`. `ps_ui_wait_event` pumps the AppKit
  queue itself, so the program owns the loop; the application delegate turns window close, Cmd+Q and edits into events
  and never terminates the process on its own.
- `tools/primestruct_app` runs a program: `primestruct_app app.prime` compiles at launch (development),
  `primestruct_app app.psir` runs bytecode, and `primestruct_app_runtime` (no compiler linked, about 1 MB) is the
  bundle runner. `scripts/bundle_macos_app.sh app.prime Name` makes `Name.app` with the runtime runner, the bytecode in
  `Contents/Resources/app.psir`, an `Info.plist` and an ad-hoc signature. `PRIMESTRUCT_BUILD_NATIVE_UI` (default ON
  on Apple, OFF elsewhere) controls the Objective-C++ targets.
- **Snapshots without Screen Recording permission.** With `PRIMESTRUCT_UI_SNAPSHOT=/path/out.png`, the first shown
  window is rendered by its own view hierarchy (`cacheDisplayInRect`) into a PNG once its layout has settled, and the
  program is then told to quit. The app cannot become active (`NSApplicationActivationPolicyProhibited`; a run that
  does exits with status 3), and nothing appears on screen (TODO-5820): every window is transparent from its creation,
  casts no shadow, ignores the mouse and, once ordered in, is parked past the bottom right corner of the screens
  (AppKit keeps a corner of about 40 by 32 points of a titled window on a screen; leaving every screen would change
  the backing scale of the picture and mark the window occluded, which stops a web view from drawing). It still lays
  out and draws as for a user, so the pictures and the launch phases are unchanged (both checked pixel for pixel and
  by `scripts/measure_startup.sh`). `PRIMESTRUCT_UI_SHOW_WINDOWS=1` shows the windows, for a person watching a run;
  the bundles the UI drivers build are agent apps (`PS_UI_ELEMENT=1` makes `scripts/bundle_macos_app.sh` write
  `LSUIElement`), so their Launch Services launches show no Dock icon either. CTest
  `PrimeStruct_native_ui_appkit_smoke` bundles `examples/native_ui/hello_window.prime`, runs the bundle in snapshot
  mode and checks the PNG; it also bundles the editor and checks two pictures of it (below).
- **Snapshot hooks for pictures of the editor.** `PRIMESTRUCT_UI_TYPE=text` types into the first text view,
  `PRIMESTRUCT_UI_OPEN=file` loads a file into it and highlights it (the title becomes the path), and
  `PRIMESTRUCT_UI_MENU=File` pops that menu-bar menu open for the picture. A pop-up menu is a window of its own
  whose Liquid Glass backdrop does not draw offscreen, so the picture draws a rounded backdrop and then each real
  menu row (`NSContextMenuItemView`) on it: the text, shortcuts and separators are AppKit's own rendering, the
  backdrop is ours. Example: `PRIMESTRUCT_UI_OPEN=$PWD/tests/fixtures/ui/screenshot/showcase.prime
  PRIMESTRUCT_UI_MENU=File PRIMESTRUCT_UI_SNAPSHOT=/path/out.png primestruct_app examples/apps/text_editor/main.prime`.
  The editor's menu structure is asserted exactly by the headless gate; the picture is a visual check.
  `PRIMESTRUCT_UI_TRACE=1` logs the document and styling calls to the unified log (subsystem `com.primestruct.ui`).
  `PRIMESTRUCT_UI_OPEN_PANEL=file` makes the Open panel answer with that path, and `PRIMESTRUCT_UI_TARGET_LAST=1`
  aims the open/type hooks and the picture at the newest tab (the tab bar does draw in these pictures when the app
  has run the New command, e.g. `PRIMESTRUCT_UI_COMMANDS=1000000,1000000` opens a file into the first and a second
  tab; the editor's command ids are listed with `EditorCommands` in main.prime).

**Unicode (TODO-5548).** Text crosses the ABI as UTF-8 and files are read and written byte for byte by the backend
(no newline translation, a missing final newline stays missing, invalid UTF-8 is refused with `ps_ui_last_error()`).
Cursor movement, selection, deletion and double-click word selection are not implemented by PrimeStruct: AppKit's
`NSTextView` (TextKit) moves by extended grapheme clusters, so an emoji with a skin-tone modifier, a ZWJ sequence, a
flag or a base letter with combining marks is one cursor step, and bidirectional text is laid out by the platform.
Checked by the gate cases in `tests/unit/ui/test_native_ui_editor_acceptance.cpp` (round trips of
`tests/fixtures/ui/unicode_sample.txt`, CRLF, no final newline, empty file, typed text, invalid UTF-8) and, for
rendering, by the snapshot of TODO-5553; stepping the cursor over clusters by hand is part of TODO-5545.

**Pointer and key events (TODO-5725).** Nothing is installed at launch: `ps_ui_wait_event` hands each pumped event
to `NSApp` at once while no widget is watched. `ps_ui_widget_watch` with the hover bit adds an `NSTrackingArea`
(entered and exited, active in the active app, the visible rect) to the widget's view, whose owner reports
`pointer_enter` and `pointer_leave`; dropping the bit removes it. With any watch, the pump looks at presses, releases
and keys before AppKit handles them, so the controls keep their own behaviour: a press is hit-tested to the innermost
watched widget, and when a control's tracking loop (a button's) takes the release from the queue the release is
reported once the loop returns, ahead of the click the control made of it. A key goes to the watched widget that
holds the window's first responder; a text view gets the key first, and the program hears of it only when the view
had no use for it: its delegate sees `noop:` (an unbound combination), `cancelOperation:` (Escape) or a command the
view does not implement while the key is handed to it. Snapshot runs drive it through `PRIMESTRUCT_UI_CLICK`
(`enter:H:X:Y`, `leave:H:X:Y`, `press:H:X:Y` hit-tested but not handed to the control, `key:H:K:M` sent to a window
that is not key, so nothing takes focus); `PrimeStruct_native_ui_compiled_app` runs
`tests/fixtures/ui/input_handlers.prime` that way.

Smoke checklist (macOS 27, arm64, 2026-10-08): window and title, text view with preset text, bundle launch from
bytecode, snapshot, quit request ending the loop: checked by the automated smoke. Typing, menu shortcuts, open/save
panels and the alert need a person at the keyboard: TODO-5545.

## 14. Markdown preview

**Goal (2026-10-23): PrimeScroll shows a markdown file as it will read next to its source, in every compile mode.**
View > Show Markdown Preview (Command-Shift-M) splits the active tab into the text view and a rendered page, and the
page follows the text as it is edited. It is done when the installed native PrimeScroll can show it.

- **One converter for every mode.** `primec::ui::markdownToHtml` / `markdownPreviewPage` (`include/primec/ui/Markdown.h`,
  `src/ui/Markdown.cpp`, in `primec_ui_highlight_lib`) is plain text in and text out. It lives behind the C ABI, so the
  editor program only makes host calls and runs the same in the VM, the optimized C++ build and the direct native
  build. It covers a CommonMark and GitHub subset (headings, paragraphs, fenced and indented code, quotes, nested and
  task lists, tables, emphasis, strikethrough, links, images, autolinks, bare URLs); raw HTML in the source is shown as
  text, and link and image targets are limited to http, https, mailto, relative references and `data:image/`, so a
  preview cannot run scripts. `PrimeStruct_native_ui_markdown` pins its output.
- **The web view is loaded only when needed.** The AppKit backend creates the `WKWebView` the first time a preview
  turns on (WebKit is `dlopen`ed then and its classes are used by name, so neither the compiled editor nor the AppKit
  dylib lists WebKit, which `PrimeStruct_native_ui_compiled_app` checks with `otool -L`), and destroys it when the
  preview turns off, so a session that never previews has the launch time and memory it has today (the installed
  PrimeScroll peaks at about 118 MB resident; with a preview on, the main process is about 25 to 30 MB larger, and the
  WebKit processes are separate).
- **As built (AppKit).** The preview is an `NSSplitView` with the text view on the left and the web view on the right,
  half and half, created for the tab that asked. JavaScript is off in the page; navigation is locked (the page the
  preview loads is allowed, in-page `#fragment` links work, a click on an http, https or mailto link opens the default
  browser and nothing navigates the preview). Refreshes are coalesced to one page load per run-loop turn. Scrolling is
  locked between the two views by source line: the converter tags every block and list item of the page with the line it
  starts on (`data-line`, `markdownToHtml(text, true)`), the text view reports the line, with the fraction through it,
  at its top, and the page scrolls to that place by interpolating between the tops of the blocks around it (and the
  other way round), so a tall code block or table does not put the views out of step; the end of either view is the end
  of the other, and a view that cannot scroll leaves the other alone. This is one of four scroll modes
  (`ps_ui_preview_set_scroll_mode`): independently; linearly (the same fraction of what each can scroll); by block
  (above, the default); and with the cursor (the page scrolls when the text cursor moves, putting its line a third of
  the page's height below the top, and scrolling either view alone moves nothing). The text is laid out in full before a
  line is mapped to a position, because TextKit only estimates the height of what it has not laid out and a line far
  down the source was put in the wrong place. A page that is reloaded (every edit) starts at the top, so after each load
  the position is put back by mode (together: to the text view; with the cursor: to the cursor's line; independent: to
  where it was), the mode can be chosen from View > Scroll Preview as well as Settings, and a document opens with its
  cursor at the start. The top line of the text view is read from its TextKit 2 layout fragments, not from a character
  index (after an edit that returned the end of the text). `PrimeStruct_native_ui_appkit_scroll` asserts the modes
  with the snapshot harness (`PRIMESTRUCT_UI_CARET`, `PRIMESTRUCT_UI_TYPE_AFTER_SCROLL`, `PRIMESTRUCT_UI_REPORT`);
  `docs/PreviewScrollTests.md` lists the scenarios and the by-hand checks. The session remembers which tabs had their
  preview on (`session.previews`, "1" or "0" per tab with a file, in the order of `session.files`) and the next launch
  switches those previews on again; restoring a session also selects the tab that was active, the first one included. The text view is observed directly (the bounds
  notification of its clip view) and moves the page with a script; the page has no scroll notification, so a local event
  monitor sees the wheel, keys and drags over it, reads its position back with a script and moves the text view (again
  after 250 ms, for the page's scrolling animation). A page that finishes loading goes to where the text view is, so an
  edit does not lose the reader's place. Snapshot runs scroll one view with `PRIMESTRUCT_UI_SCROLL=text:1` or
  `preview:1`, and the smoke test checks the other follows. The View item's check
  mark follows the active tab through `validateMenuItem:`. A snapshot (`PRIMESTRUCT_UI_SNAPSHOT`) waits for the page
  and draws the web view through `takeSnapshotWithConfiguration:`; the smoke test takes one of
  `tests/fixtures/ui/screenshot/preview.md` with `PRIMESTRUCT_UI_COMMANDS=1000008` (Show Markdown Preview).
- **ABI.** Four functions (section 4): `ps_ui_window_set_preview(window, on)`, `ps_ui_window_has_preview(window)`,
  `ps_ui_window_refresh_preview(window)` and `ps_ui_menu_follow_preview(menu, commandId)`. The backend owns the
  preview view; the editor decides when it shows (View > Show Markdown Preview, one preview per tab, its check mark
  following the active tab) and when it refreshes (every text change and every file opened into a tab).
- **Slices (all done).** The converter (TODO-5588), the ABI with the headless backend and the editor menu (TODO-5589:
  the editor scenarios run it on the VM, the optimized C++ build and the native build), and the AppKit split view with
  the lazily loaded web view (TODO-5590). The installed native PrimeScroll shows it.

## 15. Settings window

**Goal (2026-10-24): PrimeScroll has a proper settings window.** Command-Comma (the application menu's Settings...)
opens one window with tabs. Every setting is kept between runs, takes effect at once, and keeps the View menu in
step with it.

- **General:** Reopen the files of the last session (`restoreSession`, on).
- **Editor:** Show line numbers (`lineNumbers`, off; it replaced the application menu's Start with Line Numbers),
  Wrap long lines (`wrapLines`, on), Font size (`fontSize`, 13; a pop-up of 11, 12, 13, 14, 16, 18, 20 and 24).
- **Markdown:** Open markdown files with the preview on (`previewMarkdown`, off), and how the source and the preview scroll
  together (`scrollMode`, by block: independently, linearly, by block or with the cursor).
- **Controls.** The ABI grows a small widget set (section 4, Controls and forms): labels, check boxes, pop-ups, forms,
  tabs, the settings window style and the control-changed event. The headless backend prints a control tree as text
  (`headless::controlOutline`), so an acceptance test can pin the whole window like the menu bar's outline.
- **Not a document.** A settings window is never a tab and does not count as an open document: closing the last
  document ends the app and closes the settings window with it.
- **Slices (all done).** The controls ABI with the headless backend, the bindings and the stdlib (TODO-5592), the
  AppKit controls and the settings window style (TODO-5593), then the editor's window and its preferences (TODO-5594).
- **In the editor.** `main.prime` keeps a `Settings` struct of the window and control handles (0 while closed). A
  control event is matched against those handles, the matching preference is written, and `refresh_preferences` applies
  the editor preferences to every document and updates the View menu items and the check boxes. Documents are the
  windows other than the settings window; menu commands that arrive while it is active act on the first document.

## 16. File associations

**Goal (2026-10-26): PrimeScroll can be made the default application for a set of file types from its settings
window, including `.prime`; choosing another editor is the system's own way (Open With, Change All), which the declared types
support.**

- **What the app declares.** `examples/apps/text_editor/file_types.txt` lists the file types as `name|extensions|optional
  type identifier`: Plain text, Markdown, PrimeStruct source (`.prime`, with the exported identifier
  `org.primestruct.prime-source`), JSON, YAML, Configuration, C and C++, Shell scripts, Python, Web scripts and styles,
  and Logs. `scripts/bundle_macos_app.sh` turns the file next to a program into `CFBundleDocumentTypes` (ranked as an
  alternate handler, so installing the app changes nothing by itself) and, for entries with an identifier,
  `UTExportedTypeDeclarations` conforming to `public.source-code`. The program reads the list back with
  `ps_ui_document_type_count/name/extensions`, so the bundle is the one source; the headless backend has a fixed copy
  that a test keeps equal to the file.
- **Making the default.** `ps_ui_set_default_app(extensions, appPath)` makes the application at `appPath` ("" is the
  app itself) the system's default for a comma separated list of extensions (AppKit: `NSWorkspace
  setDefaultApplicationAtURL:toOpenContentType:completionHandler:`; the system may ask the user to confirm), and
  `ps_ui_default_app(extension)` reads the current handler. Choosing another editor than PrimeScroll is left to
  the system (Finder's Open With and Change All); the app has no chooser of its own. The system asks the user to
  confirm every content type it is asked to change, one question each, so a request is cut down to what it must ask:
  extensions that share a content type (txt and text, md and markdown) count once, and the types the application already
  opens are left out; the Files tab sends all selected types in one request. Launch Services' older
  `LSSetDefaultRoleHandlerForContentType` is no way around the questions: tried on a scratch type it returns success
  and changes nothing. `PRIMESTRUCT_UI_DEFAULT_APP_PLAN=file` is a dry run that writes the content types the system would
  be asked about to the file.
- **Opening.** A double click, Open With or a drop on the icon reaches the program as `PS_UI_EVENT_OPEN_FILE`
  (`AppEvent.openFile`, the path from `App.eventText()`).
- **Drag and drop.** The text views take files dropped on them as open requests (`PSDropTextView`: each dropped file is a
  `PS_UI_EVENT_OPEN_FILE`; the path is not pasted; dragging text is unchanged). The catch-all "Any file" document type
  (`public.data`, handler rank None) of the bundle lets any file be dropped on the Dock icon and offers the app in Open
  With for every file, without making it anybody's default. Checked with the harness drop and with a real
  Launch Services open of an undeclared extension; a real mouse drag and a Dock drop are tried by hand. The markdown
  preview (a web view) takes dropped files the same way (`PSDropWebView`, a runtime subclass of `WKWebView`: it used to
  refuse the navigation a drop asks for and so did nothing); the harness drops on it with `PRIMESTRUCT_UI_DROP_PREVIEW=path`.
- **Settings.** A Files tab: a selectable list (`ps_ui_list_*`: rows of text, any number selected, a change of the
  selection is a `PS_UI_EVENT_CONTROL_CHANGED`) with one row per declared file type, "✓ Markdown (.md, .markdown) -
  PrimeScroll": a check mark when PrimeScroll is the default of the type's first extension, then the application that is.
  The Make Default button makes PrimeScroll the default of the selected rows (selecting alone changes nothing; with
  nothing selected it does nothing). Opening a file the system asks for opens it in the untitled tab or a new one, or
  selects the tab that already holds it.
- **Test bundles.** A bundle that declares document types can become the system's handler of the user's files (a
  build-directory test bundle showed up as the default of `.prime` and the configuration types). Tests that do not look
  at the types set `PS_NO_DOCUMENT_TYPES=1` for the bundle script, and the smoke test unregisters the bundles that do
  declare them (`lsregister -u`).
- **No cost at startup.** Everything here happens only when the settings window is built or a button is pressed. The
  bundle's document types are read on the first call, the default application is looked up and set only then, and the
  application panel only opens on a click; nothing of it is on the way to the first window, so the launch time does not
  change. The one launch-time effect is the link to `UniformTypeIdentifiers`, which AppKit loads anyway (a launch loads
  the same libraries as before; checked with `DYLD_PRINT_LIBRARIES`).
- **AppKit.** `NSButton`; `application:openFiles:` for open-file requests (macOS also sends one for a file named on the
  command line, so a program should not open it twice); `CFBundleDocumentTypes` of the Info.plist for the declared types;
  `NSWorkspace` with `UTType` for reading and setting the default.
  The snapshot harness has `PRIMESTRUCT_UI_CLICK=button:H` (and `tab:H:I`, which shows page I of the tabs control H, and `rows:H:1,3`, which selects those rows of the list H),
  `PRIMESTRUCT_UI_OPEN_FILE_EVENT=path` and `PRIMESTRUCT_UI_DROP=path` (a file dropped on the text view). Setting a default is not exercised by a test (it changes the user's settings); reading
  it is.
- **Slices.** The ABI, the headless backend, the bindings and the stdlib (TODO-5599), the AppKit backend and the bundle's
  Info.plist (TODO-5600), then the editor's tab and open-file handling (TODO-5601). A bytecode-only bundle passes its
  arguments to the program like a native one (TODO-5602).

## 17. Markdown toolbar and editing

**Goal (2026-10-26): a toolbar that shows up by context, starting with markdown: a markdown document has one, other file
types do not.** The toolbar switches between the text editor, a split view and the preview, and inserts the common
markdown constructs; tables get the most help.

- **Context.** A window shows the toolbar when its file is markdown (`.md`, `.markdown`) or when its preview is on; the
  editor decides per window (`ps_ui_window_set_toolbar`, 0 hides it) whenever the file opens, is saved under another name
  or the preview changes. Nothing of it is built for other files, and the toolbar's items are made when the first
  markdown window needs them, so the launch time does not change.
- **View modes.** The preview of a window has three modes (`ps_ui_window_set_preview_mode`): 0 the text editor alone,
  1 split (the existing preview), 2 the preview alone. View > Show Markdown Preview still toggles 0 and 1, and the
  mode is part of the saved session. The toolbar's segmented control (Editor, Split, Preview) shows and sets it; View
  gets the same three commands with shortcuts.
- **A toolbar is a list of commands.** `ps_ui_toolbar_*` describes buttons (a command id, a title, an SF Symbol name),
  a segmented control (consecutive command ids), menu buttons (an existing `Menu`) and separators; a click is a
  `PS_UI_EVENT_COMMAND`, exactly like a menu item, so the program has one place that handles a command whether it came
  from the toolbar, the menu bar or a shortcut, and the Format menu offers the same commands with shortcuts. AppKit makes
  an `NSToolbar` per window from the description (the toolbar's own overflow handles a narrow window).
- **Insertions are text transformations, written once.** `primec::ui::markdownEdit` (`include/primec/ui/MarkdownEdit.h`)
  takes the text, the selection and an operation and returns the change and the new selection; the backends only apply
  it as one undoable edit (`ps_ui_text_view_markdown_edit`), so the behaviour is the same on every backend and is tested
  as plain strings. How editors usually do it, and what this does:
  - *Inline marks* (bold `**`, italic `*`, strikethrough `~~`, code `` ` ``): wrap the selection and keep it selected;
    with no selection wrap the word at the cursor, or insert the pair with the cursor between; a second use takes the
    marks off again (bold and italic together `***` are told apart by counting the stars); spaces at the edges of the
    selection stay outside the marks. Link and image: `[text](url)` with the selection as the text and `url` selected for
    typing (a selected URL becomes the target instead).
  - *Line formats* (heading level 1 to 6 or none, bulleted, numbered and task lists, quote): apply to every line the
    selection touches, convert one list kind into another, and toggle off when every line already has it; blank lines are
    left alone; numbers count up. A fenced code block wraps the selected lines (or inserts an empty one with the cursor
    inside) and unwraps when it is already wrapped; a horizontal rule goes on its own line between blank lines.
  - *Tables.* Raw pipe tables are hard to type, so the help comes in four parts, as in the table editors of the popular
    editors: Insert Table (columns and rows from a menu: 2 by 2, 3 by 3, 4 by 3, 5 by 4) makes a formatted skeleton with the
    first header selected for typing; Format Table aligns the columns (padding by the column's alignment, `:--`, `:-:`, `--:`
    kept); with the cursor in a table Add Row, Add Column, Delete Row, Delete Column and Align Left, Center, Right edit it
    and reformat it; and, in a markdown text view, Tab and Shift-Tab move between cells (selecting the cell's text),
    formatting the table first, and Tab in the last cell adds a row. Outside a table Tab is an ordinary tab. A table is
    the lines around the cursor that contain pipes with a delimiter row as the second line (GitHub's rule); escaped pipes
    `\|` stay in their cell.
- **Slices.** The editing library (TODO-5609), the ABI, headless backend, bindings and stdlib (TODO-5610), the AppKit
  toolbar, preview-only layout and edits (TODO-5611), then the editor's toolbar, Format menu and commands (TODO-5612).
  Smart list continuation on Return (a new bullet after a bullet) is the usual next step and is not part of these.

## 18. Running scripts

**Goal (2026-10-27): editing a `.prime` file offers a Run button; the editor runs the script on the PrimeScript interpreter
and shows its text output in a pane at the bottom of the window, like the markdown preview but simpler.**

How it is read: the editor's text is compiled and run by the interpreter (`primec::embed::ScriptEngine`, docs/Embedding.md)
**in a process of its own**. A run takes no arguments and shows what the script prints (stdout and stderr, in program
order) plus the compile or runtime diagnostic or the exit code. A script that does not end can be stopped, and a script
that opens windows is an app of its own.

- **Cancellation (TODO-5613, done).** `Vm::setCancelFlag` / `RunOptions::cancel`: the plain interpreter polls an atomic
  flag every 1024 steps and fails with "execution stopped"; a run with a flag skips the fast kernel. Output goes to a
  `RunOptions::output` callback (the VM's `VmOutputSink`). The editor no longer uses it (it ends a process instead); it
  stays for embedders.
- **The runner process (TODO-5617, done).** `primestruct_app --script-host NAME` (the development runner: compiler,
  interpreter and the native UI bindings; bundled as `Contents/MacOS/PrimeScriptRunner`) reads the script from stdin,
  runs it with the native UI bound and prints its last line ("[exit code N]", or the diagnostic) to stderr; stdout is
  unbuffered and shares one pipe with stderr, so the output stays in program order. `src/ui/ScriptProcess.cpp` (in
  `primec_ui_highlight_lib`, plain C functions `ps_script_start/stop/release`, `include/primec/ui/ScriptRunner.h`) spawns
  it with `posix_spawn`, writes the text, reads the pipe on a worker thread, and ends the process on Stop or when its
  window closes (SIGTERM, SIGKILL after three seconds). It finds the runner through `PRIMESTRUCT_SCRIPT_RUNNER`, then
  `PrimeScriptRunner` beside the library or program, then the build tree's `primestruct_app`. The stdlib directory goes to
  the child as `PRIMESTRUCT_STDLIB`. An earlier in-process version (a 19 MB dylib loaded with dlopen) was replaced: it
  could not run scripts that use the UI, could not stop a blocked script, and its allocator clashed with the compiler
  arena's global `operator new` inside libc++.
- **ABI (TODO-5614, done).** `ps_ui_script_run(window, name, text, endCommand)`, `ps_ui_script_stop`,
  `ps_ui_script_running`, `ps_ui_window_set_output_visible/output_visible`. The backend owns the pane: a run clears and
  shows it and appends the output as it arrives (queued to the main thread); the end of a run is a command event with
  `endCommand` and the window. Headless: the output and the end are handed over only when no user action is left, or at
  a `wait` scenario step (`pushWaitScript`), so a scenario is deterministic; `WindowState.output/outputVisible/
  scriptRunning`.
- **AppKit (TODO-5615, done).** The output pane is a read-only monospaced NSTextView in an NSSplitView with a horizontal
  divider under the window's content (about a third of the height); a run clears and shows it, appends the output on the
  main thread (`dispatch_async` plus a wake-up event, since `ps_ui_wait_event` sleeps in `nextEvent`) and enqueues the end
  command. Turning a preview on closes the pane first. `scripts/bundle_macos_app.sh` copies `primestruct_app` into
  `Contents/MacOS/PrimeScriptRunner` and `stdlib/` into `Contents/Resources`. Nothing is started at launch. The harness
  delivers `PRIMESTRUCT_UI_COMMANDS=+1000058` (Run) after `PRIMESTRUCT_UI_OPEN` has loaded its file and a snapshot
  waits for a running script.
- **Editor (TODO-5616, done).** A `.prime` window gets a toolbar with Run (`play.fill`), Stop (`stop.fill`) and,
  after a separator, Output (`rectangle.bottomhalf.inset.filled`: hides or shows the output pane, the same command as
  the menu's Show Output); the Run menu has Run Cmd+R, Stop Cmd+period and Show Output (Cmd+Option+O); the end of a run
  is the command `scriptDone` (`window.runScriptCommand`; nothing to do but what follows every command). The text run is the tab's text as it is, saved or not, named by the tab's path (or
  "untitled.prime"). `sync_toolbars` takes the kind of toolbar (1 markdown, 2 script) and is called once for each; a
  window of neither kind has none. The Run menu shifts the handles of every window created after the menus by one.
  A compiled (optexe) editor crashed freeing a reassigned `String` local, so `run_script` builds its strings with
  locals and branches.
