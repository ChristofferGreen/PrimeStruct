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
| TODO-5727 | Every struct copy site tells a place from a temporary by resolution | deferred | lifecycle-copy-sources |
| TODO-5723 | Shared UI event structs for web and native handlers | ready | ui-events |
| TODO-5722 | Web controls take click closures | blocked | web-controls |
| TODO-5724 | Web hover and key handlers through the fragment script | blocked | web-controls |
| TODO-5725 | Native pointer and key events for control handlers | blocked | native-ui-events |
| TODO-5706 | Fragment-swapping buttons without user JavaScript | ready | web-fragments |
| TODO-5707 | `Function` types accept `mut` parameters | deferred | closures-mut-params |
| TODO-5708 | Type aliases | deferred | type-aliases |
| TODO-5709 | Native controls take handler closures and `App.run` | blocked | native-ui-handlers |
| TODO-5710 | PrimeScroll dispatches through handlers | blocked | native-ui-handlers |
| TODO-5711 | Retire the `/std/ui` layout and command-list prototype | ready | ui-prototype-retire |
| TODO-5688 | Calls require the callee's whole active effect set | deferred | effects-transitive |
| TODO-5656 | Thread-safe `SharedString` (atomic count, per-thread string heaps) | deferred | string-concurrency |
| TODO-5603 | Startup audit: what a launch loads and does | ready | startup |
| TODO-5586 | Native String heap: pooled blocks, statistics and limit | deferred | native-backend-strings |
| TODO-5536 | Small, lazily loaded bytecode for apps | ready | bytecode-startup |
| TODO-5535 | Launch-time harness and reference Objective-C++ editor | ready | native-ui-launch |
| TODO-5533 | Windows and Linux native UI backends | deferred | native-ui-platforms |
| TODO-5712 | A callable field of a local struct is called | ready | closures-field-calls |
| TODO-5713 | A lambda whose capture list starts with `mut` parses as a call argument | ready | closures-parse |
| TODO-5714 | Native: a `Function` call nested in another call's arguments lowers | ready | native-backend |
| TODO-5715 | `Function<R, A...>{move(closure)}` infers the boxed type | ready | closures |
| TODO-5716 | A literal converts to a `StringView` at a call whose overloads differ there | deferred | text-literal-views |
| TODO-5717 | One file imports two modules that export the same name | ready | import-conflicts |
| TODO-5718 | All values of a repeated parameter and decoded path parameters | deferred | web-params |
| TODO-5719 | `location(this.field)` passes directly as a `Function` call argument | ready | closures-arguments |
| TODO-5721 | A string converts to a `StringView` at a field of a generic struct | ready | text-literal-views |
| TODO-5681 | A struct field built from a named vector local owns its storage once | ready | collections-fields |
| TODO-5726 | An indexed element of a stdlib `Vector<T>` of structs binds without a fault | ready | collections-access |
| TODO-5728 | `slice` and `sliceMut` on a vector field of a struct resolve | ready | collections-access |
| TODO-5729 | The web example loopback tests run in their own CTest entries | ready | web-tests |
| TODO-5682 | Native: a mutating vector-field method inside an arithmetic expression returns the right value | ready | native-backend |
| TODO-5676 | Lambdas can state their return type | deferred | closures-return-types |
| TODO-5677 | A `Function` whose closure holds a same-typed `Function` copies and destroys | deferred | closures-lifecycle |
| TODO-5510 | `return` returns from unit-variant pick arms correctly | ready | control-returns |
| TODO-5524 | Int-backed error structs round-trip through stdlib Result sums | ready | result-error-structs |
| TODO-5523 | A Result-returning main exits with its error code | deferred | result-main |
| TODO-5525 | `Result.ok(x)` passes as a stdlib Result argument | deferred | result-arguments |
| TODO-5526 | Vectors of stdlib Result values keep their elements | deferred | result-containers |
| TODO-5511 | Safe code cannot reach container storage or unsafe stdlib helpers | deferred | safety-stdlib |
| TODO-5512 | Pointers and aliases count as borrows of their root | deferred | safety-borrows |
| TODO-5513 | Methods through a dereferenced vector pointer read the right fields | deferred | collections-access |
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

- TODO-5706 (web-fragments): fragment-swapping buttons without user JavaScript
- TODO-5603 (startup): startup audit: what a launch loads and does
- TODO-5536 (bytecode-startup): small, lazily loaded bytecode for apps
- TODO-5535 (native-ui-launch): launch-time harness and reference Objective-C++ editor
- TODO-5510 (control-returns): `return` returns from unit-variant pick arms correctly
- TODO-5524 (result-error-structs): int-backed error structs round-trip through stdlib Result sums
- TODO-5723 (ui-events): shared UI event structs for web and native handlers
- TODO-5728 (collections-access): `slice` and `sliceMut` on a vector field of a struct resolve

