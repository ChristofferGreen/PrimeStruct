#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.benchmark_harness");


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
      "      \"max_worst_peak_rss_bytes\": 140\n"
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
      "      \"max_worst_peak_rss_bytes\": 200\n"
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
      "      \"max_worst_peak_rss_bytes\": 200\n"
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
      "      \"max_worst_peak_rss_bytes\": 200\n"
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
      "      \"max_worst_peak_rss_bytes\": 200\n"
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
      "      \"max_worst_peak_rss_bytes\": 200\n"
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
    std::ofstream recentHistory(historyDir / "semantic_memory_report_20260102.json");
    REQUIRE(recentHistory.good());
    recentHistory << "{\n"
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
    REQUIRE(recentHistory.good());
  }
  {
    std::ofstream duplicateHistory(historyDir / "semantic_memory_report_20260103.json");
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
  CHECK(stdoutText.find("semantic_memory_report_20260102.json") != std::string::npos);
  CHECK(stdoutText.find("semantic_memory_report_20260103.json") == std::string::npos);
}

TEST_CASE("semantic memory ci artifact wrapper forwards definition worker mode") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath =
      repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::string script = readFile(wrapperPath.string());
  REQUIRE_FALSE(script.empty());
  CHECK(script.find("--benchmark-definition-validation-workers") != std::string::npos);
  CHECK(script.find("args.benchmark_definition_validation_workers") != std::string::npos);
  CHECK(script.find("--definition-validation-workers") != std::string::npos);
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

TEST_CASE("semantic memory ci artifact wrapper benchmark mode runs budget gate") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch = testScratchDir("semantic_memory_ci_artifacts_benchmark_mode");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path trendPath = rootScratch / "semantic_memory_trend_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";

  const std::string benchScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_bench.py",
      "import json, pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "payload = {\n"
      "  'schema': 'primestruct_semantic_memory_report_v1',\n"
      "  'results': [{'fixture': 'toy', 'phase': 'ast-semantic', 'worst_peak_rss_bytes': 1, 'worst_wall_seconds': 0.1}],\n"
      "}\n"
      "path.write_text(json.dumps(payload) + '\\n', encoding='utf-8')\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_trend.py",
      "import json, pathlib, sys\n"
      "budget_path = pathlib.Path(sys.argv[1])\n"
      "trend_path = pathlib.Path(sys.argv[2])\n"
      "budget_path.parent.mkdir(parents=True, exist_ok=True)\n"
      "trend_path.parent.mkdir(parents=True, exist_ok=True)\n"
      "budget_path.write_text(json.dumps({'schema': 'primestruct_semantic_memory_budget_check_report_v1', 'entries': [], 'failure_count': 0}) + '\\n', encoding='utf-8')\n"
      "trend_path.write_text(json.dumps({'schema': 'primestruct_semantic_memory_trend_report_v1', 'status': 'passed'}) + '\\n', encoding='utf-8')\n");

  const std::string benchmarkCmdOverride =
      "python3 " + quoteShellArg(benchScript) + " " + quoteShellArg(reportPath.string());
  const std::string trendCmdOverride =
      "python3 " + quoteShellArg(trendScript) + " " + quoteShellArg(budgetPath.string()) +
      " " + quoteShellArg(trendPath.string());

  const std::string stdoutPath = writeTemp("semantic_memory_ci_artifacts_benchmark_mode.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_ci_artifacts_benchmark_mode.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode benchmark --run-label test_benchmark_mode --repo-root " + quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --trend-report " + quoteShellArg(trendPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());

  const std::filesystem::path latestManifest = artifactsDir / "test_benchmark_mode_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"mode\": \"benchmark\"") != std::string::npos);
  CHECK(manifest.find("\"status\": \"passed\"") != std::string::npos);
  CHECK(manifest.find("\"benchmark_exit_code\": 0") != std::string::npos);
  CHECK(manifest.find("\"trend_exit_code\": 0") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": false") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": \"semantic_memory_budget_report.json\"") != std::string::npos);
  CHECK(manifest.find("\"trend_report\": \"semantic_memory_trend_report.json\"") != std::string::npos);

  bool sawHistory = false;
  for (const auto &entry : std::filesystem::directory_iterator(historyDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind("semantic_memory_report_test_benchmark_mode_", 0) == 0 &&
        entry.path().extension() == ".json") {
      sawHistory = true;
      break;
    }
  }
  CHECK(sawHistory);
}

