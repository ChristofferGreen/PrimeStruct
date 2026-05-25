#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REPORT_SCHEMA = "primestruct_architecture_health_dashboard_v1"

FACT_FAMILY_RE = re.compile(
    r'\{\s*"(?P<name>[^"]+)"\s*,\s*'
    r"SemanticProgramFactOwnership::(?P<ownership>[A-Za-z0-9_]+)\s*,\s*"
    r'"(?P<description>(?:[^"\\]|\\.)*)"\s*\}',
    re.DOTALL,
)


@dataclass(frozen=True)
class BudgetPathSpec:
  area: str
  label: str
  path: Path


def repo_relative(path: Path, repo_root: Path) -> str:
  try:
    return path.resolve().relative_to(repo_root.resolve()).as_posix()
  except ValueError:
    return path.as_posix()


def is_markdown_separator_row(line: str) -> bool:
  cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
  return bool(cells) and all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells)


def parse_markdown_table_rows(text: str, required_header: str) -> list[list[str]]:
  lines = text.splitlines()
  table_start = -1
  for index, line in enumerate(lines):
    if line.strip().startswith("|") and required_header in line:
      table_start = index
      break

  if table_start < 0:
    return []

  rows: list[list[str]] = []
  index = table_start + 1
  if index < len(lines) and is_markdown_separator_row(lines[index]):
    index += 1

  while index < len(lines):
    stripped = lines[index].strip()
    if not stripped.startswith("|"):
      break
    if not is_markdown_separator_row(stripped):
      rows.append([cell.strip() for cell in stripped.strip("|").split("|")])
    index += 1

  return rows


def collect_source_lock_inventory(repo_root: Path) -> dict[str, Any]:
  path = repo_root / "docs" / "source_lock_inventory.md"
  text = path.read_text(encoding="utf-8")
  rows = parse_markdown_table_rows(text, "Source-lock surface")
  return {
      "path": repo_relative(path, repo_root),
      "count": len(rows),
      "surfaces": [row[0] for row in rows if row],
  }


def collect_include_layer_allowlist(repo_root: Path) -> dict[str, Any]:
  path = repo_root / "scripts" / "include_layer_allowlist.txt"
  entries: list[str] = []
  for line in path.read_text(encoding="utf-8").splitlines():
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
      continue
    entries.append(stripped)

  return {
      "path": repo_relative(path, repo_root),
      "count": len(entries),
      "entries": entries,
  }


def collect_semantic_product_fact_families(repo_root: Path) -> dict[str, Any]:
  path = repo_root / "src" / "SemanticProduct.cpp"
  text = path.read_text(encoding="utf-8")
  families: list[dict[str, str]] = []
  by_ownership: dict[str, int] = {}

  for match in FACT_FAMILY_RE.finditer(text):
    ownership = match.group("ownership")
    family = {
        "name": match.group("name"),
        "ownership": ownership,
        "description": match.group("description"),
    }
    families.append(family)
    by_ownership[ownership] = by_ownership.get(ownership, 0) + 1

  if not families:
    raise ValueError(f"no semantic-product fact families found in {path}")

  return {
      "path": repo_relative(path, repo_root),
      "total_count": len(families),
      "by_ownership": dict(sorted(by_ownership.items())),
      "families": families,
  }


def count_lines(path: Path) -> int:
  with path.open("r", encoding="utf-8", errors="ignore") as handle:
    return sum(1 for _ in handle)


def collect_largest_files(repo_root: Path, top_files: int) -> list[dict[str, Any]]:
  files: list[dict[str, Any]] = []
  for subsystem in ("src", "include", "tests"):
    base = repo_root / subsystem
    if not base.exists():
      continue
    for path in base.rglob("*"):
      if not path.is_file():
        continue
      relative = repo_relative(path, repo_root)
      files.append({
          "path": relative,
          "subsystem": subsystem,
          "lines": count_lines(path),
      })

  files.sort(key=lambda item: (-int(item["lines"]), str(item["path"])))
  return files[:top_files]


