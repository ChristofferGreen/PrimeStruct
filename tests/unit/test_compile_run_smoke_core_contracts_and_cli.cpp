#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("graphics api contract doc-linked constraints stay locked" * doctest::skip(true)) {
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
    CHECK(diagnostics.find("\"notes\":[\"stage: semantic\",\"semantic-product: available\"]") !=
          std::string::npos);
  }
}

TEST_CASE("canonical gfx helpers remain behind private substrate boundary") {
  auto resolveStdlibPath = [](const std::string &name) -> std::filesystem::path {
    std::filesystem::path path = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / name;
    if (!std::filesystem::exists(path)) {
      path = std::filesystem::current_path().parent_path() / "stdlib" / "std" / "gfx" / name;
    }
    return path;
  };

  const std::filesystem::path gfxStdlibPath = resolveStdlibPath("gfx.prime");
  REQUIRE(std::filesystem::exists(gfxStdlibPath));
  const std::string gfxStdlib = readFile(gfxStdlibPath.string());

  CHECK(gfxStdlib.find("[struct]\n  GraphicsSubstrate() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateSwapchainConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateMeshConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstratePipelineConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateFrameConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateRenderPassConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateDrawMeshConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("[struct]\n  SubstrateRenderPassEndConfig() {") != std::string::npos);
  CHECK(gfxStdlib.find("createWindow([SubstrateWindowConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("createDevice([SubstrateDeviceConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("createQueue([SubstrateDeviceConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("createSwapchain([SubstrateSwapchainConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("createMesh([SubstrateMeshConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("createPipeline([SubstratePipelineConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("acquireFrame([SubstrateFrameConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("openRenderPass([SubstrateRenderPassConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("drawMesh([SubstrateDrawMeshConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("endRenderPass([SubstrateRenderPassEndConfig] config)") != std::string::npos);
  CHECK(gfxStdlib.find("submitFrame([i32] frameToken, [i32] queueToken)") != std::string::npos);
  CHECK(gfxStdlib.find("presentFrame([i32] frameToken)") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createWindow(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createDevice(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createQueue(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createSwapchain(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createMesh(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.createPipeline(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.acquireFrame(config)?") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.openRenderPass(config)") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.drawMesh(config)") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.endRenderPass(config)") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.submitFrame(this.token, queue.token)") != std::string::npos);
  CHECK(gfxStdlib.find("GraphicsSubstrate.presentFrame(this.token)") != std::string::npos);
  CHECK(gfxStdlib.find("[public struct]\n  GraphicsSubstrate() {") == std::string::npos);
  CHECK(gfxStdlib.find("[public struct]\n  SubstrateSwapchainConfig() {") == std::string::npos);
  CHECK(gfxStdlib.find("[public struct]\n  SubstrateRenderPassConfig() {") == std::string::npos);
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

TEST_SUITE_END();
