# PrimeStruct Web Target: Spinning Cube

This example is a milestone target for cross-platform backend work.

## Goal
- Render a colored spinning cube in a web browser using PrimeStruct-generated artifacts.
- Keep simulation deterministic so animation state matches VM/native/Wasm reference runs.
- Keep native and Metal host smoke paths aligned with the shared simulation data contract.

## Current Native Status (Parity Gap)
- Native emit `/main` is currently unsupported (`native backend does not support return type on /cubeInit`).
- Native smoke runs through `/mainNative` and `examples/native/spinning_cube/main.cpp`.
- For a visible rotating window today, use the browser path
  (`/spinning_cube/index.html` + the thin `main.js` wrapper).
- macOS now has a real native window host sample at
  `examples/native/spinning_cube/window_host.mm` (window/layer/render-loop bring-up).
- The archived native-window roadmap has landed its v1 macOS host target; add
  a concrete TODO before tracking another native-window parity milestone.

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
- `main.js`: thin browser sample bootstrap that binds selectors/assets onto the
  shared browser runtime helper.
- `examples/web/shared/browser_runtime_shared.js`: shared browser runtime
  helper that loads `.wasm`, validates shader/bootstrap assets, updates status
  text, and drives the deterministic proxy frame loop.
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
  - Native window-host gfx stream entrypoint:
    `cubeStdGfxEmitFrameStream` emits deterministic `/std/gfx/*` header data
    plus the shared simulation tail consumed by the macOS window host and
    launcher smoke path.
  - `mainNative`: native-only split entrypoint for host smoke while native
    `/main` support remains blocked on struct-return lowering.

## Current Browser Host Assets
- `index.html` provides the canvas shell and module bootstrap.
- `main.js` is now only a thin sample wrapper over
  `examples/web/shared/browser_runtime_shared.js`.
- `examples/web/shared/browser_runtime_shared.js` loads `cube.wasm` when
  present, validates `cube.wgsl`, updates deterministic status text, and
  renders the wireframe cube proxy so host bootstrap can be smoke-tested
  before WebGPU integration lands.
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
  - loader assets (`spinning_cube/index.html`, `spinning_cube/main.js`,
    `spinning_cube/cube.wgsl`, `spinning_cube/cube.wasm`, and
    `shared/browser_runtime_shared.js`)
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
python3 -m http.server 8080 --bind 127.0.0.1 --directory examples/web
```
Open `http://127.0.0.1:8080/spinning_cube/index.html`.

Expected runtime behavior:
- Startup: page title and canvas appear, then status text reports host bootstrap.
- Controls: none yet (v1 sample has no camera/input bindings).
- FPS/diagnostic overlay: status text under the canvas is the current
  diagnostics surface (no dedicated FPS counter yet).
- The sample `main.js` now only binds `#cube-canvas`, `#status`, and the local
  asset URLs onto `examples/web/shared/browser_runtime_shared.js`.

### Browser Launcher
```bash
./scripts/run_browser_spinning_cube.sh --primec ./build-debug/primec
./scripts/run_browser_spinning_cube.sh --primec ./build-debug/primec --headless-smoke
```
- The sample wrapper now delegates its reusable compile/stage/serve/headless
  smoke flow to `scripts/run_canonical_browser_sample.sh`.
- The launcher stages a root directory that contains both
  `spinning_cube/index.html` assets and the sibling shared helper
  `shared/browser_runtime_shared.js`, so the browser path no longer depends on
  serving the sample directory in isolation.
