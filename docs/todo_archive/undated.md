# Finished TODOs - undated

- [x] TODO-5541: Host functions take and return `String`
  - owner: ai
  - status: done
  - created_at: 2026-10-08
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: string-host
  - scope: The host boundary for owned text, independent of the `string` view. A `[host]` definition may name the stdlib `String` as a parameter or result type. A `String` parameter crosses as `IrHostValueKind::Text`: the compiler passes the string's data block and the VM hands the host a pointer to its NUL-terminated bytes without a copy. A `String` result crosses as `Text` too: the host returns text, the VM copies it into a fresh string-heap block, and the call builds the `String` (length, capacity, data, owning) in the program. The embedding API gains `HostText` (result) and `HostTextView` (parameter) next to the existing `std::string` forms. PSIR v30. docs/Embedding.md and docs/spec/vm-design.md updated. A module-table `string` literal in `String` argument position and `[host("link_name")]` are deferred to TODO-5571, which needs them.
  - acceptance:
    - embed tests: String arguments and results round-trip, empty text, a loop of 10000 host text calls keeps the string heap constant, discarded results are freed; full release gate green
  - stop_rule: the host boundary on the VM and the embedding API; the native UI port is TODO-5571 and the C++ emitters are TODO-5564.

- [x] TODO-5571: Native UI text uses `String`
  - owner: ai
  - status: done
  - created_at: 2026-10-22
  - phase: Native UI compiled
  - parallel_track: native-ui-compiled
  - scope: Ported stdlib/std/ui/native, the ScriptEngine bindings (src/ui/NativeUiBindings.cpp) and examples/apps/text_editor/main.prime to `String`: text-returning host functions (text view contents, window path, last error, panel results) return `String` (`HostText`), and parameters that receive run-time text (window title, text, file paths, highlight path, alert detail) take `String` (`HostTextView`); fixed labels (titles at creation, menu items, panel titles, alert message and buttons) stay `string` literals, and a literal reaches a `String` parameter through `stringFrom`. The command-line file argument reaches the editor through the new VM-answered `__pstext_argument` and `argumentText(index)`. The scripted scenarios of tests/unit/ui replay with identical call logs.
  - acceptance:
    - tests/unit/ui pass with unchanged call logs; PrimeScroll opens a file named on the command line (and reports one that cannot be opened); the text-heap tests keep the string heap constant over host text loops; full release gate green
  - stop_rule: the UI wrappers, bindings and the editor; no new language features.

- [x] TODO-5542: Native and C++ parity for bytes, slices and strings
  - owner: ai
  - status: done
  - created_at: 2026-10-08
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: string-backends
  - scope: Delivered for the optimized C++ emitters (`optcpp`, `optexe`): the `u8` byte opcodes (`HeapAllocBytes`, `LoadU8`, `StoreU8`, `ZextU8`) run on the pasted `VmHeapCore`, the string heap is the VM's own `VmTextHeap` (now `VmTextHeapBody.inc`, expanded as code and as `VmTextHeapSource` like the VM heap), and the Text host kind crosses `[host]` calls as `const char *` (a `String` argument is its NUL-terminated data block, a result is copied into the program's string heap); the `__pstext_*` functions and `__pstext_argument` are answered by the generated runtime. A new `IrValidationTarget::OptCpp` admits the byte opcodes and `u8`/text host values; the simple C++ emitters, the native backend, Wasm and GLSL still reject them with a diagnostic that names TODO-5574. This also delivers the host text boundary of TODO-5564 for the optimized emitters (its remaining matrix and loop hardening stays in TODO-5564).
  - acceptance:
    - tests/unit/backend_host/test_cpp_strings.cpp runs `u8` math, `vector<u8>`, `String` editing and growth, string-heap reuse and slices through the VM and `--emit=optexe` with equal exit codes; the native UI hello_window runs through `optexe` on the headless backend with the VM's call log; full release gate green
  - stop_rule: the optimized C++ emitters; the simple C++ emitters and the native backend are TODO-5574.

- [x] TODO-5559: Compiled native UI programs run on the platform backend
  - owner: ai
  - status: done
  - created_at: 2026-10-21
  - phase: Native UI compiled
  - parallel_track: native-ui-compiled
  - scope: `scripts/build_native_app.sh <app.prime> <output> [--backend appkit|headless] [--build-dir DIR] [--link-arg ARG]...` compiles a native UI program with `primec --emit=optexe` and links the AppKit backend (the default on macOS) or the headless backend, plus the highlighter library; the generated `main` runs the entry on the main thread and there is no VM and no compiler in the binary. `tests/fixtures/host_calls/scenario.h` writes a scripted headless run as text so the VM test and the compiled program (`headless_log_dump.cpp`, driven by `HEADLESS_SCENARIO`) queue the same user actions.
  - acceptance:
    - tests/unit/backend_host/test_cpp_native_ui.cpp runs hello_window and the editor (typing and saving, opening a chosen file, opening the command-line file, an unopenable file, the unsaved-close prompt) on the VM and as compiled programs with identical headless call logs and exit codes (CTest `PrimeStruct_cpp_native_ui`); cmake/NativeUiCompiledAppTest.cmake builds both as native AppKit executables and checks their snapshot PNGs, including the command-line file (CTest `PrimeStruct_native_ui_compiled_app`, Apple only); full release gate green
  - stop_rule: build path and equivalence tests; the rest of the editor's gate scenarios are TODO-5560.

- [x] TODO-5560: PrimeScroll compiles to native code
  - owner: ai
  - status: done
  - created_at: 2026-10-21
  - phase: Native UI compiled
  - parallel_track: native-ui-compiled
  - scope: The editor program compiles through `--emit=optexe` and behaves like the VM build: the CMake target `native_ui_compiled_editor_headless` builds it against the headless backend with `scripts/build_native_app.sh`, and every scenario of the editor, acceptance and highlight test files (`tests/unit/ui`) now also runs on it through `tests/unit/ui/CompiledEditorMirror.h`. The mirror remembers the scratch files before the VM run, replays the pushed actions (`headless::pushedScenario()`) in the compiled program from those same files, and checks the headless call log, the exit code and the files left behind are identical. No emitter or semantics difference showed up; the first mismatch found was the harness running the compiled editor after the VM run had already changed the files.
  - acceptance:
    - the whole `PrimeStruct_native_ui_editor` and `PrimeStruct_native_ui_editor_acceptance` scenario sets (including the 30 highlight samples) pass on the compiled editor with call logs identical to the VM; full release gate green
  - stop_rule: the editor program's feature set and its compiled-vs-VM equivalence.