TEST_CASE("semantic memory ci artifact wrapper benchmark mode can skip budget gate") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch =
      testScratchDir("semantic_memory_ci_artifacts_benchmark_mode_skip_budget");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path trendPath = rootScratch / "semantic_memory_trend_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";
  const std::filesystem::path trendTouchedPath = rootScratch / "trend_touched.txt";

  const std::string benchScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_skip_budget_bench.py",
      "import json, pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "payload = {\n"
      "  'schema': 'primestruct_semantic_memory_report_v1',\n"
      "  'results': [{'fixture': 'toy', 'phase': 'ast-semantic', 'worst_peak_rss_bytes': 1, 'worst_wall_seconds': 0.1}],\n"
      "}\n"
      "path.write_text(json.dumps(payload) + '\\n', encoding='utf-8')\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_skip_budget_trend.py",
      "import pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "path.write_text('trend should not run\\n', encoding='utf-8')\n"
      "sys.exit(4)\n");

  const std::string benchmarkCmdOverride =
      "python3 " + quoteShellArg(benchScript) + " " + quoteShellArg(reportPath.string());
  const std::string trendCmdOverride =
      "python3 " + quoteShellArg(trendScript) + " " + quoteShellArg(trendTouchedPath.string());

  const std::string stdoutPath =
      writeTemp("semantic_memory_ci_artifacts_benchmark_mode_skip_budget.out", "");
  const std::string stderrPath =
      writeTemp("semantic_memory_ci_artifacts_benchmark_mode_skip_budget.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode benchmark --skip-budget-check-in-benchmark"
      " --run-label test_benchmark_mode_skip_budget --repo-root " + quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --trend-report " + quoteShellArg(trendPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 0);
  CHECK(readFile(stderrPath).empty());
  CHECK_FALSE(std::filesystem::exists(trendTouchedPath));

  const std::filesystem::path latestManifest =
      artifactsDir / "test_benchmark_mode_skip_budget_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"mode\": \"benchmark\"") != std::string::npos);
  CHECK(manifest.find("\"status\": \"passed\"") != std::string::npos);
  CHECK(manifest.find("\"benchmark_exit_code\": 0") != std::string::npos);
  CHECK(manifest.find("\"trend_exit_code\": null") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": false") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": null") != std::string::npos);
  CHECK(manifest.find("\"trend_report\": null") != std::string::npos);

  bool sawHistory = false;
  for (const auto &entry : std::filesystem::directory_iterator(historyDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind("semantic_memory_report_test_benchmark_mode_skip_budget_", 0) == 0 &&
        entry.path().extension() == ".json") {
      sawHistory = true;
      break;
    }
  }
  CHECK(sawHistory);
}

