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
  - `cubeInit`, `cubeTick`, and `wrapAngle`: shared fixed-step update helpers.

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
