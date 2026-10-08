# Native UI: Plan

Status: planned (2026-10-08). Nothing below is implemented yet; the work is tracked as the TODO slices in section 10.

PrimeStruct programs get desktop GUIs built from **platform-native widgets**: an `NSTextView` on macOS, an `EDIT`
control or RichEdit on Windows, a `GtkTextView` on Linux. The first target is macOS and the first example is a plain
text editor. This plan is separate from `docs/Graphics_API_Design.md`, whose `/std/ui` command lists and `/std/scene`
graph stay the path for custom-drawn UI (games, canvases, the web host); native widgets never go through the scene
renderer.

## 1. Principles

- **Native first.** Each widget is the platform's own control, so it brings the platform's editing, selection,
  clipboard, undo, input methods, accessibility, spell checking and look. PrimeStruct code never draws a native
  widget.
- **Best effort per platform.** The API describes what an app wants (a menu bar, a text view, a save prompt). Each
  platform does as much of it as it can; where a platform has no equivalent, the call is a documented no-op or the
  nearest native form (for example, an iOS app has no menu bar, so its menus become a toolbar or are omitted). The
  coverage matrix in section 8 is the contract.
- **The program owns the loop.** `main` creates the app, then loops on `waitEvent()` until it decides to quit. State
  lives in ordinary locals, and events arrive as a sum type handled with `pick`; no callbacks are needed.
- **One C ABI, many hosts.** Every platform implements the same small C interface (section 3). Programs reach it
  through `[host]` functions; the runner binds them. The same program runs against the macOS backend, a headless test
  backend, and later Windows and Linux backends.
- **VM first.** Host calls run only on the VM today (`docs/spec/host-and-core-library.md`), so the first runner embeds
  the VM (`docs/Embedding.md`). Compiled backends calling the ABI directly come later (section 9).
- **Testable without a screen.** A headless backend implements the whole ABI in memory: it replays a scripted list of
  user actions and records every widget call, so the editor's behavior is golden-tested on Linux CI. Only the thin
  platform backends need manual checks on their OS.

## 2. Architecture

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

## 3. The C ABI (version 0)

Handles are opaque non-zero `uint64_t`; `0` means none or failure. Strings in and out are UTF-8. Every function
is callable only on the thread that called `ps_ui_init`, the main thread on macOS.

| Group | Functions |
| --- | --- |
| App | `ps_ui_init(appName)`, `ps_ui_quit()`, `ps_ui_platform() -> i32` (macOS, Windows, Linux, iOS, headless) |
| Events | `ps_ui_wait_event() -> i32 kind`, `ps_ui_event_window() -> u64`, `ps_ui_event_widget() -> u64`, `ps_ui_event_command() -> i32` |
| Window | `ps_ui_window_create(title, width, height) -> u64`, `_set_title`, `_set_content(window, widget)`, `_set_edited(window, bool)`, `_show`, `_close` |
| Text view | `ps_ui_text_view_create() -> u64`, `_get_text -> string`, `_set_text`, `_set_monospace(bool)`, `_is_modified -> bool`, `_clear_modified` |
| Menus | `ps_ui_menu_create(title) -> u64`, `_add_item(menu, title, shortcut, commandId)`, `_add_separator`, `_add_standard(menu, standardId)`, `ps_ui_menu_bar_add(menu)` |
| Dialogs | `ps_ui_open_panel(title) -> string`, `ps_ui_save_panel(title, suggestedName) -> string` (empty string means cancelled), `ps_ui_alert(message, detail, buttons) -> i32` |

- **Events.** `ps_ui_wait_event` blocks in the platform's run loop until something the program should handle happens,
  then returns its kind; the accessors read the fields of that event until the next wait. Kinds: `command` (a menu
  item or shortcut, with its command id), `window_close_requested`, `text_changed`, `quit_requested`. Everything
  else (typing, selection, scrolling, copy and paste, undo) is handled by the native widget and never reaches the
  program.
- **Shortcuts** are written portably (`"cmd+s"`, `"cmd+shift+s"`); `cmd` means Command on macOS and Ctrl elsewhere.
- **Standard items** (`undo`, `redo`, `cut`, `copy`, `paste`, `select_all`, `find`, `quit`, `about`) map to the
  platform's own actions (the AppKit responder chain), so they work in the focused native widget without program code.
