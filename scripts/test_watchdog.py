#!/usr/bin/env python3
"""Run one test command and kill it when it stops reporting progress.

Usage: test_watchdog.py --silence SECONDS --hard-cap SECONDS -- COMMAND [ARGS...]

The command runs in its own process group with a pipe on its output and a
second pipe, the beat pipe, whose write end it inherits as the descriptor
named by PRIMESTRUCT_BEAT_FD. A beat is one line of text written to that
descriptor by the test's work loop. Progress is a beat or any output on the
command's stdout or stderr. The whole process group is killed when no progress
arrived for --silence seconds, or when the command has run for --hard-cap
seconds whatever it reports. The message names the last beat. See
docs/TestWatchdog.md.
"""

import argparse
import os
import select
import signal
import subprocess
import sys
import time

BEAT_ENV = "PRIMESTRUCT_BEAT_FD"
KILLED_EXIT_CODE = 124
MAX_BEAT_LINE = 4096


class WatchResult:
    def __init__(self, exit_code, killed, last_beat, message):
        self.exit_code = exit_code
        # None, "silence" or "hard-cap".
        self.killed = killed
        self.last_beat = last_beat
        self.message = message


def _format_seconds(value):
    return "%g" % round(value, 1)


class Watchdog:
    """Runs a command and decides from beats and output when to kill it.

    `clock` returns seconds and is read only by this object; tests inject a
    clock they advance themselves. `on_beat(line)` runs after a beat is
    recorded, `on_output(data)` after output is forwarded.
    """

    def __init__(self, command, silence, hard_cap, clock=time.monotonic,
                 poll_interval=0.5, out=None, err=None, stdin=None,
                 env=None, pass_fds=(), on_beat=None, on_output=None):
        self.command = list(command)
        self.silence = silence
        self.hard_cap = hard_cap
        self.clock = clock
        self.poll_interval = poll_interval
        self.out = out if out is not None else sys.stdout.buffer
        self.err = err if err is not None else sys.stderr
        self.stdin = stdin
        self.env = dict(os.environ if env is None else env)
        self.pass_fds = tuple(pass_fds)
        self.on_beat = on_beat
        self.on_output = on_output
        self.pid = None

    def kill_group(self):
        if self.pid is None:
            return
        try:
            os.killpg(self.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass

    def run(self):
        beat_read, beat_write = os.pipe()
        os.set_inheritable(beat_write, True)
        env = dict(self.env)
        env[BEAT_ENV] = str(beat_write)
        try:
            proc = subprocess.Popen(
                self.command, stdin=self.stdin, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, env=env, start_new_session=True,
                pass_fds=(beat_write,) + self.pass_fds)
        finally:
            os.close(beat_write)
        self.pid = proc.pid
        try:
            return self._watch(proc, beat_read)
        finally:
            self.kill_group()
            proc.stdout.close()
            os.close(beat_read)
            proc.wait()

    def _watch(self, proc, beat_read):
        start = self.clock()
        last_progress = start
        last_beat = None
        last_beat_at = None
        pending = b""
        out_fd = proc.stdout.fileno()
        open_fds = {beat_read, out_fd}
        killed = None
        message = ""

        def pump(timeout):
            nonlocal last_progress, last_beat, last_beat_at, pending
            if not open_fds:
                if timeout:
                    time.sleep(min(timeout, 0.01))
                return False
            ready, _, _ = select.select(sorted(open_fds), [], [], timeout)
            for fd in ready:
                data = os.read(fd, 65536)
                if not data:
                    open_fds.discard(fd)
                    continue
                now = self.clock()
                last_progress = now
                if fd == beat_read:
                    pending += data
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        last_beat = line[:MAX_BEAT_LINE].decode("utf-8", "replace")
                        last_beat_at = now - start
                        if self.on_beat:
                            self.on_beat(last_beat)
                    if len(pending) > MAX_BEAT_LINE:
                        pending = b""
                else:
                    self.out.write(data)
                    self.out.flush()
                    if self.on_output:
                        self.on_output(data)
            return bool(ready)

        while True:
            pump(self.poll_interval)
            if proc.poll() is not None:
                break
            now = self.clock()
            if now - start > self.hard_cap:
                killed = "hard-cap"
                message = ("test_watchdog: killed at the hard cap of %s s; %s"
                           % (_format_seconds(self.hard_cap),
                              self._describe_last(last_beat, last_beat_at)))
                break
            if now - last_progress > self.silence:
                killed = "silence"
                message = ("test_watchdog: killed after %s s without a beat "
                           "or output (limit %s s); %s"
                           % (_format_seconds(now - last_progress),
                              _format_seconds(self.silence),
                              self._describe_last(last_beat, last_beat_at)))
                break
        if killed:
            self.kill_group()
            proc.wait()
            self.err.write(message + "\n")
            self.err.flush()
        else:
            # The command ended on its own: forward what it left in the
            # pipes, then stop anything it left running in its group.
            self.kill_group()
            while pump(0):
                pass
        code = proc.wait()
        if killed:
            code = KILLED_EXIT_CODE
        elif code < 0:
            code = 128 - code
        return WatchResult(code, killed, last_beat, message)

    @staticmethod
    def _describe_last(last_beat, last_beat_at):
        if last_beat is None:
            return "no beat was ever received"
        return "last beat: \"%s\" at %s s after start" % (
            last_beat, _format_seconds(last_beat_at))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--silence", type=float, required=True,
                        help="kill after this many seconds without a beat or output")
    parser.add_argument("--hard-cap", type=float, required=True,
                        help="kill after this many seconds whatever the beats say")
    parser.add_argument("--poll-interval", type=float, default=0.5)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("a command is required after --")
    watchdog = Watchdog(command, args.silence, args.hard_cap,
                        poll_interval=args.poll_interval)

    def on_term(signum, frame):
        watchdog.kill_group()
        sys.exit(128 + signum)

    signal.signal(signal.SIGTERM, on_term)
    signal.signal(signal.SIGINT, on_term)
    try:
        return watchdog.run().exit_code
    except FileNotFoundError as error:
        sys.stderr.write("test_watchdog: %s\n" % error)
        return 127


if __name__ == "__main__":
    sys.exit(main())
