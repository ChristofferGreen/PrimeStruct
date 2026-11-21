# Test Watchdog

Status: implementation note. The mechanism replaces wall-clock-only CTest timeouts with liveness: a test is killed when it
stops reporting progress, not when it is slow.

## Why

A wall-clock timeout cannot tell a hung test from a slow one. On a loaded gate (several gates at once, load 40 to 120)
an entry that takes 5 s alone takes 5 to 15 times as long at `nice -n 10`, and a timeout sized for a quiet machine kills
tests that are making progress. A timeout sized for the worst load lets a real hang sit for as long. Progress is the
signal that separates the two: a deadlocked test stops reporting, a slow one does not.

## The beat

A beat is one line of text written by the test's own work loop to an inherited file descriptor. The descriptor number is
in the environment variable `PRIMESTRUCT_BEAT_FD`; the variable is unset outside the wrapper and a writer then does
nothing. A line is at most a few hundred bytes (a write of that size to a pipe is atomic, so lines from several
processes do not interleave) and names what the test is about to do: `doctest case: <name>`, `command: <what it runs>`,
`primec: <stage>`.

A beat is never written by a timer thread. A thread that beats every second would keep a deadlocked test alive; a beat
written where one unit of work starts stops when the work stops. The unit of work is what a test does between two beats,
and the silence limit has to cover one unit under heavy load, not the whole test.

C++ writers use `primec::emitProgressBeat` (`include/primec/support/ProgressBeat.h`), Python writers write the line
themselves.

## The wrapper

`scripts/test_watchdog.py --silence N --hard-cap M -- COMMAND ARGS...` runs the command in its own process group with
the beat pipe and a pipe on its stdout and stderr. It forwards the output unchanged and returns the command's exit code
(128 plus the signal number when a signal ended it). It kills the whole process group with `SIGKILL` and exits 124 when

- no progress arrived for `N` seconds (silence), or
- the command has run for `M` seconds, whatever it reports (the hard cap).

Progress is a beat or any output on the command's stdout or stderr. The message goes to stderr and names the cause and the
last beat, for example:

```
test_watchdog: killed after 181 s without a beat or output (limit 180 s); last beat: "doctest case: vm threads" at 12.4 s after start
```

`ctest` has no heartbeat of its own, so the wrapper is part of the CTest registration (a command prefix; `scripts/compile.sh`
and its `nice -n 10` are not changed). When `ctest` itself times a wrapped entry out at its `TIMEOUT`, it ends the wrapper,
which kills the group on `SIGTERM`.

## Child processes

A test that shells out (`std::system`, `popen`) leaves the beat descriptor open in the child, so a program the test runs
can beat too. Two rules cover what a test spends its time on:

1. Output on the child's pipe counts as progress. The wrapper owns the pipe of the command it starts; a test that
   redirects a child's output to a file does not feed it, so
2. `primec` beats itself: at the start of a compile and between its stages, when `PRIMESTRUCT_BEAT_FD` is set. A test that
   compiles many programs or one big program keeps beating through `primec`, and the compile-run, build and library test
   helpers beat before each command they run.

A program a test compiles and then runs (a short VM or native executable) does not beat; the beat before the command
covers it.

## The hard cap

A test that beats forever (a loop that beats and never ends) is caught by the hard cap. Each wrapped entry keeps its
existing CTest `TIMEOUT` as the hard cap; the wrapper's `--hard-cap` is a few seconds below it so the wrapper's message,
with the last beat, is the one printed. No timeout is raised by this mechanism, and no test asserts on time.

## The kill message

A kill names the cause (silence or hard cap) and the last beat, then lists the last eight beats with their times and the
number of beats so far, and a snapshot taken before the group dies: the load average, swap use and free-memory level of
the machine (macOS `sysctl`), and every process of the group with its state (`R` running, `S` sleeping, `U`
uninterruptible wait, `T` stopped), elapsed time and CPU time. A test that passes alone and is killed in a gate is
diagnosed from this text: a swapping machine shows swap near full and processes in `U`, a stalled child shows `S` with
no CPU time, a runaway shows `R` with CPU time near the elapsed time.

## Process tree

The group is started with `setsid`, so a kill reaches every descendant that stayed in it. A descendant that starts its own
session (a daemon) is not reached; no test starts one.

## Tests of the wrapper

`tests/scripts/test_test_watchdog.py` (ctest `PrimeStruct_test_watchdog_self_test`) drives fake children
(`tests/scripts/watchdog_fake_child.py`) against an injected clock. A fake child makes progress only when the test sends
it a line and the test advances the clock only after it saw the beat or output, so nothing depends on machine speed. It
covers: a child that beats and goes silent is killed with its last beat named; a child that beats with gaps just under the
limit for five times the limit is not killed; a printing child with no beats is not killed; a beating child past the hard
cap is killed; a grandchild dies with the group; exit code and output pass through.

## Registration and families

`primestructWatchedCommand(<out var> <timeout> <command...>)` in `CMakeLists.txt` puts the wrapper in front of a command.
The hard cap is `timeout - 5` (the `TIMEOUT` of the entry, which is not changed, ends it five seconds later if the wrapper
does not); the silence limit is `PRIMESTRUCT_TEST_SILENCE_SECONDS` (default 180, never above the hard cap).
`-DPRIMESTRUCT_TEST_WATCHDOG=OFF` registers the plain commands, for a debugger. `scripts/check_test_registration.py` reads
through the wrapper prefix.

