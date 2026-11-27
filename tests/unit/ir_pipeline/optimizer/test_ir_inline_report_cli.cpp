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
// that tries with an `on_error` handler is one too, and the small handler is expanded.
TEST_CASE(
    "inline-report names real calls and the blocker of each expanded definition on the vm target") {
  const std::string source = writeProgram();
  const ReportResult report = runCommand("./primec --emit=vm --dump-stage inline-report " + source);
  CHECK(report.exitCode == 0);
  CHECK(report.out == "inline_report_v1 definitions=4\n"
                      "added=18 with_callees=18 call_sites=3 /fib: real call\n"
                      "added=8 with_callees=8 call_sites=2 /area: real call\n"
                      "added=6 with_callees=6 call_sites=2 /log_file_error: tiny-leaf: its body is "
                      "small enough that expanding "
                      "it costs less than calling it\n"
                      "added=2 with_callees=2 call_sites=1 /check_exists: real call\n");
}

// A body that returns a stdlib `Result`, tries and has an `on_error` handler writes its error into the caller's block.
TEST_CASE("inline-report shows a stdlib Result body that tries as a real call on the vm target") {
  const char *text = R"(
import /std/result/*

[return<Result<i32>>]
check([i32] x) {
  if(x > 3i32) {
    [Result<i32>] bad{[error] x}
    return(bad)
  }
  return(/std/result/ok<i32>())
}

[effects(io_out)]
on_err([i32] err) {
  print_line(1000i32 + err)
}

[return<Result<i32>> effects(io_out) on_error<i32, /on_err>]
both([i32] x) {
  try(check(x))
  try(check(x + 1i32))
  return(/std/result/ok<i32>())
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(both(1i32)))
  print_line(Result.error(both(3i32)))
  return(0i32)
}
)";
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/both.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
  const ReportResult report =
      runCommand("./primec --emit=vm --dump-stage inline-report " + path.string());
  CHECK(report.exitCode == 0);
  CHECK(report.out.find("/both: real call\n") != std::string::npos);
  CHECK(report.out.find("uses-try") == std::string::npos);
  const ReportResult run = runCommand("./primevm " + path.string());
  CHECK(run.exitCode == 0);
  CHECK(run.out == "0\n1004\n1\n");
}

// A packed `Result` with a struct value returns the payload block through the caller's block, so the definition is a
// real call; a collection value does not fit and keeps it inline, with a text that names the value.
TEST_CASE("inline-report shows a packed Result with a struct value as a real call") {
  const char *text = R"(
import /std/file/*

[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, FileError>>]
make_pair([i32] left, [i32] right) {
  return(Result.ok(Pair{[left] left, [right] right}))
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(make_pair(1i32, 2i32)))
  print_line(Result.error(make_pair(3i32, 4i32)))
  return(0i32)
}
)";
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/pair.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
  const ReportResult report =
      runCommand("./primec --emit=vm --dump-stage inline-report " + path.string());
  CHECK(report.exitCode == 0);
  CHECK(report.out.find("/make_pair: real call\n") != std::string::npos);
  CHECK(report.out.find("unsupported-result") == std::string::npos);
  const ReportResult run = runCommand("./primevm " + path.string());
  CHECK(run.exitCode == 0);
  CHECK(run.out == "0\n0\n");
}

TEST_CASE("inline-report names a packed Result with a string value as an unsupported result") {
  const char *text = R"(
import /std/file/*

[return<Result<string, FileError>>]
make_text([i32] left) {
  return(Result.ok("text"utf8))
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(make_text(1i32)))
  print_line(Result.error(make_text(3i32)))
  return(0i32)
}
)";
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/text.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
  const ReportResult report =
      runCommand("./primec --emit=vm --dump-stage inline-report " + path.string());
  CHECK(report.exitCode == 0);
  CHECK(report.out.find("/make_text: unsupported-result: its result `Result<string, FileError>` is a "
                        "packed `Result` whose value is not a scalar, a `File` or a struct") !=
        std::string::npos);
}

// A `string` result is a one-slot table index. It is a real call when every call site of the function reads it as a
// runtime index (`print_line`, `count()`, a file open, another real call); any other use keeps the function inline,
// where the result is a static string that indexes on every target.
namespace {

std::string describeProgram(const std::string &extraStatements) {
  return R"(
[return<string> effects(none)]
describe([i32] code) {
  if(code == 1i32) {
    return("one"utf8)
  }
  if(code == 2i32) {
    return("two"utf8)
  }
  return("many"utf8)
}

[return<int> effects(io_out)]
main() {
  print_line(describe(1i32))
  print_line(describe(2i32))
  print_line(describe(9i32))
)" + extraStatements +
         R"(
  return(count(describe(1i32)) + count(describe(9i32)))
}
)";
}

} // namespace

TEST_CASE("inline-report shows a string result read only as an index as a real call") {
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/string_result_index.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << describeProgram("");
  const ReportResult report =
      runCommand("./primec --emit=vm --dump-stage inline-report " + path.string());
  CHECK(report.exitCode == 0);
  CHECK(report.out.find("/describe: real call\n") != std::string::npos);
  CHECK(report.out.find("unsupported-result") == std::string::npos);
  const ReportResult vm = runCommand("./primevm " + path.string());
  CHECK(vm.exitCode == 7);
  CHECK(vm.out == "one\ntwo\nmany\n");
  const std::filesystem::path exe = path.parent_path() / "string_result_index_optexe";
  const ReportResult built =
      runCommand("./primec --emit=optexe " + path.string() + " -o " + exe.string());
  CHECK_MESSAGE(built.exitCode == 0, built.err);
  const ReportResult optexe = runCommand(exe.string());
  CHECK(optexe.exitCode == 7);
  CHECK(optexe.out == vm.out);
}

TEST_CASE("inline-report keeps a string result inline when a call site binds it") {
  const std::filesystem::path path =
      primec::testing::testScratchPath("inline_report_cli/string_result_bound.prime");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << describeProgram("  [string] kept{describe(2i32)}\n  print_line(kept)\n");
  const ReportResult report =
      runCommand("./primec --emit=vm --dump-stage inline-report " + path.string());
  CHECK(report.exitCode == 0);
  CHECK(report.out.find("/describe: unsupported-result: its result `string` is not one a real call "
                        "returns") != std::string::npos);
  CHECK(report.out.find("every call site uses it as a runtime index") != std::string::npos);
  const ReportResult vm = runCommand("./primevm " + path.string());
  CHECK(vm.exitCode == 7);
  CHECK(vm.out == "one\ntwo\nmany\ntwo\n");
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