- **Versioning.** `ps_ui_abi_version() -> i32` returns 0 for this table. Additions append functions; a change to an
  existing signature bumps the version.

## 4. The PrimeStruct surface

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

The names follow `docs/CodeExamples.md`; the exact API is settled in the stdlib slice (TODO-5529).

## 5. The text editor example

`examples/apps/text_editor/main.prime` is a single-window plain-text editor:

- **File menu:** New, Open… (⌘O), Save (⌘S), Save As… (⇧⌘S), Close (⌘W). **Edit menu:** the standard items.
- The window title is the file name (or `Untitled`), and the window shows the platform's unsaved-changes marker once
  the text changes.
- Closing or quitting with unsaved changes asks Save / Don't Save / Cancel.
- Open and Save read and write UTF-8 files; a read or write error is shown in an alert instead of quitting.
- The text view uses the system monospace font; everything else about editing comes from the native control.

## 6. Runtime gaps and how this plan closes them

| Gap | Resolution |
| --- | --- |
| Programs cannot read a whole file into a string (they cannot build strings) | VM: a stdlib `readText(path)` that returns a VM-owned run-time string (TODO-5530); writing a run-time string already works (`FileWriteString*` resolves dynamic strings). |
| Host calls are VM-only | The runner embeds the VM; compiled backends follow in TODO-5534. |
| Each `Script::call` is a fresh run | The program owns the loop inside one long `main` run, so its state persists. |
| No callbacks | Events are values (`AppEvent` sum) returned by `waitEvent()`. |
| Native file I/O bugs (TODO-5514) | Not on the VM path; fixed separately. |

## 7. Packaging

- CMake: `PRIMESTRUCT_BUILD_NATIVE_UI` (default ON on `APPLE`) builds the AppKit backend and `primestruct_app`.
- `scripts/bundle_macos_app.sh <app.prime> <Name>` makes `Name.app`: the runner, compiled bytecode, the stdlib if
  needed, an `Info.plist`, and an ad-hoc signature for local runs. Notarization is out of scope.

## 8. Platform coverage (contract)

| Feature | macOS (AppKit) | Headless | Windows (Win32) | Linux (GTK 4) | iOS (UIKit) |
| --- | --- | --- | --- | --- | --- |
| Window, title, edited marker | full | full | title only (marker drawn as `*` in the title) | title only (`*`) | one window; title in the navigation bar |
| Multi-line text view | `NSTextView` | in-memory | RichEdit | `GtkTextView` | `UITextView` |
| Menu bar + shortcuts | full | full | window menu + accelerators | `GtkPopoverMenuBar` + accelerators | none (commands via toolbar) |
| Standard edit items | responder chain | recorded | `WM_*` messages | GTK actions | system edit menu |
| Open/Save panels | `NSOpenPanel`/`NSSavePanel` | scripted | common dialogs | `GtkFileDialog` | document picker |
| Alerts | `NSAlert` | scripted | `MessageBox` | `GtkAlertDialog` | `UIAlertController` |

Planned columns are design intent; only macOS and headless are in the first slices.

## 9. Later

- **Compiled programs.** Let the C++ emitter (`--emit=exe`/`optexe`) call the ABI directly through `extern "C"`, then
  the native backend through a platform shim; programs stop needing the VM runner (TODO-5534).
- **More widgets:** label, button, single-line field, checkbox, list/table, split view, toolbar, status bar, layout
  stacks; each added to the ABI and the coverage matrix together.
- **Second and third platforms** (TODO-5533).

## 10. TODO slices

| TODO | Slice | Verifiable here |
| --- | --- | --- |
| TODO-5528 | C ABI header, headless backend, engine bindings, unit tests | yes |
| TODO-5529 | `/std/ui/native` surface and `AppEvent`, VM compile-run tests on the headless backend | yes |
| TODO-5530 | `readText(path)` into a VM-owned string | yes |
| TODO-5531 | AppKit backend, `primestruct_app` runner, CMake and `.app` bundling | builds and runs on a Mac only |
| TODO-5532 | Text editor example with headless golden scenarios | yes (macOS run by hand) |
| TODO-5533 | Windows and Linux backends | per platform |
| TODO-5534 | Compiled backends call the ABI | yes for the C++ emitter |
