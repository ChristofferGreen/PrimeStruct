TEST_CASE("primec wasm wasi rejects malformed png inputs deterministically") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_invalid_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = {0x00, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    input.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_invalid.prime", source);
  const std::string wasmPath = (tempRoot / "png_invalid.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_invalid_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_invalid_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            0,
                            "image_invalid_operation\n"
                            "0\n"
                            "0\n"
                            "0\n");
}

TEST_CASE("primec wasm parity corpus matches vm outputs and exits deterministically") {
  struct ParityCase {
    std::string name;
    std::string source;
    int expectedExit = 0;
    std::string expectedStdout;
    std::string expectedStderr;
  };

  const std::vector<ParityCase> cases = {
      {
          "arith_calls_loops",
          R"(
[return<int>]
main() {
  [i32 mut] total{1i32}
  repeat(3i32) {
    assign(total, plus(total, 1i32))
  }
  return(plus(multiply(total, 2i32), 11i32))
}
)",
          19,
          "",
          "",
      },
      {
          "stdout_stderr",
          R"(
[return<int> effects(io_out, io_err)]
main() {
  print_line("alpha"utf8)
  print("beta"utf8)
  print_error("warn"utf8)
  print_line_error("!"utf8)
  return(0i32)
}
)",
          0,
          "alpha\nbeta",
          "warn!\n",
      },
      {
          "branching",
          R"(
[return<int>]
main() {
  [i32] value{plus(1i32, 1i32)}
  if(equal(value, 2i32)) {
    return(7i32)
  } else {
    return(3i32)
  }
}
)",
          7,
          "",
          "",
      },
  };

  struct RunResult {
    int exitCode = -1;
    std::string stdoutText;
    std::string stderrText;
  };

  const auto runAndCapture = [](const std::string &commandBase, const std::string &outPath, const std::string &errPath) {
    RunResult result;
    const std::string command = commandBase + " > " + quoteShellArg(outPath) + " 2> " + quoteShellArg(errPath);
    result.exitCode = runCommand(command);
    result.stdoutText = readFile(outPath);
    result.stderrText = readFile(errPath);
    return result;
  };

  const bool hasRuntime = hasWasmtime();
  for (const auto &parity : cases) {
    CAPTURE(parity.name);

    const std::string srcPath = writeTemp("compile_emit_wasm_vm_parity_" + parity.name + ".prime", parity.source);
    const std::string wasmPath =
        (testScratchPath("") / ("primec_emit_wasm_vm_parity_" + parity.name + ".wasm")).string();
    const std::string compileErrPath =
        (testScratchPath("") / ("primec_emit_wasm_vm_parity_" + parity.name + "_compile.err")).string();

    const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                                " --entry /main 2> " + quoteShellArg(compileErrPath);
    CHECK(runCommand(wasmCmd) == 0);
    CHECK(std::filesystem::exists(wasmPath));

    const std::string vmOutA =
        (testScratchPath("") / ("primec_vm_parity_" + parity.name + "_a.out")).string();
    const std::string vmErrA =
        (testScratchPath("") / ("primec_vm_parity_" + parity.name + "_a.err")).string();
    const std::string vmOutB =
        (testScratchPath("") / ("primec_vm_parity_" + parity.name + "_b.out")).string();
    const std::string vmErrB =
        (testScratchPath("") / ("primec_vm_parity_" + parity.name + "_b.err")).string();

    const std::string vmCmdBase = "./primevm " + quoteShellArg(srcPath) + " --entry /main";
    const RunResult vmRunA = runAndCapture(vmCmdBase, vmOutA, vmErrA);
    const RunResult vmRunB = runAndCapture(vmCmdBase, vmOutB, vmErrB);
    CHECK(vmRunA.exitCode == parity.expectedExit);
    CHECK(vmRunA.stdoutText == parity.expectedStdout);
    CHECK(vmRunA.stderrText == parity.expectedStderr);
    CHECK(vmRunB.exitCode == vmRunA.exitCode);
    CHECK(vmRunB.stdoutText == vmRunA.stdoutText);
    CHECK(vmRunB.stderrText == vmRunA.stderrText);

    if (!hasRuntime) {
      continue;
    }

    const std::string wasmOutA =
        (testScratchPath("") / ("primec_wasm_parity_" + parity.name + "_a.out")).string();
    const std::string wasmErrA =
        (testScratchPath("") / ("primec_wasm_parity_" + parity.name + "_a.err")).string();
    const std::string wasmOutB =
        (testScratchPath("") / ("primec_wasm_parity_" + parity.name + "_b.out")).string();
    const std::string wasmErrB =
        (testScratchPath("") / ("primec_wasm_parity_" + parity.name + "_b.err")).string();

    const std::string wasmCmdBase = "wasmtime --invoke main " + quoteShellArg(wasmPath);
    const RunResult wasmRunA = runAndCapture(wasmCmdBase, wasmOutA, wasmErrA);
    const RunResult wasmRunB = runAndCapture(wasmCmdBase, wasmOutB, wasmErrB);
    CHECK(wasmRunA.exitCode == parity.expectedExit);
    CHECK(wasmRunA.stdoutText == parity.expectedStdout);
    CHECK(wasmRunA.stderrText == parity.expectedStderr);
    CHECK(wasmRunB.exitCode == wasmRunA.exitCode);
    CHECK(wasmRunB.stdoutText == wasmRunA.stdoutText);
    CHECK(wasmRunB.stderrText == wasmRunA.stderrText);

    CHECK(wasmRunA.exitCode == vmRunA.exitCode);
    CHECK(wasmRunA.stdoutText == vmRunA.stdoutText);
    CHECK(wasmRunA.stderrText == vmRunA.stderrText);
  }
}

