# PrimeStruct TODO Investigation Log

Dated investigation/progress notes for currently-open `docs/todo.md`
task blocks. Each section's heading matches a `TODO-XXXX` in that
file. When a task closes, fold whatever's still relevant into its
resolution note in `docs/todo_finished.md` and delete its section
here - this file only ever tracks OPEN tasks' history, same
open-work-only scope rule as `docs/todo.md` itself.

## TODO-5931

- 2026-11-20 (TODO-5931, release build, macOS arm64, `primec --emit=vm`, loaded machine, alternating runs, min of 20, instructions retired via `/usr/bin/time -l`; twice, identical instruction counts):

  | Program | Instructions | Real |
  | --- | --- | --- |
  | empty `main` | 61M | 0.01 s |
  | one `linearToSrgbChannel` call, `import /std/math/*` | 3293M | 0.22 s |
  | same call, source of color.prime only (`import<"colorsrc.prime">`) | 425M | 0.03 s |
  | same call, color.psa (`--library`, 80 KB) | 1226M | 0.08 s |
  | one `Vec2` built and read, vector.prime as source | 759M | 0.06 s |
  | same, vector.psa (`--library`, 125 KB) | 1247M | 0.08 s |

  The library costs about 1.2G instructions per link whatever it holds, more than compiling the source it replaces. Building `std.psa` is also refused for matrix, quaternion, image, text/string and map today, and a method with an explicit `self` is seen with one parameter too many through the interface. Decision: low-value, archived without building (threshold: 20 percent of the compile time of a typical program).

## TODO-6093

