TEST_SUITE_BEGIN("primestruct.compile.run.bindings");

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_void.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_binding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles with struct definition") {
  const std::string source = R"(
[struct]
data() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs assign to mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(value, 6i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs assign to reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 9i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_ref_location.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_ref_arith.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array reference helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(2i32, 7i32)}
  [Reference<array<i32>>] ref{location(values)}
  return(plus(count(ref), at(ref, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_ref_helpers.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_ref_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

static bool spinningCubeBackendsSupportArrayReturns() {
  static int cached = -1;
  if (cached != -1) {
    return cached == 1;
  }

  std::filesystem::path cubePath = std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  if (!std::filesystem::exists(cubePath)) {
    cached = 1;
    return true;
  }

  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_backend_probe.err.txt").string();
  const std::string cmd =
      "./primec --emit=ir " + quoteShellArg(cubePath.string()) + " --entry /main 2> " + quoteShellArg(errPath);
  const int code = runCommand(cmd);
  const std::string errorText = readFile(errPath);
  if (code != 0 && errorText.find("only supports returning array values") != std::string::npos) {
    cached = 0;
    return false;
  }

  cached = 1;
  return true;
}

TEST_CASE("compiles examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));

  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_examples_ir";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::filesystem::path path = entry.path();
    if (path.extension() != ".prime") {
      continue;
    }
    if (path.string().find("borrow_checker_negative") != std::string::npos) {
      continue;
    }
    if (path.filename() == "result_helpers.prime") {
      continue;
    }
    if (path.filename() == "soa_vector_ecs_draft.prime") {
      continue;
    }
    exampleFiles.push_back(path);
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());
  const bool supportsSpinningCube = spinningCubeBackendsSupportArrayReturns();

  for (const auto &path : exampleFiles) {
    if (!supportsSpinningCube && path.string().find("spinning_cube") != std::string::npos) {
      continue;
    }
    const std::string compileCmd =
        "./primec --emit=ir " + quoteShellArg(path.string()) + " --out-dir " + quoteShellArg(outDir.string()) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(outDir / (path.stem().string() + ".psir")));
  }
}

TEST_CASE("collection docs snippets stay c++ style and executable") {
  auto resolveDocPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::path("..") / "docs" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path() / "docs" / name;
    }
    return path;
  };

  const std::filesystem::path primeStructPath = resolveDocPath("PrimeStruct.md");
  const std::filesystem::path syntaxSpecPath = resolveDocPath("PrimeStruct_SyntaxSpec.md");
  const std::filesystem::path guidelinesPath = resolveDocPath("Coding_Guidelines.md");
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(syntaxSpecPath));
  REQUIRE(std::filesystem::exists(guidelinesPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string syntaxSpecDoc = readFile(syntaxSpecPath.string());
  const std::string guidelinesDoc = readFile(guidelinesPath.string());

  const std::vector<std::string> requiredPrimeStructSnippets = {
      "value.push(item)", "value.at(index)", "value[index]", "value.count()",
      "map<string, i32>{\"a\"utf8=1i32}", "to_soa(spawnQueue)",
      "two-phase: run a stable-size update pass first"};
  const std::vector<std::string> requiredSyntaxSpecSnippets = {
      "vector<i32>{1i32, 2i32}", "vector<i32>[1i32, 2i32]",
      "map<i32, i32>{1i32=2i32, 3i32=4i32}", "value.push(item)",
      "value.at(index)", "value[index]", "Structural mutation boundaries are `push`, `reserve`, `to_soa`, and `to_aos`."};
  const std::vector<std::string> requiredGuidelinesSnippets = {
      "value.push(x)", "value.at(i)", "value[i]", "value.count()",
      "[vector<i32> mut] values{vector<i32>{1, 2}}"};

  for (const std::string &snippet : requiredPrimeStructSnippets) {
    CAPTURE(snippet);
    CHECK(primeStructDoc.find(snippet) != std::string::npos);
  }
  for (const std::string &snippet : requiredSyntaxSpecSnippets) {
    CAPTURE(snippet);
    CHECK(syntaxSpecDoc.find(snippet) != std::string::npos);
  }
  for (const std::string &snippet : requiredGuidelinesSnippets) {
    CAPTURE(snippet);
    CHECK(guidelinesDoc.find(snippet) != std::string::npos);
  }

  struct SnippetCase {
    std::string tempName;
    std::string source;
    int expectedExitCode;
  };
  const std::vector<SnippetCase> snippetCases = {
      {"docs_collections_prime_struct_style.prime",
       R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32}}
  [vector<i32> mut] list{vector<i32>{4i32, 5i32}}
  [map<i32, i32>] pairs{map<i32, i32>{7i32=8i32, 9i32=10i32}}
  list.push(6i32)
  list.reserve(8i32)
  return(values[1i32] + list.at(2i32) + list.count() + at(pairs, 9i32))
}
)",
       21},
      {"docs_collections_syntax_spec_style.prime",
       R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32> mut] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] pairs{map<i32, i32>[5i32=6i32, 7i32=8i32]}
  list.push(9i32)
  return(values.at(0i32) + list[2i32] + at_unsafe(pairs, 7i32))
}
)",
       18},
      {"docs_collections_guidelines_style.prime",
       R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>{1, 2}}
  [map<i32, i32>] pairs{map<i32, i32>{7=10}}
  values.push(3)
  values.reserve(8)
  return(values[0] + values.at(2) + values.count() + at(pairs, 7))
}
)",
       17},
  };

  for (const auto &snippetCase : snippetCases) {
    CAPTURE(snippetCase.tempName);
    const std::string srcPath = writeTemp(snippetCase.tempName, snippetCase.source);
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == snippetCase.expectedExitCode);
  }
}

TEST_CASE("collection docs snippets keep statement-only mutator diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>{1i32}}
  return(values.push(2i32))
}
)";
  const std::string srcPath = writeTemp("docs_collections_statement_only_negative.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_docs_collections_statement_only_negative.err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supported as a statement") != std::string::npos);
}

