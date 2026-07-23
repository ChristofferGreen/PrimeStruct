#!/usr/bin/env python3
"""Flag ctest shards that eat too much of their timeout budget.

Reads a duration report produced by collect_test_durations.py and reports
any shard whose measured wall time exceeds a fraction of its ctest TIMEOUT
(default 50%, matching TODO-4738's acceptance bar in docs/todo.md). For a
flagged shard, also proposes a duration-balanced resharding of its cases
(bin-packing by measured per-case duration rather than raw case count) so
a human can decide how to rewrite the CASES_PER_SHARD/TOTAL_CASES call in
the owning cmake/PrimeStructManaged*Suites.cmake file.

Waivers: some shards are known, already-investigated exceptions to the
budget guideline rather than resharding candidates - e.g.
calls_flow_collections_201_210 stacks SoaColumnsN template-monomorphization
cases (N=13..16) with genuine non-linear per-case cost documented in
docs/TestRuntimeOptimization.md and tracked for a real algorithmic fix
under TODO-4713; splitting it into more/smaller shards would mask the
budget signal without addressing the actual cost. Pass --waivers pointing
at a JSON file of {"ctest_name_pattern": "why", ...} (regex on the ctest
name) to report those separately instead of as violations.

Usage:
  scripts/check_test_duration_budget.py --report /tmp/durations.json
  scripts/check_test_duration_budget.py --report /tmp/durations.json --budget-fraction 0.5
  scripts/check_test_duration_budget.py --report /tmp/durations.json --waivers scripts/test_duration_waivers.json
"""
import argparse
import json
import re
import sys
from pathlib import Path


def load_waivers(path: str) -> dict:
  if not path:
    return {}
  return json.loads(Path(path).read_text(encoding="utf-8"))


def matching_waiver(ctest_name: str, waivers: dict):
  for pattern, reason in waivers.items():
    if re.search(pattern, ctest_name):
      return reason
  return None


def propose_shards(cases: list, target_duration_s: float) -> list:
  """Greedily pack cases (in their existing --order-by=file order) into
  shards whose cumulative duration stays under target_duration_s. Keeps
  file order rather than sorting by duration - shards still need to be
  expressible as contiguous --first/--last ranges against the suite's
  doctest --order-by=file ordering."""
  shards = []
  current = []
  current_total = 0.0
  for case in cases:
    duration = case["duration_s"]
    if current and current_total + duration > target_duration_s:
      shards.append(current)
      current = []
      current_total = 0.0
    current.append(case)
    current_total += duration
  if current:
    shards.append(current)
  return shards


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Flag ctest shards exceeding a fraction of their timeout budget.")
  parser.add_argument("--report", required=True, help="Path to a collect_test_durations.py JSON report.")
  parser.add_argument("--budget-fraction", type=float, default=0.5,
                      help="Flag shards whose wall time exceeds this fraction of their ctest TIMEOUT (default 0.5).")
  parser.add_argument("--waivers", default="",
                      help="Path to a JSON file of {ctest_name_regex: reason} for known, accepted exceptions.")
  args = parser.parse_args()

  report = json.loads(Path(args.report).read_text(encoding="utf-8"))
  if report.get("schema") != "primestruct_test_durations_report_v1":
    print("error: unrecognized report schema", file=sys.stderr)
    return 2
  waivers = load_waivers(args.waivers)

  violations = []
  waived = []
  for shard in report["shards"]:
    timeout_s = shard.get("ctest_timeout_s")
    if not timeout_s:
      continue
    budget_s = timeout_s * args.budget_fraction
    if shard["wall_time_s"] > budget_s or shard.get("timed_out"):
      reason = matching_waiver(shard["ctest_name"], waivers)
      if reason:
        waived.append((shard, budget_s, reason))
      else:
        violations.append((shard, budget_s))

  if waived:
    print(f"{len(waived)} shard(s) exceed the budget but are waived (known, tracked exceptions):\n")
    for shard, budget_s, reason in waived:
      print(f"- {shard['ctest_name']}: {shard['wall_time_s']:.1f}s vs {budget_s:.1f}s budget")
      print(f"    waiver: {reason}\n")

  if not violations:
    print(f"OK: no shard exceeds {args.budget_fraction:.0%} of its timeout budget "
          f"({len(report['shards'])} shards checked, {len(waived)} waived).")
    return 0

  print(f"{len(violations)} shard(s) exceed {args.budget_fraction:.0%} of their timeout budget:\n")
  for shard, budget_s in violations:
    status = "TIMED OUT" if shard.get("timed_out") else f"{shard['wall_time_s']:.1f}s"
    print(f"- {shard['ctest_name']}")
    print(f"    wall time: {status} (budget: {budget_s:.1f}s of {shard['ctest_timeout_s']:.1f}s timeout)")
    print(f"    case count: {shard['case_count']}")
    if shard["cases"]:
      total = sum(c["duration_s"] for c in shard["cases"])
      slowest = sorted(shard["cases"], key=lambda c: -c["duration_s"])[:3]
      print(f"    sum of measured case durations: {total:.1f}s")
      print("    slowest cases:")
      for case in slowest:
        print(f"      {case['duration_s']:.2f}s  {case['name']}")
      proposed = propose_shards(shard["cases"], budget_s)
      if len(proposed) > 1:
        print(f"    proposed resharding into {len(proposed)} duration-balanced shards "
              f"(currently 1 shard of {shard['case_count']} cases):")
        offset = 1
        for sub in proposed:
          sub_total = sum(c["duration_s"] for c in sub)
          print(f"      cases {offset}-{offset + len(sub) - 1}: {sub_total:.1f}s")
          offset += len(sub)
    print()

  return 1


if __name__ == "__main__":
  sys.exit(main())