### Immediate Next 10

1. TODO-5706
2. TODO-5723
3. TODO-5603
4. TODO-5536
5. TODO-5535
6. TODO-5510
7. TODO-5524
8. TODO-5728

### Priority Lanes

- Strings and slices (docs/spec/strings-and-views.md; first leg of the PrimeScroll native goal below): TODO-5656
- Native PrimeScroll, direct native backend (docs/NativeUiPlan.md): TODO-5586
- - Native UI (docs/NativeUiPlan.md): TODO-5536, TODO-5535, TODO-5533
- Startup (docs/NativeUiPlan.md section 16): TODO-5603
- Effects (docs/spec/language-core.md, Effects): TODO-5688
- Result and control flow (docs/spec/errors-and-file-io.md): TODO-5510, TODO-5524, TODO-5515, TODO-5523, TODO-5525, TODO-5526
- Memory safety (docs/spec/type-system.md Memory safety): TODO-5511, TODO-5512, TODO-5513
- Backend parity: TODO-5516, TODO-5517
- Frontend and diagnostics: TODO-5518, TODO-5519, TODO-5520, TODO-5521
- macOS support: TODO-5543, TODO-5545 (need macOS)
- Language gaps found by native UI: TODO-5544
- Closures (docs/spec/value-lifecycle.md, Lambdas): TODO-5676, TODO-5677, TODO-5712, TODO-5713, TODO-5714, TODO-5715, TODO-5719; collection fields found by closures and the todo example: TODO-5681, TODO-5682, TODO-5726, TODO-5727, TODO-5728
- Web apps (docs/WebAppsPlan.md; single-threaded, built with `--emit=optexe`): TODO-5706, TODO-5729, TODO-5718, TODO-5722, TODO-5724
- Language surface for handlers (docs/WebAppsPlan.md): TODO-5707, TODO-5708, TODO-5716, TODO-5717, TODO-5721
- Native UI handlers (docs/WebAppsPlan.md): TODO-5723, TODO-5709, TODO-5710, TODO-5711, TODO-5725
- Optimizing backends (docs/OptimizingBackendsPlan.md): flags ; IR dump ; benchmarks ; test matrix; arm64 SextI32 TODO-5483 (needs macOS); VM speed ; passes ; optexe

### Execution Queue

Run `ready` leaves in the order listed under Immediate Next 10. Lanes are independent except where a leaf names `blocked_on`; `Ready Now` is capped at eight.

### Task Blocks

- [ ] TODO-5727: Every struct copy site tells a place from a temporary by resolution
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Lifecycle (found while fixing TODO-5720)
  - parallel_track: lifecycle-copy-sources
  - scope: TODO-5720 added `isTemporaryStructCopySourceExpr` (src/ir_lowerer/IrLowererFlowControlHelpers.cpp), which resolves a call before deciding whether a struct value is a place to copy or a temporary to move; bindings, assignments, returns, brace fields and sum payloads use it. Parameter and packed arguments, the collection receiver, sum mapping, `pick` and the method-call form of a sum payload still classify syntactically, so a user-defined `at` method whose result feeds one of them would be copied and its temporary leaked. Route those sites through the same resolver.
  - acceptance:
    - matrix cases with a user `at` method feeding each listed site destroy every value exactly once on VM, native and optexe
    - full release gate green
  - stop_rule: copy-source classification at the listed sites only.

- [ ] TODO-5723: Shared UI event structs for web and native handlers
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Native UI handlers (docs/WebAppsPlan.md)
  - parallel_track: ui-events
  - scope: Add `/std/ui/events` (`stdlib/std/ui/events/events.prime`) with the event values every control handler receives (docs/WebAppsPlan.md, principle 7): `PointerEvent` (`button` as a `PointerButton` enum: none, primary, secondary, middle; control-local `x`, `y`; `modifiers`; `clickCount`), `KeyEvent` (`key` as a `Key` value: named keys and characters; `modifiers`; `repeat`), `ChangeEvent` for check boxes, popups and lists (the new value or index), and `Modifiers` (shift, control, alt, meta). The retired `/std/ui` prototype's `UiEventStream` records (stdlib/std/ui/ui.prime) had the same fields and modifier bits; reuse their meaning. Pure data plus small helpers (`modifiers.shift()`), no host calls, so it runs on every backend. Document in docs/spec/stdlib-reference.md.
  - acceptance:
    - a VM and optexe test builds each event, reads every field and compares enum values
    - full release gate green
  - stop_rule: the event types only; no dispatch.

