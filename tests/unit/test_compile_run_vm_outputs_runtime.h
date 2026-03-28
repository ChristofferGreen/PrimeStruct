#pragma once

TEST_CASE("cpp and exe emitters match cpp-ir and exe-ir on shared corpus") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  struct DifferentialCase {
    const char *name;
    const char *source;
    const char *runtimeArgs;
    int expectedExitCode;
  };

  const std::vector<DifferentialCase> cases = {
      {
          "i32_arithmetic",
          R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)",
          "",
          3,
      },
      {
          "argv_and_io",
          R"(
[return<int> effects(io_out, io_err)]
main([array<string>] args) {
  print_line(args[1i32])
  print_error("!"utf8)
  return(args.count())
}
)",
          " alpha beta",
          3,
      },
      {
          "dynamic_string",
          R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)",
          "",
          0,
      },
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_cpp_ir_differential_") + testCase.name + ".prime",
                                          testCase.source);
    const std::string astCppPath =
        (testScratchPath("") / (std::string("primec_cpp_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string irCppPath =
        (testScratchPath("") / (std::string("primec_cpp_ir_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string astExePath =
        (testScratchPath("") / (std::string("primec_exe_differential_") + testCase.name)).string();
    const std::string irExePath =
        (testScratchPath("") / (std::string("primec_exe_ir_differential_") + testCase.name)).string();

    const std::string compileAstCppCmd = "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astCppPath) + " --entry /main";
    const std::string compileIrCppCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irCppPath) + " --entry /main";
    CHECK(runCommand(compileAstCppCmd) == 0);
    CHECK(runCommand(compileIrCppCmd) == 0);
    const std::string astCppSource = readFile(astCppPath);
    const std::string irCppSource = readFile(irCppPath);
    CHECK(!astCppSource.empty());
    CHECK(!irCppSource.empty());
    CHECK(astCppSource == irCppSource);

    const std::string compileAstExeCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astExePath) + " --entry /main";
    const std::string compileIrExeCmd = "./primec --emit=exe-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irExePath) + " --entry /main";
    CHECK(runCommand(compileAstExeCmd) == 0);
    CHECK(runCommand(compileIrExeCmd) == 0);

    const std::string astOutPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".out"))
                                       .string();
    const std::string astErrPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".err"))
                                       .string();
    const std::string irOutPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".out"))
                                      .string();
    const std::string irErrPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".err"))
                                      .string();

    const std::string runAstCmd = quoteShellArg(astExePath) + testCase.runtimeArgs + " > " + quoteShellArg(astOutPath) +
                                  " 2> " + quoteShellArg(astErrPath);
    const std::string runIrCmd = quoteShellArg(irExePath) + testCase.runtimeArgs + " > " + quoteShellArg(irOutPath) +
                                 " 2> " + quoteShellArg(irErrPath);
    CHECK(runCommand(runAstCmd) == testCase.expectedExitCode);
    CHECK(runCommand(runIrCmd) == testCase.expectedExitCode);
    CHECK(readFile(astOutPath) == readFile(irOutPath));
    CHECK(readFile(astErrPath) == readFile(irErrPath));
  }
}

TEST_CASE("cpp and exe diagnostics match cpp-ir and exe-ir (text and json)") {
  struct DiagnosticCase {
    const char *name;
    const char *source;
  };

  const std::vector<DiagnosticCase> cases = {
      {
          "semantic_argument_mismatch",
          R"(
/consume([i32] value) {
  value
}
[return<int>]
main() {
  consume(true)
  return(0i32)
}
)",
      },
      {
          "lowering_unsupported_lambda",
          R"(
[return<int>]
main() {
  holder{[]([i32] x) { return(x) }}
  return(0i32)
}
)",
      },
      {
          "lowering_software_numeric",
          R"(
[return<decimal>]
main() {
  [decimal] value{convert<decimal>(1.5f32)}
  return(value)
}
)",
      },
      {
          "lowering_non_empty_soa_vector_literal",
          R"(
Particle() {
  [i32] x{1i32}
}
[return<int> effects(heap_alloc)]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>(Particle(1i32))}
  return(0i32)
}
)",
      },
  };

  struct EmitPair {
    const char *left;
    const char *right;
  };

  const std::vector<EmitPair> emitPairs = {
      {"cpp", "cpp-ir"},
      {"exe", "exe-ir"},
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_diagnostics_parity_") + testCase.name + ".prime",
                                          testCase.source);

    for (const auto &pair : emitPairs) {
      CAPTURE(pair.left);
      CAPTURE(pair.right);

      const std::filesystem::path tempDir = testScratchPath("");
      const std::string leftErrPath =
          (tempDir / (std::string("primec_diag_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightErrPath =
          (tempDir / (std::string("primec_diag_") + pair.right + "_" + testCase.name + ".txt")).string();
      const std::string leftJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.right + "_" + testCase.name + ".txt")).string();

      const std::string leftTextCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main 2> " + quoteShellArg(leftErrPath);
      const std::string rightTextCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main 2> " + quoteShellArg(rightErrPath);
      CHECK(runCommand(leftTextCmd) == 2);
      CHECK(runCommand(rightTextCmd) == 2);
      CHECK(readFile(leftErrPath) == readFile(rightErrPath));

      const std::string leftJsonCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                      quoteShellArg(leftJsonErrPath);
      const std::string rightJsonCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                       quoteShellArg(rightJsonErrPath);
      CHECK(runCommand(leftJsonCmd) == 2);
      CHECK(runCommand(rightJsonCmd) == 2);
      CHECK(readFile(leftJsonErrPath) == readFile(rightJsonErrPath));
    }
  }
}

TEST_CASE("compiles and runs explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_void_return.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for file read subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("/dev/null"utf8)?}
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_exe_file_read_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_file_read_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
  value
}
)";
  const std::string srcPath = writeTemp("compile_void_implicit.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_args_count_helper.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_no_newline_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv error output u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_u64_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe line error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv print in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(args[1i32])
    print_line(args[2i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv print without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_no_newline.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_no_newline_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_args_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs argv print with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_u64_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(at_unsafe(args, 1i32))
    print_line(at_unsafe(args, 2i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_unsafe_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(at_unsafe(args, 1u64))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_unsafe_u64_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs three-element array literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs literal method call in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

