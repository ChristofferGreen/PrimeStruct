# PrimeStruct Metal Target: Spinning Cube Shader Path

This directory contains the macOS Metal shader/output path for the shared
spinning-cube sample.

## Files
- `cube.metal`: minimal Metal vertex/fragment shaders for the cube sample.
- `metal_host.mm`: minimal macOS Metal host glue that loads the compiled
  shader library and submits a one-frame draw pass.

## Local Shader Build (macOS)
1. Compile Metal source to AIR:
   - `xcrun metal -std=metal3.0 -c cube.metal -o cube.air`
2. Build metallib output:
   - `xcrun metallib cube.air -o cube.metallib`
3. Build host glue:
   - `xcrun clang++ -std=c++17 -fobjc-arc metal_host.mm -framework Foundation -framework Metal -o metal_host`
4. Run host smoke:
   - `./metal_host ./cube.metallib`

The host prints `gfx_profile=metal-osx` and `frame_rendered=1` and exits 0
when frame submission completes.

Failure diagnostics print deterministic:
- `gfx_profile=metal-osx`
- `gfx_error_code=<device_create_failed|mesh_create_failed|pipeline_create_failed|frame_acquire_failed|queue_submit_failed>`
- `gfx_error_why=<human_readable_details>`

## Parity Helpers
- `./metal_host --snapshot-code 120` prints the fixed-step snapshot code.
- `./metal_host --parity-check 120` runs tolerance-based transform/rotation
  parity checks against chunked stepping (`parity_ok=1` means pass).

## Profile Gating
- Browser profile remains `--emit=wasm --wasm-profile browser`.
- `metal-osx` is a design-profile identifier; attempting to pass it as a Wasm
  profile is rejected by compile-time option validation.