- [ ] TODO-5722: Web controls take click closures
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5723
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md)
  - parallel_track: web-controls
  - scope: A button on a web page should take its click handler directly, a closure that may have its own `mut` state (docs/WebAppsPlan.md, principle 7), instead of being joined to a hand-routed POST path by a string. Add a page layer over `AppRouter<State>` (`stdlib/std/http/http.prime`): controls are registered once at setup, `app.button(id, title, handler)` with `handler` a `Function<void, Reference<State>, PointerEvent>`, each under a generated route (for example `POST /_on/<id>/click`); rendering writes the control as a form posting there (post/redirect/get), and the router calls the stored closure with the state and a `PointerEvent` (primary button, no position, for a plain form POST). Registration at setup is required: a closure made while rendering would lose its `mut` captures every request. Rework examples/apps/todo_web or add examples/apps/hello_button (text on a page, a button that adds a line per click) to use it, with a loopback test, and add the CodeExamples entry.
  - acceptance:
    - a closure with a `[mut said{0i32}]` capture counts clicks across requests in a loopback test; the page HTML after each click is checked
    - VM and optexe tests for registration, the generated routes and a duplicate id
    - full release gate green
  - stop_rule: click handlers over form POSTs; hover and key events are TODO-5724.

- [ ] TODO-5724: Web hover and key handlers through the fragment script
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5722
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md)
  - parallel_track: web-controls
  - depends_on: TODO-5706
  - scope: Extend the page layer of TODO-5722 with `onMouseOver`, `onMouseDown`/`onMouseUp` and `onKeyDown`/`onKeyUp` (the last only while the control has focus), each taking the matching struct of `/std/ui/events`. These events have no HTML-only form, so the fragment script of TODO-5706 sends them with the event's fields and swaps the returned fragment. Each event is a round trip: document that purely visual hover belongs in CSS, and debounce pointer moves in the script.
  - acceptance:
    - a test drives the script's request format for each event kind and checks the decoded `PointerEvent`/`KeyEvent` and the fragment answer
    - the script stays within the size budget of TODO-5706
    - full release gate green
  - stop_rule: the event kinds listed; no drag and drop, no touch.

- [ ] TODO-5725: Native pointer and key events for control handlers
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5709
  - created_at: 2026-11-07
  - phase: Native UI handlers (docs/WebAppsPlan.md)
  - parallel_track: native-ui-events
  - scope: The C ABI (include/primec/ui/NativeUi.h, version 0) reports only menu commands, control changes, window close and text changes. Add hover enter and leave, mouse down and up with button, position and modifiers, and key down and up for the focused control, as a versioned ABI extension, in the AppKit and headless backends, and add `onMouseOver`, `onMouseDown`, `onMouseUp`, `onKeyDown`, `onKeyUp` to the `/std/ui/native` controls with `/std/ui/events` structs. Text views keep typing inside the widget; key handlers on them see only keys the widget does not consume.
  - acceptance:
    - headless scenarios deliver each event kind to a closure and check the struct fields; the compiled program gives the same call log
    - docs/NativeUiPlan.md section 4 and the coverage matrix describe the new events
    - full release gate green
  - stop_rule: the listed events on the existing controls; no new widgets.

- [ ] TODO-5729: The web example loopback tests run in their own CTest entries
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md; found by TODO-5705)
  - parallel_track: web-tests
  - scope: The ctest `PrimeStruct_net` runs the whole primestruct.backend.net suite, which builds five optexe programs (net_echo twice, http_hello, web_app, todo_web, serve_file); todo_web alone takes about 250 s (the frontend about 50 s, clang++ about 80 s of CPU plus load). On 2026-11-07 the suite took 365 s alone and 879 s and over 900 s (timeout) in two release gates with load averages up to 90, so it fails under load. Register the example tests (http_hello, web_app, todo_web) as separate CTest entries filtered by `--test-case`, like PrimeStruct_string_conformance_optexe, and keep the registration guard and inventory in sync.
  - acceptance:
    - each example's loopback test is its own ctest entry with its own timeout, and the union still covers every primestruct.backend.net case (scripts/check_test_registration.py passes)
    - full release gate green
  - stop_rule: CTest registration of the net suite only; no change to the tests themselves.

- [ ] TODO-5706: Fragment-swapping buttons without user JavaScript
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md)
  - parallel_track: web-fragments
  - scope: Add the fragment layer of docs/WebAppsPlan.md (principle 4): `/std/html` attributes that name a route and a target element, one small fixed script served by a stdlib route, and responses that are HTML fragments. A button click calls a PrimeStruct handler and replaces part of the page. Without the script the same forms still work by post/redirect/get.
  - acceptance:
    - the todo example toggles an item through a fragment; a test checks the fragment bytes and that the plain form path still works
    - the script is a fixed asset under stdlib/ with a size budget stated in the spec
    - full release gate green
  - stop_rule: the attributes, the script and one example use; no client-side state or templating.

