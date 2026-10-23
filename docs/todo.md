# PrimeStruct TODO Log

## Purpose

This file is the live open-work queue for PrimeStruct.

- Keep only open work here: `[ ]` queued or `[~]` in progress.
- Move completed work to `docs/todo_finished.md` (below its marker), then run
  `python3 scripts/archive_todo_finished.py`: it files the block verbatim under
  `docs/todo_archive/` and regenerates the index (`grep TODO-NNNN
  docs/todo_finished.md` finds the archive file).
- Do not keep completed-task summaries, historical rollout notes, or closed
  coverage snapshots in this file.
- When this file has no task blocks, the tracked TODO queue is empty.

## Operating Rules

1. Use one task block per item with a stable `TODO-XXXX` ID.
2. Every active leaf must be implementable by someone arriving with no session
   context, including an AI agent.
3. Every active leaf must include `owner`, `created_at`, `scope`,
   `acceptance`, and `stop_rule`.
4. Prefer small, testable leaves over broad epics; split work before starting
   when acceptance cannot be verified in one bounded change.
5. Every active leaf must target at least one value outcome:
   - user-visible behavior change
   - measurable perf/memory improvement
   - deletion of a real compatibility subsystem
6. Avoid standalone micro-cleanups unless bundled into a value outcome.
7. If a leaf misses its value target after two attempts, archive it as
   low-value and replace it with a different hotspot.
8. Keep `Ready Now`, `Immediate Next 10`, `Priority Lanes`, `Execution Queue`,
   and task blocks synchronized when adding, splitting, completing, or deleting
   a task.
9. Keep `Ready Now` capped at eight active leaf tasks.
10. Keep active work leaf-shaped: queue sections must not contain umbrella,
    tracker, phase, research-shaped, or "continue with another slice" items.
11. For parallel work, each `Ready Now` item must name a `parallel_track` and a
    primary surface. Do not put two same-track successors in `Ready Now` unless
    their task blocks prove they touch different source/test surfaces.
12. Treat disabled tests as debt: each retained `doctest::skip(true)` cluster
    must map to an active TODO leaf with a re-enable-or-delete outcome, or be
    removed once proven stale.
13. Treat failing release-test cases as the top priority queue item: before
    starting new implementation work, update `docs/failing_tests.md`, fix the
    oldest reproducible failure first, and keep `docs/todo.md` aligned with the
    active test-fix work.
14. Every release test run must record any failing cases in
    `docs/failing_tests.md` before broader work continues.
15. When completing a task, mark it `[x]`, add `finished_at` plus a short
    evidence note, move the full block below the marker in `docs/todo_finished.md`,
    run `scripts/archive_todo_finished.py` (moves it into
    `docs/todo_archive/<YYYY-MM>.md` and regenerates the index), and remove
    it from this file.
16. This file is read fresh by an LLM agent each session, not browsed by a
    human - optimize for grep-ability over prose. Keep each task block to
    its current state (scope/acceptance/stop_rule), not a narrative history.
    Dated investigation/progress notes go in `docs/todo_log.md` instead,
    under that task's own `## TODO-XXXX` heading - when a task closes, fold
    whatever's still relevant into its `docs/todo_finished.md` resolution
    note and delete its `docs/todo_log.md` section.
17. Every active leaf must set `status` to exactly one of `ready` (its own
    repro/acceptance gap is confirmed and nothing external blocks starting),
    `blocked` (a specific, currently-`[ ]` `TODO-XXXX` must close first -
    name it in `blocked_on`; if that TODO is actually closed, the leaf is
    not blocked - fix the status instead of leaving it stale), or `deferred`
    (deprioritized, needs further scoping, or of confirmed-low value, with
    no single external blocker). Only `ready` leaves belong in `Ready Now`.

## Task Template

```md
- [ ] TODO-<id>: Short title
  - owner: ai|human
  - status: ready|blocked|deferred
  - blocked_on: TODO-XXXX (required when status: blocked; omit otherwise)
  - created_at: YYYY-MM-DD
  - phase: Group/Phase name (optional)
  - parallel_track: short-track-name (required when listed in Ready Now)
  - depends_on: TODO-XXXX, TODO-YYYY (optional)
  - scope: ...
  - implementation_notes: optional, but required when source/test entry points are not obvious
  - acceptance:
    - ...
    - ...
  - stop_rule: ...
  - notes: optional
```

Dated investigation history for this task goes in `docs/todo_log.md` under
a matching `## TODO-<id>` heading, not inline here.

## Open Tasks

### Queue Summary

Generated from each task block's own `status`/`parallel_track` fields -
re-derive after editing any block rather than hand-editing this table out
of sync with them.