TEST_CASE("spinning cube shared source compiles across profile targets") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube backend matrix until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_native_smoke").string();
  const std::string nativeMainErrPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_native_main_reject.err.txt").string();
  const std::string wasmPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_smoke.wasm").string();
  const std::string metalErrPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_smoke.err.txt").string();

  const std::string nativeMainCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                    quoteShellArg(nativePath) + " --entry /main 2> " +
                                    quoteShellArg(nativeMainErrPath);
  CHECK(runCommand(nativeMainCmd) == 2);
  CHECK(readFile(nativeMainErrPath).find("native backend does not support return type on /cubeInit") !=
        std::string::npos);

  const std::string nativeCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                quoteShellArg(nativePath) + " --entry /mainNative";
  CHECK(runCommand(nativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath) + " --entry /main";
  CHECK(runCommand(wasmBrowserCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string metalCmd = "./primec --emit=metal " + quoteShellArg(cubePath.string()) +
                               " -o /dev/null --entry /main 2> " + quoteShellArg(metalErrPath);
  CHECK(runCommand(metalCmd) == 2);
  CHECK(readFile(metalErrPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("spinning cube rotation parity entry compiles across targets") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube rotation parity until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_entry_native_smoke").string();
  const std::string wasmPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_entry_browser_smoke.wasm").string();

  const std::string nativeCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                quoteShellArg(nativePath) + " --entry /cubeRotationParity120";
  CHECK(runCommand(nativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath) + " --entry /cubeRotationParity120";
  CHECK(runCommand(wasmBrowserCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
}

TEST_CASE("spinning cube native flat frame entrypoints compile and run deterministically") {
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_flat_frame_entrypoints";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  struct NativeEntryExpectation {
    std::string entry;
    int exitCode;
  };

  const std::vector<NativeEntryExpectation> expected = {
      {"/cubeNativeMeshVertexCount", 8},
      {"/cubeNativeMeshIndexCount", 36},
      {"/cubeNativeFrameInitTick", 0},
      {"/cubeNativeFrameInitAngleMilli", 0},
      {"/cubeNativeFrameInitAxisXCenti", 100},
      {"/cubeNativeFrameInitAxisYCenti", 0},
      {"/cubeNativeFrameStepSnapshotCode", 117},
  };

  int index = 0;
  for (const auto &entry : expected) {
    const std::filesystem::path nativePath = outDir / ("native_entry_" + std::to_string(index));
    const std::string compileCmd =
        "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativePath.string()) +
        " --entry " + entry.entry;
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(nativePath));
    CHECK(runCommand(quoteShellArg(nativePath.string())) == entry.exitCode);
    index = index + 1;
  }

  const std::filesystem::path angularVelocityPath = outDir / "native_entry_angular_velocity";
  const std::string compileAngularVelocityCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(angularVelocityPath.string()) +
      " --entry /cubeNativeFrameAngularVelocityMilli";
  CHECK(runCommand(compileAngularVelocityCmd) == 0);
  CHECK(std::filesystem::exists(angularVelocityPath));
}

TEST_CASE("spinning cube native window ABI contract conformance stays deterministic") {
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_window_abi_conformance";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  struct NativeEntryExpectation {
    std::string entry;
    int exitCode;
  };

  const std::vector<NativeEntryExpectation> expected = {
      {"/cubeNativeAbiVersion", 1},
      {"/cubeNativeAbiFixedStepMillis", 16},
      {"/cubeNativeAbiStatusOk", 0},
      {"/cubeNativeAbiStatusInvalidDeltaMillis", 201},
      {"/cubeNativeAbiConformanceInitSnapshotCode", 144},
      {"/cubeNativeAbiConformanceTickStatusOkCode", 0},
      {"/cubeNativeAbiConformanceTickStatusInvalidDeltaCode", 201},
      {"/cubeNativeAbiConformanceTickSnapshotCode", 117},
      {"/cubeNativeAbiConformanceTransformUniformSnapshotCode", 152},
  };

  int index = 0;
  for (const auto &entry : expected) {
    const std::filesystem::path nativePath = outDir / ("native_abi_entry_" + std::to_string(index));
    const std::string compileCmd =
        "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativePath.string()) +
        " --entry " + entry.entry;
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(nativePath));
    CHECK(runCommand(quoteShellArg(nativePath.string())) == entry.exitCode);
    index = index + 1;
  }
}

TEST_CASE("spinning cube native window host frame stream entry stays deterministic") {
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_window_frame_stream";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path streamBinaryPath = outDir / "cube_native_frame_stream";
  const std::string compileCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(streamBinaryPath.string()) +
      " --entry /cubeNativeAbiEmitFrameStream";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(streamBinaryPath));

  const std::filesystem::path streamOutPath = outDir / "cube_native_frame_stream.out.txt";
  const std::string runCmd =
      quoteShellArg(streamBinaryPath.string()) + " > " + quoteShellArg(streamOutPath.string());
  CHECK(runCommand(runCmd) == 0);

  const std::string streamOutput = readFile(streamOutPath.string());
  CHECK(streamOutput.rfind("0\n0\n100\n0\n1\n16\n99\n1\n", 0) == 0);
  CHECK(streamOutput.find("\n2\n32\n98\n2\n") != std::string::npos);
}

TEST_CASE("spinning cube browser host assets pass pipeline smoke checks") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube browser asset checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(sampleDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  const std::filesystem::path indexPath = sampleDir / "index.html";
  const std::filesystem::path mainJsPath = sampleDir / "main.js";
  const std::filesystem::path shaderPath = sampleDir / "cube.wgsl";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(shaderPath));

  const std::string indexHtml = readFile(indexPath.string());
  CHECK(indexHtml.find("id=\"cube-canvas\"") != std::string::npos);
  CHECK(indexHtml.find("src=\"./main.js\"") != std::string::npos);

  const std::string mainJs = readFile(mainJsPath.string());
  CHECK(mainJs.find("./cube.wasm") != std::string::npos);
  CHECK(mainJs.find("./cube.wgsl") != std::string::npos);
  CHECK(mainJs.find("requestAnimationFrame") != std::string::npos);
  const std::string cubeSource = readFile(cubePath.string());
  CHECK(cubeSource.find("positionStrideBytes{16i32}") != std::string::npos);
  CHECK(cubeSource.find("colorStrideBytes{16i32}") != std::string::npos);
  const std::string shaderText = readFile(shaderPath.string());
  CHECK(shaderText.find("@vertex") != std::string::npos);
  CHECK(shaderText.find("@fragment") != std::string::npos);
  CHECK(shaderText.find("@location(0) position : vec4<f32>") != std::string::npos);
  CHECK(shaderText.find("@location(1) color : vec4<f32>") != std::string::npos);
  CHECK(shaderText.find("out.clipPosition = in.position;") != std::string::npos);

  const std::filesystem::path pipelineDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_assets";
  std::error_code ec;
  std::filesystem::remove_all(pipelineDir, ec);
  std::filesystem::create_directories(pipelineDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = pipelineDir / "cube.wasm";
  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(wasmBrowserCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  std::filesystem::copy_file(indexPath, pipelineDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, pipelineDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(shaderPath, pipelineDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  CHECK(std::filesystem::exists(pipelineDir / "index.html"));
  CHECK(std::filesystem::exists(pipelineDir / "main.js"));
  CHECK(std::filesystem::exists(pipelineDir / "cube.wgsl"));
  CHECK(std::filesystem::exists(pipelineDir / "cube.wasm"));

  if (runCommand("node --version > /dev/null 2>&1") == 0) {
    const std::string nodeCheckSource = "node --check " + quoteShellArg(mainJsPath.string()) + " > /dev/null 2>&1";
    CHECK(runCommand(nodeCheckSource) == 0);
    const std::string nodeCheckStaged =
        "node --check " + quoteShellArg((pipelineDir / "main.js").string()) + " > /dev/null 2>&1";
    CHECK(runCommand(nodeCheckStaged) == 0);
  }
}

TEST_CASE("spinning cube browser profile rules gate unsupported code") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube browser profile checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(sampleDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  REQUIRE(std::filesystem::exists(cubePath));

  {
    const std::filesystem::path wasmPath =
        std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_profile_ok.wasm";
    const std::string cmd = "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) +
                            " -o " + quoteShellArg(wasmPath.string()) + " --entry /main";
    CHECK(runCommand(cmd) == 0);
    CHECK(std::filesystem::exists(wasmPath));
  }

  {
    const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("browser"utf8)
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("spinning_cube_browser_profile_reject_io.prime", source);
    const std::string errPath =
        (std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_profile_reject_io.json").string();
    const std::string cmd = "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(srcPath) +
                            " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    CHECK(readFile(errPath).find("unsupported effect mask bits for wasm-browser target") != std::string::npos);
  }

  {
    const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
    const std::string srcPath = writeTemp("spinning_cube_browser_profile_reject_argv.prime", source);
    const std::string errPath =
        (std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_profile_reject_argv.json").string();
    const std::string cmd = "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(srcPath) +
                            " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    CHECK(readFile(errPath).find("unsupported opcode for wasm-browser target") != std::string::npos);
  }
}

TEST_CASE("spinning cube native host runtime smoke emits success marker") {
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(nativeSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path hostSourcePath = nativeSampleDir / "main.cpp";
  const std::filesystem::path hostReadmePath = nativeSampleDir / "README.md";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(hostSourcePath));
  REQUIRE(std::filesystem::exists(hostReadmePath));

  const std::filesystem::path cubeNativePath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_target_sample";
  const std::string compileCubeCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(cubeNativePath.string()) +
      " --entry /mainNative";
  CHECK(runCommand(compileCubeCmd) == 0);
  CHECK(std::filesystem::exists(cubeNativePath));
  CHECK(runCommand(quoteShellArg(cubeNativePath.string())) == 36);

  std::string cxx = "";
  if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
    cxx = "c++";
  } else if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
    cxx = "clang++";
  } else if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
    cxx = "g++";
  } else {
    INFO("no C++ compiler found; skipping native host glue compile/run");
    return;
  }

  const std::filesystem::path hostBinaryPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_host";
  const std::string compileHostCmd = cxx + " -std=c++17 " + quoteShellArg(hostSourcePath.string()) + " -o " +
                                     quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::filesystem::path hostOutPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_host_runtime.out.txt";
  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(cubeNativePath.string()) + " > " +
      quoteShellArg(hostOutPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  CHECK(readFile(hostOutPath.string()).find("native host verified cube simulation output") != std::string::npos);
}

TEST_CASE("spinning cube native window host locks indexed cube pipeline resources") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube native host pipeline checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(nativeSampleDir));

  const std::filesystem::path hostSourcePath = nativeSampleDir / "window_host.mm";
  REQUIRE(std::filesystem::exists(hostSourcePath));
  const std::string hostSource = readFile(hostSourcePath.string());

  CHECK(hostSource.find("struct WindowVertexIn") != std::string::npos);
  CHECK(hostSource.find("float4 position [[attribute(0)]];") != std::string::npos);
  CHECK(hostSource.find("float4 color [[attribute(1)]];") != std::string::npos);
  CHECK(hostSource.find("out.position = uniforms.modelViewProjection * in.position;") != std::string::npos);
  CHECK(hostSource.find("struct WindowVertex {") != std::string::npos);
  CHECK(hostSource.find("static_assert(offsetof(WindowVertex, px) == 0);") != std::string::npos);
  CHECK(hostSource.find("static_assert(offsetof(WindowVertex, pw) == 12);") != std::string::npos);
  CHECK(hostSource.find("static_assert(offsetof(WindowVertex, r) == 16);") != std::string::npos);
  CHECK(hostSource.find("static_assert(sizeof(WindowVertex) == 32);") != std::string::npos);
  CHECK(hostSource.find("static_assert(alignof(WindowVertex) == 4);") != std::string::npos);
  CHECK(hostSource.find("constant WindowUniforms &uniforms [[buffer(1)]]") != std::string::npos);
  CHECK(hostSource.find("CubeVertices") != std::string::npos);
  CHECK(hostSource.find("CubeIndices") != std::string::npos);
  CHECK(hostSource.find("newBufferWithBytes:CubeVertices.data()") != std::string::npos);
  CHECK(hostSource.find("newBufferWithBytes:CubeIndices.data()") != std::string::npos);
  CHECK(hostSource.find("newBufferWithLength:sizeof(WindowUniforms)") != std::string::npos);
  CHECK(hostSource.find("setVertexBuffer:_vertexBuffer offset:0 atIndex:0") != std::string::npos);
  CHECK(hostSource.find("setVertexBuffer:_uniformBuffer offset:0 atIndex:1") != std::string::npos);
  CHECK(hostSource.find("vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;") != std::string::npos);
  CHECK(hostSource.find("vertexDescriptor.attributes[0].offset = offsetof(WindowVertex, px);") != std::string::npos);
  CHECK(hostSource.find("vertexDescriptor.attributes[1].offset = offsetof(WindowVertex, r);") != std::string::npos);
  CHECK(hostSource.find("drawIndexedPrimitives:MTLPrimitiveTypeTriangle") != std::string::npos);
  CHECK(hostSource.find("keyCode == 53") != std::string::npos);
  CHECK(hostSource.find("handleEscapeKey") != std::string::npos);
  CHECK(hostSource.find("startup_success=1") != std::string::npos);
  CHECK(hostSource.find("startup_failure=1") != std::string::npos);
  CHECK(hostSource.find("gfx_profile=native-desktop") != std::string::npos);
  CHECK(hostSource.find("gfx_error_code=") != std::string::npos);
  CHECK(hostSource.find("gfx_error_why=") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=") != std::string::npos);
  CHECK(hostSource.find("startup_failure_reason=") != std::string::npos);
  CHECK(hostSource.find("startup_failure_exit_code=") != std::string::npos);
  CHECK(hostSource.find("window_create_failed") != std::string::npos);
  CHECK(hostSource.find("device_create_failed") != std::string::npos);
  CHECK(hostSource.find("swapchain_create_failed") != std::string::npos);
  CHECK(hostSource.find("mesh_create_failed") != std::string::npos);
  CHECK(hostSource.find("pipeline_create_failed") != std::string::npos);
  CHECK(hostSource.find("material_create_failed") != std::string::npos);
  CHECK(hostSource.find("frame_acquire_failed") != std::string::npos);
  CHECK(hostSource.find("queue_submit_failed") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=first_frame_submission") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=window_creation") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=pipeline_setup") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=shader_load") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=gpu_device_acquisition") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=simulation_stream_load") != std::string::npos);
  CHECK(hostSource.find("exit_reason=window_close") != std::string::npos);
  CHECK(hostSource.find("exit_reason=esc_key") != std::string::npos);
  CHECK(hostSource.find("exit_reason=max_frames") != std::string::npos);
}

TEST_CASE("spinning cube native window host software surface bridge stays source locked") {
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(nativeSampleDir));

  const std::filesystem::path hostSourcePath = nativeSampleDir / "window_host.mm";
  REQUIRE(std::filesystem::exists(hostSourcePath));
  const std::string hostSource = readFile(hostSourcePath.string());

  CHECK(hostSource.find("#include \"../../shared/software_surface_bridge.h\"") != std::string::npos);
  CHECK(hostSource.find("uploadSoftwareSurfaceFrameToTexture") != std::string::npos);
  CHECK(hostSource.find("software_surface_bridge=1") != std::string::npos);
  CHECK(hostSource.find("software_surface_width=") != std::string::npos);
  CHECK(hostSource.find("software_surface_height=") != std::string::npos);
  CHECK(hostSource.find("software_surface_presenter_ready=1") != std::string::npos);
  CHECK(hostSource.find("software_surface_presented=1") != std::string::npos);
  CHECK(hostSource.find("--software-surface-demo") != std::string::npos);
  CHECK(hostSource.find("--simulation-smoke is incompatible with --software-surface-demo") != std::string::npos);
  CHECK(hostSource.find("--cube-sim is incompatible with --software-surface-demo") != std::string::npos);
  CHECK(hostSource.find("copyFromTexture:_softwareSurfaceTexture") != std::string::npos);
  CHECK(hostSource.find("id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];") !=
        std::string::npos);
}

