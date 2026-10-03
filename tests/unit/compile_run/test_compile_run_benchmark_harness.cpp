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

TEST_CASE("benchmark baseline artifact includes native allocator coverage") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path baselinePath = repoRoot / "benchmarks" / "benchmark_baseline.json";
  const std::string baseline = readFile(baselinePath.string());
  REQUIRE_FALSE(baseline.empty());
  CHECK(baseline.find("\"schema\": \"primestruct_benchmark_baseline_v1\"") != std::string::npos);
  CHECK(baseline.find("\"benchmark\": \"aggregate\"") != std::string::npos);
  CHECK(baseline.find("\"benchmark\": \"json_scan\"") != std::string::npos);
  CHECK(baseline.find("\"benchmark\": \"json_parse\"") != std::string::npos);
  CHECK(baseline.find("\"benchmark\": \"compile_speed\"") != std::string::npos);
  CHECK(baseline.find("\"entry\": \"primestruct_cpp\"") != std::string::npos);
}

TEST_CASE("semantic memory benchmark helper keeps primary fixture first") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string script = readFile(scriptPath.string());
  REQUIRE_FALSE(script.empty());

  const std::string primaryFixtureDecl =
      "FixtureSpec(\"math_star_repro\", \"benchmarks/semantic_memory/fixtures/math_star_repro.prime\", \"primary\")";
  const std::string importsFixtureDecl =
      "FixtureSpec(\"no_import\", \"benchmarks/semantic_memory/fixtures/no_import.prime\", \"imports\")";
  const std::size_t primaryPos = script.find(primaryFixtureDecl);
  const std::size_t importsPos = script.find(importsFixtureDecl);
  REQUIRE(primaryPos != std::string::npos);
  REQUIRE(importsPos != std::string::npos);
  CHECK(primaryPos < importsPos);
}

TEST_CASE("semantic memory primary fixture stays minimal math-star reproducer") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path fixturePath =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "math_star_repro.prime";
  const std::string fixture = readFile(fixturePath.string());
  REQUIRE_FALSE(fixture.empty());

  CHECK(fixture.find("import /std/math/*") != std::string::npos);
  CHECK(fixture.find("import /std/math/Vec2") == std::string::npos);
  CHECK(fixture.find("import /std/math/Mat2") == std::string::npos);
  CHECK(fixture.find("[void]\nstep0()") != std::string::npos);
  CHECK(fixture.find("[void]\nstep1()") != std::string::npos);
  CHECK(fixture.find("[void]\nstep2()") != std::string::npos);
  CHECK(fixture.find("[return<i32>]\nmain()") != std::string::npos);
  CHECK(countOccurrences(fixture, "[void]\nstep") == 3);
  CHECK(fixture.size() <= 256);
}

