#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.benchmark_harness");

namespace {

std::size_t countOccurrences(const std::string &text, const std::string &needle) {
  if (needle.empty()) {
    return 0;
  }
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

} // namespace

TEST_CASE("semantic memory benchmark helper canonicalizes legacy no-fact rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_legacy_no_fact_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_legacy_no_fact_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'on',\n"
          "    'no_fact_emission': 'false',\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 95,\n"
          "    'worst_peak_rss_bytes': 110,\n"
          "    'median_wall_seconds': 1.0,\n"
          "    'worst_wall_seconds': 1.1,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'off',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 90,\n"
          "    'worst_peak_rss_bytes': 100,\n"
          "    'median_wall_seconds': 0.9,\n"
          "    'worst_wall_seconds': 1.0,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_semantic_product_force_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('no_fact_emission') is False\n"
          "  ok = ok and delta.get('fact_families') == 'auto'\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_on_minus_off'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps({'rows': rows, 'deltas': deltas}, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper canonicalizes mixed-case legacy mode rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_mixed_case_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_mixed_case_mode_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'method_target_memoization': 'ON',\n"
          "    'graph_local_auto_key_mode': 'COMPACT',\n"
          "    'graph_local_auto_side_channel_mode': 'LEGACY_SHADOW',\n"
          "    'semantic_product_force': 'OFF',\n"
          "    'no_fact_emission': 'FALSE',\n"
          "    'fact_families': 'method_call_targets, Direct_Call_Targets',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'PMR',\n"
          "    'median_peak_rss_bytes': 110,\n"
          "    'worst_peak_rss_bytes': 125,\n"
          "    'median_wall_seconds': 1.2,\n"
          "    'worst_wall_seconds': 1.3,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'legacy-shadow',\n"
          "    'semantic_product_force': 'off',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'direct_call_targets,method_call_targets',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'std',\n"
          "    'median_peak_rss_bytes': 120,\n"
          "    'worst_peak_rss_bytes': 140,\n"
          "    'median_wall_seconds': 1.4,\n"
          "    'worst_wall_seconds': 1.5,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_graph_local_auto_dependency_scratch_mode_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('method_target_memoization') == 'on'\n"
          "  ok = ok and delta.get('graph_local_auto_key_mode') == 'compact'\n"
          "  ok = ok and delta.get('graph_local_auto_side_channel_mode') == 'legacy-shadow'\n"
          "  ok = ok and delta.get('semantic_product_force') == 'off'\n"
          "  ok = ok and delta.get('no_fact_emission') is False\n"
          "  ok = ok and delta.get('fact_families') == 'direct_call_targets,method_call_targets'\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_std_minus_pmr'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_std_minus_pmr'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps({'rows': rows, 'deltas': deltas}, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper canonicalizes null no-fact rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_null_no_fact_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_null_no_fact_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'on',\n"
          "    'no_fact_emission': None,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 97,\n"
          "    'worst_peak_rss_bytes': 108,\n"
          "    'median_wall_seconds': 1.0,\n"
          "    'worst_wall_seconds': 1.1,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'off',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 90,\n"
          "    'worst_peak_rss_bytes': 99,\n"
          "    'median_wall_seconds': 0.9,\n"
          "    'worst_wall_seconds': 1.0,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_semantic_product_force_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('no_fact_emission') is False\n"
          "  ok = ok and delta.get('fact_families') == 'auto'\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_on_minus_off'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps({'rows': rows, 'deltas': deltas}, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper canonicalizes null legacy mode rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_null_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_null_mode_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'method_target_memoization': None,\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': None,\n"
          "    'semantic_product_force': None,\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': None,\n"
          "    'graph_local_auto_dependency_scratch_mode': None,\n"
          "    'median_peak_rss_bytes': 101,\n"
          "    'worst_peak_rss_bytes': 111,\n"
          "    'median_wall_seconds': 1.1,\n"
          "    'worst_wall_seconds': 1.2,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'legacy-shadow',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'semantic_product_force': 'auto',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 121,\n"
          "    'worst_peak_rss_bytes': 141,\n"
          "    'median_wall_seconds': 1.4,\n"
          "    'worst_wall_seconds': 1.5,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_graph_local_auto_key_mode_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('method_target_memoization') == 'on'\n"
          "  ok = ok and delta.get('graph_local_auto_side_channel_mode') == 'flat'\n"
          "  ok = ok and delta.get('graph_local_auto_dependency_scratch_mode') == 'pmr'\n"
          "  ok = ok and delta.get('semantic_product_force') == 'auto'\n"
          "  ok = ok and delta.get('no_fact_emission') is False\n"
          "  ok = ok and delta.get('fact_families') == 'auto'\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_legacy_shadow_minus_compact'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_legacy_shadow_minus_compact'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps({'rows': rows, 'deltas': deltas}, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper canonicalizes boolean-like mode rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_bool_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_bool_mode_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "semantic_product_rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': '1',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'true',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 140,\n"
          "    'worst_peak_rss_bytes': 150,\n"
          "    'median_wall_seconds': 1.5,\n"
          "    'worst_wall_seconds': 1.6,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': '0',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 120,\n"
          "    'worst_peak_rss_bytes': 130,\n"
          "    'median_wall_seconds': 1.3,\n"
          "    'worst_wall_seconds': 1.4,\n"
          "  },\n"
          "]\n"
          "memoization_rows = [\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'true',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': '1',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 133,\n"
          "    'worst_peak_rss_bytes': 144,\n"
          "    'median_wall_seconds': 1.4,\n"
          "    'worst_wall_seconds': 1.5,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'on',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': '0',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 119,\n"
          "    'worst_peak_rss_bytes': 129,\n"
          "    'median_wall_seconds': 1.2,\n"
          "    'worst_wall_seconds': 1.3,\n"
          "  },\n"
          "]\n"
          "force_deltas = mod.compute_semantic_product_force_deltas(semantic_product_rows)\n"
          "memo_deltas = mod.compute_method_target_memoization_deltas(memoization_rows)\n"
          "ok = len(force_deltas) == 1 and len(memo_deltas) == 1\n"
          "if ok:\n"
          "  force = force_deltas[0]\n"
          "  memo = memo_deltas[0]\n"
          "  ok = ok and force.get('method_target_memoization') == 'on'\n"
          "  ok = ok and memo.get('semantic_product_force') == 'on'\n"
          "  ok = ok and isinstance(force.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "  ok = ok and isinstance(memo.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "if not ok:\n"
          "  print(json.dumps({'force_rows': semantic_product_rows,\n"
          "                    'memo_rows': memoization_rows,\n"
          "                    'force_deltas': force_deltas,\n"
          "                    'memo_deltas': memo_deltas},\n"
          "                   indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper emits wall-rss machine report rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_wall_rss_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_wall_rss.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_wall_rss.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_wall_rss_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_wall_rss_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "ok = report.get('schema') == 'primestruct_semantic_memory_report_v1'\n"
          "ok = ok and len(rows) == 1\n"
          "if rows:\n"
          "  row = rows[0]\n"
          "  wall = row.get('wall_seconds', [])\n"
          "  peak = row.get('peak_rss_bytes', [])\n"
          "  ok = ok and row.get('fixture') == 'no_import'\n"
          "  ok = ok and row.get('phase') == 'ast-semantic'\n"
          "  ok = ok and row.get('runs') == 1\n"
          "  ok = ok and isinstance(wall, list) and len(wall) == 1 and float(wall[0]) >= 0.0\n"
          "  ok = ok and isinstance(peak, list) and len(peak) == 1 and int(peak[0]) > 0\n"
          "  ok = ok and abs(float(row.get('median_wall_seconds', -1.0)) - float(wall[0])) < 1e-12\n"
          "  ok = ok and abs(float(row.get('worst_wall_seconds', -1.0)) - float(wall[0])) < 1e-12\n"
          "  ok = ok and int(row.get('median_peak_rss_bytes', -1)) == int(peak[0])\n"
          "  ok = ok and int(row.get('worst_peak_rss_bytes', -1)) == int(peak[0])\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper emits key-cardinality report fields") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_key_cardinality_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_key_cardinality.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_key_cardinality.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import,math_vector --phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_key_cardinality_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_key_cardinality_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "ok = len(rows) == 4\n"
          "method_positive = False\n"
          "for row in rows:\n"
          "  phase = row.get('phase')\n"
          "  card = row.get('key_cardinality')\n"
          "  if phase == 'semantic-product':\n"
          "    ok = ok and isinstance(card, dict)\n"
          "    if isinstance(card, dict):\n"
          "      direct = card.get('distinct_direct_call_target_keys')\n"
          "      method = card.get('distinct_method_call_target_keys')\n"
          "      max_len = card.get('max_target_key_length')\n"
          "      ok = ok and isinstance(direct, int) and direct >= 0\n"
          "      ok = ok and isinstance(method, int) and method >= 0\n"
          "      ok = ok and isinstance(max_len, int) and max_len >= 0\n"
          "      if direct > 0 or method > 0:\n"
          "        ok = ok and max_len > 0\n"
          "      method_positive = method_positive or method > 0\n"
          "  elif phase == 'ast-semantic':\n"
          "    ok = ok and card is None\n"
          "  else:\n"
          "    ok = False\n"
          "ok = ok and method_positive\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper counts semantic-product index families from dumps") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_index_family_counts_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_index_family_counts_validate.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "dump = '''semantic_product {\n"
          "  direct_call_targets[0]: scope_path=\"/main\" call_name=\"id\"\n"
          "  direct_call_targets[1]: scope_path=\"/main\" call_name=\"plus\"\n"
          "  method_call_targets[0]: scope_path=\"/main\" method_name=\"count\"\n"
          "  bridge_path_choices[0]: scope_path=\"/main\" collection_family=\"vector\"\n"
          "  binding_facts[0]: scope_path=\"/main\" site_kind=\"local\"\n"
          "  return_facts[0]: definition_path=\"/main\"\n"
          "  local_auto_facts[0]: scope_path=\"/main\" binding_name=\"value\"\n"
          "  query_facts[0]: scope_path=\"/main\" call_name=\"lookup\"\n"
          "  try_facts[0]: scope_path=\"/main\"\n"
          "  on_error_facts[0]: definition_path=\"/main\"\n"
          "}'''\n"
          "counts = mod.collect_semantic_product_index_family_counts(dump)\n"
          "ok = counts == {\n"
          "  'direct_call_targets': 2,\n"
          "  'method_call_targets': 1,\n"
          "  'bridge_path_choices': 1,\n"
          "  'binding_facts': 1,\n"
          "  'return_facts': 1,\n"
          "  'local_auto_facts': 1,\n"
          "  'query_facts': 1,\n"
          "  'try_facts': 1,\n"
          "  'on_error_facts': 1,\n"
          "}\n"
          "if not ok:\n"
          "  print(json.dumps(counts, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper reports semantic-product index family parity across definition workers") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_index_family_parity_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_index_family_parity.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_index_family_parity.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures math_vector --phases semantic-product "
      "--definition-validation-workers both --report-json " +
      quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_index_family_parity_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_index_family_parity_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('definition_validation_worker_mode_deltas', [])\n"
          "required = {\n"
          "  'direct_call_targets', 'method_call_targets', 'bridge_path_choices',\n"
          "  'binding_facts', 'return_facts', 'local_auto_facts',\n"
          "  'query_facts', 'try_facts', 'on_error_facts'\n"
          "}\n"
          "ok = len(rows) == 2 and len(deltas) == 1\n"
          "if ok:\n"
          "  for row in rows:\n"
          "    counts = row.get('semantic_product_index_family_counts')\n"
          "    ok = ok and isinstance(counts, dict)\n"
          "    if isinstance(counts, dict):\n"
          "      ok = ok and set(counts.keys()) == required\n"
          "      ok = ok and all(isinstance(counts[name], int) and counts[name] >= 0 for name in required)\n"
          "      ok = ok and sum(counts.values()) > 0\n"
          "  delta = deltas[0]\n"
          "  single = delta.get('semantic_product_index_family_counts_single_worker')\n"
          "  dual = delta.get('semantic_product_index_family_counts_dual_worker')\n"
          "  ok = ok and delta.get('fixture') == 'math_vector'\n"
          "  ok = ok and delta.get('phase') == 'semantic-product'\n"
          "  ok = ok and bool(delta.get('dump_sha256_identical'))\n"
          "  ok = ok and bool(delta.get('semantic_product_index_family_counts_identical'))\n"
          "  ok = ok and isinstance(single, dict) and isinstance(dual, dict)\n"
          "  if isinstance(single, dict) and isinstance(dual, dict):\n"
          "    ok = ok and set(single.keys()) == required and set(dual.keys()) == required\n"
          "    ok = ok and single == dual\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper defaults to three runs with median-worst rollups") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_default_runs_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_default_runs.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_default_runs.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --fixtures no_import --phases ast-semantic "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_default_runs_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_default_runs_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "ok = report.get('runs') == 3 and len(rows) == 1\n"
          "if rows:\n"
          "  row = rows[0]\n"
          "  wall = row.get('wall_seconds', [])\n"
          "  peak = row.get('peak_rss_bytes', [])\n"
          "  ok = ok and row.get('fixture') == 'no_import'\n"
          "  ok = ok and row.get('phase') == 'ast-semantic'\n"
          "  ok = ok and row.get('runs') == 3\n"
          "  ok = ok and isinstance(wall, list) and len(wall) == 3\n"
          "  ok = ok and isinstance(peak, list) and len(peak) == 3\n"
          "  if isinstance(wall, list) and len(wall) == 3:\n"
          "    wall_sorted = sorted(float(v) for v in wall)\n"
          "    wall_median = wall_sorted[1]\n"
          "    wall_worst = max(float(v) for v in wall)\n"
          "    ok = ok and abs(float(row.get('median_wall_seconds', -1.0)) - wall_median) < 1e-12\n"
          "    ok = ok and abs(float(row.get('worst_wall_seconds', -1.0)) - wall_worst) < 1e-12\n"
          "  if isinstance(peak, list) and len(peak) == 3:\n"
          "    peak_values = [int(v) for v in peak]\n"
          "    peak_sorted = sorted(peak_values)\n"
          "    peak_median = peak_sorted[1]\n"
          "    peak_worst = max(peak_values)\n"
          "    ok = ok and int(row.get('median_peak_rss_bytes', -1)) == peak_median\n"
          "    ok = ok and int(row.get('worst_peak_rss_bytes', -1)) == peak_worst\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper covers no-import and math-vector phases") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_phase_fixture_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_phase_fixture.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_phase_fixture.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import,math_vector --phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_phase_fixture_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_phase_fixture_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "pairs = {(row['fixture'], row['phase']) for row in report['results']}\n"
          "expected = {\n"
          "  ('no_import', 'ast-semantic'),\n"
          "  ('no_import', 'semantic-product'),\n"
          "  ('math_vector', 'ast-semantic'),\n"
          "  ('math_vector', 'semantic-product'),\n"
          "}\n"
          "ok = pairs == expected and len(report['results']) == 4\n"
          "if not ok:\n"
          "  print('pairs=', sorted(pairs))\n"
          "  print('count=', len(report['results']))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper covers math-vector-matrix and math-star phases") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_matrix_star_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_matrix_star.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_matrix_star.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures math_vector_matrix,math_star_repro "
      "--phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_matrix_star_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_matrix_star_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "pairs = {(row['fixture'], row['phase']) for row in report['results']}\n"
          "expected = {\n"
          "  ('math_vector_matrix', 'ast-semantic'),\n"
          "  ('math_vector_matrix', 'semantic-product'),\n"
          "  ('math_star_repro', 'ast-semantic'),\n"
          "  ('math_star_repro', 'semantic-product'),\n"
          "}\n"
          "ok = pairs == expected and len(report['results']) == 4\n"
          "if not ok:\n"
          "  print('pairs=', sorted(pairs))\n"
          "  print('count=', len(report['results']))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper covers inline-vs-import math fixture phases") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path inlineFixturePath =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "inline_math_body.prime";
  const std::filesystem::path importedFixturePath =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "imported_math_body.prime";
  const std::string inlineFixture = readFile(inlineFixturePath.string());
  const std::string importedFixture = readFile(importedFixturePath.string());
  REQUIRE_FALSE(inlineFixture.empty());
  REQUIRE_FALSE(importedFixture.empty());
  CHECK(inlineFixture.find("import /std/math/Vec2") == std::string::npos);
  CHECK(importedFixture.find("import /std/math/Vec2") != std::string::npos);

  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string fixturesValidateOutPath = writeTemp("semantic_memory_inline_import_fixture_validate.out", "");
  const std::string fixturesValidateErrPath = writeTemp("semantic_memory_inline_import_fixture_validate.err", "");
  const std::string fixturesValidateCmd =
      "python3 -c " +
      quoteShellArg(
          "import re, sys\n"
          "def normalize(path):\n"
          "  lines = []\n"
          "  with open(path, encoding='utf-8') as handle:\n"
          "    for raw in handle:\n"
          "      stripped = raw.strip()\n"
          "      if not stripped:\n"
          "        continue\n"
          "      if stripped.startswith('import '):\n"
          "        continue\n"
          "      lines.append(re.sub(r'\\s+', ' ', stripped))\n"
          "  return lines\n"
          "inline_lines = normalize(sys.argv[1])\n"
          "import_lines = normalize(sys.argv[2])\n"
          "ok = inline_lines == import_lines\n"
          "if not ok:\n"
          "  print('inline_lines=', inline_lines)\n"
          "  print('import_lines=', import_lines)\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(inlineFixturePath.string()) +
      " " + quoteShellArg(importedFixturePath.string()) +
      " > " + quoteShellArg(fixturesValidateOutPath) + " 2> " + quoteShellArg(fixturesValidateErrPath);
  CHECK(runCommand(fixturesValidateCmd) == 0);
  CHECK(readFile(fixturesValidateErrPath).empty());

  const std::string reportPath = writeTemp("semantic_memory_inline_import_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_inline_import.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_inline_import.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures inline_math_body,imported_math_body "
      "--phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_inline_import_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_inline_import_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "pairs = {(row['fixture'], row['phase']) for row in report['results']}\n"
          "expected = {\n"
          "  ('inline_math_body', 'ast-semantic'),\n"
          "  ('inline_math_body', 'semantic-product'),\n"
          "  ('imported_math_body', 'ast-semantic'),\n"
          "  ('imported_math_body', 'semantic-product'),\n"
          "}\n"
          "ok = pairs == expected and len(report['results']) == 4\n"
          "if not ok:\n"
          "  print('pairs=', sorted(pairs))\n"
          "  print('count=', len(report['results']))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper reports 1x-2x-4x scale slopes") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scale1Path =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "scale_1x.prime";
  const std::filesystem::path scale2Path =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "scale_2x.prime";
  const std::filesystem::path scale4Path =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "scale_4x.prime";
  const std::string scale1 = readFile(scale1Path.string());
  const std::string scale2 = readFile(scale2Path.string());
  const std::string scale4 = readFile(scale4Path.string());
  REQUIRE_FALSE(scale1.empty());
  REQUIRE_FALSE(scale2.empty());
  REQUIRE_FALSE(scale4.empty());

  CHECK(countOccurrences(scale1, "[return<i32>]\nf") == 2);
  CHECK(countOccurrences(scale2, "[return<i32>]\nf") == 4);
  CHECK(countOccurrences(scale4, "[return<i32>]\nf") == 8);
  CHECK(scale1.find("return(f1(0i32))") != std::string::npos);
  CHECK(scale2.find("return(f3(0i32))") != std::string::npos);
  CHECK(scale4.find("return(f7(0i32))") != std::string::npos);

  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_scale_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_scale.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_scale.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures scale_1x,scale_2x,scale_4x "
      "--phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_scale_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_scale_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, math, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "pairs = {(row['fixture'], row['phase']) for row in report['results']}\n"
          "expected_pairs = {\n"
          "  ('scale_1x', 'ast-semantic'),\n"
          "  ('scale_2x', 'ast-semantic'),\n"
          "  ('scale_4x', 'ast-semantic'),\n"
          "  ('scale_1x', 'semantic-product'),\n"
          "  ('scale_2x', 'semantic-product'),\n"
          "  ('scale_4x', 'semantic-product'),\n"
          "}\n"
          "slopes = {row['phase']: row for row in report.get('scale_slopes', [])}\n"
          "ok_pairs = pairs == expected_pairs and len(report['results']) == 6\n"
          "ok_phases = set(slopes.keys()) == {'ast-semantic', 'semantic-product'}\n"
          "ok_shape = True\n"
          "for phase in ('ast-semantic', 'semantic-product'):\n"
          "  row = slopes.get(phase)\n"
          "  if row is None:\n"
          "    ok_shape = False\n"
          "    continue\n"
          "  if row.get('x_axis') != [1, 2, 4]:\n"
          "    ok_shape = False\n"
          "  if row.get('fixtures') != ['scale_1x', 'scale_2x', 'scale_4x']:\n"
          "    ok_shape = False\n"
          "  rss = row.get('rss_bytes_per_scale_unit')\n"
          "  wall = row.get('wall_seconds_per_scale_unit')\n"
          "  if not isinstance(rss, (int, float)) or not math.isfinite(rss):\n"
          "    ok_shape = False\n"
          "  if not isinstance(wall, (int, float)) or not math.isfinite(wall):\n"
          "    ok_shape = False\n"
          "ok = ok_pairs and ok_phases and ok_shape\n"
          "if not ok:\n"
          "  print('pairs=', sorted(pairs))\n"
          "  print('scale_slopes=', report.get('scale_slopes'))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark required fixture-phase matrix fails when tuples are missing") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string reportPath = writeTemp("semantic_memory_required_pairs_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_required_pairs.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_required_pairs.err", "");

  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import,math_vector --phases ast-semantic,semantic-product "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_required_pairs_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_required_pairs_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "pairs = {(row.get('fixture'), row.get('phase')) for row in report.get('results', [])}\n"
          "fixtures = [\n"
          "  'math_star_repro',\n"
          "  'no_import',\n"
          "  'math_vector',\n"
          "  'math_vector_matrix',\n"
          "  'non_math_large_include',\n"
          "  'inline_math_body',\n"
          "  'imported_math_body',\n"
          "  'scale_1x',\n"
          "  'scale_2x',\n"
          "  'scale_4x',\n"
          "]\n"
          "phases = ['ast-semantic', 'semantic-product']\n"
          "required = {(fixture, phase) for fixture in fixtures for phase in phases}\n"
          "missing = sorted(required - pairs)\n"
          "ok = not missing\n"
          "if not ok:\n"
          "  print('missing=', missing)\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 1);
  CHECK(readFile(validateOutPath).find("missing=") != std::string::npos);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper defines method-target memoization delta report fields") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string script = readFile(scriptPath.string());
  REQUIRE_FALSE(script.empty());
  CHECK(script.find("def benchmark_row_semantic_product_force_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("value = row.get(\"semantic_product_force\", \"auto\")") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"auto\", \"null\", \"none\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"1\", \"true\", \"yes\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"0\", \"false\", \"no\"):") != std::string::npos);
  CHECK(script.find("def benchmark_row_fact_families_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("value = row.get(\"fact_families\", \"auto\")") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"all\", \"auto\", \"null\"):") != std::string::npos);
  CHECK(script.find("known = [family for family in SEMANTIC_COLLECTOR_FAMILIES if family in seen]") != std::string::npos);
  CHECK(script.find("unknown = sorted(token for token in seen if token not in SEMANTIC_COLLECTOR_FAMILIES)") != std::string::npos);
  CHECK(script.find("def benchmark_row_method_target_memoization_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("value = row.get(\"method_target_memoization\", \"on\")") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"on\", \"null\", \"none\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"1\", \"true\", \"yes\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"0\", \"false\", \"no\"):") != std::string::npos);
  CHECK(script.find("def benchmark_row_no_fact_emission_mode(row: dict) -> bool:") != std::string::npos);
  CHECK(script.find("if value is None:") != std::string::npos);
  CHECK(script.find("normalized not in (\"\", \"0\", \"false\", \"off\", \"no\", \"none\", \"null\")") != std::string::npos);
  CHECK(script.find("def benchmark_row_graph_local_auto_key_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("def benchmark_row_graph_local_auto_side_channel_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("def benchmark_row_graph_local_auto_dependency_scratch_mode(row: dict) -> str:") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"compact\", \"null\", \"none\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"flat\", \"null\", \"none\"):") != std::string::npos);
  CHECK(script.find("if normalized in (\"\", \"pmr\", \"null\", \"none\"):") != std::string::npos);
  CHECK(script.find("normalized = normalized.replace(\"_\", \"-\")") != std::string::npos);
  CHECK(script.find("bool(row.get(\"no_fact_emission\", False))") == std::string::npos);
  CHECK(script.find("return \"auto\"") != std::string::npos);
  CHECK(script.find("--semantic-product-force") != std::string::npos);
  CHECK(script.find("--semantic-validation-without-fact-emission") != std::string::npos);
  CHECK(script.find("--method-target-memoization") != std::string::npos);
  CHECK(script.find("--graph-local-auto-key-mode") != std::string::npos);
  CHECK(script.find("--graph-local-auto-side-channel-mode") != std::string::npos);
  CHECK(script.find("--graph-local-auto-dependency-scratch-mode") != std::string::npos);
  CHECK(script.find("--definition-validation-workers") != std::string::npos);
  CHECK(script.find("selected_semantic_product_force_modes") != std::string::npos);
  CHECK(script.find("selected_semantic_validation_without_fact_emission_modes") != std::string::npos);
  CHECK(script.find("selected_method_target_memoization_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_key_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_side_channel_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_dependency_scratch_modes") != std::string::npos);
  CHECK(script.find("selected_definition_validation_worker_modes") != std::string::npos);
  CHECK(script.find("benchmark_row_definition_validation_workers_mode") != std::string::npos);
  CHECK(script.find("SEMANTIC_PRODUCT_INDEX_FAMILY_LABELS") != std::string::npos);
  CHECK(script.find("SEMANTIC_PRODUCT_INDEX_FAMILY_PATTERNS") != std::string::npos);
  CHECK(script.find("def collect_semantic_product_index_family_counts(dump_text: str) -> dict:") != std::string::npos);
  CHECK(script.find("def benchmark_row_semantic_product_index_family_counts(row: dict) -> dict[str, int]:") != std::string::npos);
  CHECK(script.find("compute_semantic_product_force_deltas") != std::string::npos);
  CHECK(script.find("compute_semantic_validation_without_fact_emission_deltas") != std::string::npos);
  CHECK(script.find("compute_method_target_memoization_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_key_mode_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_side_channel_mode_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_dependency_scratch_mode_deltas") != std::string::npos);
  CHECK(script.find("compute_definition_validation_worker_mode_deltas") != std::string::npos);
  CHECK(script.find("\"semantic_product_force_deltas\"") != std::string::npos);
  CHECK(script.find("\"semantic_validation_without_fact_emission\"") != std::string::npos);
  CHECK(script.find("\"semantic_validation_without_fact_emission_deltas\"") != std::string::npos);
  CHECK(script.find("\"method_target_memoization_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_key_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_side_channel_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_dependency_scratch_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"definition_validation_worker_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"definition_validation_workers\"") != std::string::npos);
  CHECK(script.find("\"dump_sha256_identical\"") != std::string::npos);
  CHECK(script.find("\"semantic_product_index_family_counts\"") != std::string::npos);
  CHECK(script.find("\"semantic_product_index_family_counts_single_worker\"") != std::string::npos);
  CHECK(script.find("\"semantic_product_index_family_counts_dual_worker\"") != std::string::npos);
  CHECK(script.find("\"semantic_product_index_family_counts_identical\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_on_minus_off\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_no_fact_emission_minus_fact_emission\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_legacy_shadow_minus_compact\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_legacy_shadow_minus_flat\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_std_minus_pmr\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_dual_minus_single\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_on_minus_off\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_no_fact_emission_minus_fact_emission\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_legacy_shadow_minus_compact\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_legacy_shadow_minus_flat\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_std_minus_pmr\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_dual_minus_single\"") != std::string::npos);
}

