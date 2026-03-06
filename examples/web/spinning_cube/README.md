# PrimeStruct Web Target: Spinning Cube

This example is a milestone target for cross-platform backend work.

## Goal
- Render a colored spinning cube in a web browser using PrimeStruct-generated artifacts.
- Keep simulation deterministic so animation state matches VM/native/Wasm reference runs.
- Keep native and Metal host smoke paths aligned with the shared simulation data contract.

## Current Native Status (Parity Gap)
- Native emit `/main` is currently unsupported (`native backend does not support return type on /cubeInit`).
- Native smoke runs through `/mainNative` and `examples/native/spinning_cube/main.cpp`.
- For a visible rotating window today, use the browser path (`index.html` + `main.js`).
- macOS now has a real native window host sample at
  `examples/native/spinning_cube/window_host.mm` (window/layer/render-loop bring-up).
- Windowed native parity target is tracked in `docs/todo.md` under `Native Windowed Spinning Cube (Roadmap)`.

## First Supported Native Window Target (v1)
- Target: macOS + Metal window host (`examples/native/spinning_cube/window_host.mm`).
- Required tools: Xcode Command Line Tools with accepted license, plus `xcrun`, `metal`, `metallib`, and `clang++`.
- Required frameworks: `Foundation`, `Metal`, `AppKit`, and `QuartzCore`.
- Minimum OS: macOS 14.0 (Sonoma).
- Non-goals for v1:
  - No Linux/Windows native window host support.
  - No OpenGL/Vulkan/DirectX host backends.
  - No promise that shared-source `/main` native compile parity is restored in this milestone.

## Planned Artifacts
- `cube.prime`: PrimeStruct source for cube transform/update logic.
- `index.html`: minimal host page and canvas bootstrap.
- `main.js`: browser runtime glue (load `.wasm`, bind uniforms/buffers, drive frame loop).
- `cube.wgsl` (or generated shader output): shader module used by the browser graphics backend.
- `cube_native` (or platform equivalent): native executable produced from the same sample logic.
- `cube.metal` (or generated Metal shader output): shader module for macOS Metal path.
- `metal_host.mm` (or equivalent host glue): minimal macOS renderer bootstrap for the Metal target.

## Current Shared Source
- `cube.prime` now provides the shared simulation/data contract used by all hosts:
  - `CubeMeshLayout`: host-independent mesh/uniform stride + count metadata.
  - `CubeSimulationState`: deterministic tick + angle state.
  - `cubeInit`, `cubeTick`, and `wrapAngle`: shared simulation helpers.
  - `cubeAdvanceFixed`: deterministic fixed-step loop primitive with
    accumulator + max-step guard.
  - `cubeFixedStepSnapshot120` and `cubeFixedStepSnapshot120Chunked`: golden
    deterministic tick-state snapshot entries used by smoke tests.
  - `cubeRotationParity120`: tolerance-based transform/rotation parity entry
    shared across backend parity suites.
  - Native flat-frame entrypoints for host driving without struct-return ABI:
    `cubeNativeFrameInit*`, `cubeNativeFrameStep*`,
    `cubeNativeMeshVertexCount`, `cubeNativeMeshIndexCount`, and
    `cubeNativeFrameStepSnapshotCode`.
  - Native window host ABI v1 entrypoints:
    `cubeNativeAbiVersion`, `cubeNativeAbiInit*`, `cubeNativeAbiTick*`,
    `cubeNativeAbiUniform*`, `cubeNativeAbiConformance*`, and
    `cubeNativeAbiEmitFrameStream`.
  - `mainNative`: native-only split entrypoint for host smoke while native
    `/main` support remains blocked on struct-return lowering.

## Native Window Host ABI Contract (v1)
- Versioning:
  - `cubeNativeAbiVersion` returns `1`.
- Init surface (scalar fixed-point outputs):
  - `cubeNativeAbiInitTick`
  - `cubeNativeAbiInitAngleMilli`
  - `cubeNativeAbiInitAxisXCenti`
  - `cubeNativeAbiInitAxisYCenti`
  - `cubeNativeAbiInitMeshVertexCount`
  - `cubeNativeAbiInitMeshIndexCount`