- [ ] TODO-5707: `Function` types accept `mut` parameters
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md)
  - parallel_track: closures-mut-params
  - scope: Handlers stored in a `Function` take `Reference<Response>` because a `Function<R, A...>` type cannot say that a parameter is `mut`, while a generic handler can take `[Response mut]`. Specify a spelling for a mutable parameter in a `Function` type in docs/spec/value-lifecycle.md and lower it. Deferred: needs the spelling decided.
  - acceptance:
    - a `Router` route lambda takes `[Response mut] response` and runs on VM, native and optexe; spec text with a runnable example
    - full release gate green
  - stop_rule: the parameter mode in `Function` types only.

- [ ] TODO-5708: Type aliases
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md)
  - parallel_track: type-aliases
  - scope: There is no way to name a type: `Function<void, Request, Reference<Response>>` is spelled out at every route and seven times in /std/http. Specify a public/private alias declaration (docs/spec/type-system.md), with generic aliases if cheap, and use it for the handler type. Deferred: needs the spelling decided.
  - acceptance:
    - an alias of a `Function` type and of a generic struct is used in parameters, fields and construction; a negative test for a cycle; spec text with a runnable example
    - full release gate green
  - stop_rule: alias declarations and their resolution only.

- [ ] TODO-5709: Native controls take handler closures and `App.run`
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5723
  - created_at: 2026-11-07
  - phase: Native UI handlers (docs/WebAppsPlan.md)
  - parallel_track: native-ui-handlers
  - scope: Native UI events reach code as integer command ids and widget handles compared in an if-chain (examples/apps/text_editor/main.prime, the `pick(app.waitEvent())` loop). Give the `/std/ui/native` controls handler closures over the events ABI version 0 already reports (docs/WebAppsPlan.md, principle 7): `Button.onClick`, `Menu.item(title, shortcut, handler)` and toolbar items (`PointerEvent`), `CheckBox.onChange`, `Popup.onChange`, `ItemList.onChange` (`ChangeEvent`), and window close and text change. Each handler is a `Function<void, Reference<State>, Event>`, so a closure may keep its own `mut` state while shared state comes in as the parameter. `App.run(state)` allocates command ids, keeps the handler table and dispatches until quit; app code never sees an id. `waitEvent` stays. Document in docs/NativeUiPlan.md and docs/spec/stdlib-reference.md.
  - acceptance:
    - examples/native_ui/hello_window.prime has a handler form with a counting `[mut]` closure on a button; it runs on the VM against the headless backend and as a compiled program with the same call log
    - full release gate green
  - stop_rule: handlers over the existing C ABI events; pointer and key events are TODO-5725.
- [ ] TODO-5710: PrimeScroll dispatches through handlers
  - owner: ai
  - status: blocked
  - blocked_on: TODO-5709
  - created_at: 2026-11-07
  - phase: Native UI handlers (docs/WebAppsPlan.md)
  - parallel_track: native-ui-handlers
  - scope: Move the text editor's menu, toolbar and settings-window dispatch from the if-chains on command ids and handles to the handlers of TODO-5709, one menu at a time.
  - acceptance:
    - every editor scenario (docs/NativeUiPlan.md section 13) leaves the same call log, exit code and files on the VM and the native build
    - the dispatch loop in main.prime is gone or reduced to what has no handler form
    - full release gate green
  - stop_rule: the editor's dispatch only; no behavior change.

- [ ] TODO-5711: Retire the `/std/ui` layout and command-list prototype
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Native UI handlers (docs/WebAppsPlan.md)
  - parallel_track: ui-prototype-retire
  - scope: `stdlib/std/ui/ui.prime` (`LayoutTree`, `CommandList`, `HtmlCommandList`, `UiEventStream`, the login form) has no user: no app imports it and its HTML records have no consumer. Delete it with its tests (`test_compile_run_vm_core_ui.cpp`, the `core_ui_layout_*` native shards and the cases in `test_compile_run_vm_core_basic.cpp` that only exercise it), drop its sections from docs/Graphics_API_Design.md and docs/spec/architecture.md, and remove `stdlib/std/ui/*` from the style-aligned list in AGENTS.md and docs/CodeExamples.md (keeping `/std/ui/native`). Lowerer tests that only borrow its struct shapes keep a local copy of the shape.
  - acceptance:
    - nothing references the removed names; the test inventory and registration checks pass
    - full release gate green
  - stop_rule: deletion and doc alignment only.

