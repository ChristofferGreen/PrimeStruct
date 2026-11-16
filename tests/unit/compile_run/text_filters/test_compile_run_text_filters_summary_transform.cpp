#include "test_compile_run_text_filters_helpers.h"

#include "primec/testing/DumpNormalization.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct SummaryRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

SummaryRun runSummarySource(const std::string &name, const std::string &source, const std::string &args) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primec " + quoteShellArg(srcPath) + " " + args + " > " + quoteShellArg(outPath) +
                          " 2> " + quoteShellArg(errPath);
  SummaryRun run;
  run.exitCode = runCommand(cmd);
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

const std::string SummaryMainSource = R"(
[return<int>]
main() {
  return(1i32)
}
)";

void checkSummaryDiagnostic(const std::string &name, const std::string &source, const std::string &expected) {
  const SummaryRun run = runSummarySource(name, source + SummaryMainSource, "--emit=vm --entry /main");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find(expected) != std::string::npos);
}

const std::string DocumentedSummarySource = R"(
[namespace summary("Text buffers and cursors.")]
model {
  [summary("The answer.") return<i32>] answer() { return(42i32) }
  [summary("A point on the grid.") struct] Point() { [i32] x{1i32} }
}

[return<int>]
main() {
  return(/model/answer())
}
)";

} // namespace

TEST_CASE("summary transform on a function, struct and namespace runs on the VM") {
  const SummaryRun run = runSummarySource("summary_transform_vm", DocumentedSummarySource, "--emit=vm --entry /main");
  CHECK(run.exitCode == 42);
  CHECK(run.err.empty());
}

TEST_CASE("summary transform is valid on an enum") {
  const std::string source = R"(
[summary("Traffic light colors.") enum]
Color {
  Red
  Green
}

[return<int>]
main() {
  return(1i32)
}
)";
  const SummaryRun run = runSummarySource("summary_transform_enum", source, "--emit=vm --entry /main");
  CHECK(run.exitCode == 1);
}

TEST_CASE("summary transform rejects a wrong argument count") {
  checkSummaryDiagnostic("summary_transform_count",
                         "[summary(\"One.\", \"Two.\") return<i32>]\nf() { return(1i32) }\n",
                         "summary transform requires exactly one string literal argument on /f");
  checkSummaryDiagnostic("summary_transform_none",
                         "[summary return<i32>]\nf() { return(1i32) }\n",
                         "summary transform requires exactly one string literal argument on /f");
}

TEST_CASE("summary transform rejects a non-literal argument") {
  checkSummaryDiagnostic("summary_transform_name",
                         "[summary(text) return<i32>]\nf() { return(1i32) }\n",
                         "summary transform argument must be a string literal");
  checkSummaryDiagnostic("summary_transform_number",
                         "[summary(3i32) return<i32>]\nf() { return(1i32) }\n",
                         "summary transform argument must be a string literal");
}

TEST_CASE("summary transform rejects a repeated summary") {
  checkSummaryDiagnostic("summary_transform_repeat",
                         "[summary(\"One.\") summary(\"Two.\") return<i32>]\nf() { return(1i32) }\n",
                         "duplicate summary transform on /f (first at 1:2, again at 1:22)");
}

TEST_CASE("summary transform rejects two summaries for one namespace") {
  checkSummaryDiagnostic("summary_transform_namespace",
                         "[namespace summary(\"One.\")]\ndemo {\n}\n"
                         "[namespace summary(\"Two.\")]\ndemo {\n}\n",
                         "duplicate summary transform on /demo (first at 1:12, again at 4:12)");
}

TEST_CASE("summary transform is rejected on a parameter") {
  checkSummaryDiagnostic("summary_transform_param",
                         "[return<i32>]\nf([summary(\"One.\") i32] value) { return(value) }\n",
                         "summary transform is only valid on definitions: /f");
}

TEST_CASE("summary transform shows in the semantic product dump") {
  const SummaryRun run = runSummarySource("summary_transform_product", DocumentedSummarySource,
                                          "--dump-stage semantic-product");
  CHECK(run.exitCode == 0);
  CHECK(run.out.find("full_path=\"/model\" name=\"model\" namespace_prefix=\"\" summary=\"Text buffers and "
                     "cursors.\" provenance_handle=") != std::string::npos);
  CHECK(run.out.find("full_path=\"/model/answer\" name=\"answer\" namespace_prefix=\"/model\" "
                     "summary=\"The answer.\" provenance_handle=") != std::string::npos);
  CHECK(run.out.find("full_path=\"/model/Point\" name=\"Point\" namespace_prefix=\"/model\" "
                     "summary=\"A point on the grid.\" provenance_handle=") != std::string::npos);
  CHECK(run.out.find("full_path=\"/main\" name=\"main\" namespace_prefix=\"\" provenance_handle=") !=
        std::string::npos);
}

TEST_CASE("summary transform leaves the lowered ir unchanged") {
  const std::string plain = R"(
[namespace]
model {
  [return<i32>] answer() { return(42i32) }
  [struct] Point() { [i32] x{1i32} }
}

[return<int>]
main() {
  return(/model/answer())
}
)";
  const SummaryRun withSummary =
      runSummarySource("summary_transform_ir_with", DocumentedSummarySource, "--dump-stage ir-lowered");
  const SummaryRun withoutSummary = runSummarySource("summary_transform_ir_without", plain, "--dump-stage ir-lowered");
  CHECK(withSummary.exitCode == 0);
  CHECK(withoutSummary.exitCode == 0);
  CHECK(!withSummary.out.empty());
  CHECK(primec::testing::stripDumpTimings(withSummary.out) == primec::testing::stripDumpTimings(withoutSummary.out));
}

TEST_SUITE_END();
