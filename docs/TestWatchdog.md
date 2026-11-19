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
| every doctest binary (`tests/unit/test_main.cpp`) | each test case and subcase start | the managed suites (`addPrimeStructDoctestSuite`) and `primestructAddDoctestShards` entries |
| compile-run, build and library tests that shell out | `runCommand` and `primec` stage beats | with the doctest binary they run in |
| `PrimeStruct_embed_host_calls`, `PrimeStruct_embed_runtime_only`, `PrimeStruct_build_function_steps_*` (the entries that hit timeouts) | doctest beat | yes |
| the other direct `add_test` entries in `CMakeLists.txt`, Python checks, native UI entries, `PrimeStruct_vm_threads_corpus_*` | doctest beat where they are doctest binaries | no; their `TIMEOUT` is their only limit until the open leaf in `docs/todo.md` (track `gate-timeouts`) moves them |

An entry that cannot beat (one long `primec` or VM run with no output) keeps its `TIMEOUT` alone and is listed in the
open leaf.