TEST_CASE("semantic memory ci artifact wrapper benchmark mode fails on budget gate") {
  if (!hasPython3()) {
    INFO("python3 not available");
    return;
  }

  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path wrapperPath = repoRoot / "scripts" / "semantic_memory_ci_artifacts.py";
  const std::filesystem::path rootScratch =
      testScratchDir("semantic_memory_ci_artifacts_benchmark_mode_budget_fail");
  const std::filesystem::path reportPath = rootScratch / "semantic_memory_report.json";
  const std::filesystem::path budgetPath = rootScratch / "semantic_memory_budget_report.json";
  const std::filesystem::path trendPath = rootScratch / "semantic_memory_trend_report.json";
  const std::filesystem::path historyDir = rootScratch / "history";
  const std::filesystem::path artifactsDir = rootScratch / "artifacts";

  const std::string benchScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_budget_fail_bench.py",
      "import json, pathlib, sys\n"
      "path = pathlib.Path(sys.argv[1])\n"
      "path.parent.mkdir(parents=True, exist_ok=True)\n"
      "payload = {\n"
      "  'schema': 'primestruct_semantic_memory_report_v1',\n"
      "  'results': [{'fixture': 'toy', 'phase': 'ast-semantic', 'worst_peak_rss_bytes': 1, 'worst_wall_seconds': 0.1}],\n"
      "}\n"
      "path.write_text(json.dumps(payload) + '\\n', encoding='utf-8')\n");
  const std::string trendScript = writeTemp(
      "semantic_memory_ci_artifacts_benchmark_mode_budget_fail_trend.py",
      "import pathlib, sys\n"
      "budget_path = pathlib.Path(sys.argv[1])\n"
      "trend_path = pathlib.Path(sys.argv[2])\n"
      "budget_path.parent.mkdir(parents=True, exist_ok=True)\n"
      "trend_path.parent.mkdir(parents=True, exist_ok=True)\n"
      "budget_path.write_text('{\"schema\":\"primestruct_semantic_memory_budget_check_report_v1\"}\\n', encoding='utf-8')\n"
      "trend_path.write_text('{\"schema\":\"primestruct_semantic_memory_trend_report_v1\",\"status\":\"failed\"}\\n', encoding='utf-8')\n"
      "sys.exit(5)\n");

  const std::string benchmarkCmdOverride =
      "python3 " + quoteShellArg(benchScript) + " " + quoteShellArg(reportPath.string());
  const std::string trendCmdOverride =
      "python3 " + quoteShellArg(trendScript) + " " + quoteShellArg(budgetPath.string()) +
      " " + quoteShellArg(trendPath.string());

  const std::string stdoutPath = writeTemp("semantic_memory_ci_artifacts_benchmark_mode_budget_fail.out", "");
  const std::string stderrPath = writeTemp("semantic_memory_ci_artifacts_benchmark_mode_budget_fail.err", "");
  const std::string cmd =
      "python3 " + quoteShellArg(wrapperPath.string()) +
      " --mode benchmark --run-label test_benchmark_mode_budget_fail --repo-root " +
      quoteShellArg(repoRoot.string()) +
      " --benchmark-report " + quoteShellArg(reportPath.string()) +
      " --budget-report " + quoteShellArg(budgetPath.string()) +
      " --trend-report " + quoteShellArg(trendPath.string()) +
      " --history-dir " + quoteShellArg(historyDir.string()) +
      " --artifacts-dir " + quoteShellArg(artifactsDir.string()) +
      " --benchmark-cmd " + quoteShellArg(benchmarkCmdOverride) +
      " --trend-cmd " + quoteShellArg(trendCmdOverride) +
      " > " + quoteShellArg(stdoutPath) + " 2> " + quoteShellArg(stderrPath);

  CHECK(runCommand(cmd) == 5);
  CHECK(readFile(stderrPath).empty());

  const std::filesystem::path latestManifest =
      artifactsDir / "test_benchmark_mode_budget_fail_latest_manifest.json";
  REQUIRE(std::filesystem::exists(latestManifest));
  const std::string manifest = readFile(latestManifest.string());
  CHECK(manifest.find("\"mode\": \"benchmark\"") != std::string::npos);
  CHECK(manifest.find("\"status\": \"failed\"") != std::string::npos);
  CHECK(manifest.find("\"benchmark_exit_code\": 0") != std::string::npos);
  CHECK(manifest.find("\"trend_exit_code\": 5") != std::string::npos);
  CHECK(manifest.find("\"trend_skipped\": false") != std::string::npos);
  CHECK(manifest.find("\"benchmark_report\": \"semantic_memory_report.json\"") != std::string::npos);
  CHECK(manifest.find("\"budget_report\": null") != std::string::npos);
  CHECK(manifest.find("\"history_report\": null") != std::string::npos);
  const bool historyDirIsEmpty =
      !std::filesystem::exists(historyDir) ||
      std::filesystem::directory_iterator(historyDir) == std::filesystem::directory_iterator{};
  CHECK(historyDirIsEmpty);
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

