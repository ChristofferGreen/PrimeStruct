# PrimeStruct Metal Target: Spinning Cube Shader Path

This directory contains the macOS Metal shader/output path for the shared
spinning-cube sample.

## Files
- `cube.metal`: minimal Metal vertex/fragment shaders for the cube sample.
- `metal_host.mm`: thin sample-owned CLI/parity wrapper that binds triangle
  resource callbacks onto the shared offscreen Metal helper.
- `../../shared/gfx_contract_shared.h`: shared host-side `GfxError` mapping and
  locked `/std/gfx/VertexColored` upload-layout definition used by the native
  and Metal sample hosts.
- `../../shared/metal_offscreen_host.h`: shared offscreen Metal runtime helper
  that owns device/library/queue setup, shader-loading failures, command
  submission, and the software-surface demo path.
- `../../../scripts/run_metal_spinning_cube.sh`: thin sample launcher over the
  shared Metal build/run helper.

## Local Shader Build (macOS)
1. Compile Metal source to AIR:
   - `xcrun metal -std=metal3.0 -c cube.metal -o cube.air`
2. Build metallib output:
   - `xcrun metallib cube.air -o cube.metallib`
3. Build host glue:
   - `xcrun clang++ -std=c++17 -fobjc-arc metal_host.mm -framework Foundation -framework Metal -o metal_host`
4. Run host smoke:
   - `./metal_host ./cube.metallib`
5. Optional software-surface bridge smoke:
   - `./metal_host --software-surface-demo`

## Shared Launcher (macOS)
```bash
./scripts/run_metal_spinning_cube.sh
./scripts/run_metal_spinning_cube.sh --software-surface-demo
./scripts/run_metal_spinning_cube.sh --snapshot-code 120
./scripts/run_metal_spinning_cube.sh --parity-check 120
```
- Builds into `build-debug/spinning-cube-metal` by default, unless `--out-dir`
  is provided.
- Delegates the reusable shader/metallib/host compile flow to
  `scripts/run_canonical_metal_host.sh`, leaving only sample path binding in
  `run_metal_spinning_cube.sh`.
- Default mode compiles `cube.metal`, links `cube.metallib`, builds the host,
  runs one offscreen frame, and requires `frame_rendered=1`.
- `--software-surface-demo` skips shader/metallib compilation, runs the shared
  software-surface path, and requires `software_surface_presented=1`.
- `--snapshot-code` and `--parity-check` compile only the host and forward the
  requested deterministic helper mode.

The host prints `gfx_profile=metal-osx` and `frame_rendered=1` and exits 0
when frame submission completes.
The software-surface bridge smoke uploads a deterministic BGRA8 software buffer
into a shared Metal texture, blits it into the host target texture, and prints
`software_surface_bridge=1`, `software_surface_width=64`,
`software_surface_height=64`, and `software_surface_presented=1`.

Failure diagnostics print deterministic:
- `gfx_profile=metal-osx`
- `gfx_error_code=<device_create_failed|mesh_create_failed|pipeline_create_failed|frame_acquire_failed|queue_submit_failed>`
- `gfx_error_why=<human_readable_details>`

## Parity Helpers
- `./metal_host --snapshot-code 120` prints the fixed-step snapshot code.
- `./metal_host --parity-check 120` runs tolerance-based transform/rotation
  parity checks against chunked stepping (`parity_ok=1` means pass).
- The launcher exposes the same modes: `./scripts/run_metal_spinning_cube.sh
  --snapshot-code 120` and `./scripts/run_metal_spinning_cube.sh
  --parity-check 120`.

## Profile Gating
- Browser profile remains `--emit=wasm --wasm-profile browser`.
- `metal-osx` is a design-profile identifier; attempting to pass it as a Wasm
  profile is rejected by compile-time option validation.