- Fixed-step tick surface:
  - `cubeNativeAbiFixedStepMillis` returns `16` (host step size in milliseconds).
  - `cubeNativeAbiTickStatus(deltaMillis)` validates step input.
  - `cubeNativeAbiTickNextTick`, `cubeNativeAbiTickNextAngleMilli`,
    `cubeNativeAbiTickNextAxisXCenti`, and `cubeNativeAbiTickNextAxisYCenti`
    produce next-frame scalar outputs.
- Transform/uniform scalar outputs:
  - `cubeNativeAbiUniformAngleMilli`
  - `cubeNativeAbiUniformAxisXCenti`
  - `cubeNativeAbiUniformAxisYCenti`
  - `cubeNativeAbiUniformMeshIndexCount`
- Error/return conventions:
  - `0` (`cubeNativeAbiStatusOk`) means success.
  - `201` (`cubeNativeAbiStatusInvalidDeltaMillis`) means invalid tick delta.
  - Conformance wrappers (`cubeNativeAbiConformance*`) lock deterministic ABI
    behavior in compile-run tests.
- Host integration stream:
  - `cubeNativeAbiEmitFrameStream` prints deterministic fixed-step frame-state
    quads (`tick`, `angleMilli`, `axisXCenti`, `axisYCenti`) for host-driven
    render loops.

## Current Browser Host Assets
- `index.html` provides the canvas shell and module bootstrap.
- `main.js` loads `cube.wasm` when present and renders a deterministic
  wireframe cube proxy so host bootstrap can be smoke-tested before WebGPU
  integration lands.
- `cube.wgsl` is the minimal WebGPU shader path for the browser profile.
- `examples/metal/spinning_cube/cube.metal` is the minimal macOS Metal shader
  path (`metal-osx` profile artifact path).
- `examples/metal/spinning_cube/metal_host.mm` is the minimal macOS host glue
  that submits one frame using the generated Metal shader library.

## Minimal Browser Profile (Current)
- Emit target: `--emit=wasm --wasm-profile browser` (alias: `wasm-browser`).
- Shader path: `examples/web/spinning_cube/cube.wgsl` is the canonical browser
  shader artifact for this sample.
- Compile-time gating: browser profile must reject WASI-only effects/opcodes
  (for example `effects(io_out)` and argv-dependent entry signatures).

## Acceptance Criteria
- The page shows a rotating cube without manual setup beyond launching a local static server.
- A native smoke run (`/mainNative` + `spinning_cube_host`) reports deterministic success.
- A macOS Metal smoke run reports deterministic startup success (`frame_rendered=1`).
- Rotation is time-step deterministic for a fixed delta (same angle progression for the same tick count).
- Builds emit stable artifacts for all targets: Wasm module, native executable, shader outputs, and host glue assets.
- Unsupported language features/effects for the browser profile fail at compile time with clear diagnostics.
- Unsupported language features/effects for native/Metal profiles fail at compile time with clear diagnostics.

## Suggested Milestones
1. Add browser profile validation (`wasm-browser`) and compile-time gating.
2. Add Wasm emitter MVP for numeric/control-flow subset used by this sample.
3. Add browser shader path (WebGPU-oriented) and minimal binding model.
4. Add native host target wiring for the same sample source.
5. Add Metal shader/output path and minimal macOS host binding.
6. Wire fixed-step render/update loops for all sample targets.
7. Add parity/integration tests in CI for multi-target sample build output.

## Runtime Notes
- Prefer WebGPU shader targets for browser support.
- Keep sample dependencies minimal (no framework requirement).
- Include a small debug HUD (tick count + frame time) once VM debug events can be surfaced in web tooling.
- Keep simulation/update logic shared; only rendering/host glue should vary by target.

## Integration Artifact Matrix
- CI/compile-run integration coverage builds:
  - `cube.wasm` (browser profile)
  - `cube_native` (native profile)
  - loader assets (`index.html`, `main.js`, `cube.wgsl`)
  - optional macOS shader outputs (`cube.air`, `cube.metallib`) when `xcrun`
    is available.
- The integration test emits an `artifact_manifest.json` with per-artifact size
  and deterministic FNV-1a hash fields for schema/hash validation.