- 2026-11-20 (the TODO-5931 measurement repeated after TODO-6091 and TODO-6092; release build of master b119b45cf, macOS arm64, `primec --emit=vm`, 20 alternating runs, min of 20, instructions retired from `/usr/bin/time -l`, run twice; the two passes agree to 0.1 percent, the table is the second). The libraries are copies of stdlib files whose `namespace std` is renamed (`lcolor` from math/color.prime, `lvector` from math/vector.prime, `lpath` from path/path.prime) because the real stdlib cannot be consumed as a library today (below); the source runs import the same copy with `import<"lcolor.prime">`, the library runs use `import</lcolor>` and `--library lcolor.psa`, and the programs call one function of each. `lpath` imports `/std/text/*`, `/std/text/view/*` and `/std/collections/vector/*` (it holds its own copy of them, which the link shares by `sharedRoots`), and the programs that use it import the first two from source (the program needs `StringView`).

  | Program | Source | Library | Library / source |
  | --- | --- | --- | --- |
  | empty `main` | 61.5M | | |
  | one call of `lcolor` (82 KB archive) | 379.7M | 182.1M | 0.48 |
  | one `Vec2` dot of `lvector` (129 KB) | 656.7M | 200.1M | 0.30 |
  | one call of `lpath` (338 KB), text from source | 6485.5M | 5899.1M | 0.91 |
  | all three in one program, text from source | 7507.7M | 5997.6M | 0.80 |
  | only `import /std/text/*` and `/std/text/view/*` (no module) | 5749.9M | | |
  | real stdlib: `import /std/math/*`, one `linearToSrgbChannel` call | 3291.1M | | |

  Real time is 0.01 s for the empty program, 0.02 to 0.03 s for color and vector (both ways), 0.51 s against 0.48 s for path, 0.60 s against 0.48 s for all three. Before TODO-6092 the same library runs cost about 1.2G instructions more than their source; they now cost 120M (color) to 140M (vector) over the empty program, which is the build id (the SHA-256 of the 22 MB `primec`, 129M over an empty `main` in all) plus the archive and the link.

  What the numbers say. (1) A library of a module that is the whole cost of a program is 2 to 3.3 times cheaper than its source (color 0.48, vector 0.30): the archive load, the splice and the link are about 2 ms, and the 129M of build id is what remains. (2) A typical program that imports several modules is dominated by `import /std/text/*` and `/std/text/view/*`: 5750M, 77 percent of the 7508M of the three-module program, and that is `String`, which cannot be a library (TODO-6032: `stringFrom` takes a `string` parameter used beyond `count()`). With text from source the three libraries save 1510M, 20.1 percent of the program; path alone saves 9 percent (its own cost is mostly the text imports of its source). So the measured gain reaches the 20 percent bar only in a program that imports exactly the three library-able modules and nothing else of weight, and it misses it for one module. (3) `import /std/math/*` is 3291M because it also brings matrix and quaternion, which cannot be libraries; color and vector as libraries would save at most about 0.5G of that.

  Why the real stdlib cannot be consumed as a library today, apart from the numbers. Every stdlib library has the root `/std` (the root of a library is the first path segment of its symbols), and `loadLibraries` refuses two libraries that own one root: `--library color.psa --library vector.psa` gives `library color and library vector both own /std`. And `import /std/math/*` splices the library interface (the root is owned) and then also the stdlib source (the stdlib pass reads `sourceStdImports` of the expanded text without asking who owns the root), so the definitions exist twice: `cannot link library color: duplicate definition of /std/math/linearToSrgbChannel`; with `import /std/math/linearToSrgbChannel` it is the same. A stdlib library therefore needs a decision, not a patch: one `std.psa` for all of `/std` (then every stdlib module has to build, and text, image, matrix and quaternion do not), or roots below `/std` (`/std/math`), and in both cases the stdlib pass must skip a module the library owns. `IrLinkOptions::sharedRoots` (`/std`, src/pipeline/LibraryLink.cpp) only removes a duplicate at the link, after the front end has already paid for the source; the namespace-ownership check (`checkProgramDefinesNoOwnedPath`, src/pipeline/LibraryOwnership.cpp) looks at the program's own definitions and imports, not at stdlib splices.

  Cannot be a library today (`--emit=lib` of the file): math/matrix and math/quaternion (`unsupported return type`), image/image (`unsupported return type`), text/string (`stringFrom`, a `string` parameter used beyond `count()`, TODO-6032), text/shared, text/view (`unsupported binding`), collections/map (`unknown import path: /std/collections/equality/*`), collections/equality, errors, buffer_checked, buffer_unchecked, ring_buffer and slice (result, receiver or body shape, TODO-6032), file, dir, glob, sync, scene, ui/events, http/http and http/server, build, build_runner, build_step, gfx, ui/native and web (shapes, `on_error` handlers, unknown call targets). Builds: math/color, math/vector, path, os, json, html, net, maybe, result, panic, tuple, cursor, text/interner, secret, string_map, unicode, unicode_data, unicode_norm, unicode_norm_data, collections/vector, soa and soa_storage, http/files, bench_non_math.

  Decision: not reopened. The bar was at least 20 percent of the compile time of a typical program that imports several stdlib modules, achievable today without language changes; the best measured case sits at 20.1 percent with every other import of weight left out, and a stdlib library is not consumable without a decision about the ownership root of `/std` (and, for most of the value, `String` as a library). TODO-5931 stays archived with these numbers. Found on the way: `import /std/*` ends the compiler with an uncaught `std::out_of_range` (TODO-6094); the fixed cost left is the build id (TODO-6093).

## TODO-6097

