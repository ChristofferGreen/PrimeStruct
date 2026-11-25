#include "primec/testing/DumpNormalization.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.optimizer_cli");

// `--dump-stage inline-report`: what the lowerer did with every definition a call site reached. The tests run
// the primec and primevm binaries from the build directory, like the optimizer CLI tests.

namespace {

struct ReportResult {
  int exitCode = -1;
  std::string out;
  std::string err;
};

std::string readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

ReportResult runCommand(const std::string &command) {
  const std::filesystem::path outPath =
      primec::testing::testScratchPath("inline_report_cli/stdout.txt");
  const std::filesystem::path errPath =
      primec::testing::testScratchPath("inline_report_cli/stderr.txt");
  std::filesystem::create_directories(outPath.parent_path());
  const int status = std::system(
      (command + " > '" + outPath.string() + "' 2> '" + errPath.string() + "'").c_str());
  ReportResult result;
#if defined(__unix__) || defined(__APPLE__)
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  result.exitCode = status;
#endif
  result.out = readFile(outPath);
  result.err = readFile(errPath);
  return result;
}

std::string writeProgram() {
  // One recursive function (a real call), one `on_error` definition and one function with a sum parameter.
  const char *text = R"(
[sum]
Shape {
  none
  [i32] circle
  [i32] square
}

[return<int> effects(none)]
fib([int] n) {
  if(n < 2i32) {
    return(n)
  }
  return(plus(fib(minus(n, 1i32)), fib(minus(n, 2i32))))
}

[return<int> effects(none)]
area([Shape] shape) {
  [int mut] total{0i32}
  total = plus(total, 1i32)
  total = plus(total, 2i32)
  total = plus(total, 3i32)
  total = plus(total, 4i32)
  return(total)
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}

[return<Result<FileError>> effects(file_read) on_error<FileError, /log_file_error>]
check_exists() {
  [File<Read>] file{File<Read>("/nonexistent"utf8)?}
  file.close()?
  return(Result.ok())
}

[return<int> effects(file_read, io_out)]
main() {
  [Shape] shape{[circle] 3i32}
  print_line(fib(10i32))
  print_line(area(shape))
  print_line(area(shape))
  check_exists()
  return(0i32)
}
)";
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/program.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
  return path.string();
}

} // namespace

// The VM has the address ABI: the recursive function and the one with a sum parameter are real calls, the definition
// that tries and returns a `Result` and the small handler are expanded.
TEST_CASE(
    "inline-report names real calls and the blocker of each expanded definition on the vm target") {
  const std::string source = writeProgram();
  const ReportResult report = runCommand("./primec --emit=vm --dump-stage inline-report " + source);
  CHECK(report.exitCode == 0);
  CHECK(report.out ==
        "inline_report_v1 definitions=4\n"
        "added=54 with_callees=60 call_sites=1 /check_exists: uses-try: its body uses "
        "`try`\n"
        "added=18 with_callees=18 call_sites=3 /fib: real call\n"
        "added=8 with_callees=8 call_sites=2 /area: real call\n"
        "added=6 with_callees=6 call_sites=2 /log_file_error: tiny-leaf: its body is "
        "small enough that expanding "
        "it costs less than calling it\n");
}

// The target-neutral listing lowers for the scalar ABI, where a sum parameter is not a scalar; primevm prints the
// same text as primec without `--emit`.
TEST_CASE("inline-report on the scalar target names the parameter that keeps a definition inline") {
  const std::string source = writeProgram();
  const ReportResult report = runCommand("./primec --dump-stage inline-report " + source);
  CHECK(report.exitCode == 0);
  CHECK(report.out == "inline_report_v1 definitions=4\n"
                      "added=76 with_callees=76 call_sites=2 /area: scalar-only-parameter: "
                      "parameter `shape` is a `Shape`, "
                      "which a call on this target does not take (only scalar values)\n"
                      "added=54 with_callees=60 call_sites=1 /check_exists: on-error-handler: it "
                      "has an `on_error` handler, which a call on this target does not take\n"
                      "added=18 with_callees=18 call_sites=3 /fib: real call\n"
                      "added=6 with_callees=6 call_sites=2 /log_file_error: tiny-leaf: its body is "
                      "small enough that expanding "
                      "it costs less than calling it\n");
  const ReportResult viaVm = runCommand("./primevm --dump-stage inline-report " + source);
  CHECK(viaVm.exitCode == 0);
  CHECK(primec::testing::stripDumpTimings(viaVm.out) ==
        primec::testing::stripDumpTimings(report.out));
}

TEST_CASE("inline-report is listed in the help and accepts the underscore spelling") {
  const std::string source = writeProgram();
  const ReportResult help = runCommand("./primec --help");
  CHECK(help.err.find("ir-lowered, ir-optimized, inline-report") != std::string::npos);
  const ReportResult dashed = runCommand("./primec --emit=vm --dump-stage inline-report " + source);
  const ReportResult underscored =
      runCommand("./primec --emit=vm --dump-stage inline_report " + source);
  CHECK(underscored.exitCode == 0);
  CHECK(primec::testing::stripDumpTimings(underscored.out) ==
        primec::testing::stripDumpTimings(dashed.out));
}

TEST_SUITE_END();
