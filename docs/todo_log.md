# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.

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
