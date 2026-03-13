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
From the repo root, a one-command launcher can build and run the window host:
- `./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec`
  (defaults to `--max-frames 600`, about 10 seconds at 60 fps)
- `./scripts/run_native_spinning_cube_window.sh --primec ./build-debug/primec --visual-smoke`
  (checks `window_shown`, `render_loop_alive`, and `rotation_changes_over_time`;
  emits `VISUAL-SMOKE: SKIP ...` and exits `0` on non-macOS/non-GUI CI runners)

Or run the manual steps:
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
6. Optional software-surface bridge demo:
   - `./spinning_cube_window_host --software-surface-demo --max-frames 1`

The window host now renders an indexed cube mesh each frame and updates
transform uniforms from the deterministic fixed-step simulation stream.
The software-surface demo reuses the same window presenter path, uploads a
deterministic BGRA8 software buffer into a shared Metal texture, and blits it
into the drawable.

Expected diagnostics include:
- `gfx_profile=native-desktop`
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
- `software_surface_bridge=1`
- `software_surface_width=64`
- `software_surface_height=64`
- `software_surface_presented=1` (software-surface demo)

Close handling diagnostics:
- `exit_reason=window_close` when the user closes the window.
- `exit_reason=esc_key` when ESC is pressed in the focused window.

Startup failure diagnostics:
- `startup_failure=1`
- `gfx_profile=native-desktop`
- `startup_failure_stage=<simulation_stream_load|gpu_device_acquisition|shader_load|pipeline_setup|window_creation|first_frame_submission>`
- `startup_failure_reason=<stable_reason_token>`
- `startup_failure_exit_code=<stable_stage_code>`
- `gfx_error_code=<window_create_failed|device_create_failed|swapchain_create_failed|mesh_create_failed|pipeline_create_failed|material_create_failed|frame_acquire_failed|queue_submit_failed>` for graphics-related failures
- `gfx_error_why=<human_readable_details>` for graphics-related failures