TEST_CASE("spinning cube native window host sample compiles and validates args deterministically") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube native host arg checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(nativeSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path hostSourcePath = nativeSampleDir / "window_host.mm";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(hostSourcePath));

  const std::string hostSource = readFile(hostSourcePath.string());
  CHECK(hostSource.find("#import <AppKit/AppKit.h>") != std::string::npos);
  CHECK(hostSource.find("#import <QuartzCore/CAMetalLayer.h>") != std::string::npos);
  CHECK(hostSource.find("--cube-sim") != std::string::npos);
  CHECK(hostSource.find("--simulation-smoke") != std::string::npos);
  CHECK(hostSource.find("--software-surface-demo") != std::string::npos);
  CHECK(hostSource.find("simulation_stream_loaded=1") != std::string::npos);
  CHECK(hostSource.find("simulation_fixed_step_millis=16") != std::string::npos);
  CHECK(hostSource.find("gfx_profile=native-desktop") != std::string::npos);
  CHECK(hostSource.find("shader_library_ready=1") != std::string::npos);
  CHECK(hostSource.find("vertex_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("index_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("uniform_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("startup_success=1") != std::string::npos);
  CHECK(hostSource.find("startup_failure=1") != std::string::npos);
  CHECK(hostSource.find("startup_failure_stage=") != std::string::npos);
  CHECK(hostSource.find("startup_failure_reason=") != std::string::npos);
  CHECK(hostSource.find("startup_failure_exit_code=") != std::string::npos);
  CHECK(hostSource.find("gfx_error_code=") != std::string::npos);
  CHECK(hostSource.find("gfx_error_why=") != std::string::npos);
  CHECK(hostSource.find("exit_reason=max_frames") != std::string::npos);
  CHECK(hostSource.find("exit_reason=window_close") != std::string::npos);
  CHECK(hostSource.find("exit_reason=esc_key") != std::string::npos);
  CHECK(hostSource.find("simulation_tick=") != std::string::npos);
  CHECK(hostSource.find("window_created=1") != std::string::npos);
  CHECK(hostSource.find("swapchain_layer_created=1") != std::string::npos);
  CHECK(hostSource.find("pipeline_ready=1") != std::string::npos);
  CHECK(hostSource.find("frame_rendered=1") != std::string::npos);

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("xcrun unavailable; skipping native window host compile smoke");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_window_host_compile";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "spinning_cube_window_host";
  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(hostSourcePath.string()) +
      " -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o " +
      quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::filesystem::path helpOutPath = outDir / "window_host.help.out.txt";
  const std::filesystem::path helpErrPath = outDir / "window_host.help.err.txt";
  const std::string helpCmd = quoteShellArg(hostBinaryPath.string()) + " --help > " + quoteShellArg(helpOutPath.string()) +
                              " 2> " + quoteShellArg(helpErrPath.string());
  CHECK(runCommand(helpCmd) == 0);
  CHECK(readFile(helpErrPath.string()).empty());
  CHECK(readFile(helpOutPath.string())
            .find("usage: window_host (--cube-sim <path> | --software-surface-demo) [--max-frames <positive-int>] "
                  "[--simulation-smoke]") !=
        std::string::npos);

  const std::filesystem::path missingCubeOutPath = outDir / "window_host.missing_cube.out.txt";
  const std::filesystem::path missingCubeErrPath = outDir / "window_host.missing_cube.err.txt";
  const std::string missingCubeCmd =
      quoteShellArg(hostBinaryPath.string()) + " --max-frames 1 > " + quoteShellArg(missingCubeOutPath.string()) + " 2> " +
      quoteShellArg(missingCubeErrPath.string());
  CHECK(runCommand(missingCubeCmd) == 64);
  CHECK(readFile(missingCubeOutPath.string()).empty());
  CHECK(readFile(missingCubeErrPath.string()).find("missing required --cube-sim <path> or --software-surface-demo") !=
        std::string::npos);

  const std::filesystem::path missingCubeValueOutPath = outDir / "window_host.missing_cube_value.out.txt";
  const std::filesystem::path missingCubeValueErrPath = outDir / "window_host.missing_cube_value.err.txt";
  const std::string missingCubeValueCmd =
      quoteShellArg(hostBinaryPath.string()) + " --cube-sim > " + quoteShellArg(missingCubeValueOutPath.string()) + " 2> " +
      quoteShellArg(missingCubeValueErrPath.string());
  CHECK(runCommand(missingCubeValueCmd) == 64);
  CHECK(readFile(missingCubeValueOutPath.string()).empty());
  CHECK(readFile(missingCubeValueErrPath.string()).find("missing value for --cube-sim") != std::string::npos);

  const std::filesystem::path badArgOutPath = outDir / "window_host.bad_arg.out.txt";
  const std::filesystem::path badArgErrPath = outDir / "window_host.bad_arg.err.txt";
  const std::string badArgCmd =
      quoteShellArg(hostBinaryPath.string()) + " --bogus > " + quoteShellArg(badArgOutPath.string()) + " 2> " +
      quoteShellArg(badArgErrPath.string());
  CHECK(runCommand(badArgCmd) == 64);
  CHECK(readFile(badArgOutPath.string()).empty());
  CHECK(readFile(badArgErrPath.string()).find("unknown arg: --bogus") != std::string::npos);

  const std::filesystem::path badFramesOutPath = outDir / "window_host.bad_frames.out.txt";
  const std::filesystem::path badFramesErrPath = outDir / "window_host.bad_frames.err.txt";
  const std::string badFramesCmd =
      quoteShellArg(hostBinaryPath.string()) + " --max-frames 0 > " + quoteShellArg(badFramesOutPath.string()) + " 2> " +
      quoteShellArg(badFramesErrPath.string());
  CHECK(runCommand(badFramesCmd) == 64);
  CHECK(readFile(badFramesOutPath.string()).empty());
  CHECK(readFile(badFramesErrPath.string()).find("invalid value for --max-frames: 0") != std::string::npos);

  const std::filesystem::path incompatibleModeOutPath = outDir / "window_host.incompatible_mode.out.txt";
  const std::filesystem::path incompatibleModeErrPath = outDir / "window_host.incompatible_mode.err.txt";
  const std::string incompatibleModeCmd =
      quoteShellArg(hostBinaryPath.string()) + " --software-surface-demo --simulation-smoke > " +
      quoteShellArg(incompatibleModeOutPath.string()) + " 2> " + quoteShellArg(incompatibleModeErrPath.string());
  CHECK(runCommand(incompatibleModeCmd) == 64);
  CHECK(readFile(incompatibleModeOutPath.string()).empty());
  CHECK(readFile(incompatibleModeErrPath.string()).find("--simulation-smoke is incompatible with --software-surface-demo") !=
        std::string::npos);

  const std::filesystem::path badStreamOutPath = outDir / "window_host.bad_stream.out.txt";
  const std::filesystem::path badStreamErrPath = outDir / "window_host.bad_stream.err.txt";
  const std::string badStreamCmd =
      quoteShellArg(hostBinaryPath.string()) + " --cube-sim /usr/bin/true --simulation-smoke > " +
      quoteShellArg(badStreamOutPath.string()) + " 2> " + quoteShellArg(badStreamErrPath.string());
  CHECK(runCommand(badStreamCmd) == 69);
  CHECK(readFile(badStreamOutPath.string()).empty());
  const std::string badStreamErr = readFile(badStreamErrPath.string());
  CHECK(badStreamErr.find("startup_failure=1") != std::string::npos);
  CHECK(badStreamErr.find("gfx_profile=native-desktop") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_stage=simulation_stream_load") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_reason=simulation_stream_load_failed") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_exit_code=69") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_detail=cube simulation stream was empty") != std::string::npos);

  const std::filesystem::path frameStreamBinaryPath = outDir / "cube_native_frame_stream";
  const std::string compileFrameStreamCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(frameStreamBinaryPath.string()) +
      " --entry /cubeNativeAbiEmitFrameStream";
  CHECK(runCommand(compileFrameStreamCmd) == 0);
  CHECK(std::filesystem::exists(frameStreamBinaryPath));

  const std::filesystem::path smokeOutPath = outDir / "window_host.simulation_smoke.out.txt";
  const std::filesystem::path smokeErrPath = outDir / "window_host.simulation_smoke.err.txt";
  const std::string smokeCmd = quoteShellArg(hostBinaryPath.string()) + " --cube-sim " +
                               quoteShellArg(frameStreamBinaryPath.string()) +
                               " --simulation-smoke > " + quoteShellArg(smokeOutPath.string()) + " 2> " +
                               quoteShellArg(smokeErrPath.string());
  CHECK(runCommand(smokeCmd) == 0);
  CHECK(readFile(smokeErrPath.string()).empty());
  const std::string smokeOutput = readFile(smokeOutPath.string());
  CHECK(smokeOutput.find("simulation_stream_loaded=1") != std::string::npos);
  CHECK(smokeOutput.find("simulation_frames_loaded=600") != std::string::npos);
  CHECK(smokeOutput.find("simulation_fixed_step_millis=16") != std::string::npos);
  CHECK(smokeOutput.find("simulation_smoke_tick=0") != std::string::npos);
  CHECK(smokeOutput.find("simulation_smoke_angle_milli=0") != std::string::npos);
  CHECK(smokeOutput.find("simulation_smoke_axis_x_centi=100") != std::string::npos);
  CHECK(smokeOutput.find("simulation_smoke_axis_y_centi=0") != std::string::npos);
}

TEST_CASE("spinning cube native window host software surface bridge visual smoke") {
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(nativeSampleDir));

  const std::filesystem::path hostSourcePath = nativeSampleDir / "window_host.mm";
  REQUIRE(std::filesystem::exists(hostSourcePath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; native software surface bridge smoke requires macOS toolchain");
    return;
  }
  if (runCommand("command -v launchctl > /dev/null 2>&1") != 0) {
    INFO("SKIP: launchctl unavailable; native software surface bridge smoke requires GUI-capable macOS");
    return;
  }
  if (runCommand("launchctl print gui/$(id -u) > /dev/null 2>&1") != 0) {
    INFO("SKIP: GUI session unavailable; native software surface bridge smoke requires GUI-capable macOS");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_native_window_software_surface";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "spinning_cube_window_host";
  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(hostSourcePath.string()) +
      " -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o " +
      quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::filesystem::path hostOutPath = outDir / "window_host.software_surface.out.txt";
  const std::filesystem::path hostErrPath = outDir / "window_host.software_surface.err.txt";
  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " --software-surface-demo --max-frames 1 > " +
      quoteShellArg(hostOutPath.string()) + " 2> " + quoteShellArg(hostErrPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  CHECK(readFile(hostErrPath.string()).empty());
  const std::string hostOutput = readFile(hostOutPath.string());
  CHECK(hostOutput.find("software_surface_bridge=1") != std::string::npos);
  CHECK(hostOutput.find("software_surface_width=64") != std::string::npos);
  CHECK(hostOutput.find("software_surface_height=64") != std::string::npos);
  CHECK(hostOutput.find("software_surface_presented=1") != std::string::npos);
  CHECK(hostOutput.find("frame_rendered=1") != std::string::npos);
  CHECK(hostOutput.find("exit_reason=max_frames") != std::string::npos);
}

TEST_CASE("spinning cube fixed-step snapshots stay deterministic") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube snapshot checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  REQUIRE(std::filesystem::exists(cubePath));

  constexpr int GoldenSnapshot = 45;

  const std::string vmSnapshotCmd =
      "./primec --emit=vm " + quoteShellArg(cubePath.string()) + " --entry /cubeFixedStepSnapshot120";
  CHECK(runCommand(vmSnapshotCmd) == GoldenSnapshot);

  const std::string vmSnapshotChunkedCmd =
      "./primec --emit=vm " + quoteShellArg(cubePath.string()) + " --entry /cubeFixedStepSnapshot120Chunked";
  CHECK(runCommand(vmSnapshotChunkedCmd) == GoldenSnapshot);

  const std::filesystem::path nativeSnapshotPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_fixed_step_snapshot_native";
  const std::string compileNativeSnapshotCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(nativeSnapshotPath.string()) + " --entry /cubeFixedStepSnapshot120";
  CHECK(runCommand(compileNativeSnapshotCmd) == 0);
  CHECK(std::filesystem::exists(nativeSnapshotPath));
  CHECK(runCommand(quoteShellArg(nativeSnapshotPath.string())) == GoldenSnapshot);

  const std::filesystem::path nativeSnapshotChunkedPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_fixed_step_snapshot_chunked_native";
  const std::string compileNativeSnapshotChunkedCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(nativeSnapshotChunkedPath.string()) + " --entry /cubeFixedStepSnapshot120Chunked";
  CHECK(runCommand(compileNativeSnapshotChunkedCmd) == 0);
  CHECK(std::filesystem::exists(nativeSnapshotChunkedPath));
  CHECK(runCommand(quoteShellArg(nativeSnapshotChunkedPath.string())) == GoldenSnapshot);

  const std::filesystem::path wasmSnapshotPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_fixed_step_snapshot.wasm";
  const std::string compileWasmSnapshotCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmSnapshotPath.string()) + " --entry /cubeFixedStepSnapshot120";
  CHECK(runCommand(compileWasmSnapshotCmd) == 0);
  CHECK(std::filesystem::exists(wasmSnapshotPath));
}

TEST_CASE("spinning cube transform rotation parity stays aligned across backends") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube parity checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  constexpr int ExpectedParityPass = 1;
  constexpr int ExpectedSnapshotCode = 45;

  const std::string vmParityCmd =
      "./primec --emit=vm " + quoteShellArg(cubePath.string()) + " --entry /cubeRotationParity120";
  CHECK(runCommand(vmParityCmd) == ExpectedParityPass);

  const std::filesystem::path nativeParityPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_parity_native";
  const std::string compileNativeParityCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativeParityPath.string()) +
      " --entry /cubeRotationParity120";
  CHECK(runCommand(compileNativeParityCmd) == 0);
  CHECK(std::filesystem::exists(nativeParityPath));
  CHECK(runCommand(quoteShellArg(nativeParityPath.string())) == ExpectedParityPass);

  const std::filesystem::path wasmParityPath =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_parity.wasm";
  const std::string compileWasmParityCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmParityPath.string()) + " --entry /cubeRotationParity120";
  CHECK(runCommand(compileWasmParityCmd) == 0);
  CHECK(std::filesystem::exists(wasmParityPath));

  if (hasWasmtime()) {
    const std::filesystem::path wasmOutPath =
        std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_parity_wasm.out.txt";
    const std::string runWasmParityCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmParityPath.string()) + " > " + quoteShellArg(wasmOutPath.string());
    CHECK(runCommand(runWasmParityCmd) == 0);
    const std::string wasmOutput = readFile(wasmOutPath.string());
    CHECK(wasmOutput.find("1") != std::string::npos);
  }

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("xcrun not available; skipping metal-hosted rotation parity smoke");
    return;
  }

  const std::filesystem::path hostBuildDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_rotation_parity_metal_host";
  std::error_code ec;
  std::filesystem::remove_all(hostBuildDir, ec);
  std::filesystem::create_directories(hostBuildDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = hostBuildDir / "metal_host";
  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::filesystem::path hostParityOutPath = hostBuildDir / "parity.out.txt";
  const std::string runHostParityCmd = quoteShellArg(hostBinaryPath.string()) +
                                       " --parity-check 120 > " + quoteShellArg(hostParityOutPath.string());
  CHECK(runCommand(runHostParityCmd) == 0);
  const std::string hostParityOutput = readFile(hostParityOutPath.string());
  CHECK(hostParityOutput.find("parity_ok=1") != std::string::npos);

  const std::filesystem::path hostSnapshotOutPath = hostBuildDir / "snapshot.out.txt";
  const std::string runHostSnapshotCmd = quoteShellArg(hostBinaryPath.string()) +
                                         " --snapshot-code 120 > " + quoteShellArg(hostSnapshotOutPath.string());
  CHECK(runCommand(runHostSnapshotCmd) == 0);
  const std::string hostSnapshotOutput = readFile(hostSnapshotOutPath.string());
  CHECK(hostSnapshotOutput.find("snapshot_code=" + std::to_string(ExpectedSnapshotCode)) != std::string::npos);
}

