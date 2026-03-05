# PrimeStruct Metal Target: Spinning Cube Shader Path

This directory contains the macOS Metal shader/output path for the shared
spinning-cube sample.

## Files
- `cube.metal`: minimal Metal vertex/fragment shaders for the cube sample.

## Local Shader Build (macOS)
1. Compile Metal source to AIR:
   - `xcrun metal -std=metal3.0 -c cube.metal -o cube.air`
2. Build metallib output:
   - `xcrun metallib cube.air -o cube.metallib`

## Profile Gating
- Browser profile remains `--emit=wasm --wasm-profile browser`.
- `metal-osx` is a design-profile identifier; attempting to pass it as a Wasm
  profile is rejected by compile-time option validation.
