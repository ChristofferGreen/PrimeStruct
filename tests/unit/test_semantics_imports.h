#pragma once

TEST_SUITE_BEGIN("primestruct.semantics.imports");

TEST_CASE("import brings immediate children into root") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("import resolves definitions declared before import") {
  const std::string source = R"(
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
import /util
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves struct types and constructors") {
  const std::string source = R"(
import /util
namespace util {
  [public struct]
  Widget() {
    [i32] value{1i32}
  }
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves methods on struct types") {
  const std::string source = R"(
import /util
[public struct]
/util/Widget() {
  [i32] value{1i32}
}
[public return<int>]
/util/Widget/get([Widget] self, [i32] value) {
  return(value)
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(item.get(5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit auto inference crosses imported call graph") {
  const std::string source = R"(
import /util
import /bridge
namespace util {
  [public return<auto>]
  id([auto] value) {
    return(value)
  }
}
namespace bridge {
  [public return<auto>]
  forward([auto] value) {
    return(id(value))
  }
}
[return<int>]
main() {
  return(forward(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import aliases a single definition") {
  const std::string source = R"(
import /util/inc
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: inner") != std::string::npos);
}

TEST_CASE("import aliases explicit nested definition path") {
  const std::string source = R"(
import /util/nested/inner
namespace util {
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias namespace blocks") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(nested())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: nested") != std::string::npos);
}

TEST_CASE("import rejects namespace-only path") {
  const std::string source = R"(
import /util
namespace util {
  namespace nested {
    [public return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/*") != std::string::npos);
}

TEST_CASE("import rejects /std/math without wildcard after semicolon") {
  const std::string source = R"(
import /util; /std/math
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("import /std/math is not supported; use import /std/math/* or /std/math/<name>") != std::string::npos);
}

TEST_CASE("import accepts whitespace-separated paths") {
  const std::string source = R"(
import /util /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts semicolon-separated paths") {
  const std::string source = R"(
import /util; /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts wildcard math and util paths") {
  const std::string source = R"(
import /std/math/* /util/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<f64>]
main() {
  [i32] value{inc(1i32)}
  return(sin(0.0f64))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple explicit math paths") {
  const std::string source = R"(
import /std/math/sin /std/math/pi
[return<f64>]
main() {
  return(plus(pi, sin(0.0f64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts explicit math min and max") {
  const std::string source = R"(
import /std/math/min /std/math/max
[return<i32>]
main() {
  return(max(min(1i32, 4i32), 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects unknown wildcard path") {
  const std::string source = R"(
import /missing/*
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing/*") != std::string::npos);
}

TEST_CASE("import rejects unknown single-segment path") {
  const std::string source = R"(
import /missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing/*") != std::string::npos);
}

TEST_CASE("import rejects missing definition") {
  const std::string source = R"(
import /util/missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/missing") != std::string::npos);
}

TEST_CASE("import rejects private definition path") {
  const std::string source = R"(
import /util/inc
namespace util {
  [private return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import path refers to private definition: /util/inc") != std::string::npos);
}

TEST_CASE("import rejects wildcard with only private children") {
  const std::string source = R"(
import /util
namespace util {
  [private return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/*") != std::string::npos);
}

TEST_CASE("import conflicts with existing root definitions") {
  const std::string source = R"(
import /util
[return<int>]
dup() {
  return(1i32)
}
namespace util {
  [public return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import conflicts with root builtin names") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  plus([i32] value) {
    return(value)
  }
}
[return<int>]
main() {
  return(plus(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: plus") != std::string::npos);
}

TEST_CASE("import rejects explicit root builtin alias") {
  const std::string source = R"(
import /util/assign
namespace util {
  [public return<void>]
  assign([i32] value) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: assign") != std::string::npos);
}

TEST_CASE("import conflicts between namespaces") {
  const std::string source = R"(
import /util, /tools
namespace util {
  [public return<int>]
  dup() {
    return(1i32)
  }
}
namespace tools {
  [public return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import resolves execution targets") {
  const std::string source = R"(
import /util
namespace util {
  [public return<void>]
  run([i32] count) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
run(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple paths in one statement") {
  const std::string source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(1i32), 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts comma-separated wildcards") {
  const std::string source = R"(
import /std/math/*, /util/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<f32>]
main() {
  return(sin(convert<f32>(inc(1i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("duplicate imports are ignored") {
  const std::string source = R"(
import /util, /util
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import works after definitions") {
  const std::string source = R"(
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(2i32))
}
import /util
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import ignores nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  first() {
    return(1i32)
  }
  namespace nested {
    [public return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(first())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects nested definitions in root") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [public return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(second())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: second") != std::string::npos);
}

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

TEST_SUITE_END();