TEST_CASE("spinning cube integration artifact matrix stays valid") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube artifact matrix checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path indexPath = webSampleDir / "index.html";
  const std::filesystem::path mainJsPath = webSampleDir / "main.js";
  const std::filesystem::path wgslPath = webSampleDir / "cube.wgsl";
  const std::filesystem::path metalPath = metalSampleDir / "cube.metal";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(metalPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_artifact_matrix";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = outDir / "cube.wasm";
  const std::filesystem::path nativePath = outDir / "cube_native";
  const std::filesystem::path loaderDir = outDir / "loader";
  std::filesystem::create_directories(loaderDir, ec);
  REQUIRE(!ec);

  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativePath.string()) +
      " --entry /mainNative";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));
  CHECK(std::filesystem::file_size(nativePath) > 0);

  std::filesystem::copy_file(indexPath, loaderDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, loaderDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, loaderDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  CHECK(std::filesystem::exists(loaderDir / "index.html"));
  CHECK(std::filesystem::exists(loaderDir / "main.js"));
  CHECK(std::filesystem::exists(loaderDir / "cube.wgsl"));
  CHECK(readFile((loaderDir / "index.html").string()).find("cube-canvas") != std::string::npos);
  CHECK(readFile((loaderDir / "main.js").string()).find("./cube.wasm") != std::string::npos);
  CHECK(readFile((loaderDir / "cube.wgsl").string()).find("@vertex") != std::string::npos);

  const auto readBinary = [](const std::filesystem::path &path, std::vector<unsigned char> &bytes) {
    bytes.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
      return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
      return false;
    }
    bytes.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(bytes.data()), size);
    return file.good() || file.eof();
  };

  const auto fnv1a64 = [](const std::vector<unsigned char> &bytes) {
    unsigned long long hash = 1469598103934665603ull;
    for (unsigned char b : bytes) {
      hash ^= static_cast<unsigned long long>(b);
      hash *= 1099511628211ull;
    }
    return hash;
  };

  std::vector<unsigned char> wasmBytes;
  REQUIRE(readBinary(wasmPath, wasmBytes));
  REQUIRE(wasmBytes.size() >= 8);
  CHECK(wasmBytes[0] == 0x00);
  CHECK(wasmBytes[1] == 0x61);
  CHECK(wasmBytes[2] == 0x73);
  CHECK(wasmBytes[3] == 0x6d);

  std::vector<unsigned char> nativeBytes;
  REQUIRE(readBinary(nativePath, nativeBytes));

  std::vector<std::pair<std::string, std::filesystem::path>> artifacts = {
      {"cube.wasm", wasmPath},
      {"cube_native", nativePath},
      {"loader/index.html", loaderDir / "index.html"},
      {"loader/main.js", loaderDir / "main.js"},
      {"loader/cube.wgsl", loaderDir / "cube.wgsl"},
  };

  const bool hasXcrun = runCommand("xcrun --version > /dev/null 2>&1") == 0;
  if (hasXcrun) {
    const std::filesystem::path airPath = outDir / "cube.air";
    const std::filesystem::path metallibPath = outDir / "cube.metallib";
    const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalPath.string()) + " -o " +
                                        quoteShellArg(airPath.string());
    CHECK(runCommand(compileMetalCmd) == 0);
    CHECK(std::filesystem::exists(airPath));
    const std::string compileMetallibCmd =
        "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
    CHECK(runCommand(compileMetallibCmd) == 0);
    CHECK(std::filesystem::exists(metallibPath));
    artifacts.push_back({"cube.air", airPath});
    artifacts.push_back({"cube.metallib", metallibPath});
  }

  const std::filesystem::path manifestPath = outDir / "artifact_manifest.json";
  std::ofstream manifest(manifestPath);
  REQUIRE(manifest.good());
  manifest << "{\n";
  manifest << "  \"version\": 1,\n";
  manifest << "  \"profile\": \"spinning_cube\",\n";
  manifest << "  \"artifacts\": [\n";
  for (size_t i = 0; i < artifacts.size(); ++i) {
    const auto &entry = artifacts[i];
    std::vector<unsigned char> bytes;
    REQUIRE(readBinary(entry.second, bytes));
    const unsigned long long hash = fnv1a64(bytes);
    manifest << "    {\"name\":\"" << entry.first << "\",\"size\":" << bytes.size() << ",\"hash_fnv1a64\":\"" << hash
             << "\"}";
    if (i + 1 != artifacts.size()) {
      manifest << ",";
    }
    manifest << "\n";
  }
  manifest << "  ]\n";
  manifest << "}\n";
  manifest.close();
  CHECK(std::filesystem::exists(manifestPath));

  const std::string manifestText = readFile(manifestPath.string());
  CHECK(manifestText.find("\"version\": 1") != std::string::npos);
  CHECK(manifestText.find("\"profile\": \"spinning_cube\"") != std::string::npos);
  CHECK(manifestText.find("\"artifacts\": [") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"cube.wasm\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"cube_native\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/index.html\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/main.js\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/cube.wgsl\"") != std::string::npos);
  CHECK(manifestText.find("\"hash_fnv1a64\":\"0\"") == std::string::npos);
}