- 2026-11-20 (the benchmark of TODO-5931 and TODO-6093 rerun with the real stdlib files as libraries; release build of the TODO-6097 change, macOS arm64, `primec --emit=vm`, instructions retired from `/usr/bin/time -l`, min of 5 runs). The libraries are `primec --emit=lib` of `stdlib/std/math/color.prime`, `math/vector.prime` and `path/path.prime` themselves (roots `/std/math/ColorRGB` and so on, no copy with a renamed namespace). The library runs write `import</std/math>` (and `import</std/path>`) before the imports of the source runs; the programs are the ones of the TODO-6093 table. Outputs and exit codes of every pair are equal (checked for the three-module program: 79 both ways).

  | Program | Source | Libraries | Libraries / source |
  | --- | --- | --- | --- |
  | empty `main` | 62.3M | | |
  | `import /std/math/*`, one `linearToSrgbChannel` call | 3313.3M | 3066.7M (color) / 2405.6M (color and vector) | 0.93 / 0.73 |
  | the same plus one `Vec2` length | 3322.5M | 2412.5M (color and vector) | 0.73 |
  | `import /std/text/*` and `/std/text/view/*` only | 5736.3M | | |
  | one `is_absolute` call of path, text from source | 6591.5M | 5916.2M (path) | 0.90 |
  | color, vector and path in one program, text from source | 10379.3M | 8532.4M (all three) | 0.82 |

  Notes. (1) Several stdlib libraries now link (color and vector, and all three), and `import</std/math>` plus `import /std/math/*` no longer gives `duplicate definition`; before the change `--library color.psa --library vector.psa` was refused (`both own /std`) and one library gave `duplicate definition: /std/math/ColorRGB`. The stdlib source of an owned module is not read: `--emit-deps` of the one-library run lacks `std/math/color.prime` and lists `vector.prime`, `matrix.prime` and `quaternion.prime` (a test counts it). (2) The 20 percent bar of a typical program that imports several stdlib modules: the program with every library-able module and the text imports saves 17.8 percent, so the bar is not met; a program of `/std/math/*` alone saves 27 percent with color and vector, because the cost of that import is mostly color and vector (matrix and quaternion, which cannot be libraries, are 2.4G of the rest). One module (path) saves 10 percent, because the text modules it needs are 5.7G of its 6.6G. The build id is 62M of every run and is TODO-6093. (3) The stdlib libraries are not added to the default build or the build graph: the bar is not met and `String` (text) is not library-able (TODO-6032). Startup of `primec` without `--library` is unchanged (62.3M for an empty `main`, the 61.5M of the TODO-6093 table was another build; the only change on that path is one empty-vector argument).

## TODO-6030

- 2026-11-18 (TODO-6030, release build, macOS arm64, `ctest -R PrimeStruct_vm_threads_corpus_ --parallel 1`, other agents'
  jobs on the machine at load 17 to 20, so entry times are 1.0 to 1.5 times a quiet run): before, 432 corpus entries
  (the program-matrix suites tasks 270, shared_strings 50, channels 44, mutexes 28, atomics 20, and the 20 `cases` entries) took
  296.9 s in all and the slowest 5.45 s (`cases_4_r1`). The time was the front end of each of the 21 runs of a program. After
  (one primevm process compiles a program once and runs the module n times: `primevm --vm-threads --vm-repeat <n>`;
  `runThreadsCorpus` runs the eager schedule in one process and the 20 threads runs in a second): 89 entries took 70.0 s,
  76 percent less, the slowest 3.2 s alone for each entry except `tasks_25` (task_heap_churn, about 0.9 s of real run per
  threads run, so it keeps 10 rounds of 2 runs; its rounds took 3.0 to 4.7 s here, one 7.4 s under a load spike). Per
  group, before then after: tasks 142.9 then 31.0 s (27 entries), shared_strings 87.7 then 12.6 s (5 entries, 2.3 to
  2.8 s each), channels 20.9 then 3.2, mutexes 15.2 then 4.0, atomics 10.5 then 2.6, `cases` 19.8 then 12.3 s (25
  entries of eager plus 4 runs in place of 5, because `cases_4_r1` was above 5 s; they run `primec --emit=vm` per run
  and stay as they were). Every program still runs on the eager schedule once and under `--vm-threads` 20 times.
- 2026-11-17 (TODO-6027): `scripts/tsan_vm_check.sh --also-runtime`
  (ThreadSanitizer build of primevm, primec and PrimeStruct_vm_byte_tests, 5 threads runs per
  program) ran the 19 corpus programs and the runtime suites vm_threads, vm_threads_sync,
  vm_threads_host, vm_sync_replay (TODO-5901: tests/unit/runtime/test_vm_sync_replay.cpp,
  src/runtime/VmSyncLog.cpp), heap_arenas, tasks, atomics, mutexes and conditions: no
  ThreadSanitizer report, exit 0. The embed sync-replay suite (test_embed_sync_replay.cpp) is a
  `repeated` corpus line and is covered by the release gate, not by this script's ThreadSanitizer
  build. TODO-6027 left the 382 corpus entries alone (each at most 2.9 s alone).

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

