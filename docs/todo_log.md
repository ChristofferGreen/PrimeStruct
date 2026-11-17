# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.

## TODO-6027

- 2026-11-17 (TODO-6026, release build of d01dcaede plus the sharding
  changes, 11 cores, quiet machine, `ctest --parallel 1` of every entry
  above 4 s in a `--parallel 4` run): of 2,552 entries 105 took more
  than 5 s alone and the slowest was 32 s (before the split). The
  gate's quiet `--parallel 4` run took 12.5 min. Timeouts are 300 s for
  2,099 entries, 600 for 141, 900 for 146, 1800 for six and 1200 for
  one, so before the change no entry had a timeout below 16 times its
  quiet time.
- The five entries of the TODO-5896 and TODO-5897 gates, alone and
  quiet: `native_ui_compiled_app` 32 s (timeout 900),
  `native_ui_appkit_scroll_blocks` 16.5 s, `_scroll_independent` 7.3 s
  (not 80 s), `native_window_launcher_and_preflight_56_56` 1.9 s,
  `language_level_examples_80_80` 3.5 s. A loaded repro (44 and 60
  busy loops at normal priority, load up to 145, `nice -n 10` like the
  gate): launcher 21 s, language levels 28 s, scroll_independent
  26 s, the bundle fixture 72 s; at `nice -n 0` 10, 23, 9 and 34 s.
  Load alone therefore costs 5 to 15 times at the gate's priority, not
  the 150 times a 1.9 s entry needs to reach 300 s: the timeouts in
  the gates were stalls (memory pressure, Gatekeeper, disk) that this
  repro does not produce (TODO-6028).
- No polls or fixed sleeps in the AppKit scroll driver: the harness
  header says its steps wait for events (the event queue going quiet, a
  page loaded, a report written), and the only waits in
  src/ui/appkit/NativeUiAppKit.mm are a 5 s bounded wait for a web view
  snapshot callback and the 60 s default-app wait, both pumping the run
  loop. A scroll scenario costs about 1.5 s, the launch of the editor.
  Host `-O0` is no faster for these builds (the compiled editor 7.4 s
  at `-O2` and at `-O0`, a small program 1.5 s), so cheaper host builds
  were not an option.
- Changed by TODO-6026: `native_ui_compiled_app` (32 s) is four entries
  `_hello` 4.6 s, `_handlers` 10.6 s, `_editor` 14.9 s, `_bundle` 6.4 s
  (timeout 300 each); `native_ui_appkit_scroll_blocks` (16.5 s) is
  `_blocks_follow` 8.1 s and `_blocks_ends` 6.7 s, `_cursor` (11.6 s) is
  `_cursor_moves` 5.9 s and `_cursor_edits` 4.5 s;
  `cpp_host_calls` shards went from five cases (up to 19 s) to two (up
  to 7.1 s), `string_conformance_vm` from five (10 s) to three (5.7 s),
  `string_conformance_optexe` from four (12.9 s) to two (5.0 s),
  `u8_scalar_string_owned` from ten (15.3 s) to five (7.4 s).