TEST_CASE("spinning cube optional startup visual smoke checks") {
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(nativeSampleDir));
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path indexPath = webSampleDir / "index.html";
  const std::filesystem::path mainJsPath = webSampleDir / "main.js";
  const std::filesystem::path wgslPath = webSampleDir / "cube.wgsl";
  const std::filesystem::path nativeHostPath = nativeSampleDir / "main.cpp";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(nativeHostPath));
  REQUIRE(std::filesystem::exists(metalHostPath));
  REQUIRE(std::filesystem::exists(metalShaderPath));

  // Native visual startup proxy: host bootstrap prints a stable ready marker.
  {
    const std::filesystem::path nativeExePath =
        std::filesystem::temp_directory_path() / "primec_spinning_cube_visual_native";
    const std::string compileNativeCmd =
        "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativeExePath.string()) +
        " --entry /mainNative";
    CHECK(runCommand(compileNativeCmd) == 0);
    CHECK(std::filesystem::exists(nativeExePath));

    std::string cxx = "";
    if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
      cxx = "c++";
    } else if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
      cxx = "clang++";
    } else if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
      cxx = "g++";
    } else {
      INFO("no C++ compiler found; skipping native startup visual smoke");
      cxx.clear();
    }
    if (!cxx.empty()) {
      const std::filesystem::path hostPath =
          std::filesystem::temp_directory_path() / "primec_spinning_cube_visual_native_host";
      const std::string compileHostCmd = cxx + " -std=c++17 " + quoteShellArg(nativeHostPath.string()) + " -o " +
                                         quoteShellArg(hostPath.string());
      CHECK(runCommand(compileHostCmd) == 0);
      const std::filesystem::path hostOutPath =
          std::filesystem::temp_directory_path() / "primec_spinning_cube_visual_native_host.out.txt";
      const std::string runHostCmd = quoteShellArg(hostPath.string()) + " " + quoteShellArg(nativeExePath.string()) +
                                     " > " + quoteShellArg(hostOutPath.string());
      CHECK(runCommand(runHostCmd) == 0);
      CHECK(readFile(hostOutPath.string()).find("native host verified cube simulation output") != std::string::npos);
    }
  }

  // Metal visual startup smoke (headless GPU where available).
  if (runCommand("xcrun --version > /dev/null 2>&1") == 0) {
    const std::filesystem::path outDir =
        std::filesystem::temp_directory_path() / "primec_spinning_cube_visual_metal";
    std::error_code ec;
    std::filesystem::remove_all(outDir, ec);
    std::filesystem::create_directories(outDir, ec);
    REQUIRE(!ec);

    const std::filesystem::path airPath = outDir / "cube.air";
    const std::filesystem::path metallibPath = outDir / "cube.metallib";
    const std::filesystem::path hostPath = outDir / "metal_host";
    const std::filesystem::path hostOutPath = outDir / "metal_host.out.txt";

    const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                        " -o " + quoteShellArg(airPath.string());
    CHECK(runCommand(compileMetalCmd) == 0);
    const std::string compileLibCmd =
        "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
    CHECK(runCommand(compileLibCmd) == 0);
    const std::string compileHostCmd =
        "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
        " -framework Foundation -framework Metal -o " + quoteShellArg(hostPath.string());
    CHECK(runCommand(compileHostCmd) == 0);
    const std::string runHostCmd =
        quoteShellArg(hostPath.string()) + " " + quoteShellArg(metallibPath.string()) + " > " +
        quoteShellArg(hostOutPath.string());
    CHECK(runCommand(runHostCmd) == 0);
    CHECK(readFile(hostOutPath.string()).find("frame_rendered=1") != std::string::npos);
  } else {
    INFO("xcrun unavailable; skipping metal startup visual smoke");
  }

  // Browser visual smoke (headless where possible, explicit skip otherwise).
  std::string browserCmd = "";
  if (runCommand("chromium --version > /dev/null 2>&1") == 0) {
    browserCmd = "chromium";
  } else if (runCommand("google-chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome";
  } else if (runCommand("chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "chrome";
  } else if (runCommand("google-chrome-stable --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome-stable";
  }

  if (browserCmd.empty() || runCommand("python3 --version > /dev/null 2>&1") != 0) {
    INFO("no headless browser+python available; skip browser startup visual smoke");
    INFO("interactive fallback: run python3 -m http.server in examples/web/spinning_cube and open index.html");
    return;
  }

  if (runCommand(browserCmd + " --headless --disable-gpu --dump-dom about:blank > /dev/null 2>&1") != 0) {
    INFO("browser runtime lacks headless mode; skip browser startup visual smoke");
    INFO("interactive fallback: run python3 -m http.server in examples/web/spinning_cube and open index.html");
    return;
  }

  const std::filesystem::path browserOutDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_visual_browser";
  std::error_code ec;
  std::filesystem::remove_all(browserOutDir, ec);
  std::filesystem::create_directories(browserOutDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = browserOutDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);

  std::filesystem::copy_file(indexPath, browserOutDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, browserOutDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, browserOutDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  const std::filesystem::path serverLogPath = browserOutDir / "server.log";
  const std::filesystem::path serverPidPath = browserOutDir / "server.pid";
  const std::filesystem::path browserDomPath = browserOutDir / "browser.dom.txt";
  const std::filesystem::path browserErrPath = browserOutDir / "browser.err.txt";

  const int port = 18765;
  const std::string startServerCmd = "python3 -m http.server " + std::to_string(port) + " --bind 127.0.0.1 --directory " +
                                     quoteShellArg(browserOutDir.string()) + " > " + quoteShellArg(serverLogPath.string()) +
                                     " 2>&1 & echo $! > " + quoteShellArg(serverPidPath.string());
  CHECK(runCommand(startServerCmd) == 0);
  CHECK(runCommand("sleep 1") == 0);

  const std::string runBrowserCmd = browserCmd + " --headless --disable-gpu --virtual-time-budget=5000 --dump-dom " +
                                    "http://127.0.0.1:" + std::to_string(port) + "/index.html > " +
                                    quoteShellArg(browserDomPath.string()) + " 2> " + quoteShellArg(browserErrPath.string());
  const int browserCode = runCommand(runBrowserCmd);

  const std::string stopServerCmd = "kill $(cat " + quoteShellArg(serverPidPath.string()) + ") > /dev/null 2>&1";
  runCommand(stopServerCmd);

  CHECK(browserCode == 0);
  const std::string dom = readFile(browserDomPath.string());
  CHECK(dom.find("PrimeStruct Spinning Cube Host") != std::string::npos);
  const bool hasHostRunning = dom.find("Host running") != std::string::npos;
  const bool hasWasmLoadSkipped = dom.find("Wasm load skipped") != std::string::npos;
  const bool hasExpectedStatus = hasHostRunning || hasWasmLoadSkipped;
  CHECK(hasExpectedStatus);
}

TEST_CASE("spinning cube browser startup smoke proves wasm bootstrap") {
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(sampleDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  const std::filesystem::path indexPath = sampleDir / "index.html";
  const std::filesystem::path mainJsPath = sampleDir / "main.js";
  const std::filesystem::path wgslPath = sampleDir / "cube.wgsl";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));

  std::string browserCmd = "";
  if (runCommand("chromium --version > /dev/null 2>&1") == 0) {
    browserCmd = "chromium";
  } else if (runCommand("google-chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome";
  } else if (runCommand("chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "chrome";
  } else if (runCommand("google-chrome-stable --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome-stable";
  }

  if (browserCmd.empty() || runCommand("python3 --version > /dev/null 2>&1") != 0) {
    INFO("no headless browser+python available; skip spinning cube browser startup smoke");
    return;
  }
  if (runCommand(browserCmd + " --headless --disable-gpu --dump-dom about:blank > /dev/null 2>&1") != 0) {
    INFO("browser runtime lacks headless mode; skip spinning cube browser startup smoke");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_startup_smoke";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = outDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  std::filesystem::copy_file(indexPath, outDir / "index.html", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, outDir / "main.js", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, outDir / "cube.wgsl", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  const std::filesystem::path serverLogPath = outDir / "server.log";
  const std::filesystem::path serverPidPath = outDir / "server.pid";
  const std::filesystem::path browserDomPath = outDir / "browser.dom.txt";
  const std::filesystem::path browserErrPath = outDir / "browser.err.txt";

  const int port = 18767;
  const std::string startServerCmd = "python3 -m http.server " + std::to_string(port) + " --bind 127.0.0.1 --directory " +
                                     quoteShellArg(outDir.string()) + " > " + quoteShellArg(serverLogPath.string()) +
                                     " 2>&1 & echo $! > " + quoteShellArg(serverPidPath.string());
  CHECK(runCommand(startServerCmd) == 0);
  CHECK(runCommand("sleep 1") == 0);

  const std::string runBrowserCmd = browserCmd + " --headless --disable-gpu --virtual-time-budget=6000 --dump-dom " +
                                    "http://127.0.0.1:" + std::to_string(port) + "/index.html > " +
                                    quoteShellArg(browserDomPath.string()) + " 2> " + quoteShellArg(browserErrPath.string());
  const int browserCode = runCommand(runBrowserCmd);
  runCommand("kill $(cat " + quoteShellArg(serverPidPath.string()) + ") > /dev/null 2>&1");

  CHECK(browserCode == 0);
  const std::string dom = readFile(browserDomPath.string());
  CHECK(dom.find("Host running with cube.wasm and cube.wgsl bootstrap.") != std::string::npos);
  CHECK(dom.find("Wasm load skipped") == std::string::npos);
}

TEST_CASE("spinning cube docs command snippets stay executable") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube docs command checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webReadmePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "README.md";
  if (!std::filesystem::exists(webReadmePath)) {
    webReadmePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "README.md";
  }
  REQUIRE(std::filesystem::exists(webReadmePath));
  const std::string readme = readFile(webReadmePath.string());

  const std::vector<std::string> requiredCommandSnippets = {
      "./primec --emit=wasm --wasm-profile browser examples/web/spinning_cube/cube.prime -o "
      "examples/web/spinning_cube/cube.wasm --entry /main",
      "python3 -m http.server 8080 --bind 127.0.0.1 --directory examples/web/spinning_cube",
      "./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native --entry /mainNative",
      "c++ -std=c++17 examples/native/spinning_cube/main.cpp -o /tmp/spinning_cube_host",
      "./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native_frame_stream --entry /cubeNativeAbiEmitFrameStream",
      "xcrun clang++ -std=c++17 -fobjc-arc examples/native/spinning_cube/window_host.mm -framework Foundation -framework "
      "AppKit -framework QuartzCore -framework Metal -o /tmp/spinning_cube_window_host",
      "/tmp/spinning_cube_window_host --cube-sim /tmp/cube_native_frame_stream --max-frames 120",
      "/tmp/spinning_cube_window_host --software-surface-demo --max-frames 1",
      "uploads a deterministic BGRA8 software color buffer into a",
      "shared Metal texture and blits it into the window drawable.",
      "### Native Window Launcher (macOS)",
      "./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec",
      "./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec --visual-smoke",
      "Defaults to `--max-frames 600` for normal windowed runs (about 10 seconds at",
      "60 fps), satisfying the native done-condition smoke target.",
      "`window_shown`: `window_created=1` and `startup_success=1`.",
      "`render_loop_alive`: `frame_rendered=1` and `exit_reason=max_frames`.",
      "`rotation_changes_over_time`: first two `angleMilli` values from",
      "CI skip rules for `--visual-smoke`: exits `0` with a `VISUAL-SMOKE: SKIP`",
      "marker on non-macOS runners or when `launchctl print gui/<uid>` fails.",
      "xcrun metal -std=metal3.0 -c examples/metal/spinning_cube/cube.metal -o /tmp/cube.air",
      "xcrun metallib /tmp/cube.air -o /tmp/cube.metallib",
      "xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework "
      "Metal -o /tmp/metal_host",
      "/tmp/metal_host /tmp/cube.metallib",
      "/tmp/metal_host --software-surface-demo",
      "shared Metal texture and blits it into the host target texture.",
      "### Native Windowed Execution Preflight (macOS)",
      "./scripts/preflight_native_spinning_cube_window.sh",
      "`xcrun --find metal`, `xcrun --find metallib`, and",
      "`launchctl print gui/<uid>` before host launch.",
      "./scripts/run_spinning_cube_demo.sh --primec ./build-debug/primec",
      "## First Supported Native Window Target (v1)",
      "Target: macOS + Metal window host (`examples/native/spinning_cube/window_host.mm`).",
      "Required frameworks: `Foundation`, `Metal`, `AppKit`, and `QuartzCore`.",
      "Minimum OS: macOS 14.0 (Sonoma).",
      "No Linux/Windows native window host support.",
      "Native emit `/main` is currently unsupported (`native backend does not support return type on /cubeInit`).",
      "Native smoke runs through `/mainNative` and `examples/native/spinning_cube/main.cpp`.",
      "macOS now has a real native window host sample at",
      "`examples/native/spinning_cube/window_host.mm` (window/layer/render-loop bring-up).",
      "`cubeNativeFrameInit*`, `cubeNativeFrameStep*`,",
      "`cubeNativeMeshVertexCount`, `cubeNativeMeshIndexCount`, and",
      "`cubeNativeFrameStepSnapshotCode`.",
      "`cubeNativeAbiEmitFrameStream`.",
      "## Native Window Host ABI Contract (v1)",
      "`cubeNativeAbiVersion` returns `1`.",
      "`cubeNativeAbiFixedStepMillis` returns `16` (host step size in milliseconds).",
      "`cubeNativeAbiTickStatus(deltaMillis)` validates step input.",
      "`cubeNativeAbiUniformMeshIndexCount`",
      "`201` (`cubeNativeAbiStatusInvalidDeltaMillis`) means invalid tick delta.",
      "Conformance wrappers (`cubeNativeAbiConformance*`) lock deterministic ABI",
      "Rendering: submits indexed cube draw calls each frame with transform",
      "uniforms updated from the deterministic simulation stream.",
      "Diagnostics: prints `gfx_profile=native-desktop`,",
      "`simulation_stream_loaded=1`,",
      "`simulation_fixed_step_millis=16`, `shader_library_ready=1`,",
      "`vertex_buffer_ready=1`, `index_buffer_ready=1`,",
      "`uniform_buffer_ready=1`, `window_created=1`,",
      "`swapchain_layer_created=1`, `pipeline_ready=1`, `startup_success=1`,",
      "`frame_rendered=1`, and `exit_reason=max_frames`.",
      "Failure diagnostics: startup-stage failures print deterministic",
      "`gfx_profile`, `startup_failure_stage`, `startup_failure_reason`,",
      "`startup_failure_exit_code`, and graphics-path `gfx_error_code` /",
      "`gfx_error_why` fields before exit.",
      "For a visible rotating window today, use the browser path (`index.html` + `main.js`).",
      "shared-source `/main` is still unsupported for native emit until",
      "Diagnostics: prints `native host verified cube simulation output`.",
      "Diagnostics: prints `frame_rendered=1`.",
      "FPS/diagnostic overlay: status text under the canvas is the current",
  };
  for (const std::string &snippet : requiredCommandSnippets) {
    CAPTURE(snippet);
    CHECK(readme.find(snippet) != std::string::npos);
  }
  CHECK(readme.find("Render the same spinning cube as a native desktop executable.") == std::string::npos);
  CHECK(readme.find("A native desktop run shows the same rotating cube behavior from the same simulation source.") ==
        std::string::npos);

  std::filesystem::path rootDir = std::filesystem::current_path();
  if (!std::filesystem::exists(rootDir / "examples" / "web" / "spinning_cube")) {
    rootDir = rootDir.parent_path();
  }
  REQUIRE(std::filesystem::exists(rootDir / "examples" / "web" / "spinning_cube"));

  const std::filesystem::path cubePath = rootDir / "examples" / "web" / "spinning_cube" / "cube.prime";
  const std::filesystem::path indexPath = rootDir / "examples" / "web" / "spinning_cube" / "index.html";
  const std::filesystem::path mainJsPath = rootDir / "examples" / "web" / "spinning_cube" / "main.js";
  const std::filesystem::path wgslPath = rootDir / "examples" / "web" / "spinning_cube" / "cube.wgsl";
  const std::filesystem::path nativeHostPath = rootDir / "examples" / "native" / "spinning_cube" / "main.cpp";
  const std::filesystem::path nativeWindowHostPath = rootDir / "examples" / "native" / "spinning_cube" / "window_host.mm";
  const std::filesystem::path metalShaderPath = rootDir / "examples" / "metal" / "spinning_cube" / "cube.metal";
  const std::filesystem::path metalHostPath = rootDir / "examples" / "metal" / "spinning_cube" / "metal_host.mm";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(nativeHostPath));
  REQUIRE(std::filesystem::exists(nativeWindowHostPath));
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_doc_command_smoke";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = outDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::filesystem::path browserDir = outDir / "browser";
  std::filesystem::create_directories(browserDir, ec);
  REQUIRE(!ec);
  std::filesystem::copy_file(indexPath, browserDir / "index.html", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, browserDir / "main.js", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, browserDir / "cube.wgsl", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wasmPath, browserDir / "cube.wasm", std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  if (runCommand("python3 --version > /dev/null 2>&1") == 0 && runCommand("curl --version > /dev/null 2>&1") == 0) {
    const std::filesystem::path serverPidPath = outDir / "server.pid";
    const std::filesystem::path serverLogPath = outDir / "server.log";
    const std::filesystem::path curlOutPath = outDir / "index.curl.txt";
    const int port = 18766;
    const std::string startServerCmd = "python3 -m http.server " + std::to_string(port) +
                                       " --bind 127.0.0.1 --directory " + quoteShellArg(browserDir.string()) + " > " +
                                       quoteShellArg(serverLogPath.string()) + " 2>&1 & echo $! > " +
                                       quoteShellArg(serverPidPath.string());
    CHECK(runCommand(startServerCmd) == 0);
    CHECK(runCommand("sleep 1") == 0);
    const std::string curlCmd = "curl -fsS http://127.0.0.1:" + std::to_string(port) + "/index.html > " +
                                quoteShellArg(curlOutPath.string());
    CHECK(runCommand(curlCmd) == 0);
    runCommand("kill $(cat " + quoteShellArg(serverPidPath.string()) + ") > /dev/null 2>&1");
    const std::string html = readFile(curlOutPath.string());
    CHECK(html.find("PrimeStruct Spinning Cube Host") != std::string::npos);
    CHECK(html.find("cube-canvas") != std::string::npos);
  } else {
    INFO("python3/curl unavailable; skipping scripted browser serve smoke");
  }

  const std::filesystem::path nativePath = outDir / "cube_native";
  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativePath.string()) +
      " --entry /mainNative";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

  const std::filesystem::path nativeFrameStreamPath = outDir / "cube_native_frame_stream";
  const std::string compileNativeFrameStreamCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(nativeFrameStreamPath.string()) + " --entry /cubeNativeAbiEmitFrameStream";
  CHECK(runCommand(compileNativeFrameStreamCmd) == 0);
  CHECK(std::filesystem::exists(nativeFrameStreamPath));

  std::string cxx = "";
  if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
    cxx = "c++";
  } else if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
    cxx = "clang++";
  } else if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
    cxx = "g++";
  }
  if (!cxx.empty()) {
    const std::filesystem::path nativeHostBinPath = outDir / "spinning_cube_host";
    const std::string compileHostCmd = cxx + " -std=c++17 " + quoteShellArg(nativeHostPath.string()) + " -o " +
                                       quoteShellArg(nativeHostBinPath.string());
    CHECK(runCommand(compileHostCmd) == 0);
    const std::filesystem::path nativeHostOutPath = outDir / "native_host.out.txt";
    const std::string runHostCmd = quoteShellArg(nativeHostBinPath.string()) + " " + quoteShellArg(nativePath.string()) +
                                   " > " + quoteShellArg(nativeHostOutPath.string());
    CHECK(runCommand(runHostCmd) == 0);
    CHECK(readFile(nativeHostOutPath.string()).find("native host verified cube simulation output") != std::string::npos);
  } else {
    INFO("no C++ compiler available; skipping scripted native host doc-command smoke");
  }

  if (runCommand("xcrun --version > /dev/null 2>&1") == 0) {
    const std::filesystem::path nativeWindowHostBinPath = outDir / "spinning_cube_window_host";
    const std::string compileWindowHostCmd =
        "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(nativeWindowHostPath.string()) +
        " -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o " +
        quoteShellArg(nativeWindowHostBinPath.string());
    CHECK(runCommand(compileWindowHostCmd) == 0);
    CHECK(std::filesystem::exists(nativeWindowHostBinPath));

    const std::filesystem::path airPath = outDir / "cube.air";
    const std::filesystem::path metallibPath = outDir / "cube.metallib";
    const std::filesystem::path metalHostBinPath = outDir / "metal_host";
    const std::filesystem::path metalHostOutPath = outDir / "metal_host.out.txt";
    const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) + " -o " +
                                        quoteShellArg(airPath.string());
    CHECK(runCommand(compileMetalCmd) == 0);
    const std::string compileMetallibCmd =
        "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
    CHECK(runCommand(compileMetallibCmd) == 0);
    const std::string compileMetalHostCmd =
        "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
        " -framework Foundation -framework Metal -o " + quoteShellArg(metalHostBinPath.string());
    CHECK(runCommand(compileMetalHostCmd) == 0);
    const std::string runMetalHostCmd =
        quoteShellArg(metalHostBinPath.string()) + " " + quoteShellArg(metallibPath.string()) + " > " +
        quoteShellArg(metalHostOutPath.string());
    CHECK(runCommand(runMetalHostCmd) == 0);
    CHECK(readFile(metalHostOutPath.string()).find("frame_rendered=1") != std::string::npos);
  } else {
    INFO("xcrun unavailable; skipping scripted metal doc-command smoke");
  }
}