- 2026-11-19 (agent allowed to show windows until 07:20; fresh `ditto` copies of the master bundle at scratch paths,
  never the installed app; load 5 to 22 from other jobs, never under 4): `--open` on a copy signed and warmed as the
  install recipe does. (1) 10 runs, no file, three rounds: first text 296, 305 and 362 ms (loads 6 to 17, round
  loads rose to 27); with `small.txt` 330 ms (load 9.5 to 13.5). (2) first launch after install and warm-up, three
  fresh copies, `--runs 1` twice each: warm-up (first) `main` 71.9, 55.7 and 57.2 ms against 48.8, 52.0 and 50.1 ms
  for the second launch (an earlier first try measured 834 ms because the probe library had been rebuilt just before and is
  itself new code). (3) `--open --sample --runs 3` at load 9.4: `main` 60, `app_init` 164, `loop` 205, window 234, first
  text 242 ms; `ps_ui_init` 101 ms (sampled: `-[NSApplication init]` 44 to 49 percent, the menu-bar presentation
  instances 21 to 37 percent), `window_create` 17, `window_show` 9, `text_view_create` 3.5; no verdict, since the
  condition is a load under 4. `log show --last 20m --predicate 'process == "PrimeScroll" OR process ==
  "SafariPlatformSupport.Helper"'` printed no entries. TextEdit and the reference editor figures are in docs/NativeUiPlan.md
  section 1.

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

## TODO-5904

- 2026-11-17 (TODO-5900, `scripts/tsan_vm_check.sh --repeats 5`, build-tsan/ RelWithDebInfo with clang++ on macOS arm64, 11 cores): the last
  ThreadSanitizer result of the concurrent corpus. Ten corpus runs (the five program-matrix suites tasks, atomics, mutexes, channels and
  shared strings, and the five compile-run task cases), each program on the eager schedule once and under `--vm-threads` five times:
  all passed and no ThreadSanitizer report was written (`primevm` links libclang_rt.tsan). The hand-built runtime suites of
  `PrimeStruct_vm_byte_tests`, built in build-tsan/ and run by hand (vm_threads, vm_threads_sync, vm_threads_host, heap_arenas, tasks, atomics,
  mutexes, conditions), also passed with no report. The release gate runs every program 20 times; rerun the script, and record its
  result here, before the default flips.

## TODO-5902

- 2026-11-17 (from TODO-5896): four tasks that each sum 10^8 integers in a `while` loop of i64 adds: default schedule (flat loop) 1.14 s real; `--vm-threads` (step kernel per task) 12.5 s real, 46.3 s user on an 11-core machine. The threads are about 3.7 times parallel and each task is about 10 times slower than on the flat loop, so the fast kernel per task is what makes the mode faster than the default for CPU-bound tasks.

- 2026-11-20 (release build, macOS arm64, 11 cores, instructions retired by `/usr/bin/time -l`, the `primevm` compile step included, about 2.0e9 of each): the string loop of TODO-5870 (200000 rounds of a `String`, a 3-byte text and four `append`s) in a spawned task on `--vm-threads`: 27.37e9 before (step kernel per task; two more runs 27.38e9 and 27.42e9), 4.46e9 after (runs of 4.44e9 to 4.49e9), against 4.18e9 for the same loop in a program without tasks and 4.18e9 for the task on the default schedule. So a threads-mode task retires 1.07 times the loop without tasks (was 6.5 times). The 0.28e9 that remain (about 1400 instructions per round) are not the polls at calls: a build without the call poll retired the same 4.44e9 to 4.47e9; they are the threads mode's host calls (the one host lock and the thread's heap arena around each of the loop's `__pstext_*` calls), not measured separately. Default mode guards: the plain loop 4.1807e9 before and 4.1810e9 after, the task on the default schedule 4.1832e9 and 4.1820e9 (a second pair 4.1799e9 and 4.1797e9), `fib(32)` 0.986e9 and 0.968e9 (a second pair 0.946e9 and 0.946e9): the default loop is the `Poll = false` instantiation and has no poll. Four tasks that each sum 10^8 integers (`while` loop of i64 adds): `--vm-threads` 11.93 s real, 44.87 s user and 835e9 instructions before; 0.28 s real, 1.01 s user and 14.9e9 after (the default schedule runs the same program in 0.91 s and 11.7e9 instructions, so the threads mode is about 3.2 times faster in wall time and pays about 1.3 times the instructions per task in this tight loop for the poll at the back-edge; the loop has no calls). Startup of `primevm` is unchanged: a `print_line` program retires 61.4e6 to 62.9e6 instructions before and 61.6e6 to 63.4e6 after (three alternating pairs, the spread is the noise), and the dispatch table of each instantiation is filled on first use, and a run without tasks uses only the default one.