TEST_CASE("primec wasm i64 and u64 conversion edge cases trap in runtime") {
  const std::string negativeSource = R"(
[return<int>]
main() {
  return(convert<int>(convert<u64>(-1.0f32)))
}
)";
  const std::string nanSource = R"(
[return<int>]
main() {
  return(convert<int>(convert<i64>(0.0f32 / 0.0f32)))
}
)";

  const std::string negativePath = writeTemp("compile_emit_wasm_u64_negative.prime", negativeSource);
  const std::string nanPath = writeTemp("compile_emit_wasm_i64_nan.prime", nanSource);
  const std::string negativeWasmPath =
      (testScratchPath("") / "primec_emit_wasm_u64_negative.wasm").string();
  const std::string nanWasmPath =
      (testScratchPath("") / "primec_emit_wasm_i64_nan.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_convert_edges_err.txt").string();

  const std::string compileNegativeCmd = "./primec --emit=wasm " + quoteShellArg(negativePath) + " -o " +
                                         quoteShellArg(negativeWasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
  const std::string compileNanCmd = "./primec --emit=wasm " + quoteShellArg(nanPath) + " -o " +
                                    quoteShellArg(nanWasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileNegativeCmd) == 0);
  CHECK(runCommand(compileNanCmd) == 0);

  if (hasWasmtime()) {
    const std::string runNegativeCmd = "wasmtime --invoke main " + quoteShellArg(negativeWasmPath);
    const std::string runNanCmd = "wasmtime --invoke main " + quoteShellArg(nanWasmPath);
    CHECK(runCommand(runNegativeCmd) != 0);
    CHECK(runCommand(runNanCmd) != 0);
  }
}

TEST_CASE("primec emits wasm bytecode for repeat while and for loops") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  repeat(2i32) {
    assign(total, plus(total, 2i32))
  }

  [i32 mut] i{0i32}
  while(less_than(i, 3i32)) {
    assign(total, plus(total, i))
    assign(i, plus(i, 1i32))
  }

  for([i32 mut] j{0i32} less_than(j, 2i32) assign(j, plus(j, 1i32))) {
    assign(total, plus(total, j))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_loops.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_loops.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_loops_err.txt").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_wasm_loops_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("8") != std::string::npos);
  }
}

TEST_CASE("primec options default to wasm extension for emit kind") {
  std::vector<std::string> args = {
      "primec",
      "--emit=wasm",
      "/tmp/compile_default_wasm_output.prime",
  };
  std::vector<char *> argv;
  argv.reserve(args.size());
  for (std::string &arg : args) {
    argv.push_back(arg.data());
  }

  primec::Options options;
  std::string parseError;
  CHECK(primec::parseOptions(
      static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, parseError));
  CHECK(parseError.empty());
  CHECK(options.emitKind == "wasm");
  CHECK(options.outputPath == "compile_default_wasm_output.wasm");
}

TEST_CASE("primec options parse wasm profile aliases and validate values") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  {
    primec::Options options;
    std::string error;
    CHECK(parsePrimec({"primec", "--emit=wasm", "--wasm-profile=wasm-browser", "/tmp/input.prime"}, options, error));
    CHECK(error.empty());
    CHECK(options.wasmProfile == "browser");
  }

  {
    primec::Options options;
    std::string error;
    CHECK(parsePrimec({"primec", "--emit=wasm", "--wasm-profile", "wasm-wasi", "/tmp/input.prime"}, options, error));
    CHECK(error.empty());
    CHECK(options.wasmProfile == "wasi");
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", "--emit=wasm", "--wasm-profile=desktop", "/tmp/input.prime"}, options, error));
    CHECK(error.find("unsupported --wasm-profile value: desktop (expected wasi|browser)") != std::string::npos);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", "--emit=wasm", "--wasm-profile"}, options, error));
    CHECK(error.find("--wasm-profile requires a value") != std::string::npos);
  }
}

