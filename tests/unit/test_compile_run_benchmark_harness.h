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
