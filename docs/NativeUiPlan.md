# Native UI: Plan

Status: in progress (2026-10-08). The C ABI header, the headless backend and the engine bindings exist
(`include/primec/ui/`, `src/ui/`; TODO-5528); the rest is tracked as the TODO slices in section 11.

PrimeStruct programs get desktop GUIs built from **platform-native widgets**: an `NSTextView` on macOS, an `EDIT`
control or RichEdit on Windows, a `GtkTextView` on Linux. The first target is macOS and the first example is a plain
text editor. This plan is separate from `docs/Graphics_API_Design.md`, whose `/std/ui` command lists and `/std/scene`
graph stay the path for custom-drawn UI (games, canvases, the web host); native widgets never go through the scene
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

**Measured on the Mac, 2026-10-22** (`scripts/measure_app_startup.sh`, 10 runs of the editor in the AppKit backend's
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

**How it is measured** (TODO-5535): a launch harness starts an app, records the time until its first window is on
screen (window-list polling) and until the text is drawn (a signpost from the app), for cold and warm launches. It runs
the PrimeStruct editor, the reference Objective-C++ editor, and installed editors (TextEdit and others) for comparison.

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
| Tabs and documents | `ps_ui_window_add_tab(host, tab)` (native tab group; AppKit `addTabbedWindow:`), `ps_ui_active_window()`, `ps_ui_window_tab_count/tab_at(window, index)` (tab-bar order) and `ps_ui_window_select(window)` (what Command-1 to Command-9 use), `ps_ui_window_content(window)`, `ps_ui_window_count()`, `ps_ui_window_at(index)`, and a per-window document path (`ps_ui_window_set_path/path/has_path`, stored by the backend so programs keep per-document state without handling strings). Standard items 17-21: Show Tab Bar, Merge All Windows, Move Tab to New Window, Show Next/Previous Tab. Menu commands carry no window: programs act on the active window |
| Styled text | `ps_ui_text_view_clear_styles(view) -> bool`, `ps_ui_text_view_add_style(view, startByte, endByte, rgb, flags) -> bool` (UTF-8 byte ranges on code point boundaries, `rgb` is 0xRRGGBB, flags `PS_UI_STYLE_BOLD`/`PS_UI_STYLE_ITALIC`; later runs win; presentation only: no modified flag, no text-changed event, no undo entry; replacing the text drops the runs, typing keeps them so programs restyle on text-changed). AppKit applies them as text storage attributes with undo registration off), `ps_ui_text_view_highlight(view, path) -> bool` (restyles the text with the syntax highlighting of the file name's extension: PrimeStruct, C/C++/Objective-C, Python, JSON, Markdown, JavaScript/TypeScript, Rust, Go, Java, Kotlin, Swift, shell, SQL, CMake, with keyword, type, string, comment, number, function-name, annotation and variable colours; the lexers are C++ in `src/ui/SyntaxHighlight.cpp` because programs cannot scan strings yet) |
| Text files | `ps_ui_text_view_load_file(view, path) -> bool`, `ps_ui_text_view_save_file(view, path) -> bool`, `ps_ui_last_error() -> String` (the backend reads and writes UTF-8 files itself, so no program-side text is built and a large file never passes through the program) |
| Recent files | `ps_ui_recent_add(path)`, `ps_ui_recent_path(index) -> String` (most recent first), `ps_ui_recent_clear()` (the app's own defaults list `ps.recent.files` on macOS, kept between runs and across app updates; `PRIMESTRUCT_UI_RECENT=/a:/b` and every snapshot run use an in-memory list instead; the headless backend keeps the list in memory, preset with `headless::pushRecentFile`, at most 10 entries) |
| Preferences | `ps_ui_pref_get_bool(key, fallback) -> bool`, `ps_ui_pref_set_bool(key, value) -> bool` (and `_get_int/_set_int`, plus lists of texts: `ps_ui_pref_list_clear/add/count/get(key, ...)`; booleans the app keeps between runs: `NSUserDefaults` on macOS under `ps.pref.<key>`; `PRIMESTRUCT_UI_PREFS=key=1,other=0` and every snapshot run answer from the environment instead and never touch the user's defaults; the headless backend keeps them in memory, preset with `headless::setBoolPref`) |
| Controls and forms | `ps_ui_label_create(text)`, `_set_text`; `ps_ui_checkbox_create(title, checked)`, `_set_checked`, `_checked`; `ps_ui_popup_create()`, `_add_item(popup, title)`, `_select(popup, index)`, `_selected(popup) -> i32` (the first item added is chosen); `ps_ui_form_create()`, `_add_heading(form, title)`, `_add_row(form, label, control)` (the labels line up in one column, the controls in the next), `_add_control(form, control)` (a control alone in the controls' column, such as a check box); `ps_ui_tabs_create()`, `_add(tabs, title, content)`, `_select(tabs, index)` (the first page added is shown). A control is held by one form or tabs, or is a window's content (`ps_ui_window_set_content` takes any of them). The user's click on a check box or choice in a pop-up is `PS_UI_EVENT_CONTROL_CHANGED` with the control as the event's widget and its window; the program's own changes make no event. `ps_ui_window_set_style(window, PS_UI_WINDOW_SETTINGS)` makes a window that is never a tab and fits its content. `ps_ui_text_view_set_font_size(view, points)` (6 to 96). The headless backend keeps all of it in memory, answers scripted user actions (`headless::pushToggleCheckBox`, `pushSelectPopupItem`; `check:H` and `popup:H:I` in a scenario) and shows a control tree as text (`headless::controlOutline(root)`); the AppKit backend answers the controls with TODO-5593 |
| Markdown preview | `ps_ui_window_set_preview(window, bool)` (the window shows its text view and, beside it, the markdown of that text rendered as a page; the backend builds the preview only when it is turned on; false for a bad window or one with no text view; on while on is a no-op), `ps_ui_window_has_preview(window) -> bool`, `ps_ui_window_refresh_preview(window) -> bool` (render again from the current text; false without a preview; a backend may coalesce refreshes), `ps_ui_menu_follow_preview(menu, commandId) -> bool` (the items' check mark shows whether the active window has a preview, so it stays right when the user clicks another tab). The headless backend records the page (`headless::previewPage(window)`) and how often it was built (`WindowState::previewRenders`) |
| Dialogs | `ps_ui_open_panel(title) -> String`, `ps_ui_save_panel(title, suggestedName) -> String` (empty means cancelled), `ps_ui_panel_chosen() -> bool` (whether the last panel returned a path), `ps_ui_alert(message, detail, buttons) -> i32` |

- **Events.** `ps_ui_wait_event` blocks in the platform's run loop until something the program should handle happens,
  then returns its kind; the accessors read the fields of that event until the next wait. Kinds: `command` (a menu
  item or shortcut, with its command id), `window_close_requested`, `text_changed`, `quit_requested`. Everything
  else (typing, selection, scrolling, copy and paste, undo) is handled by the native widget and never reaches the
  program.
- **Shortcuts** are written portably (`"cmd+s"`, `"cmd+shift+s"`); `cmd` means Command on macOS and Ctrl elsewhere.
- **Standard items** (`undo`, `redo`, `cut`, `copy`, `paste`, `select_all`, `find`, `quit`, `about`) map to the
  platform's own actions (the AppKit responder chain), so they work in the focused native widget without program code.
- **As built.** `include/primec/ui/NativeUi.h` is the authority for exact signatures: flags are C `bool`, a text result
  is a `const char *` valid until the next text-returning call, `ps_ui_alert` takes button titles separated by `\n` and
  returns the pressed index, and `ps_ui_init` returns false when the app is already running. The headless backend
  (`primec/ui/NativeUiHeadless.h`) ends a scripted run with `quit_requested` once its action list is empty.
  `primec/ui/NativeUiBindings.h` binds all 88 functions into a `ScriptEngine` or `Script` under their ABI names. The
  three text-file functions (TODO-5546) let the editor open and save before owned `String` exists (TODO-5530).
- **Versioning.** `ps_ui_abi_version() -> i32` returns 0 for this table. Additions append functions; a change to an
  existing signature bumps the version.

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

## 6. The text editor example

`examples/apps/text_editor/main.prime` is a single-window plain-text editor:

- **File menu:** New, Open… (⌘O), Save (⌘S), Save As… (⇧⌘S), Close (⌘W). **Edit menu:** the standard items.
- The window title is the file name (or `Untitled`), and the window shows the platform's unsaved-changes marker once
  the text changes.
- Closing or quitting with unsaved changes asks Save / Don't Save / Cancel.
- Open and Save read and write UTF-8 files; a read or write error is shown in an alert instead of quitting.
- The text view uses the system monospace font; everything else about editing comes from the native control.
- **Line numbers:** View > Show Line Numbers (⇧⌘L) shows or hides a bar of line numbers to the left of the text in every
  tab (new tabs follow the current setting; the item carries a check mark). The application menu's *Start with Line
  Numbers* item is the preference for the next launch (a check mark, stored with `ps_ui_pref_set_bool`); the bar is off
  until it is chosen. The AppKit bar is an `NSRulerView` that numbers the TextKit 2 layout fragments of the visible
  part only.
- **Wrapping:** View > Wrap Lines (check-marked, on by default) wraps long lines at the window width or lets them
  run to the right with a horizontal scroll bar, in every tab; new tabs follow the current setting.
- **Session:** the first window is named for frame autosave, so its position and size come back. The files the
  tabs hold (in tab-bar order) and the active tab are kept as the list `session.files` and the integer
  `session.active`, saved after every command and before the tabs close on quit; the next launch reopens them one
  tab each and selects the active one (a file that is gone is skipped, a file named on the command line replaces the
  restore, and closing the last tab leaves the session as it was, since the app ends then like it does on quit; closing
  one of several tabs drops it from the session).
- **Open Recent:** File > Open Recent lists up to ten recently used files (opened or saved by the editor, or given on
  the command line), most recent first, with a Clear Menu item. The submenu is rebuilt after every open, save and
  clear with `Menu.clear()` and `Menu.addRecentFiles(...)`; choosing a file opens it like Open does.

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
  program is then told to quit. The app runs as an accessory (no Dock icon, no activation). CTest
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
  has run the New command, e.g. `PRIMESTRUCT_UI_COMMANDS=2,2` opens a file into the first and a second tab).

**Unicode (TODO-5548).** Text crosses the ABI as UTF-8 and files are read and written byte for byte by the backend
(no newline translation, a missing final newline stays missing, invalid UTF-8 is refused with `ps_ui_last_error()`).
Cursor movement, selection, deletion and double-click word selection are not implemented by PrimeStruct: AppKit's
`NSTextView` (TextKit) moves by extended grapheme clusters, so an emoji with a skin-tone modifier, a ZWJ sequence, a
flag or a base letter with combining marks is one cursor step, and bidirectional text is laid out by the platform.
Checked by the gate cases in `tests/unit/ui/test_native_ui_editor_acceptance.cpp` (round trips of
`tests/fixtures/ui/unicode_sample.txt`, CRLF, no final newline, empty file, typed text, invalid UTF-8) and, for
rendering, by the snapshot of TODO-5553; stepping the cursor over clusters by hand is part of TODO-5545.

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
  locked between the two views: both sit at the same fraction of the height they can scroll (proportional, not
  line-accurate; a view that cannot scroll leaves the other alone). The text view is observed directly (the bounds
  notification of its clip view) and moves the page with a script; the page has no scroll notification, so a local event
  monitor sees the wheel, keys and drags over it, reads its position back with a script and moves the text view (again
  after 250 ms, for the page's scrolling animation). A page that finishes loading goes to where the text view is, so an
  edit does not lose the reader's place. Snapshot runs scroll one view with `PRIMESTRUCT_UI_SCROLL=text:1` or
  `preview:1`, and the smoke test checks the other follows. The View item's check
  mark follows the active tab through `validateMenuItem:`. A snapshot (`PRIMESTRUCT_UI_SNAPSHOT`) waits for the page
  and draws the web view through `takeSnapshotWithConfiguration:`; the smoke test takes one of
  `tests/fixtures/ui/screenshot/preview.md` with `PRIMESTRUCT_UI_COMMANDS=10`.
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
- **Editor:** Show line numbers (`lineNumbers`, off; it replaces the application menu's Start with Line Numbers),
  Wrap long lines (`wrapLines`, on), Font size (`fontSize`, 13; a pop-up of 11, 12, 13, 14, 16, 18, 20 and 24).
- **Markdown:** Open markdown files with the preview on (`previewMarkdown`, off), and Scroll the source and the preview
  together (`lockScroll`, on).
- **Controls.** The ABI grows a small widget set (section 4, Controls and forms): labels, check boxes, pop-ups, forms,
  tabs, the settings window style and the control-changed event. The headless backend prints a control tree as text
  (`headless::controlOutline`), so an acceptance test can pin the whole window like the menu bar's outline.
- **Not a document.** A settings window is never a tab and does not count as an open document: closing the last
  document ends the app and closes the settings window with it.
- **Slices.** The controls ABI with the headless backend, the bindings and the stdlib (TODO-5592, done), the AppKit
  controls and the settings window style (TODO-5593), then the editor's window and its preferences (TODO-5594).