- [ ] TODO-5688: Calls require the callee's whole active effect set
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Effects
  - parallel_track: effects-transitive
  - scope: TODO-5687 checks only the effects a callee declares. A callee without `[effects]` runs with the default set (`io_out`), so the full rule would also require the caller to hold the callee's defaults. Measured with the TODO-5687 prototype (semantics defaults `io_out io_err`), the full rule failed 527 of 2840 semantics cases (narrow rule: 47) and 9 of the 42 examples that compile (narrow rule: 1), mostly inside stdlib lifecycle helpers such as `Vector.Copy`/`Destroy` and `SoaColumn.Destroy`, which declare `heap_alloc` only and call unannotated helpers. Decide whether unannotated helpers should infer their effects from their bodies (or default to none) before turning on the full transitive rule.
  - acceptance:
    - a rule for unannotated callees in docs/spec/language-core.md (Effects), stdlib, examples and tests annotated to match, and the full rule enforced in semantics
    - full release gate green
  - stop_rule: call-site effect checks only; no new effects.

- [ ] TODO-5712: A callable field of a local struct is called
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures-field-calls
  - scope: With `[Route mut] direct{Route{[code] 6i32, [handler] double}}` and `Route` holding `[Function<i32, i32> mut] handler`, `direct.handler(2i32)` (as a statement, a binding initializer or a return argument) fails with `unknown method: /Route/handler`. The same call through an indexed element (`routes[i].handler(x)`) or `this` works (`rewriteCallableFieldCall` in src/semantics/TemplateMonomorphCallableRewrites.cpp), so the receiver of a plain local struct does not resolve there.
  - acceptance:
    - `direct.handler(2i32)` on a local, a parameter and a `Reference` to the struct calls the field on VM, native and optexe (matrix case in test_program_matrix_closures.cpp)
    - full release gate green
  - stop_rule: receiver resolution for callable fields; no change to method lookup.

- [ ] TODO-5713: A lambda whose capture list starts with `mut` parses as a call argument
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures-parse
  - scope: `router.add(3i32, [mut calls{0i32}]([i32] x) { ... })` fails with `Parse error: reserved keyword cannot be used as identifier: mut`: in argument position `[` starts an argument label. `[base](...)` and `[seed{5i32}](...)` parse, and the same lambda parses inside braces (`Function<i32, i32>{[mut calls{0i32}](...) {...}}`) and as a binding initializer. Since TODO-5669 a lambda passes straight to a `Function` parameter, so this is the remaining reason to bind a stateful handler first (examples/apps/http_hello `/greet`). Disambiguate a label from a capture list when `[` is followed by a parameter list and a body.
  - acceptance:
    - a `mut`-capture lambda and a `[mut x, y]` lambda as call arguments parse and run on VM and optexe; labeled arguments still parse; a parser test for each
    - full release gate green
  - stop_rule: argument-position lambda parsing only.

- [ ] TODO-5714: Native: a `Function` call nested in another call's arguments lowers
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: native-backend
  - scope: `apply_twice([Function<i32, i32> mut] f, [i32] x) { return(f(f(x))) }` fails on `--emit=native` with `native backend does not support recursive calls: /__Function1__t.../call`, with the explicit `Function<i32, i32>{...}` too; the VM and optexe return the right value, and `[i32] once{f(x)} return(f(once))` works on native. The native lowering treats a call of `call` inside the arguments of the same `call` as recursion.
  - acceptance:
    - `f(f(x))` on a `Function` parameter returns the right value on native-O0 and native-O2 (matrix case)
    - full release gate green
  - stop_rule: the native recursion check for nested calls of one definition.

- [ ] TODO-5715: `Function<R, A...>{move(closure)}` infers the boxed type
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures follow-ups (found by TODO-5669)
  - parallel_track: closures
  - scope: `Function<i32, i32>{move(adder)}` with `adder` a closure local fails with `unable to infer implicit template arguments for /__fn_box`: implicit template inference does not see through `move(...)`. A copy (`Function<i32, i32>{adder}`) works. Once it infers, accept `move(name)` at the implicit conversion sites too (`isClosureArgument` in src/semantics/TemplateMonomorphCallableRewrites.cpp).
  - acceptance:
    - the explicit and the implicit form with `move(adder)` run on VM and optexe, and `adder` is moved-from afterwards (a negative test for a use after the move)
    - full release gate green
  - stop_rule: `move` arguments of `/__fn_box` and the implicit sites.

- [ ] TODO-5716: A literal converts to a `StringView` at a call whose overloads differ there
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5700)
  - parallel_track: text-literal-views
  - scope: A `string` converts to a `StringView` where one is expected (docs/spec/strings-and-views.md), but at a call of same-arity overloads only when every overload takes a `StringView` in that position. With `pick([i32])` and `pick([StringView])`, `pick("y")` reports no viable overload, and even `pick(textOf("y"))` is ambiguous, because overload selection types a nested call argument as unknown (`requirementOverloadArgumentTypeText` in src/semantics/TemplateMonomorphCoreUtilities.cpp). Give a call with a known non-generic return type, such as `textOf`, its type there, then let the conversion pick the `StringView` overload when no overload takes the `string` itself (`TextViewConversion::convert`).
  - acceptance:
    - `pick("y")` and `pick(textOf("y"))` select the `StringView` overload on VM and optexe; `describe([string])` next to `describe([StringView])` still takes the literal as a `string`
    - full release gate green
  - stop_rule: overload argument facts for calls with a declared return type and the literal conversion at such calls; no change to the specificity rule.
  - notes: deferred: no stdlib family overloads `string` against `StringView` or another type today.

