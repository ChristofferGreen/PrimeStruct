#!/usr/bin/env python3

from __future__ import annotations

import argparse
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Self-test the architecture health dashboard helper.")
  parser.add_argument(
      "--repo-root",
      type=Path,
      default=Path(__file__).resolve().parents[2],
      help="PrimeStruct repository root.",
  )
  parser.add_argument(
      "--scratch-dir",
      type=Path,
      help="Repo-local scratch directory for temporary fixture repositories.",
  )
  return parser.parse_args()


def load_dashboard_module(repo_root: Path):
  sys.dont_write_bytecode = True
  script_path = repo_root / "scripts" / "architecture_health_dashboard.py"
  spec = importlib.util.spec_from_file_location("architecture_health_dashboard", script_path)
  if spec is None or spec.loader is None:
    raise RuntimeError(f"failed to import {script_path}")
  module = importlib.util.module_from_spec(spec)
  sys.modules[spec.name] = module
  spec.loader.exec_module(module)
  return module


def write_minimal_repo(repo_root: Path, *, with_budget_artifacts: bool) -> None:
  (repo_root / "docs").mkdir(parents=True)
  (repo_root / "scripts").mkdir()
  (repo_root / "src").mkdir()
  (repo_root / "include").mkdir()
  (repo_root / "tests").mkdir()

  (repo_root / "docs" / "source_lock_inventory.md").write_text(
      "# Source-Lock Inventory\n"
      "\n"
      "| Source-lock surface | Classification | Current purpose | Replacement surface |\n"
      "| --- | --- | --- | --- |\n"
      "| `tests/unit/a.cpp` reading `src/A.cpp` | temporary migration lock | Guard A. | Public A. |\n"
      "| `tests/unit/b.cpp` reading docs | contract-worthy | Guard B. | Keep. |\n",
      encoding="utf-8",
  )
  (repo_root / "scripts" / "include_layer_allowlist.txt").write_text(
      "# private dependency exceptions\n"
      "\n"
      "tests/unit/Private.cpp -> src/private/Thing.h\n"
      "src/ir_lowerer/Thing.cpp -> src/semantics/\n",
      encoding="utf-8",
  )
  (repo_root / "src" / "SemanticProduct.cpp").write_text(
      "const std::vector<SemanticProgramFactFamilyInfo> &semanticProgramFactFamilyInfos() {\n"
      "  static const std::vector<SemanticProgramFactFamilyInfo> Families = {\n"
      "      {\"definitions\",\n"
      "       SemanticProgramFactOwnership::AstProvenance,\n"
      "       \"AST-owned callable body and source provenance inventory\"},\n"
      "      {\"publishedRoutingLookups\",\n"
      "       SemanticProgramFactOwnership::DerivedIndex,\n"
      "       \"lowerer lookup indexes derived from semantic-product facts\"},\n"
      "      {\"bindingFacts\",\n"
      "       SemanticProgramFactOwnership::SemanticProduct,\n"
      "       \"published binding facts\"},\n"
      "  };\n"
      "  return Families;\n"
      "}\n",
      encoding="utf-8",
  )
  (repo_root / "src" / "Large.cpp").write_text("".join(f"line {index}\n" for index in range(30)),
                                                encoding="utf-8")
  (repo_root / "include" / "Medium.h").write_text("a\nb\nc\n", encoding="utf-8")
  (repo_root / "tests" / "Small.cpp").write_text("a\nb\n", encoding="utf-8")

  if with_budget_artifacts:
    (repo_root / "benchmarks" / "semantic_memory").mkdir(parents=True)
    (repo_root / "benchmarks" / "type_graph_budget_baseline.json").write_text(
        "{}\n", encoding="utf-8")
    (repo_root / "benchmarks" / "semantic_memory_baseline_report.json").write_text(
        "{}\n", encoding="utf-8")
    (repo_root / "benchmarks" / "semantic_memory" /
     "semantic_product_index_parity_evidence.json").write_text("{}\n", encoding="utf-8")
    (repo_root / "benchmarks" / "source_location_mapper_lookup_budget.json").write_text(
        "{}\n", encoding="utf-8")
    (repo_root / "build-release" / "benchmarks").mkdir(parents=True)
    (repo_root / "build-release" / "graph_budget_report.json").write_text(
        "{}\n", encoding="utf-8")
    (repo_root / "build-release" / "benchmarks" / "semantic_memory_report.json").write_text(
        "{}\n", encoding="utf-8")