TEST_CASE("benchmark regression checker passes for in-threshold report") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_benchmark_report.py";

  const std::string baselinePath = writeTemp(
      "benchmark_checker_pass_baseline.json",
      "{\n"
      "  \"schema\": \"primestruct_benchmark_baseline_v1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"phase\": \"runtime\",\n"
      "      \"benchmark\": \"aggregate\",\n"
      "      \"entry\": \"primestruct_cpp\",\n"
      "      \"max_mean_seconds\": 1.0,\n"
      "      \"max_artifact_size_bytes\": 120\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "benchmark_checker_pass_report.json",
      "{\n"
      "  \"schema\": \"primestruct_benchmark_report_v1\",\n"
      "  \"runtime_results\": [\n"
      "    {\n"
      "      \"benchmark\": \"aggregate\",\n"
      "      \"entry\": \"primestruct_cpp\",\n"
      "      \"mean_seconds\": 0.8,\n"
      "      \"median_seconds\": 0.7,\n"
      "      \"artifact_size_bytes\": 100\n"
      "    }\n"
      "  ],\n"
      "  \"compile_results\": []\n"
      "}\n");
  const std::string stdoutPath = writeTemp("benchmark_checker_pass.out", "");
  const std::string stderrPath = writeTemp("benchmark_checker_pass.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) + " --baseline " + quoteShellArg(baselinePath) +
      " --report " + quoteShellArg(reportPath) + " > " + quoteShellArg(stdoutPath) + " 2> " +
      quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stdoutPath).find("baseline check passed") != std::string::npos);
  CHECK(readFile(stderrPath).empty());
}