- [ ] TODO-5717: One file imports two modules that export the same name
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/WebAppsPlan.md; found by TODO-5701)
  - parallel_track: import-conflicts
  - scope: `/std/html` and `/std/json` both export `Writer`. `import /std/html/*` next to `import /std/json/*` (or `import /std/json/Writer`) fails with `import creates name conflict: Writer` (src/semantics/SemanticsValidatorBuildImports.cpp), even when the program names one of them by its full path, and a full path such as `/std/json/Writer` into a module that is not imported is `unsupported binding type`. A web app that serves pages and JSON needs both writers in one file. Report the conflict where the bare name is used instead of at the import, so `/std/html/Writer` and `/std/json/Writer` work side by side; an unqualified use of the shared name stays an error that names both paths.
  - acceptance:
    - a program importing both modules builds an HTML page and a JSON value through their full paths on VM and optexe
    - an unqualified `Writer` in that program is a diagnostic naming `/std/html/Writer` and `/std/json/Writer`
    - docs/PrimeStruct_SyntaxSpec.md (Imports) and docs/spec/stdlib-reference.md (HTML writer) updated
    - full release gate green
  - stop_rule: use-site conflict detection for wildcard and single-name imports; no import aliases or renaming syntax.

- [ ] TODO-5718: All values of a repeated parameter and decoded path parameters
  - owner: ai
  - status: deferred
  - created_at: 2026-11-07
  - phase: Web apps (docs/WebAppsPlan.md; found by TODO-5702 and TODO-5703)
  - parallel_track: web-params
  - scope: `queryParam` and `formParam` give the first of repeated parameters (`?tag=a&tag=b`), so a form with checkboxes or a multi-select cannot read every value; and `pathParam` returns the segment as sent, so `/files/a%20b` gives `a%20b`. Add a way to read every value of a name (a count plus an index, as the header accessors do, or a vector of Strings) and a decoded form of a path parameter with the same decoding and failure rules as `queryParam`. Defer until an example needs either.
  - acceptance:
    - VM and optexe tests: three values of a repeated query and form parameter in order; a `%20` and a malformed path segment
    - docs/spec/stdlib-reference.md updated
    - full release gate green
  - stop_rule: repeated values and decoded path segments only; no typed parameters, no multipart.

- [ ] TODO-5719: `location(this.field)` passes directly as a `Function` call argument
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Closures (docs/spec/value-lifecycle.md, Function values; found by TODO-5704)
  - parallel_track: closures-arguments
  - scope: In a method, `this.handler(location(this.state), x)`, where `handler` is a `Function<void, Reference<S>, i32>` field, fails VM lowering with `struct parameter type mismatch: expected /S, got <unknown>`; binding `[Reference<S>] state{location(this.state)}` first and passing `state` works, and `handler(location(local), x)` on a local works. `AppRouter.call` in stdlib/std/http/http.prime uses the local. Make the lowerer infer the struct type of a `location(...)` of a field in a `Function` call's arguments.
  - acceptance:
    - the direct form runs on the VM and optexe, and `AppRouter.call` drops its local
    - full release gate green
  - stop_rule: `location` arguments of `Function` calls only.

- [ ] TODO-5721: A string converts to a `StringView` at a field of a generic struct
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Language surface for handlers (docs/spec/strings-and-views.md; found by TODO-5704)
  - parallel_track: text-literal-views
  - scope: A `string` value or literal converts to a `StringView` at a brace-constructor field of a non-generic struct (TODO-5700), but not of a generic one: `AppRoute<State>{[method] method}` with a `[string] method` fails with `argument type mismatch ... expected /std/text/view/StringView`, so `AppRouter.route` wraps both in `textOf`. Insert the conversion once the instance's field types are known.
  - acceptance:
    - a generic struct's StringView field takes a literal and a `string` value without `textOf` on the VM and optexe, and `AppRouter.route` drops its `textOf`
    - full release gate green
  - stop_rule: fields of generic struct instances only.

