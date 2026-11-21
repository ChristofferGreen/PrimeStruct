#!/usr/bin/env python3
"""Self-test for scripts/test_watchdog.py.

Deterministic by construction: the watchdog reads an injected clock that the
test advances itself, and a fake child makes progress only when the test sends
it a line, so no outcome depends on how fast the machine runs. The only waits
are blocking reads and events with a generous backstop that turns a hang into
a failure.
"""

from __future__ import annotations

import argparse
import importlib.util
import io
import os
import select
import subprocess
import sys
import threading
from pathlib import Path

sys.dont_write_bytecode = True

BACKSTOP_SECONDS = 60


class FakeClock:
    def __init__(self):
        self.now = 0.0
        # Set on the first read: the watchdog takes its start time then, so a
        # test must not move the clock before it.
        self.first_read = threading.Event()

    def __call__(self):
        value = self.now
        self.first_read.set()
        return value


class Run:
    """One watched fake child, driven step by step by the test."""

    def __init__(self, module, child, args, silence, hard_cap, pass_fds=(), snapshot=None):
        self.clock = FakeClock()
        self.beats = []
        self.beat_event = threading.Event()
        self.output_event = threading.Event()
        self.out = io.BytesIO()
        self.err = io.StringIO()
        control_read, self.control = os.pipe()
        self.control_read = control_read
        self.result = None

        def on_beat(line):
            self.beats.append(line)
            self.beat_event.set()

        def on_output(_data):
            self.output_event.set()

        self.watchdog = module.Watchdog(
            [sys.executable, str(child)] + [str(a) for a in args],
            silence, hard_cap, clock=self.clock, poll_interval=0.005,
            out=self.out, err=self.err, stdin=os.fdopen(control_read, "rb", 0),
            pass_fds=pass_fds, on_beat=on_beat, on_output=on_output,
            snapshot=snapshot)
        self.thread = threading.Thread(target=self._run)
        self.thread.start()
        assert self.clock.first_read.wait(BACKSTOP_SECONDS), "the watchdog did not start"

    def _run(self):
        self.result = self.watchdog.run()

    def go(self):
        os.write(self.control, b"go\n")

    def wait_beat(self):
        assert self.beat_event.wait(BACKSTOP_SECONDS), "no beat arrived"
        self.beat_event.clear()

    def wait_output(self):
        assert self.output_event.wait(BACKSTOP_SECONDS), "no output arrived"
        self.output_event.clear()

    def finish(self):
        self.thread.join(BACKSTOP_SECONDS)
        assert not self.thread.is_alive(), "the watchdog did not finish"
        os.close(self.control)
        return self.result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    spec = importlib.util.spec_from_file_location("test_watchdog", repo / "scripts" / "test_watchdog.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    child = repo / "tests" / "scripts" / "watchdog_fake_child.py"

    failures: list[str] = []

    def expect(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    # A child that beats and then goes silent is killed, and the message names
    # its last beat.
    run = Run(module, child, ["block"], silence=10, hard_cap=1000)
    run.wait_beat()
    run.clock.now = 11
    result = run.finish()
    expect(result.killed == "silence", "silent child killed for silence: %r" % result.killed)
    expect(result.exit_code == module.KILLED_EXIT_CODE, "killed exit code is 124: %r" % result.exit_code)
    expect(result.last_beat == "ready", "last beat recorded: %r" % result.last_beat)
    expect('last beat: "ready"' in run.err.getvalue(), "message names the last beat: %r" % run.err.getvalue())
    expect("without a beat" in run.err.getvalue(), "message says silence: %r" % run.err.getvalue())

    # A kill message also carries the beat history and the snapshot of the
    # machine and the process group, taken before the group dies.
    snapshots = []
    run = Run(module, child, ["stepsblock", 3], silence=10, hard_cap=1000,
              snapshot=lambda pgid: snapshots.append(pgid) or "snapshot text")
    for _ in range(3):
        run.go()
        run.wait_beat()
    run.clock.now = 11
    result = run.finish()
    message = run.err.getvalue()
    expect(result.killed == "silence", "history child killed: %r" % result.killed)
    expect("3 beat(s)" in message and "step 1" in message and "step 3" in message,
           "message lists the beat history: %r" % message)
    expect(message.rstrip().endswith("snapshot text"), "message ends with the snapshot: %r" % message)
    expect(len(snapshots) == 1 and snapshots[0] == run.watchdog.pid,
           "snapshot taken once for the child's group: %r" % snapshots)

    # The real snapshot names the processes of a live group (the fake child
    # is one) with their state and CPU time.
    run = Run(module, child, ["block"], silence=10, hard_cap=1000, snapshot=None)
    run.wait_beat()
    text = module.system_snapshot(run.watchdog.pid)
    expect("process group %d: " % run.watchdog.pid in text and " state " in text and " cpu " in text,
           "snapshot names the live group: %r" % text)
    run.clock.now = 11
    run.finish()

    # A child that beats slowly is not killed: every gap is just under the
    # silence limit, and the total time is five times the limit.
    run = Run(module, child, ["steps", 5], silence=10, hard_cap=1000)
    for _ in range(5):
        run.clock.now += 9
        run.go()
        run.wait_beat()
    result = run.finish()
    expect(result.killed is None, "slow beating child not killed: %r" % result.killed)
    expect(result.exit_code == 0, "slow beating child exit code: %r" % result.exit_code)
    expect(run.beats == ["step %d" % i for i in range(1, 6)], "beats in order: %r" % run.beats)
    expect(run.err.getvalue() == "", "no message for a child that finishes: %r" % run.err.getvalue())

    # A child that only prints (no beats) is alive while its output arrives.
    run = Run(module, child, ["echo", 4], silence=10, hard_cap=1000)
    for _ in range(4):
        run.clock.now += 9
        run.go()
        run.wait_output()
    result = run.finish()
    expect(result.killed is None, "printing child not killed: %r" % result.killed)
    expect(run.out.getvalue() == b"".join(b"line %d\n" % i for i in range(1, 5)), "output forwarded: %r" % run.out.getvalue())

    # A child that never beats is killed with a message that says so.
    run = Run(module, child, ["echo", 1], silence=10, hard_cap=1000)
    run.clock.now = 11
    result = run.finish()
    expect(result.killed == "silence", "child with no progress killed: %r" % result.killed)
    expect("no beat was ever received" in run.err.getvalue(), "message for no beat: %r" % run.err.getvalue())

    # A child that keeps beating past the hard cap is killed anyway.
    run = Run(module, child, ["steps", 5], silence=10, hard_cap=25)
    for _ in range(2):
        run.clock.now += 9
        run.go()
        run.wait_beat()
    run.clock.now = 26
    result = run.finish()
    expect(result.killed == "hard-cap", "beating child past the cap killed: %r" % result.killed)
    expect(result.last_beat == "step 2", "hard cap names the last beat: %r" % result.last_beat)
    expect("hard cap of 25 s" in run.err.getvalue(), "message names the cap: %r" % run.err.getvalue())

    # Exit codes and output pass through.
    run = Run(module, child, ["exit", 3], silence=10, hard_cap=1000)
    result = run.finish()
    expect(result.exit_code == 3, "exit code passes through: %r" % result.exit_code)
    expect(result.killed is None, "exiting child not killed: %r" % result.killed)
    expect(run.out.getvalue() == b"hello\n", "child output passes through: %r" % run.out.getvalue())
    expect(run.beats == ["once"], "beat seen from an exiting child: %r" % run.beats)

    # The whole process group dies with the child: a grandchild holds the
    # write end of a pipe, which reaches end of file only once every holder is
    # dead.
    alive_read, alive_write = os.pipe()
    run = Run(module, child, ["tree", alive_write], silence=10, hard_cap=1000, pass_fds=(alive_write,))
    run.wait_beat()
    os.close(alive_write)
    expect(run.beats == ["tree ready"], "tree child ready: %r" % run.beats)
    ready, _, _ = select.select([alive_read], [], [], 0)
    expect(not ready, "the grandchild is alive before the kill")
    run.clock.now = 11
    result = run.finish()
    expect(result.killed == "silence", "tree child killed: %r" % result.killed)
    ready, _, _ = select.select([alive_read], [], [], BACKSTOP_SECONDS)
    expect(bool(ready) and os.read(alive_read, 1) == b"", "grandchild died with the process group")
    os.close(alive_read)

    # The command line entry: argument parsing, the exit code and the beat
    # descriptor in the environment, with the real clock and no kill.
    script = repo / "scripts" / "test_watchdog.py"
    completed = subprocess.run(
        [sys.executable, str(script), "--silence", "600", "--hard-cap", "1200", "--", sys.executable, str(child), "exit", "7"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=BACKSTOP_SECONDS)
    expect(completed.returncode == 7, "command line passes the exit code: %r" % completed.returncode)
    expect(completed.stdout == b"hello\n", "command line passes output: %r" % completed.stdout)
    completed = subprocess.run(
        [sys.executable, str(script), "--silence", "600", "--hard-cap", "1200", "--", "/nonexistent/command"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=BACKSTOP_SECONDS)
    expect(completed.returncode == 127, "a missing command is exit 127: %r" % completed.returncode)

    if failures:
        for failure in failures:
            print("FAIL: " + failure, file=sys.stderr)
        return 1
    print("test_watchdog self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
