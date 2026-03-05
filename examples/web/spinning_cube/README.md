# PrimeStruct Web Target: Spinning Cube

This example is a milestone target for cross-platform backend work.

## Goal
- Render a colored spinning cube in a web browser using PrimeStruct-generated artifacts.
- Render the same spinning cube as a native desktop executable.
- Render the same spinning cube through a macOS Metal-targeted graphics path.
- Keep simulation deterministic so animation state matches VM/native/Wasm reference runs.

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
- A native desktop run shows the same rotating cube behavior from the same simulation source.
- A macOS Metal run shows the same rotating cube behavior using Metal shader output.
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
