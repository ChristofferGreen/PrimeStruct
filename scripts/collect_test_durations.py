#!/usr/bin/env python3
"""Collect per-doctest-case durations across ctest-registered shards.

Re-invokes each matching ctest test's underlying doctest binary with
`--duration=true` (rather than trusting ctest's own per-test wall time,
which includes process startup/teardown and doesn't break down by case),
parses doctest's `<seconds> s: <case name>` lines, and persists a JSON
report suitable for driving duration-aware resharding (see TODO-4738 in
docs/todo.md) or just spotting newly-slow cases in CI.

Usage:
  scripts/collect_test_durations.py --build-dir build-debug \
      --filter '^PrimeStruct_primestruct_semantics_calls_flow_collections_' \
      --out /tmp/durations.json
"""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path


REPORT_SCHEMA = "primestruct_test_durations_report_v1"

DURATION_LINE_RE = re.compile(r"^([0-9]+\.[0-9]+) s: (.+)$")


def list_ctest_tests(build_dir: Path, name_filter: str) -> list:
  proc = subprocess.run(
      ["ctest", "--show-only=json-v1"],
      cwd=build_dir,
      capture_output=True,
      text=True,
      check=True,
  )
  data = json.loads(proc.stdout)
  pattern = re.compile(name_filter) if name_filter else None
  tests = []
  for test in data.get("tests", []):
    name = test.get("name", "")
    if pattern and not pattern.search(name):
      continue
    command = test.get("command")
    if not command:
      continue
    timeout_s = None
    for prop in test.get("properties", []):
      if prop.get("name") == "TIMEOUT":
        timeout_s = prop.get("value")
    tests.append({"name": name, "command": command, "timeout_s": timeout_s})
  return tests


def parse_duration_output(output: str) -> list:
  cases = []
  for line in output.splitlines():
    match = DURATION_LINE_RE.match(line.strip())
    if match:
      cases.append({"name": match.group(2), "duration_s": float(match.group(1))})
  return cases


def run_shard_with_duration(build_dir: Path, command: list, timeout_s: int) -> dict:
  binary = command[0]
  args = command[1:]
  # doctest's own binary path in ctest's command is absolute already;
  # append --duration=true so it prints per-case timing instead of
  # relying on the wrapping shell/ctest wall clock (which folds in
  # process startup and reporting overhead uninterestingly).
  full_command = [binary] + args + ["--duration=true"]
  started = time.monotonic()
  try:
    proc = subprocess.run(
        full_command,
        cwd=build_dir,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )
    output = proc.stdout + proc.stderr
    timed_out = False
  except subprocess.TimeoutExpired as exc:
    output = (exc.stdout or "") + (exc.stderr or "")
    timed_out = True
  elapsed = time.monotonic() - started
  cases = parse_duration_output(output)
  return {
      "wall_time_s": elapsed,
      "timed_out": timed_out,
      "case_count": len(cases),
      "cases": cases,
  }


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Collect per-case doctest durations across ctest shards.")
  parser.add_argument("--build-dir", required=True, help="CMake build directory (e.g. build-debug).")
  parser.add_argument("--filter", default="", help="Regex matched against ctest test names; empty means all.")
  parser.add_argument("--out", required=True, help="Path to write the JSON duration report.")
  parser.add_argument("--per-shard-timeout", type=int, default=2400,
                      help="Timeout in seconds applied to each shard invocation (default matches the project's ctest TIMEOUT default).")
  args = parser.parse_args()

  build_dir = Path(args.build_dir).resolve()
  if not build_dir.is_dir():
    print(f"error: build dir not found: {build_dir}", file=sys.stderr)
    return 2

  tests = list_ctest_tests(build_dir, args.filter)
  if not tests:
    print(f"error: no ctest tests matched filter {args.filter!r}", file=sys.stderr)
    return 2

  shards = []
  for index, test in enumerate(tests, start=1):
    print(f"[{index}/{len(tests)}] {test['name']}", file=sys.stderr)
    result = run_shard_with_duration(build_dir, test["command"], args.per_shard_timeout)
    shards.append({
        "ctest_name": test["name"],
        "command": test["command"],
        "ctest_timeout_s": test["timeout_s"],
        **result,
    })

  report = {
      "schema": REPORT_SCHEMA,
      "build_dir": str(build_dir),
      "filter": args.filter,
      "shards": shards,
  }
  out_path = Path(args.out)
  out_path.parent.mkdir(parents=True, exist_ok=True)
  out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
  print(f"wrote {out_path}", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