- Entries still above 5 s alone, by family (slowest of the family,
  quiet, serial, measured before the changes above; none has a timeout
  below 16 times its time). Justification: each builds and runs one
  program (or one editor launch) on several backends, or one optexe
  program through the host C++ compiler; the entries above 10 s are
  TODO-6027, the rest stay.
  - primestruct_program_matrix_tasks: 9 entries, slowest 18.2 s alone (primestruct_program_matrix_tasks_15_15, TIMEOUT 300, 16x)
  - native_ui_startup_guard: 1 entries, slowest 14.9 s alone (native_ui_startup_guard, TIMEOUT 600, 40x)
  - primestruct_compile_run_benchmark_harness: 1 entries, slowest 13.2 s alone (primestruct_compile_run_benchmark_harness, TIMEOUT 900, 68x)
  - test_registration: 1 entries, slowest 11.8 s alone (test_registration, TIMEOUT 300, 25x)
  - native_ui_editor: 1 entries, slowest 11.4 s alone (native_ui_editor, TIMEOUT 300, 26x)
  - primestruct_compile_run_native_backend_collections_apple_collections: 4 entries, slowest 11.3 s alone (primestruct_compile_run_native_backend_collections_apple_collections_221_230, TIMEOUT 300, 26x)
  - primestruct_ir_native_codegen: 1 entries, slowest 11.2 s alone (primestruct_ir_native_codegen, TIMEOUT 300, 26x)
  - native_ui_appkit_smoke_toolbar: 1 entries, slowest 10.7 s alone (native_ui_appkit_smoke_toolbar, TIMEOUT 300, 28x)
  - primestruct_ir_optexe: 2 entries, slowest 10.6 s alone (primestruct_ir_optexe_8_8, TIMEOUT 300, 28x)
  - cpp_native_ui: 8 entries, slowest 9.7 s alone (cpp_native_ui_5_5, TIMEOUT 600, 61x)
  - http_loopback: 1 entries, slowest 9.7 s alone (http_loopback, TIMEOUT 1200, 123x)
  - primestruct_program_matrix_container_copies: 2 entries, slowest 9.4 s alone (primestruct_program_matrix_container_copies_15_15, TIMEOUT 300, 31x)
  - collection_parity_soa_part_1_of_2: 1 entries, slowest 9.2 s alone (collection_parity_soa_part_1_of_2, TIMEOUT 300, 32x)
  - embed_exports: 2 entries, slowest 8.8 s alone (embed_exports_21_29, TIMEOUT 300, 34x)
  - primestruct_program_matrix_closures_routers: 2 entries, slowest 8.6 s alone (primestruct_program_matrix_closures_routers_8_8, TIMEOUT 300, 34x)
  - collection_parity_soa_part_2_of_2: 1 entries, slowest 8.3 s alone (collection_parity_soa_part_2_of_2, TIMEOUT 300, 36x)
  - web: 3 entries, slowest 8.2 s alone (web_1_2, TIMEOUT 900, 109x)
  - native_ui_editor_acceptance: 1 entries, slowest 7.9 s alone (native_ui_editor_acceptance, TIMEOUT 300, 37x)
  - http: 1 entries, slowest 7.6 s alone (http, TIMEOUT 600, 78x)
  - primestruct_program_matrix_channels: 2 entries, slowest 7.6 s alone (primestruct_program_matrix_channels_4_4, TIMEOUT 300, 39x)
  - native_ui_appkit_scroll_linear: 1 entries, slowest 7.5 s alone (native_ui_appkit_scroll_linear, TIMEOUT 300, 40x)
  - native_ui_appkit_smoke_open: 1 entries, slowest 7.4 s alone (native_ui_appkit_smoke_open, TIMEOUT 300, 40x)
  - primestruct_program_matrix_vector_owners: 11 entries, slowest 7.3 s alone (primestruct_program_matrix_vector_owners_29_29, TIMEOUT 300, 41x)
  - native_ui_appkit_scroll_independent: 1 entries, slowest 7.3 s alone (native_ui_appkit_scroll_independent, TIMEOUT 300, 41x)
  - native_ui_appkit_smoke_editor_views: 1 entries, slowest 7.1 s alone (native_ui_appkit_smoke_editor_views, TIMEOUT 300, 42x)
  - net_hello_button: 1 entries, slowest 6.9 s alone (net_hello_button, TIMEOUT 1800, 260x)
  - primestruct_program_matrix_text_views: 1 entries, slowest 6.7 s alone (primestruct_program_matrix_text_views_4_4, TIMEOUT 300, 44x)
  - native_ui_appkit_smoke_editor_scroll_uneven: 1 entries, slowest 6.6 s alone (native_ui_appkit_smoke_editor_scroll_uneven, TIMEOUT 300, 45x)
  - net: 1 entries, slowest 6.6 s alone (net, TIMEOUT 600, 90x)
  - file_listing_optexe: 1 entries, slowest 6.3 s alone (file_listing_optexe, TIMEOUT 900, 142x)
  - primestruct_program_matrix_closures_references: 1 entries, slowest 6.3 s alone (primestruct_program_matrix_closures_references_8_8, TIMEOUT 300, 47x)
  - primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16: 1 entries, slowest 6.0 s alone (primestruct_compile_run_vm_collections_collections_newly_exposed_2026_07_16_429_432, TIMEOUT 300, 50x)
  - primestruct_program_matrix_shared_strings: 1 entries, slowest 5.9 s alone (primestruct_program_matrix_shared_strings_5_5, TIMEOUT 300, 50x)
  - native_ui_appkit_smoke_editor_scroll: 1 entries, slowest 5.9 s alone (native_ui_appkit_smoke_editor_scroll, TIMEOUT 300, 50x)
  - semantic_memory_benchmark: 1 entries, slowest 5.8 s alone (semantic_memory_benchmark, TIMEOUT 1800, 310x)
  - net_todo_web: 1 entries, slowest 5.8 s alone (net_todo_web, TIMEOUT 1800, 310x)
  - primestruct_compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2: 1 entries, slowest 5.8 s alone (primestruct_compile_run_vm_outputs_ir_and_output_modes_argv_arrays_77_86_part2_49_49, TIMEOUT 300, 51x)
  - primestruct_program_matrix_closures_fields: 2 entries, slowest 5.7 s alone (primestruct_program_matrix_closures_fields_3_3, TIMEOUT 300, 52x)
  - primestruct_compile_run_imports_operations_and_collections_part2: 1 entries, slowest 5.6 s alone (primestruct_compile_run_imports_operations_and_collections_part2_111_112, TIMEOUT 300, 53x)
  - native_ui_appkit_smoke_run: 1 entries, slowest 5.6 s alone (native_ui_appkit_smoke_run, TIMEOUT 300, 53x)
  - semantic_memory_definition_worker_parity: 1 entries, slowest 5.4 s alone (semantic_memory_definition_worker_parity, TIMEOUT 1800, 333x)
  - http_optexe: 2 entries, slowest 5.4 s alone (http_optexe_5_6, TIMEOUT 900, 166x)
  - primestruct_program_matrix_element_receivers: 1 entries, slowest 5.3 s alone (primestruct_program_matrix_element_receivers_7_7, TIMEOUT 300, 56x)
  - http_workers: 1 entries, slowest 5.2 s alone (http_workers, TIMEOUT 600, 115x)

