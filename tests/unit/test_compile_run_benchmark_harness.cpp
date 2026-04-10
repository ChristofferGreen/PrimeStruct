#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.benchmark_harness");

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

TEST_CASE("semantic memory benchmark fixtures are checked in") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path fixtureRoot = repoRoot / "benchmarks" / "semantic_memory" / "fixtures";
  REQUIRE(std::filesystem::exists(fixtureRoot));
  CHECK(std::filesystem::exists(fixtureRoot / "math_star_repro.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "no_import.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "math_vector.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "math_vector_matrix.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "non_math_large_include.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "inline_math_body.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "imported_math_body.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "scale_1x.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "scale_2x.prime"));
  CHECK(std::filesystem::exists(fixtureRoot / "scale_4x.prime"));
}

TEST_CASE("semantic memory baseline report is checked in with fixture phase coverage") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path baselinePath = repoRoot / "benchmarks" / "semantic_memory_baseline_report.json";
  const std::string baseline = readFile(baselinePath.string());
  REQUIRE_FALSE(baseline.empty());
  CHECK(baseline.find("\"schema\": \"primestruct_semantic_memory_report_v1\"") != std::string::npos);
  CHECK(baseline.find("\"fixture\": \"math_star_repro\"") != std::string::npos);
  CHECK(baseline.find("\"fixture\": \"no_import\"") != std::string::npos);
  CHECK(baseline.find("\"phase\": \"ast-semantic\"") != std::string::npos);
  CHECK(baseline.find("\"phase\": \"semantic-product\"") != std::string::npos);
  CHECK(baseline.find("\"worst_wall_seconds\"") != std::string::npos);
  CHECK(baseline.find("\"worst_peak_rss_bytes\"") != std::string::npos);
  CHECK(baseline.find("\"expensive_thresholds\"") != std::string::npos);
  CHECK(baseline.find("\"is_expensive_threshold_offender\": true") != std::string::npos);
}

TEST_CASE("semantic memory benchmark ctest target stays serial and expensive") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  const std::string cmakeText = readFile(cmakePath.string());
  REQUIRE_FALSE(cmakeText.empty());

  CHECK(cmakeText.find("NAME PrimeStruct_semantic_memory_benchmark") != std::string::npos);
  CHECK(cmakeText.find("set_tests_properties(\n"
                       "    PrimeStruct_semantic_memory_benchmark\n"
                       "    PROPERTIES RUN_SERIAL TRUE TIMEOUT 1800 LABELS \"expensive\")") !=
        std::string::npos);
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
  CHECK(note.find("Sustained-Window Rule") != std::string::npos);
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
  CHECK(criteria.find("\"target_reduction_ratio\": 0.1") != std::string::npos);
  CHECK(criteria.find("\"absolute_cap_value\": 5368709120") != std::string::npos);
  CHECK(note.find("Primary Goal") != std::string::npos);
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

TEST_CASE("semantic memory benchmark helper defines method-target memoization delta report fields") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "semantic_memory_benchmark.py";
  const std::string script = readFile(scriptPath.string());
  REQUIRE_FALSE(script.empty());
  CHECK(script.find("--method-target-memoization") != std::string::npos);
  CHECK(script.find("--graph-local-auto-key-mode") != std::string::npos);
  CHECK(script.find("--graph-local-auto-side-channel-mode") != std::string::npos);
  CHECK(script.find("--graph-local-auto-dependency-scratch-mode") != std::string::npos);
  CHECK(script.find("selected_method_target_memoization_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_key_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_side_channel_modes") != std::string::npos);
  CHECK(script.find("selected_graph_local_auto_dependency_scratch_modes") != std::string::npos);
  CHECK(script.find("compute_method_target_memoization_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_key_mode_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_side_channel_mode_deltas") != std::string::npos);
  CHECK(script.find("compute_graph_local_auto_dependency_scratch_mode_deltas") != std::string::npos);
  CHECK(script.find("\"method_target_memoization_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_key_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_side_channel_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"graph_local_auto_dependency_scratch_mode_deltas\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_on_minus_off\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_legacy_shadow_minus_compact\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_legacy_shadow_minus_flat\"") != std::string::npos);
  CHECK(script.find("\"median_peak_rss_bytes_std_minus_pmr\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_on_minus_off\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_legacy_shadow_minus_compact\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_legacy_shadow_minus_flat\"") != std::string::npos);
  CHECK(script.find("\"median_wall_seconds_std_minus_pmr\"") != std::string::npos);
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
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 140,\n"
      "      \"max_worst_wall_seconds\": 1.4\n"
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