def budget_path_specs(repo_root: Path) -> list[BudgetPathSpec]:
  specs = [
      BudgetPathSpec("graph", "type_graph_budget_baseline",
                     Path("benchmarks/type_graph_budget_baseline.json")),
      BudgetPathSpec("semantic_memory", "semantic_memory_baseline_report",
                     Path("benchmarks/semantic_memory_baseline_report.json")),
      BudgetPathSpec("semantic_memory", "semantic_memory_budget_policy",
                     Path("benchmarks/semantic_memory_budget_policy.json")),
      BudgetPathSpec("semantic_memory", "semantic_memory_budget_policy_debug",
                     Path("benchmarks/semantic_memory_budget_policy_debug.json")),
      BudgetPathSpec("semantic_memory", "semantic_memory_phase_one_criteria",
                     Path("benchmarks/semantic_memory_phase_one_success_criteria.json")),
      BudgetPathSpec("semantic_memory", "semantic_product_index_parity_evidence",
                     Path("benchmarks/semantic_memory/semantic_product_index_parity_evidence.json")),
      BudgetPathSpec("semantic_memory", "semantic_product_index_report",
                     Path("benchmarks/semantic_memory/semantic_product_index_math_star_repro_report.json")),
  ]

  for build_dir in sorted(repo_root.glob("build-release*")):
    if not build_dir.is_dir():
      continue
    build_relative = build_dir.relative_to(repo_root)
    specs.extend([
        BudgetPathSpec("graph", f"{build_dir.name}_graph_budget_report",
                       build_relative / "graph_budget_report.json"),
        BudgetPathSpec("semantic_memory", f"{build_dir.name}_semantic_memory_report",
                       build_relative / "benchmarks" / "semantic_memory_report.json"),
        BudgetPathSpec("semantic_memory", f"{build_dir.name}_semantic_memory_budget_check_report",
                       build_relative / "benchmarks" / "semantic_memory_budget_check_report.json"),
        BudgetPathSpec("semantic_memory", f"{build_dir.name}_semantic_memory_trend_report",
                       build_relative / "benchmarks" / "semantic_memory_trend_report.json"),
        BudgetPathSpec("semantic_memory", f"{build_dir.name}_semantic_memory_artifacts",
                       build_relative / "benchmarks" / "semantic_memory_artifacts"),
    ])

  return specs


def collect_budget_summaries(repo_root: Path) -> dict[str, list[dict[str, str]]]:
  available: list[dict[str, str]] = []
  missing_optional: list[dict[str, str]] = []

  for spec in budget_path_specs(repo_root):
    absolute = repo_root / spec.path
    entry = {
        "area": spec.area,
        "label": spec.label,
        "path": spec.path.as_posix(),
    }
    if absolute.exists():
      available.append(entry)
    else:
      missing_optional.append(entry)

  return {
      "available": available,
      "missing_optional": missing_optional,
  }


def collect_dashboard(repo_root: Path, top_files: int) -> dict[str, Any]:
  repo_root = repo_root.resolve()
  return {
      "schema": REPORT_SCHEMA,
      "root": str(repo_root),
      "source_lock_inventory": collect_source_lock_inventory(repo_root),
      "include_layer_allowlist": collect_include_layer_allowlist(repo_root),
      "semantic_product_fact_families": collect_semantic_product_fact_families(repo_root),
      "largest_files": collect_largest_files(repo_root, top_files),
      "budget_summaries": collect_budget_summaries(repo_root),
  }


def format_budget_entries(entries: list[dict[str, str]]) -> list[str]:
  if not entries:
    return ["    none"]
  return [
      f"    {entry['area']}:{entry['label']} -> {entry['path']}"
      for entry in entries
  ]


def format_text(report: dict[str, Any]) -> str:
  source_locks = report["source_lock_inventory"]
  allowlist = report["include_layer_allowlist"]
  fact_families = report["semantic_product_fact_families"]
  ownership = fact_families["by_ownership"]
  budget_summaries = report["budget_summaries"]

  lines = [
      "PrimeStruct architecture health dashboard",
      f"Root: {report['root']}",
      "",
      f"Source-lock inventory: {source_locks['count']} rows ({source_locks['path']})",
      f"Include-layer allowlist: {allowlist['count']} entries ({allowlist['path']})",
      "Semantic-product fact families: "
      f"{fact_families['total_count']} total "
      f"({', '.join(f'{key}={value}' for key, value in ownership.items())}) "
      f"({fact_families['path']})",
      "",
      "Largest subsystem files:",
  ]

  for entry in report["largest_files"]:
    lines.append(f"  {entry['lines']:>6}  {entry['path']} [{entry['subsystem']}]")

  lines.extend([
      "",
      "Budget summary paths:",
      "  available:",
      *format_budget_entries(budget_summaries["available"]),
      "  missing optional:",
      *format_budget_entries(budget_summaries["missing_optional"]),
  ])
  return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Report PrimeStruct architecture health metrics without thresholds.")
  parser.add_argument(
      "--root",
      type=Path,
      default=Path(__file__).resolve().parents[1],
      help="PrimeStruct repository root.",
  )
  parser.add_argument(
      "--top-files",
      type=int,
      default=10,
      help="Number of largest src/include/tests files to report.",
  )
  parser.add_argument(
      "--format",
      choices=("text", "json"),
      default="text",
      help="Dashboard stdout format.",
  )
  parser.add_argument(
      "--json-output",
      type=Path,
      help="Optional path to write the JSON dashboard report.",
  )
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  if args.top_files <= 0:
    print("[architecture_health_dashboard] --top-files must be positive", file=sys.stderr)
    return 2

  try:
    report = collect_dashboard(args.root, args.top_files)
    json_text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.json_output:
      output_path = args.json_output
      if not output_path.is_absolute():
        output_path = (args.root / output_path).resolve()
      output_path.parent.mkdir(parents=True, exist_ok=True)
      output_path.write_text(json_text, encoding="utf-8")

    if args.format == "json":
      print(json_text, end="")
    else:
      print(format_text(report), end="")
  except Exception as exc:  # pylint: disable=broad-except
    print(f"[architecture_health_dashboard] failed: {exc}", file=sys.stderr)
    return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