## TODO-6019

- 2026-11-17 (found by TODO-5920): `examples/apps/text_editor/make.prime`
  against `scripts/build_native_app.sh` (appkit, release build
  directory, one worktree, nothing launched). Same: the generated C++
  (`primescroll.cpp`, 1,153,458 bytes, SHA-256 3b0b0cb3...) and the
  `otool -L` library list of the executables. Differences: (1) the
  runner always passes `-O<n>`; without one `primec` optimizes a native
  program at level 2 (src/bin/main.cpp), so make.prime says
  `cc.optimize(2i32)`; with the default 0 the executable is 1.8 MB
  instead of 1.0 MB. (2) The executables differ in 8 bytes of size and
  in content (1,015,336 and 1,015,344 bytes): paths in the object
  files, not code, since the C++ is identical. (3) The executable is
  `<project>/out/primescroll`, not a path the caller names. (4) The
  runner leaves `out/primescroll.cpp`; the script deletes it
  (fixed by TODO-6018). (5) No `--direct` form: no step copied
  `libps_ui_appkit.dylib` (fixed by TODO-6017: `--option direct=true`). (6) The backend libraries are
  CMake products; the graph reads them through `--option build_dir`
  and cannot build them (needs command steps, not planned yet). (7) Cold
  `primec build` of the folder 3.4 s of which the script compile is
  about 2.5 s (TODO-6016).
- Startup was not measured: `scripts/measure_startup.sh` shows a window
  and the agent may not open one. A person measures both (this leaf).

## TODO-5757

- 2026-11-09: PrimeScroll master bundle (06268bdc7-era code, the same
  bundle throughout), `scripts/measure_startup.sh --runs 9`, snapshot
  first-text medians, load average in brackets. Quiet: 186 (9), 195 (15),
  210 (10), 218 (15). Gates at normal priority: 604 (99), 514 (114),
  360 (76), 370 (70), 231 (57). Gates with the whole of compile.sh under
  `nice` (default 10): 406 (46), 280 (59), 306 (154), 254 (113), 294
  (45); with compile.sh's own `nice -n 10`: 319 (210), 356 (206), 219
  (168). `taskpolicy -c utility ./scripts/compile.sh --release`: 258 (41),
  but 21 tests in 7 minutes and PrimeStruct_embed_exports timed out;
  stopped. ctest totals: normal 1217 s (with these measurements running)
  and 1067 s; nice 944 s and 1082 s. Timeouts seen: smoke driver at
  normal priority (load ~100) and under nice (load ~200), closures matrix
  under nice (load ~200); each passed alone (smoke 100 s, closures 85 s).