TEST_CASE("semantic memory budget checker fails when report tuple lacks policy entry") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_budget.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_budget_policy_missing_tuple.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 140,\n"
      "      \"max_worst_wall_seconds\": 1.4\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_budget_report_missing_tuple.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 110,\n"
      "      \"worst_wall_seconds\": 1.1\n"
      "    },\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"semantic-product\",\n"
      "      \"worst_peak_rss_bytes\": 110,\n"
      "      \"worst_wall_seconds\": 1.1\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string stdoutPath = writeTemp("semantic_memory_budget_missing_tuple.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_budget_missing_tuple.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) + " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) + " > " + quoteShellArg(stdoutPath) + " 2> " +
      quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 1);
  const std::string stderrText = readFile(stderrPath);
  CHECK(stderrText.find("budget check failed") != std::string::npos);
  CHECK(stderrText.find("missing policy entry for report result toy:semantic-product") != std::string::npos);
}

TEST_CASE("semantic memory budget checker fails for sustained regressions") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_budget.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_budget_policy_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 200,\n"
      "      \"max_worst_wall_seconds\": 2.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string historyOnePath = writeTemp(
      "semantic_memory_budget_history_1.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 130,\n"
      "      \"worst_wall_seconds\": 1.3\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string historyTwoPath = writeTemp(
      "semantic_memory_budget_history_2.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 100,\n"
      "      \"worst_wall_seconds\": 1.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_budget_report_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 131,\n"
      "      \"worst_wall_seconds\": 1.31\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string stdoutPath = writeTemp("semantic_memory_budget_fail.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_budget_fail.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) + " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-report " + quoteShellArg(historyOnePath) +
      " --history-report " + quoteShellArg(historyTwoPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 1);
  const std::string stderrText = readFile(stderrPath);
  CHECK(stderrText.find("budget check failed") != std::string::npos);
  CHECK(stderrText.find("sustained RSS regression") != std::string::npos);
  CHECK(stderrText.find("sustained wall regression") != std::string::npos);
}