## TODO-5901

- 2026-11-17 (release build, macOS arm64): startup of `primevm -O2` on a task-free program (a `print_line("hello")` main; compile, run and
  exit, 40 runs per sample, `bash` loop timed with `perf_counter`): 15.9, 11.8 and 12.8 ms per run in three samples (the spread is the machine,
  not the change). The only code a run without `--vm-record-sync` and `--vm-replay-sync` meets is one pointer on the VM host (null), one
  null test in `executeVmKernel` for a module without tasks, and the same test per scheduler opcode of a module with tasks; `VmSyncLog` has no
  static state and the flags add no work to option parsing beyond four string compares. No before/after binary was built for an A/B.
- 2026-11-17 (TODO-5901): 30 `primevm --vm-threads --vm-record-sync` runs of a two-producer Mutex and Channel program, each replayed with
  `--vm-replay-sync`: all 30 replays printed what their record printed, and the records covered three different arrival orders (17, 2 and 11
  runs), so replay follows the record and not the eager order; the same with `PRIMEVM_KERNEL=step` (15 runs, no difference). A record is 193
  events (about 1.6 KB) for that program.
- 2026-11-19 (TODO-6028, closed by the owner): the gate keeps `nice -n 10` (TODO-5757). TODO-6026 measured the same five entries alone and with 44 and 60
  busy-loop processes at normal priority (11 cores): at `nice -n 10` they took 5 to 15 times their quiet time, at normal priority 2 to 5 times; a load
  average of 124 was seen on 2026-11-19 with several gates running at once, and a gate then advanced about 1% an hour. Timeouts of 300 s reached by
  entries of 2 to 40 s are not explained by CPU load alone (memory pressure or a stalled disk are the suspects). The owner chose a liveness watchdog
  (TODO-6053) over a priority change.

## TODO-6093 (compiler build id)

- 2026-11-20 (release build, macOS arm64, `primec --emit=vm`, instructions retired from `/usr/bin/time -l`, min of 5 runs; the library is one public `add(a, b)` of 5 lines, the program calls it, `--library lib.psa`). After the change: an empty `main` 62.8M, the same program with the library linked 68.0M, so a linked library costs 5.2M instructions over an empty `main`. Before (the TODO-6093 measurement above, the same method, one build earlier): 61.5M for an empty `main` and 120M to 140M more for a linked library, 129M of it the SHA-256 of the 22 MB `primec`. The before/after pair is not from one binary: a copy of the new `primec` with its `LC_UUID` removed does not start (dyld requires the command), so the full-hash path could not be timed in the same build; the fallback is tested through `BuildIdSource::FullHash` and byte images without an identifier instead. The empty `main` is 0.7M above the earlier 62.1M and 62.3M figures of other builds, which is the size of the new code for a program that never links a library (the id is read only when `--library` or `--emit=lib` runs): startup of a compile without a library does not read the executable.
- Determinism, without a timing assertion (`tests/unit/runtime/test_executable_build_id.cpp`): a hand-built Mach-O image and its byte-identical copy give equal ids in both forms, an image with another UUID and other code gives another id in both forms, an ELF image (32 and 64 bit, both byte orders) yields its GNU build-id note and one without the note yields none, and `primevm` of the test build and a byte-identical copy of it give the same id. A rebuilt different `primec` differs because the linker derives its UUID from the content of the output (Apple ld hashes the output, GNU ld and lld hash it with `--build-id`); the unit test stands for that with images that differ in code and identifier, and the compile-run tests read the real `./primec` id and the archives it wrote.