TEST_CASE("spinning cube demo script emits deterministic summary") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path workDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script";
  std::error_code ec;
  std::filesystem::remove_all(workDir, ec);
  std::filesystem::create_directories(workDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = workDir / "script.out.txt";
  const std::filesystem::path errPath = workDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec ./primec --work-dir " +
                              quoteShellArg(workDir.string()) + " > " + quoteShellArg(outPath.string()) + " 2> " +
                              quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] SUMMARY") != std::string::npos);

  const bool webStatusPresent = output.find("[spinning-cube-demo] WEB: PASS (") != std::string::npos ||
                                output.find("[spinning-cube-demo] WEB: SKIP (") != std::string::npos;
  const bool nativeStatusPresent = output.find("[spinning-cube-demo] NATIVE: PASS (") != std::string::npos ||
                                   output.find("[spinning-cube-demo] NATIVE: SKIP (") != std::string::npos;
  const bool metalStatusPresent = output.find("[spinning-cube-demo] METAL: PASS (") != std::string::npos ||
                                  output.find("[spinning-cube-demo] METAL: SKIP (") != std::string::npos;
  CHECK(webStatusPresent);
  CHECK(nativeStatusPresent);
  CHECK(metalStatusPresent);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script skips known native backend limitation") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_native_skip";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script accepts primec path with spaces") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_primec_path_spaces";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin with spaces";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec stub";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script accepts work-dir path with spaces") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_work_dir_spaces";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path workDirPath = outDir / "work dir with spaces";
  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg(workDirPath.string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
  CHECK(std::filesystem::exists(workDirPath / "native"));
}

TEST_CASE("spinning cube demo script skips when browser command is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_browser_unavailable";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakePython << "  echo \"Python 3.11.0\"\n";
    fakePython << "  exit 0\n";
    fakePython << "fi\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const char *browserNames[] = {"chromium", "google-chrome", "chrome", "google-chrome-stable"};
  for (const char *name : browserNames) {
    const std::filesystem::path browserPath = binDir / name;
    std::ofstream browserStub(browserPath);
    browserStub << "#!/usr/bin/env bash\n";
    browserStub << "exit 1\n";
    REQUIRE(runCommand("chmod +x " + quoteShellArg(browserPath.string())) == 0);
  }

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (headless browser unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script reports fail when native compile fails") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_native_fail";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"synthetic native compile failure\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 1);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: FAIL (failed to compile cube native binary)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: FAIL") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects non-integer port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_invalid_port";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base not-a-number > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base must be an integer: not-a-number") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects out-of-range port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_invalid_port_range";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base 70000 > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 70000") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects zero port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_zero_port";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base 0 > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 0") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing port base value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_missing_port_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing primec value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_missing_primec_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_primec_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec --work-dir /tmp > " + quoteShellArg(outPath.string()) + " 2> " +
                              quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_primec_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec -- > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing primec binary path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_missing_primec_binary";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /definitely/not/a/real/primec > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: /definitely/not/a/real/primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing default primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_missing";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + (tempRoot / "build-debug" / "primec").string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unexecutable default primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_unexecutable_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path defaultPrimecPath = tempRoot / "build-debug" / "primec";
  std::filesystem::create_directories(defaultPrimecPath.parent_path(), ec);
  REQUIRE(!ec);
  {
    std::ofstream defaultPrimec(defaultPrimecPath);
    defaultPrimec << "#!/usr/bin/env bash\n";
    defaultPrimec << "exit 0\n";
  }
  REQUIRE(runCommand("chmod 644 " + quoteShellArg(defaultPrimecPath.string())) == 0);

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_unexecutable";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + defaultPrimecPath.string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects default primec directory path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_dir_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path defaultPrimecPath = tempRoot / "build-debug" / "primec";
  std::filesystem::create_directories(defaultPrimecPath, ec);
  REQUIRE(!ec);

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_default_primec_dir";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + defaultPrimecPath.string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unexecutable primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_unexecutable_primec";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = outDir / "not_executable_primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
  }
  REQUIRE(runCommand("chmod 644 " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(fakePrimecPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: " + fakePrimecPath.string()) !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec directory path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_primec_dir";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecDir = outDir / "primec_dir";
  std::filesystem::create_directories(fakePrimecDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(fakePrimecDir.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: " + fakePrimecDir.string()) !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing work-dir value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_missing_workdir_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --work-dir > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects work-dir followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_workdir_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec /bin/true --work-dir -- > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects work-dir followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_workdir_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec /bin/true --work-dir --port-base 18870 > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects port-base followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_port_base_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec /bin/true --port-base --work-dir /tmp > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects port-base followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_port_base_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base -- > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown arg") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_unknown_arg";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --bogus > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown arg as first token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_unknown_arg_first";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --bogus > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown positional token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_unknown_positional";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " foo > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: foo") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects bare dashdash token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_demo_script_unknown_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " -- > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --") != std::string::npos);
}

TEST_CASE("native window launcher script builds and launches with preflight") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_success";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "if [[ -z \"$out\" ]]; then\n";
    fakePrimec << "  echo \"missing -o\" >&2\n";
    fakePrimec << "  exit 91\n";
    fakePrimec << "fi\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  if [[ -z \"$out\" ]]; then\n";
    fakeXcrun << "    echo \"missing -o\" >&2\n";
    fakeXcrun << "    exit 92\n";
    fakeXcrun << "  fi\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_host_invoked=1\"\n";
    fakeXcrun << "echo \"window_host_args=$*\"\n";
    fakeXcrun << "if [[ \"${1:-}\" != \"--cube-sim\" || -z \"${2:-}\" ]]; then\n";
    fakeXcrun << "  echo \"bad args\" >&2\n";
    fakeXcrun << "  exit 93\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ -x \"${2}\" ]]; then\n";
    fakeXcrun << "  echo \"cube_sim_exists=1\"\n";
    fakeXcrun << "else\n";
    fakeXcrun << "  echo \"cube_sim_exists=0\"\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --max-frames 8 --simulation-smoke > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-preflight] PASS: prerequisites satisfied") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling cube simulation stream binary") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling native window host") != std::string::npos);
  CHECK(output.find("window_host_invoked=1") != std::string::npos);
  CHECK(output.find("window_host_args=--cube-sim") != std::string::npos);
  CHECK(output.find("--max-frames 8 --simulation-smoke") != std::string::npos);
  CHECK(output.find("cube_sim_exists=1") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
}

TEST_CASE("native window launcher script rejects missing primec binary path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_missing_primec";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path missingPrimecPath = outDir / "missing-primec";
  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(missingPrimecPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[native-window-launcher] ERROR: primec binary not found: ") !=
        std::string::npos);
}

TEST_CASE("native window launcher script rejects unknown args") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_unknown_arg";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --bogus > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[native-window-launcher] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("native window launcher script rejects incompatible smoke flags") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_incompatible_smoke_flags";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --simulation-smoke --visual-smoke > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-launcher] ERROR: --simulation-smoke and --visual-smoke cannot be combined") !=
        std::string::npos);
}

TEST_CASE("native window launcher defaults to ten-second bounded run") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_default_ten_second_run";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_host_args=$*\"\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-launcher] Defaulting to --max-frames 600 (~10s at 60fps)") != std::string::npos);
  CHECK(output.find("window_host_args=--cube-sim") != std::string::npos);
  CHECK(output.find("--max-frames 600") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
}

TEST_CASE("native window launcher visual smoke skips on non-macOS runners") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_visual_smoke_skip_non_macos";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Linux\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " +
                              quoteShellArg(scriptPath.string()) + " --visual-smoke > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);
  CHECK(readFile(errPath.string()).empty());
  CHECK(readFile(outPath.string()).find("[native-window-launcher] VISUAL-SMOKE: SKIP non-macOS runner") !=
        std::string::npos);
}

TEST_CASE("native window launcher visual smoke skips without GUI session") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_visual_smoke_skip_no_gui";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 1\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " +
                              quoteShellArg(scriptPath.string()) + " --visual-smoke > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);
  CHECK(readFile(errPath.string()).empty());
  CHECK(readFile(outPath.string()).find("[native-window-launcher] VISUAL-SMOKE: SKIP GUI session unavailable") !=
        std::string::npos);
}

TEST_CASE("native window launcher visual smoke validates criteria") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_native_window_visual_smoke_success";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "cat <<'EOD'\n";
    fakePrimec << "0\n0\n100\n0\n1\n16\n99\n1\n";
    fakePrimec << "EOD\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_created=1\"\n";
    fakeXcrun << "echo \"startup_success=1\"\n";
    fakeXcrun << "echo \"frame_rendered=1\"\n";
    fakeXcrun << "echo \"exit_reason=max_frames\"\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build-output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --visual-smoke --max-frames 4 > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: window_shown=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: render_loop_alive=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: rotation_changes_over_time=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: PASS") != std::string::npos);
}

TEST_CASE("native window launcher visual smoke fails when rotation does not change") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() / "primec_native_window_visual_smoke_rotation_fail";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "cat <<'EOD'\n";
    fakePrimec << "0\n0\n100\n0\n1\n0\n99\n1\n";
    fakePrimec << "EOD\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_created=1\"\n";
    fakeXcrun << "echo \"startup_success=1\"\n";
    fakeXcrun << "echo \"frame_rendered=1\"\n";
    fakeXcrun << "echo \"exit_reason=max_frames\"\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build-output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --visual-smoke --max-frames 4 > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(errPath.string()).find(
            "[native-window-launcher] ERROR: visual smoke criterion failed: rotation_changes_over_time") !=
        std::string::npos);
}

TEST_CASE("native window launcher compile run coverage validates host build and visual smoke") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  if (runCommand("uname -s | grep -Fq Darwin") != 0) {
    INFO("SKIP: non-macOS runner; launcher compile/run coverage requires macOS");
    return;
  }
  if (runCommand("command -v launchctl > /dev/null 2>&1") != 0) {
    INFO("SKIP: launchctl unavailable; launcher compile/run coverage requires GUI-capable macOS");
    return;
  }
  if (runCommand("launchctl print gui/$(id -u) > /dev/null 2>&1") != 0) {
    INFO("SKIP: GUI session unavailable; launcher compile/run coverage requires GUI-capable macOS");
    return;
  }
  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; launcher compile/run coverage requires Xcode command-line tools");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; launcher compile/run coverage requires Metal tooling");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; launcher compile/run coverage requires Metal tooling");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_launcher_compile_run_coverage";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path launcherOutPath = outDir / "launcher.out.txt";
  const std::filesystem::path launcherErrPath = outDir / "launcher.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec ./primec --out-dir " + quoteShellArg(outDir.string()) +
      " --visual-smoke --max-frames 120 > " + quoteShellArg(launcherOutPath.string()) + " 2> " +
      quoteShellArg(launcherErrPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(launcherOutPath.string());
  const std::string diagnostics = readFile(launcherErrPath.string());
  CHECK(diagnostics.find("[native-window-launcher] ERROR:") == std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling cube simulation stream binary") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling native window host") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Launching native window host") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: window_shown=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: render_loop_alive=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: rotation_changes_over_time=PASS") !=
        std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: SKIP") == std::string::npos);

  CHECK(std::filesystem::exists(outDir / "cube_native_frame_stream"));
  CHECK(std::filesystem::exists(outDir / "spinning_cube_window_host"));
  CHECK(std::filesystem::exists(outDir / "native_window_host.log"));
}

TEST_CASE("native window preflight script validates required tools and GUI session") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_preflight_success";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-preflight] xcrun metal: /fake/toolchain/metal") != std::string::npos);
  CHECK(output.find("[native-window-preflight] xcrun metallib: /fake/toolchain/metallib") != std::string::npos);
  CHECK(output.find("[native-window-preflight] GUI session: gui/") != std::string::npos);
  CHECK(output.find("[native-window-preflight] PASS: prerequisites satisfied") != std::string::npos);
}

TEST_CASE("native window preflight script fails when xcrun metal is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_preflight_missing_metal";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  exit 1\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: xcrun metal unavailable; run 'xcrun --find metal' after installing "
            "Command Line Tools.") != std::string::npos);
}

TEST_CASE("native window preflight script fails when xcrun metallib is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_preflight_missing_metallib";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  exit 1\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: xcrun metallib unavailable; run 'xcrun --find metallib' after "
            "installing Command Line Tools.") != std::string::npos);
}

TEST_CASE("native window preflight script fails when GUI session is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_preflight_missing_gui";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 1\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: GUI session unavailable; log in via macOS desktop session and rerun.") !=
        std::string::npos);
}

TEST_CASE("native window preflight script rejects unknown args") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_native_window_preflight_unknown_arg";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --bogus > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[native-window-preflight] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("spinning cube metal shader path compiles and enforces profile gating") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  const std::filesystem::path metalReadmePath = metalSampleDir / "README.md";
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalReadmePath));

  const std::string shaderText = readFile(metalShaderPath.string());
  CHECK(shaderText.find("vertex") != std::string::npos);
  CHECK(shaderText.find("fragment") != std::string::npos);
  CHECK(shaderText.find("float4 position [[attribute(0)]];") != std::string::npos);
  CHECK(shaderText.find("float4 color [[attribute(1)]];") != std::string::npos);
  CHECK(shaderText.find("out.position = in.position;") != std::string::npos);

  {
    const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("spinning_cube_metal_profile_reject.prime", source);
    const std::string errPath =
        (std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_profile_reject.err.txt").string();
    const std::string cmd = "./primec --emit=wasm --wasm-profile metal-osx " + quoteShellArg(srcPath) +
                            " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    CHECK(readFile(errPath).find("unsupported --wasm-profile value: metal-osx (expected wasi|browser)") !=
          std::string::npos);
  }

  {
    const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("spinning_cube_metal_emit_reject.prime", source);
    const std::string errPath =
        (std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_emit_reject.err.txt").string();
    const std::string cmd = "./primec --emit=metal " + quoteShellArg(srcPath) +
                            " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
  }

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("xcrun not available; skipping macOS metal/metallib shader compile smoke");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_shader_path";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path airPath = outDir / "cube.air";
  const std::filesystem::path metallibPath = outDir / "cube.metallib";
  const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                      " -o " + quoteShellArg(airPath.string());
  CHECK(runCommand(compileMetalCmd) == 0);
  CHECK(std::filesystem::exists(airPath));
  CHECK(std::filesystem::file_size(airPath) > 0);

  const std::string compileLibraryCmd =
      "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
  CHECK(runCommand(compileLibraryCmd) == 0);
  CHECK(std::filesystem::exists(metallibPath));
  CHECK(std::filesystem::file_size(metallibPath) > 0);
}

