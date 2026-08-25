#include "../test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

#include <fstream>

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

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
        (testScratchPath("") / "primec_spinning_cube_metal_profile_reject.err.txt").string();
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
        (testScratchPath("") / "primec_spinning_cube_metal_emit_reject.err.txt").string();
    const std::string cmd = "./primec --emit=metal " + quoteShellArg(srcPath) +
                            " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
  }

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("xcrun not available; skipping macOS metal/metallib shader compile smoke");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("xcrun metal not available; skipping macOS metal shader compile smoke");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("xcrun metallib not available; skipping macOS metallib link smoke");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_shader_path";
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

TEST_CASE("browser launcher skips smoke before wasm compile when python3 is unavailable") {
  std::filesystem::path scriptPath = std::filesystem::path("..") / "scripts" / "run_browser_spinning_cube.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_browser_spinning_cube.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_browser_launcher_python_skip";
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
    REQUIRE(fakePrimec.good());
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "echo \"unexpected primec invocation\" >&2\n";
    fakePrimec << "exit 99\n";
    REQUIRE(fakePrimec.good());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    REQUIRE(fakePython.good());
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
    REQUIRE(fakePython.good());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "launcher.out.txt";
  const std::filesystem::path errPath = outDir / "launcher.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg((outDir / "stage").string()) +
      " --port 18769 --headless-smoke > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[browser-launcher] SMOKE: SKIP python3 unavailable") != std::string::npos);
  CHECK(output.find("[browser-launcher] Compiling browser wasm") == std::string::npos);
  CHECK(output.find("[browser-launcher] Staging browser assets") == std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outDir / "stage" / "spinning_cube" / "cube.wasm"));
}

TEST_CASE("browser launcher compile run coverage validates shared helper path") {
  std::filesystem::path scriptPath = std::filesystem::path("..") / "scripts" / "run_browser_spinning_cube.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_browser_spinning_cube.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_browser_launcher_compile_run_coverage";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "launcher.out.txt";
  const std::filesystem::path errPath = outDir / "launcher.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec ./primec --out-dir " +
                              quoteShellArg(outDir.string()) + " --port 18769 --headless-smoke > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  const int code = runCommand(command);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  const bool browserCompileUnsupported =
      diagnostics.find("only supports returning array values") != std::string::npos ||
      diagnostics.find("graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*") !=
          std::string::npos;
  if (browserCompileUnsupported) {
    CHECK(code == 2);
    CHECK(output.find("[browser-launcher] Compiling browser wasm") != std::string::npos);
    CHECK(diagnostics.find("[browser-launcher] ERROR: failed to compile cube.wasm") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(outDir / "spinning_cube" / "cube.wasm"));
    return;
  }

  CHECK(code == 0);
  CHECK(diagnostics.empty());
  const bool reportedPass = output.find("PASS: wasm bootstrap status verified") != std::string::npos;
  const bool reportedSkipPython = output.find("SMOKE: SKIP python3 unavailable") != std::string::npos;
  const bool reportedSkipBrowser = output.find("SMOKE: SKIP headless browser unavailable") != std::string::npos;
  const bool reportedSkipHeadlessMode =
      output.find("SMOKE: SKIP browser headless mode unavailable") != std::string::npos;
  const bool reportedSkip = reportedSkipPython || reportedSkipBrowser || reportedSkipHeadlessMode;
  CHECK((reportedPass || reportedSkip));
  if (reportedPass) {
    CHECK(std::filesystem::exists(outDir / "spinning_cube" / "index.html"));
    CHECK(std::filesystem::exists(outDir / "spinning_cube" / "main.js"));
    CHECK(std::filesystem::exists(outDir / "spinning_cube" / "cube.wgsl"));
    CHECK(std::filesystem::exists(outDir / "spinning_cube" / "cube.wasm"));
    CHECK(std::filesystem::exists(outDir / "shared" / "browser_runtime_shared.js"));
    CHECK(output.find("[browser-launcher] Compiling browser wasm") != std::string::npos);
    CHECK(output.find("[browser-launcher] Staging browser assets") != std::string::npos);
  } else {
    CHECK(output.find("[browser-launcher] Compiling browser wasm") == std::string::npos);
    CHECK(output.find("[browser-launcher] Staging browser assets") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(outDir / "spinning_cube" / "cube.wasm"));
  }
}

TEST_CASE("metal launcher compile run coverage validates shared helper path") {
  std::filesystem::path scriptPath = std::filesystem::path("..") / "scripts" / "run_metal_spinning_cube.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_metal_spinning_cube.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; metal launcher coverage requires macOS tooling");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; metal launcher coverage requires Metal tooling");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; metal launcher coverage requires Metal tooling");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_launcher_compile_run_coverage";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path launcherOutPath = outDir / "launcher.out.txt";
  const std::filesystem::path launcherErrPath = outDir / "launcher.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --out-dir " + quoteShellArg(outDir.string()) + " > " +
      quoteShellArg(launcherOutPath.string()) + " 2> " + quoteShellArg(launcherErrPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(launcherOutPath.string());
  const std::string diagnostics = readFile(launcherErrPath.string());
  CHECK(diagnostics.find("[metal-launcher] ERROR:") == std::string::npos);
  CHECK(output.find("[metal-launcher] Compiling metal host") != std::string::npos);
  CHECK(output.find("[metal-launcher] Compiling Metal shader") != std::string::npos);
  CHECK(output.find("[metal-launcher] Linking metallib") != std::string::npos);
  CHECK(output.find("[metal-launcher] Launching metal host") != std::string::npos);
  CHECK(output.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(output.find("frame_rendered=1") != std::string::npos);
  CHECK(output.find("[metal-launcher] PASS: launch completed") != std::string::npos);

  CHECK(std::filesystem::exists(outDir / "cube.air"));
  CHECK(std::filesystem::exists(outDir / "cube.metallib"));
  CHECK(std::filesystem::exists(outDir / "spinning_cube_metal_host"));
  CHECK(std::filesystem::exists(outDir / "spinning_cube_metal_host.stdout.txt"));
  CHECK(std::filesystem::exists(outDir / "spinning_cube_metal_host.stderr.txt"));
}

TEST_SUITE_END();