## Optional Visual Startup Smoke
- Automated visual startup checks run with explicit capability gates:
  - native host startup marker (`native host verified cube simulation output`)
  - macOS Metal startup marker (`frame_rendered=1`) when `xcrun` exists
  - browser headless startup (`chromium`/`google-chrome` + `python3`) when
    available.
- If browser headless tooling is unavailable, tests emit an interactive fallback
  note instead of failing.

## Build And Run (Repo Root)
### Browser
```bash
./primec --emit=wasm --wasm-profile browser examples/web/spinning_cube/cube.prime -o examples/web/spinning_cube/cube.wasm --entry /main
python3 -m http.server 8080 --bind 127.0.0.1 --directory examples/web/spinning_cube
```
Open `http://127.0.0.1:8080/`.

Expected runtime behavior:
- Startup: page title and canvas appear, then status text reports host bootstrap.
- Controls: none yet (v1 sample has no camera/input bindings).
- FPS/diagnostic overlay: status text under the canvas is the current
  diagnostics surface (no dedicated FPS counter yet).

### Native
```bash
./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native --entry /mainNative
c++ -std=c++17 examples/native/spinning_cube/main.cpp -o /tmp/spinning_cube_host
/tmp/spinning_cube_host /tmp/cube_native
```
Expected runtime behavior:
- Startup: host process returns 0 after running one simulation validation pass.
- Diagnostics: prints `native host verified cube simulation output`.
- Note: shared-source `/main` is still unsupported for native emit until
  struct-return lowering parity is restored.

### Native Window Host (macOS)
```bash
./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_native_frame_stream --entry /cubeNativeAbiEmitFrameStream
xcrun clang++ -std=c++17 -fobjc-arc examples/native/spinning_cube/window_host.mm -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o /tmp/spinning_cube_window_host
/tmp/spinning_cube_window_host --cube-sim /tmp/cube_native_frame_stream --max-frames 120
```
Expected runtime behavior:
- Startup: opens a desktop window, configures a `CAMetalLayer` swapchain, and
  creates cube pipeline resources (shader library + vertex/index/uniform
  buffers).
- Rendering: submits indexed cube draw calls each frame with transform
  uniforms updated from the deterministic simulation stream.
- Diagnostics: prints `simulation_stream_loaded=1`,
  `simulation_fixed_step_millis=16`, `shader_library_ready=1`,
  `vertex_buffer_ready=1`, `index_buffer_ready=1`,
  `uniform_buffer_ready=1`, `window_created=1`,
  `swapchain_layer_created=1`, `pipeline_ready=1`, `startup_success=1`,
  `frame_rendered=1`, and `exit_reason=max_frames`.

### macOS Metal
```bash
xcrun metal -std=metal3.0 -c examples/metal/spinning_cube/cube.metal -o /tmp/cube.air
xcrun metallib /tmp/cube.air -o /tmp/cube.metallib
xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework Metal -o /tmp/metal_host
/tmp/metal_host /tmp/cube.metallib
```
Expected runtime behavior:
- Startup: host process returns 0 after one frame submission.
- Diagnostics: prints `frame_rendered=1`.

### Native Windowed Execution Preflight (macOS)
```bash
./scripts/preflight_native_spinning_cube_window.sh
xcrun metal -std=metal3.0 -c examples/metal/spinning_cube/cube.metal -o /tmp/cube.air
xcrun metallib /tmp/cube.air -o /tmp/cube.metallib
xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework Metal -o /tmp/metal_host
/tmp/metal_host /tmp/cube.metallib
```
- `scripts/preflight_native_spinning_cube_window.sh` validates
  `xcrun --find metal`, `xcrun --find metallib`, and
  `launchctl print gui/<uid>` before host launch.
- If preflight fails, treat Metal host execution as unsupported on the current
  machine and skip windowed launch.

### Combined Smoke Helper
```bash
./scripts/run_spinning_cube_demo.sh --primec ./build-debug/primec
```
Expected behavior:
- Runs web, native, and metal checks in order.
- Prints deterministic summary lines:
  - `[spinning-cube-demo] WEB: ...`
  - `[spinning-cube-demo] NATIVE: ...`
  - `[spinning-cube-demo] METAL: ...`
  - `[spinning-cube-demo] RESULT: PASS|FAIL`
- Exit code is `0` when checks pass or are skipped, and `1` on any check failure.
