#include "../test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

// The columns of a debug session are those of the text the user wrote, not those of the text the filters rewrote
// (docs/spec/vm-design.md, Debugger): `a = a + 5i32` after `a{1i32 + 1i32}` on one line is `assign(a, plus(a, 5i32))`
// further right once the operators are rewritten. A compile that is not for a debugger keeps the parser's columns and
// never builds the offset map that moves them.

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

namespace {

// The second statement of line 4 starts at column 28 as written and at column 33 once the operator of the first is
// rewritten.
const std::string ColumnProgram = R"(
[return<int>]
main() {
  [i32 mut] a{1i32 + 1i32} a = a + 5i32
  return(a)
}
)";

std::string breakpointSession(int column) {
  return makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
         makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
         makeDapFrame(
             R"({"seq":3,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[{"line":4,"column":)" +
             std::to_string(column) + R"(}]}})") +
         makeDapFrame(
             R"({"seq":4,"type":"request","command":"configurationDone","arguments":{}})") +
         makeDapFrame(
             R"({"seq":5,"type":"request","command":"continue","arguments":{"threadId":1}})") +
         makeDapFrame(
             R"({"seq":6,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
         makeDapFrame(R"({"seq":7,"type":"request","command":"disconnect","arguments":{}})");
}

std::string runSession(const std::string &program, const std::string &name, int column) {
  const std::string requestPath = writeTemp(name + ".requests", breakpointSession(column));
  const std::string outPath = (testScratchPath("") / (name + ".out")).string();
  const std::string cmd = "./primevm " + quoteShellArg(program) + " --entry /main --debug-dap < " +
                          quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);
  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(readFile(outPath), framingOk);
  CHECK(framingOk);
  std::string joined;
  for (const std::string &frame : frames) {
    joined += frame + "\n";
  }
  return joined;
}

} // namespace

TEST_CASE("primevm debug-dap breakpoint and stack frame use the column the user wrote") {
  const std::string srcPath = writeTemp("primevm_debug_columns.prime", ColumnProgram);
  const std::string written = runSession(srcPath, "primevm_debug_columns_written", 28);
  CHECK(written.find(R"("verified":true,"line":4,"column":28)") != std::string::npos);
  CHECK(written.find(R"("reason":"breakpoint")") != std::string::npos);
  CHECK(written.find(R"("line":4,"column":28,"instructionPointer")") != std::string::npos);
  // The column of the rewritten text is not a position the user wrote.
  const std::string rewritten = runSession(srcPath, "primevm_debug_columns_rewritten", 33);
  CHECK(rewritten.find("\"verified\":true") == std::string::npos);
  CHECK(rewritten.find("source breakpoint not found") != std::string::npos);
}

TEST_CASE("a serialized module carries the column the user wrote into a debug session") {
  const std::string srcPath = writeTemp("primevm_debug_columns_ir.prime", ColumnProgram);
  const std::string irPath = (testScratchPath("") / "primevm_debug_columns_ir.psir").string();
  CHECK(runCommand("./primec --emit=ir " + quoteShellArg(srcPath) + " -o " +
                   quoteShellArg(irPath)) == 0);
  const std::string session = runSession(irPath, "primevm_debug_columns_ir", 28);
  CHECK(session.find(R"("verified":true,"line":4,"column":28)") != std::string::npos);
  CHECK(session.find(R"("line":4,"column":28,"instructionPointer")") != std::string::npos);
}

TEST_SUITE_END();