- [ ] TODO-5681: A struct field built from a named vector local owns its storage once
  - owner: ai
  - status: ready
  - created_at: 2026-11-06
  - phase: Closures follow-ups (split from TODO-5668)
  - parallel_track: collections-fields
  - scope: `KeepV{[seen] start}` with `start` a local `vector<i32>` copies the 5-slot vector record into the field with `ownsData` still set in both, so the local and the struct both free the buffer (`VM error: invalid heap free address in IR: 9223372036854775808` on VM; same on optexe). Related: a plain local copy `[vector<i32> mut] b{a}` aliases `a` (after `b.push(2i32)`, `a.count()` is 2, not 1), while a struct copy (`[KeepV mut] copy{k}`) and the stdlib `Vector<T>` spelling copy elements. Brace construction from a named vector local must copy its elements (value semantics, docs/spec/value-lifecycle.md, Copies), and the `vector<T>` local copy must too.
  - implementation_notes: the vector record layout is `IrLowererVectorRecordLayoutHelpers.h` (`ownsData` at slot 4); the struct copy of TODO-5668's matrix case `struct_vector_field_copy_and_closure` already copies a vector field's elements.
  - acceptance:
    - `KeepV{[seen] start}` then pushes on both run on VM, native and optexe with independent counts and no double free; `b{a}` then `b.push` leaves `a.count()` unchanged; a test that the original stays usable
    - full release gate green
  - stop_rule: vector copies at brace construction and local copy bindings; no change to move semantics.

- [ ] TODO-5726: An indexed element of a stdlib `Vector<T>` of structs binds without a fault
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Collections (docs/spec/value-lifecycle.md, Copies; found by TODO-5720)
  - parallel_track: collections-access
  - scope: With `[Vector<Noisy> mut] v{vector<Noisy>()}` filled by `vectorPush<Noisy>`, where `Noisy` is a struct (one `i32` field, or an `i32` and a `bool`), `[Noisy] a{v[0i32]}`, `at(v, 0i32)` and `v.at(0i32)` stop with `VM error: unaligned indirect address in IR: 1` (or `: 2`); `vectorAt<Noisy>(v, 0i32)` binds and copies once, and the builtin `vector<Noisy>` spelling works. Present before TODO-5720; likely the builtin element access reads the stdlib Vector record with the builtin record layout.
  - acceptance:
    - the three spellings bind, copy once and destroy once on the VM and optexe (matrix case)
    - full release gate green
  - stop_rule: indexed reads of stdlib `Vector<T>` struct elements only.

- [ ] TODO-5728: `slice` and `sliceMut` on a vector field of a struct resolve
  - owner: ai
  - status: ready
  - created_at: 2026-11-07
  - phase: Collections (docs/spec/strings-and-views.md, slices; found by TODO-5705)
  - parallel_track: collections-access
  - scope: With `[Holder mut] holder{Holder{}}` where `Holder` has a field `[vector<u8> mut] bytes`, `holder.bytes.slice(0i32, 2i32)` and `holder.bytes.sliceMut(0i32, 2i32)` fail with `unknown call target: slice` / `sliceMut` (PSC1005), while `push`, `count` and indexing on the same field work and the same calls on a `vector<u8>` local or parameter work. The /std/http/server connection table keeps its buffers in a separate `vector<u8>` parameter because of this.
  - acceptance:
    - `slice` and `sliceMut` on a vector field of a local struct and of a `mut` struct parameter read and write the field's storage on the VM and optexe, with a positive and a negative (out of range) case
    - full release gate green
  - stop_rule: method resolution of the slice calls on struct fields only.

- [ ] TODO-5682: Native: a mutating vector-field method inside an arithmetic expression returns the right value
  - owner: ai
  - status: ready
  - created_at: 2026-11-06
  - phase: Closures follow-ups (split from TODO-5668)
  - parallel_track: native-backend
  - scope: With `KeepV { [vector<i32> mut] seen  add([i32] x) { this.seen.push(x)  return(this.seen.count()) } }`, `return(direct * 10i32 + m.add(6i32))` returns 18 on `--emit=native` and 22 on the VM and optexe; binding `[i32] second{m.add(6i32)}` first gives 22 everywhere. Present before TODO-5668's count fix, so the native backend likely clobbers a pending operand while lowering the inlined push.
  - acceptance:
    - the expression form returns 22 on native-O0 and native-O2 (matrix case)
    - full release gate green
  - stop_rule: operand preservation around inlined vector mutations on native.

- [ ] TODO-5676: Lambdas can state their return type
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Closures follow-ups
  - parallel_track: closures-return-types
  - scope: A lambda has no return-type spelling, and inference fails for struct and `Result` returns. `Function<Result<i32, i32>, i32>{[]([i32] x) { return(Result<i32, i32>{[ok] x * 2i32}) }}` is rejected with `unable to infer return type on /main__lambda0/call`. In a generic wrapper whose lambda returns `inner(x) + seen` with `inner` a template-typed capture, the error is `return type inference cycle requires explicit annotation on /with_logging__lambda0__t.../call`, and there is no annotation to give. Specify a return annotation in docs/spec/value-lifecycle.md, and use the `R` of `Function<R, A...>` as the expected type when a lambda is converted. Deferred: handlers that return `void` and write into a `Reference<Response>` avoid it.
  - acceptance:
    - both repros run on VM and optexe; a negative test for a mismatched annotation; spec text with a runnable example
    - full release gate green
  - stop_rule: lambda return types only.

