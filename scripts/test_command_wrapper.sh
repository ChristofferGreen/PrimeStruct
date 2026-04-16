#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: test_command_wrapper.sh '<command>'" >&2
  exit 2
fi

command_text="$1"
memory_threshold_mb="${PRIMESTRUCT_EXPENSIVE_MEMORY_THRESHOLD_MB:-500}"
memory_guard_mb="${PRIMESTRUCT_TEST_MEMORY_GUARD_MB:-0}"
metrics_file="${PRIMESTRUCT_COMMAND_METRICS_FILE:-}"
ctest_name="${PRIMESTRUCT_CURRENT_CTEST_NAME:-unknown}"

result_file="$(mktemp "${TMPDIR:-/tmp}/primestruct-command-wrapper.XXXXXX")"
trap 'rm -f "$result_file"' EXIT

python3 - "$command_text" "$memory_guard_mb" >"$result_file" <<'PY'
import os
import resource
import signal
import subprocess
import sys
import time

command_text = sys.argv[1]
guard_mb_arg = sys.argv[2]
guard_kb = 0
if guard_mb_arg.isdigit() and int(guard_mb_arg) > 0:
    guard_kb = int(guard_mb_arg) * 1024


def apply_memory_limit() -> None:
    if guard_kb <= 0:
        return
    limit_bytes = guard_kb * 1024
    for rlimit_name in ("RLIMIT_AS", "RLIMIT_DATA"):
        limit_kind = getattr(resource, rlimit_name, None)
        if limit_kind is None:
            continue
        try:
            resource.setrlimit(limit_kind, (limit_bytes, limit_bytes))
        except Exception:
            # Some platforms or limits might not be adjustable in this context.
            continue

proc = subprocess.Popen(
    ["/bin/bash", "-lc", command_text],
    preexec_fn=apply_memory_limit,
    start_new_session=True,
)

peak_rss_kb = 0
guard_triggered = False

def subtree_rss_kb(root_pid: int) -> int:
    try:
        out = subprocess.check_output(
            ["ps", "-axo", "pid=,pgid=,rss="],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except Exception:
        return 0

    rss_by_pid = {}
    pgid_by_pid = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue
        try:
            pid = int(parts[0])
            pgid = int(parts[1])
            rss = int(parts[2])
        except ValueError:
            continue
        rss_by_pid[pid] = rss
        pgid_by_pid[pid] = pgid

    root_pgid = pgid_by_pid.get(root_pid, root_pid)
    total = 0
    for pid, rss in rss_by_pid.items():
        if pgid_by_pid.get(pid) == root_pgid:
            total += rss
    return total

while True:
    if proc.poll() is not None:
        break

    current_rss = subtree_rss_kb(proc.pid)
    if current_rss > peak_rss_kb:
        peak_rss_kb = current_rss

    if guard_kb > 0 and current_rss > guard_kb:
        guard_triggered = True
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        break

    time.sleep(0.1)

proc.wait()
if proc.returncode is None:
    exit_code = 1
elif proc.returncode < 0:
    exit_code = 128 + (-proc.returncode)
else:
    exit_code = proc.returncode

print(f"MAX_RSS_KB={peak_rss_kb}")
print(f"GUARD_TRIGGERED={1 if guard_triggered else 0}")
print(f"EXIT_CODE={exit_code}")
PY

max_rss_kb="$(awk -F= '/^MAX_RSS_KB=/{print $2; exit}' "$result_file")"
guard_triggered="$(awk -F= '/^GUARD_TRIGGERED=/{print $2; exit}' "$result_file")"
exit_code="$(awk -F= '/^EXIT_CODE=/{print $2; exit}' "$result_file")"

if [[ "$max_rss_kb" =~ ^[0-9]+$ ]]; then
  max_rss_mb=$(((max_rss_kb + 1023) / 1024))
  echo "PRIMESTRUCT_MEMORY_SAMPLE max_rss_mb=${max_rss_mb} threshold_mb=${memory_threshold_mb}" >&2
  if [[ "$memory_threshold_mb" =~ ^[1-9][0-9]*$ ]] && (( max_rss_mb > memory_threshold_mb )); then
    echo "PRIMESTRUCT_EXPENSIVE_MEMORY_OFFENDER max_rss_mb=${max_rss_mb} threshold_mb=${memory_threshold_mb}" >&2
  fi
  if [[ -n "$metrics_file" ]]; then
    printf '%s\t%s\t%s\n' "$ctest_name" "$max_rss_mb" "${guard_triggered}" >> "$metrics_file"
  fi
fi

if [[ "$guard_triggered" == "1" ]]; then
  echo "PRIMESTRUCT_MEMORY_GUARD_TRIGGERED guard_mb=${memory_guard_mb} exit_code=${exit_code}" >&2
  if [[ -n "$metrics_file" && ! "$max_rss_kb" =~ ^[0-9]+$ ]]; then
    printf '%s\t%s\t%s\n' "$ctest_name" "0" "${guard_triggered}" >> "$metrics_file"
  fi
fi

if [[ "$exit_code" =~ ^[0-9]+$ ]]; then
  exit "$exit_code"
fi
exit 1
