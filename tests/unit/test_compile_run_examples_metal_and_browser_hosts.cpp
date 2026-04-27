#include "test_compile_run_helpers.h"

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
  CHECK(source.find("#include \"../../shared/gfx_contract_shared.h\"") != std::string::npos);
  CHECK(source.find("#include \"../../shared/metal_offscreen_host.h\"") != std::string::npos);
  CHECK(source.find("#include \"../../shared/spinning_cube_simulation_reference.h\"") != std::string::npos);
  CHECK(source.find("metal-osx") != std::string::npos);
  CHECK(source.find("using GfxErrorCode = primestruct::gfx_contract::GfxErrorCode;") != std::string::npos);
  CHECK(source.find("using RenderCallbacks = primestruct::metal_offscreen_host::RenderCallbacks;") !=
        std::string::npos);
  CHECK(source.find("using RenderConfig = primestruct::metal_offscreen_host::RenderConfig;") !=
        std::string::npos);
  CHECK(source.find("using Vertex = primestruct::gfx_contract::VertexColoredHost;") != std::string::npos);
  CHECK(source.find("struct Vertex {") == std::string::npos);
  CHECK(source.find("struct CubeSimulationState {") == std::string::npos);
  CHECK(source.find("struct CubeStepResult {") == std::string::npos);
  CHECK(source.find("const char *gfxErrorCodeName(") == std::string::npos);
  CHECK(source.find("configureTrianglePipeline") != std::string::npos);
  CHECK(source.find("prepareTriangleResources") != std::string::npos);
  CHECK(source.find("encodeTriangleFrame") != std::string::npos);
  CHECK(source.find("primestruct::spinning_cube_simulation::snapshotCodeForTicks") != std::string::npos);
  CHECK(source.find("primestruct::spinning_cube_simulation::parityOk") != std::string::npos);
  CHECK(source.find("primestruct::metal_offscreen_host::runLibraryRender") != std::string::npos);
  CHECK(source.find("cubeRunFixedTicks(") == std::string::npos);
  CHECK(source.find("cubeRunFixedTicksChunked(") == std::string::npos);
  CHECK(source.find("cubeAdvanceFixed(") == std::string::npos);
  CHECK(source.find("cubeTick(") == std::string::npos);
  CHECK(source.find("newLibraryWithURL") == std::string::npos);
  CHECK(source.find("newCommandQueue") == std::string::npos);
  CHECK(source.find("renderCommandEncoderWithDescriptor") == std::string::npos);
  CHECK(source.find("MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[0].format = MTLVertexFormatFloat4;") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[0].offset = offsetof(Vertex, px);") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[1].format = MTLVertexFormatFloat4;") != std::string::npos);
  CHECK(source.find("vertexDesc.attributes[1].offset = offsetof(Vertex, r);") != std::string::npos);
  CHECK(source.find("vertexDesc.layouts[0].stride = sizeof(Vertex);") != std::string::npos);
  CHECK(source.find("pipelineDesc.vertexDescriptor = vertexDesc;") != std::string::npos);
  CHECK(source.find("state.vertexBuffer =") != std::string::npos);
  CHECK(source.find("setRenderPipelineState:pipeline") != std::string::npos);
  CHECK(source.find("setVertexBuffer:state.vertexBuffer offset:0 atIndex:0") != std::string::npos);
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
  CHECK(source.find("#include \"../../shared/gfx_contract_shared.h\"") != std::string::npos);
  CHECK(source.find("#include \"../../shared/metal_offscreen_host.h\"") != std::string::npos);
  CHECK(source.find("--software-surface-demo") != std::string::npos);
  CHECK(source.find("SoftwareSurfaceDemoConfig config;") != std::string::npos);
  CHECK(source.find("primestruct::metal_offscreen_host::runSoftwareSurfaceDemo(config)") != std::string::npos);
  CHECK(source.find("uploadSoftwareSurfaceFrame(") == std::string::npos);
  CHECK(source.find("renderSoftwareSurfaceDemo()") == std::string::npos);
}