- [ ] TODO-5677: A `Function` whose closure holds a same-typed `Function` copies and destroys
  - owner: ai
  - status: deferred
  - created_at: 2026-11-06
  - phase: Closures follow-ups
  - parallel_track: closures-lifecycle
  - scope: `with_logging([Function<i32, i32> move] next)` returning `Function<i32, i32>{[mut inner{next}, mut seen{0i32}]([i32] x) { ... inner(x) ... }}` stops at lowering with `vm backend does not support recursive calls: /__Function1__t.../Copy` (and the native/optexe equivalent). The generated `Copy` and `Destroy` of that `Function` instance call themselves, and lifecycle helpers are lowered inline (`src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp`), which rejects recursion; plain recursive functions work. Lower recursive lifecycle helpers as real calls. Deferred: generic middleware (a closure wrapping a template-typed handler) composes without erasure and runs today.
  - acceptance:
    - the repro (a two-level wrapped `Function` called, copied and destroyed) runs on VM, native and optexe with no leaked blocks
    - full release gate green
  - stop_rule: recursive generated lifecycle helpers.

- [ ] TODO-5510: `return` returns from unit-variant pick arms correctly
  - owner: ai
  - status: ready
  - created_at: 2026-10-06
  - phase: Correctness audit 2026-10
  - parallel_track: control-returns
  - scope: `pick(m) { none { return(-1i32) } num(v) { return(v) } }` in statement position does not return from the unit-variant arm (rewritten to `none{-1}`, IR `PushI32 -1; Pop`); same for `Result<E>`'s `ok { return(...) }`. (The lambda half is done: lambdas are closure structs, TODO-5663.)
  - acceptance:
    - unit-variant pick arms return; parse/semantic + matrix tests
    - full release gate green
  - stop_rule: return lowering in pick arms.

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

- [ ] TODO-5656: Thread-safe `SharedString` (atomic count, per-thread string heaps)
  - owner: ai
  - status: deferred
  - created_at: 2026-11-03
  - phase: Strings and slices (docs/spec/strings-and-views.md, Beyond std::string)
  - parallel_track: string-concurrency
  - scope: Make `SharedString` (TODO-5638) safe to send between threads: an atomic count and string blocks that may be freed on a thread other than the one that made them. Deferred until the concurrency model (docs/MultithreadingPrototype.md) has a scheduler, atomics and per-thread string heaps; scope it again then.
  - acceptance:
    - to be set when the leaf is un-deferred
  - stop_rule: not started.


- [ ] TODO-5603: Startup audit: what a launch loads and does
  - owner: ai
  - status: ready
  - created_at: 2026-10-26
  - phase: Native PrimeScroll (docs/NativeUiPlan.md)
  - parallel_track: startup
  - scope: Take stock of what PrimeScroll loads and does on the way to its first window, and keep it minimal (the user asked for this pass after the file-association work, where a framework link and system lookups had to be checked against the launch time). Measure first: the libraries dyld loads (`DYLD_PRINT_LIBRARIES=1` on a launch with `PRIMESTRUCT_UI_SNAPSHOT` set; the baseline of 2026-10-26 is 1404 lines for both the installed build and the one with the file-type code, UniformTypeIdentifiers included because AppKit loads it) and the time to the first window over repeated launches of the installed app, as a script (`scripts/measure_startup.sh`) so the numbers can be compared. Then list the work a launch does before the first window (preference and session reads, building every menu, the Info.plist, the runtime's setup: the native String heap, the host-call table, the highlighter) and move whatever can wait behind first use, as the markdown preview's web view already does. Record the before and after numbers in docs/NativeUiPlan.md.
  - acceptance:
    - `scripts/measure_startup.sh` prints the loaded-library count and the median time to the first window of a bundle over a number of launches; the plan records the baseline and what moved; a guard (a test or the script with a recorded limit) fails when a launch loads more libraries than the baseline; full release gate green
  - stop_rule: measurement, the guard and the launch-time work that can wait; no feature changes.
  - notes: system calls of a feature belong behind its window (the file associations only run when the settings window is built or a button is pressed).

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
  - notes: deferred to keep Ready Now at eight. PrimeScroll ships from the native build, so every String it makes takes a mapping of its own; the writable data segment this needs exists (`MachOData::stateBytes`).

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