## TODO-6123 (build command compiler id)

- 2026-11-20 (release build, macOS arm64, instructions retired from `/usr/bin/time -l`, an up-to-date `primec build <dir> --jobs 1` of a one-step `ir` project, three runs each). Before (the step key and the script key each SHA-256 the 22 MB `primec`): 145.4M, 145.5M, 145.8M. After (both keys read `executableBuildId`, the linker UUID): 60.4M, 61.1M (the first run after the change is a rebuild: the old keys miss once). The 85M saved is the two hashes (about 42M each) and nothing else changed. `build` is not the compile path, so startup of a plain compile is not touched.


## TODO-6124 (Wasm refusals per leaf)

- 2026-11-21 (after TODO-6146, `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1` and `PRIMESTRUCT_MATRIX_WASM_RUN=1` of `PrimeStruct_program_matrix_tests`, 514 programs): 164 compile for Wasm, run under node and agree with the VM (none differs, all list `wasm-wasi`). The 350 refusals, first refusal each: `HeapAlloc` 136, `HeapRealloc` 14, `HeapFree` 5, `LoadStringByte` 2 (TODO-6141); host calls 111 and u8 2 (TODO-6127); `unsupported effect mask bits` 32, atomics 18, channels 16 (TODO-6128); recursive calls a real call cannot take 9 (TODO-6140); the struct parameter prologue 2 (TODO-6145); the graphics substrate 1 (by design) and 2 expected frontend diagnostics. No `unsupported control-flow pattern` refusal is left (38 before TODO-6146).
- 2026-11-21 (after TODO-6145, same probes): 166 programs agree on Wasm (the two struct-method programs `frontend_ctor_only_field` and `pick_payload_struct_method` joined), 348 are refused; the struct parameter prologue is no longer a refusal.
- 2026-11-21 (after TODO-6148, same probes): 168 programs agree on Wasm (`native_string_count_and_indexing_21` and `string_scan` joined), 346 are refused; `LoadStringByte` is no longer a refusal. TODO-6141 is split: the slot heap (`HeapAlloc` 136, `HeapRealloc` 14, `HeapFree` 5) is TODO-6149 and the host call behind `heapLiveBlocks()` and the string heap statistics (87 of the 111 host-call refusals) is TODO-6150.
- 2026-11-22 (after TODO-6150, `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1` and `PRIMESTRUCT_MATRIX_WASM_RUN=1`, release build, 534 cases): 293 programs agree on Wasm (28 joined: the `heapLiveBlocks()` leak checks of the vector, pick, early-return, slot and element programs), 1 differs (`file_io_errno_newline_png`: TODO-6176), 220 are refused. Refusals by first cause: 140 `CallHost` of a `String` program (`__pstext_alloc` and the other string heap functions behind byte regions: TODO-6177 byte regions, TODO-6178 string heap, TODO-6179 byte operations, TODO-6180 float text), 32 unsupported effect mask bits, 16 channel and condition opcodes and 18 `AtomicStore` (TODO-6128), 11 front-end errors reported at a scratch path (not examined), 2 `ZextU8` (TODO-6127), 1 `/std/gfx` substrate. The 87 statistic programs counted for this leaf were mostly `String` programs too: only 28 needed the statistic alone.
- 2026-11-22 (after TODO-6176, same probes, 535 cases): with the working directory preopened the new case `file_io_write_read_roundtrip` agrees on Wasm (294 listed programs agree, 220 refused as before); `file_io_errno_newline_png` still differs because a failed file open is packed differently on Wasm (TODO-6182) and its probe run crashes node (exit 139) when the garbage handle is closed.

## TODO-5932