- `--headless-smoke` compiles `cube.wasm`, stages the browser root, serves it
  locally, and verifies the deterministic status text
  `Host running with cube.wasm and cube.wgsl bootstrap.` when browser tooling
  is available.

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
./primec --emit=native examples/web/spinning_cube/cube.prime -o /tmp/cube_stdlib_gfx_stream --entry /cubeStdGfxEmitFrameStream
xcrun clang++ -std=c++17 -fobjc-arc examples/native/spinning_cube/window_host.mm -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o /tmp/spinning_cube_window_host
/tmp/spinning_cube_window_host --gfx /tmp/cube_stdlib_gfx_stream --max-frames 120
```
- Startup: compiles and runs the shared canonical `/std/gfx/*` `.prime` stream
  from `cubeStdGfxEmitFrameStream`, then feeds the native window host with
  deterministic window/swapchain/pass metadata plus the shared simulation
  tail.
- Rendering: the macOS host consumes the stdlib gfx stream header for
  window size and clear settings while the shared simulation frames still drive
  cube rotation uniforms.
- Diagnostics: prints `gfx_stream_loaded=1`,
  `gfx_window_width=1280`,
  `gfx_window_height=720`,
  `gfx_mesh_vertex_count=8`,
  `gfx_mesh_index_count=36`,
  `gfx_submit_present_mask=3`, `startup_success=1`,
  `frame_rendered=1`, and `exit_reason=max_frames`.

Software surface bridge demo:
```bash
xcrun clang++ -std=c++17 -fobjc-arc examples/native/spinning_cube/window_host.mm -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o /tmp/spinning_cube_window_host
/tmp/spinning_cube_window_host --software-surface-demo --max-frames 1
```
- Startup: opens the same `CAMetalLayer` presenter path without requiring a
  cube simulation stream.
- Bridge behavior: uploads a deterministic BGRA8 software color buffer into a
  shared Metal texture and blits it into the window drawable.
- Diagnostics: prints `software_surface_bridge=1`,
  `software_surface_width=64`, `software_surface_height=64`,
  `software_surface_presented=1`, `frame_rendered=1`, and
  `exit_reason=max_frames`.

### Native Window Launcher (macOS)
```bash
./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec
./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec --visual-smoke
```
- Builds both the canonical stdlib gfx stream binary and the native window
  host into `build-debug/spinning-cube-native-window`, then launches the host.
- The launcher compiles `cubeStdGfxEmitFrameStream` and runs the host through
  `--gfx`, so scripted smoke no longer depends on the old `--cube-sim` mode.
- The sample wrapper now delegates the reusable build/launch/smoke flow to
  `scripts/run_canonical_gfx_native_window.sh`, leaving only spinning-cube
  path/entrypoint binding in `run_native_spinning_cube_window.sh`.
- The native host itself now binds its cube/software-surface callbacks onto
  `examples/shared/native_metal_window_host.h`, so the sample file no longer
  owns the `NSWindow`/`CAMetalLayer` presenter shell directly.
- Defaults to `--max-frames 600` for normal windowed runs (about 10 seconds at
  60 fps), satisfying the native done-condition smoke target.
- Runs `scripts/preflight_native_spinning_cube_window.sh` first unless
  `--skip-preflight` is provided.
- Optional launcher flags: `--out-dir`, `--max-frames`,
  `--simulation-smoke`, and `--visual-smoke`.
- Visual smoke criteria:
  - `window_shown`: `window_created=1` and `startup_success=1`.
  - `render_loop_alive`: `frame_rendered=1` and `exit_reason=max_frames`.
  - `rotation_changes_over_time`: first two `angleMilli` values from the
    simulation payload in `cube_stdlib_gfx_stream` differ.
- CI skip rules for `--visual-smoke`: exits `0` with a `VISUAL-SMOKE: SKIP`
  marker on non-macOS runners or when `launchctl print gui/<uid>` fails.

### macOS Metal
```bash
xcrun metal -std=metal3.0 -c examples/metal/spinning_cube/cube.metal -o /tmp/cube.air
xcrun metallib /tmp/cube.air -o /tmp/cube.metallib
xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework Metal -o /tmp/metal_host
/tmp/metal_host /tmp/cube.metallib
./scripts/run_metal_spinning_cube.sh
./scripts/run_metal_spinning_cube.sh --software-surface-demo
```
Expected runtime behavior:
- Startup: host process returns 0 after one frame submission.
- Diagnostics: prints `frame_rendered=1`.
- The sample wrapper now delegates the reusable shader/metallib/host build and
  launch flow to `scripts/run_canonical_metal_host.sh`, leaving only
  spinning-cube path binding in `run_metal_spinning_cube.sh`.
- The Metal host itself now binds its triangle/resource callbacks onto
  `examples/shared/metal_offscreen_host.h`, so the sample file no longer owns
  the offscreen device/library/queue/submit shell directly.

Software surface bridge demo:
```bash
xcrun clang++ -std=c++17 -fobjc-arc examples/metal/spinning_cube/metal_host.mm -framework Foundation -framework Metal -o /tmp/metal_host
/tmp/metal_host --software-surface-demo
```
- Startup: host process returns 0 after one software-surface blit pass.
- Bridge behavior: uploads the deterministic BGRA8 software buffer into a
  shared Metal texture and blits it into the host target texture.
- Diagnostics: prints `software_surface_bridge=1`,
  `software_surface_width=64`, `software_surface_height=64`,
  `software_surface_presented=1`, and `frame_rendered=1`.

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
