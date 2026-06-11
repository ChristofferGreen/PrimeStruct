#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("gfx compatibility shim type surface imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Window] window{Window{[token] 7i32, [width] 1280i32, [height] 720i32}}
  [ColorFormat] colorFormat{ColorFormat{[value] 0i32}}
  [DepthFormat] depthFormat{DepthFormat{[value] 0i32}}
  [PresentMode] presentMode{PresentMode{[value] 0i32}}
  [ShaderLibrary] shader{ShaderLibrary{[value] 0i32}}
  [Buffer<i32>] buffer{[token] 3i32, [elementCount] 4i32}
  [Texture2D<i32>] texture{[token] 5i32, [width] 64i32, [height] 32i32}
  [Swapchain] swapchain{Swapchain{[token] 11i32}}
  [Mesh] mesh{Mesh{[token] 13i32, [vertexCount] 8i32, [indexCount] 36i32}}
  [VertexColored] vertex{
    VertexColored{
      [px] 1.0f32,
      [py] 2.0f32,
      [pz] 3.0f32,
      [pw] 1.0f32,
      [r] 0.25f32,
      [g] 0.50f32,
      [b] 0.75f32,
      [a] 1.0f32
    }
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
  [Window] window{Window{[token] 7i32, [width] 1280i32, [height] 720i32}}
  [ColorFormat] colorFormat{ColorFormat{[value] 0i32}}
  [DepthFormat] depthFormat{DepthFormat{[value] 0i32}}
  [PresentMode] presentMode{PresentMode{[value] 0i32}}
  [ShaderLibrary] shader{ShaderLibrary{[value] 0i32}}
  [Buffer<i32>] buffer{[token] 3i32, [elementCount] 4i32}
  [Texture2D<i32>] texture{[token] 5i32, [width] 64i32, [height] 32i32}
  [Swapchain] swapchain{Swapchain{[token] 11i32}}
  [Mesh] mesh{Mesh{[token] 13i32, [vertexCount] 8i32, [indexCount] 36i32}}
  [VertexColored] vertex{
    VertexColored{
      [px] 1.0f32,
      [py] 2.0f32,
      [pz] 3.0f32,
      [pw] 1.0f32,
      [r] 0.25f32,
      [g] 0.50f32,
      [b] 0.75f32,
      [a] 1.0f32
    }
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

TEST_CASE("gfx compatibility shim error helper imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [string] whyText{GfxError.why(err)}
  [Window] window{Window{[token] 1i32, [width] 1280i32, [height] 720i32}}
  [ColorFormat] colorFormat{ColorFormat{[value] 0i32}}
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

TEST_CASE("gfx compatibility shim substrate boundary imports across backends") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [SubstrateWindowConfig] windowConfig{
    SubstrateWindowConfig{[hostToken] 11i32, [width] 1280i32, [height] 720i32}
  }
  [i32] windowToken{GraphicsSubstrate.createWindow(windowConfig)?}
  [Window] window{Window{[token] windowToken, [width] windowConfig.width, [height] windowConfig.height}}
  [SubstrateDeviceConfig] deviceConfig{
    SubstrateDeviceConfig{[window] window, [deviceToken] 13i32, [queueToken] 17i32}
  }
  [i32] deviceToken{GraphicsSubstrate.createDevice(deviceConfig)?}
  [i32] queueToken{GraphicsSubstrate.createQueue(deviceConfig)?}
  [Device] device{Device{[token] deviceToken}}
  [Queue] queue{Queue{[token] queueToken}}
  [SubstrateSwapchainConfig] swapchainConfig{
    SubstrateSwapchainConfig{
      [window] window,
      [device] device,
      [swapchainToken] 19i32,
      [colorFormat] ColorFormat.Bgra8Unorm,
      [depthFormat] DepthFormat.Depth32F,
      [presentMode] PresentMode.Fifo
    }
  }
  [i32] swapchainToken{GraphicsSubstrate.createSwapchain(swapchainConfig)?}
  [Swapchain] swapchain{
    Swapchain{
      [token] swapchainToken,
      [colorFormat] swapchainConfig.colorFormat,
      [depthFormat] swapchainConfig.depthFormat,
      [presentMode] swapchainConfig.presentMode
    }
  }
  [SubstrateMeshConfig] meshConfig{
    SubstrateMeshConfig{[meshToken] 21i32, [vertexCount] 8i32, [indexCount] 36i32}
  }
  [i32] meshToken{GraphicsSubstrate.createMesh(meshConfig)?}
  [Mesh] mesh{Mesh{[token] meshToken, [vertexCount] meshConfig.vertexCount, [indexCount] meshConfig.indexCount}}
  [SubstratePipelineConfig] pipelineConfig{
    SubstratePipelineConfig{
      [pipelineToken] 23i32,
      [shader] ShaderLibrary.CubeBasic,
      [colorFormat] ColorFormat.Bgra8Unorm,
      [depthFormat] DepthFormat.Depth32F
    }
  }
  [i32] pipelineToken{GraphicsSubstrate.createPipeline(pipelineConfig)?}
  [Pipeline] pipeline{Pipeline{[token] pipelineToken}}
  [SubstrateFrameConfig] frameConfig{SubstrateFrameConfig{[swapchain] swapchain, [frameToken] 27i32}}
  [i32] frameToken{GraphicsSubstrate.acquireFrame(frameConfig)?}
  [Frame] frame{Frame{[token] frameToken}}
  [SubstrateRenderPassConfig] renderPassConfig{
    SubstrateRenderPassConfig{
      [frame] frame,
      [renderPassToken] 29i32,
      [clearColor] ColorRGBA{0.05f32, 0.07f32, 0.10f32, 1.0f32},
      [clearDepth] 1.0f32
    }
  }
  [i32] renderPassToken{GraphicsSubstrate.beginRenderPass(renderPassConfig)?}
  [RenderPass] renderPass{RenderPass{[token] renderPassToken}}
  [Material] material{Material{[token] 31i32}}
  [SubstrateDrawMeshConfig] drawMeshConfig{
    SubstrateDrawMeshConfig{[renderPass] renderPass, [mesh] mesh, [material] material, [drawToken] 79i32}
  }
  [i32] drawToken{GraphicsSubstrate.drawMesh(drawMeshConfig)}
  [SubstrateRenderPassEndConfig] endConfig{
    SubstrateRenderPassEndConfig{[renderPass] renderPass, [endToken] 83i32}
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


TEST_SUITE_END();
