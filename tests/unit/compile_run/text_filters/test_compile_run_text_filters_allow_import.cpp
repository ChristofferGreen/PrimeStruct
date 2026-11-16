#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct AllowImportRun {
  int exitCode = 0;
  std::string err;
};

AllowImportRun runAllowImport(const std::string &name, const std::string &source, const std::string &args) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " " + args + " > " + quoteShellArg(outPath) +
                          " 2> " + quoteShellArg(errPath);
  AllowImportRun run;
  run.exitCode = runCommand(cmd);
  run.err = readFile(errPath);
  return run;
}

const std::string NetImportSource = R"(
import /std/net/*

[return<i32>]
main() {
  return(3i32)
}
)";

const std::string MathImportSource = R"(
import /std/math/*

[return<i32>]
main() {
  return(abs(-5i32))
}
)";

} // namespace

TEST_CASE("allow-import rejects an import outside the list") {
  const AllowImportRun run =
      runAllowImport("allow_import_reject", NetImportSource, "--allow-import /std/math,/std/file");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("import /std/net/* is outside the allowed imports: /std/math,/std/file") !=
        std::string::npos);
}

TEST_CASE("allow-import accepts an import that the list names") {
  const AllowImportRun run =
      runAllowImport("allow_import_accept", NetImportSource, "--allow-import /std/math,/std/net/*");
  CHECK(run.exitCode == 3);
  CHECK(run.err.empty());
}

TEST_CASE("allow-import matches whole path components") {
  const AllowImportRun sibling = runAllowImport("allow_import_sibling", MathImportSource, "--allow-import /std/mat");
  CHECK(sibling.exitCode == 2);
  CHECK(sibling.err.find("import /std/math/* is outside the allowed imports: /std/mat") != std::string::npos);

  const AllowImportRun nested = runAllowImport("allow_import_nested",
                                               "import /std/collections/vector\n"
                                               "[return<i32>]\nmain() {\n  return(4i32)\n}\n",
                                               "--allow-import /std/collections");
  CHECK(nested.exitCode == 4);
  CHECK(nested.err.empty());

  const AllowImportRun parent = runAllowImport(
      "allow_import_parent", "import /std/collections/vector\n[return<i32>]\nmain() {\n  return(4i32)\n}\n",
      "--allow-import /std/collections/map");
  CHECK(parent.exitCode == 2);
  CHECK(parent.err.find("import /std/collections/vector is outside the allowed imports: /std/collections/map") !=
        std::string::npos);
}

TEST_CASE("allow-import accepts an import of a project file under any list") {
  const std::string helperPath = writeTemp("allow_import_helper.prime",
                                           "[return<i32>]\nhelper_value() {\n  return(6i32)\n}\n");
  const std::string source = "import<\"" + helperPath + "\">\n\n[return<i32>]\nmain() {\n  return(helper_value())\n}\n";
  const AllowImportRun run = runAllowImport("allow_import_project", source, "--allow-import ''");
  CHECK(run.exitCode == 6);
  CHECK(run.err.empty());
}

TEST_CASE("allow-import rejects a stdlib import written in a project file") {
  const std::string helperPath = writeTemp("allow_import_helper_net.prime",
                                           "import /std/net/*\n\n[return<i32>]\nhelper_value() {\n  return(6i32)\n}\n");
  const std::string source = "import<\"" + helperPath + "\">\n\n[return<i32>]\nmain() {\n  return(helper_value())\n}\n";
  const AllowImportRun run = runAllowImport("allow_import_project_net", source, "--allow-import /std/math");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("import /std/net/* is outside the allowed imports: /std/math") != std::string::npos);
}

TEST_CASE("allow-import does not check imports written inside stdlib files") {
  const std::string source = R"(
import /std/image/*

[return<i32>]
main() {
  return(8i32)
}
)";
  const AllowImportRun run = runAllowImport("allow_import_stdlib_internal", source, "--allow-import /std/image");
  CHECK(run.exitCode == 8);
  CHECK(run.err.empty());
}

TEST_CASE("without allow-import every stdlib import is accepted") {
  const AllowImportRun run = runAllowImport("allow_import_absent", NetImportSource, "");
  CHECK(run.exitCode == 3);
  CHECK(run.err.empty());
}

TEST_CASE("allow-import reports every violation under collect-diagnostics") {
  const std::string source = R"(
import /std/net/*
import /std/file/*
import /std/math/*

[return<i32>]
main() {
  return(0i32)
}
)";
  const AllowImportRun run = runAllowImport(
      "allow_import_collect", source, "--allow-import /std/math --emit-diagnostics --collect-diagnostics");
  CHECK(run.exitCode == 2);
  const size_t first = run.err.find("import /std/net/* is outside");
  const size_t second = run.err.find("import /std/file/* is outside");
  CHECK(first != std::string::npos);
  CHECK(second != std::string::npos);
  CHECK(first < second);
  CHECK(run.err.find("import /std/math/* is outside") == std::string::npos);
}

TEST_CASE("allow-import with an empty list allows no stdlib import") {
  const AllowImportRun rejected = runAllowImport("allow_import_empty", MathImportSource, "--allow-import ''");
  CHECK(rejected.exitCode == 2);
  CHECK(rejected.err.find("import /std/math/* is outside the allowed imports: none") != std::string::npos);

  const AllowImportRun accepted = runAllowImport(
      "allow_import_empty_pure", "[return<i32>]\nmain() {\n  return(9i32)\n}\n", "--allow-import ''");
  CHECK(accepted.exitCode == 9);
  CHECK(accepted.err.empty());
}

TEST_CASE("allow-import rejects a prefix that is not a slash path") {
  const AllowImportRun run = runAllowImport("allow_import_bad_prefix", MathImportSource, "--allow-import std/math");
  CHECK(run.exitCode != 0);
  CHECK(run.err.find("--allow-import prefixes must be slash paths: std/math") != std::string::npos);
}

TEST_SUITE_END();
