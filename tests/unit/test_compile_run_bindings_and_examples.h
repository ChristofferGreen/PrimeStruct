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
    exampleFiles.push_back(path);
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());

  for (const auto &path : exampleFiles) {
    const std::string compileCmd =
        "./primec --emit=ir " + quoteShellArg(path.string()) + " --out-dir " + quoteShellArg(outDir.string()) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(outDir / (path.stem().string() + ".psir")));
  }
}

TEST_CASE("spinning cube shared source compiles across profile targets") {
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_native_smoke").string();
  const std::string wasmPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_browser_smoke.wasm").string();
  const std::string metalErrPath =
      (std::filesystem::temp_directory_path() / "primec_spinning_cube_metal_smoke.err.txt").string();

  const std::string nativeCmd = "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
                                quoteShellArg(nativePath) + " --entry /main";
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

TEST_CASE("spinning cube browser host assets pass pipeline smoke checks") {
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
  const std::string shaderText = readFile(shaderPath.string());
  CHECK(shaderText.find("@vertex") != std::string::npos);
  CHECK(shaderText.find("@fragment") != std::string::npos);

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
      " --entry /main";
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

TEST_CASE("spinning cube fixed-step snapshots stay deterministic") {
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
      " --entry /main";
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
        " --entry /main";
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
  CHECK(dom.find("Host running") != std::string::npos || dom.find("Wasm load skipped") != std::string::npos);
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
      "./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native --entry /main",
      "c++ -std=c++17 examples/native/spinning_cube/main.cpp -o /tmp/spinning_cube_host",
      "xcrun metal -std=metal3.0 -c examples/metal/spinning_cube/cube.metal -o /tmp/cube.air",
      "xcrun metallib /tmp/cube.air -o /tmp/cube.metallib",
      "xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework "
      "Metal -o /tmp/metal_host",
      "/tmp/metal_host /tmp/cube.metallib",
      "Diagnostics: prints `native host verified cube simulation output`.",
      "Diagnostics: prints `frame_rendered=1`.",
      "FPS/diagnostic overlay: status text under the canvas is the current",
  };
  for (const std::string &snippet : requiredCommandSnippets) {
    CAPTURE(snippet);
    CHECK(readme.find(snippet) != std::string::npos);
  }

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
  const std::filesystem::path metalShaderPath = rootDir / "examples" / "metal" / "spinning_cube" / "cube.metal";
  const std::filesystem::path metalHostPath = rootDir / "examples" / "metal" / "spinning_cube" / "metal_host.mm";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(nativeHostPath));
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
      " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));

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