TEST_CASE("semantic memory phase-one checker passes for current and sustained window") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_phase_one_success.py";

  const std::string criteriaPath = writeTemp(
      "semantic_memory_phase_one_criteria_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_phase_one_success_criteria_v1\",\n"
      "  \"criteria\": [\n"
      "    {\n"
      "      \"id\": \"toy_phase_one\",\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"metric\": \"worst_peak_rss_bytes\",\n"
      "      \"baseline_value\": 100,\n"
      "      \"target_reduction_ratio\": 0.1,\n"
      "      \"absolute_cap_value\": 95,\n"
      "      \"effective_target_value\": 90\n"
      "    }\n"
      "  ],\n"
      "  \"sustained_gate\": {\"window_size\": 3, \"minimum_passes\": 2}\n"
      "}\n");
  const std::string historyOnePath = writeTemp(
      "semantic_memory_phase_one_history_pass_1.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 89,\n"
      "      \"worst_wall_seconds\": 1.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string historyTwoPath = writeTemp(
      "semantic_memory_phase_one_history_pass_2.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 92,\n"
      "      \"worst_wall_seconds\": 1.1\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_phase_one_report_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 90,\n"
      "      \"worst_wall_seconds\": 1.2\n"
      "    }\n"
      "  ]\n"
      "}\n");

  const std::string checkerReportPath = writeTemp("semantic_memory_phase_one_check_pass.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_phase_one_pass.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_phase_one_pass.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --criteria " + quoteShellArg(criteriaPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-report " + quoteShellArg(historyOnePath) +
      " --history-report " + quoteShellArg(historyTwoPath) +
      " --report-json " + quoteShellArg(checkerReportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());
  CHECK(readFile(stdoutPath).find("phase-one check passed") != std::string::npos);

  const std::string checkerReport = readFile(checkerReportPath);
  CHECK(checkerReport.find("\"schema\": \"primestruct_semantic_memory_phase_one_check_report_v1\"") !=
        std::string::npos);
  CHECK(checkerReport.find("\"failure_count\": 0") != std::string::npos);
  CHECK(checkerReport.find("\"sustained_pass\": true") != std::string::npos);
}

TEST_CASE("semantic memory phase-one checker fails when sustained window misses target") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_phase_one_success.py";

  const std::string criteriaPath = writeTemp(
      "semantic_memory_phase_one_criteria_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_phase_one_success_criteria_v1\",\n"
      "  \"criteria\": [\n"
      "    {\n"
      "      \"id\": \"toy_phase_one\",\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"metric\": \"worst_peak_rss_bytes\",\n"
      "      \"baseline_value\": 100,\n"
      "      \"target_reduction_ratio\": 0.1,\n"
      "      \"absolute_cap_value\": 95,\n"
      "      \"effective_target_value\": 90\n"
      "    }\n"
      "  ],\n"
      "  \"sustained_gate\": {\"window_size\": 3, \"minimum_passes\": 2}\n"
      "}\n");
  const std::string historyOnePath = writeTemp(
      "semantic_memory_phase_one_history_fail_1.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 95,\n"
      "      \"worst_wall_seconds\": 1.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string historyTwoPath = writeTemp(
      "semantic_memory_phase_one_history_fail_2.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 94,\n"
      "      \"worst_wall_seconds\": 1.1\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_phase_one_report_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 89,\n"
      "      \"worst_wall_seconds\": 1.2\n"
      "    }\n"
      "  ]\n"
      "}\n");

  const std::string stdoutPath = writeTemp("semantic_memory_phase_one_fail.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_phase_one_fail.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --criteria " + quoteShellArg(criteriaPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-report " + quoteShellArg(historyOnePath) +
      " --history-report " + quoteShellArg(historyTwoPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 1);
  const std::string stderrText = readFile(stderrPath);
  CHECK(stderrText.find("phase-one check failed") != std::string::npos);
  CHECK(stderrText.find("sustained window failed") != std::string::npos);
}

TEST_CASE("semantic memory trend checker passes without history reports") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_trend.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_trend_policy_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 200,\n"
      "      \"max_worst_wall_seconds\": 2.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_trend_report_pass.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 130,\n"
      "      \"worst_wall_seconds\": 1.3\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::filesystem::path missingHistoryDir = testScratchPath("semantic_memory_trend/missing_history");
  const std::string stdoutPath = writeTemp("semantic_memory_trend_pass.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_trend_pass.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-dir " + quoteShellArg(missingHistoryDir.string()) +
      " --history-limit 2 > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stdoutPath).find("no history reports discovered") != std::string::npos);
  CHECK(readFile(stderrPath).empty());
}

TEST_CASE("semantic memory trend checker writes trend summary report") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_trend.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_trend_policy_report.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 200,\n"
      "      \"max_worst_wall_seconds\": 2.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_trend_report_summary.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 100,\n"
      "      \"worst_wall_seconds\": 1.0\n"
      "    }\n"
      "  ]\n"
      "}\n");

  const std::filesystem::path historyDir = testScratchDir("semantic_memory_trend_report_history");
  {
    std::ofstream history(historyDir / "semantic_memory_report_20260103.json");
    REQUIRE(history.good());
    history << "{\n"
               "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
               "  \"results\": [\n"
               "    {\n"
               "      \"fixture\": \"toy\",\n"
               "      \"phase\": \"ast-semantic\",\n"
               "      \"worst_peak_rss_bytes\": 110,\n"
               "      \"worst_wall_seconds\": 1.1\n"
               "    }\n"
               "  ]\n"
               "}\n";
    REQUIRE(history.good());
  }

  const std::string budgetReportPath = writeTemp("semantic_memory_trend_budget_summary.json", "");
  const std::string trendReportPath = writeTemp("semantic_memory_trend_summary.json", "");
  const std::string stdoutPath = writeTemp("semantic_memory_trend_summary.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_trend_summary.err", "");

  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --history-limit 1" +
      " --report-json " + quoteShellArg(budgetReportPath) +
      " --trend-report-json " + quoteShellArg(trendReportPath) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string trendReport = readFile(trendReportPath);
  CHECK(trendReport.find("\"schema\": \"primestruct_semantic_memory_trend_report_v1\"") != std::string::npos);
  CHECK(trendReport.find("\"status\": \"passed\"") != std::string::npos);
  CHECK(trendReport.find("\"checker_exit_code\": 0") != std::string::npos);
  CHECK(trendReport.find("semantic_memory_report_20260103.json") != std::string::npos);
  CHECK(trendReport.find("\"history_count\": 1") != std::string::npos);
  CHECK(trendReport.find("\"failure_count\": 0") != std::string::npos);
}