TEST_CASE("spinning cube metal host pipeline config locks vertex descriptor wiring") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  const std::string source = readFile(metalHostPath.string());
  CHECK(source.find("gfx_profile=") != std::string::npos);
  CHECK(source.find("metal-osx") != std::string::npos);
  CHECK(source.find("gfx_error_code=") != std::string::npos);
  CHECK(source.find("gfx_error_why=") != std::string::npos);
  CHECK(source.find("device_create_failed") != std::string::npos);
  CHECK(source.find("mesh_create_failed") != std::string::npos);
  CHECK(source.find("pipeline_create_failed") != std::string::npos);
  CHECK(source.find("frame_acquire_failed") != std::string::npos);
  CHECK(source.find("queue_submit_failed") != std::string::npos);
  CHECK(source.find("newLibraryWithURL") != std::string::npos);
  CHECK(source.find("newLibraryWithFile") == std::string::npos);
  CHECK(source.find("static_assert(offsetof(Vertex, px) == 0);") != std::string::npos);
  CHECK(source.find("static_assert(offsetof(Vertex, pw) == 12);") != std::string::npos);
  CHECK(source.find("static_assert(offsetof(Vertex, r) == 16);") != std::string::npos);
  CHECK(source.find("static_assert(sizeof(Vertex) == 32);") != std::string::npos);
  CHECK(source.find("static_assert(alignof(Vertex) == 4);") != std::string::npos);
  CHECK(source.find("MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[0].format = MTLVertexFormatFloat4;") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[0].offset = offsetof(Vertex, px);") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[1].format = MTLVertexFormatFloat4;") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[1].offset = offsetof(Vertex, r);") != std::string::npos);
  CHECK(source.find("vertexDesc.layouts[0].stride = sizeof(Vertex);") != std::string::npos);
  CHECK(source.find("pipelineDesc.vertexDescriptor = vertexDesc;") != std::string::npos);
}

TEST_CASE("spinning cube metal host software surface bridge stays source locked") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  const std::string source = readFile(metalHostPath.string());
  CHECK(source.find("#include \"../../shared/software_surface_bridge.h\"") != std::string::npos);
  CHECK(source.find("uploadSoftwareSurfaceFrame(") != std::string::npos);
  CHECK(source.find("renderSoftwareSurfaceDemo()") != std::string::npos);
  CHECK(source.find("--software-surface-demo") != std::string::npos);
  CHECK(source.find("software_surface_bridge=1") != std::string::npos);
  CHECK(source.find("software_surface_width=") != std::string::npos);
  CHECK(source.find("software_surface_height=") != std::string::npos);
  CHECK(source.find("software_surface_presented=1") != std::string::npos);
  CHECK(source.find("id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];") !=
        std::string::npos);
  CHECK(source.find("copyFromTexture:softwareSurfaceTexture") != std::string::npos);
}

TEST_CASE("spinning cube vertexcolored snippets stay source locked") {
  std::filesystem::path docsPath = std::filesystem::path("..") / "docs" / "Coding_Guidelines.md";
  std::filesystem::path sampleDir = std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  if (!std::filesystem::exists(docsPath)) {
    docsPath = std::filesystem::current_path() / "docs" / "Coding_Guidelines.md";
  }
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(docsPath));
  REQUIRE(std::filesystem::exists(sampleDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  REQUIRE(std::filesystem::exists(cubePath));

  const std::string guidelines = readFile(docsPath.string());
  const std::string cubeSource = readFile(cubePath.string());

  CHECK(guidelines.find("VertexColored([position] Vec4(-1.0, -1.0, -1.0, 1.0),") != std::string::npos);
  CHECK(guidelines.find("VertexColored([position] Vec4( 1.0,  1.0,  1.0, 1.0),") != std::string::npos);
  CHECK(guidelines.find("[vertex_type] VertexColored,") != std::string::npos);
  CHECK(cubeSource.find("positionStrideBytes{16i32}") != std::string::npos);
  CHECK(cubeSource.find("colorStrideBytes{16i32}") != std::string::npos);
}

TEST_CASE("software renderer command list docs stay source locked") {
  std::filesystem::path graphicsDocPath = std::filesystem::path("..") / "docs" / "Graphics_API_Design.md";
  std::filesystem::path specDocPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(graphicsDocPath)) {
    graphicsDocPath = std::filesystem::current_path() / "docs" / "Graphics_API_Design.md";
  }
  if (!std::filesystem::exists(specDocPath)) {
    specDocPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  REQUIRE(std::filesystem::exists(graphicsDocPath));
  REQUIRE(std::filesystem::exists(specDocPath));

  const std::string graphicsDoc = readFile(graphicsDocPath.string());
  const std::string specDoc = readFile(specDocPath.string());

  CHECK(graphicsDoc.find("The initial stdlib prototype lives under `/std/ui/*`") != std::string::npos);
  CHECK(graphicsDoc.find("`Rgba8`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_text(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_rounded_rect(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.push_clip(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.clip_depth()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree`") != std::string::npos);
  CHECK(graphicsDoc.find("`LoginFormNodes`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_root_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_leaf(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.measure()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.arrange(x, y, width, height)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.begin_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.end_panel()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.bind_event(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_move(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_preedit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_commit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_resize(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_gained(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_lost(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.event_count()`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("First word: format version (`1`)") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `draw_text`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `draw_rounded_rect`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `push_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `pop_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/HtmlCommandList.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `emit_panel`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `emit_label`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `emit_button`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `emit_input`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `bind_event`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `click`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `input`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/UiEventStream.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `pointer_move`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `pointer_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `pointer_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `key_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `key_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`6` = `ime_preedit`") != std::string::npos);
  CHECK(graphicsDoc.find("`7` = `ime_commit`") != std::string::npos);
  CHECK(graphicsDoc.find("`8` = `resize`") != std::string::npos);
  CHECK(graphicsDoc.find("`9` = `focus_gained`") != std::string::npos);
  CHECK(graphicsDoc.find("`10` = `focus_lost`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `shift`, `2` = `control`, `4` = `alt`, `8` = `meta`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`pop_clip()` at depth `0` is a deterministic no-op") != std::string::npos);
  CHECK(graphicsDoc.find("Single-root flat tree; nodes are appended in parent-before-child") != std::string::npos);
  CHECK(graphicsDoc.find("`measure()` walks reverse insertion order") != std::string::npos);
  CHECK(graphicsDoc.find("`arrange(x, y, width, height)` assigns the root rectangle") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `leaf`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `column`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_label(...)` creates a leaf whose measured width is") != std::string::npos);
  CHECK(graphicsDoc.find("`append_button(...)` creates a leaf whose measured size is the label") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_input(...)` creates a leaf whose measured height matches the") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_button(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_input(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_panel(...)` creates a column-backed container node") != std::string::npos);
  CHECK(graphicsDoc.find("`begin_panel(...)` emits one rounded rect for the panel background") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`end_panel()` emits one balancing `pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_login_form(...) -> LoginFormNodes` is the first composite widget helper") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_login_form(...)` emits only through `begin_panel`, `draw_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("must not call raw `draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`emit_login_form(...)` emits only through `emit_panel`, `emit_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("must not call raw `append_word`, `append_color`, or `append_string`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_pointer_move(...)`, `push_pointer_down(...)`, and `push_pointer_up(...)` normalize through one pointer event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_key_down(...)` and `push_key_up(...)` normalize through one key event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_ime_preedit(...)` and `push_ime_commit(...)` normalize through one IME event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_resize(...)`, `push_focus_gained(...)`, and `push_focus_lost(...)` normalize through one view event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("can upload a deterministic BGRA8 software surface into a shared Metal") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`--software-surface-demo`") != std::string::npos);
  CHECK(graphicsDoc.find("[CommandList mut] commands{CommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("tree.append_root_column(2i32, 3i32, 10i32, 4i32)") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_label(root, 10i32, \"Hi\"utf8)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_button(") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.begin_panel(layout, panel, 4i32") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[HtmlCommandList mut] html{HtmlCommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("html.emit_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[UiEventStream mut] events{UiEventStream()}") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_preedit(login.usernameInput, 1i32, 4i32, \"al|\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_commit(login.usernameInput, \"alice\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_resize(login.panel, 40i32, 57i32)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_gained(login.usernameInput)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_lost(login.usernameInput)") != std::string::npos);
  CHECK(specDoc.find("the first `/std/ui/*` foundation now includes deterministic command-list rendering, a two-pass layout tree contract, basic control emission, a basic panel container primitive, and the first composite widget helper") !=
        std::string::npos);
  CHECK(specDoc.find("`draw_label`, `draw_button`, `draw_input`, `begin_panel`, `end_panel`, `draw_login_form`, `HtmlCommandList`, `emit_panel`, `emit_label`, `emit_button`, `emit_input`, `bind_event`, `emit_login_form`, `UiEventStream`, `push_pointer_move`, `push_pointer_down`, `push_pointer_up`, `push_key_down`, `push_key_up`, `push_ime_preedit`, `push_ime_commit`, `push_resize`, `push_focus_gained`, `push_focus_lost`, `LayoutTree`, `LoginFormNodes`, `append_root_column`, `append_column`, `append_leaf`, `append_label`, `append_button`, `append_input`, `append_panel`, `append_login_form`, `measure`, `arrange`, deterministic `serialize()` output") != std::string::npos);
  CHECK(specDoc.find("blit a deterministic BGRA8 software surface through the native window presenter") !=
        std::string::npos);
  CHECK(specDoc.find("shared widget/layout model can also emit deterministic HTML/backend adapter records") !=
        std::string::npos);
  CHECK(specDoc.find("normalize pointer, keyboard, IME, resize, and focus input into deterministic UI event-stream records") !=
        std::string::npos);
}