TEST_CASE("shared metal offscreen host helper stays source locked") {
  std::filesystem::path sharedDir = std::filesystem::path("..") / "examples" / "shared";
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "shared";
  }
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path sharedHeaderPath = sharedDir / "metal_offscreen_host.h";
  REQUIRE(std::filesystem::exists(sharedHeaderPath));

  const std::string source = readFile(sharedHeaderPath.string());
  CHECK(source.find("namespace primestruct::metal_offscreen_host {") != std::string::npos);
  CHECK(source.find("struct SoftwareSurfaceDemoConfig {") != std::string::npos);
  CHECK(source.find("struct RenderConfig {") != std::string::npos);
  CHECK(source.find("struct RenderCallbacks {") != std::string::npos);
  CHECK(source.find("gfx_error_code=") != std::string::npos);
  CHECK(source.find("gfx_error_why=") != std::string::npos);
  CHECK(source.find("uploadSoftwareSurfaceFrame(") != std::string::npos);
  CHECK(source.find("runSoftwareSurfaceDemo") != std::string::npos);
  CHECK(source.find("runLibraryRender") != std::string::npos);
  CHECK(source.find("newLibraryWithURL") != std::string::npos);
  CHECK(source.find("newCommandQueue") != std::string::npos);
  CHECK(source.find("renderCommandEncoderWithDescriptor") != std::string::npos);
  CHECK(source.find("software_surface_bridge=1") != std::string::npos);
  CHECK(source.find("software_surface_presented=1") != std::string::npos);
  CHECK(source.find("frame_rendered=1") != std::string::npos);
}

TEST_CASE("shared spinning cube simulation reference helper stays source locked") {
  std::filesystem::path sharedDir = std::filesystem::path("..") / "examples" / "shared";
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "shared";
  }
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path helperPath = sharedDir / "spinning_cube_simulation_reference.h";
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string source = readFile(helperPath.string());
  CHECK(source.find("namespace primestruct::spinning_cube_simulation {") != std::string::npos);
  CHECK(source.find("struct State {") != std::string::npos);
  CHECK(source.find("struct StepResult {") != std::string::npos);
  CHECK(source.find("inline State runFixedTicks(int ticks)") != std::string::npos);
  CHECK(source.find("inline State runFixedTicksChunked(int ticks)") != std::string::npos);
  CHECK(source.find("inline int snapshotCodeForTicks(int ticks)") != std::string::npos);
  CHECK(source.find("inline bool parityOk(int ticks)") != std::string::npos);
}

TEST_CASE("shared browser runtime helper stays source locked") {
  std::filesystem::path sharedDir = std::filesystem::path("..") / "examples" / "web" / "shared";
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path helperPath = sharedDir / "browser_runtime_shared.js";
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string source = readFile(helperPath.string());
  CHECK(source.find("export async function launchBrowserRuntime") != std::string::npos);
  CHECK(source.find("drawWireCubeProxy") != std::string::npos);
  CHECK(source.find("WebAssembly.instantiateStreaming") != std::string::npos);
  CHECK(source.find("requestAnimationFrame") != std::string::npos);
  CHECK(source.find("Host running with cube.wasm and cube.wgsl bootstrap.") != std::string::npos);
}

TEST_CASE("browser spinning cube launcher wrapper stays thin over shared helper") {
  std::filesystem::path scriptPath = std::filesystem::path("..") / "scripts" / "run_browser_spinning_cube.sh";
  std::filesystem::path helperPath = std::filesystem::path("..") / "scripts" / "run_canonical_browser_sample.sh";
  std::filesystem::path demoScriptPath = std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_browser_spinning_cube.sh";
  }
  if (!std::filesystem::exists(helperPath)) {
    helperPath = std::filesystem::current_path() / "scripts" / "run_canonical_browser_sample.sh";
  }
  if (!std::filesystem::exists(demoScriptPath)) {
    demoScriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));
  REQUIRE(std::filesystem::exists(helperPath));
  REQUIRE(std::filesystem::exists(demoScriptPath));

  const std::string scriptSource = readFile(scriptPath.string());
  const std::string helperSource = readFile(helperPath.string());
  const std::string demoSource = readFile(demoScriptPath.string());

  CHECK(scriptSource.find("exec bash \"$ROOT_DIR/scripts/run_canonical_browser_sample.sh\"") != std::string::npos);
  CHECK(scriptSource.find("--sample-dir \"$ROOT_DIR/examples/web/spinning_cube\"") != std::string::npos);
  CHECK(scriptSource.find("--shared-runtime \"$ROOT_DIR/examples/web/shared/browser_runtime_shared.js\"") !=
        std::string::npos);
  CHECK(helperSource.find("Compiling browser wasm") != std::string::npos);
  CHECK(helperSource.find("Staging browser assets") != std::string::npos);
  CHECK(helperSource.find("SMOKE: SKIP headless browser unavailable") != std::string::npos);
  CHECK(helperSource.find("PASS: wasm bootstrap status verified") != std::string::npos);
  CHECK(helperSource.find("Serving ${INDEX_URL}") != std::string::npos);
  const size_t headlessCheckPos = helperSource.find("if (( HEADLESS_SMOKE == 1 )); then");
  const size_t compileWasmPos = helperSource.find("echo \"${LAUNCHER_PREFIX} Compiling browser wasm\"");
  REQUIRE(headlessCheckPos != std::string::npos);
  REQUIRE(compileWasmPos != std::string::npos);
  CHECK(headlessCheckPos < compileWasmPos);
  CHECK(demoSource.find("run_browser_spinning_cube.sh") != std::string::npos);
  CHECK(demoSource.find("browser launcher execution failed") != std::string::npos);
  CHECK(demoSource.find("find_browser()") == std::string::npos);
  CHECK(demoSource.find("python3 -m http.server") == std::string::npos);
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

