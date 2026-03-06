# PrimeStruct Native Target: Spinning Cube Host

This sample hosts the shared spinning-cube simulation source from
`examples/web/spinning_cube/cube.prime` through a native desktop glue program.

## Files
- `main.cpp`: minimal native host glue that executes the compiled cube sample
  and validates the expected deterministic exit code.
- `window_host.mm`: native macOS window host sample (`AppKit` + `CAMetalLayer`)
  that opens a real desktop window and runs a Metal render loop.

## Local Build Steps
1. Build the shared cube sample to a native binary:
   - `./primec --emit=native ../../web/spinning_cube/cube.prime -o ./cube_native --entry /mainNative`
2. Build the native host glue:
   - `c++ -std=c++17 main.cpp -o spinning_cube_host`
3. Run host + sample:
   - `./spinning_cube_host ./cube_native`

The host should print `native host verified cube simulation output` and exit 0.
Shared-source `/main` remains unsupported for native emit until struct-return
lowering parity is restored.

## Window Host Build (macOS)
1. Build the deterministic simulation stream binary:
   - `./primec --emit=native ../../web/spinning_cube/cube.prime -o ./cube_native_frame_stream --entry /cubeNativeAbiEmitFrameStream`
2. Build the native window host:
   - `xcrun clang++ -std=c++17 -fobjc-arc window_host.mm -framework Foundation -framework AppKit -framework QuartzCore -framework Metal -o spinning_cube_window_host`
3. Run the window host:
   - `./spinning_cube_window_host --cube-sim ./cube_native_frame_stream`
4. Optional bounded smoke run:
   - `./spinning_cube_window_host --cube-sim ./cube_native_frame_stream --max-frames 120`
5. Optional non-GUI integration smoke:
   - `./spinning_cube_window_host --cube-sim ./cube_native_frame_stream --simulation-smoke`

The window host now renders an indexed cube mesh each frame and updates
transform uniforms from the deterministic fixed-step simulation stream.

Expected diagnostics include:
- `simulation_stream_loaded=1`
- `simulation_fixed_step_millis=16`
- `shader_library_ready=1`
- `vertex_buffer_ready=1`
- `index_buffer_ready=1`
- `uniform_buffer_ready=1`
- `window_created=1`
- `swapchain_layer_created=1`
- `pipeline_ready=1`
- `startup_success=1`
- `frame_rendered=1`
- `exit_reason=max_frames` (bounded smoke run)

Close handling diagnostics:
- `exit_reason=window_close` when the user closes the window.
- `exit_reason=esc_key` when ESC is pressed in the focused window.
