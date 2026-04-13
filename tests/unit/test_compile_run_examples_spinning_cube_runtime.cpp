#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

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
  std::filesystem::path sharedHelperPath =
      std::filesystem::path("..") / "examples" / "shared" / "native_metal_window_host.h";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(hostSourcePath));
  if (!std::filesystem::exists(sharedHelperPath)) {
    sharedHelperPath = std::filesystem::current_path() / "examples" / "shared" / "native_metal_window_host.h";
  }
  REQUIRE(std::filesystem::exists(sharedHelperPath));

  const std::string hostSource = readFile(hostSourcePath.string());
  const std::string sharedHelperSource = readFile(sharedHelperPath.string());
  CHECK(hostSource.find("#import <AppKit/AppKit.h>") != std::string::npos);
  CHECK(hostSource.find("#import <QuartzCore/CAMetalLayer.h>") != std::string::npos);
  CHECK(hostSource.find("#include \"../../shared/native_metal_window_host.h\"") != std::string::npos);
  CHECK(hostSource.find("--gfx") != std::string::npos);
  CHECK(hostSource.find("--simulation-smoke") != std::string::npos);
  CHECK(hostSource.find("--software-surface-demo") != std::string::npos);
  CHECK(hostSource.find("gfx_stream_loaded=1") != std::string::npos);
  CHECK(hostSource.find("gfx_window_width=") != std::string::npos);
  CHECK(hostSource.find("gfx_mesh_vertex_count=") != std::string::npos);
  CHECK(hostSource.find("gfx_submit_present_mask=") != std::string::npos);
  CHECK(hostSource.find("simulation_stream_loaded=1") != std::string::npos);
  CHECK(hostSource.find("simulation_fixed_step_millis=16") != std::string::npos);
  CHECK(hostSource.find("shader_library_ready=1") != std::string::npos);
  CHECK(hostSource.find("vertex_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("index_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("uniform_buffer_ready=1") != std::string::npos);
  CHECK(hostSource.find("simulation_tick=") != std::string::npos);
  CHECK(hostSource.find("pipeline_ready=1") != std::string::npos);
  CHECK(sharedHelperSource.find("gfx_profile=") != std::string::npos);
  CHECK(sharedHelperSource.find("startup_success=1") != std::string::npos);
  CHECK(sharedHelperSource.find("startup_failure=1") != std::string::npos);
  CHECK(sharedHelperSource.find("startup_failure_stage=") != std::string::npos);
  CHECK(sharedHelperSource.find("startup_failure_reason=") != std::string::npos);
  CHECK(sharedHelperSource.find("startup_failure_exit_code=") != std::string::npos);
  CHECK(sharedHelperSource.find("gfx_error_code=") != std::string::npos);
  CHECK(sharedHelperSource.find("gfx_error_why=") != std::string::npos);
  CHECK(sharedHelperSource.find("exit_reason=max_frames") != std::string::npos);
  CHECK(sharedHelperSource.find("exit_reason=window_close") != std::string::npos);
  CHECK(sharedHelperSource.find("exit_reason=esc_key") != std::string::npos);
  CHECK(sharedHelperSource.find("window_created=1") != std::string::npos);
  CHECK(sharedHelperSource.find("swapchain_layer_created=1") != std::string::npos);
  CHECK(sharedHelperSource.find("frame_rendered=1") != std::string::npos);

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("xcrun unavailable; skipping native window host compile smoke");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_native_window_host_compile";
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
            .find("usage: window_host (--gfx <path> | --software-surface-demo) [--max-frames <positive-int>] "
                  "[--simulation-smoke]") !=
        std::string::npos);

  const std::filesystem::path missingRequiredOutPath = outDir / "window_host.missing_required.out.txt";
  const std::filesystem::path missingRequiredErrPath = outDir / "window_host.missing_required.err.txt";
  const std::string missingRequiredCmd =
      quoteShellArg(hostBinaryPath.string()) + " --max-frames 1 > " + quoteShellArg(missingRequiredOutPath.string()) + " 2> " +
      quoteShellArg(missingRequiredErrPath.string());
  CHECK(runCommand(missingRequiredCmd) == 64);
  CHECK(readFile(missingRequiredOutPath.string()).empty());
  CHECK(readFile(missingRequiredErrPath.string())
            .find("missing required --gfx <path> or --software-surface-demo") !=
        std::string::npos);

  const std::filesystem::path missingGfxOutPath = outDir / "window_host.missing_gfx_value.out.txt";
  const std::filesystem::path missingGfxErrPath = outDir / "window_host.missing_gfx_value.err.txt";
  const std::string missingGfxCmd =
      quoteShellArg(hostBinaryPath.string()) + " --gfx > " +
      quoteShellArg(missingGfxOutPath.string()) + " 2> " + quoteShellArg(missingGfxErrPath.string());
  CHECK(runCommand(missingGfxCmd) == 64);
  CHECK(readFile(missingGfxOutPath.string()).empty());
  CHECK(readFile(missingGfxErrPath.string()).find("missing value for --gfx") != std::string::npos);

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
      quoteShellArg(hostBinaryPath.string()) + " --gfx /usr/bin/true --simulation-smoke > " +
      quoteShellArg(badStreamOutPath.string()) + " 2> " + quoteShellArg(badStreamErrPath.string());
  CHECK(runCommand(badStreamCmd) == 69);
  CHECK(readFile(badStreamOutPath.string()).empty());
  const std::string badStreamErr = readFile(badStreamErrPath.string());
  CHECK(badStreamErr.find("startup_failure=1") != std::string::npos);
  CHECK(badStreamErr.find("gfx_profile=native-desktop") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_stage=simulation_stream_load") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_reason=gfx_stream_load_failed") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_exit_code=69") != std::string::npos);
  CHECK(badStreamErr.find("startup_failure_detail=gfx stream was empty") != std::string::npos);

  const std::filesystem::path gfxStreamBinaryPath = outDir / "cube_stdlib_gfx_frame_stream";
  const std::string compileGfxStreamCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(gfxStreamBinaryPath.string()) + " --entry /cubeStdGfxEmitFrameStream";
  CHECK(runCommand(compileGfxStreamCmd) == 0);
  CHECK(std::filesystem::exists(gfxStreamBinaryPath));

  const std::filesystem::path gfxSmokeOutPath = outDir / "window_host.gfx_smoke.out.txt";
  const std::filesystem::path gfxSmokeErrPath = outDir / "window_host.gfx_smoke.err.txt";
  const std::string gfxSmokeCmd =
      quoteShellArg(hostBinaryPath.string()) + " --gfx " +
      quoteShellArg(gfxStreamBinaryPath.string()) +
      " --simulation-smoke > " + quoteShellArg(gfxSmokeOutPath.string()) + " 2> " +
      quoteShellArg(gfxSmokeErrPath.string());
  CHECK(runCommand(gfxSmokeCmd) == 0);
  CHECK(readFile(gfxSmokeErrPath.string()).empty());
  const std::string gfxSmokeOutput = readFile(gfxSmokeOutPath.string());
  CHECK(gfxSmokeOutput.find("gfx_stream_loaded=1") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_window_width=1280") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_window_height=720") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_mesh_vertex_count=8") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_mesh_index_count=36") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_submit_present_mask=3") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_frame_token=5") != std::string::npos);
  CHECK(gfxSmokeOutput.find("gfx_render_pass_token=6") != std::string::npos);
  CHECK(gfxSmokeOutput.find("simulation_stream_loaded=1") != std::string::npos);
  CHECK(gfxSmokeOutput.find("simulation_frames_loaded=600") != std::string::npos);
  CHECK(gfxSmokeOutput.find("simulation_smoke_tick=0") != std::string::npos);
  CHECK(gfxSmokeOutput.find("simulation_smoke_angle_milli=0") != std::string::npos);
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
      testScratchPath("") / "primec_spinning_cube_native_window_software_surface";
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
      testScratchPath("") / "primec_spinning_cube_fixed_step_snapshot_native";
  const std::string compileNativeSnapshotCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(nativeSnapshotPath.string()) + " --entry /cubeFixedStepSnapshot120";
  CHECK(runCommand(compileNativeSnapshotCmd) == 0);
  CHECK(std::filesystem::exists(nativeSnapshotPath));
  CHECK(runCommand(quoteShellArg(nativeSnapshotPath.string())) == GoldenSnapshot);

  const std::filesystem::path nativeSnapshotChunkedPath =
      testScratchPath("") / "primec_spinning_cube_fixed_step_snapshot_chunked_native";
  const std::string compileNativeSnapshotChunkedCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(nativeSnapshotChunkedPath.string()) + " --entry /cubeFixedStepSnapshot120Chunked";
  CHECK(runCommand(compileNativeSnapshotChunkedCmd) == 0);
  CHECK(std::filesystem::exists(nativeSnapshotChunkedPath));
  CHECK(runCommand(quoteShellArg(nativeSnapshotChunkedPath.string())) == GoldenSnapshot);

  const std::filesystem::path wasmSnapshotPath =
      testScratchPath("") / "primec_spinning_cube_fixed_step_snapshot.wasm";
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
      testScratchPath("") / "primec_spinning_cube_rotation_parity_native";
  const std::string compileNativeParityCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativeParityPath.string()) +
      " --entry /cubeRotationParity120";
  CHECK(runCommand(compileNativeParityCmd) == 0);
  CHECK(std::filesystem::exists(nativeParityPath));
  CHECK(runCommand(quoteShellArg(nativeParityPath.string())) == ExpectedParityPass);

  const std::filesystem::path wasmParityPath =
      testScratchPath("") / "primec_spinning_cube_rotation_parity.wasm";
  const std::string compileWasmParityCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmParityPath.string()) + " --entry /cubeRotationParity120";
  CHECK(runCommand(compileWasmParityCmd) == 0);
  CHECK(std::filesystem::exists(wasmParityPath));

  if (hasWasmtime()) {
    const std::filesystem::path wasmOutPath =
        testScratchPath("") / "primec_spinning_cube_rotation_parity_wasm.out.txt";
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
      testScratchPath("") / "primec_spinning_cube_rotation_parity_metal_host";
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


TEST_SUITE_END();