## TODO-5603

- 2026-11-09: launch audit (M3 Pro, 11 cores, 18 GB, macOS 27.0.1
  26A434). Tools: `scripts/measure_startup.sh` with
  `tools/startup_probe/startup_probe.c` (phases from the kernel's process
  start; time inside interposed ps_ui calls before the first
  `ps_ui_wait_event`; optional 1 ms main-thread sampler; session
  injection through `PS_PROBE_SESSION`), the unified log, and a scratch
  reference editor (Objective-C++, one NSWindow, NSScrollView and
  NSTextView, policy Prohibited, exits after its first drawRect).
  Snapshot mode reads no preferences or session and never activates.
- `DYLD_PRINT_STATISTICS` prints nothing under dyld4.
  `DYLD_PRINT_LIBRARIES` (images, not counting "move loaded to delayed"
  lines): installed 1274 (1403 lines, baseline 1404), master 1269,
  hello_button 1266, reference editor 1263. WebKit is image ~950 in all
  of them, AppKit's own closure.
- Snapshot medians, load ~10, ms from process start
  (main/app_init/loop/window/text): master 06268bdc7 bundle
  26/113/167/210/217 (15 runs); installed 2026-10-27 32/121/175/212/220
  (15); reference editor main 13, finishLaunching 92, orderFront 150,
  text 192 (15). Interleaved 15 runs of the same primec's builds:
  handlers 2cead3370-era 179, loop 279a3d4b3 213, reference 212 (text).
- Inside the 50 ms from app_init to loop (no file): window_create 34-38,
  window_show 2-17, text_view_create 3-4, ~120 menu/pref calls 1.5 ms in
  total, PrimeStruct code under 2 ms. ps_ui_init 85-112 ms (sampled:
  -[NSApplication init] 63%, of which _NSInitializeAppContext and the
  menu-bar presentation's WindowServer connection most).
- Document and session (master, load ~10, 9 runs): `.prime` 50 KB as
  argument loop 241 (set_toolbar 61, load 5, highlight 3); session
  small.txt + editor.prime + plan.md (preview 1) loop 559, window 245,
  text 254 (add_tab 2x 166, set_toolbar 4x 112, set_preview_mode 94,
  window_create 3x 46). Earlier runs: plan.md preview only +300-350,
  missing session file +0. Sampled: add_tab is tab-group layout under
  -[NSWindow _addTabbedWindow:ordered:]. Open-file event after a
  restored session: new tab, window_create 16 + add_tab 86.
- New code (cdhash never seen), direct exec, snapshot: editor exe
  exec-to-main 357-517 against 18-34 after (4 trials); 50 KB hello_window
  341-349 against 16-29; plain C `int main` wall 299-369 against 52-63;
  reference editor main 255-280 against 12-20; new dylib only 194-246
  against 14-26; master bundle (exe and dylib new) 428. Log of one:
  amfid "adhoc signed" (no chain), syspolicyd "GK process assessment",
  CloudKit ticket lookup 149 ms (QUIC connection reused, request
  timeouts 3.0 s), "ticket not available", XProtect scan 11 ms, kernel
  AppleSystemPolicy "evaluation result: exec, allowed, cache" wakes the
  waiting exec. Later launches: cached, ~10 ms of amfid per image.
- Load: during another agent's release gate (load 85-126, swap 2.8 GB
  used, 85 MB free) the reference editor's first text was 473-1057 ms
  (6 runs); PrimeScroll installed first draw 240-383, hello_button
  window 605-1169; one run with 3916 major faults reached its first
  window at 1173 ms.
