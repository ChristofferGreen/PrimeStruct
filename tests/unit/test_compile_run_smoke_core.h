TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

static constexpr std::string_view IrResultOkUnsupportedMessage =
    "IR backends only support Result.ok with supported payload values";

static bool compileAcrossBackendsOrExpectUnsupported(const std::string &nameStem,
                                                     const std::string &compileExeCmd,
                                                     const std::string &exePath,
                                                     const std::string &runVmCmd,
                                                     const std::string &compileNativeCmd,
                                                     const std::string &nativePath,
                                                     const std::string &nativeMessage,
                                                     const std::string &vmMessage) {
  const std::filesystem::path tempRoot = testScratchPath("");
  const std::string exeErrPath = (tempRoot / (nameStem + "_exe_err.txt")).string();
  const std::string vmErrPath = (tempRoot / (nameStem + "_vm_err.txt")).string();
  const std::string nativeErrPath = (tempRoot / (nameStem + "_native_err.txt")).string();

  const int exeCode = runCommand(compileExeCmd + " 2> " + quoteShellArg(exeErrPath));
  if (exeCode == 0) {
    return true;
  }

  CHECK(exeCode == 2);
  CHECK_FALSE(std::filesystem::exists(exePath));
  CHECK(readFile(exeErrPath).find(nativeMessage) != std::string::npos);

  const int vmCode = runCommand(runVmCmd + " 2> " + quoteShellArg(vmErrPath));
  CHECK(vmCode == 2);
  CHECK(readFile(vmErrPath).find(vmMessage) != std::string::npos);

  const int nativeCode = runCommand(compileNativeCmd + " 2> " + quoteShellArg(nativeErrPath));
  CHECK(nativeCode == 2);
  CHECK_FALSE(std::filesystem::exists(nativePath));
  CHECK(readFile(nativeErrPath).find(nativeMessage) != std::string::npos);
  return false;
}

static bool compileWasmWasiOrExpectUnsupported(const std::string &srcPath,
                                               const std::string &wasmPath,
                                               const std::string &errPath) {
  const int code = compileWasmWasiProgram(srcPath, wasmPath, errPath);
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  const std::string error = readFile(errPath);
  CHECK((error.find("Wasm IR validation error") != std::string::npos ||
         error.find("unsupported opcode for wasm target") != std::string::npos ||
         error.find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
             std::string::npos));
  return false;
}

static bool runWasmCompileCommandOrExpectUnsupported(const std::string &wasmCmd,
                                                     const std::string &wasmPath,
                                                     const std::string &errPath) {
  const int code = runCommand(wasmCmd);
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  const std::string error = readFile(errPath);
  CHECK((error.find("Wasm IR validation error") != std::string::npos ||
         error.find("unsupported opcode for wasm target") != std::string::npos ||
         error.find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
             std::string::npos));
  return false;
}

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_simple_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs float arithmetic in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_float_vm.prime", source);

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs primitive brace constructors") {
  const std::string source = R"(
[return<bool>]
truthy() {
  return(bool{ 35i32 })
}

[return<int>]
main() {
  return(convert<int>(truthy()))
}
)";
  const std::string srcPath = writeTemp("compile_brace_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_brace_convert_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(runVmCmd) == 1);
}

TEST_CASE("default entry path is main") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_entry.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_default_entry_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primevm " + srcPath;
  CHECK(runCommand(runVmCmd) == 4);
}