TEST_CASE("semantic memory trend checker fails for sustained regressions from history dir") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_trend.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_trend_policy_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 200,\n"
      "      \"max_worst_wall_seconds\": 2.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_trend_report_fail.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 131,\n"
      "      \"worst_wall_seconds\": 1.31\n"
      "    }\n"
      "  ]\n"
      "}\n");

  const std::filesystem::path historyDir = testScratchDir("semantic_memory_trend_history");
  {
    std::ofstream oldHistory(historyDir / "semantic_memory_report_20260101.json");
    REQUIRE(oldHistory.good());
    oldHistory << "{\n"
                  "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
                  "  \"results\": [\n"
                  "    {\n"
                  "      \"fixture\": \"toy\",\n"
                  "      \"phase\": \"ast-semantic\",\n"
                  "      \"worst_peak_rss_bytes\": 100,\n"
                  "      \"worst_wall_seconds\": 1.0\n"
                  "    }\n"
                  "  ]\n"
                  "}\n";
    REQUIRE(oldHistory.good());
  }
  {
    std::ofstream recentHistory(historyDir / "semantic_memory_report_20260102.json");
    REQUIRE(recentHistory.good());
    recentHistory << "{\n"
                     "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
                     "  \"results\": [\n"
                     "    {\n"
                     "      \"fixture\": \"toy\",\n"
                     "      \"phase\": \"ast-semantic\",\n"
                     "      \"worst_peak_rss_bytes\": 130,\n"
                     "      \"worst_wall_seconds\": 1.3\n"
                     "    }\n"
                     "  ]\n"
                     "}\n";
    REQUIRE(recentHistory.good());
  }

  const std::string stdoutPath = writeTemp("semantic_memory_trend_fail.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_trend_fail.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --history-limit 2 > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 1);
  CHECK(readFile(stdoutPath).find("history reports:") != std::string::npos);
  const std::string stderrText = readFile(stderrPath);
  CHECK(stderrText.find("budget check failed") != std::string::npos);
  CHECK(stderrText.find("sustained RSS regression") != std::string::npos);
  CHECK(stderrText.find("sustained wall regression") != std::string::npos);
}