TEST_CASE("semantic memory non-math include fixture keeps comparable definition scale") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path nonMathFixturePath =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "non_math_large_include.prime";
  const std::string nonMathFixture = readFile(nonMathFixturePath.string());
  REQUIRE_FALSE(nonMathFixture.empty());
  CHECK(nonMathFixture.find("import /std/bench_non_math/*") != std::string::npos);
  CHECK(nonMathFixture.find("import /std/math/*") == std::string::npos);

  const std::filesystem::path primecPath = repoRoot / "build-release" / "primec";
  if (!std::filesystem::exists(primecPath)) {
    INFO("primec not available in build-release");
    return;
  }

  const std::string nonMathDumpPath = writeTemp("semantic_memory_non_math_ast.txt", "");
  const std::string nonMathErrPath = writeTemp("semantic_memory_non_math_ast.err", "");
  const std::string mathDumpPath = writeTemp("semantic_memory_math_star_ast.txt", "");
  const std::string mathErrPath = writeTemp("semantic_memory_math_star_ast.err", "");

  const std::string nonMathCmd =
      quoteShellArg(primecPath.string()) + " " + quoteShellArg(nonMathFixturePath.string()) +
      " --entry=/bench/main --dump-stage=ast-semantic --emit=ir " +
      "> " + quoteShellArg(nonMathDumpPath) + " 2> " + quoteShellArg(nonMathErrPath);
  CHECK(runCommand(nonMathCmd) == 0);
  CHECK(readFile(nonMathErrPath).empty());

  const std::filesystem::path mathFixturePath =
      repoRoot / "benchmarks" / "semantic_memory" / "fixtures" / "math_star_repro.prime";
  const std::string mathCmd =
      quoteShellArg(primecPath.string()) + " " + quoteShellArg(mathFixturePath.string()) +
      " --entry=/bench/main --dump-stage=ast-semantic --emit=ir " +
      "> " + quoteShellArg(mathDumpPath) + " 2> " + quoteShellArg(mathErrPath);
  CHECK(runCommand(mathCmd) == 0);
  CHECK(readFile(mathErrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_non_math_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_non_math_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import re, sys\n"
          "def count_defs(path):\n"
          "  count = 0\n"
          "  with open(path, encoding='utf-8') as handle:\n"
          "    for line in handle:\n"
          "      if re.match(r'^  \\[.*\\] /', line):\n"
          "        count += 1\n"
          "  return count\n"
          "non_math = count_defs(sys.argv[1])\n"
          "math_star = count_defs(sys.argv[2])\n"
          "ratio = (non_math / math_star) if math_star else 0.0\n"
          "comparable_scale = non_math >= 64 and math_star >= 64 and ratio >= 0.80 and ratio <= 1.25\n"
          "math_pruned_scale = non_math >= 64 and math_star >= 4 and math_star <= 32 and ratio >= 2.0\n"
          "ok = comparable_scale or math_pruned_scale\n"
          "if not ok:\n"
          "  print('non_math_defs=', non_math)\n"
          "  print('math_star_defs=', math_star)\n"
          "  print('ratio=', ratio)\n"
          "  print('comparable_scale=', comparable_scale)\n"
          "  print('math_pruned_scale=', math_pruned_scale)\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(nonMathDumpPath) +
      " " + quoteShellArg(mathDumpPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory baseline report is checked in with fixture phase coverage") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path baselinePath = repoRoot / "benchmarks" / "semantic_memory_baseline_report.json";
  const std::string baseline = readFile(baselinePath.string());
  REQUIRE_FALSE(baseline.empty());

  const std::string validateOutPath = writeTemp("semantic_memory_baseline_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_baseline_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, math, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "required_counts = {\n"
          "  'direct_call_targets', 'method_call_targets', 'bridge_path_choices',\n"
          "  'binding_facts', 'return_facts', 'local_auto_facts',\n"
          "  'query_facts', 'try_facts', 'on_error_facts'\n"
          "}\n"
          "fixtures = report.get('fixtures', [])\n"
          "phases = report.get('phases', [])\n"
          "results = report.get('results', [])\n"
          "deltas = report.get('definition_validation_worker_mode_deltas', [])\n"
          "fixture_names = [row.get('name') for row in fixtures]\n"
          "pairs = {(row.get('fixture'), row.get('phase')) for row in results}\n"
          "worker_rows = {(row.get('fixture'), row.get('phase'), row.get('definition_validation_workers')) for row in results}\n"
          "expected_pairs = {(f, p) for f in fixture_names for p in phases}\n"
          "expected_worker_rows = {(f, p, w) for f in fixture_names for p in phases for w in (1, 2)}\n"
          "required_fields = (\n"
          "  'median_wall_seconds',\n"
          "  'worst_wall_seconds',\n"
          "  'median_peak_rss_bytes',\n"
          "  'worst_peak_rss_bytes',\n"
          ")\n"
          "def row_has_required_metrics(row):\n"
          "  for field in required_fields:\n"
          "    value = row.get(field)\n"
          "    if not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:\n"
          "      return False\n"
          "  return True\n"
          "ok = report.get('schema') == 'primestruct_semantic_memory_report_v1'\n"
          "ok = ok and report.get('runs') == 3\n"
          "ok = ok and phases == ['ast-semantic', 'semantic-product']\n"
          "ok = ok and report.get('benchmark_options', {}).get('definition_validation_workers') == 'both'\n"
          "ok = ok and len(fixtures) == 10\n"
          "ok = ok and len(results) == len(expected_worker_rows)\n"
          "ok = ok and pairs == expected_pairs\n"
          "ok = ok and worker_rows == expected_worker_rows\n"
          "semantic_rows = [row for row in results if row.get('phase') == 'semantic-product']\n"
          "ok = ok and len(semantic_rows) == len(fixtures) * 2\n"
          "ok = ok and all(row_has_required_metrics(row) for row in results)\n"
          "ok = ok and all(isinstance(row.get('key_cardinality'), dict) for row in semantic_rows)\n"
          "ok = ok and all(\n"
          "  isinstance(row['key_cardinality'].get('distinct_direct_call_target_keys'), int)\n"
          "  and isinstance(row['key_cardinality'].get('distinct_method_call_target_keys'), int)\n"
          "  and isinstance(row['key_cardinality'].get('max_target_key_length'), int)\n"
          "  for row in semantic_rows\n"
          ")\n"
          "ok = ok and all(set(row.get('semantic_product_index_family_counts', {}).keys()) == required_counts for row in semantic_rows)\n"
          "ok = ok and len(deltas) == len(expected_pairs)\n"
          "ok = ok and all(bool(delta.get('dump_sha256_identical')) for delta in deltas)\n"
          "ok = ok and all(\n"
          "  set(delta.get('semantic_product_index_family_counts_single_worker', {}).keys()) == required_counts\n"
          "  and set(delta.get('semantic_product_index_family_counts_dual_worker', {}).keys()) == required_counts\n"
          "  and bool(delta.get('semantic_product_index_family_counts_identical'))\n"
          "  for delta in deltas\n"
          ")\n"
          "ok = ok and all('is_expensive_threshold_offender' in row for row in results)\n"
          "ok = ok and isinstance(report.get('expensive_offenders', []), list)\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(baselinePath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory ctest targets keep dependency ordering and serialization") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path cmakePath = repoRoot / "CMakeLists.txt";
  const std::filesystem::path baselinePath = repoRoot / "benchmarks" / "semantic_memory_baseline_report.json";
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(baselinePath));

  const std::string validateOutPath = writeTemp("semantic_memory_ctest_policy_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_ctest_policy_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, re, sys\n"
          "baseline = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "cmake_text = open(sys.argv[2], encoding='utf-8').read()\n"
          "thresholds = baseline.get('expensive_thresholds', {})\n"
          "offenders = baseline.get('expensive_offenders', [])\n"
          "runtime_limit = thresholds.get('max_wall_seconds')\n"
          "rss_limit = thresholds.get('max_peak_rss_bytes')\n"
          "has_thresholds = runtime_limit == 3.0 and rss_limit == 500 * 1024 * 1024\n"
          "has_offenders = isinstance(offenders, list)\n"
          "offender_exceeds = has_offenders and all(\n"
          "  bool(row.get('exceeds_expensive_runtime_threshold'))\n"
          "  or bool(row.get('exceeds_expensive_rss_threshold'))\n"
          "  for row in offenders\n"
          ")\n"
          "benchmark_target_contract_guard = re.search(\n"
          "  r'add_custom_target\\(\\s*primestruct_semantic_memory_benchmark\\s*'\n"
          "  r'COMMAND .*?--mode benchmark\\s*.*?--skip-budget-check-in-benchmark\\s*'\n"
          "  r'.*?--trend-report \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_trend_report\\.json\\s*'\n"
          "  r'.*?COMMENT \"Running semantic memory benchmark artifact collection\"\\s*'\n"
          "  r'VERBATIM\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "benchmark_test_contract_guard = re.search(\n"
          "  r'add_test\\(\\s*NAME PrimeStruct_semantic_memory_benchmark\\s*'\n"
          "  r'COMMAND .*?--mode benchmark\\s*.*?--skip-budget-check-in-benchmark\\s*'\n"
          "  r'.*?--trend-report \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_trend_report\\.json\\s*'\n"
          "  r'.*?--artifacts-dir \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_artifacts\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "benchmark_timeout_guard = re.search(\n"
          "  r'set_tests_properties\\(\\s*PrimeStruct_semantic_memory_benchmark\\s*'\n"
          "  r'PROPERTIES\\s*RUN_SERIAL TRUE\\s*TIMEOUT 1800\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "parity_target_contract_guard = re.search(\n"
          "  r'add_custom_target\\(\\s*primestruct_semantic_memory_definition_worker_parity\\s*'\n"
          "  r'COMMAND .*?--mode benchmark\\s*.*?--skip-budget-check-in-benchmark\\s*'\n"
          "  r'.*?--benchmark-definition-validation-workers both\\s*'\n"
          "  r'.*?--trend-report \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_definition_worker_parity_trend_report\\.json\\s*'\n"
          "  r'.*?COMMENT \"Running semantic memory definition-worker parity artifact collection\"\\s*'\n"
          "  r'VERBATIM\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "parity_test_contract_guard = re.search(\n"
          "  r'add_test\\(\\s*NAME PrimeStruct_semantic_memory_definition_worker_parity\\s*'\n"
          "  r'COMMAND .*?--mode benchmark\\s*.*?--skip-budget-check-in-benchmark\\s*'\n"
          "  r'.*?--benchmark-definition-validation-workers both\\s*'\n"
          "  r'.*?--artifacts-dir \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_definition_worker_parity_artifacts\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "parity_timeout_guard = re.search(\n"
          "  r'set_tests_properties\\(\\s*PrimeStruct_semantic_memory_definition_worker_parity\\s*'\n"
          "  r'PROPERTIES\\s*RUN_SERIAL TRUE\\s*TIMEOUT 1800\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "trend_timeout_guard = re.search(\n"
          "  r'set_tests_properties\\(\\s*PrimeStruct_semantic_memory_trend\\s*'\n"
          "  r'PROPERTIES\\s*DEPENDS PrimeStruct_semantic_memory_benchmark\\s*'\n"
          "  r'TIMEOUT 600\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "graph_budget_gate_guard = re.search(\n"
          "  r'add_test\\(\\s*NAME PrimeStruct_graph_budget\\s*'\n"
          "  r'COMMAND .*?scripts/check_graph_budget\\.py\\s*'\n"
          "  r'.*?--baseline \\$\\{CMAKE_SOURCE_DIR\\}/benchmarks/type_graph_budget_baseline\\.json\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "semantic_budget_gate_guard = re.search(\n"
          "  r'add_test\\(\\s*NAME PrimeStruct_semantic_memory_trend\\s*'\n"
          "  r'COMMAND .*?--mode trend\\s*'\n"
          "  r'.*?--budget-report \\$\\{CMAKE_BINARY_DIR\\}/benchmarks/semantic_memory_budget_check_report\\.json\\s*'\n"
          "  r'.*?--repo-root \\$\\{CMAKE_SOURCE_DIR\\}\\s*\\)',\n"
          "  cmake_text,\n"
          "  re.S,\n"
          ") is not None\n"
          "ok = has_thresholds and has_offenders and offender_exceeds and benchmark_target_contract_guard and benchmark_test_contract_guard and benchmark_timeout_guard and parity_target_contract_guard and parity_test_contract_guard and parity_timeout_guard and trend_timeout_guard and graph_budget_gate_guard and semantic_budget_gate_guard\n"
          "if not ok:\n"
          "  print(json.dumps({\n"
          "    'thresholds': thresholds,\n"
          "    'offender_count': len(offenders) if isinstance(offenders, list) else -1,\n"
          "    'offender_exceeds': offender_exceeds,\n"
          "    'benchmark_target_contract_guard': benchmark_target_contract_guard,\n"
          "    'benchmark_test_contract_guard': benchmark_test_contract_guard,\n"
          "    'benchmark_timeout_guard': benchmark_timeout_guard,\n"
          "    'parity_target_contract_guard': parity_target_contract_guard,\n"
          "    'parity_test_contract_guard': parity_test_contract_guard,\n"
          "    'parity_timeout_guard': parity_timeout_guard,\n"
          "    'trend_timeout_guard': trend_timeout_guard,\n"
          "    'graph_budget_gate_guard': graph_budget_gate_guard,\n"
          "    'semantic_budget_gate_guard': semantic_budget_gate_guard,\n"
          "  }, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(baselinePath.string()) +
      " " + quoteShellArg(cmakePath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory budget policy artifacts are checked in") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path policyPath = repoRoot / "benchmarks" / "semantic_memory_budget_policy.json";
  const std::filesystem::path notePath = repoRoot / "docs" / "semantic_memory_benchmark_policy.md";
  const std::string policy = readFile(policyPath.string());
  const std::string note = readFile(notePath.string());

  REQUIRE_FALSE(policy.empty());
  REQUIRE_FALSE(note.empty());
  CHECK(policy.find("\"schema\": \"primestruct_semantic_memory_budget_policy_v1\"") != std::string::npos);
  CHECK(policy.find("\"window_size\": 3") != std::string::npos);
  CHECK(policy.find("\"minimum_regressions\": 2") != std::string::npos);
  CHECK(policy.find("\"fixture\": \"math_star_repro\"") != std::string::npos);
  CHECK(policy.find("\"phase\": \"semantic-product\"") != std::string::npos);
  CHECK(policy.find("\"fixture\": \"math_vector\"") != std::string::npos);
  CHECK(policy.find("\"max_worst_peak_rss_bytes\": 31281152") !=
        std::string::npos);
  CHECK(note.find("Sustained-Window Rule") != std::string::npos);
  CHECK(note.find("`math_vector:ast-semantic` keeps a hard cap of `31281152` bytes") !=
        std::string::npos);
  CHECK(note.find("Unified Semantic-Product Index Evidence") != std::string::npos);
  CHECK(note.find("`40` total rows") != std::string::npos);
  CHECK(note.find("key_cardinality") != std::string::npos);
  CHECK(note.find("semantic_product_index_family_counts") != std::string::npos);
  CHECK(note.find("definition_validation_worker_mode_deltas") != std::string::npos);
  CHECK(note.find("semantic_product_index_parity_evidence.json") != std::string::npos);
  CHECK(note.find("semantic_product_index_math_star_repro_report.json") != std::string::npos);
  CHECK(note.find("--fixtures math_star_repro") != std::string::npos);
  CHECK(note.find("--definition-validation-workers both") != std::string::npos);
}

TEST_CASE("semantic memory semantic-product index parity evidence artifact is checked in") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path evidencePath =
      repoRoot / "benchmarks" / "semantic_memory" / "semantic_product_index_parity_evidence.json";
  const std::string evidence = readFile(evidencePath.string());
  REQUIRE_FALSE(evidence.empty());

  const std::string validateOutPath = writeTemp("semantic_memory_index_parity_evidence_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_index_parity_evidence_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "artifact = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "required = {\n"
          "  'direct_call_targets', 'method_call_targets', 'bridge_path_choices',\n"
          "  'binding_facts', 'return_facts', 'local_auto_facts',\n"
          "  'query_facts', 'try_facts', 'on_error_facts'\n"
          "}\n"
          "rows = artifact.get('rows', [])\n"
          "deltas = artifact.get('definition_validation_worker_mode_deltas', [])\n"
          "ok = artifact.get('schema') == 'primestruct_semantic_product_index_parity_evidence_v1'\n"
          "ok = ok and artifact.get('baseline_report') == 'benchmarks/semantic_memory_baseline_report.json'\n"
          "ok = ok and artifact.get('report_script') == 'scripts/semantic_memory_benchmark.py'\n"
          "ok = ok and artifact.get('source_contracts') == [\n"
          "  'collect_semantic_product_index_family_counts',\n"
          "  'compute_definition_validation_worker_mode_deltas'\n"
          "]\n"
          "ok = ok and len(rows) == 2 and len(deltas) == 1\n"
          "if ok:\n"
          "  worker_modes = sorted(row.get('definition_validation_workers') for row in rows)\n"
          "  ok = ok and worker_modes == [1, 2]\n"
          "  row_counts = []\n"
          "  for row in rows:\n"
          "    counts = row.get('semantic_product_index_family_counts')\n"
          "    ok = ok and row.get('fixture') == 'semantic_product_index_contract_fixture'\n"
          "    ok = ok and row.get('phase') == 'semantic-product'\n"
          "    ok = ok and row.get('semantic_product_force') == 'auto'\n"
          "    ok = ok and row.get('no_fact_emission') == 'off'\n"
          "    ok = ok and row.get('fact_families') == 'auto'\n"
          "    ok = ok and isinstance(counts, dict)\n"
          "    if isinstance(counts, dict):\n"
          "      ok = ok and set(counts.keys()) == required\n"
          "      ok = ok and counts.get('direct_call_targets') == 2\n"
          "      ok = ok and all(isinstance(counts[name], int) and counts[name] >= 1 for name in required)\n"
          "      row_counts.append(counts)\n"
          "  delta = deltas[0]\n"
          "  single = delta.get('semantic_product_index_family_counts_single_worker')\n"
          "  dual = delta.get('semantic_product_index_family_counts_dual_worker')\n"
          "  ok = ok and delta.get('fixture') == 'semantic_product_index_contract_fixture'\n"
          "  ok = ok and delta.get('phase') == 'semantic-product'\n"
          "  ok = ok and delta.get('semantic_product_force') == 'auto'\n"
          "  ok = ok and delta.get('no_fact_emission') == 'off'\n"
          "  ok = ok and delta.get('fact_families') == 'auto'\n"
          "  ok = ok and bool(delta.get('semantic_product_index_family_counts_identical'))\n"
          "  ok = ok and isinstance(single, dict) and isinstance(dual, dict)\n"
          "  if isinstance(single, dict) and isinstance(dual, dict):\n"
          "    ok = ok and set(single.keys()) == required and set(dual.keys()) == required\n"
          "    ok = ok and len(row_counts) == 2 and row_counts[0] == row_counts[1] == single == dual\n"
          "if not ok:\n"
          "  print(json.dumps(artifact, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(evidencePath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory semantic-product index measured report artifact is checked in") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path reportPath =
      repoRoot / "benchmarks" / "semantic_memory" / "semantic_product_index_math_star_repro_report.json";
  const std::string report = readFile(reportPath.string());
  REQUIRE_FALSE(report.empty());

  const std::string validateOutPath = writeTemp("semantic_memory_index_measured_report_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_index_measured_report_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "required = {\n"
          "  'direct_call_targets', 'method_call_targets', 'bridge_path_choices',\n"
          "  'binding_facts', 'return_facts', 'local_auto_facts',\n"
          "  'query_facts', 'try_facts', 'on_error_facts'\n"
          "}\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('definition_validation_worker_mode_deltas', [])\n"
          "fixtures = report.get('fixtures', [])\n"
          "ok = report.get('schema') == 'primestruct_semantic_memory_report_v1'\n"
          "ok = ok and report.get('runs') == 3\n"
          "ok = ok and report.get('phases') == ['semantic-product']\n"
          "ok = ok and len(fixtures) == 1 and fixtures[0].get('name') == 'math_star_repro'\n"
          "ok = ok and len(rows) == 2 and len(deltas) == 1\n"
          "if ok:\n"
          "  worker_modes = sorted(row.get('definition_validation_workers') for row in rows)\n"
          "  ok = ok and worker_modes == [1, 2]\n"
          "  row_counts = []\n"
          "  for row in rows:\n"
          "    counts = row.get('semantic_product_index_family_counts')\n"
          "    ok = ok and row.get('fixture') == 'math_star_repro'\n"
          "    ok = ok and row.get('phase') == 'semantic-product'\n"
          "    ok = ok and row.get('semantic_product_force') == 'auto'\n"
          "    ok = ok and row.get('runs') == 3\n"
          "    ok = ok and isinstance(counts, dict)\n"
          "    if isinstance(counts, dict):\n"
          "      ok = ok and set(counts.keys()) == required\n"
          "      ok = ok and counts.get('binding_facts') == 3\n"
          "      ok = ok and counts.get('direct_call_targets') == 4\n"
          "      ok = ok and counts.get('query_facts') == 4\n"
          "      ok = ok and counts.get('return_facts') == 4\n"
          "      row_counts.append(counts)\n"
          "  delta = deltas[0]\n"
          "  single = delta.get('semantic_product_index_family_counts_single_worker')\n"
          "  dual = delta.get('semantic_product_index_family_counts_dual_worker')\n"
          "  ok = ok and delta.get('fixture') == 'math_star_repro'\n"
          "  ok = ok and delta.get('phase') == 'semantic-product'\n"
          "  ok = ok and bool(delta.get('dump_sha256_identical'))\n"
          "  ok = ok and bool(delta.get('semantic_product_index_family_counts_identical'))\n"
          "  ok = ok and isinstance(single, dict) and isinstance(dual, dict)\n"
          "  if isinstance(single, dict) and isinstance(dual, dict):\n"
          "    ok = ok and set(single.keys()) == required and set(dual.keys()) == required\n"
          "    ok = ok and len(row_counts) == 2 and row_counts[0] == row_counts[1] == single == dual\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory phase-one success criteria artifacts are checked in") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path criteriaPath =
      repoRoot / "benchmarks" / "semantic_memory_phase_one_success_criteria.json";
  const std::filesystem::path notePath =
      repoRoot / "docs" / "semantic_memory_phase_one_success_criteria.md";
  const std::string criteria = readFile(criteriaPath.string());
  const std::string note = readFile(notePath.string());

  REQUIRE_FALSE(criteria.empty());
  REQUIRE_FALSE(note.empty());
  CHECK(criteria.find("\"schema\": \"primestruct_semantic_memory_phase_one_success_criteria_v1\"") != std::string::npos);
  CHECK(criteria.find("\"fixture\": \"math_star_repro\"") != std::string::npos);
  CHECK(criteria.find("\"phase\": \"semantic-product\"") != std::string::npos);
  CHECK(criteria.find("\"baseline_value\": 9060352") != std::string::npos);
  CHECK(criteria.find("\"target_reduction_ratio\": 0.1") != std::string::npos);
  CHECK(criteria.find("\"absolute_cap_value\": 5368709120") != std::string::npos);
  CHECK(criteria.find("\"effective_target_value\": 8154317") != std::string::npos);
  CHECK(note.find("Primary Goal") != std::string::npos);
  CHECK(note.find("9060352") != std::string::npos);
  CHECK(note.find("8154317") != std::string::npos);
  CHECK(note.find("Sustained Rule") != std::string::npos);
}

TEST_CASE("semantic memory benchmark helper accepts benchmark-only collector controls") {
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

  const std::string reportPath = writeTemp("semantic_memory_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_benchmark.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_benchmark.err", "");

  const std::string cmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic --semantic-product-force on "
      "--fact-families callable_summaries --semantic-rss-checkpoints "
      "--repeat-compile-leak-check-runs 3 --report-json " +
      quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string report = readFile(reportPath);
  CHECK(report.find("\"schema\": \"primestruct_semantic_memory_report_v1\"") != std::string::npos);
  CHECK(report.find("\"fixture\": \"no_import\"") != std::string::npos);
  CHECK(report.find("\"phase\": \"ast-semantic\"") != std::string::npos);
  CHECK(report.find("\"semantic_product_force\": \"on\"") != std::string::npos);
  CHECK(report.find("\"fact_families\": \"callable_summaries\"") != std::string::npos);
  CHECK(report.find("\"method_target_memoization\": \"on\"") != std::string::npos);
  CHECK(report.find("\"graph_local_auto_key_mode\": \"compact\"") != std::string::npos);
  CHECK(report.find("\"graph_local_auto_side_channel_mode\": \"flat\"") != std::string::npos);
  CHECK(report.find("\"graph_local_auto_dependency_scratch_mode\": \"pmr\"") != std::string::npos);
  CHECK(report.find("\"semantic_rss_checkpoints\": true") != std::string::npos);
  CHECK(report.find("\"repeat_compile_leak_check_runs\": 3") != std::string::npos);
  CHECK(report.find("\"rss_before_bytes\"") != std::string::npos);
  CHECK(report.find("\"rss_after_bytes\"") != std::string::npos);
  CHECK(report.find("\"repeat_compile_leak_check\"") != std::string::npos);
  CHECK(report.find("\"rss_drift_bytes\"") != std::string::npos);
  CHECK(report.find("\"expensive_thresholds\"") != std::string::npos);
  CHECK(report.find("\"is_expensive_threshold_offender\": false") != std::string::npos);
}

TEST_CASE("semantic memory benchmark helper supports validator-vs-fact A/B mode") {
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

  const std::string reportPath = writeTemp("semantic_memory_no_fact_mode_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_no_fact_mode.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_no_fact_mode.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures math_vector --phases semantic-product "
      "--semantic-validation-without-fact-emission both "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_no_fact_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_no_fact_mode_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "by_mode = {bool(row.get('no_fact_emission', False)): row for row in rows}\n"
          "deltas = report.get('semantic_validation_without_fact_emission_deltas', [])\n"
          "options = report.get('benchmark_options', {})\n"
          "ok = options.get('semantic_validation_without_fact_emission') == 'both'\n"
          "ok = ok and len(rows) == 2 and len(by_mode) == 2 and False in by_mode and True in by_mode\n"
          "ok = ok and len(deltas) == 1\n"
          "if ok:\n"
          "  with_facts = by_mode[False].get('key_cardinality', {})\n"
          "  no_facts = by_mode[True].get('key_cardinality', {})\n"
          "  delta = deltas[0]\n"
          "  ok = ok and int(with_facts.get('distinct_direct_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(with_facts.get('distinct_method_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(no_facts.get('distinct_direct_call_target_keys', -1)) == 0\n"
          "  ok = ok and int(no_facts.get('distinct_method_call_target_keys', -1)) == 0\n"
          "  ok = ok and delta.get('fixture') == 'math_vector'\n"
          "  ok = ok and delta.get('phase') == 'semantic-product'\n"
          "  ok = ok and isinstance(\n"
          "    delta.get('median_peak_rss_bytes_no_fact_emission_minus_fact_emission'), int)\n"
          "  ok = ok and isinstance(\n"
          "    delta.get('median_wall_seconds_no_fact_emission_minus_fact_emission'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper supports semantic-product-force A/B mode") {
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

  const std::string reportPath = writeTemp("semantic_memory_force_mode_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_force_mode.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_force_mode.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--semantic-product-force both --semantic-phase-counters "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_force_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_force_mode_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "by_force = {str(row.get('semantic_product_force')): row for row in rows}\n"
          "deltas = report.get('semantic_product_force_deltas', [])\n"
          "options = report.get('benchmark_options', {})\n"
          "ok = options.get('semantic_product_force') == 'both'\n"
          "ok = ok and len(rows) == 2 and set(by_force.keys()) == {'on', 'off'}\n"
          "ok = ok and len(deltas) == 1\n"
          "if ok:\n"
          "  on_row = by_force['on']\n"
          "  off_row = by_force['off']\n"
          "  on_build = on_row.get('semantic_phase_counters', {}).get('semantic_product_build', {})\n"
          "  off_build = off_row.get('semantic_phase_counters', {}).get('semantic_product_build', {})\n"
          "  delta = deltas[0]\n"
          "  ok = ok and int(on_build.get('facts_produced', -1)) > 0\n"
          "  ok = ok and int(off_build.get('facts_produced', -1)) == 0\n"
          "  ok = ok and delta.get('fixture') == 'no_import'\n"
          "  ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_on_minus_off'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper keeps memoization deltas mode-scoped") {
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

  const std::string reportPath = writeTemp("semantic_memory_memo_mode_scope_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_memo_mode_scope.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_memo_mode_scope.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--method-target-memoization both --graph-local-auto-key-mode both "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_memo_mode_scope_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_memo_mode_scope_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('method_target_memoization_deltas', [])\n"
          "row_keys = {\n"
          "  (str(row.get('method_target_memoization')),\n"
          "   str(row.get('graph_local_auto_key_mode')))\n"
          "  for row in rows\n"
          "}\n"
          "delta_keys = {\n"
          "  (str(delta.get('graph_local_auto_key_mode')),\n"
          "   str(delta.get('graph_local_auto_side_channel_mode')),\n"
          "   str(delta.get('graph_local_auto_dependency_scratch_mode')))\n"
          "  for delta in deltas\n"
          "}\n"
          "ok = len(rows) == 4\n"
          "ok = ok and row_keys == {\n"
          "  ('on', 'compact'), ('off', 'compact'),\n"
          "  ('on', 'legacy-shadow'), ('off', 'legacy-shadow')\n"
          "}\n"
          "ok = ok and len(deltas) == 2\n"
          "ok = ok and delta_keys == {\n"
          "  ('compact', 'flat', 'pmr'),\n"
          "  ('legacy-shadow', 'flat', 'pmr')\n"
          "}\n"
          "if ok:\n"
          "  for delta in deltas:\n"
          "    ok = ok and delta.get('fixture') == 'no_import'\n"
          "    ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "    ok = ok and delta.get('semantic_product_force') == 'auto'\n"
          "    ok = ok and delta.get('no_fact_emission') is False\n"
          "    ok = ok and delta.get('fact_families') == 'auto'\n"
          "    ok = ok and isinstance(delta.get('median_peak_rss_bytes_on_minus_off'), int)\n"
          "    ok = ok and isinstance(delta.get('median_wall_seconds_on_minus_off'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper keeps key-mode deltas force-scoped") {
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

  const std::string reportPath = writeTemp("semantic_memory_key_mode_force_scope_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_key_mode_force_scope.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_key_mode_force_scope.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--semantic-product-force both --graph-local-auto-key-mode both "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_key_mode_force_scope_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_key_mode_force_scope_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('graph_local_auto_key_mode_deltas', [])\n"
          "row_keys = {\n"
          "  (str(row.get('semantic_product_force')),\n"
          "   str(row.get('graph_local_auto_key_mode')))\n"
          "  for row in rows\n"
          "}\n"
          "delta_forces = {str(delta.get('semantic_product_force')) for delta in deltas}\n"
          "ok = len(rows) == 4\n"
          "ok = ok and row_keys == {\n"
          "  ('on', 'compact'), ('on', 'legacy-shadow'),\n"
          "  ('off', 'compact'), ('off', 'legacy-shadow')\n"
          "}\n"
          "ok = ok and len(deltas) == 2\n"
          "ok = ok and delta_forces == {'on', 'off'}\n"
          "if ok:\n"
          "  for delta in deltas:\n"
          "    ok = ok and delta.get('fixture') == 'no_import'\n"
          "    ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "    ok = ok and delta.get('method_target_memoization') == 'on'\n"
          "    ok = ok and delta.get('no_fact_emission') is False\n"
          "    ok = ok and delta.get('fact_families') == 'auto'\n"
          "    ok = ok and delta.get('graph_local_auto_side_channel_mode') == 'flat'\n"
          "    ok = ok and delta.get('graph_local_auto_dependency_scratch_mode') == 'pmr'\n"
          "    ok = ok and isinstance(delta.get('median_peak_rss_bytes_legacy_shadow_minus_compact'), int)\n"
          "    ok = ok and isinstance(delta.get('median_wall_seconds_legacy_shadow_minus_compact'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper keeps side-channel deltas force-scoped") {
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

  const std::string reportPath = writeTemp("semantic_memory_side_channel_force_scope_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_side_channel_force_scope.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_side_channel_force_scope.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--semantic-product-force both --graph-local-auto-side-channel-mode both "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_side_channel_force_scope_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_side_channel_force_scope_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('graph_local_auto_side_channel_mode_deltas', [])\n"
          "row_keys = {\n"
          "  (str(row.get('semantic_product_force')),\n"
          "   str(row.get('graph_local_auto_side_channel_mode')))\n"
          "  for row in rows\n"
          "}\n"
          "delta_forces = {str(delta.get('semantic_product_force')) for delta in deltas}\n"
          "ok = len(rows) == 4\n"
          "ok = ok and row_keys == {\n"
          "  ('on', 'flat'), ('on', 'legacy-shadow'),\n"
          "  ('off', 'flat'), ('off', 'legacy-shadow')\n"
          "}\n"
          "ok = ok and len(deltas) == 2\n"
          "ok = ok and delta_forces == {'on', 'off'}\n"
          "if ok:\n"
          "  for delta in deltas:\n"
          "    ok = ok and delta.get('fixture') == 'no_import'\n"
          "    ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "    ok = ok and delta.get('method_target_memoization') == 'on'\n"
          "    ok = ok and delta.get('graph_local_auto_key_mode') == 'compact'\n"
          "    ok = ok and delta.get('no_fact_emission') is False\n"
          "    ok = ok and delta.get('fact_families') == 'auto'\n"
          "    ok = ok and delta.get('graph_local_auto_dependency_scratch_mode') == 'pmr'\n"
          "    ok = ok and isinstance(delta.get('median_peak_rss_bytes_legacy_shadow_minus_flat'), int)\n"
          "    ok = ok and isinstance(delta.get('median_wall_seconds_legacy_shadow_minus_flat'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper keeps dependency deltas force-scoped") {
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

  const std::string reportPath = writeTemp("semantic_memory_dependency_force_scope_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_dependency_force_scope.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_dependency_force_scope.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--semantic-product-force both --graph-local-auto-dependency-scratch-mode both "
      "--report-json " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_dependency_force_scope_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_dependency_force_scope_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('graph_local_auto_dependency_scratch_mode_deltas', [])\n"
          "row_keys = {\n"
          "  (str(row.get('semantic_product_force')),\n"
          "   str(row.get('graph_local_auto_dependency_scratch_mode')))\n"
          "  for row in rows\n"
          "}\n"
          "delta_forces = {str(delta.get('semantic_product_force')) for delta in deltas}\n"
          "ok = len(rows) == 4\n"
          "ok = ok and row_keys == {\n"
          "  ('on', 'pmr'), ('on', 'std'),\n"
          "  ('off', 'pmr'), ('off', 'std')\n"
          "}\n"
          "ok = ok and len(deltas) == 2\n"
          "ok = ok and delta_forces == {'on', 'off'}\n"
          "if ok:\n"
          "  for delta in deltas:\n"
          "    ok = ok and delta.get('fixture') == 'no_import'\n"
          "    ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "    ok = ok and delta.get('method_target_memoization') == 'on'\n"
          "    ok = ok and delta.get('graph_local_auto_key_mode') == 'compact'\n"
          "    ok = ok and delta.get('graph_local_auto_side_channel_mode') == 'flat'\n"
          "    ok = ok and delta.get('no_fact_emission') is False\n"
          "    ok = ok and delta.get('fact_families') == 'auto'\n"
          "    ok = ok and isinstance(delta.get('median_peak_rss_bytes_std_minus_pmr'), int)\n"
          "    ok = ok and isinstance(delta.get('median_wall_seconds_std_minus_pmr'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper toggles fact families independently") {
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

  const std::string directReportPath = writeTemp("semantic_memory_fact_direct_report.json", "");
  const std::string methodReportPath = writeTemp("semantic_memory_fact_method_report.json", "");
  const std::string bothReportPath = writeTemp("semantic_memory_fact_both_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_fact_families.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_fact_families.err", "");

  const auto runBenchmark = [&](const std::string& families, const std::string& reportPath) {
    const std::string cmd =
        "python3 " + quoteShellArg(scriptPath.string()) +
        " --repo-root " + quoteShellArg(repoRoot.string()) +
        " --primec " + quoteShellArg(primecPath.string()) +
        " --runs 1 --fixtures math_vector --phases semantic-product --fact-families " +
        quoteShellArg(families) +
        " --report-json " + quoteShellArg(reportPath) +
        " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
    CHECK(runCommand(cmd) == 0);
    CHECK(readFile(stderrPath).empty());
  };

  runBenchmark("direct_call_targets", directReportPath);
  runBenchmark("method_call_targets", methodReportPath);
  runBenchmark("direct_call_targets,method_call_targets", bothReportPath);

  const std::string validateOutPath = writeTemp("semantic_memory_fact_families_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_fact_families_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "def load_row(path):\n"
          "  report = json.load(open(path, encoding='utf-8'))\n"
          "  rows = report.get('results', [])\n"
          "  if len(rows) != 1:\n"
          "    return None\n"
          "  return rows[0]\n"
          "direct = load_row(sys.argv[1])\n"
          "method = load_row(sys.argv[2])\n"
          "both = load_row(sys.argv[3])\n"
          "ok = direct is not None and method is not None and both is not None\n"
          "if ok:\n"
          "  d = direct.get('key_cardinality', {})\n"
          "  m = method.get('key_cardinality', {})\n"
          "  b = both.get('key_cardinality', {})\n"
          "  ok = ok and direct.get('fact_families') == 'direct_call_targets'\n"
          "  ok = ok and method.get('fact_families') == 'method_call_targets'\n"
          "  ok = ok and both.get('fact_families') == 'direct_call_targets,method_call_targets'\n"
          "  ok = ok and int(d.get('distinct_direct_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(d.get('distinct_method_call_target_keys', -1)) == 0\n"
          "  ok = ok and int(m.get('distinct_direct_call_target_keys', -1)) == 0\n"
          "  ok = ok and int(m.get('distinct_method_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(b.get('distinct_direct_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(b.get('distinct_method_call_target_keys', -1)) > 0\n"
          "  ok = ok and int(d.get('max_target_key_length', -1)) > 0\n"
          "  ok = ok and int(m.get('max_target_key_length', -1)) > 0\n"
          "  ok = ok and int(b.get('max_target_key_length', -1)) > 0\n"
          "if not ok:\n"
          "  print('direct=', json.dumps(direct, indent=2, sort_keys=True) if direct else None)\n"
          "  print('method=', json.dumps(method, indent=2, sort_keys=True) if method else None)\n"
          "  print('both=', json.dumps(both, indent=2, sort_keys=True) if both else None)\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(directReportPath) +
      " " + quoteShellArg(methodReportPath) +
      " " + quoteShellArg(bothReportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper reports deterministic worker-mode parity") {
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

  const std::string reportPath = writeTemp("semantic_memory_definition_workers_report.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_definition_workers.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_definition_workers.err", "");
  const std::string benchmarkCmd =
      "python3 " + quoteShellArg(scriptPath.string()) +
      " --repo-root " + quoteShellArg(repoRoot.string()) +
      " --primec " + quoteShellArg(primecPath.string()) +
      " --runs 1 --fixtures no_import --phases ast-semantic "
      "--definition-validation-workers both --report-json " +
      quoteShellArg(reportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(benchmarkCmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string validateOutPath = writeTemp("semantic_memory_definition_workers_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_definition_workers_validate.err", "");
  const std::string validateCmd =
      "python3 -c " +
      quoteShellArg(
          "import json, sys\n"
          "report = json.load(open(sys.argv[1], encoding='utf-8'))\n"
          "rows = report.get('results', [])\n"
          "deltas = report.get('definition_validation_worker_mode_deltas', [])\n"
          "worker_modes = {int(row.get('definition_validation_workers', 0)) for row in rows}\n"
          "ok = len(rows) == 2\n"
          "ok = ok and worker_modes == {1, 2}\n"
          "ok = ok and len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('fixture') == 'no_import'\n"
          "  ok = ok and delta.get('phase') == 'ast-semantic'\n"
          "  ok = ok and bool(delta.get('dump_sha256_identical'))\n"
          "  ok = ok and isinstance(delta.get('median_peak_rss_bytes_dual_minus_single'), int)\n"
          "  ok = ok and isinstance(delta.get('median_wall_seconds_dual_minus_single'), (int, float))\n"
          "if not ok:\n"
          "  print(json.dumps(report, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(reportPath) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper fails worker parity on fact-family drift") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_worker_parity_failures.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_worker_parity_failures.err", "");
  const std::string validateCmd =
      "PYTHONDONTWRITEBYTECODE=1 python3 -c " +
      quoteShellArg(
          "import importlib.util, json, sys\n"
          "spec = importlib.util.spec_from_file_location('semantic_memory_benchmark', sys.argv[1])\n"
          "mod = importlib.util.module_from_spec(spec)\n"
          "sys.modules[spec.name] = mod\n"
          "spec.loader.exec_module(mod)\n"
          "report = {\n"
          "  'definition_validation_worker_mode_deltas': [\n"
          "    {\n"
          "      'fixture': 'stable',\n"
          "      'phase': 'semantic-product',\n"
          "      'dump_sha256_identical': True,\n"
          "      'semantic_product_index_family_counts_identical': True,\n"
          "      'semantic_product_index_family_counts_single_worker': {'return_facts': 1},\n"
          "      'semantic_product_index_family_counts_dual_worker': {'return_facts': 1},\n"
          "    },\n"
          "    {\n"
          "      'fixture': 'dump_drift',\n"
          "      'phase': 'semantic-product',\n"
          "      'dump_sha256_identical': False,\n"
          "      'semantic_product_index_family_counts_identical': True,\n"
          "      'semantic_product_index_family_counts_single_worker': {'return_facts': 1},\n"
          "      'semantic_product_index_family_counts_dual_worker': {'return_facts': 1},\n"
          "    },\n"
          "    {\n"
          "      'fixture': 'fact_drift',\n"
          "      'phase': 'semantic-product',\n"
          "      'dump_sha256_identical': True,\n"
          "      'semantic_product_index_family_counts_identical': False,\n"
          "      'semantic_product_index_family_counts_single_worker': {'return_facts': 1},\n"
          "      'semantic_product_index_family_counts_dual_worker': {'return_facts': 2},\n"
          "    },\n"
          "  ]\n"
          "}\n"
          "failures = mod.collect_definition_validation_worker_parity_failures(report)\n"
          "names = [row.get('fixture') for row in failures]\n"
          "ok = names == ['dump_drift', 'fact_drift']\n"
          "if ok:\n"
          "  drift = failures[1]\n"
          "  ok = drift.get('semantic_product_index_family_counts_single_worker') == {'return_facts': 1}\n"
          "  ok = ok and drift.get('semantic_product_index_family_counts_dual_worker') == {'return_facts': 2}\n"
          "if not ok:\n"
          "  print(json.dumps({'failures': failures}, indent=2, sort_keys=True))\n"
          "sys.exit(0 if ok else 1)\n") +
      " " + quoteShellArg(scriptPath.string()) +
      " > " + quoteShellArg(validateOutPath) + " 2> " + quoteShellArg(validateErrPath);
  CHECK(runCommand(validateCmd) == 0);
  CHECK(readFile(validateErrPath).empty());
}

TEST_CASE("semantic memory benchmark helper canonicalizes legacy all fact-family rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_legacy_fact_family_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_legacy_fact_family_validate.err", "");
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
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'all',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 100,\n"
          "    'worst_peak_rss_bytes': 120,\n"
          "    'median_wall_seconds': 1.2,\n"
          "    'worst_wall_seconds': 1.3,\n"
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
          "    'worst_peak_rss_bytes': 115,\n"
          "    'median_wall_seconds': 1.1,\n"
          "    'worst_wall_seconds': 1.2,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_semantic_product_force_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('fact_families') == 'auto'\n"
          "  ok = ok and delta.get('no_fact_emission') is False\n"
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

TEST_CASE("semantic memory benchmark helper canonicalizes blank force mode rows") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string validateOutPath = writeTemp("semantic_memory_blank_force_mode_validate.out", "");
  const std::string validateErrPath = writeTemp("semantic_memory_blank_force_mode_validate.err", "");
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
          "    'semantic_product_force': '',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'on',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 120,\n"
          "    'worst_peak_rss_bytes': 125,\n"
          "    'median_wall_seconds': 1.3,\n"
          "    'worst_wall_seconds': 1.4,\n"
          "  },\n"
          "  {\n"
          "    'fixture': 'no_import',\n"
          "    'phase': 'ast-semantic',\n"
          "    'semantic_product_force': 'auto',\n"
          "    'no_fact_emission': False,\n"
          "    'fact_families': 'auto',\n"
          "    'method_target_memoization': 'off',\n"
          "    'graph_local_auto_key_mode': 'compact',\n"
          "    'graph_local_auto_side_channel_mode': 'flat',\n"
          "    'graph_local_auto_dependency_scratch_mode': 'pmr',\n"
          "    'median_peak_rss_bytes': 110,\n"
          "    'worst_peak_rss_bytes': 118,\n"
          "    'median_wall_seconds': 1.2,\n"
          "    'worst_wall_seconds': 1.3,\n"
          "  },\n"
          "]\n"
          "deltas = mod.compute_method_target_memoization_deltas(rows)\n"
          "ok = len(deltas) == 1\n"
          "if ok:\n"
          "  delta = deltas[0]\n"
          "  ok = ok and delta.get('semantic_product_force') == 'auto'\n"
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

TEST_SUITE_END();