TEST_CASE("enum value access lowers across backends") {
  const std::string source = R"(
[enum]
Colors() {
  assign(Blue, 5i32)
  Red
}

[return<int>]
main() {
  return(Colors.Blue.value)
}
)";
  const std::string srcPath = writeTemp("compile_enum_access.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_enum_access_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_enum_access_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("experimental gfx type surface imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Window] window{Window([token] 7i32, [width] 1280i32, [height] 720i32)}
  [ColorFormat] colorFormat{ColorFormat([value] 0i32)}
  [DepthFormat] depthFormat{DepthFormat([value] 0i32)}
  [PresentMode] presentMode{PresentMode([value] 0i32)}
  [ShaderLibrary] shader{ShaderLibrary([value] 0i32)}
  [Buffer<i32>] buffer{Buffer<i32>([token] 3i32, [elementCount] 4i32)}
  [Texture2D<i32>] texture{Texture2D<i32>([token] 5i32, [width] 64i32, [height] 32i32)}
  [Swapchain] swapchain{Swapchain([token] 11i32)}
  [Mesh] mesh{Mesh([token] 13i32, [vertexCount] 8i32, [indexCount] 36i32)}
  [VertexColored] vertex{
    VertexColored(
      [px] 1.0f32,
      [py] 2.0f32,
      [pz] 3.0f32,
      [pw] 1.0f32,
      [r] 0.25f32,
      [g] 0.50f32,
      [b] 0.75f32,
      [a] 1.0f32
    )
  }
  [GfxError] err{queueSubmitFailed()}
  [i32 mut] score{0i32}

  if(equal(colorFormat.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(depthFormat.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(presentMode.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(equal(shader.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  if(equal(window.width, 1280i32)) {
    score = plus(score, 1i32)
  } else {
    return(94i32)
  }
  if(equal(mesh.vertexCount, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(95i32)
  }
  if(equal(mesh.indexCount, 36i32)) {
    score = plus(score, 1i32)
  } else {
    return(96i32)
  }
  if(less_than(vertex.g, 0.60f32)) {
    score = plus(score, 1i32)
  } else {
    return(97i32)
  }
  if(equal(swapchain.presentMode.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(98i32)
  }
  if(equal(err.code, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(99i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_type_surface.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_gfx_experimental_type_surface_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_type_surface_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 10);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("canonical gfx type surface imports across backends") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  [Window] window{Window([token] 7i32, [width] 1280i32, [height] 720i32)}
  [ColorFormat] colorFormat{ColorFormat([value] 0i32)}
  [DepthFormat] depthFormat{DepthFormat([value] 0i32)}
  [PresentMode] presentMode{PresentMode([value] 0i32)}
  [ShaderLibrary] shader{ShaderLibrary([value] 0i32)}
  [Buffer<i32>] buffer{Buffer<i32>([token] 3i32, [elementCount] 4i32)}
  [Texture2D<i32>] texture{Texture2D<i32>([token] 5i32, [width] 64i32, [height] 32i32)}
  [Swapchain] swapchain{Swapchain([token] 11i32)}
  [Mesh] mesh{Mesh([token] 13i32, [vertexCount] 8i32, [indexCount] 36i32)}
  [VertexColored] vertex{
    VertexColored(
      [px] 1.0f32,
      [py] 2.0f32,
      [pz] 3.0f32,
      [pw] 1.0f32,
      [r] 0.25f32,
      [g] 0.50f32,
      [b] 0.75f32,
      [a] 1.0f32
    )
  }
  [GfxError] err{queueSubmitFailed()}
  [i32 mut] score{0i32}

  if(equal(colorFormat.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(depthFormat.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(presentMode.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(equal(shader.value, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  if(equal(window.width, 1280i32)) {
    score = plus(score, 1i32)
  } else {
    return(94i32)
  }
  if(equal(mesh.vertexCount, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(95i32)
  }
  if(equal(mesh.indexCount, 36i32)) {
    score = plus(score, 1i32)
  } else {
    return(96i32)
  }
  if(less_than(vertex.g, 0.60f32)) {
    score = plus(score, 1i32)
  } else {
    return(97i32)
  }
  if(equal(swapchain.token, 11i32)) {
    score = plus(score, 1i32)
  } else {
    return(98i32)
  }
  if(equal(err.code, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(99i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_canonical_type_surface.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_gfx_canonical_type_surface_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_canonical_type_surface_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 10);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("experimental gfx error helper imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [string] whyText{GfxError.why(err)}
  [Window] window{Window([token] 1i32, [width] 1280i32, [height] 720i32)}
  [ColorFormat] colorFormat{ColorFormat([value] 0i32)}
  [i32 mut] score{0i32}

  if(equal(window.width, 1280i32)) {
    score = plus(score, 1i32)
  }
  if(equal(colorFormat.value, 0i32)) {
    score = plus(score, 1i32)
  }
  if(equal(err.code, 8i32)) {
    score = plus(score, 1i32)
  }
  if(equal(count(whyText), 19i32)) {
    score = plus(score, 1i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_error_helper_vm.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_error_helper_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_error_helper_native").string();

  const std::string compileExeCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileExeCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("experimental gfx substrate boundary imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [SubstrateWindowConfig] windowConfig{
    SubstrateWindowConfig([hostToken] 11i32, [width] 1280i32, [height] 720i32)
  }
  [i32] windowToken{GraphicsSubstrate.createWindow(windowConfig)?}
  [Window] window{Window([token] windowToken, [width] windowConfig.width, [height] windowConfig.height)}
  [SubstrateDeviceConfig] deviceConfig{
    SubstrateDeviceConfig([window] window, [deviceToken] 13i32, [queueToken] 17i32)
  }
  [i32] deviceToken{GraphicsSubstrate.createDevice(deviceConfig)?}
  [i32] queueToken{GraphicsSubstrate.createQueue(deviceConfig)?}
  [Device] device{Device([token] deviceToken)}
  [Queue] queue{Queue([token] queueToken)}
  [SubstrateSwapchainConfig] swapchainConfig{
    SubstrateSwapchainConfig(
      [window] window,
      [device] device,
      [swapchainToken] 19i32,
      [colorFormat] ColorFormat.Bgra8Unorm,
      [depthFormat] DepthFormat.Depth32F,
      [presentMode] PresentMode.Fifo
    )
  }
  [i32] swapchainToken{GraphicsSubstrate.createSwapchain(swapchainConfig)?}
  [Swapchain] swapchain{
    Swapchain(
      [token] swapchainToken,
      [colorFormat] swapchainConfig.colorFormat,
      [depthFormat] swapchainConfig.depthFormat,
      [presentMode] swapchainConfig.presentMode
    )
  }
  [SubstrateMeshConfig] meshConfig{
    SubstrateMeshConfig([meshToken] 21i32, [vertexCount] 8i32, [indexCount] 36i32)
  }
  [i32] meshToken{GraphicsSubstrate.createMesh(meshConfig)?}
  [Mesh] mesh{Mesh([token] meshToken, [vertexCount] meshConfig.vertexCount, [indexCount] meshConfig.indexCount)}
  [SubstratePipelineConfig] pipelineConfig{
    SubstratePipelineConfig(
      [pipelineToken] 23i32,
      [shader] ShaderLibrary.CubeBasic,
      [colorFormat] ColorFormat.Bgra8Unorm,
      [depthFormat] DepthFormat.Depth32F
    )
  }
  [i32] pipelineToken{GraphicsSubstrate.createPipeline(pipelineConfig)?}
  [Pipeline] pipeline{Pipeline([token] pipelineToken)}
  [SubstrateFrameConfig] frameConfig{SubstrateFrameConfig([swapchain] swapchain, [frameToken] 27i32)}
  [i32] frameToken{GraphicsSubstrate.acquireFrame(frameConfig)?}
  [Frame] frame{Frame([token] frameToken)}
  [SubstrateRenderPassConfig] renderPassConfig{
    SubstrateRenderPassConfig(
      [frame] frame,
      [renderPassToken] 29i32,
      [clearColor] ColorRGBA(0.05f32, 0.07f32, 0.10f32, 1.0f32),
      [clearDepth] 1.0f32
    )
  }
  [i32] renderPassToken{GraphicsSubstrate.beginRenderPass(renderPassConfig)?}
  [RenderPass] renderPass{RenderPass([token] renderPassToken)}
  [Material] material{Material([token] 31i32)}
  [SubstrateDrawMeshConfig] drawMeshConfig{
    SubstrateDrawMeshConfig([renderPass] renderPass, [mesh] mesh, [material] material, [drawToken] 79i32)
  }
  [i32] drawToken{GraphicsSubstrate.drawMesh(drawMeshConfig)}
  [SubstrateRenderPassEndConfig] endConfig{
    SubstrateRenderPassEndConfig([renderPass] renderPass, [endToken] 83i32)
  }
  [i32] endToken{GraphicsSubstrate.endRenderPass(endConfig)}
  [i32 mut] score{0i32}

  if(equal(window.token, 11i32)) {
    score = plus(score, 1i32)
  }
  if(equal(device.token, 13i32)) {
    score = plus(score, 1i32)
  }
  if(equal(queue.token, 17i32)) {
    score = plus(score, 1i32)
  }
  if(equal(swapchain.colorFormat.value, 0i32)) {
    score = plus(score, 1i32)
  }
  if(equal(mesh.indexCount, 36i32)) {
    score = plus(score, 1i32)
  }
  if(equal(pipeline.token, 23i32)) {
    score = plus(score, 1i32)
  }
  if(equal(frame.token, 27i32)) {
    score = plus(score, 1i32)
  }
  if(equal(renderPass.token, 29i32)) {
    score = plus(score, 1i32)
  }
  if(equal(drawToken, 79i32)) {
    score = plus(score, 1i32)
  }
  if(equal(endToken, 83i32)) {
    score = plus(score, 1i32)
  }

  GraphicsSubstrate.submitFrame(frame.token, queue.token)?
  GraphicsSubstrate.presentFrame(frame.token)?
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_substrate_boundary.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_substrate_boundary_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_substrate_boundary_native").string();

  const std::string compileExeCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileExeCmd) == 0);
  CHECK(runCommand(exePath) == 10);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 10);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("experimental gfx window constructor entry point runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  [i32 mut] score{0i32}

  if(window.is_open()) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(window.width, 1280i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(window.height, 720i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(greater_than(window.aspect_ratio(), 1.7f32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_window_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_window_constructor_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_window_constructor_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_window_constructor",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 4);
  CHECK(runCommand(runVmCmd) == 4);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("experimental gfx device constructor entry point runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Device] device{Device()?}
  [Queue] queue{device.default_queue()}
  [i32 mut] score{0i32}

  if(equal(device.token, 2i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(queue.token, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_device_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_device_constructor_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_device_constructor_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_device_constructor",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 2);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("experimental gfx resource wrapper slice runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  [Device] device{Device()?}
  [Queue] queue{device.default_queue()}
  [Swapchain] swapchain{
    device.create_swapchain(
      window,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F,
      [present_mode] PresentMode.Fifo
    )?
  }
  [array<VertexColored>] vertices{
    array<VertexColored>(
      VertexColored([px] 0.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 1.0f32, [g] 0.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 1.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 1.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 0.0f32, [py] 1.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 0.0f32, [b] 1.0f32, [a] 1.0f32)
    )
  }
  [array<i32>] indices{array<i32>(0i32, 1i32, 2i32)}
  [Mesh] mesh{device.create_mesh([vertices] vertices, [indices] indices)?}
  [Frame] frame{swapchain.frame()?}
  [i32 mut] score{0i32}

  if(equal(queue.token, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(swapchain.token, 4i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(mesh.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(equal(mesh.vertexCount, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  if(equal(mesh.indexCount, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(94i32)
  }
  if(equal(frame.token, 5i32)) {
    score = plus(score, 1i32)
  } else {
    return(95i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_resource_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_resource_wrappers_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_resource_wrappers_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_resource_wrappers",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 6);
  CHECK(runCommand(runVmCmd) == 6);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("experimental gfx render pass wrapper slice runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Frame] frame{Frame([token] 31i32)}
  [Mesh] mesh{Mesh([token] 37i32, [vertexCount] 3i32, [indexCount] 3i32)}
  [Material] material{Material([token] 41i32)}
  [RenderPass] pass{
    frame.render_pass(
      [clear_color] ColorRGBA(0.05f32, 0.07f32, 0.10f32, 1.0f32),
      [clear_depth] 1.0f32
    )
  }
  [Frame] badFrame{Frame([token] 0i32)}
  [RenderPass] badPass{
    badFrame.render_pass(
      [clear_color] ColorRGBA(0.0f32, 0.0f32, 0.0f32, 1.0f32),
      [clear_depth] 1.0f32
    )
  }
  pass.draw_mesh(mesh, material)
  pass.end()
  badPass.draw_mesh(mesh, material)
  badPass.end()
  [i32 mut] score{0i32}

  if(equal(pass.token, 32i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(badPass.token, 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_render_pass_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_render_pass_wrappers_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_render_pass_wrappers_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_resource_wrapper_errors",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
    return;
  }
  CHECK(runCommand(exePath) == 2);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("experimental gfx resource wrapper errors stay deterministic across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Device] device{Device([token] 0i32)}
  [array<VertexColored>] vertices{
    array<VertexColored>(
      VertexColored([px] 0.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 1.0f32, [g] 0.0f32, [b] 0.0f32, [a] 1.0f32)
    )
  }
  [array<i32>] indices{array<i32>(0i32)}
  [Result<Mesh, GfxError>] meshResult{device.create_mesh([vertices] vertices, [indices] indices)}
  [i32 mut] score{0i32}

  if(Result.error(meshResult)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(greater_than(count(Result.why(meshResult)), 0i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_resource_wrapper_errors.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_resource_wrapper_errors_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_resource_wrapper_errors_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_resource_wrapper_errors",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
    return;
  }
  CHECK(runCommand(exePath) == 2);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("experimental gfx pipeline entry point runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Device] device{Device()?}
  [Pipeline] pipeline{
    device.create_pipeline(
      [shader] ShaderLibrary.CubeBasic,
      [vertex_type] VertexColored,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  [Material] material{pipeline.material()?}
  [i32 mut] score{0i32}

  if(equal(pipeline.token, 7i32)) {
    score = plus(score, 1i32)
  } else {
    return(90i32)
  }
  if(equal(material.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_pipeline_entry.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_pipeline_entry_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_pipeline_entry_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_pipeline_entry",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 2);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("experimental gfx end-to-end conformance runs across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  [Device] device{Device()?}
  [Queue] queue{device.default_queue()}
  if(window.is_open()) {
    window.poll_events()
  } else {
    return(90i32)
  }
  [Swapchain] swapchain{
    device.create_swapchain(
      window,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F,
      [present_mode] PresentMode.Fifo
    )?
  }
  [array<VertexColored>] vertices{
    array<VertexColored>(
      VertexColored([px] 0.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 1.0f32, [g] 0.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 1.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 1.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 0.0f32, [py] 1.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 0.0f32, [b] 1.0f32, [a] 1.0f32)
    )
  }
  [array<i32>] indices{array<i32>(0i32, 1i32, 2i32)}
  [Mesh] mesh{device.create_mesh([vertices] vertices, [indices] indices)?}
  [Pipeline] pipeline{
    device.create_pipeline(
      [shader] ShaderLibrary.CubeBasic,
      [vertex_type] VertexColored,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  [Material] material{pipeline.material()?}
  [Frame] frame{swapchain.frame()?}
  [RenderPass] pass{
    frame.render_pass(
      [clear_color] ColorRGBA(0.05f32, 0.07f32, 0.10f32, 1.0f32),
      [clear_depth] 1.0f32
    )
  }
  pass.draw_mesh(mesh, material)
  pass.end()
  frame.submit(queue)?
  frame.present()?
  [i32 mut] score{0i32}

  if(equal(window.token, 1i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(device.token, 2i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(equal(queue.token, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  if(equal(swapchain.token, 4i32)) {
    score = plus(score, 1i32)
  } else {
    return(94i32)
  }
  if(equal(mesh.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(95i32)
  }
  if(equal(pipeline.token, 7i32)) {
    score = plus(score, 1i32)
  } else {
    return(96i32)
  }
  if(equal(material.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(97i32)
  }
  if(equal(frame.token, 5i32)) {
    score = plus(score, 1i32)
  } else {
    return(98i32)
  }
  if(equal(pass.token, 6i32)) {
    score = plus(score, 1i32)
  } else {
    return(99i32)
  }
  if(greater_than(window.aspect_ratio(), 1.7f32)) {
    score = plus(score, 1i32)
  } else {
    return(100i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_end_to_end_conformance.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_experimental_end_to_end_conformance_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_end_to_end_conformance_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_experimental_end_to_end_conformance",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 10);
  CHECK(runCommand(runVmCmd) == 10);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("canonical gfx end-to-end conformance runs across backends") {
  const std::string source = R"(
import /std/gfx/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  [Device] device{Device()?}
  [Queue] queue{device.default_queue()}
  if(window.is_open()) {
    window.poll_events()
  } else {
    return(90i32)
  }
  [Swapchain] swapchain{
    device.create_swapchain(
      window,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F,
      [present_mode] PresentMode.Fifo
    )?
  }
  [array<VertexColored>] vertices{
    array<VertexColored>(
      VertexColored([px] 0.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 1.0f32, [g] 0.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 1.0f32, [py] 0.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 1.0f32, [b] 0.0f32, [a] 1.0f32),
      VertexColored([px] 0.0f32, [py] 1.0f32, [pz] 0.0f32, [pw] 1.0f32, [r] 0.0f32, [g] 0.0f32, [b] 1.0f32, [a] 1.0f32)
    )
  }
  [array<i32>] indices{array<i32>(0i32, 1i32, 2i32)}
  [Mesh] mesh{device.create_mesh([vertices] vertices, [indices] indices)?}
  [Pipeline] pipeline{
    device.create_pipeline(
      [shader] ShaderLibrary.CubeBasic,
      [vertex_type] VertexColored,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  [Material] material{pipeline.material()?}
  [Frame] frame{swapchain.frame()?}
  [RenderPass] pass{
    frame.render_pass(
      [clear_color] ColorRGBA(0.05f32, 0.07f32, 0.10f32, 1.0f32),
      [clear_depth] 1.0f32
    )
  }
  pass.draw_mesh(mesh, material)
  pass.end()
  frame.submit(queue)?
  frame.present()?
  [i32 mut] score{0i32}

  if(equal(window.token, 1i32)) {
    score = plus(score, 1i32)
  } else {
    return(91i32)
  }
  if(equal(device.token, 2i32)) {
    score = plus(score, 1i32)
  } else {
    return(92i32)
  }
  if(equal(queue.token, 3i32)) {
    score = plus(score, 1i32)
  } else {
    return(93i32)
  }
  if(equal(swapchain.token, 4i32)) {
    score = plus(score, 1i32)
  } else {
    return(94i32)
  }
  if(equal(mesh.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(95i32)
  }
  if(equal(pipeline.token, 7i32)) {
    score = plus(score, 1i32)
  } else {
    return(96i32)
  }
  if(equal(material.token, 8i32)) {
    score = plus(score, 1i32)
  } else {
    return(97i32)
  }
  if(equal(frame.token, 5i32)) {
    score = plus(score, 1i32)
  } else {
    return(98i32)
  }
  if(equal(pass.token, 6i32)) {
    score = plus(score, 1i32)
  } else {
    return(99i32)
  }
  if(greater_than(window.aspect_ratio(), 1.7f32)) {
    score = plus(score, 1i32)
  } else {
    return(100i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_canonical_end_to_end_conformance.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_gfx_canonical_end_to_end_conformance_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_canonical_end_to_end_conformance_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_canonical_end_to_end_conformance",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                std::string(IrResultOkUnsupportedMessage),
                                                std::string(IrResultOkUnsupportedMessage))) {
    return;
  }
  CHECK(runCommand(exePath) == 10);
  CHECK(runCommand(runVmCmd) == 10);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("gfx imports reject unsupported backend targets with deterministic diagnostics") {
  struct NegativeCase {
    std::string name;
    std::string importPath;
    std::string emitKind;
    std::string wasmProfile;
    std::string outputSuffix;
    std::string expectedMessage;
  };

  const std::vector<NegativeCase> cases = {
      {
          "canonical_wasm_browser",
          "/std/gfx/*",
          "wasm",
          "browser",
          ".wasm",
          "graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*",
      },
      {
          "canonical_wasm_wasi",
          "/std/gfx/*",
          "wasm",
          "wasi",
          ".wasm",
          "graphics stdlib runtime substrate unavailable for wasm-wasi target: /std/gfx/*",
      },
      {
          "experimental_glsl",
          "/std/gfx/experimental/*",
          "glsl",
          "wasi",
          ".glsl",
          "graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/experimental/*",
      },
      {
          "canonical_spirv",
          "/std/gfx/*",
          "spirv",
          "wasi",
          ".spv",
          "graphics stdlib runtime substrate unavailable for spirv target: /std/gfx/*",
      },
  };

  for (const auto &negativeCase : cases) {
    CAPTURE(negativeCase.name);
    const std::string source = "import " + negativeCase.importPath + R"(

[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("compile_gfx_unsupported_backend_" + negativeCase.name + ".prime", source);
    const std::string outPath =
        (testScratchPath("") / ("primec_gfx_unsupported_backend_" + negativeCase.name +
                                                   negativeCase.outputSuffix))
            .string();
    const std::string errPath =
        (testScratchPath("") / ("primec_gfx_unsupported_backend_" + negativeCase.name + ".json"))
            .string();
    std::string command = "./primec --emit=" + negativeCase.emitKind;
    if (negativeCase.emitKind == "wasm") {
      command += " --wasm-profile " + negativeCase.wasmProfile;
    }
    command += " " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
               " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(command) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find(negativeCase.expectedMessage) != std::string::npos);
  }
}

TEST_CASE("experimental gfx static fields import across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [ColorFormat] color{ColorFormat.Bgra8Unorm}
  [DepthFormat] depth{DepthFormat.Depth32F}
  [PresentMode] mode{PresentMode.Fifo}
  [ShaderLibrary] shader{ShaderLibrary.CubeBasic}
  [i32 mut] score{0i32}

  if(equal(color.value, 0i32)) {
    score = plus(score, 1i32)
  }
  if(equal(depth.value, 0i32)) {
    score = plus(score, 1i32)
  }
  if(equal(mode.value, 0i32)) {
    score = plus(score, 1i32)
  }
  if(equal(shader.value, 0i32)) {
    score = plus(score, 1i32)
  }
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_gfx_experimental_static_fields.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_gfx_experimental_static_fields_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_gfx_experimental_static_fields_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("count forwards to type method across backends") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 2i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_count_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_count_method_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("semicolons act as separators") {
  const std::string source = R"(
;
[return<int>]
add([i32] a; [i32] b) {
  return(plus(a, b));
}
;
[return<int>]
main() {
  [i32] value{
    add(1i32; 2i32);
  };
  return(value);
}
)";
  const std::string srcPath = writeTemp("compile_semicolon_separators.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_semicolon_sep_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_semicolon_sep_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("rejects non-argv entry parameter in exe") {
  const std::string source = R"(
[return<int>]
main([i32] value) {
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_entry_bad_param.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_entry_bad_param_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("rejects unsupported emit kinds") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_invalid.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_emit_invalid_err.txt").string();

  const std::string emitMetalCmd =
      "./primec --emit=metal " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitMetalCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);

  const std::string emitLlvmCmd =
      "./primec --emit=llvm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitLlvmCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("primec emits wasm bytecode for integer local control-flow subset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{plus(2i32, 5i32)}
  if(equal(value, 7i32)) {
    return(7i32)
  } else {
    return(3i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_subset.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_subset.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_option_err.txt").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_wasm_subset_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("7") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for float ops with tolerance-gated conversions") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  if(less_than(abs(minus(convert<f32>(convert<f64>(plus(1.25f32, 0.5f32))), 1.75f32)), 0.0001f32)) {
    return(convert<int>(multiply(convert<f32>(2i32), 2.5f32)))
  } else {
    return(0i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_float_subset.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_float_subset.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_float_subset_err.txt").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_wasm_float_subset_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("5") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for i64 and u64 conversion opcodes") {
  const std::string source = R"(
[return<int>]
main() {
  if(equal(convert<int>(convert<f64>(convert<i64>(7.9f32))), 7i32)) {
    if(equal(convert<int>(convert<f32>(convert<u64>(9.9f32))), 9i32)) {
      return(1i32)
    }
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_i64_u64_conversions.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for support-matrix math nominal helpers") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] raw{Quat(0.0f32, 0.0f32, 0.0f32, 2.0f32)}
  [Quat] normalized{raw.toNormalized()}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, -1.0f32, 0.0f32,
    0.0f32, 0.0f32, -1.0f32
  ))}
  [f32] total{m2.m00 + m2.m11 + normalized.w + basis3.m00 + basis4.m33 + restored.x + restored.w}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_math_nominal_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_math_nominal_helpers.wasm").string();
  const std::string errPath = (testScratchPath("") /
                               "primec_emit_wasm_support_matrix_math_nominal_helpers_err.txt")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_emit_wasm_support_matrix_math_nominal_helpers_out.txt")
                                  .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("9") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for quaternion reference multiply and rotation") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] turnX{Quat(1.0f32, 0.0f32, 0.0f32, 0.0f32)}
  [Quat] turnY{Quat(0.0f32, 1.0f32, 0.0f32, 0.0f32)}
  [Quat] product{multiply(turnX, turnY)}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 3.0f32)}
  [Vec3] rotated{multiply(product, input)}
  [f32] total{product.z - product.x - product.y - product.w + rotated.z - rotated.x - rotated.y}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_quaternion_reference_multiply_rotation.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_reference_multiply_rotation.wasm")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_emit_wasm_quaternion_reference_multiply_rotation_err.txt")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_emit_wasm_quaternion_reference_multiply_rotation_out.txt")
          .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("7") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for matrix composition order references with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] rotate{Mat3(
    0.0f32, -1.0f32, 0.0f32,
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat3] scale{Mat3(
    2.0f32, 0.0f32, 0.0f32,
    0.0f32, 3.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 4.0f32)}
  [Vec3] rotatedInput{multiply(rotate, input)}
  [Vec3] nested{multiply(scale, rotatedInput)}
  [Mat3] combined{multiply(scale, rotate)}
  [Vec3] viaCombined{multiply(combined, input)}
  [Mat3] wrongCombined{multiply(rotate, scale)}
  [Vec3] wrongOrder{multiply(wrongCombined, input)}
  [f32] tolerance{0.0001f32}
  [f32] nestedError{abs(nested.x + 4.0f32) + abs(nested.y - 3.0f32) + abs(nested.z - 4.0f32)}
  [f32] combinedError{abs(viaCombined.x + 4.0f32) + abs(viaCombined.y - 3.0f32) + abs(viaCombined.z - 4.0f32)}
  [f32] wrongOrderError{abs(wrongOrder.x + 6.0f32) + abs(wrongOrder.y - 2.0f32) + abs(wrongOrder.z - 4.0f32)}
  return(convert<int>(nestedError <= tolerance && combinedError <= tolerance && wrongOrderError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_matrix_composition_reference.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for matrix arithmetic helpers with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] base2{Mat2(
    1.0f32, 2.0f32,
    3.0f32, 4.0f32
  )}
  [Mat2] delta2{Mat2(
    0.5f32, -1.0f32,
    1.5f32, 2.0f32
  )}
  [Mat2] sum2{plus(base2, delta2)}
  [Mat2] div2{divide(sum2, 2.0f32)}
  [Mat3] base3{Mat3(
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  )}
  [Mat3] delta3{Mat3(
    0.5f32, 1.0f32, 1.5f32,
    2.0f32, 2.5f32, 3.0f32,
    3.5f32, 4.0f32, 4.5f32
  )}
  [Mat3] diff3{minus(base3, delta3)}
  [Mat3] scaledLeft3{multiply(2i32, base3)}
  [Mat4] base4{Mat4(
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  )}
  [Mat4] scaledRight4{multiply(base4, 0.5f32)}
  [Mat4] doubled4{multiply(base4, 2.0f32)}
  [Mat4] restored4{divide(doubled4, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum2.m00 - 1.5f32) +
    abs(sum2.m01 - 1.0f32) +
    abs(sum2.m10 - 4.5f32) +
    abs(sum2.m11 - 6.0f32) +
    abs(div2.m00 - 0.75f32) +
    abs(div2.m11 - 3.0f32) +
    abs(diff3.m00 - 0.5f32) +
    abs(diff3.m12 - 3.0f32) +
    abs(diff3.m22 - 4.5f32) +
    abs(scaledLeft3.m00 - 2.0f32) +
    abs(scaledLeft3.m12 - 12.0f32) +
    abs(scaledLeft3.m22 - 18.0f32) +
    abs(scaledRight4.m00 - 0.5f32) +
    abs(scaledRight4.m13 - 4.0f32) +
    abs(scaledRight4.m31 - 7.0f32) +
    abs(restored4.m00 - 1.0f32) +
    abs(restored4.m12 - 7.0f32) +
    abs(restored4.m30 - 13.0f32) +
    abs(restored4.m33 - 16.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_matrix_arithmetic_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for quaternion arithmetic helpers with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] base{Quat(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] delta{Quat(0.5f32, -1.0f32, 1.5f32, 2.0f32)}
  [Quat] sum{plus(base, delta)}
  [Quat] diff{minus(base, delta)}
  [Quat] scaledLeft{multiply(2i32, base)}
  [Quat] scaledRight{multiply(base, 0.5f32)}
  [Quat] divided{divide(sum, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum.x - 1.5f32) +
    abs(sum.y - 1.0f32) +
    abs(sum.z - 4.5f32) +
    abs(sum.w - 6.0f32) +
    abs(diff.x - 0.5f32) +
    abs(diff.y - 3.0f32) +
    abs(diff.z - 1.5f32) +
    abs(diff.w - 2.0f32) +
    abs(scaledLeft.x - 2.0f32) +
    abs(scaledLeft.y - 4.0f32) +
    abs(scaledLeft.z - 6.0f32) +
    abs(scaledLeft.w - 8.0f32) +
    abs(scaledRight.x - 0.5f32) +
    abs(scaledRight.y - 1.0f32) +
    abs(scaledRight.z - 1.5f32) +
    abs(scaledRight.w - 2.0f32) +
    abs(divided.x - 0.75f32) +
    abs(divided.y - 0.5f32) +
    abs(divided.z - 2.25f32) +
    abs(divided.w - 3.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_quaternion_arithmetic_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec rejects wasm support-matrix plus mismatch with deterministic diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] rhs{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat2] value{plus(lhs, rhs)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_plus_mismatch.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_plus_mismatch.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_plus_mismatch_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(readFile(errPath).find("plus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("primec rejects wasm support-matrix implicit conversion with deterministic diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] basis{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [auto] value{quat_to_mat3(basis)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_implicit_conversion.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_implicit_conversion.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_implicit_conversion_err.txt")
          .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  CHECK(readFile(errPath).find(
            "implicit matrix/quaternion family conversion requires explicit helper: expected /std/math/Quat got "
            "/std/math/Mat3") != std::string::npos);
}

TEST_CASE("primec emits wasm bytecode for direct callable definitions") {
  const std::string source = R"(
[return<int>]
helper() {
  return(9i32)
}

[return<int>]
main() {
  return(plus(helper(), 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_direct_call.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_direct_call.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_direct_call_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unsupported control-flow pattern") != std::string::npos);
  CHECK(std::filesystem::exists(wasmPath) == false);
}

TEST_CASE("primec wasm wasi stdout and stderr match vm output") {
  const std::string source = R"(
[return<int> effects(io_out, io_err)]
main() {
  print_line("hello"utf8)
  print_error("warn"utf8)
  print_line_error("err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_wasi_output_parity.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_output_parity.wasm").string();
  const std::string compileErrPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_output_parity_compile_err.txt").string();
  const std::string vmOutPath = (testScratchPath("") / "primec_vm_wasi_output_parity_out.txt").string();
  const std::string vmErrPath = (testScratchPath("") / "primec_vm_wasi_output_parity_err.txt").string();
  const std::string wasmOutPath =
      (testScratchPath("") / "primec_wasm_wasi_output_parity_out.txt").string();
  const std::string wasmErrPath =
      (testScratchPath("") / "primec_wasm_wasi_output_parity_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string vmCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(vmOutPath) +
                            " 2> " + quoteShellArg(vmErrPath);
  CHECK(runCommand(vmCmd) == 0);

  if (hasWasmtime()) {
    const std::string wasmRunCmd = "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " +
                                   quoteShellArg(wasmOutPath) + " 2> " + quoteShellArg(wasmErrPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(wasmOutPath) == readFile(vmOutPath));
    CHECK(readFile(wasmErrPath) == readFile(vmErrPath));
  }
}

TEST_CASE("primec wasm wasi argc path matches vm exit code") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_wasi_argc_parity.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_argc_parity.wasm").string();
  const std::string compileErrPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_argc_parity_compile_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string vmCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main";
  const int vmExitCode = runCommand(vmCmd);

  if (hasWasmtime()) {
    const std::string wasmRunCmd = "wasmtime --invoke main " + quoteShellArg(wasmPath);
    const int wasmExitCode = runCommand(wasmRunCmd);
    CHECK(wasmExitCode == vmExitCode);
  }
}

TEST_CASE("primec wasm wasi supports File<Read>.read_byte with deterministic eof") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_file_read_byte_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.bin", std::ios::binary);
    REQUIRE(input.good());
    input.write("AB", 2);
    REQUIRE(input.good());
  }

  const std::string source = R"(
[return<Result<FileError>> effects(file_read, io_out) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("input.bin"utf8)? }
  [i32 mut] first{0i32}
  [i32 mut] second{0i32}
  [i32 mut] third{99i32}
  file.read_byte(first)?
  file.read_byte(second)?
  print_line(first)
  print_line(second)
  print_line(Result.why(file.read_byte(third)))
  print_line(third)
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_file_read_byte.prime", source);
  const std::string wasmPath = (tempRoot / "file_read_byte.wasm").string();
  const std::string compileErrPath = (tempRoot / "file_read_byte_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "file_read_byte_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath) == "65\n66\nEOF\n99\n");
  }
}

TEST_CASE("primec wasm wasi runs ppm read for ascii p3 inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_read_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P3\n2 1\n255\n255 0 0 0 255 128\n";
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_read.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_read.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_read_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_read_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "255\n"
                            "0\n"
                            "0\n"
                            "0\n"
                            "255\n"
                            "128\n");
}

TEST_CASE("primec wasm wasi runs binary p6 ppm inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_p6_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P6\n2 1\n255\n";
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(128));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_read), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_p6.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_p6.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_p6_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_p6_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "255\n"
                            "0\n"
                            "0\n"
                            "0\n"
                            "255\n"
                            "128\n");
}

TEST_CASE("primec wasm wasi rejects truncated binary ppm reads deterministically") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_ppm_p6_truncated_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P6\n2 1\n255\n";
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(255));
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
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_p6_truncated.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_p6_truncated.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_p6_truncated_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_p6_truncated_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            0,
                            "image_invalid_operation\n"
                            "0\n"
                            "0\n"
                            "0\n");
}

TEST_CASE("primec wasm wasi ppm write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_write_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi invalid ppm write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_ppm_write_invalid_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write_invalid.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write_invalid.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_invalid_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi runs ppm write for deterministic rgb outputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_write_success_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write_success.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write_success.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_success_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "ppm_write_success_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath).empty());
    CHECK(readFile((tempRoot / "output.ppm").string()) ==
          "P3\n"
          "2 1\n"
          "255\n"
          "255\n"
          "0\n"
          "0\n"
          "0\n"
          "255\n"
          "128\n");
  }
}

TEST_CASE("primec wasm wasi png write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_write_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write.prime", source);
  const std::string wasmPath = (tempRoot / "png_write.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi runs png write for deterministic rgb outputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_write_success_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_success.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_success.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_success_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "png_write_success_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath).empty());
    const std::vector<unsigned char> expected = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x00, 0xff, 0x80, 0x08, 0x7f,
        0x02, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
    CHECK(readBinaryFile((tempRoot / "output.png").string()) == expected);
  }
}

TEST_CASE("primec wasm wasi invalid png write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_png_write_invalid_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_invalid.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_invalid.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_invalid_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi rejects invalid png write inputs deterministically") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_png_write_invalid_result_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_invalid_result.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_invalid_result.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_invalid_result_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "png_write_invalid_result_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath) == "image_invalid_operation\n");
    CHECK(!std::filesystem::exists(tempRoot / "output.png"));
  }
}

TEST_CASE("primec wasm wasi runs stored rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_read_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0f, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_read.prime", source);
  const std::string wasmPath = (tempRoot / "png_read.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_read_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_read_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot, wasmPath, outPath, 2, "1\n1\n3\n255\n0\n0\n");
}

TEST_CASE("primec wasm wasi runs stored sub-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_sub_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x01,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x02, 0x3e,
        0x00, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_sub.prime", source);
  const std::string wasmPath = (tempRoot / "png_sub.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_sub_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_sub_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "50\n"
                            "70\n"
                            "90\n");
}

TEST_CASE("primec wasm wasi runs stored up-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_up_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x02, 0x05, 0x07, 0x09, 0x01,
        0x8a, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_up.prime", source);
  const std::string wasmPath = (tempRoot / "png_up.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_up_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_up_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "1\n"
                            "2\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "15\n"
                            "27\n"
                            "39\n");
}

TEST_CASE("primec wasm wasi runs stored average-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_average_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x03, 0x0a, 0x11, 0x18, 0x01,
        0xc0, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_average.prime", source);
  const std::string wasmPath = (tempRoot / "png_average.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_average_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_average_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "1\n"
                            "2\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "15\n"
                            "27\n"
                            "39\n");
}

TEST_CASE("primec wasm wasi runs stored paeth-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_paeth_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x0e, 0x00, 0xf1, 0xff, 0x04,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x04, 0x05,
        0x07, 0x09, 0x0a, 0x14, 0x1e, 0x09, 0x19, 0x01,
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_paeth.prime", source);
  const std::string wasmPath = (tempRoot / "png_paeth.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_paeth_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_paeth_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "2\n"
                            "2\n"
                            "12\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "50\n"
                            "70\n"
                            "90\n"
                            "15\n"
                            "27\n"
                            "39\n"
                            "60\n"
                            "90\n"
                            "120\n");
}

TEST_CASE("primec wasm wasi runs fixed-huffman backreference rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_fixed_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x63, 0xe0, 0x12, 0x91, 0x83, 0x20,
        0x00, 0x03, 0x52, 0x00, 0xb5, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44,
        0x00, 0x00, 0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_fixed.prime", source);
  const std::string wasmPath = (tempRoot / "png_fixed.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_fixed_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_fixed_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "3\n"
                            "1\n"
                            "9\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n");
}

TEST_CASE("primec wasm wasi runs dynamic-huffman literal rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_dynamic_literal_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x05, 0xc0, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xb0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
        0xe7, 0xfb, 0x73, 0x7d, 0xa0, 0x9c, 0xee, 0x02,
        0x37, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    input.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_dynamic_literal.prime", source);
  const std::string wasmPath = (tempRoot / "png_dynamic_literal.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_dynamic_literal_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_dynamic_literal_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "40\n"
                            "50\n"
                            "60\n");
}

TEST_CASE("primec wasm wasi runs dynamic-huffman backreference rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_dynamic_backref_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x25, 0xc3, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0xc3, 0x80, 0xff, 0xf1, 0xf9, 0xf9, 0xfe,
        0x98, 0x0f, 0xdf, 0x36, 0x38, 0x7d, 0x03, 0x03,
        0x52, 0x00, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    input.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_dynamic_backref.prime", source);
  const std::string wasmPath = (tempRoot / "png_dynamic_backref.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_dynamic_backref_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_dynamic_backref_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "3\n"
                            "1\n"
                            "9\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n");
}

TEST_CASE("primec wasm wasi runs broader interlaced png decode programs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_interlaced_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x08, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x61, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x56, 0x00, 0xa9, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x34, 0x68,
        0x00, 0x1c, 0xc8, 0x2c, 0xbc, 0xfc, 0x94, 0x00,
        0x50, 0x1a, 0xb4, 0x00, 0x6c, 0xe2, 0xe0, 0x00,
        0x0e, 0x64, 0x16, 0x5e, 0x7e, 0xca, 0xae, 0x98,
        0x7e, 0x00, 0x28, 0x0d, 0x5a, 0x78, 0x27, 0x0e,
        0x00, 0x36, 0x71, 0x70, 0x86, 0x8b, 0x24, 0x00,
        0x44, 0xd5, 0x86, 0x94, 0xef, 0x3a, 0x00, 0x07,
        0x32, 0x0b, 0x2f, 0x3f, 0x65, 0x57, 0x4c, 0xbf,
        0x7f, 0x59, 0x19, 0xa7, 0x66, 0x73, 0x00, 0x15,
        0x96, 0x21, 0x3d, 0xa3, 0x7b, 0x65, 0xb0, 0xd5,
        0x8d, 0xbd, 0x2f, 0xb5, 0xca, 0x89, 0xcf, 0x85,
        0x1f, 0x37,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[12i32])
  print_line(pixels[13i32])
  print_line(pixels[14i32])
  print_line(pixels[60i32])
  print_line(pixels[61i32])
  print_line(pixels[62i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  print_line(pixels[30i32])
  print_line(pixels[31i32])
  print_line(pixels[32i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[15i32])
  print_line(pixels[16i32])
  print_line(pixels[17i32])
  print_line(pixels[54i32])
  print_line(pixels[55i32])
  print_line(pixels[56i32])
  print_line(pixels[72i32])
  print_line(pixels[73i32])
  print_line(pixels[74i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_interlaced.prime", source);
  const std::string wasmPath = (tempRoot / "png_interlaced.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_interlaced_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_interlaced_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            10,
                            "5\n"
                            "5\n"
                            "75\n"
                            "0\n"
                            "0\n"
                            "0\n"
                            "160\n"
                            "52\n"
                            "104\n"
                            "28\n"
                            "200\n"
                            "44\n"
                            "80\n"
                            "26\n"
                            "180\n"
                            "120\n"
                            "39\n"
                            "14\n"
                            "14\n"
                            "100\n"
                            "22\n"
                            "40\n"
                            "13\n"
                            "90\n"
                            "7\n"
                            "50\n"
                            "11\n"
                            "141\n"
                            "189\n"
                            "47\n"
                            "188\n"
                            "252\n"
                            "148\n");
}

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

TEST_CASE("primec wasm documented limit IDs have conformance coverage") {
  struct WasmSectionInfo {
    uint8_t id = 0;
    size_t payloadOffset = 0;
    uint32_t payloadSize = 0;
  };
  struct WasmImportInfo {
    std::string module;
    std::string name;
  };

  const auto readBinary = [](const std::string &path, std::vector<uint8_t> &out) {
    out.clear();
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
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(out.data()), size);
    return file.good() || file.eof();
  };

  const auto readU32Leb = [](const std::vector<uint8_t> &bytes, size_t &cursor, uint32_t &value) {
    value = 0;
    uint32_t shift = 0;
    for (int i = 0; i < 5; ++i) {
      if (cursor >= bytes.size()) {
        return false;
      }
      const uint8_t byte = bytes[cursor++];
      value |= static_cast<uint32_t>(byte & 0x7fu) << shift;
      if ((byte & 0x80u) == 0) {
        return true;
      }
      shift += 7;
    }
    return false;
  };

  const auto parseSections = [&](const std::vector<uint8_t> &bytes, std::vector<WasmSectionInfo> &sections) {
    sections.clear();
    if (bytes.size() < 8) {
      return false;
    }
    const std::vector<uint8_t> wasmHeader = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    for (size_t i = 0; i < wasmHeader.size(); ++i) {
      if (bytes[i] != wasmHeader[i]) {
        return false;
      }
    }
    size_t cursor = 8;
    while (cursor < bytes.size()) {
      WasmSectionInfo section;
      section.id = bytes[cursor++];
      if (!readU32Leb(bytes, cursor, section.payloadSize)) {
        return false;
      }
      section.payloadOffset = cursor;
      if (cursor + section.payloadSize > bytes.size()) {
        return false;
      }
      sections.push_back(section);
      cursor += section.payloadSize;
    }
    return true;
  };

  const auto parseImportSection = [&](const std::vector<uint8_t> &bytes,
                                      const std::vector<WasmSectionInfo> &sections,
                                      std::vector<WasmImportInfo> &imports) {
    imports.clear();
    const WasmSectionInfo *importSection = nullptr;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 2) {
        importSection = &section;
        break;
      }
    }
    if (importSection == nullptr) {
      return false;
    }

    auto readName = [&](size_t &cursor, const size_t limit, std::string &out) {
      uint32_t length = 0;
      if (!readU32Leb(bytes, cursor, length)) {
        return false;
      }
      if (cursor + length > limit) {
        return false;
      }
      out.assign(reinterpret_cast<const char *>(bytes.data() + cursor), length);
      cursor += length;
      return true;
    };

    size_t cursor = importSection->payloadOffset;
    const size_t limit = importSection->payloadOffset + importSection->payloadSize;
    uint32_t importCount = 0;
    if (!readU32Leb(bytes, cursor, importCount)) {
      return false;
    }
    for (uint32_t i = 0; i < importCount; ++i) {
      WasmImportInfo entry;
      if (!readName(cursor, limit, entry.module) || !readName(cursor, limit, entry.name)) {
        return false;
      }
      if (cursor >= limit) {
        return false;
      }
      const uint8_t kind = bytes[cursor++];
      if (kind != 0x00) {
        return false;
      }
      uint32_t typeIndex = 0;
      if (!readU32Leb(bytes, cursor, typeIndex)) {
        return false;
      }
      imports.push_back(entry);
    }
    return cursor == limit;
  };

  {
    CAPTURE("WASM-LIMIT-MEM-ON-DEMAND");
    const std::string source = R"(
[return<int>]
main() {
  return(plus(2i32, 3i32))
}
)";
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_mem_on_demand.prime", source);
    const std::string wasmPath =
        (testScratchPath("") / "primec_emit_wasm_limit_mem_on_demand.wasm").string();
    const std::string errPath =
        (testScratchPath("") / "primec_emit_wasm_limit_mem_on_demand.err").string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile browser --default-effects none " +
                                quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) + " --entry /main 2> " +
                                quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 0);

    std::vector<uint8_t> bytes;
    REQUIRE(readBinary(wasmPath, bytes));
    std::vector<WasmSectionInfo> sections;
    REQUIRE(parseSections(bytes, sections));
    size_t memorySectionCount = 0;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 5) {
        ++memorySectionCount;
      }
    }
    CHECK(memorySectionCount == 0u);

    std::vector<WasmImportInfo> imports;
    REQUIRE(parseImportSection(bytes, sections, imports));
    CHECK(imports.empty());
  }

  {
    CAPTURE("WASM-LIMIT-MEM-SINGLE");
    CAPTURE("WASM-LIMIT-IMPORTS-WASI");
    const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line("ok"utf8)
  return(args.count())
}
)";
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_runtime_surface.prime", source);
    const std::string wasmPath =
        (testScratchPath("") / "primec_emit_wasm_limit_runtime_surface.wasm").string();
    const std::string errPath =
        (testScratchPath("") / "primec_emit_wasm_limit_runtime_surface.err").string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 0);

    std::vector<uint8_t> bytes;
    REQUIRE(readBinary(wasmPath, bytes));
    std::vector<WasmSectionInfo> sections;
    REQUIRE(parseSections(bytes, sections));

    const WasmSectionInfo *memorySection = nullptr;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 5) {
        CHECK(memorySection == nullptr);
        memorySection = &section;
      }
    }
    REQUIRE(memorySection != nullptr);
    size_t memoryCursor = memorySection->payloadOffset;
    const size_t memoryLimit = memorySection->payloadOffset + memorySection->payloadSize;
    uint32_t memoryCount = 0;
    REQUIRE(readU32Leb(bytes, memoryCursor, memoryCount));
    CHECK(memoryCount == 1u);
    CHECK(memoryCursor <= memoryLimit);

    std::vector<WasmImportInfo> imports;
    REQUIRE(parseImportSection(bytes, sections, imports));
    std::vector<std::string> importModules;
    std::vector<std::string> importNames;
    importModules.reserve(imports.size());
    importNames.reserve(imports.size());
    for (const WasmImportInfo &entry : imports) {
      importModules.push_back(entry.module);
      importNames.push_back(entry.name);
    }
    for (const std::string &moduleName : importModules) {
      CHECK(moduleName == "wasi_snapshot_preview1");
    }
    std::sort(importNames.begin(), importNames.end());
    const std::vector<std::string> expectedNames = {"args_get", "args_sizes_get", "fd_write"};
    CHECK(importNames == expectedNames);
  }

  struct DiagnosticLimitCase {
    std::string limitId;
    std::string name;
    std::string profile;
    std::string source;
    std::string expectedMessage;
  };
  const std::vector<DiagnosticLimitCase> diagnosticsCases = {
      {
          "WASM-LIMIT-PROFILE-BROWSER",
          "browser_rejects_effects",
          "browser",
          R"(
[return<int> effects(io_out)]
main() {
  return(0i32)
}
)",
          "unsupported effect mask bits for wasm-browser target",
      },
      {
          "WASM-LIMIT-PROFILE-BROWSER",
          "browser_rejects_wasi_opcodes",
          "browser",
          R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)",
          "unsupported opcode for wasm-browser target",
      },
      {
          "WASM-LIMIT-PROFILE-WASI-ALLOWLIST",
          "wasi_rejects_unsupported_opcode",
          "wasi",
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
  };
  for (const auto &diagCase : diagnosticsCases) {
    CAPTURE(diagCase.limitId);
    CAPTURE(diagCase.name);
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_diag_" + diagCase.name + ".prime", diagCase.source);
    const std::string wasmPath =
        (testScratchPath("") / ("primec_emit_wasm_limit_diag_" + diagCase.name + ".wasm")).string();
    const std::string errPath =
        (testScratchPath("") / ("primec_emit_wasm_limit_diag_" + diagCase.name + ".json")).string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile " + diagCase.profile +
                                " --default-effects none " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main --emit-diagnostics 2> " +
                                quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find(diagCase.expectedMessage) != std::string::npos);
    CHECK(diagnostics.find("\"notes\":[\"backend: wasm\",\"stage: ir-validate\"]") != std::string::npos);
  }
}

TEST_CASE("graphics api contract doc-linked constraints stay locked") {
  auto resolveDocPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::current_path() / "docs" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path().parent_path() / "docs" / name;
    }
    return path;
  };

  const std::filesystem::path graphicsDocPath = resolveDocPath("Graphics_API_Design.md");
  const std::filesystem::path primeStructPath = resolveDocPath("PrimeStruct.md");
  const std::filesystem::path guidelinesDocPath = resolveDocPath("Coding_Guidelines.md");
  REQUIRE(std::filesystem::exists(graphicsDocPath));
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(guidelinesDocPath));
  const std::string graphicsDoc = readFile(graphicsDocPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string guidelinesDoc = readFile(guidelinesDocPath.string());

  const std::vector<std::string> lockedConstraintIds = {
      "GFX-CORE-API-NAMESPACE",
      "GFX-CORE-SURFACE-V1",
      "GFX-CORE-NO-EXT-NS",
      "GFX-PROFILE-IDENTIFIERS",
      "GFX-PROFILE-GATING",
      "GFX-DIAG-PROFILE-CONTEXT",
      "GFX-V1-MINISPEC-SIGNATURES",
      "GFX-V1-PROFILE-DEDUCTION",
      "GFX-V1-VERTEXCOLORED-LAYOUT",
      "GFX-V1-ERROR-CODES",
      "GFX-V1-RESULT-PROPAGATION",
  };
  for (const std::string &constraintId : lockedConstraintIds) {
    CAPTURE(constraintId);
    CHECK(graphicsDoc.find(constraintId) != std::string::npos);
  }

  const std::vector<std::string> lockedCoreSymbols = {
      "Buffer<T>",
      "Texture2D<T>",
      "Sampler",
      "ShaderModule",
      "Pipeline",
      "BindGroupLayout",
      "BindGroup",
      "CommandEncoder",
      "RenderPass",
      "Queue",
      "Swapchain",
  };
  for (const std::string &symbol : lockedCoreSymbols) {
    CAPTURE(symbol);
    CHECK(graphicsDoc.find(symbol) != std::string::npos);
  }

  const std::vector<std::string> lockedV1SignatureSnippets = {
      "Window([title] string, [width] i32, [height] i32)",
      "Device()",
      "Buffer<T>(count) -> Buffer<T>",
      "Device.default_queue(self)",
      "Device.create_swapchain(",
      "Device.create_mesh(",
      "Device.create_pipeline(",
      "Pipeline.material(self)",
      "Window.is_open(self) -> bool",
      "Window.poll_events(self) -> void",
      "Window.aspect_ratio(self) -> f32",
      "Swapchain.frame(self)",
      "Frame.render_pass(self, [clear_color] ColorRGBA, [clear_depth] f32)",
      "RenderPass.draw_mesh(self, [mesh] Mesh, [material] Material) -> void",
      "RenderPass.end(self) -> void",
      "Frame.submit(self, [queue] Queue) -> Result<void, GfxError>",
      "Frame.present(self) -> Result<void, GfxError>",
  };
  for (const std::string &snippet : lockedV1SignatureSnippets) {
    CAPTURE(snippet);
    CHECK(graphicsDoc.find(snippet) != std::string::npos);
  }

  const std::vector<std::string> lockedVertexColoredLayoutSnippets = {
      "| `px` | `f32` | `0` |",
      "| `py` | `f32` | `4` |",
      "| `pz` | `f32` | `8` |",
      "| `pw` | `f32` | `12` |",
      "| `r` | `f32` | `16` |",
      "| `g` | `f32` | `20` |",
      "| `b` | `f32` | `24` |",
      "| `a` | `f32` | `28` |",
      "Total size: `32` bytes.",
      "Alignment: `4` bytes.",
      "No implicit backend-added padding.",
  };
  for (const std::string &snippet : lockedVertexColoredLayoutSnippets) {
    CAPTURE(snippet);
    CHECK(graphicsDoc.find(snippet) != std::string::npos);
  }

  const std::vector<std::string> lockedGfxErrorCodes = {
      "window_create_failed",   "device_create_failed",   "swapchain_create_failed",
      "mesh_create_failed",     "pipeline_create_failed", "material_create_failed",
      "frame_acquire_failed",   "queue_submit_failed",    "frame_present_failed",
  };
  for (const std::string &code : lockedGfxErrorCodes) {
    CAPTURE(code);
    CHECK(graphicsDoc.find(code) != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-PROFILE-DEDUCTION");
    CHECK(graphicsDoc.find("Device creation must not require source-level profile literals.") != std::string::npos);
    CHECK(graphicsDoc.find("Source forms like `Device([profile] \"metal-osx\")` are out of contract for v1") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("Canonical `/std/gfx/*` contract example: `device{Device()?}` is preferred") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("Current `/std/gfx/experimental/*` status: the constructor-shaped `Window(...)`") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-WINDOW-CONSTRUCTOR-STATUS");
    CHECK(graphicsDoc.find("constructor-shaped `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
    CHECK(graphicsDoc.find("dedicated stdlib helpers on both the experimental and") !=
          std::string::npos);
    CHECK(graphicsDoc.find("canonical paths") !=
          std::string::npos);
    CHECK(primeStructDoc.find(
              "constructor-shaped experimental and canonical `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-DEVICE-CONSTRUCTOR-STATUS");
    CHECK(graphicsDoc.find("constructor-shaped `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
    CHECK(graphicsDoc.find("dedicated stdlib helpers on both the experimental and") !=
          std::string::npos);
    CHECK(graphicsDoc.find("canonical paths") !=
          std::string::npos);
    CHECK(primeStructDoc.find(
              "constructor-shaped experimental and canonical `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-BUFFER-CONSTRUCTOR-STATUS");
    CHECK(graphicsDoc.find("constructor-shaped `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
    CHECK(primeStructDoc.find(
              "constructor-shaped experimental and canonical `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-PIPELINE-CONSTRUCTOR-STATUS");
    CHECK(graphicsDoc.find("type-valued `Device.create_pipeline([vertex_type] VertexColored, ...)`") !=
          std::string::npos);
    CHECK(graphicsDoc.find("VertexColored, ...)` entry") != std::string::npos);
    CHECK(graphicsDoc.find("locked v1 vertex wire type") != std::string::npos);
    CHECK(primeStructDoc.find("`Device.create_pipeline([vertex_type] VertexColored, ...)` wrapper paths now run through") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("`Device.create_pipeline([vertex_type] VertexColored, ...)`") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-RESOURCE-WRAPPER-STATUS");
    CHECK(graphicsDoc.find("fallible `Device.create_swapchain(...)`,") != std::string::npos);
    CHECK(graphicsDoc.find("`Device.create_mesh(...)`, and `Swapchain.frame()` wrapper paths") !=
          std::string::npos);
    CHECK(primeStructDoc.find("experimental and canonical `create_swapchain(...)`, `create_mesh(...)`, `frame()`, and") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("the fallible `create_swapchain(...)`,") != std::string::npos);
    CHECK(guidelinesDoc.find("`create_mesh(...)`, and `frame()` wrapper paths now route through") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-RENDER-PASS-STATUS");
    CHECK(graphicsDoc.find("The non-Result `Frame.render_pass(...)` plus") != std::string::npos);
    CHECK(graphicsDoc.find("`RenderPass.draw_mesh(...)` / `RenderPass.end()` path now routes through") !=
          std::string::npos);
    CHECK(graphicsDoc.find("minimal pass-encoding substrate helpers while preserving deterministic") !=
          std::string::npos);
    CHECK(primeStructDoc.find("non-Result `render_pass(...)` / `draw_mesh(...)` / `end()` path now routes") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("the non-Result `render_pass(...)` /") != std::string::npos);
    CHECK(guidelinesDoc.find("`draw_mesh(...)` / `end()` path now routes through minimal pass-encoding") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-HOST-RUNTIME-STATUS");
    CHECK(graphicsDoc.find("first real") != std::string::npos);
    CHECK(graphicsDoc.find("native-desktop host/runtime path now consumes a deterministic canonical `/std/gfx/*` stream") !=
          std::string::npos);
    CHECK(graphicsDoc.find("submit/present") != std::string::npos);
    CHECK(primeStructDoc.find("shared spinning-cube native-window sample path now emits a deterministic canonical `/std/gfx/*` stream") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("shared spinning-cube native-window sample path now emits and") != std::string::npos);
    CHECK(guidelinesDoc.find("deterministic canonical `/std/gfx/*` stream") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-CONFORMANCE-STATUS");
    CHECK(graphicsDoc.find("real compile-run") != std::string::npos);
    CHECK(graphicsDoc.find("conformance programs that import both") != std::string::npos);
    CHECK(graphicsDoc.find("`/std/gfx/experimental/*` and") != std::string::npos);
    CHECK(graphicsDoc.find("`/std/gfx/*` and exercise") != std::string::npos);
    CHECK(graphicsDoc.find("across exe/vm/native") != std::string::npos);
    CHECK(primeStructDoc.find("real compile-run conformance now imports both `/std/gfx/experimental/*` and `/std/gfx/*`") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("real compile-run conformance now imports `/std/gfx/experimental/*`") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-SHARED-HOST-CONTRACT-STATUS");
    CHECK(graphicsDoc.find("shared host definition rather than duplicate ad-hoc structs per sample") !=
          std::string::npos);
    CHECK(graphicsDoc.find("one shared mapping of these identifiers rather than re-declare them in") !=
          std::string::npos);
    CHECK(primeStructDoc.find("host-side sample `GfxError` mapping plus locked `VertexColored` upload layout definitions now live in one shared example header") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("native/Metal sample hosts now share one canonical host-side `GfxError` +") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-V1-NATIVE-LAUNCHER-THINNING-STATUS");
    CHECK(graphicsDoc.find("native macOS launcher path is now a thin wrapper over a shared canonical gfx launch helper") !=
          std::string::npos);
    CHECK(primeStructDoc.find("native launcher script itself is now only a thin wrapper over a shared canonical gfx launch helper") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("native window launcher now delegates to") != std::string::npos);
    CHECK(guidelinesDoc.find("shared canonical gfx launch helper") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-NATIVE-HOST-THINNING-STATUS");
    CHECK(graphicsDoc.find("native macOS window host itself now binds its cube/software-surface callbacks onto a shared") !=
          std::string::npos);
    CHECK(graphicsDoc.find("native Metal window presenter helper") != std::string::npos);
    CHECK(primeStructDoc.find("native window host runtime shell now also lives in one shared presenter helper") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("native window host runtime shell") != std::string::npos);
    CHECK(guidelinesDoc.find("now lives in a shared presenter helper") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-METAL-HOST-LAUNCHER-THINNING-STATUS");
    CHECK(graphicsDoc.find("Metal sample path now also routes through shared helpers") != std::string::npos);
    CHECK(graphicsDoc.find("shared metal launch helper") != std::string::npos);
    CHECK(graphicsDoc.find("shared spinning-cube simulation reference helper") != std::string::npos);
    CHECK(primeStructDoc.find("Metal sample launcher now also delegates to one shared metal launch helper") !=
          std::string::npos);
    CHECK(primeStructDoc.find("shared spinning-cube simulation reference helper") != std::string::npos);
    CHECK(primeStructDoc.find("offscreen runtime shell lives in one shared helper") != std::string::npos);
    CHECK(guidelinesDoc.find("Metal launcher now delegates to") != std::string::npos);
    CHECK(guidelinesDoc.find("shared metal launch helper") != std::string::npos);
    CHECK(guidelinesDoc.find("shared spinning-cube simulation reference helper") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-BROWSER-HOST-LAUNCHER-THINNING-STATUS");
    CHECK(graphicsDoc.find("browser runtime shell now lives in") != std::string::npos);
    CHECK(graphicsDoc.find("browser sample launcher now delegates to") != std::string::npos);
    CHECK(primeStructDoc.find("browser sample launcher now also delegates to one shared browser launch helper") !=
          std::string::npos);
    CHECK(primeStructDoc.find("browser_runtime_shared.js") != std::string::npos);
    CHECK(guidelinesDoc.find("shared browser launch helper") != std::string::npos);
    CHECK(guidelinesDoc.find("shared JS helper") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-CANONICAL-STDLIB-STATUS");
    CHECK(graphicsDoc.find("first canonical `/std/gfx/*` stdlib surface") != std::string::npos);
    CHECK(primeStructDoc.find(
              "constructor-shaped experimental and canonical `Window(...)`, `Device()`, and `Buffer<T>(count)` entry points") !=
          std::string::npos);
    CHECK(guidelinesDoc.find("The canonical `/std/gfx/*` entry") != std::string::npos);
    CHECK(guidelinesDoc.find("real compile-run conformance across exe/vm/native") != std::string::npos);
  }

  {
    CAPTURE("GFX-V1-RESULT-PROPAGATION");
    CHECK(graphicsDoc.find("Examples and conformance for this mini-spec must use `Result` propagation (`?`)") !=
          std::string::npos);
    CHECK(graphicsDoc.find("plus `on_error<...>` handlers") != std::string::npos);
    CHECK(guidelinesDoc.find("on_error<GfxError, /log_gfx_error>") != std::string::npos);
    CHECK(guidelinesDoc.find("frame.submit(queue)?") != std::string::npos);
    CHECK(guidelinesDoc.find("frame.present()?") != std::string::npos);
  }

  {
    CAPTURE("GFX-CORE-NO-EXT-NS");
    const std::string source = R"(
import /std/gfx/ext/*

[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("gfx_contract_no_ext_namespace.prime", source);
    const std::string errPath =
        (testScratchPath("") / "primec_gfx_contract_no_ext_namespace.err.json").string();
    const std::string command = "./primec --emit=vm " + quoteShellArg(srcPath) +
                                " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(command) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("unknown import path: /std/gfx/ext/*") != std::string::npos);
  }

  {
    CAPTURE("GFX-PROFILE-IDENTIFIERS");
    const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("gfx_contract_profile_identifier_reject.prime", source);
    const std::string errPath =
        (testScratchPath("") / "primec_gfx_contract_profile_identifier_reject.err.txt").string();
    const std::string command = "./primec --emit=wasm --wasm-profile webgpu-browser " + quoteShellArg(srcPath) +
                                " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(command) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("unsupported --wasm-profile value: webgpu-browser (expected wasi|browser)") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-PROFILE-GATING");
    CAPTURE("GFX-DIAG-PROFILE-CONTEXT");
    const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("gfx_contract_profile_gating_reject.prime", source);
    const std::string wasmPath =
        (testScratchPath("") / "primec_gfx_contract_profile_gating_reject.wasm").string();
    const std::string errPath =
        (testScratchPath("") / "primec_gfx_contract_profile_gating_reject.err.json").string();
    const std::string command = "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main --emit-diagnostics 2> " +
                                quoteShellArg(errPath);
    CHECK(runCommand(command) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*") !=
          std::string::npos);
  }

  {
    CAPTURE("GFX-CORE-API-NAMESPACE");
    const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  return(0i32)
}
)";
    const std::string srcPath = writeTemp("gfx_contract_backend_emit_reject.prime", source);
    const std::string errPath =
        (testScratchPath("") / "primec_gfx_contract_backend_emit_reject.err.json").string();
    const std::string outPath =
        (testScratchPath("") / "primec_gfx_contract_backend_emit_reject.glsl").string();
    const std::string command = "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
    CHECK(runCommand(command) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/*") !=
          std::string::npos);
  }
}

TEST_CASE("rejects stdlib version flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_stdlib_version_invalid.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_stdlib_version_err.txt").string();

  const std::string compileCmd =
      "./primec --stdlib-version=1.2.0 " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("primec and primevm usage prefer text transforms and import flags") {
  const std::string primecErrPath =
      (testScratchPath("") / "primec_usage_modern_flags_err.txt").string();
  const std::string primevmErrPath =
      (testScratchPath("") / "primevm_usage_modern_flags_err.txt").string();

  CHECK(runCommand("./primec --unknown-option 2> " + quoteShellArg(primecErrPath)) == 2);
  const std::string primecErr = readFile(primecErrPath);
  CHECK(primecErr.find("Usage: primec") != std::string::npos);
  CHECK(primecErr.find("[--emit=cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir]") !=
        std::string::npos);
  CHECK(primecErr.find("[--import-path <dir>] [-I <dir>]") != std::string::npos);
  CHECK(primecErr.find("[--wasm-profile wasi|browser]") != std::string::npos);
  CHECK(primecErr.find("[--text-transforms <list>]") != std::string::npos);
  CHECK(primecErr.find("[--ir-inline]") != std::string::npos);
  CHECK(primecErr.find("--text-filters <list>") == std::string::npos);

  CHECK(runCommand("./primevm --unknown-option 2> " + quoteShellArg(primevmErrPath)) == 2);
  const std::string primevmErr = readFile(primevmErrPath);
  CHECK(primevmErr.find("Usage: primevm") != std::string::npos);
  CHECK(primevmErr.find("[--import-path <dir>] [-I <dir>]") != std::string::npos);
  CHECK(primevmErr.find("[--text-transforms <list>]") != std::string::npos);
  CHECK(primevmErr.find("[--ir-inline]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-json]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-json-snapshots [none|stop|all]]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-trace <path>]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-dap]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-replay <trace>]") != std::string::npos);
  CHECK(primevmErr.find("[--debug-replay-sequence <n>]") != std::string::npos);
  CHECK(primevmErr.find("--text-filters <list>") == std::string::npos);
}

TEST_CASE("primec and primevm accept ir inline flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_inline_flag.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_ir_inline_flag_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --ir-inline";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main --ir-inline";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("primevm accepts explicit emit vm compatibility flag") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("primevm_emit_vm_flag.prime", source);
  const std::string runVmCmd = "./primevm --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);
}

TEST_CASE("primevm debug-json emits stable NDJSON schema") {
  const std::string source = R"(
[return<int>]
double() {
  return(plus(4i32, 4i32))
}

[return<int>]
main() {
  return(plus(double(), 1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_schema.prime", source);
  const std::string outPathA =
      (testScratchPath("") / "primevm_debug_json_schema_a.ndjson").string();
  const std::string outPathB =
      (testScratchPath("") / "primevm_debug_json_schema_b.ndjson").string();

  const std::string cmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-json > " + quoteShellArg(outPathA);
  const std::string cmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-json > " + quoteShellArg(outPathB);
  CHECK(runCommand(cmdA) == 9);
  CHECK(runCommand(cmdB) == 9);

  const std::string outA = readFile(outPathA);
  const std::string outB = readFile(outPathB);
  CHECK(outA == outB);
  CHECK(outA.find("Usage: primevm") == std::string::npos);

  std::vector<std::string> lines;
  std::stringstream stream(outA);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 6);

  CHECK(lines.front().find("\"version\":1") != std::string::npos);
  CHECK(lines.front().find("\"event\":\"session_start\"") != std::string::npos);
  CHECK(lines.front().find("\"snapshot\":{") != std::string::npos);

  bool sawBefore = false;
  bool sawAfter = false;
  bool sawCallPush = false;
  bool sawCallPop = false;
  for (const auto &entry : lines) {
    CHECK(entry.front() == '{');
    CHECK(entry.back() == '}');
    if (entry.find("\"event\":\"before_instruction\"") != std::string::npos) {
      sawBefore = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"opcode\":") != std::string::npos);
      CHECK(entry.find("\"immediate\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"after_instruction\"") != std::string::npos) {
      sawAfter = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"opcode\":") != std::string::npos);
      CHECK(entry.find("\"immediate\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"call_push\"") != std::string::npos) {
      sawCallPush = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"function_index\":") != std::string::npos);
      CHECK(entry.find("\"returns_value_to_caller\":") != std::string::npos);
    }
    if (entry.find("\"event\":\"call_pop\"") != std::string::npos) {
      sawCallPop = true;
      CHECK(entry.find("\"sequence\":") != std::string::npos);
      CHECK(entry.find("\"snapshot\":{") != std::string::npos);
      CHECK(entry.find("\"function_index\":") != std::string::npos);
      CHECK(entry.find("\"returns_value_to_caller\":") != std::string::npos);
    }
  }
  CHECK(sawBefore);
  CHECK(sawAfter);
  const bool sawCallEventsOrInstructionEvents = sawCallPush || sawCallPop || (sawBefore && sawAfter);
  CHECK(sawCallEventsOrInstructionEvents);

  CHECK(lines.back().find("\"event\":\"stop\"") != std::string::npos);
  CHECK(lines.back().find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(lines.back().find("\"snapshot\":{") != std::string::npos);
  CHECK(lines.back().find("\"result\":9") != std::string::npos);
}

TEST_CASE("primevm debug-json snapshots include payloads across step boundaries") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{3i32}
  assign(value, plus(value, 4i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_payloads.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primevm_debug_json_snapshot_payloads.ndjson").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-json --debug-json-snapshots=all > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 7);

  const std::string output = readFile(outPath);
  std::vector<std::string> lines;
  std::stringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 4);

  bool sawPayload = false;
  bool sawNonEmptyLocals = false;
  bool sawNonEmptyOperandStack = false;
  bool sawAdvancedInstructionPointer = false;
  uint64_t firstPayloadIp = 0;
  bool firstPayloadIpSet = false;

  for (const auto &entry : lines) {
    if (entry.find("\"snapshot_payload\":{") == std::string::npos) {
      continue;
    }
    sawPayload = true;
    CHECK(entry.find("\"instruction_pointer\":") != std::string::npos);
    CHECK(entry.find("\"call_stack\":[") != std::string::npos);
    CHECK(entry.find("\"frame_locals\":[") != std::string::npos);
    CHECK(entry.find("\"current_frame_locals\":[") != std::string::npos);
    CHECK(entry.find("\"operand_stack\":[") != std::string::npos);

    if (entry.find("\"current_frame_locals\":[]") == std::string::npos) {
      sawNonEmptyLocals = true;
    }
    if (entry.find("\"operand_stack\":[]") == std::string::npos) {
      sawNonEmptyOperandStack = true;
    }

    const size_t ipKey = entry.find("\"snapshot_payload\":{\"instruction_pointer\":");
    if (ipKey != std::string::npos) {
      const size_t valueStart = ipKey + std::string("\"snapshot_payload\":{\"instruction_pointer\":").size();
      const size_t valueEnd = entry.find(",", valueStart);
      REQUIRE(valueEnd != std::string::npos);
      const uint64_t payloadIp = static_cast<uint64_t>(std::stoull(entry.substr(valueStart, valueEnd - valueStart)));
      if (!firstPayloadIpSet) {
        firstPayloadIp = payloadIp;
        firstPayloadIpSet = true;
      } else if (payloadIp != firstPayloadIp) {
        sawAdvancedInstructionPointer = true;
      }
    }
  }

  CHECK(sawPayload);
  CHECK(sawNonEmptyLocals);
  CHECK(sawNonEmptyOperandStack);
  CHECK(sawAdvancedInstructionPointer);
  CHECK(lines.back().find("\"event\":\"stop\"") != std::string::npos);
  CHECK(lines.back().find("\"snapshot_payload\":{") != std::string::npos);
}

TEST_CASE("primevm debug-json snapshots mode requires debug-json") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_requires_mode.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_debug_json_snapshot_requires_mode.err").string();
  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-json-snapshots=all 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-json-snapshots requires --debug-json") != std::string::npos);
}

TEST_CASE("primevm rejects invalid debug-json snapshots mode") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_json_snapshot_invalid_mode.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_debug_json_snapshot_invalid_mode.err").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --debug-json --debug-json-snapshots=weird 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unsupported --debug-json-snapshots value: weird (expected none|stop|all)") !=
        std::string::npos);
}

TEST_CASE("primec rejects debug-json option") {
  const std::string errPath =
      (testScratchPath("") / "primec_reject_debug_json_err.txt").string();
  const std::string cmd = "./primec --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unknown option: --debug-json") != std::string::npos);
  CHECK(diagnostics.find("Usage: primec") != std::string::npos);

  const std::string snapshotErrPath =
      (testScratchPath("") / "primec_reject_debug_json_snapshots_err.txt").string();
  const std::string snapshotCmd = "./primec --debug-json-snapshots=all 2> " + quoteShellArg(snapshotErrPath);
  CHECK(runCommand(snapshotCmd) == 2);
  const std::string snapshotDiagnostics = readFile(snapshotErrPath);
  CHECK(snapshotDiagnostics.find("unknown option: --debug-json-snapshots=all") != std::string::npos);

  const std::string dapErrPath =
      (testScratchPath("") / "primec_reject_debug_dap_err.txt").string();
  const std::string dapCmd = "./primec --debug-dap 2> " + quoteShellArg(dapErrPath);
  CHECK(runCommand(dapCmd) == 2);
  const std::string dapDiagnostics = readFile(dapErrPath);
  CHECK(dapDiagnostics.find("unknown option: --debug-dap") != std::string::npos);

  const std::string traceErrPath =
      (testScratchPath("") / "primec_reject_debug_trace_err.txt").string();
  const std::string traceCmd = "./primec --debug-trace=trace.ndjson 2> " + quoteShellArg(traceErrPath);
  CHECK(runCommand(traceCmd) == 2);
  const std::string traceDiagnostics = readFile(traceErrPath);
  CHECK(traceDiagnostics.find("unknown option: --debug-trace=trace.ndjson") != std::string::npos);

  const std::string replayErrPath =
      (testScratchPath("") / "primec_reject_debug_replay_err.txt").string();
  const std::string replayCmd = "./primec --debug-replay=trace.ndjson 2> " + quoteShellArg(replayErrPath);
  CHECK(runCommand(replayCmd) == 2);
  const std::string replayDiagnostics = readFile(replayErrPath);
  CHECK(replayDiagnostics.find("unknown option: --debug-replay=trace.ndjson") != std::string::npos);

  const std::string replaySequenceErrPath =
      (testScratchPath("") / "primec_reject_debug_replay_sequence_err.txt").string();
  const std::string replaySequenceCmd =
      "./primec --debug-replay-sequence=7 2> " + quoteShellArg(replaySequenceErrPath);
  CHECK(runCommand(replaySequenceCmd) == 2);
  const std::string replaySequenceDiagnostics = readFile(replaySequenceErrPath);
  CHECK(replaySequenceDiagnostics.find("unknown option: --debug-replay-sequence=7") != std::string::npos);
}

TEST_CASE("primevm debug-trace requires path and mode exclusivity") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_trace_errors.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_trace_errors.err").string();

  const std::string missingPathCmd = "./primevm " + quoteShellArg(srcPath) + " --debug-trace 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingPathCmd) == 2);
  std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-trace requires an output path") != std::string::npos);

  const std::string conflictJsonCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-trace=trace.ndjson --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictJsonCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-json cannot be combined with --debug-trace") != std::string::npos);

  const std::string conflictDapCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-trace=trace.ndjson --debug-dap 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictDapCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-dap cannot be combined with --debug-trace") != std::string::npos);
}

TEST_CASE("primevm debug-trace writes deterministic complete event logs") {
  const std::string source = R"(
[return<int>]
double() {
  return(plus(4i32, 4i32))
}

[return<int>]
main() {
  return(plus(double(), 1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_trace_deterministic.prime", source);
  const std::string tracePathA =
      (testScratchPath("") / "primevm_debug_trace_deterministic_a.ndjson").string();
  const std::string tracePathB =
      (testScratchPath("") / "primevm_debug_trace_deterministic_b.ndjson").string();

  const std::string cmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePathA);
  const std::string cmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePathB);
  CHECK(runCommand(cmdA) == 9);
  CHECK(runCommand(cmdB) == 9);

  const std::string traceA = readFile(tracePathA);
  const std::string traceB = readFile(tracePathB);
  CHECK(traceA == traceB);
  CHECK(traceA.find("\"event\":\"session_start\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"before_instruction\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"after_instruction\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"call_pop\"") != std::string::npos);
  CHECK(traceA.find("\"event\":\"stop\"") != std::string::npos);
  CHECK(traceA.find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(traceA.find("\"snapshot_payload\":{") != std::string::npos);
  CHECK(traceA.find("\"frame_locals\":[") != std::string::npos);

  std::vector<std::string> lines;
  std::stringstream stream(traceA);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  REQUIRE(lines.size() >= 6);

  int64_t lastSequence = -1;
  size_t observedSequenceCount = 0;
  for (const std::string &entry : lines) {
    const size_t key = entry.find("\"sequence\":");
    if (key == std::string::npos) {
      continue;
    }
    const size_t start = key + std::string("\"sequence\":").size();
    size_t end = start;
    while (end < entry.size() && std::isdigit(static_cast<unsigned char>(entry[end])) != 0) {
      ++end;
    }
    REQUIRE(end > start);
    const int64_t sequence = std::stoll(entry.substr(start, end - start));
    if (lastSequence >= 0) {
      CHECK(sequence == lastSequence + 1);
    }
    lastSequence = sequence;
    ++observedSequenceCount;
  }
  CHECK(observedSequenceCount > 0);
}

TEST_CASE("primevm debug-replay requires trace and mode exclusivity") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_replay_errors.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_replay_errors.err").string();

  const std::string missingPathCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingPathCmd) == 2);
  std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay requires an input trace path") != std::string::npos);

  const std::string conflictJsonCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay=trace.ndjson --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictJsonCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-json") != std::string::npos);

  const std::string conflictDapCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay=trace.ndjson --debug-dap 2> " + quoteShellArg(errPath);
  CHECK(runCommand(conflictDapCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-dap") != std::string::npos);

  const std::string conflictTraceCmd = "./primevm " + quoteShellArg(srcPath) +
                                       " --debug-replay=trace.ndjson --debug-trace=trace_out.ndjson 2> " +
                                       quoteShellArg(errPath);
  CHECK(runCommand(conflictTraceCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay cannot be combined with --debug-trace") != std::string::npos);

  const std::string missingReplayCmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-replay-sequence=3 2> " + quoteShellArg(errPath);
  CHECK(runCommand(missingReplayCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-replay-sequence requires --debug-replay") != std::string::npos);

  const std::string invalidSequenceCmd = "./primevm " + quoteShellArg(srcPath) +
                                         " --debug-replay=trace.ndjson --debug-replay-sequence=abc 2> " +
                                         quoteShellArg(errPath);
  CHECK(runCommand(invalidSequenceCmd) == 2);
  diagnostics = readFile(errPath);
  CHECK(diagnostics.find("invalid --debug-replay-sequence value: abc") != std::string::npos);
}

TEST_CASE("primevm debug-replay restores checkpoint snapshots at requested sequence") {
  const auto extractUnsignedField = [](const std::string &json, const std::string &key, uint64_t &outValue) -> bool {
    const std::string needle = "\"" + key + "\":";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
      return false;
    }
    const size_t valueStart = keyPos + needle.size();
    if (valueStart >= json.size() || std::isdigit(static_cast<unsigned char>(json[valueStart])) == 0) {
      return false;
    }
    size_t valueEnd = valueStart;
    while (valueEnd < json.size() && std::isdigit(static_cast<unsigned char>(json[valueEnd])) != 0) {
      ++valueEnd;
    }
    try {
      outValue = static_cast<uint64_t>(std::stoull(json.substr(valueStart, valueEnd - valueStart)));
      return true;
    } catch (...) {
      return false;
    }
  };
  const auto extractObjectField = [](const std::string &json, const std::string &key, std::string &outValue) -> bool {
    const std::string needle = "\"" + key + "\":";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
      return false;
    }
    const size_t objectStart = keyPos + needle.size();
    if (objectStart >= json.size() || json[objectStart] != '{') {
      return false;
    }
    size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = objectStart; i < json.size(); ++i) {
      const char c = json[i];
      if (inString) {
        if (!escaped && c == '"') {
          inString = false;
        }
        if (!escaped && c == '\\') {
          escaped = true;
        } else {
          escaped = false;
        }
        continue;
      }
      if (c == '"') {
        inString = true;
        escaped = false;
        continue;
      }
      if (c == '{') {
        ++depth;
      } else if (c == '}') {
        if (depth == 0) {
          return false;
        }
        --depth;
        if (depth == 0) {
          outValue = json.substr(objectStart, i - objectStart + 1);
          return true;
        }
      }
    }
    return false;
  };

  const std::string source = R"(
[return<int>]
double() {
  return(plus(4i32, 4i32))
}

[return<int>]
main() {
  return(plus(double(), 1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_replay_equivalence.prime", source);
  const std::string tracePath =
      (testScratchPath("") / "primevm_debug_replay_equivalence_trace.ndjson").string();
  const std::string replayPath =
      (testScratchPath("") / "primevm_debug_replay_equivalence_replay.ndjson").string();

  const std::string traceCmd =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePath);
  CHECK(runCommand(traceCmd) == 9);

  const std::string trace = readFile(tracePath);
  std::stringstream traceStream(trace);
  std::string traceLine;
  uint64_t checkpointSequence = 0;
  std::string checkpointSnapshot;
  std::string checkpointSnapshotPayload;
  bool foundCheckpoint = false;
  while (std::getline(traceStream, traceLine)) {
    if (traceLine.find("\"event\":\"before_instruction\"") == std::string::npos) {
      continue;
    }
    if (!extractUnsignedField(traceLine, "sequence", checkpointSequence)) {
      continue;
    }
    if (!extractObjectField(traceLine, "snapshot", checkpointSnapshot)) {
      continue;
    }
    if (!extractObjectField(traceLine, "snapshot_payload", checkpointSnapshotPayload)) {
      continue;
    }
    foundCheckpoint = true;
    break;
  }
  REQUIRE(foundCheckpoint);

  const std::string replayCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " +
                                quoteShellArg(tracePath) + " --debug-replay-sequence " +
                                std::to_string(checkpointSequence) + " > " + quoteShellArg(replayPath);
  CHECK(runCommand(replayCmd) == 0);

  const std::string replayOutput = readFile(replayPath);
  std::stringstream replayStream(replayOutput);
  std::string replayLine;
  bool foundReplayLine = false;
  while (std::getline(replayStream, replayLine)) {
    if (!replayLine.empty()) {
      foundReplayLine = true;
      break;
    }
  }
  REQUIRE(foundReplayLine);

  CHECK(replayLine.find("\"event\":\"replay_checkpoint\"") != std::string::npos);
  CHECK(replayLine.find("\"target_sequence\":" + std::to_string(checkpointSequence)) != std::string::npos);
  CHECK(replayLine.find("\"checkpoint_sequence\":" + std::to_string(checkpointSequence)) != std::string::npos);

  std::string replaySnapshot;
  std::string replaySnapshotPayload;
  REQUIRE(extractObjectField(replayLine, "snapshot", replaySnapshot));
  REQUIRE(extractObjectField(replayLine, "snapshot_payload", replaySnapshotPayload));
  CHECK(replaySnapshot == checkpointSnapshot);
  CHECK(replaySnapshotPayload == checkpointSnapshotPayload);
}

TEST_CASE("primevm debug-replay is deterministic and rejects invalid traces") {
  const std::string source = R"(
[return<int>]
double() {
  return(plus(4i32, 4i32))
}

[return<int>]
main() {
  return(plus(double(), 1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_replay_determinism.prime", source);
  const std::string tracePath =
      (testScratchPath("") / "primevm_debug_replay_determinism_trace.ndjson").string();
  const std::string replayPathA =
      (testScratchPath("") / "primevm_debug_replay_determinism_a.ndjson").string();
  const std::string replayPathB =
      (testScratchPath("") / "primevm_debug_replay_determinism_b.ndjson").string();
  const std::string errPath = (testScratchPath("") / "primevm_debug_replay_determinism.err").string();

  const std::string traceCmd =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-trace " + quoteShellArg(tracePath);
  CHECK(runCommand(traceCmd) == 9);

  const std::string replayCmdA =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " + quoteShellArg(tracePath) + " > " +
      quoteShellArg(replayPathA);
  const std::string replayCmdB =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " + quoteShellArg(tracePath) + " > " +
      quoteShellArg(replayPathB);
  CHECK(runCommand(replayCmdA) == 9);
  CHECK(runCommand(replayCmdB) == 9);

  const std::string replayA = readFile(replayPathA);
  const std::string replayB = readFile(replayPathB);
  CHECK(replayA == replayB);
  CHECK(replayA.find("\"event\":\"replay_checkpoint\"") != std::string::npos);
  CHECK(replayA.find("\"checkpoint_event\":\"stop\"") != std::string::npos);
  CHECK(replayA.find("\"reason\":\"Exit\"") != std::string::npos);
  CHECK(replayA.find("\"snapshot\":{") != std::string::npos);
  CHECK(replayA.find("\"snapshot_payload\":{") != std::string::npos);

  const std::string invalidTracePath = writeTemp("primevm_debug_replay_invalid_trace.ndjson", "{\"version\":1}\n");
  const std::string invalidReplayCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-replay " +
                                       quoteShellArg(invalidTracePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(invalidReplayCmd) == 3);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("replay trace has no checkpoint-capable events") != std::string::npos);
}

TEST_CASE("primevm debug-dap rejects incompatible debug-json mode") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_conflict.prime", source);
  const std::string errPath = (testScratchPath("") / "primevm_debug_dap_conflict.err").string();
  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --debug-dap --debug-json 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("--debug-dap cannot be combined with --debug-json") != std::string::npos);
}

TEST_CASE("primevm debug-dap emits deterministic framed transcripts") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{1i32}
  assign(value, plus(value, 1i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_transcript.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[{"line":4,"column":3}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"configurationDone","arguments":{}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"scopes","arguments":{"frameId":1}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"variables","arguments":{"variablesReference":257}})") +
                               makeDapFrame(R"({"seq":9,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_transcript.requests", requests);
  const std::string outPathA =
      (testScratchPath("") / "primevm_debug_dap_transcript_a.out").string();
  const std::string outPathB =
      (testScratchPath("") / "primevm_debug_dap_transcript_b.out").string();

  const std::string cmdA = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathA);
  const std::string cmdB = "./primevm " + quoteShellArg(srcPath) +
                           " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPathB);
  CHECK(runCommand(cmdA) == 0);
  CHECK(runCommand(cmdB) == 0);

  const std::string outA = readFile(outPathA);
  const std::string outB = readFile(outPathB);
  CHECK(outA == outB);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(outA, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 13);

  CHECK(frames[0].find("\"type\":\"response\"") != std::string::npos);
  CHECK(frames[0].find("\"command\":\"initialize\"") != std::string::npos);
  CHECK(frames[1].find("\"command\":\"launch\"") != std::string::npos);
  CHECK(frames[2].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[2].find("\"event\":\"initialized\"") != std::string::npos);
  CHECK(frames[3].find("\"type\":\"event\"") != std::string::npos);
  CHECK(frames[3].find("\"event\":\"stopped\"") != std::string::npos);
  CHECK(frames[3].find("\"reason\":\"entry\"") != std::string::npos);
  CHECK(frames[4].find("\"command\":\"setBreakpoints\"") != std::string::npos);
  CHECK(frames[4].find("\"verified\":false") != std::string::npos);
  CHECK(frames[4].find("source breakpoint not found") != std::string::npos);
  CHECK(frames[6].find("\"command\":\"continue\"") != std::string::npos);
  CHECK(frames[7].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[8].find("\"event\":\"terminated\"") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[9].find("\"totalFrames\":0") != std::string::npos);
  CHECK(frames[10].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[10].find("invalid frame id") != std::string::npos);
  CHECK(frames[11].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[11].find("invalid variable reference") != std::string::npos);
  CHECK(frames[12].find("\"command\":\"disconnect\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap end-to-end process smoke emits exit events") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_smoke.prime", source);
  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_smoke.requests", requests);
  const std::string outPath = (testScratchPath("") / "primevm_debug_dap_smoke.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  const std::string output = readFile(outPath);
  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(output, framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 8);
  CHECK(frames[5].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[5].find("\"exitCode\":9") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"terminated\"") != std::string::npos);
}

TEST_CASE("primevm debug-dap exposes non-top frame locals in variables") {
  const std::string source = R"(
[return<int>]
helper() {
  [int mut] inner{7i32}
  return(inner)
}

[return<int>]
main() {
  [int mut] outer{9i32}
  return(helper())
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_non_top_locals.prime", source);

  const std::string requests = makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
                               makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
                               makeDapFrame(R"({"seq":3,"type":"request","command":"setInstructionBreakpoints","arguments":{"breakpoints":[{"instructionReference":"f1:ip2"}]}})") +
                               makeDapFrame(R"({"seq":4,"type":"request","command":"continue","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":5,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
                               makeDapFrame(R"({"seq":6,"type":"request","command":"scopes","arguments":{"frameId":2}})") +
                               makeDapFrame(R"({"seq":7,"type":"request","command":"variables","arguments":{"variablesReference":513}})") +
                               makeDapFrame(R"({"seq":8,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_non_top_locals.requests", requests);
  const std::string outPath = (testScratchPath("") / "primevm_debug_dap_non_top_locals.out").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --entry /main --debug-dap < " + quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(readFile(outPath), framingOk);
  CHECK(framingOk);
  REQUIRE(frames.size() == 12);
  CHECK(frames[4].find("\"command\":\"setInstructionBreakpoints\"") != std::string::npos);
  CHECK(frames[4].find("invalid breakpoint instruction pointer") != std::string::npos);
  CHECK(frames[6].find("\"event\":\"exited\"") != std::string::npos);
  CHECK(frames[7].find("\"event\":\"terminated\"") != std::string::npos);
  CHECK(frames[8].find("\"command\":\"stackTrace\"") != std::string::npos);
  CHECK(frames[8].find("\"totalFrames\":0") != std::string::npos);
  CHECK(frames[9].find("\"command\":\"scopes\"") != std::string::npos);
  CHECK(frames[9].find("invalid frame id") != std::string::npos);
  CHECK(frames[10].find("\"command\":\"variables\"") != std::string::npos);
  CHECK(frames[10].find("invalid variable reference") != std::string::npos);
  CHECK(frames[11].find("\"command\":\"disconnect\"") != std::string::npos);
}

TEST_CASE("primevm rejects primec output flags") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_reject_output_flags.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_reject_output_flags_err.txt").string();

  const std::string outputPathCmd = "./primevm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(outputPathCmd) == 2);
  const std::string outputPathErr = readFile(errPath);
  CHECK(outputPathErr.find("unknown option: -o") != std::string::npos);
  CHECK(outputPathErr.find("Usage: primevm") != std::string::npos);

  const std::string outDirCmd = "./primevm " + srcPath + " --out-dir /tmp --entry /main 2> " + errPath;
  CHECK(runCommand(outDirCmd) == 2);
  const std::string outDirErr = readFile(errPath);
  CHECK(outDirErr.find("unknown option: --out-dir") != std::string::npos);
  CHECK(outDirErr.find("Usage: primevm") != std::string::npos);
}

TEST_CASE("wasm runtime tooling hook executes or reports explicit skip") {
  const std::filesystem::path scriptPath =
      std::filesystem::current_path().parent_path() / "scripts" / "run_wasm_runtime_checks.sh";
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::string outPath =
      (testScratchPath("") / "primec_wasm_runtime_hook_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_wasm_runtime_hook_err.txt").string();

  const std::string command = quoteShellArg(scriptPath.string()) + " --build-dir " +
                              quoteShellArg(std::filesystem::current_path().string()) + " --primec ./primec > " +
                              quoteShellArg(outPath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.empty());
  if (hasWasmtime()) {
    CHECK(output.find("PASS: executed wasm runtime checks with wasmtime.") != std::string::npos);
  } else {
    CHECK(output.find("SKIP: wasmtime unavailable; wasm runtime checks not executed.") != std::string::npos);
  }
}

TEST_CASE("defaults to native output with stem name") {
  const std::string source = R"(
[return<int>]
main() {
  return(5i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_output.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec " + quoteShellArg(srcPath) + " --out-dir " + quoteShellArg(outDir.string()) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  CHECK(std::filesystem::exists(outputPath));
  CHECK(runCommand(quoteShellArg(outputPath.string())) == 5);
}

TEST_CASE("emits PSIR bytecode with --emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_ir.prime", source);
  const std::string irPath = (testScratchPath("") / "primec_emit_ir.psir").string();

  const std::string compileCmd =
      "./primec --emit=ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(irPath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(irPath));

  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data;
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
  }
  CHECK(!data.empty());
  REQUIRE(data.size() >= 8);
  auto readU32 = [&](size_t offset) -> uint32_t {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  };
  const uint32_t magic = readU32(0);
  const uint32_t version = readU32(4);
  CHECK(magic == 0x50534952u);
  CHECK(version == 21u);

  primec::IrModule module;
  std::string error;
  CHECK(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
}

TEST_CASE("primevm forwards entry args") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_args.prime", source);
  const std::string outPath = (testScratchPath("") / "primevm_args_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("primevm supports argv string bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{args[1i32]}
  print_line(first)
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_binding.prime", source);
  const std::string outPath = (testScratchPath("") / "primevm_args_binding_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs with line comments after expressions") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}// comment with a+b and a/b should be ignored
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_line_comment.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_line_comment_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_line_comment_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{'{"k":"v"}'utf8}
  [i32] k{at(text, 2i32)}
  [i32] v{at_unsafe(text, 6i32)}
  [i32] len{count(text)}
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs method calls via type namespaces") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_call_i32.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_method_call_i32_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_method_call_i32_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs count forwarding to method") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 4i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_forward.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_count_forward_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_count_forward_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs method call on constructor") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/ping([Foo] self) {
  return(9i32)
}

[return<int>]
main() {
  return(Foo().ping())
}
)";
  const std::string srcPath = writeTemp("compile_constructor_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_constructor_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_constructor_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs call with body block") {
  const std::string source = R"(
[return<int>]
sum([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  [i32 mut] value{0i32}
  [i32] total{ sum(2i32, 3i32) { assign(value, 7i32) } }
  return(plus(total, value))
}
)";
  const std::string srcPath = writeTemp("compile_call_body_block.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_call_body_block_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_call_body_block_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 12);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 12);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 12);
}

TEST_CASE("compiles and runs templated method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  pick<T>([i32] self, [T] other) {
    return(self)
  }
}

[return<int>]
main() {
  return(3i32.pick<int>(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_template_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_template_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_template_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    [i32] value{1i32}
    return(plus(value, 6i32))
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_block_expr_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_block_expr_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs boolean ops with conversions") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(convert<bool>(1i32), or(convert<bool>(0i32), not(convert<bool>(0i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_int.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_bool_ops_int_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_bool_ops_int_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs integer width converts") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(-1i32), 18446744073709551615u64))
}
)";
  const std::string srcPath = writeTemp("compile_integer_width_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_integer_width_convert_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_integer_width_convert_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs convert bool from negative integer") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_negative.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_convert_bool_negative_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_convert_bool_negative_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs boolean ops short-circuit") {
  const std::string source = R"(
[return<int>]
main() {
  [bool mut] witness{false}
  or(true, assign(witness, true))
  and(false, assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_short_circuit.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_bool_ops_short_circuit_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_bool_ops_short_circuit_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_SUITE_END();
