#include "../test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube native flat frame entrypoints compile and run deterministically") {
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_native_flat_frame_entrypoints";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string cubeSource = readFile(cubePath.string());
  const std::string aggregateSource = cubeSource + R"(

[return<int>]
cubeNativeFlatFrameDeterminismSmoke() {
  if(not_equal(cubeNativeMeshVertexCount(), 8i32)) {
    return(11i32)
  } else {
  }
  if(not_equal(cubeNativeMeshIndexCount(), 36i32)) {
    return(12i32)
  } else {
  }
  if(not_equal(cubeNativeFrameInitTick(), 0i32)) {
    return(13i32)
  } else {
  }
  if(not_equal(cubeNativeFrameInitAngleMilli(), 0i32)) {
    return(14i32)
  } else {
  }
  if(not_equal(cubeNativeFrameInitAxisXCenti(), 100i32)) {
    return(15i32)
  } else {
  }
  if(not_equal(cubeNativeFrameInitAxisYCenti(), 0i32)) {
    return(16i32)
  } else {
  }
  if(not_equal(cubeNativeFrameAngularVelocityMilli(), 1047i32)) {
    return(17i32)
  } else {
  }
  if(not_equal(cubeNativeFrameStepSnapshotCode(), 117i32)) {
    return(18i32)
  } else {
  }
  return(0i32)
}
)";

  const std::string aggregateSrcPath = writeTemp("spinning_cube_native_flat_frame_entrypoints.prime", aggregateSource);
  const std::filesystem::path aggregatePath = outDir / "native_flat_frame_aggregate";
  const std::string compileAggregateCmd =
      "./primec --emit=native " + quoteShellArg(aggregateSrcPath) + " -o " + quoteShellArg(aggregatePath.string()) +
      " --entry /cubeNativeFlatFrameDeterminismSmoke";
  CHECK(runCommand(compileAggregateCmd) == 0);
  CHECK(std::filesystem::exists(aggregatePath));
  CHECK(runCommand(quoteShellArg(aggregatePath.string())) == 0);

  const std::filesystem::path snapshotPath = outDir / "native_frame_snapshot";
  const std::string compileSnapshotCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(snapshotPath.string()) +
      " --entry /cubeNativeFrameStepSnapshotCode";
  CHECK(runCommand(compileSnapshotCmd) == 0);
  CHECK(std::filesystem::exists(snapshotPath));
  CHECK(runCommand(quoteShellArg(snapshotPath.string())) == 117);
}

TEST_CASE("spinning cube stdlib gfx frame stream entry stays deterministic") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube stdlib gfx frame stream checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path cubePath =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  REQUIRE(std::filesystem::exists(cubePath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_stdlib_gfx_frame_stream";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path streamBinaryPath = outDir / "cube_stdlib_gfx_frame_stream";
  const std::string compileCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(streamBinaryPath.string()) +
      " --entry /cubeStdGfxEmitFrameStream";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(streamBinaryPath));

  const std::filesystem::path streamOutPath = outDir / "cube_stdlib_gfx_frame_stream.out.txt";
  const std::string runCmd =
      quoteShellArg(streamBinaryPath.string()) + " > " + quoteShellArg(streamOutPath.string());
  CHECK(runCommand(runCmd) == 0);

  const std::string streamOutput = readFile(streamOutPath.string());
  CHECK(streamOutput.rfind("17001\n1\n1280\n720\n0\n0\n0\n50\n70\n100\n1000\n1000\n8\n36\n1\n2\n3\n4\n46\n7\n8\n5\n6\n60\n7\n3\n0\n0\n100\n0\n1\n16\n99\n1\n", 0) == 0);
  CHECK(streamOutput.find("\n2\n32\n98\n2\n") != std::string::npos);
}

