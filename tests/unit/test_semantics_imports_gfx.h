#pragma once

TEST_CASE("import resolves std gfx experimental type surface") {
  const std::string source = R"(
import /std/gfx/experimental/*
[return<int>]
main() {
  [Window] window{Window([token] 1i32, [width] 1280i32, [height] 720i32)}
  [ColorFormat] colorFormat{ColorFormat.Bgra8Unorm}
  [PresentMode] presentMode{PresentMode.Fifo}
  [Buffer<i32>] buffer{Buffer<i32>([token] 2i32, [elementCount] 4i32)}
  [Texture2D<i32>] texture{Texture2D<i32>([token] 3i32, [width] 64i32, [height] 32i32)}
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
  [GfxError] err{deviceCreateFailed()}
  [string] whyText{GfxError.why(err)}
  return(plus(plus(plus(window.width, colorFormat.value), presentMode.value), count(whyText)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves std gfx canonical type surface") {
  const std::string source = R"(
import /std/gfx/*
[return<int>]
main() {
  [Window] window{Window([token] 1i32, [width] 1280i32, [height] 720i32)}
  [ColorFormat] colorFormat{ColorFormat.Bgra8Unorm}
  [PresentMode] presentMode{PresentMode.Fifo}
  [Buffer<i32>] buffer{Buffer<i32>([token] 2i32, [elementCount] 4i32)}
  [Texture2D<i32>] texture{Texture2D<i32>([token] 3i32, [width] 64i32, [height] 32i32)}
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
  [GfxError] err{deviceCreateFailed()}
  [string] whyText{GfxError.why(err)}
  return(plus(plus(plus(window.width, colorFormat.value), presentMode.value), count(whyText)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves std collections experimental map wildcard surface") {
  const std::string source = R"(
import /std/collections/experimental_map/*
[return<int> effects(heap_alloc)]
main() {
  [Map<i32, i32>] values{mapPair<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  return(plus(mapCount<i32, i32>(values), mapAt<i32, i32>(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves std gfx experimental substrate boundary") {
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
  [SubstrateFrameConfig] frameConfig{SubstrateFrameConfig([swapchain] swapchain, [frameToken] 23i32)}
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
    SubstrateDrawMeshConfig([renderPass] renderPass, [mesh] Mesh([token] 27i32, [vertexCount] 3i32, [indexCount] 3i32), [material] material, [drawToken] 41i32)
  }
  [i32] drawToken{GraphicsSubstrate.drawMesh(drawMeshConfig)}
  [SubstrateRenderPassEndConfig] endConfig{
    SubstrateRenderPassEndConfig([renderPass] renderPass, [endToken] 43i32)
  }
  [i32] endToken{GraphicsSubstrate.endRenderPass(endConfig)}
  GraphicsSubstrate.submitFrame(frame.token, queue.token)?
  GraphicsSubstrate.presentFrame(frame.token)?
  return(plus(plus(window.width, device.token), plus(drawToken, endToken)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves std gfx experimental method wrapper surface") {
  const std::string source = R"(
import /std/math/*
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([token] 11i32, [width] 1280i32, [height] 720i32)}
  [Device] device{Device([token] 13i32)}
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
  [Pipeline] pipeline{
    device.create_pipeline(
      [shader] ShaderLibrary.CubeBasic,
      [vertex_type] VertexColored,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  [Material] material{pipeline.material()?}
  if(window.is_open(), then() { window.poll_events() }, else() { })
  [f32] aspect{window.aspect_ratio()}
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
  return(plus(plus(queue.token, frame.token), convert<i32>(aspect)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves std gfx canonical method wrapper surface") {
  const std::string source = R"(
import /std/math/*
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
  if(window.is_open(), then() { window.poll_events() }, else() { })
  [f32] aspect{window.aspect_ratio()}
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
  return(plus(plus(queue.token, frame.token), convert<i32>(aspect)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical gfx imports reject wasm-browser targets without runtime substrate") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgramForCompileTarget(source, "/main", "wasm", "browser", error));
  CHECK(error.find("graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*") !=
        std::string::npos);
}

TEST_CASE("experimental gfx imports reject glsl targets without runtime substrate") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgramForCompileTarget(source, "/main", "glsl", "wasi", error));
  CHECK(error.find("graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/experimental/*") !=
        std::string::npos);
}

TEST_CASE("experimental gfx result wrappers reject bare explicit struct bindings") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Window] window{Window([token] 11i32, [width] 1280i32, [height] 720i32)}
  [Device] device{Device([token] 13i32)}
  [Swapchain] swapchain{
    device.create_swapchain(
      window,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F,
      [present_mode] PresentMode.Fifo
    )
  }
  return(swapchain.token)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
}

TEST_CASE("experimental gfx window constructor entry point validates through stdlib helper") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  return(plus(window.width, window.height))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental gfx window constructor still rejects bare explicit struct binding") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)}
  return(window.width)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
}

TEST_CASE("experimental gfx profile literals keep deterministic reject") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Device] device{Device([profile] "metal-osx"utf8)?}
  return(device.token)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: profile") != std::string::npos);
}

TEST_CASE("experimental gfx device constructor entry point validates through stdlib helper") {
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
  return(plus(device.token, queue.token))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental gfx device constructor still rejects bare explicit struct binding") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Device] device{Device()}
  return(device.token)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
}

TEST_CASE("canonical gfx window constructor entry point validates through stdlib helper") {
  const std::string source = R"(
import /std/gfx/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Window] window{Window([title] "PrimeStruct"utf8, [width] 1280i32, [height] 720i32)?}
  return(plus(window.width, window.height))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical gfx profile literals keep deterministic reject") {
  const std::string source = R"(
import /std/gfx/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Device] device{Device([profile] "metal-osx"utf8)?}
  return(device.token)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: profile") != std::string::npos);
}

TEST_CASE("canonical gfx device constructor entry point validates through stdlib helper") {
  const std::string source = R"(
import /std/gfx/*

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(GfxError.why(err))
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  [Device] device{Device()?}
  [Queue] queue{device.default_queue()}
  return(plus(device.token, queue.token))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental gfx Buffer constructor entry point validates through builtin rewrite") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical gfx Buffer constructor entry point validates through builtin rewrite") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental gfx pipeline entry point validates through stdlib helper") {
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
  return(plus(pipeline.token, material.token))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical gfx pipeline entry point validates through stdlib helper") {
  const std::string source = R"(
import /std/gfx/*

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
  return(plus(pipeline.token, material.token))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental gfx pipeline entry point rejects unsupported vertex_type") {
  const std::string source = R"(
import /std/gfx/experimental/*

[struct]
VertexPlain() {
  [i32] x{0i32}
}

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
      [vertex_type] VertexPlain,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  return(pipeline.token)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("experimental gfx create_pipeline currently supports only VertexColored") != std::string::npos);
}

TEST_CASE("canonical gfx pipeline entry point rejects unsupported vertex_type") {
  const std::string source = R"(
import /std/gfx/*

[struct]
VertexPlain() {
  [i32] x{0i32}
}

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
      [vertex_type] VertexPlain,
      [color_format] ColorFormat.Bgra8Unorm,
      [depth_format] DepthFormat.Depth32F
    )?
  }
  return(pipeline.token)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("experimental gfx create_pipeline currently supports only VertexColored") != std::string::npos);
}

TEST_CASE("import rejects missing std gfx experimental path") {
  const std::string source = R"(
import /std/gfx/experimental/nope
[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /std/gfx/experimental/nope") != std::string::npos);
}