- Unified log, user launches (request -> Launch Services CHECKIN):
  11-08 21:45:21 207, 21:45:56 61, 21:52:27 149, 22:43:55 96; 11-09
  09:50:20 160, 10:29:11 81, 10:31:29 119 ms. Login relaunch 11-07
  18:50:57 5610 ms (launched stopped, role LaunchTAL). 10:29:11 (Finder
  opened a document): spawn +3, amfid dylib +30, CHECKIN +78,
  SignalReady (finishLaunching done) +353, odoc +393, "Restoring
  windows" +430, CursorUI (text view key) +480, AutoFill helper +483;
  Finder's double click was 10:29:11.007, its "Asking CSUI to launch"
  .320. 10:31:29: CHECKIN +110, SignalReady +560, odoc +600, CursorUI
  +900. 21:45:21: linkd rejected AppIntents registration 4 times
  (requiresValidatedBundle), WritingTools/GenerativeModels availability
  +380, Spotlight donation +540 and +920.
- The user's saved session (`ps.list.session.files`) names one file under
  ~/Library/CloudStorage/OneDrive-Arm that no longer exists; a failing
  stat there takes 0.04 ms, but a present, evicted cloud file would be
  downloaded by `loadFile` before the first frame.
- Not measured here (needs a person at the machine): the Launch Services
  path with the probe (`scripts/measure_startup.sh --open`), the first
  Finder launch after an install, a launch after a reboot, Instruments
  App Launch.
- 2026-11-09, the guard: dyld's `DYLD_PRINT_LIBRARIES` lines include
  "move delayed to loaded: name" (105 of them for PrimeScroll), which the
  earlier counts took for images: the distinct image paths are 1164 for
  PrimeScroll, 1161 for a plain AppKit program with one window and text
  view (tools/startup_probe/reference_app.m). Images PrimeScroll loads
  and the reference does not: libps_ui_appkit.dylib, MenuBarClientCore
  and (not every run) CampoUIServices. Linking AVKit adds AVKit,
  AVKitCore and PIP (checked with DYLD_INSERT_LIBRARIES on PrimeScroll:
  6 extra images against the guard's limit of 4). CTest
  `PrimeStruct_native_ui_startup_guard` builds hello_window and
  PrimeScroll natively and runs `scripts/measure_startup.sh --runs 3
  --reference <reference_app> --max-extra-images 4` on each (about 20 s;
  hello_window 2 extra images, PrimeScroll 2). The probe also reports the
  peak resident set (101 to 105 MB in snapshot mode), so
  `scripts/measure_app_startup.sh` (whose mean included the snapshot's
  600 ms idle wait) is removed. The probe imports the calls a backend may
  predate weakly, so it loads into an app built before them.




## TODO-4737

- 2026-07-23c: implemented the safe, verifiable slice of option (b)
  above - deferred the unsafe slice rather than force it. Added
  `tests/unit/ir_pipeline/test_ir_pipeline_validation_ir_lowerer_helpers_classifies_builtin_method_call_targets.cpp`
  (7 true cases covering every exemption, 6 false cases covering
  wrong arity/wrong call name/unknown target) as a scoped,
  machine-checked version of the gap (c) invariant for this specific
  duplication. Verified with a real before/after diff, not just a
  green run: built and ran the full 1387/1389-case
  `ir.pipeline.validation` suite twice (pre-change via `git stash`,
  then with the change restored), diffed the full sorted list of
  failing `TEST CASE:` names from both untruncated runs - byte-for-
  byte identical set of 40 pre-existing failures both times (none
  touch method-call-target exemption logic), with the new file's 2
  cases / 14 assertions passing on top. Zero regressions, zero
  fixed-by-accident. A general "runs in the lowering pipeline,
  checks every published target" invariant pass still isn't
  implemented - only the duplicated flat-string half of the
  exemption surface is single-sourced and regression-tested.
- 2026-09-03: TODO-4724 closed without a reusable extracted seam
  covering the remaining fuzzy-path-matching lambda - confirmed by
  reading its finished-task entry, it wasn't one of the seams
  landed there (that task decomposed a different function
  entirely). This item's remaining scope is unchanged.

## TODO-5902

- 2026-11-17 (from TODO-5896): four tasks that each sum 10^8 integers in a `while` loop of i64 adds: default schedule (flat loop) 1.14 s real; `--vm-threads` (step kernel per task) 12.5 s real, 46.3 s user on an 11-core machine. The threads are about 3.7 times parallel and each task is about 10 times slower than on the flat loop, so the fast kernel per task is what makes the mode faster than the default for CPU-bound tasks.