- 2026-11-21 (release build of the branch that adds TODO-6172, macOS arm64, `primec` release, one run each; `/usr/bin/time -l` for instructions retired and real time; the C++ compiler is the system `clang++`). `--emit=optexe` against `--emit=optcpp` (the same C++ text, not compiled):

  | Program | `optcpp` (front end) | `optexe` | Generated lines |
  | --- | --- | --- | --- |
  | empty `main` | 0.02 s, 69M | 0.91 s | 555 |
  | benchmarks/call_fib.prime | 0.02 s, 86M | 1.38 s | 609 |
  | benchmarks/json_parse.prime | 0.03 s, 250M | 0.84 s | 893 |
  | examples/apps/http_hello (link flags missing, so the link step failed after the compile) | 1.29 s, 14.6G | 4.18 s | 23,739 |
  | benchmarks/http/bench_server.prime (same) | 1.45 s, 16.7G | 4.27 s | 35,930 |

  Of the empty `main`'s 0.91 s, `clang++ -std=c++23 -fsyntax-only` of the generated unit is 0.64 s; one header alone costs 0.06 s (`<cmath>`) to 0.28 s (`<vector>`). With the include block precompiled (`-x c++-header`, 15.6 MB, 0.60 s to build once) and `-include-pch`, the empty `main` compiles in 0.39 s instead of 0.70 s (`-O2 -ffp-contract=off -pthread`, no link). So a library translation unit would pay at least the same fixed cost as the program.

## TODO-5933

- 2026-11-21 (reading, no code run): `ScriptEngine::compileEntry` builds an `Options` and calls `runCompilePipelineResult` then `prepareIrModule`; `prepareIrModule` links `options.libraryPaths` (src/ir/IrPreparation.cpp). `loadLibraries` reads each archive and checks `compilerBuildId` against `currentCompilerBuildId()`, the linker UUID of `selfExecutablePath("")`, the running executable. With 5M instructions for the id (TODO-6093) and about 2 ms for the link, the cost of a library per process is small; the id of an embed host differs from `primec`'s, so archives from `primec --emit=lib` are refused there.

## TODO-6032

- 2026-11-21 (`primec --emit=lib` of each of the 59 `stdlib/std/**/*.prime` files in release, first diagnostic of each): 28 build, 31 fail. The 18 that name a shape fail on: result type 9 (collections/errors, dir, file/errors, glob, http/server, web/fragment_script, sync, text/shared, scene), `string` parameter beyond `count()` 4 (build_step, collections/equality, file, text/string), the canonical collections root 4 (buffer_checked, buffer_unchecked, ring_buffer, slice), a generated tail value 1 (ui/events). The other 13 fail before the shape check because the file is not self-contained as a root (build, build_runner, collections/map, gfx and gfx/experimental, http/http, image, math/matrix, math/quaternion, text/view, ui/native/handlers, ui/native, web). A borrowed sum parameter is refused without a name (TODO-6173).


## TODO-6174

- 2026-11-22 (release build, macOS arm64, wall seconds of `primec --emit=optexe` incl. the C++ compile, machine not idle, one run each): empty `main` 2.09 s with `PRIMEC_OPTEXE_PCH=0`, 1.71 s on the cold run that builds the header (one-off, 15.6 MB), 0.95 s warm; examples/apps/http_hello 3.74 s without, 3.33 s with. The header is `build-release/optexe-pch/pch-<key>.pch`, key = hash of the compiler's `-dM -E` macros under the compile flags, the flags and the header text (src/support/CppPrecompiledHeader.cpp).

## TODO-6149

- 2026-11-22 (refusal accounting of `PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1`, release build): before the heap 168 cases compiled for `--emit=wasm --wasm-profile wasi`; after it 266 compile, and `PRIMESTRUCT_MATRIX_WASM_RUN=1` finds 265 that agree with the VM (168 already listed, 97 newly listed with `wasm-wasi`) and one that differs (`file_io_errno_newline_png`, no WASI preopen in the runner: TODO-6176). The refusals left, by first cause: 168 `CallHost` (`heapLiveBlocks()` and other `__pstext_*` host calls: TODO-6150, TODO-6127), 32 unsupported effect mask bits, 18 `AtomicStore` and 16 channel opcodes (TODO-6128), 11 front-end errors reported at a scratch path (not examined), 2 `ZextU8` (TODO-6127), 1 `/std/gfx` substrate.