| Family | Beat | Wrapped |
| --- | --- | --- |
| every doctest binary (`tests/unit/test_main.cpp`) | each test case and subcase start | every CTest entry that runs one with a `TIMEOUT` of 300 s or more (the managed suites, `primestructAddDoctestShards`, and the direct entries for collection parity, compile time, embed, VM byte regions, `ir_link`, `library_*`, `vm_threads*`, u8 scalar, net, http, path, file listing, glob, build, UI events, native UI headless/markdown/bindings/stdlib/editor, misc, backend, semantics, text filter, IR cfg and optimizer suites, the TSan smoke entries, and the `PrimeStruct_vm_threads_corpus_*` rounds) |
| compile-run, build and library tests that shell out | `runCommand` and `primec` stage beats | with the doctest binary they run in |
| doctest entries registered with a `TIMEOUT` under 300 s (parser, semantics and compile-run shards at 30 and 60 s) | doctest beat | yes, with the floor below: their `TIMEOUT` is 300 s |
| `PrimeStruct_test_registration` (300 s), the other Python check scripts and their self-tests (60 to 120 s) | none | no: a script that prints nothing until it ends cannot beat, and its `TIMEOUT` is its only limit |
| `PrimeStruct_semantic_memory_benchmark`, `PrimeStruct_semantic_memory_definition_worker_parity`, `PrimeStruct_semantic_memory_trend` | none | no: one long `primec` measurement run that prints at the end |
| `PrimeStruct_embed_install_package`, `PrimeStruct_launch_bench_reference_editor`, `PrimeStruct_native_ui_appkit_*`, `PrimeStruct_native_ui_compiled_app_*`, `PrimeStruct_native_ui_startup_guard_*` | none | no: `cmake -P` scripts that run one long compile or one app launch and print at the end; the AppKit drivers also drive app windows (`PRIMESTRUCT_SKIP_UI_WINDOWS`), so no process layer is added there |
| `*_suite_registration` entries (60 s, `cmake -P`) | none | no: seconds long |

### The hard-cap floor of doctest entries

`primestructDoctestTimeout` (`CMakeLists.txt`) gives every watched doctest entry a `TIMEOUT` of at least
`PRIMESTRUCT_TEST_DOCTEST_HARD_CAP_SECONDS` (300 s, so a hard cap of 295 s). Reason: entries registered at 30 and 60 s
(`semantics_move_cases_22_24`, `vm_maps_map_helpers_4`) had a hard cap of 25 and 55 s and, because the silence limit is
never above the cap, a silence limit of the same size. They take 0.1 to 1.3 s alone; a gate at load 100 and `nice -n 10`
stretched them 20 to 200 times and the wrapper killed them while they were still beating. With the floor the silence
limit (180 s, one doctest case or one command between beats) is what governs a hang and the cap only ends a test that
beats forever. No unit of work got more time than one silence limit; a test that needs longer between two beats gets
more beats, not a longer limit. `-DPRIMESTRUCT_TEST_WATCHDOG=OFF` keeps the registered `TIMEOUT` values.

### Waiting for a child

`SystemProcessRunner` (`src/support/ProcessRunner.cpp`, the `clang++` step of `primec --emit=exe` and the like) waits for
its child with one blocking `waitpid` unless `PRIMESTRUCT_BEAT_FD` is set. Behind the watchdog it polls instead and beats
`primec: <tool> is running` whenever the child's CPU time grew since the last look (once a second at most). A `clang++`
compile of the generated C++ is one unit that no `primec` stage beat can split, and a gate at load 100 stretched it past
the 180 s silence limit (`pick_values_9_9`, `vector_owners_11_11`, `12_12`, `17_17`, `38_38`: silence at
`primec: run clang++`, 3 to 4 s alone). The beat comes from the waiting loop, not a timer thread, and only when the child
used CPU, so a child that stopped is still caught by silence. The CPU time is the child process's own
(`proc_pid_rusage` on macOS, `/proc/<pid>/stat` on Linux); a driver that only waits for a grandchild shows no growth.

A wrapped entry that is a native UI doctest binary runs headless exactly as before; the wrapper adds a pipe and a process
group, not a window.

`PRIMESTRUCT_TEST_WATCHDOG=OFF` (above) registers every entry plain again.

An entry with no `TIMEOUT` of its own (`PrimeStruct_native_ui_markdown`, `PrimeStruct_native_ui_markdown_edit`) is
passed to the wrapper with CTest's default 1500 s, so its hard cap is the limit it already had.

### Beats seen at the wrapper

The compile-run entry `PrimeStruct_primestruct_compile_run_vm_outputs_ir_and_output_modes_emitters_23_32_23_23`, run
under `Watchdog` with an `on_beat` callback (the beats as the wrapper receives them), delivered `doctest case: ...`,
`command: ./primec --emit=vm ...`, then the `primec` stage beats `primec: compile start`, `primec: parse`,
`primec: semantics` and `primec: lower and emit`; the last beat at exit was `primec: lower and emit`. A
`primevm` compile-run entry delivered `doctest case`, `command: ./primevm ...`, `primec: compile start`,
`primec: parse`, `primec: semantics`. So a stage of `primec` that stops is named in the kill message.