TEST_CASE("image api docs and stdlib stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string imageStdlib = readFile(imageStdlibPath.string());

  CHECK(primeStructDoc.find("the shared image file-I/O API currently lives under `/std/image/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`pixels` is a flat `vector<i32>` in RGB byte order") != std::string::npos);
  CHECK(primeStructDoc.find("requires `effects(file_write)`; `ppm.read(...)` and `png.read(...)` also require `heap_alloc`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("unsupported max values, non-positive dimensions, missing binary-raster separators, truncated payloads, and out-of-range ASCII component values deterministically return `image_invalid_operation`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(...)` now emits ASCII `P3` PPM files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("invalid dimensions, payload mismatches, out-of-range components, and file-open/write failures deterministically return `image_invalid_operation`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(...)` now validates PNG signatures/chunks and fully decodes the current PNG read subset") !=
        std::string::npos);
  CHECK(primeStructDoc.find("non-interlaced 8-bit RGB/RGBA images whose `IDAT` payload uses stored/no-compression deflate blocks, fixed-Huffman deflate blocks, or dynamic-Huffman deflate blocks, with filter-`0` and filter-`1` (`Sub`) scanlines") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Fixed-Huffman reads now cover both literal-only payloads and length/distance backreferences with overlapping copy semantics, and dynamic-Huffman reads now cover both literal-only payloads and length/distance backreferences with explicit code-length tables") !=
        std::string::npos);
  CHECK(primeStructDoc.find("dropping alpha when decoding RGBA inputs") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Malformed or missing PNGs still return `image_invalid_operation`, while still-valid PNGs that require remaining PNG scanline filters or broader color/interlace handling currently return `image_read_unsupported`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write` still deterministically returns unsupported `ImageError` values") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ImageError.why()` currently returns `image_read_unsupported`, `image_write_unsupported`, or `image_invalid_operation`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(ppm.read(width, height, pixels, \"input.ppm\"utf8)))") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(png.write(\"output.png\"utf8, width, height, pixels)))") !=
        std::string::npos);

  CHECK(imageStdlib.find("[public struct]\n  ImageError()") != std::string::npos);
  CHECK(imageStdlib.find("return(1i32)") != std::string::npos);
  CHECK(imageStdlib.find("return(2i32)") != std::string::npos);
  CHECK(imageStdlib.find("return(3i32)") != std::string::npos);
  CHECK(imageStdlib.find("\"image_read_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_write_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_invalid_operation\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("namespace ppm") != std::string::npos);
  CHECK(imageStdlib.find("namespace png") != std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_write, heap_alloc)]\n    read([i32 mut] width, [i32 mut] height, [vector<i32> mut] pixels, [string] path)") !=
        std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_write)]\n    write([string] path, [i32] width, [i32] height, [vector<i32>] pixels)") !=
        std::string::npos);

  const size_t ppmStart = imageStdlib.find("namespace ppm");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(ppmStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > ppmStart);
  const std::string ppmBody = imageStdlib.substr(ppmStart, pngStart - ppmStart);
  CHECK(ppmBody.find("ppmReadAsciiInt") != std::string::npos);
  CHECK(ppmBody.find("ppmReadBinaryRasterLead") != std::string::npos);
  CHECK(ppmBody.find("ppmOpenRead") != std::string::npos);
  CHECK(ppmBody.find("ppmOpenWrite") != std::string::npos);
  CHECK(ppmBody.find("ppmWriteInputValid") != std::string::npos);
  CHECK(ppmBody.find("ppmWriteHeader") != std::string::npos);
  CHECK(ppmBody.find("ppmWriteComponent") != std::string::npos);
  CHECK(ppmBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(ppmBody.find("return(Result.ok())") != std::string::npos);
  CHECK(ppmBody.find("return(unsupported_write())") == std::string::npos);
  CHECK(ppmBody.find("return(1i32)") == std::string::npos);
  CHECK(ppmBody.find("return(2i32)") == std::string::npos);

  const std::string pngBody = imageStdlib.substr(pngStart);
  CHECK(pngBody.find("return(unsupported_read())") != std::string::npos);
  CHECK(pngBody.find("pngValidateSignature") != std::string::npos);
  CHECK(pngBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngBody.find("pngReadChunkType") != std::string::npos);
  CHECK(pngBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngBody.find("pngInflateFixedHuffmanBlock") != std::string::npos);
  CHECK(pngBody.find("pngBuildFixedDistanceLengths") != std::string::npos);
  CHECK(pngBody.find("pngInflateCopyFromOutput") != std::string::npos);
  CHECK(pngBody.find("pngDecodeScanlines") != std::string::npos);
  CHECK(pngBody.find("pngAppendBytes") != std::string::npos);
  CHECK(pngBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(pngBody.find("return(unsupported_write())") != std::string::npos);
  CHECK(pngBody.find("return(1i32)") == std::string::npos);
  CHECK(pngBody.find("return(2i32)") == std::string::npos);
}

TEST_CASE("file read_byte docs and helpers stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path preludePath = std::filesystem::path("..") / "src" / "emitter" / "EmitterEmitPrelude.h";
  std::filesystem::path lowererPath = std::filesystem::path("..") / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(preludePath)) {
    preludePath = std::filesystem::current_path() / "src" / "emitter" / "EmitterEmitPrelude.h";
  }
  if (!std::filesystem::exists(lowererPath)) {
    lowererPath = std::filesystem::current_path() / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(preludePath));
  REQUIRE(std::filesystem::exists(lowererPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string prelude = readFile(preludePath.string());
  const std::string lowerer = readFile(lowererPath.string());

  CHECK(primeStructDoc.find("`read_byte([i32 mut] value)`") != std::string::npos);
  CHECK(primeStructDoc.find("`read_byte(...)` reports deterministic end-of-file as `EOF`") != std::string::npos);

  CHECK(prelude.find("static inline uint32_t ps_file_read_byte") != std::string::npos);
  CHECK(prelude.find("return 65536u;") != std::string::npos);
  CHECK(prelude.find("return std::string_view(\"EOF\")") != std::string::npos);

  CHECK(lowerer.find("read_byte requires exactly one argument") != std::string::npos);
  CHECK(lowerer.find("read_byte requires mutable integer binding") != std::string::npos);
  CHECK(lowerer.find("emitInstruction(IrOpcode::FileReadByte") != std::string::npos);
}

TEST_CASE("software renderer composite widgets stay source locked to basic widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t drawLoginStart = source.find("draw_login_form(");
  const size_t pushClipStart = source.find("\n    [public effects(heap_alloc), return<void>]\n    push_clip(");
  REQUIRE(drawLoginStart != std::string::npos);
  REQUIRE(pushClipStart != std::string::npos);
  REQUIRE(pushClipStart > drawLoginStart);
  const std::string drawLoginBody = source.substr(drawLoginStart, pushClipStart - drawLoginStart);
  CHECK(drawLoginBody.find("self.begin_panel(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_label(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_input(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_button(") != std::string::npos);
  CHECK(drawLoginBody.find("self.end_panel()") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_text(") == std::string::npos);
  CHECK(drawLoginBody.find("self.draw_rounded_rect(") == std::string::npos);
  CHECK(drawLoginBody.find("self.push_clip(") == std::string::npos);
  CHECK(drawLoginBody.find("self.pop_clip(") == std::string::npos);

  const size_t appendLoginStart = source.find("append_login_form(");
  const size_t appendLeafStart = source.find("\n    [public effects(heap_alloc), return<i32>]\n    append_leaf(");
  REQUIRE(appendLoginStart != std::string::npos);
  REQUIRE(appendLeafStart != std::string::npos);
  REQUIRE(appendLeafStart > appendLoginStart);
  const std::string appendLoginBody = source.substr(appendLoginStart, appendLeafStart - appendLoginStart);
  CHECK(appendLoginBody.find("self.append_panel(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_label(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_input(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_button(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_leaf(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_column(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_node(") == std::string::npos);
}

TEST_CASE("software renderer html adapter stays source locked to shared widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t emitLoginStart = source.find("emit_login_form(");
  const size_t serializeStart =
      source.find("\n    [public effects(heap_alloc), return<vector<i32>>]\n    serialize(", emitLoginStart);
  REQUIRE(emitLoginStart != std::string::npos);
  REQUIRE(serializeStart != std::string::npos);
  REQUIRE(serializeStart > emitLoginStart);
  const std::string emitLoginBody = source.substr(emitLoginStart, serializeStart - emitLoginStart);
  CHECK(emitLoginBody.find("self.emit_panel(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_label(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_input(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_button(") != std::string::npos);
  CHECK(emitLoginBody.find("self.append_word(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_color(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_string(") == std::string::npos);
}

TEST_CASE("software renderer ui event stream stays source locked to normalized helpers") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t pointerMoveStart = source.find("push_pointer_move(");
  const size_t keyDownStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_key_down(", pointerMoveStart);
  REQUIRE(pointerMoveStart != std::string::npos);
  REQUIRE(keyDownStart != std::string::npos);
  REQUIRE(keyDownStart > pointerMoveStart);
  const std::string pointerBody = source.substr(pointerMoveStart, keyDownStart - pointerMoveStart);
  CHECK(pointerBody.find("self.append_pointer_event(1i32, targetNodeId, pointerId, -1i32, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(2i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(3i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_word(") == std::string::npos);

  const size_t eventCountStart =
      source.find("\n    [public return<i32>]\n    event_count(", keyDownStart);
  const size_t imeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_ime_preedit(", keyDownStart);
  REQUIRE(imeStart != std::string::npos);
  REQUIRE(imeStart > keyDownStart);
  REQUIRE(eventCountStart != std::string::npos);
  const size_t resizeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_resize(", imeStart);
  REQUIRE(resizeStart != std::string::npos);
  REQUIRE(resizeStart > imeStart);
  REQUIRE(eventCountStart > resizeStart);
  const std::string keyBody = source.substr(keyDownStart, imeStart - keyDownStart);
  CHECK(keyBody.find("self.append_key_event(4i32, targetNodeId, keyCode, modifierMask, isRepeat)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_key_event(5i32, targetNodeId, keyCode, modifierMask, 0i32)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_word(") == std::string::npos);

  const std::string imeBody = source.substr(imeStart, resizeStart - imeStart);
  CHECK(imeBody.find("self.append_ime_event(6i32, targetNodeId, selectionStart, selectionEnd, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_ime_event(7i32, targetNodeId, -1i32, -1i32, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_word(") == std::string::npos);
  CHECK(imeBody.find("self.append_string(") == std::string::npos);

  const std::string viewBody = source.substr(resizeStart, eventCountStart - resizeStart);
  CHECK(viewBody.find("self.append_view_event(8i32, targetNodeId, width, height)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(9i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(10i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_word(") == std::string::npos);
}

TEST_CASE("spinning cube metal host missing metallib diagnostics stay stable") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metallib diagnostic regression");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_missing_metallib";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";
  const std::filesystem::path missingMetallibPath = outDir / "missing.metallib";

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(missingMetallibPath.string()) + " > " +
      quoteShellArg(hostStdoutPath.string()) + " 2> " + quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 71);

  const std::string hostStdout = readFile(hostStdoutPath.string());
  const std::string hostStderr = readFile(hostStderrPath.string());
  CHECK(hostStdout.empty());
  CHECK(hostStderr.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStderr.find("gfx_error_code=pipeline_create_failed") != std::string::npos);
  CHECK(hostStderr.find("gfx_error_why=failed to load metallib:") != std::string::npos);
}

TEST_CASE("spinning cube metal host pipeline creation regression stays fixed") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metal pipeline creation regression");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; cannot run metal pipeline creation regression");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; cannot run metal pipeline creation regression");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_pipeline_regression";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path airPath = outDir / "cube.air";
  const std::filesystem::path metallibPath = outDir / "cube.metallib";
  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                      " -o " + quoteShellArg(airPath.string());
  CHECK(runCommand(compileMetalCmd) == 0);
  CHECK(std::filesystem::exists(airPath));

  const std::string compileLibraryCmd =
      "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
  CHECK(runCommand(compileLibraryCmd) == 0);
  CHECK(std::filesystem::exists(metallibPath));

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd = quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(metallibPath.string()) +
                                 " > " + quoteShellArg(hostStdoutPath.string()) + " 2> " +
                                 quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);

  const std::string hostStdout = readFile(hostStdoutPath.string());
  const std::string hostStderr = readFile(hostStderrPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
  CHECK(hostStderr.find("failed to create render pipeline:") == std::string::npos);
  CHECK(hostStderr.find("Vertex function has input attributes but no vertex descriptor was set.") == std::string::npos);
}

TEST_CASE("spinning cube metal full-path smoke renders frame") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run spinning cube metal full-path smoke");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; skipping spinning cube metal full-path smoke");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; skipping spinning cube metal full-path smoke");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_host_runtime";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path airPath = outDir / "cube.air";
  const std::filesystem::path metallibPath = outDir / "cube.metallib";
  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                      " -o " + quoteShellArg(airPath.string());
  CHECK(runCommand(compileMetalCmd) == 0);
  CHECK(std::filesystem::exists(airPath));

  const std::string compileLibraryCmd =
      "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
  CHECK(runCommand(compileLibraryCmd) == 0);
  CHECK(std::filesystem::exists(metallibPath));

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd = quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(metallibPath.string()) +
                                 " > " + quoteShellArg(hostStdoutPath.string()) + " 2> " +
                                 quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  const std::string hostStdout = readFile(hostStdoutPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
}

TEST_CASE("spinning cube metal software surface bridge smoke") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metal software surface bridge smoke");
    return;
  }

  const std::filesystem::path outDir =
      std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_software_surface";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " --software-surface-demo > " +
      quoteShellArg(hostStdoutPath.string()) + " 2> " + quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  CHECK(readFile(hostStderrPath.string()).empty());
  const std::string hostStdout = readFile(hostStdoutPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("software_surface_bridge=1") != std::string::npos);
  CHECK(hostStdout.find("software_surface_width=64") != std::string::npos);
  CHECK(hostStdout.find("software_surface_height=64") != std::string::npos);
  CHECK(hostStdout.find("software_surface_presented=1") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
}

TEST_CASE("borrow checker negative examples fail with expected diagnostics") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples" / "borrow_checker_negative";
  REQUIRE(std::filesystem::exists(examplesDir));

  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".prime") {
      continue;
    }
    exampleFiles.push_back(entry.path());
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());

  for (const auto &path : exampleFiles) {
    std::filesystem::path expectedPath = path;
    expectedPath.replace_extension(".expected.txt");
    REQUIRE(std::filesystem::exists(expectedPath));

    const std::string expectedContents = readFile(expectedPath.string());
    std::istringstream expectedLines(expectedContents);
    std::vector<std::string> expectedFragments;
    std::string line;
    while (std::getline(expectedLines, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      expectedFragments.push_back(line);
    }
    REQUIRE(!expectedFragments.empty());

    const std::string primecErrPath =
        (std::filesystem::temp_directory_path() / ("primec_borrow_checker_negative_" + path.stem().string() + ".json"))
            .string();
    const std::string primevmErrPath = (std::filesystem::temp_directory_path() /
                                        ("primevm_borrow_checker_negative_" + path.stem().string() + ".json"))
                                           .string();

    const std::string primecCmd = "./primec --emit=exe " + quoteShellArg(path.string()) +
                                  " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(primecErrPath);
    CHECK(runCommand(primecCmd) == 2);
    const std::string primecDiagnostics = readFile(primecErrPath);
    CHECK(primecDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primecDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primecDiagnostics.find("Semantic error: ") == std::string::npos);

    const std::string primevmCmd =
        "./primevm " + quoteShellArg(path.string()) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(primevmErrPath);
    CHECK(runCommand(primevmCmd) == 2);
    const std::string primevmDiagnostics = readFile(primevmErrPath);
    CHECK(primevmDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primevmDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primevmDiagnostics.find("Semantic error: ") == std::string::npos);
  }
}

TEST_SUITE_END();