| ID | Title | Status | Track |
| --- | --- | --- | --- |
| TODO-5567 | Packed `array<u8>` | ready | bytes |
| TODO-5569 | Slice loans, escapes and returned borrows | ready | slices |
| TODO-5570 | Slice spellings: methods on `vector<T>`, `[i]`, `array<T>` views | ready | slices |
| TODO-5540 | `string` as the UTF-8 text view | blocked | string-view |
| TODO-5593 | AppKit controls and the settings window style | ready | settings-window |
| TODO-5594 | The editor's settings window | blocked | settings-window |
| TODO-5586 | Native String heap: pooled blocks, statistics and limit | deferred | native-backend-strings |
| TODO-5574 | Simple C++ emitters run bytes and strings | deferred | string-backends |
| TODO-5568 | String conveniences: operators, numbers, push of code points | blocked | string-owned |
| TODO-5536 | Small, lazily loaded bytecode for apps | ready | bytecode-startup |
| TODO-5535 | Launch-time harness and reference Objective-C++ editor | ready | native-ui-launch |
| TODO-5530 | Read and write whole files as String | blocked | file-text |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5510 | `return` returns from pick arms and lambdas correctly | ready | control-returns |
| TODO-5524 | Int-backed error structs round-trip through stdlib Result sums | ready | result-error-structs |
| TODO-5523 | A Result-returning main exits with its error code | deferred | result-main |
| TODO-5525 | `Result.ok(x)` passes as a stdlib Result argument | deferred | result-arguments |
| TODO-5526 | Vectors of stdlib Result values keep their elements | deferred | result-containers |
| TODO-5527 | Stdlib Result locals destroy their payload once | ready | lifecycle-result-payloads |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-5512 | Pointers and aliases count as borrows of their root | deferred | safety-borrows |
| TODO-5513 | Methods through a dereferenced vector pointer read the right fields | deferred | collections-access |
| TODO-5514 | Native file I/O writes newlines and reports errno | deferred | native-io |
| TODO-5515 | Native Result.ok(Buffer) reads as ok | deferred | native-result |
| TODO-5516 | Integer narrowing and float-to-int conversion agree across backends | deferred | numeric-conversions |
| TODO-5517 | Runtime faults exit the same way on every backend | deferred | runtime-faults |
| TODO-5518 | Valid programs the frontend rejects compile | deferred | frontend-accept |
| TODO-5519 | Operator rewriting handles calls, nested `!` and `?` precedence | deferred | text-filter |
| TODO-5520 | Diagnostics point at user code with readable names | deferred | diagnostics |
| TODO-5521 | Math helpers return correct results | deferred | stdlib-math |
| TODO-5483 | Verify arm64 SextI32 on a macOS machine | deferred | ir-semantics |
| TODO-5543 | Repair or retire the disabled Apple/arm64 native backend test shards | deferred | native-arm64-tests |
| TODO-5544 | Method calls on `pick` payload bindings lower | deferred | pick-bindings |
| TODO-5545 | Record the macOS AppKit manual smoke checklist | deferred | native-ui-macos-manual |

### Ready Now

- TODO-5567 (bytes): packed `array<u8>`
- TODO-5570 (slices): slice spellings: methods on `vector<T>`, `[i]`, `array<T>` views
- TODO-5593 (settings-window): AppKit controls and the settings window style
- TODO-5536 (bytecode-startup): small, lazily loaded bytecode for apps
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor
- TODO-5510 (control-returns): `return` returns from pick arms and lambdas correctly
- TODO-5524 (result-error-structs): int-backed error structs round-trip through stdlib Result sums
- TODO-5527 (lifecycle-result-payloads): stdlib Result locals destroy their payload once

### Immediate Next 10

2. TODO-5567
3. TODO-5570
4. TODO-5593
5. TODO-5536
6. TODO-5535
7. TODO-5510
8. TODO-5524
9. TODO-5527

### Priority Lanes

- Strings and slices (docs/spec/strings-and-views.md; first leg of the PrimeScroll native goal below): TODO-5567, TODO-5570, TODO-5569, TODO-5540, TODO-5568, TODO-5530, TODO-5574
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5586
- Settings window (docs/NativeUiPlan.md section 15): TODO-5593, TODO-5594
- Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5535, TODO-5533
- Lifecycle (docs/spec/value-lifecycle.md): TODO-5527
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5510, TODO-5524, TODO-5515, TODO-5523, TODO-5525, TODO-5526
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5514, TODO-5516, TODO-5517
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5510: `return` returns from pick arms and lambdas correctly
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: control-returns
  - scope: `pick(m) { none { return(-1i32) } num(v) { return(v) } }` in statement position does not return from the unit-variant arm (rewritten to `none{-1}`, IR `PushI32 -1; Pop`); same for `Result<E>`'s `ok { return(...) }`. A lambda passed to `Result.map`/`and_then` with `if(x > 5i32) { return(100i32) }` returns from the enclosing function.
  - acceptance:
    - unit-variant pick arms return; lambda returns stay in the lambda; parse/semantic + matrix tests
    - full release gate green
  - stop_rule: return lowering in pick arms and lambdas.