def make_temp_repo(parent: Path | None, name: str):
  if parent is not None:
    parent.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix=f"{name}_", dir=parent)
  return tempfile.TemporaryDirectory(prefix=f"{name}_")


def test_representative_parsing(module, scratch_parent: Path | None) -> None:
  with make_temp_repo(scratch_parent, "ps_arch_dash_parse") as temp:
    fixture_root = Path(temp)
    write_minimal_repo(fixture_root, with_budget_artifacts=True)

    report = module.collect_dashboard(fixture_root, 2)
    assert report["source_lock_inventory"]["count"] == 2
    assert report["include_layer_allowlist"]["count"] == 2
    assert report["semantic_product_fact_families"]["total_count"] == 3
    assert report["semantic_product_fact_families"]["by_ownership"] == {
        "AstProvenance": 1,
        "DerivedIndex": 1,
        "SemanticProduct": 1,
    }
    assert [entry["path"] for entry in report["largest_files"]] == [
        "src/Large.cpp",
        "src/SemanticProduct.cpp",
    ]
    available_paths = {entry["path"] for entry in report["budget_summaries"]["available"]}
    assert "benchmarks/type_graph_budget_baseline.json" in available_paths
    assert "benchmarks/source_location_mapper_lookup_budget.json" in available_paths
    assert "build-release/graph_budget_report.json" in available_paths

    text = module.format_text(report)
    assert "Source-lock inventory: 2 rows" in text
    assert "Include-layer allowlist: 2 entries" in text
    assert "Semantic-product fact families: 3 total" in text
    assert "graph:build-release_graph_budget_report" in text
    assert "source_location_mapping:expanded_source_segment_lookup_budget" in text


def test_missing_optional_artifacts(module, scratch_parent: Path | None) -> None:
  with make_temp_repo(scratch_parent, "ps_arch_dash_missing") as temp:
    fixture_root = Path(temp)
    write_minimal_repo(fixture_root, with_budget_artifacts=False)

    report = module.collect_dashboard(fixture_root, 3)
    assert report["budget_summaries"]["available"] == []
    missing_paths = {entry["path"] for entry in report["budget_summaries"]["missing_optional"]}
    assert "benchmarks/type_graph_budget_baseline.json" in missing_paths
    assert "benchmarks/source_location_mapper_lookup_budget.json" in missing_paths
    assert "benchmarks/semantic_memory_baseline_report.json" in missing_paths

    text = module.format_text(report)
    assert "available:\n    none" in text
    assert "missing optional:" in text


def test_cli_json_output(repo_root: Path, scratch_parent: Path | None) -> None:
  with make_temp_repo(scratch_parent, "ps_arch_dash_cli") as temp:
    fixture_root = Path(temp)
    write_minimal_repo(fixture_root, with_budget_artifacts=True)
    json_output = fixture_root / "dashboard.json"
    script_path = repo_root / "scripts" / "architecture_health_dashboard.py"

    result = subprocess.run(
        [
            sys.executable,
            str(script_path),
            "--root",
            str(fixture_root),
            "--top-files",
            "1",
            "--format",
            "json",
            "--json-output",
            str(json_output),
        ],
        text=True,
        capture_output=True,
        check=False,
    )

    if result.returncode != 0:
      print(result.stdout, end="")
      print(result.stderr, end="", file=sys.stderr)
      raise AssertionError("dashboard CLI failed")

    stdout_report = json.loads(result.stdout)
    file_report = json.loads(json_output.read_text(encoding="utf-8"))
    assert stdout_report["schema"] == module_schema()
    assert file_report["schema"] == module_schema()
    assert stdout_report["largest_files"][0]["path"] == "src/Large.cpp"
    assert file_report["source_lock_inventory"]["count"] == 2


def module_schema() -> str:
  return "primestruct_architecture_health_dashboard_v1"


def main() -> int:
  args = parse_args()
  repo_root = args.repo_root.resolve()
  scratch_parent = args.scratch_dir.resolve() if args.scratch_dir else None
  if scratch_parent is not None and scratch_parent.exists():
    shutil.rmtree(scratch_parent)
  module = load_dashboard_module(repo_root)

  test_representative_parsing(module, scratch_parent)
  test_missing_optional_artifacts(module, scratch_parent)
  test_cli_json_output(repo_root, scratch_parent)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