TEST_CASE("metal spinning cube launcher wrapper stays thin over shared helper") {
  std::filesystem::path scriptPath = std::filesystem::path("..") / "scripts" / "run_metal_spinning_cube.sh";
  std::filesystem::path helperPath = std::filesystem::path("..") / "scripts" / "run_canonical_metal_host.sh";
  std::filesystem::path demoScriptPath = std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_metal_spinning_cube.sh";
  }
  if (!std::filesystem::exists(helperPath)) {
    helperPath = std::filesystem::current_path() / "scripts" / "run_canonical_metal_host.sh";
  }
  if (!std::filesystem::exists(demoScriptPath)) {
    demoScriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));
  REQUIRE(std::filesystem::exists(helperPath));
  REQUIRE(std::filesystem::exists(demoScriptPath));

  const std::string scriptSource = readFile(scriptPath.string());
  const std::string helperSource = readFile(helperPath.string());
  const std::string demoSource = readFile(demoScriptPath.string());

  CHECK(scriptSource.find("exec bash \"$ROOT_DIR/scripts/run_canonical_metal_host.sh\"") != std::string::npos);
  CHECK(scriptSource.find("--shader-source \"$ROOT_DIR/examples/metal/spinning_cube/cube.metal\"") !=
        std::string::npos);
  CHECK(scriptSource.find("--host-source \"$ROOT_DIR/examples/metal/spinning_cube/metal_host.mm\"") !=
        std::string::npos);
  CHECK(helperSource.find("Compiling Metal shader") != std::string::npos);
  CHECK(helperSource.find("Linking metallib") != std::string::npos);
  CHECK(helperSource.find("Launching ${HOST_DESCRIPTION}") != std::string::npos);
  CHECK(helperSource.find("frame_rendered=1") != std::string::npos);
  CHECK(helperSource.find("software_surface_presented=1") != std::string::npos);
  CHECK(helperSource.find("snapshot_code=") != std::string::npos);
  CHECK(helperSource.find("parity_ok=1") != std::string::npos);
  CHECK(demoSource.find("run_metal_spinning_cube.sh") != std::string::npos);
  CHECK(demoSource.find("metal launcher execution failed") != std::string::npos);
  CHECK(demoSource.find("metal shader compile failed") == std::string::npos);
  CHECK(demoSource.find("metallib link failed") == std::string::npos);
  CHECK(demoSource.find("metal host compile failed") == std::string::npos);
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

  CHECK(guidelines.find("VertexColored{[position] Vec4{-1.0, -1.0, -1.0, 1.0},") != std::string::npos);
  CHECK(guidelines.find("VertexColored{[position] Vec4{ 1.0,  1.0,  1.0, 1.0},") != std::string::npos);
  CHECK(guidelines.find("[vertex_type] VertexColored,") != std::string::npos);
  CHECK(cubeSource.find("positionStrideBytes{16i32}") != std::string::npos);
  CHECK(cubeSource.find("colorStrideBytes{16i32}") != std::string::npos);
}


TEST_SUITE_END();