- [ ] TODO-5511: Safe code cannot reach container storage or unsafe stdlib helpers
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-stdlib
  - scope: Safe code frees or aliases container storage through stdlib internals: `[unsafe]` helpers are callable from safe code (`vectorFreeStorage`, `vectorSlotUnsafe`, `vectorBorrowSlot` returning an untracked `Reference`, `vectorTakeSlot`/`DropSlot`/`InitSlot`, ring-buffer and SoA slot helpers), non-public helpers are callable by absolute path, `Vector`'s `data`/`fieldCount`/`fieldCapacity` fields are public and writable (double free, out-of-bounds read), its brace constructor accepts arbitrary storage, and Map exposes `keys`/`payloads`/`inner`. Make those helpers `[unsafe_api]` (or private and enforced), make container fields private or read-only, and keep the stdlib compiling.
  - acceptance:
    - each repro (free via helper, borrowSlot then reserve, takeSlot then clear, `w.data = v.data`, writing fieldCount, brace-constructing a Vector around `v.data`) is rejected outside `[unsafe]`; stdlib and corpus still compile
    - full release gate green
  - stop_rule: visibility and unsafe marking; no container redesign.
  - notes: deferred: queued behind the Ready Now cap; large, split before starting.

- [ ] TODO-5512: Pointers and aliases count as borrows of their root
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: safety-borrows
  - scope: `[Pointer<vector<i32>>] p{location(v)}` stays usable after `move(v)` or passing `v` to a move parameter (only References are tracked); a read-only view `[vector<i32>] view{v}` stays live while `v` is grown or cleared through a `Reference` alias passed to a helper (the view check only matches root names).
  - acceptance:
    - both are rejected with borrow diagnostics; negative + positive semantics tests
    - full release gate green
  - stop_rule: borrow tracking for Pointer roots and aliases.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5513: Methods through a dereferenced vector pointer read the right fields
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: collections-access
  - scope: With `[Pointer<vector<i32>>] p{location(v)}`, `dereference(p).count()` returns 0 and `dereference(p).capacity()` returns the count on VM and native; `vectorCount<i32>(dereference(p))` on a `Pointer<Vector<i32>>` is rejected with `expected /std/collections/vector/Vector__t... got /vector`.
  - acceptance:
    - count/capacity/at through `dereference(p)` match the direct calls; matrix case
    - full release gate green
  - stop_rule: receiver offset handling only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5514: Native file I/O writes newlines and reports errno
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: native-io
  - scope: Native `writeLine` never writes the newline (`emitFileWriteNewline` pops the fd into a register that `emitWriteNewlineReg` overwrites with `'\n'`, so it writes to fd 10); native file open/read errors always report 1 (EPERM) instead of the syscall errno (`emitFileOpenPlaceholder`, `emitFileOpenDynamicPlaceholder`, `emitFileReadByte` in `src/native_emitter/NativeEmitterInternalsX64Io.h`). Also `png.read` of a missing file returns `image_read_unsupported` on VM/C++ because ENOENT (2) collides with readImpl's status 2.
  - acceptance:
    - native writeLine and missing-file errors match VM and C++ (matrix cases); missing PNG reports `image_invalid_operation` everywhere
    - full release gate green
  - stop_rule: native I/O emitter and png status mapping.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5567: Packed `array<u8>`
  - owner: ai
  - status: ready
  - created_at: 2026-10-22
  - phase: Strings and slices, follow-up to the packed `vector<u8>` leaf
  - parallel_track: bytes
  - scope: `array<u8>` stores one byte per element like `vector<u8>` already does. Arrays are inline `[count][elements...]` slot aggregates, so this needs a decision first (a header slot plus a byte-region data pointer, or a byte-region-only array) and then the array literal, `at`/`at_unsafe`, `count`, element assignment, iteration, copy/move/return and `Slice<u8>` over an array in the lowerer; `Pointer<u8>` into an array must then step one byte. Update docs/spec/type-system.md.
  - acceptance:
    - VM matrix cases for `array<u8>`: literal, reads, writes, count, iteration, passing and returning, out-of-range faults; an `array<u8>` of 1 MB uses about 1 MB of VM heap; full release gate green
  - stop_rule: `array<u8>` on the VM; native and C++ are TODO-5574.

