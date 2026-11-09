#!/usr/bin/env python3
"""Starts a held snapshot run and leaves it behind (the test of TODO-5823's orphan guard).

Usage: start_orphaned_snapshot_run.py <program> [args...]

Runs the program (a native UI app with PRIMESTRUCT_UI_SNAPSHOT and PRIMESTRUCT_UI_SNAPSHOT_HOLD=1 in its environment)
as a child of this process, waits for the line the app prints once its guard watches this process, and exits without
waiting for it, as a test driver killed at its time limit would. The app keeps this process's stderr, so whoever
started this script reads it to its end once the app has ended: the guard (NativeUiAppKit.mm, guardSnapshotRun) must
end the app, saying so on stderr, as soon as this process is gone. Without the guard the app is held forever.
"""
import os
import subprocess
import sys

if len(sys.argv) < 2:
    print(__doc__, file=sys.stderr)
    sys.exit(64)

app = subprocess.Popen(sys.argv[1:], stdout=subprocess.PIPE, stdin=subprocess.DEVNULL)
line = app.stdout.readline()
if b"snapshot run held" not in line:
    app.wait()
    print(f"the app did not hold its snapshot run (exit {app.returncode}, printed {line!r})", file=sys.stderr)
    sys.exit(1)
print(f"left the held app (pid {app.pid}) behind", file=sys.stderr)
sys.stderr.flush()
os._exit(0)