TEST_CASE("semantic memory trend checker ignores duplicate current report in history dir") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path checkerPath = repoRoot / "scripts" / "check_semantic_memory_trend.py";

  const std::string policyPath = writeTemp(
      "semantic_memory_trend_policy_dedup.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_budget_policy_v1\",\n"
      "  \"sustained_window\": {\"window_size\": 3, \"minimum_regressions\": 2},\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"soft_max_worst_peak_rss_bytes\": 120,\n"
      "      \"soft_max_worst_wall_seconds\": 1.2,\n"
      "      \"max_worst_peak_rss_bytes\": 200,\n"
      "      \"max_worst_wall_seconds\": 2.0\n"
      "    }\n"
      "  ]\n"
      "}\n");
  const std::string reportPath = writeTemp(
      "semantic_memory_trend_report_dedup.json",
      "{\n"
      "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
      "  \"results\": [\n"
      "    {\n"
      "      \"fixture\": \"toy\",\n"
      "      \"phase\": \"ast-semantic\",\n"
      "      \"worst_peak_rss_bytes\": 131,\n"
      "      \"worst_wall_seconds\": 1.31\n"
      "    }\n"
      "  ]\n"
      "}\n");

  const std::filesystem::path historyDir = testScratchDir("semantic_memory_trend_history_dedup");
  {
    std::ofstream oldHistory(historyDir / "semantic_memory_report_20260101.json");
    REQUIRE(oldHistory.good());
    oldHistory << "{\n"
                  "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
                  "  \"results\": [\n"
                  "    {\n"
                  "      \"fixture\": \"toy\",\n"
                  "      \"phase\": \"ast-semantic\",\n"
                  "      \"worst_peak_rss_bytes\": 100,\n"
                  "      \"worst_wall_seconds\": 1.0\n"
                  "    }\n"
                  "  ]\n"
                  "}\n";
    REQUIRE(oldHistory.good());
  }
  {
    std::ofstream duplicateHistory(historyDir / "semantic_memory_report_20260102.json");
    REQUIRE(duplicateHistory.good());
    duplicateHistory << "{\n"
                        "  \"schema\": \"primestruct_semantic_memory_report_v1\",\n"
                        "  \"results\": [\n"
                        "    {\n"
                        "      \"fixture\": \"toy\",\n"
                        "      \"phase\": \"ast-semantic\",\n"
                        "      \"worst_peak_rss_bytes\": 131,\n"
                        "      \"worst_wall_seconds\": 1.31\n"
                        "    }\n"
                        "  ]\n"
                        "}\n";
    REQUIRE(duplicateHistory.good());
  }

  const std::string stdoutPath = writeTemp("semantic_memory_trend_dedup.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_trend_dedup.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(checkerPath.string()) +
      " --policy " + quoteShellArg(policyPath) +
      " --report " + quoteShellArg(reportPath) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --history-limit 2 > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);
  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::string stdoutText = readFile(stdoutPath);
  CHECK(stdoutText.find("history reports:") != std::string::npos);
  CHECK(stdoutText.find("semantic_memory_report_20260101.json") != std::string::npos);
  CHECK(stdoutText.find("semantic_memory_report_20260102.json") == std::string::npos);
}

TEST_CASE("semantic memory ci artifact wrapper captures reports on success") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch = testScratchDir("semantic_memory_ci_artifacts_success");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";

  const std::string benchScript = writeTemp(
      "semantic_memory_ci_artifacts_success_bench.py",
      "import json, pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "payload = {\n"
      "  'schema': 'primestruct_semantic_memory_report_v1',\n"
      "  'results': [{'fixture': 'toy', 'phase': 'ast-semantic', 'worst_peak_rss_bytes': 1, 'worst_wall_seconds': 0.1}],\n"
      "}\n"
      "path.write_text(json.dumps(payload) + '\\n', encoding='utf-8')\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_success_trend.py",
      "import json, pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "payload = {'schema': 'primestruct_semantic_memory_budget_check_report_v1', 'entries': [], 'failure_count': 0}\n"
      "path.write_text(json.dumps(payload) + '\\n', encoding='utf-8')\n");

  const std::string benchmarkCmdOverride =
      "python3 " + quoteShellArg(benchScript) + " " + quoteShellArg(reportPath.string());
  const std::string trendCmdOverride =
      "python3 " + quoteShellArg(trendScript) + " " + quoteShellArg(budgetPath.string());

  const std::string stdoutPath = writeTemp("semantic_memory_ci_artifacts_success.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_ci_artifacts_success.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode full --run-label test_success --repo-root " + quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());
  const std::string stdoutText = readFile(stdoutPath);
  CHECK(stdoutText.find("status=passed") != std::string::npos);

  const std::filesystem::path latestManifest = artifactsDir / "test_success_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"status\": \"passed\"") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": false") != std::string::npos);
  CHECK(manifest.find("\"benchmark_report\": \"semantic_memory_report.json\"") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": \"semantic_memory_budget_report.json\"") != std::string::npos);

  bool sawHistory = false;
  for (const auto &entry : std::filesystem::directory_iterator(historyDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind("semantic_memory_report_test_success_", 0) == 0 &&
        entry.path().extension() == ".json") {
      sawHistory = true;
      break;
    }
  }
  CHECK(sawHistory);
}