- [ ] TODO-5570: Slice spellings: methods on `vector<T>`, `[i]`, `array<T>` views
  - owner: ai
  - status: ready
  - created_at: 2026-10-22
  - phase: Strings and slices (docs/spec/strings-and-views.md), follow-up to the slice value
  - parallel_track: slices
  - scope: The user-facing spellings the slice value leaves out: `values.view()`, `values.slice(start, end)` and `values.sliceMut(start, end)` on a `vector<T>` (today `/std/collections/vector/view<T>(values)` etc.; the builtin vector surface is name-driven in several places, and `slice` is also the array-copy builtin), `view`/`slice`/`sliceMut` and packed `u8` storage for `array<T>`, `view[i]` and `view[i] = v` on a slice (index sugar currently types only builtin collections), and `Slice<T, Capability>` resolving without an explicit `import /std/collections/slice/*` (the implicit stdlib include stub). Retire the copying `slice(array, start, end)` builtin once array views exist.
  - acceptance:
    - tests for each spelling on `vector<i32>`, `vector<u8>` and `array<i32>` owners; docs/CodeExamples.md uses the method spellings; full release gate green
  - stop_rule: the spellings and array views on the VM; native and C++ are TODO-5574.

- [ ] TODO-5569: Slice loans, escapes and returned borrows
  - owner: ai
  - status: ready
  - created_at: 2026-10-22
  - phase: Strings and slices (docs/spec/strings-and-views.md), second of two leaves for slices
  - parallel_track: slices
  - scope: The borrow rules of docs/spec/strings-and-views.md (Borrowing): a slice is a loan on its owner's root binding until its last use (the owner cannot be pushed, popped, reserved, cleared, assigned, moved, passed to a `mut` or `move` parameter or destroyed: `borrowed binding: <root>`); read slices may overlap, a `Write` slice conflicts with any other live slice of the owner; a slice cannot be stored in a field, container, outer binding or global (`slice escapes via <place> (root: <root>)`); a definition may return a slice derived from exactly one parameter, and `[returns_borrow<name>]` names which when it could come from several.
  - acceptance:
    - a negative test per diagnostic and a positive test per allowed shape (overlapping reads, returned borrow, narrowing a slice); full release gate green
  - stop_rule: borrow checking for slices on the VM front end (the checks are backend independent).

- [ ] TODO-5568: String conveniences: operators, numbers, push of code points
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5540
  - created_at: 2026-10-22
  - phase: Strings and slices (docs/spec/strings-and-views.md), follow-up to the owned `String`
  - parallel_track: string-owned
  - scope: The parts of the owned `String` API that need the `string` view: `+` and `+=` with `string` and `String`, `push(u32 codePoint)` with UTF-8 encoding, `String.from(x)` for integers, floats and `bool`, `String.fromUtf8(bytes)` returning `Result<String, TextError>`, `tryReserve` returning `Result<void, TextError>`, the `String.from(text)` spelling for `stringFrom`, `at(i)`/`[i]` on `String`, and the implicit borrow of a `String` where a `string` is expected. Per-thread string heaps and scoped arenas are separate later work.
  - acceptance:
    - VM tests for each operation, including invalid UTF-8 and the borrow rules; full release gate green
  - stop_rule: the owned-text conveniences on the VM; native and C++ are TODO-5574.

- [ ] TODO-5540: `string` as the UTF-8 text view
  - owner: ai
  - status: ready
  - created_at: 2026-10-08
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: string-view
  - scope: Make `string` the borrowed read-only UTF-8 view: literals as static views, implicit borrow from `String`, `utf8(bytes)`, byte-offset indexing with code-point boundary checks, the text operations (bytes, code points, lines, split, trim, ASCII case, parsing, `+`), the static-text rule for `string` fields, and the `run-time text requires String` diagnostic.
  - acceptance:
    - VM tests for each operation and diagnostic; the existing test suite passes with `string` parameters and literals unchanged
    - full release gate green
  - stop_rule: the text view on the VM.

- [ ] TODO-5574: Simple C++ emitters run bytes and strings
  - owner: ai
  - status: deferred
  - created_at: 2026-10-22
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: string-backends
  - scope: Port the `u8` byte opcodes (HeapAllocBytes, LoadU8, StoreU8), the `__pstext_*` string heap and the Text host kind to the simple C++ emitters (`--emit=cpp`, `--emit=exe`), so `String`, `Slice` and `vector<u8>` programs run there as well as on the VM and the optimized C++ emitters (which TODO-5542 finished); replace `std::string_view` for `string` once TODO-5540 defines the view; remove the remaining "VM-only" limits from docs/spec/vm-design.md and docs/spec/backend-type-support.md. The arm64 native backend is TODO-5581 to TODO-5585.
  - acceptance:
    - the tests/unit/backend_host/test_cpp_strings.cpp programs also pass through `--emit=exe` with the VM's exit codes; full release gate green
  - stop_rule: the simple C++ emitters only.
  - notes: deferred: PrimeScroll compiles through the optimized C++ emitter, which needs none of this.

