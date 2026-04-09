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
      "--fact-families callable_summaries --report-json " +
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
  CHECK(report.find("\"expensive_thresholds\"") != std::string::npos);
  CHECK(report.find("\"is_expensive_threshold_offender\": false") != std::string::npos);
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

TEST_SUITE_END();
