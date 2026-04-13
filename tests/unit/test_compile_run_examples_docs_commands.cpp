#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

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
      "python3 -m http.server 8080 --bind 127.0.0.1 --directory examples/web",
      "Open `http://127.0.0.1:8080/spinning_cube/index.html`.",
      "### Browser Launcher",
      "./scripts/run_browser_spinning_cube.sh --primec ./build-debug/primec",
      "./scripts/run_browser_spinning_cube.sh --primec ./build-debug/primec --headless-smoke",
      "`scripts/run_canonical_browser_sample.sh`",
      "`examples/web/shared/browser_runtime_shared.js`",
      "./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native --entry /mainNative",
      "c++ -std=c++17 examples/native/spinning_cube/main.cpp -o /tmp/spinning_cube_host",
      "./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_stdlib_gfx_stream --entry /cubeStdGfxEmitFrameStream",
      "xcrun clang++ -std=c++17 -fobjc-arc examples/native/spinning_cube/window_host.mm -framework Foundation -framework "
      "AppKit -framework QuartzCore -framework Metal -o /tmp/spinning_cube_window_host",
      "/tmp/spinning_cube_window_host --gfx /tmp/cube_stdlib_gfx_stream --max-frames 120",
      "/tmp/spinning_cube_window_host --software-surface-demo --max-frames 1",
      "uploads a deterministic BGRA8 software color buffer into a",
      "shared Metal texture and blits it into the window drawable.",
      "### Native Window Launcher (macOS)",
      "./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec",
      "./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec --visual-smoke",
      "`scripts/run_canonical_gfx_native_window.sh`",
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
      "./scripts/run_metal_spinning_cube.sh",
      "./scripts/run_metal_spinning_cube.sh --software-surface-demo",
      "./scripts/run_metal_spinning_cube.sh --snapshot-code 120",
      "./scripts/run_metal_spinning_cube.sh --parity-check 120",
      "`scripts/run_canonical_metal_host.sh`",
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
      "`cubeStdGfxEmitFrameStream`",
      "Rendering: submits indexed cube draw calls each frame with transform",
      "uniforms updated from the deterministic simulation tail.",
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
      "Canonical /std/gfx/* stream path:",
      "feeds the native window host with deterministic window/swapchain/pass",
      "`gfx_stream_loaded=1`,",
      "`gfx_window_width=1280`,",
      "`gfx_submit_present_mask=3`, `startup_success=1`,",
      "The launcher compiles `cubeStdGfxEmitFrameStream` and runs the host through",
      "`--gfx`, so scripted smoke no longer depends on the old `--cube-sim` mode.",
      "For a visible rotating window today, use the browser path",
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
  const std::filesystem::path sharedRuntimePath =
      rootDir / "examples" / "web" / "shared" / "browser_runtime_shared.js";
  const std::filesystem::path nativeHostPath = rootDir / "examples" / "native" / "spinning_cube" / "main.cpp";
  const std::filesystem::path nativeWindowHostPath = rootDir / "examples" / "native" / "spinning_cube" / "window_host.mm";
  const std::filesystem::path metalShaderPath = rootDir / "examples" / "metal" / "spinning_cube" / "cube.metal";
  const std::filesystem::path metalHostPath = rootDir / "examples" / "metal" / "spinning_cube" / "metal_host.mm";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));
  REQUIRE(std::filesystem::exists(nativeHostPath));
  REQUIRE(std::filesystem::exists(nativeWindowHostPath));
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_doc_command_smoke";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path browserDir = outDir / "browser";
  const std::filesystem::path browserSampleDir = browserDir / "spinning_cube";
  const std::filesystem::path browserSharedDir = browserDir / "shared";
  std::filesystem::create_directories(browserSampleDir, ec);
  REQUIRE(!ec);
  ec.clear();
  std::filesystem::create_directories(browserSharedDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = browserSampleDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  std::filesystem::copy_file(indexPath, browserSampleDir / "index.html", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, browserSampleDir / "main.js", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, browserSampleDir / "cube.wgsl", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(sharedRuntimePath, browserSharedDir / "browser_runtime_shared.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
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
    const std::string curlCmd = "curl -fsS http://127.0.0.1:" + std::to_string(port) + "/spinning_cube/index.html > " +
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

  const std::filesystem::path gfxStreamPath = outDir / "cube_stdlib_gfx_stream";
  const std::string compileGfxStreamCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(gfxStreamPath.string()) + " --entry /cubeStdGfxEmitFrameStream";
  CHECK(runCommand(compileGfxStreamCmd) == 0);
  CHECK(std::filesystem::exists(gfxStreamPath));

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

    const std::filesystem::path gfxHostOutPath = outDir / "window_host.gfx_smoke.out.txt";
    const std::filesystem::path gfxHostErrPath = outDir / "window_host.gfx_smoke.err.txt";
    const std::string gfxHostCmd =
        quoteShellArg(nativeWindowHostBinPath.string()) + " --gfx " +
        quoteShellArg(gfxStreamPath.string()) +
        " --simulation-smoke > " + quoteShellArg(gfxHostOutPath.string()) + " 2> " +
        quoteShellArg(gfxHostErrPath.string());
    CHECK(runCommand(gfxHostCmd) == 0);
    CHECK(readFile(gfxHostErrPath.string()).empty());
    CHECK(readFile(gfxHostOutPath.string()).find("gfx_stream_loaded=1") != std::string::npos);

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


TEST_SUITE_END();
