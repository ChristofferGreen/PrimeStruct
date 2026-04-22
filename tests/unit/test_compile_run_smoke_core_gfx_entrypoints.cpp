#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

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
  const int exeExit = runCommand(exePath);
  const int vmExit = runCommand(runVmCmd);
  CHECK((exeExit == 4 || exeExit == 1));
  CHECK((vmExit == 4 || vmExit == 1));
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
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
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
    return;
  }
  CHECK(runCommand(exePath) == 6);
  CHECK(runCommand(runVmCmd) == 6);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("canonical gfx resource wrapper slice runs across backends") {
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
  const std::string srcPath = writeTemp("compile_gfx_canonical_resource_wrappers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_resource_wrappers_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_resource_wrappers_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_canonical_resource_wrappers",
                                                compileCmd,
                                                exePath,
                                                runVmCmd,
                                                compileNativeCmd,
                                                nativePath,
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
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

TEST_CASE("canonical gfx render pass wrapper slice runs across backends") {
  const std::string source = R"(
import /std/gfx/*

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
  const std::string srcPath = writeTemp("compile_gfx_canonical_render_pass_wrappers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_render_pass_wrappers_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_render_pass_wrappers_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_canonical_render_pass_wrappers",
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

TEST_CASE("canonical gfx resource wrapper errors stay deterministic across backends") {
  const std::string source = R"(
import /std/gfx/*

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
  const std::string srcPath = writeTemp("compile_gfx_canonical_resource_wrapper_errors.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_resource_wrapper_errors_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_resource_wrapper_errors_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_canonical_resource_wrapper_errors",
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

TEST_CASE("canonical gfx pipeline entry point runs across backends") {
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
  const std::string srcPath = writeTemp("compile_gfx_canonical_pipeline_entry.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_pipeline_entry_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_gfx_canonical_pipeline_entry_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  if (!compileAcrossBackendsOrExpectUnsupported("primec_gfx_canonical_pipeline_entry",
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


TEST_SUITE_END();
