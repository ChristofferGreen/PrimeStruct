#!/usr/bin/env bash
set -euo pipefail

PREFIX="[native-window-launcher]"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$ROOT_DIR/scripts/run_canonical_gfx_native_window.sh" \
  --launcher-prefix "$PREFIX" \
  --usage-name "run_native_spinning_cube_window.sh" \
  --prime-source "$ROOT_DIR/examples/web/spinning_cube/cube.prime" \
  --entry /cubeStdGfxEmitFrameStream \
  --host-source "$ROOT_DIR/examples/native/spinning_cube/window_host.mm" \
  --preflight-script "$ROOT_DIR/scripts/preflight_native_spinning_cube_window.sh" \
  --out-subdir "spinning-cube-native-window" \
  --stream-bin-name "cube_stdlib_gfx_stream" \
  --host-bin-name "spinning_cube_window_host" \
  --stream-description "cube stdlib gfx stream binary" \
  --host-description "native window host" \
  "$@"