TEST_CASE("spinning cube browser host assets pass pipeline smoke checks") {
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path sharedDir =
      std::filesystem::path("..") / "examples" / "web" / "shared";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  REQUIRE(std::filesystem::exists(sampleDir));
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  const std::filesystem::path indexPath = sampleDir / "index.html";
  const std::filesystem::path mainJsPath = sampleDir / "main.js";
  const std::filesystem::path shaderPath = sampleDir / "cube.wgsl";
  const std::filesystem::path sharedRuntimePath = sharedDir / "browser_runtime_shared.js";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(shaderPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));

  const std::filesystem::path pipelineDir =
      testScratchPath("") / "primec_spinning_cube_browser_assets";
  const std::filesystem::path stagedSampleDir = pipelineDir / "spinning_cube";
  const std::filesystem::path stagedSharedDir = pipelineDir / "shared";
  std::error_code ec;
  std::filesystem::remove_all(pipelineDir, ec);
  std::filesystem::create_directories(stagedSampleDir, ec);
  REQUIRE(!ec);
  ec.clear();
  std::filesystem::create_directories(stagedSharedDir, ec);
  REQUIRE(!ec);

  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube browser asset compile checks until array-return lowering support lands");
    CHECK(true);
    return;
  }

  const std::filesystem::path wasmPath = stagedSampleDir / "cube.wasm";
  const std::string wasmBrowserCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(wasmBrowserCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  std::filesystem::copy_file(indexPath, stagedSampleDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, stagedSampleDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(shaderPath, stagedSampleDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(sharedRuntimePath, stagedSharedDir / "browser_runtime_shared.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  CHECK(std::filesystem::exists(stagedSampleDir / "index.html"));
  CHECK(std::filesystem::exists(stagedSampleDir / "main.js"));
  CHECK(std::filesystem::exists(stagedSampleDir / "cube.wgsl"));
  CHECK(std::filesystem::exists(stagedSampleDir / "cube.wasm"));
  CHECK(std::filesystem::exists(stagedSharedDir / "browser_runtime_shared.js"));
}

TEST_CASE("spinning cube browser host assets stay source locked") {
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path sharedDir =
      std::filesystem::path("..") / "examples" / "web" / "shared";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  REQUIRE(std::filesystem::exists(sampleDir));
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  const std::filesystem::path indexPath = sampleDir / "index.html";
  const std::filesystem::path mainJsPath = sampleDir / "main.js";
  const std::filesystem::path shaderPath = sampleDir / "cube.wgsl";
  const std::filesystem::path sharedRuntimePath = sharedDir / "browser_runtime_shared.js";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(shaderPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));

  const std::string indexHtml = readFile(indexPath.string());
  CHECK(indexHtml.find("id=\"cube-canvas\"") != std::string::npos);
  CHECK(indexHtml.find("src=\"./main.js\"") != std::string::npos);

  const std::string mainJs = readFile(mainJsPath.string());
  CHECK(mainJs.find("../shared/browser_runtime_shared.js") != std::string::npos);
  CHECK(mainJs.find("new URL(\"./cube.wasm\", import.meta.url)") != std::string::npos);
  CHECK(mainJs.find("new URL(\"./cube.wgsl\", import.meta.url)") != std::string::npos);
  CHECK(mainJs.find("launchBrowserRuntime") != std::string::npos);
  CHECK(mainJs.find("requestAnimationFrame") == std::string::npos);
  const std::string sharedRuntime = readFile(sharedRuntimePath.string());
  CHECK(sharedRuntime.find("export async function launchBrowserRuntime") != std::string::npos);
  CHECK(sharedRuntime.find("requestAnimationFrame") != std::string::npos);
  CHECK(sharedRuntime.find("WebAssembly.instantiateStreaming") != std::string::npos);
  CHECK(sharedRuntime.find("Host running with cube.wasm and cube.wgsl bootstrap.") != std::string::npos);
  const std::string cubeSource = readFile(cubePath.string());
  CHECK(cubeSource.find("positionStrideBytes{16i32}") != std::string::npos);
  CHECK(cubeSource.find("colorStrideBytes{16i32}") != std::string::npos);
  const std::string shaderText = readFile(shaderPath.string());
  CHECK(shaderText.find("@vertex") != std::string::npos);
  CHECK(shaderText.find("@fragment") != std::string::npos);
  CHECK(shaderText.find("@location(0) position : vec4<f32>") != std::string::npos);
  CHECK(shaderText.find("@location(1) color : vec4<f32>") != std::string::npos);
  CHECK(shaderText.find("out.clipPosition = in.position;") != std::string::npos);

  if (runCommand("node --version > /dev/null 2>&1") == 0) {
    const std::string nodeCheckSource = "node --check " + quoteShellArg(mainJsPath.string()) + " > /dev/null 2>&1";
    CHECK(runCommand(nodeCheckSource) == 0);
    const std::string nodeCheckHelperSource =
        "node --check " + quoteShellArg(sharedRuntimePath.string()) + " > /dev/null 2>&1";
    CHECK(runCommand(nodeCheckHelperSource) == 0);
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
        testScratchPath("") / "primec_spinning_cube_browser_profile_ok.wasm";
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
        (testScratchPath("") / "primec_spinning_cube_browser_profile_reject_io.json").string();
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
        (testScratchPath("") / "primec_spinning_cube_browser_profile_reject_argv.json").string();
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
      testScratchPath("") / "primec_spinning_cube_native_target_sample";
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
      testScratchPath("") / "primec_spinning_cube_native_host";
  const std::string compileHostCmd = cxx + " -std=c++17 " + quoteShellArg(hostSourcePath.string()) + " -o " +
                                     quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::filesystem::path hostOutPath =
      testScratchPath("") / "primec_spinning_cube_native_host_runtime.out.txt";
  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(cubeNativePath.string()) + " > " +
      quoteShellArg(hostOutPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  CHECK(readFile(hostOutPath.string()).find("native host verified cube simulation output") != std::string::npos);
}

TEST_SUITE_END();
