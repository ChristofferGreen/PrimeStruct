#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primevm debug-dap emits deterministic framed transcripts") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{1i32}
  assign(value, plus(value, 1i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_transcript.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[{"line":4,"column":3}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"configurationDone","arguments":{}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"scopes","arguments":{"frameId":1}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"variables","arguments":{"variablesReference":257}})") +
                               makeDapFrame(R"({"seq":9,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_transcript.requests", requests);
  const std::string outPathA =
      (testScratchPath("") / "primevm_debug_dap_transcript_a.out").string();
  const std::string outPathB =
      (testScratchPath("") / "primevm_debug_dap_transcript_b.out").string();

  const std::string cmdA = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathA);
  const std::string cmdB = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathB);
  CHECK(runCommand(cmdA) == 0);
  CHECK(runCommand(cmdB) == 0);

  const std::string outA = readFile(outPathA);
  const std::string outB = readFile(outPathB);
  CHECK(outA == outB);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(outA, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 13);

  CHECK(frames[0].find("\"type\":\"response\"") != std::string::npos);
  CHECK(frames[0].find("\"command\":\"initialize\"") != std::string::npos);
  CHECK(frames[1].find("\"command\":\"launch\"") != std::string::npos);
  CHECK(frames[2].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[2].find("\"event\":\"initialized\"") != std::string::npos);
  CHECK(frames[3].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[3].find("\"event\":\"stopped\"") != std::string::npos);
  CHECK(frames[3].find("\"reason\":\"entry\"") != std::string::npos);
  CHECK(frames[4].find("\"command\":\"setBreakpoints\"") != std::string::npos);
  CHECK(frames[4].find("\"verified\":false") != std::string::npos);
  CHECK(frames[4].find("source breakpoint not found") != std::string::npos);
  CHECK(frames[6].find("\"command\":\"continue\"") != std::string::npos);
  CHECK(frames[7].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[8].find("\"event\":\"terminated\"") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[9].find("\"totalFrames\":0") != std::string::npos);
  CHECK(frames[10].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[10].find("invalid frame id") != std::string::npos);
  CHECK(frames[11].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[11].find("invalid variable reference") != std::string::npos);
  CHECK(frames[12].find("\"command\":\"disconnect\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap end-to-end process smoke emits exit events") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_smoke.prime", source);
  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_smoke.requests", requests);
  const std::string outPath = (testScratchPath("") / "primevm_debug_dap_smoke.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  const std::string output = readFile(outPath);
  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(output, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 8);
  CHECK(frames[5].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[5].find("\"exitCode\":9") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"terminated\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap exposes non-top frame locals in variables") {
  const std::string source = R"(
[return<int>]
helper() {
  [int mut] inner{7i32}
  return(inner)
}

[return<int>]
main() {
  [int mut] outer{9i32}
  return(helper())
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_non_top_locals.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setInstructionBreakpoints","arguments":{"breakpoints":[{"instructionReference":"f1:ip2"}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"scopes","arguments":{"frameId":2}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"variables","arguments":{"variablesReference":513}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_non_top_locals.requests", requests);
  const std::string outPath = (testScratchPath("") / "primevm_debug_dap_non_top_locals.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(readFile(outPath), framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 12);
  CHECK(frames[4].find("\"command\":\"setInstructionBreakpoints\"") != std::string::npos);
  CHECK(frames[4].find("invalid breakpoint instruction pointer") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[7].find("\"event\":\"terminated\"") != std::string::npos);
  CHECK(frames[8].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[8].find("\"totalFrames\":0") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[9].find("invalid frame id") != std::string::npos);
  CHECK(frames[10].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[10].find("invalid variable reference") != std::string::npos);
  CHECK(frames[11].find("\"command\":\"disconnect\"") != std::string::npos);
}

TEST_CASE("primevm rejects primec output flags") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_reject_output_flags.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_reject_output_flags_err.txt").string();

  const std::string outputPathCmd = "./primevm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(outputPathCmd) == 2);
  const std::string outputPathErr = readFile(errPath);
  CHECK(outputPathErr.find("unknown option: -o") != std::string::npos);
  CHECK(outputPathErr.find("Usage: primevm") != std::string::npos);

  const std::string outDirCmd = "./primevm " + srcPath + " --out-dir /tmp --entry /main 2> " + errPath;
  CHECK(runCommand(outDirCmd) == 2);
  const std::string outDirErr = readFile(errPath);
  CHECK(outDirErr.find("unknown option: --out-dir") != std::string::npos);
  CHECK(outDirErr.find("Usage: primevm") != std::string::npos);
}

TEST_CASE("wasm runtime tooling hook executes or reports explicit skip") {
  const std::filesystem::path scriptPath =
      std::filesystem::current_path().parent_path() / "scripts" / "run_wasm_runtime_checks.sh";
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::string outPath =
      (testScratchPath("") / "primec_wasm_runtime_hook_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_wasm_runtime_hook_err.txt").string();

  const std::string command = quoteShellArg(scriptPath.string()) + " --build-dir " +
                              quoteShellArg(std::filesystem::current_path().string()) + " --primec ./primec > " +
                              quoteShellArg(outPath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.empty());
  if (hasWasmtime()) {
    CHECK(output.find("PASS: executed wasm runtime checks with wasmtime.") != std::string::npos);
  } else {
    CHECK(output.find("SKIP: wasmtime unavailable; wasm runtime checks not executed.") != std::string::npos);
  }
}

TEST_CASE("defaults to native output with stem name") {
  const std::string source = R"(
[return<int>]
main() {
  return(5i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_output.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec " + quoteShellArg(srcPath) + " --out-dir " + quoteShellArg(outDir.string()) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  CHECK(std::filesystem::exists(outputPath));
  CHECK(runCommand(quoteShellArg(outputPath.string())) == 5);
}

TEST_CASE("emits PSIR bytecode with --emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_ir.prime", source);
  const std::string irPath = (testScratchPath("") / "primec_emit_ir.psir").string();

  const std::string compileCmd =
      "./primec --emit=ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(irPath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(irPath));

  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data;
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
  }
  CHECK(!data.empty());
  REQUIRE(data.size() >= 8);
  auto readU32 = [&](size_t offset) -> uint32_t {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  };
  const uint32_t magic = readU32(0);
  const uint32_t version = readU32(4);
  CHECK(magic == 0x50534952u);
  CHECK(version == 21u);

  primec::IrModule module;
  std::string error;
  CHECK(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
}

TEST_CASE("primevm forwards entry args") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_args.prime", source);
  const std::string outPath = (testScratchPath("") / "primevm_args_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("primevm supports argv string bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{args[1i32]}
  print_line(first)
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_binding.prime", source);
  const std::string outPath = (testScratchPath("") / "primevm_args_binding_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs with line comments after expressions") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}// comment with a+b and a/b should be ignored
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_line_comment.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_line_comment_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_line_comment_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{'{"k":"v"}'utf8}
  [i32] k{at(text, 2i32)}
  [i32] v{at_unsafe(text, 6i32)}
  [i32] len{count(text)}
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs method calls via type namespaces") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_call_i32.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_method_call_i32_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_method_call_i32_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs count forwarding to method") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 4i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_forward.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_count_forward_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_count_forward_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs method call on constructor") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/ping([Foo] self) {
  return(9i32)
}

[return<int>]
main() {
  return(Foo{}.ping())
}
)";
  const std::string srcPath = writeTemp("compile_constructor_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_constructor_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_constructor_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs call with body block") {
  const std::string source = R"(
[return<int>]
sum([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  [i32 mut] value{0i32}
  [i32] total{ sum(2i32, 3i32) { assign(value, 7i32) } }
  return(plus(total, value))
}
)";
  const std::string srcPath = writeTemp("compile_call_body_block.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_call_body_block_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_call_body_block_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 12);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 12);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 12);
}

TEST_CASE("compiles and runs templated method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  pick<T>([i32] self, [T] other) {
    return(self)
  }
}

[return<int>]
main() {
  return(3i32.pick<int>(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_template_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_template_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_template_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    [i32] value{1i32}
    return(plus(value, 6i32))
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_block_expr_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_block_expr_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs boolean ops with conversions") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(convert<bool>(1i32), or(convert<bool>(0i32), not(convert<bool>(0i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_int.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_bool_ops_int_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_bool_ops_int_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs integer width converts") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(-1i32), 18446744073709551615u64))
}
)";
  const std::string srcPath = writeTemp("compile_integer_width_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_integer_width_convert_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_integer_width_convert_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs convert bool from negative integer") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_negative.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_convert_bool_negative_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_convert_bool_negative_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs boolean ops short-circuit") {
  const std::string source = R"(
[return<int>]
main() {
  [bool mut] witness{false}
  or(true, assign(witness, true))
  and(false, assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_short_circuit.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_bool_ops_short_circuit_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_bool_ops_short_circuit_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_SUITE_END();