TEST_CASE("semantic memory ci artifact wrapper writes failure artifacts") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch = testScratchDir("semantic_memory_ci_artifacts_failure");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";

  const std::string failBenchScript = writeTemp(
      "semantic_memory_ci_artifacts_failure_bench.py",
      "import sys\n"
      "sys.stderr.write('benchmark failed intentionally\\n')\n"
      "sys.exit(7)\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_failure_trend.py",
      "import pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "path.write_text('unused\\n', encoding='utf-8')\n");

  const std::string benchmarkCmdOverride = "python3 " + quoteShellArg(failBenchScript);
  const std::string trendCmdOverride =
      "python3 " + quoteShellArg(trendScript) + " " + quoteShellArg(budgetPath.string());

  const std::string stdoutPath = writeTemp("semantic_memory_ci_artifacts_failure.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_ci_artifacts_failure.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode full --run-label test_failure --repo-root " + quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 7);
  CHECK(readFile(stderrPath).empty());
  const std::string stdoutText = readFile(stdoutPath);
  CHECK(stdoutText.find("status=failed") != std::string::npos);

  const std::filesystem::path latestManifest = artifactsDir / "test_failure_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"status\": \"failed\"") != std::string::npos);
  CHECK(manifest.find("\"benchmark_exit_code\": 7") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": true") != std::string::npos);
  CHECK(manifest.find("\"benchmark_report\": null") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": null") != std::string::npos);
}

TEST_CASE("semantic memory ci artifact wrapper ignores stale reports on benchmark failure") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch = testScratchDir("semantic_memory_ci_artifacts_stale_failure");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";

  {
    std::ofstream staleReport(reportPath);
    REQUIRE(staleReport.good());
    staleReport << "{\"schema\":\"primestruct_semantic_memory_report_v1\",\"results\":[]}\n";
  }
  {
    std::ofstream staleBudget(budgetPath);
    REQUIRE(staleBudget.good());
    staleBudget << "{\"schema\":\"primestruct_semantic_memory_budget_check_report_v1\",\"entries\":[]}\n";
  }

  const std::string failBenchScript = writeTemp(
      "semantic_memory_ci_artifacts_stale_failure_bench.py",
      "import sys\n"
      "sys.stderr.write('benchmark failed intentionally\\n')\n"
      "sys.exit(9)\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_stale_failure_trend.py",
      "import sys\n"
      "sys.stderr.write('trend should be skipped\\n')\n"
      "sys.exit(3)\n");

  const std::string benchmarkCmdOverride = "python3 " + quoteShellArg(failBenchScript);
  const std::string trendCmdOverride = "python3 " + quoteShellArg(trendScript);

  const std::string stdoutPath = writeTemp("semantic_memory_ci_artifacts_stale_failure.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_ci_artifacts_stale_failure.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode full --run-label test_stale_failure --repo-root " + quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 9);
  CHECK(readFile(stderrPath).empty());

  const std::filesystem::path latestManifest = artifactsDir / "test_stale_failure_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"status\": \"failed\"") != std::string::npos);
  CHECK(manifest.find("\"benchmark_exit_code\": 9") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": true") != std::string::npos);
  CHECK(manifest.find("\"benchmark_report\": null") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": null") != std::string::npos);
}

TEST_CASE("tsan semantics smoke is gated behind optional-ci wiring") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path cmakePath = repoRoot / "CMakeLists.txt";
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "run_semantics_tsan_smoke.sh";

  const std::string cmakeText = readFile(cmakePath.string());
  CHECK(cmakeText.find("option(PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE") != std::string::npos);
  CHECK(cmakeText.find("add_executable(PrimeStruct_semantics_tsan_smoke") != std::string::npos);
  CHECK(cmakeText.find("LABELS \"optional-ci;tsan;serial-required\"") != std::string::npos);

  REQUIRE(std::filesystem::exists(scriptPath));
  const std::string scriptText = readFile(scriptPath.string());
  CHECK(scriptText.find("PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON") != std::string::npos);
  CHECK(scriptText.find("PrimeStruct_semantics_tsan_smoke") != std::string::npos);
}

TEST_SUITE_END();
