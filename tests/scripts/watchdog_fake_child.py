#!/usr/bin/env python3
"""Fake test child for tests/scripts/test_test_watchdog.py.

Every step waits for a line on stdin, so a test decides when the child makes
progress and no timing is involved.

  steps N        N times: read a line, beat "step i"; then exit 0
  echo N         N times: read a line, print "line i"; then exit 0 (no beats)
  block          beat "ready", then block reading stdin forever
  exit CODE      print "hello", beat "once", exit CODE
  tree FD        start a grandchild in the same process group that holds
                 descriptor FD, beat "tree ready", then block
  grandchild FD  print "up", then block
"""

import os
import subprocess
import sys


def beat(label):
    os.write(int(os.environ["PRIMESTRUCT_BEAT_FD"]), (label + "\n").encode())


def wait_forever():
    while True:
        if not sys.stdin.readline():
            # stdin closed: keep waiting so only a kill ends the process.
            import threading
            threading.Event().wait()


def main():
    mode = sys.argv[1]
    if mode == "steps":
        for i in range(1, int(sys.argv[2]) + 1):
            sys.stdin.readline()
            beat("step %d" % i)
        return 0
    if mode == "echo":
        for i in range(1, int(sys.argv[2]) + 1):
            sys.stdin.readline()
            print("line %d" % i, flush=True)
        return 0
    if mode == "block":
        beat("ready")
        wait_forever()
    if mode == "exit":
        print("hello", flush=True)
        beat("once")
        return int(sys.argv[2])
    if mode == "tree":
        fd = int(sys.argv[2])
        grandchild = subprocess.Popen(
            [sys.executable, __file__, "grandchild", str(fd)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, pass_fds=(fd,))
        grandchild.stdout.readline()
        beat("tree ready")
        wait_forever()
    if mode == "grandchild":
        print("up", flush=True)
        wait_forever()
    return 2


if __name__ == "__main__":
    sys.exit(main())