- [ ] TODO-5586: Native String heap: pooled blocks, statistics and limit
  - owner: ai
  - status: deferred
  - created_at: 2026-10-23
  - phase: Native PrimeScroll (docs/NativeUiPlan.md)
  - parallel_track: native-backend-strings
  - scope: The arm64 native String heap gives every text block its own mapping (a page or more each, a syscall per allocation and free). Pool small blocks in 256 KiB chunks per size class as the VM's `VmTextHeap` does, with state in the writable data segment of TODO-5582, and answer `__pstext_stat` and `__pstext_set_limit` ("string memory limit exceeded"); freed blocks are zeroed and a double free faults.
  - acceptance:
    - the string_heap program of tests/unit/backend_host/test_cpp_strings.cpp (reuse and figures) also runs through `--emit=native` with the VM's exit codes; a 100000-String loop uses no more than a few MB; full release gate green
  - stop_rule: the native String heap's allocator and its statistics.
  - notes: deferred behind the settings window (TODO-5593, TODO-5594) to keep Ready Now at eight. PrimeScroll ships from the native build, so every String it makes takes a mapping of its own; the writable data segment this needs exists (`MachOData::stateBytes`).

- [ ] TODO-5593: AppKit controls and the settings window style
  - owner: ai
  - status: ready
  - created_at: 2026-10-24
  - phase: Settings window (docs/NativeUiPlan.md section 15)
  - parallel_track: settings-window
  - scope: The AppKit backend (`NativeUiAppKit.mm`, in the static library and the dylib) answers the controls of TODO-5592: labels (`NSTextField` labels), check boxes (`NSButton` switches), pop-ups (`NSPopUpButton`), forms (an `NSGridView` with a right-aligned label column, a control column and bold heading rows), tabs (`NSTabView`), a window whose content is not a text view, `ps_ui_window_set_style` (a settings window: no tabbing, a fixed size fitted to its content, no minimize or zoom), `ps_ui_text_view_set_font_size` (monospace and proportional text, kept through highlighting), and the control events (`PS_UI_EVENT_CONTROL_CHANGED` from the control's action, with the control and its window). Snapshot mode can take a picture of a settings window built by a test program and click a control.
  - acceptance:
    - the smoke test's snapshot of a PrimeStruct settings window (tabs, a form with a heading, a check box and a pop-up) is a real PNG that looks like a macOS settings window when viewed; a scripted click on a check box and on a pop-up item arrives in the program as control events; the font size changes a text view's text; full release gate green
  - stop_rule: the AppKit controls and style; the editor's window is TODO-5594.

- [ ] TODO-5594: The editor's settings window
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5593
  - created_at: 2026-10-24
  - phase: Settings window (docs/NativeUiPlan.md section 15)
  - parallel_track: settings-window
  - scope: PrimeScroll gets a settings window: Command-Comma, "Settings..." in the application menu (it replaces "Start with Line Numbers"), a `PS_UI_WINDOW_SETTINGS` window with tabs General, Editor and Markdown. Settings, all kept as preferences and applied at once: `restoreSession` (default on: whether the last session's files come back at launch), `lineNumbers` (existing key, default off), `wrapLines` (default on), `fontSize` (default 13; a pop-up of 11, 12, 13, 14, 16, 18, 20, 24), `previewMarkdown` (default off: a file named `*.md` or `*.markdown` opens with its preview on), `lockScroll` (default on). The editor reads the preferences instead of carrying `lineNumbers` and `wrapLines` through its functions; the View menu's Show Line Numbers, Wrap Lines items and the settings check boxes stay in step; the settings window does not count as a document (the app ends when no document window is left and the settings window closes with it) and opens once (a second Command-Comma brings it forward).
  - acceptance:
    - editor scenarios on the VM, the optexe editor and the native editor through the mirror: Command-Comma builds the window, the acceptance gate pins its whole outline with `headless::controlOutline`, toggling each control changes its preference and the open tabs at once, the first launch uses the defaults, `restoreSession` off skips the session, `previewMarkdown` turns a markdown file's preview on, the app ends after the last document closes with the settings window open; the installed PrimeScroll shows the window; full release gate green
  - stop_rule: the editor's settings window and preferences.

- [ ] TODO-5536: Small, lazily loaded bytecode for apps
  - owner: ai
  - status: ready
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: bytecode-startup
  - scope: App startup is dominated by bytecode size: a program using the PNG decoder lowers to 12.7 MB of PSIR (lowering inlines almost every call) and takes ~80 ms to load on the runtime-only VM, against ~1 ms for a small program. Add a startup benchmark (bytecode size and load+run time of `embed_bytecode_runner` for a small, a medium and the PNG program) to the benchmark suite, then cut both: a size-oriented lowering mode for app bundles (real calls instead of inlining beyond a size threshold) and lazy decoding/validation of function bodies on first call.
  - acceptance:
    - the startup benchmark is in `scripts/benchmark.sh` with a baseline; the PNG program's bytecode shrinks at least 4x and loads at least 4x faster, with identical program output
    - full release gate green
  - stop_rule: bytecode size and load time only; split lazy loading out if the size change is large.

- [ ] TODO-5535: Launch-time harness and reference Objective-C++ editor
  - owner: ai
  - status: ready
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-launch
  - scope: A macOS launch harness (`tools/launch_bench/`) that starts an app N times, cold and warm, and records the time to its first on-screen window (window-list polling) and to the first drawn text (an `os_signpost` the app emits), plus a reference editor written directly in Objective-C++ with the same UI and startup rules; compare the PrimeStruct editor, the reference and installed editors (TextEdit and others).
  - acceptance:
    - the harness prints a table per app; the owner records a Mac run in docs/NativeUiPlan.md section 1, and the PrimeStruct editor meets the budget or gets follow-up TODOs for each gap
    - full release gate green
  - stop_rule: measurement tooling and the reference editor only.
  - notes: needs a Mac to run.

- [ ] TODO-5530: Read and write whole files as String
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5540
  - created_at: 2026-10-08
  - phase: Strings and slices (docs/spec/strings-and-views.md)
  - parallel_track: file-text
  - scope: Programs cannot read a file into text. Add `readText(path) -> Result<String, FileError>` and `writeText(path, [string] text) -> Result<FileError>` to `/std/file`, built on the owned `String` and byte reads (no VM-only run-time string), and document them in docs/spec/errors-and-file-io.md.
  - acceptance:
    - tests on VM, native and C++ read empty, ASCII, multi-byte UTF-8, multi-line and missing files, write them back and compare; invalid UTF-8 reports a `FileError`
    - full release gate green
  - stop_rule: whole-file text read and write only.

- [ ] TODO-5533: Windows and Linux native UI backends
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-platforms
  - scope: Implement the version-0 ABI with Win32 common controls and with GTK 4, following the coverage matrix in docs/NativeUiPlan.md section 9; split per platform before starting.
  - acceptance:
    - the editor scenarios pass on each platform by hand; the coverage matrix is updated to what each backend does
    - full release gate green
  - stop_rule: split into one leaf per platform before implementation.
  - notes: deferred: after the macOS editor works.

- [ ] TODO-5527: Stdlib Result locals destroy their payload once
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: lifecycle-result-payloads
  - scope: Sum locals now destroy their active payload at scope end, except stdlib `Result` sums: `[Result<Noisy, i32>] r{...}` never destroys an ok `Noisy` (or a `Vector` payload's buffer), because `try(r)` and `[Noisy] v{try(r)}` copy the payload out without a copy helper, so destroying `r` too would destroy twice. Make `try` on a Result local copy (or move out and disarm) the payload, then register Result locals like other sums, and copy Result payloads taken from a place.
  - acceptance:
    - matrix cases with struct and `Vector` ok payloads read through `try`, `pick` and `Result.error` destroy each payload once on VM, native and C++
    - full release gate green
  - stop_rule: stdlib Result payload ownership only.

- [ ] TODO-5524: Int-backed error structs round-trip through stdlib Result sums
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-error-structs
  - scope: With `/std/result/*` imported, a sum payload of `ContainerError`/`ImageError`/`GfxError` is stored as a scalar i32 (`valueKindFromTypeName` treats them as int-backed), but `[error] ContainerError{2i32}` stores the struct's address instead of its code; `Result.why(r)` passes the payload slot to `why()` as if it were struct storage (prints `container error` for a missing key); `pick(r) { error(e) { e.code } }` fails with "field access requires struct receiver"; the `on_error` handler receives the address as the code (`err.code` prints 560). Pick one payload storage (the code, or inline struct storage) and use it in construction, `why`, `pick` and `try`.
  - acceptance:
    - matrix cases on VM, native and C++: `error<i32, ContainerError>(ContainerError{2i32})` read through `pick`, `Result.why`, `try` with an `on_error` handler, and `m.tryAt(missing)` through `Result.why`
    - full release gate green
  - stop_rule: int-backed error struct payloads only.

- [ ] TODO-5523: A Result-returning main exits with its error code
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-main
  - scope: `[return<Result<i32, i32>>] main() { return(error<i32, i32>(3i32)) }` exits 0 on VM/C++ and 32 on native: the entry returns the raw sum pointer (or, without the import, the packed value whose low 32 bits are 0). Specify the exit code of a Result-returning entry (0 for ok, the error code or 1 otherwise) in docs/spec and decode the entry's return value on every backend.
  - acceptance:
    - spec states the rule; matrix cases for ok, int error and struct error agree on VM, native and C++
    - full release gate green
  - stop_rule: entry-point Result exit codes only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5525: `Result.ok(x)` passes as a stdlib Result argument
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-arguments
  - scope: With `/std/result/*` imported, `show(Result.ok(3i32))` for `show([Result<i32, i32>] r)` fails to lower ("struct parameter type mismatch: expected /std/result/Result__..., got <unknown>") because `Result.ok` in argument position still produces the packed value; `/std/result/ok<i32, i32>(3i32)` works. Construct the sum when a legacy `Result.ok` call feeds a stdlib Result parameter (or any sum-typed slot).
  - acceptance:
    - matrix case passing `Result.ok(...)` to a stdlib Result parameter on VM, native and C++
    - full release gate green
  - stop_rule: `Result.ok` arguments only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5526: Vectors of stdlib Result values keep their elements
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: result-containers
  - scope: `vector<Result<i32, i32>>` with the Result import pushes each sum's address instead of its storage: reading `rs[0i32]` back prints `2147483647` from `pick`, and `Result.error` reads garbage (probe `vr2`/`vr3`). Store sum elements inline (like struct elements) or reject the element type with a diagnostic.
  - acceptance:
    - matrix case pushing ok and error Results and reading them back with `pick` and `Result.error` on VM, native and C++, or a diagnostic test if rejected
    - full release gate green
  - stop_rule: Result elements in vectors only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5515: Native Result.ok(Buffer) reads as ok
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: native-result
  - scope: On native, `[Result<Buffer<i32>, GfxError>] s{Result.ok(b)}` then `Result.error(s)` prints 1 (VM/C++ print 0): the payload is packed as a stack address, and native addresses exceed 2^32.
  - acceptance:
    - matrix case agrees on VM, native and C++
    - full release gate green
  - stop_rule: covered by the convention chosen in TODO-5506 if possible.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5516: Integer narrowing and float-to-int conversion agree across backends
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: numeric-conversions
  - scope: After `[i32] x{convert<i32>(4294967297i64)}`, `x` prints 1 but `x == 1i32` is false and `convert<i64>(x)` is 4294967297 (VM/native keep high bits; spec says i32 values wrap mod 2^32). Out-of-range finite float-to-int conversions give a different answer on each backend, and optexe emits C++ undefined behavior (`static_cast<uint64_t>(-1.0)`), which crashed one program. Define and implement one rule (saturate, like the VM for i32, or a runtime fault).
  - acceptance:
    - spec states the rule; matrix cases for narrowing and out-of-range conversions agree on VM, native, C++ and optexe
    - full release gate green
  - stop_rule: conversions only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5517: Runtime faults exit the same way on every backend
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: runtime-faults
  - scope: Integer division by zero exits 3 with a message on the VM but raises SIGFPE on native and C++ (losing buffered output on C++); C++ reports runtime errors with exit 1 instead of 3; native at 100000 recursion levels segfaults instead of reporting overflow. vm-design.md lists division by zero as a runtime fault with exit 3.
  - acceptance:
    - division by zero and the C++ runtime-error exit code match the VM (exit 3, message on stderr)
    - full release gate green
  - stop_rule: fault reporting only; recursion limits stay backend-specific but documented.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5518: Valid programs the frontend rejects compile
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: frontend-accept
  - scope: Rejected though valid per the spec: an unused `import /std/result/*` or `/std/maybe/*` (`unknown import path`, no location); `return(move(v))` and `return(Holder{move(n)})` (`use-after-move`); an `if` yielding struct values (`if branches must return compatible types`); sum-typed struct fields (`missing struct field info`); `try(...)` as an argument to a user function (`missing on_error for ? usage`); `return([ok] value)` and `return(Result<i32,i32>{[error] 7i32})`; `assign(x, Val{none})` / `x = Val{[num] 3i32}` on a sum-typed local (`assign requires matching struct value`).
  - acceptance:
    - each form compiles and runs; semantics tests
    - full release gate green
  - stop_rule: acceptance of these forms; split per form if needed.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5519: Operator rewriting handles calls, nested `!` and `?` precedence
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: text-filter
  - scope: `-id(5i32)` becomes `negate(id)(5i32)`; `print_line(!(id(!true)))` leaves the inner `!` unrewritten (`invalid character '!'`); `x? + 1i32` and `x? * 3i32` are parse errors and `1i32 + x?` parses as `plus(1, x)?`; `try(f()).field` and `f()?.field` fail with `field access requires struct receiver`.
  - acceptance:
    - each form rewrites to the expected canonical call; text-filter tests
    - full release gate green
  - stop_rule: text filter only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5520: Diagnostics point at user code with readable names
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: diagnostics
  - scope: Many lowering errors carry no file/line (`missing on_error for ? usage`, `requires int-backed stdlib Result error payloads`, `missing semantic-product ...`, `argument count mismatch for /onErr`); C++ errors say `native backend`; messages show mangled names (`Result__arity2__ta77c...`, `Vector__t...`); source snippets show the text-filtered form instead of what the user wrote; an unknown `break` is reported inside stdlib/std/file/file.prime; `borrowed binding: v (root: v, sink: v)` names the root as the sink.
  - acceptance:
    - each listed diagnostic has a user-file location, the user's spelling and demangled names; diagnostics tests
    - full release gate green
  - stop_rule: diagnostic text and spans only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5521: Math helpers return correct results
  - owner: ai
  - status: deferred
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: stdlib-math
  - scope: Consistent on all backends but wrong: `atan2(1, -1)` gives 2.2749 (2.3562), `copysign(3, -0.0)` gives +3, `round(0.49999999999999994)` gives 1, `fma` is not fused.
  - acceptance:
    - each returns the IEEE/libm result on VM, native and C++; matrix cases
    - full release gate green
  - stop_rule: stdlib math only.
  - notes: deferred: queued behind the Ready Now cap.

- [ ] TODO-5483: Verify arm64 SextI32 on a macOS machine
  - owner: ai
  - status: deferred
  - deferred_reason: needs an arm64 macOS machine; the Linux x86_64 session cannot run `Arm64Emitter` output.
  - created_at: 2026-10-03
  - phase: Optimizing backends
  - parallel_track: ir-semantics
  - scope: `Arm64Emitter::emitSextI32` (SXTW x0, w0, encoded 0x93407C00) was written from the encoding and never executed. The i32 builtins audit is done: increment, decrement, abs and pow emit SextI32, and integer lerp, saturate, clamp, min, max and sign already agree at the limits on every backend (matrix cases `i32_wrap_builtins` and `i32_limit_builtins`; lerp and clamp go through plus/minus/multiply, which wrap).
  - acceptance:
    - the `i32` matrix cases (`i32_wrap_basic`, `i32_wrap_loops`, `i32_wrap_builtins`, `i32_limit_builtins`) pass on arm64 macOS native; an encoding unit test pins the SXTW bytes (done: `primestruct.ir.native_codegen` checks SXTW and the float-compare branch conditions through `primec/testing/NativeEmitterEncodings.h`; the float compares now use MI/LS so NaN compares false, also unexecuted)
  - stop_rule: do not change i64/u64 behavior or the I32 arithmetic opcodes themselves; lowering also uses them for address arithmetic.

- [ ] TODO-5543: Repair or retire the disabled Apple/arm64 native backend test shards
  - owner: ai
  - status: deferred
  - deferred_reason: needs an arm64 macOS machine to run; the Linux x86_64 session compiles these suites out.
  - created_at: 2026-10-08
  - phase: macOS support
  - parallel_track: native-arm64-tests
  - scope: `primestruct.compile.run.native_backend.{core,math_numeric,collections}` are compiled only on Apple/arm64 and were never registered with CTest, so they drifted. They are now registered for Apple/arm64 in `CMakeLists.txt` (shards `*_apple_core_*`, `*_apple_math_numeric_*`, `*_apple_collections_*`), and the 30 shards listed in `PrimeStructAppleNativeKnownFailingShards` are `DISABLED`: stale expected diagnostics (`compile ... == 2` with a changed message), native arm64 exit-code mismatches in `math_numeric_types.h`, and collection shadow/shim cases. Run each disabled shard on a Mac, then fix the expectation, fix the native arm64 backend, or delete the case, and remove the shard from the list.
  - acceptance:
    - `PrimeStructAppleNativeKnownFailingShards` is empty and all `apple_*` shards pass on arm64 macOS
    - full release gate green on Linux and macOS
  - stop_rule: tests and arm64 native emitter output only; no language semantics changes.

- [ ] TODO-5544: Method calls on `pick` payload bindings lower
  - owner: ai
  - status: deferred
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: pick-bindings
  - scope: `pick(app.waitEvent()) { windowCloseRequested(w) { w.close() } }` with `[public struct] Window` payloads passes semantics but VM lowering fails with `missing semantic-product method-call target: /main -> close`; copying the payload first (`[Window] target{w}`, then `target.close()`) works. Publish the method-call target for pick payload bindings (and make `Type.staticHelper()` resolve through `import`, which `App.start(...)` does not).
  - acceptance:
    - a pick payload struct binding accepts method calls in VM, native and C++; a negative test keeps the unknown-method diagnostic
    - docs/spec/stdlib-reference.md "Native UI" drops the copy-first note; full release gate green
  - stop_rule: method-call target publication for pick bindings and imported static helpers only.

- [ ] TODO-5545: Record the macOS AppKit manual smoke checklist
  - owner: human
  - status: deferred
  - deferred_reason: needs a person at a Mac keyboard; keystrokes, menu shortcuts and modal panels cannot be driven by the automated smoke test.
  - created_at: 2026-10-08
  - phase: Native UI (docs/NativeUiPlan.md)
  - parallel_track: native-ui-macos-manual
  - scope: Run `scripts/bundle_macos_app.sh examples/native_ui/hello_window.prime Hello`, open `build-release/apps/Hello.app` and check by hand: typing marks the window edited, Cmd+S and the Edit menu items work (undo, copy, paste, select all, find), the open and save panels and the alert answer correctly, Cmd+Q and the close button reach the program as events. Record the result in docs/NativeUiPlan.md section 12.
  - acceptance:
    - each item is recorded as passed or filed as a follow-up TODO
  - stop_rule: recording the manual run only.
