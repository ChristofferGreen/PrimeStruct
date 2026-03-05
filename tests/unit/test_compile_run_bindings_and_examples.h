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

TEST_CASE("spinning cube native host target builds and runs") {
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

  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(cubeNativePath.string());
  CHECK(runCommand(runHostCmd) == 0);
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

TEST_CASE("spinning cube metal host glue renders frame smoke") {
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
    INFO("xcrun not available; skipping macOS metal host runtime smoke");
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