TEST_CASE("benchmark regression checker fails for threshold regression") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_benchmark_report.py";

  const std::string baselinePath = writeTemp(
      "benchmark_checker_fail_baseline.json",
      "{\n"
      "  \"schema\": \"primestruct_benchmark_baseline_v1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"phase\": \"compile\",\n"
      "      \"benchmark\": \"compile_speed\",\n"
      "      \"entry\": \"primestruct_cpp\",\n"
      "      \"max_mean_seconds\": 1.0,\n"
      "      \"max_artifact_size_bytes\": 100\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "benchmark_checker_fail_report.json",
      "{\n"
      "  \"schema\": \"primestruct_benchmark_report_v1\",\n"
      "  \"runtime_results\": [],\n"
      "  \"compile_results\": [\n"
      "    {\n"
      "      \"benchmark\": \"compile_speed\",\n"
      "      \"entry\": \"primestruct_cpp\",\n"
      "      \"mean_seconds\": 3.0,\n"
      "      \"median_seconds\": 2.5,\n"
      "      \"artifact_size_bytes\": 300\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string stdoutPath = writeTemp("benchmark_checker_fail.out", "");
  const std::string stderrPath = writeTemp("benchmark_checker_fail.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) + " --baseline " + quoteShellArg(baselinePath) +
      " --report " + quoteShellArg(reportPath) + " > " + quoteShellArg(stdoutPath) + " 2> " +
      quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 1);
  CHECK(readFile(stderrPath).find("regression check failed") != std::string::npos);
}

TEST_CASE("semantic memory budget checker passes for in-budget report") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_budget.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_budget_policy_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"max_worst_peak_rss_bytes\": 140\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_budget_report_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 110,\n"
      "      \"worst_wall_seconds\": 1.1\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string stdoutPath = writeTemp("semantic_memory_budget_pass.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_budget_pass.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) + " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) + " > " + quoteShellArg(stdoutPath) + " 2> " +
      quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stdoutPath).find("policy check passed") != std::string::npos);
  CHECK(readFile(stderrPath).empty());
}

TEST_SUITE_END();