TEST_CASE("semantic benchmark plumbing keeps production validate surface narrow") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path semanticsHeaderPath = repoRoot / "include" / "primec" / "semantics" / "Semantics.h";
  const std::filesystem::path semanticsBenchmarkHeaderPath =
      repoRoot / "include" / "primec" / "semantics" / "SemanticsBenchmark.h";
  const std::filesystem::path benchmarkOrchestrationHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidationBenchmarkOrchestration.h";
  const std::filesystem::path benchmarkOrchestrationSourcePath =
      repoRoot / "src" / "semantics" / "SemanticsValidationBenchmarkOrchestration.cpp";
  const std::filesystem::path publicationOrchestrationHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidationPublicationOrchestration.h";
  const std::filesystem::path publicationOrchestrationSourcePath =
      repoRoot / "src" / "semantics" / "SemanticsValidationPublicationOrchestration.cpp";
  const std::filesystem::path semanticsValidatePath =
      repoRoot / "src" / "semantics" / "SemanticsValidate.cpp";
  const std::filesystem::path pipelineHeaderPath =
      repoRoot / "include" / "primec" / "pipeline" / "CompilePipeline.h";
  const std::filesystem::path pipelinePath = repoRoot / "src" / "pipeline" / "CompilePipeline.cpp";
  const std::string semanticsHeader = readFile(semanticsHeaderPath.string());
  const std::string semanticsBenchmarkHeader = readFile(semanticsBenchmarkHeaderPath.string());
  const std::string benchmarkOrchestrationHeader = readFile(benchmarkOrchestrationHeaderPath.string());
  const std::string benchmarkOrchestrationSource = readFile(benchmarkOrchestrationSourcePath.string());
  const std::string publicationOrchestrationHeader =
      readFile(publicationOrchestrationHeaderPath.string());
  const std::string publicationOrchestrationSource =
      readFile(publicationOrchestrationSourcePath.string());
  const std::string semanticsValidateText = readFile(semanticsValidatePath.string());
  const std::string pipelineHeader = readFile(pipelineHeaderPath.string());
  const std::string pipelineText = readFile(pipelinePath.string());

  REQUIRE_FALSE(semanticsHeader.empty());
  REQUIRE_FALSE(semanticsBenchmarkHeader.empty());
  REQUIRE_FALSE(benchmarkOrchestrationHeader.empty());
  REQUIRE_FALSE(benchmarkOrchestrationSource.empty());
  REQUIRE_FALSE(publicationOrchestrationHeader.empty());
  REQUIRE_FALSE(publicationOrchestrationSource.empty());
  REQUIRE_FALSE(semanticsValidateText.empty());
  REQUIRE_FALSE(pipelineHeader.empty());
  REQUIRE_FALSE(pipelineText.empty());

  CHECK(semanticsHeader.find("struct SemanticValidationBenchmarkConfig") == std::string::npos);
  CHECK(semanticsHeader.find("struct SemanticValidationBenchmarkObserver") == std::string::npos);
  CHECK(semanticsHeader.find("bool validateForBenchmark(") == std::string::npos);
  CHECK(semanticsHeader.find(
            "bool validate(Program &program,\n"
            "                const std::string &entryPath,\n"
            "                std::string &error,\n"
            "                const std::vector<std::string> &defaultEffects,\n"
            "                const std::vector<std::string> &entryDefaultEffects,\n"
            "                const std::vector<std::string> &semanticTransforms = {},\n"
            "                SemanticDiagnosticInfo *diagnosticInfo = nullptr,\n"
            "                bool collectDiagnostics = false,\n"
            "                SemanticProgram *semanticProgramOut = nullptr,\n"
            "                const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr,\n"
            "                const std::unordered_set<std::string> *lazyStdlibModuleKeys = nullptr) const;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticDisableMethodTargetMemoization") == std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticGraphLocalAutoLegacyKeyShadow") == std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticGraphLocalAutoLegacySideChannelShadow") == std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr") ==
        std::string::npos);
  CHECK(semanticsBenchmarkHeader.find("struct SemanticValidationBenchmarkConfig") != std::string::npos);
  CHECK(semanticsBenchmarkHeader.find("struct SemanticValidationBenchmarkObserver") != std::string::npos);
  CHECK(semanticsBenchmarkHeader.find("bool validateSemanticsForBenchmark(") != std::string::npos);
  CHECK(benchmarkOrchestrationHeader.find("SemanticValidationBenchmarkRuntime") != std::string::npos);
  CHECK(benchmarkOrchestrationHeader.find("SemanticValidationBenchmarkPhase") != std::string::npos);
  CHECK(benchmarkOrchestrationHeader.find("SemanticValidatorLifetimeBenchmark") != std::string::npos);
  CHECK(benchmarkOrchestrationSource.find("makeSemanticValidationBenchmarkRuntime(") != std::string::npos);
  CHECK(benchmarkOrchestrationSource.find("PRIMEC_BENCHMARK_SEMANTIC_VALIDATOR_LIFETIME") !=
        std::string::npos);
  CHECK(semanticsValidateText.find("makeSemanticValidationBenchmarkRuntime(") != std::string::npos);
  CHECK(semanticsValidateText.find("SemanticValidationBenchmarkPhase validationBenchmark") !=
        std::string::npos);
  CHECK(semanticsValidateText.find("PRIMEC_BENCHMARK_SEMANTIC_VALIDATOR_LIFETIME") ==
        std::string::npos);
  CHECK(publicationOrchestrationHeader.find("publishSemanticProgramAfterValidation(") !=
        std::string::npos);
  CHECK(publicationOrchestrationHeader.find("SemanticPublicationSurface publicationSurface") !=
        std::string::npos);
  CHECK(publicationOrchestrationHeader.find("class SemanticsValidator;") ==
        std::string::npos);
  CHECK(publicationOrchestrationSource.find("#include \"SemanticsValidator.h\"") ==
        std::string::npos);
  CHECK(publicationOrchestrationSource.find("SemanticsValidator &validator") ==
        std::string::npos);
  CHECK(publicationOrchestrationSource.find(
            "validator.takeSemanticPublicationSurfaceForSemanticProduct(") ==
        std::string::npos);
  CHECK(publicationOrchestrationSource.find("semanticProductBuild.callsVisited = 1;") !=
        std::string::npos);
  CHECK(publicationOrchestrationSource.find(
            "populateAllocationDelta(benchmarkRuntime.phaseCounters->semanticProductBuild") !=
        std::string::npos);
  CHECK(semanticsValidateText.find("semanticProductBuild.callsVisited = 1;") ==
        std::string::npos);
  CHECK(semanticsValidateText.find("semanticProgramFactCountForValidationPublication(") ==
        std::string::npos);

  CHECK(pipelineHeader.find("struct CompilePipelineBenchmarkConfig") != std::string::npos);
  CHECK(pipelineHeader.find("struct CompilePipelineRunConfig") != std::string::npos);
  CHECK(pipelineHeader.find("const CompilePipelineRunConfig &runConfig") != std::string::npos);
  CHECK(pipelineText.find("makeCompilePipelineRunConfigFromOptions(") != std::string::npos);
  CHECK(pipelineText.find("decideSemanticProductDecision(dumpStage, runConfig)") !=
        std::string::npos);
  CHECK(pipelineText.find("decideSemanticProductDecision(dumpStage, options)") ==
        std::string::npos);
  CHECK(pipelineText.find("semanticBenchmarkCountersRequested(benchmarkConfig)") !=
        std::string::npos);
  CHECK(pipelineText.find("semanticBenchmarkValidationConfigRequested(") != std::string::npos);
  CHECK(pipelineText.find("SemanticValidationBenchmarkConfig benchmarkConfig;") !=
        std::string::npos);
  CHECK(pipelineText.find("SemanticValidationBenchmarkObserver benchmarkObserver;") !=
        std::string::npos);
  CHECK(pipelineText.find("validateSemanticsForBenchmark(") != std::string::npos);
  CHECK(pipelineText.find("semanticValidationOk = semantics.validate(") != std::string::npos);
}

TEST_CASE("tsan semantics smoke is gated behind optional-ci wiring") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::filesystem::path cmakePath = repoRoot / "CMakeLists.txt";
  const std::filesystem::path scriptPath = repoRoot / "scripts" / "run_semantics_tsan_smoke.sh";

  const std::string cmakeText = readFile(cmakePath.string());
  CHECK(cmakeText.find("option(PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE") != std::string::npos);
  CHECK(cmakeText.find("add_executable(PrimeStruct_semantics_tsan_smoke") != std::string::npos);
  CHECK(cmakeText.find("LABELS \"optional-ci;tsan\"") != std::string::npos);

  REQUIRE(std::filesystem::exists(scriptPath));
  const std::string scriptText = readFile(scriptPath.string());
  CHECK(scriptText.find("PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON") != std::string::npos);
  CHECK(scriptText.find("PrimeStruct_semantics_tsan_smoke") != std::string::npos);
}

TEST_SUITE_END();