TEST_CASE("primec rejects removed type resolver option") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  {
    primec::Options options;
    std::string error;
    CHECK(parsePrimec({"primec", "--emit=ir", "/tmp/input.prime"}, options, error));
    CHECK(error.empty());
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", "--emit=ir", "--type-resolver=graph", "/tmp/input.prime"}, options, error));
    CHECK(error.find("unknown option: --type-resolver=graph") != std::string::npos);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", "--emit=ir", "--type-resolver", "legacy", "/tmp/input.prime"}, options, error));
    CHECK(error.find("unknown option: --type-resolver") != std::string::npos);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", "--emit=ir", "--type-resolver"}, options, error));
    CHECK(error.find("unknown option: --type-resolver") != std::string::npos);
  }
}

TEST_CASE("primec options parse benchmark semantic definition validation worker count") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  {
    primec::Options options;
    std::string error;
    CHECK(parsePrimec({"primec",
                       "--emit=ir",
                       "--benchmark-semantic-definition-validation-workers=4",
                       "/tmp/input.prime"},
                      options,
                      error));
    CHECK(error.empty());
    REQUIRE(options.benchmarkSemanticDefinitionValidationWorkerCount.has_value());
    CHECK(*options.benchmarkSemanticDefinitionValidationWorkerCount == 4);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec",
                             "--emit=ir",
                             "--benchmark-semantic-definition-validation-workers=0",
                             "/tmp/input.prime"},
                            options,
                            error));
    CHECK(error.find("invalid --benchmark-semantic-definition-validation-workers value: 0") != std::string::npos);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec(
        {"primec", "--emit=ir", "--benchmark-semantic-definition-validation-workers"},
        options,
        error));
    CHECK(error.find("--benchmark-semantic-definition-validation-workers requires a value") != std::string::npos);
  }
}

TEST_CASE("primec options parse benchmark semantic phase counters flag") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  primec::Options options;
  std::string error;
  CHECK(parsePrimec({"primec",
                     "--emit=ir",
                     "--benchmark-semantic-phase-counters",
                     "/tmp/input.prime"},
                    options,
                    error));
  CHECK(error.empty());
  CHECK(options.benchmarkSemanticPhaseCounters);
}

TEST_CASE("primec options parse benchmark semantic allocation counters flag") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  primec::Options options;
  std::string error;
  CHECK(parsePrimec({"primec",
                     "--emit=ir",
                     "--benchmark-semantic-allocation-counters",
                     "/tmp/input.prime"},
                    options,
                    error));
  CHECK(error.empty());
  CHECK(options.benchmarkSemanticAllocationCounters);
}

TEST_CASE("primec options parse benchmark semantic rss checkpoints flag") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  primec::Options options;
  std::string error;
  CHECK(parsePrimec({"primec",
                     "--emit=ir",
                     "--benchmark-semantic-rss-checkpoints",
                     "/tmp/input.prime"},
                    options,
                    error));
  CHECK(error.empty());
  CHECK(options.benchmarkSemanticRssCheckpoints);
}

TEST_CASE("primec options parse benchmark semantic repeat count flag") {
  auto parsePrimec = [](std::vector<std::string> args, primec::Options &options, std::string &error) {
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
      argv.push_back(arg.data());
    }
    return primec::parseOptions(
        static_cast<int>(argv.size()), argv.data(), primec::OptionsParserMode::Primec, options, error);
  };

  {
    primec::Options options;
    std::string error;
    CHECK(parsePrimec({"primec",
                       "--emit=ir",
                       "--benchmark-semantic-repeat-count=3",
                       "/tmp/input.prime"},
                      options,
                      error));
    CHECK(error.empty());
    REQUIRE(options.benchmarkSemanticRepeatCompileCount.has_value());
    CHECK(*options.benchmarkSemanticRepeatCompileCount == 3);
  }

  {
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec",
                             "--emit=ir",
                             "--benchmark-semantic-repeat-count=0",
                             "/tmp/input.prime"},
                            options,
                            error));
    CHECK(error.find("invalid --benchmark-semantic-repeat-count value: 0") != std::string::npos);
  }
}

