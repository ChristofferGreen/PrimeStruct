#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("experimental gfx end-to-end conformance runs across backends" * doctest::skip(true)) {
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
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
    return;
  }
  CHECK(runCommand(exePath) == 10);
  CHECK(runCommand(runVmCmd) == 10);
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("canonical gfx end-to-end conformance runs across backends" * doctest::skip(true)) {
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
                                                "native backend requires typed bindings",
                                                "vm backend requires typed bindings")) {
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


TEST_SUITE_END();