TEST_CASE("primec emit-diagnostics reports structured wasm emit payload") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(dereference(ref))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_diagnostics.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_diagnostics.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_diagnostics_err.json").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("unsupported opcode for wasm target") != std::string::npos);
  CHECK(diagnostics.find("\"notes\":[\"backend: wasm\",\"stage: ir-validate\"]") != std::string::npos);
  CHECK(diagnostics.find("Usage: primec") == std::string::npos);
}

TEST_CASE("primec emit-diagnostics rejects unsupported wasm IR features with stable payloads") {
  struct NegativeCase {
    std::string name;
    std::string source;
    std::string expectedMessageFragment;
  };

  const std::vector<NegativeCase> cases = {
      {
          "unsupported_opcode_reference",
          R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(dereference(ref))
}
)",
          "unsupported opcode for wasm target",
      },
      {
          "unsupported_opcode_reference_alias",
          R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(dereference(location(value)))
}
)",
          "unsupported opcode for wasm target",
      },
      {
          "unsupported_effect_pathspace_notify",
          R"(
[return<int> effects(pathspace_notify)]
main() {
  return(0i32)
}
)",
          "unsupported effect mask bits for wasm target",
      },
  };

  for (const auto &negative : cases) {
    CAPTURE(negative.name);
    const std::string srcPath = writeTemp("compile_emit_wasm_negative_" + negative.name + ".prime", negative.source);
    const std::string wasmPath =
        (testScratchPath("") / ("primec_emit_wasm_negative_" + negative.name + ".wasm")).string();
    const std::string errPath =
        (testScratchPath("") / ("primec_emit_wasm_negative_" + negative.name + ".json")).string();

    const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                                " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 2);

    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("\"version\":1") != std::string::npos);
    CHECK(diagnostics.find("\"code\":\"PSC2001\"") != std::string::npos);
    CHECK(diagnostics.find("\"severity\":\"error\"") != std::string::npos);
    CHECK(diagnostics.find(negative.expectedMessageFragment) != std::string::npos);
    CHECK(diagnostics.find("\"notes\":[\"backend: wasm\",\"stage: ir-validate\"]") != std::string::npos);
    CHECK(diagnostics.find("\"stage\":\"ir-validate\"") == std::string::npos);
    CHECK(diagnostics.find("Usage: primec") == std::string::npos);
  }
}

TEST_CASE("primec wasm profile matrix gates wasi-only effects and opcodes") {
  struct ProfileCase {
    std::string name;
    std::string profile;
    std::string source;
    bool expectSuccess = false;
    std::string expectedError;
  };

  const std::vector<ProfileCase> cases = {
      {
          "wasi_accepts_io_effects",
          "wasi",
          R"(
[return<int> effects(io_out)]
main() {
  print_line("ok"utf8)
  return(0i32)
}
)",
          true,
          "",
      },
      {
          "browser_accepts_compute_subset",
          "browser",
          R"(
[return<int>]
main() {
  return(plus(2i32, 3i32))
}
)",
          true,
          "",
      },
      {
          "browser_rejects_io_effects",
          "browser",
          R"(
[return<int> effects(io_out)]
main() {
  return(0i32)
}
)",
          false,
          "unsupported effect mask bits for wasm-browser target",
      },
      {
          "browser_rejects_argv_opcode",
          "browser",
          R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)",
          false,
          "unsupported opcode for wasm-browser target",
      },
  };

  for (const auto &profileCase : cases) {
    CAPTURE(profileCase.name);
    const std::string srcPath =
        writeTemp("compile_emit_wasm_profile_matrix_" + profileCase.name + ".prime", profileCase.source);
    const std::string wasmPath =
        (testScratchPath("") / ("primec_emit_wasm_profile_matrix_" + profileCase.name + ".wasm"))
            .string();
    const std::string errPath =
        (testScratchPath("") / ("primec_emit_wasm_profile_matrix_" + profileCase.name + ".json"))
            .string();

    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile " + profileCase.profile +
                                " --default-effects none " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main --emit-diagnostics 2> " +
                                quoteShellArg(errPath);
    if (profileCase.expectSuccess) {
      CHECK(runCommand(wasmCmd) == 0);
      CHECK(std::filesystem::exists(wasmPath));
      continue;
    }

    CHECK(runCommand(wasmCmd) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find(profileCase.expectedError) != std::string::npos);
    CHECK(diagnostics.find("\"notes\":[\"backend: wasm\",\"stage: ir-validate\"]") != std::string::npos);
  }
}
