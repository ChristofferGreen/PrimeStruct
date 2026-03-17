#!/usr/bin/env bash
set -euo pipefail

PREFIX="[metal-launcher]"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$ROOT_DIR/scripts/run_canonical_metal_host.sh" \
  --launcher-prefix "$PREFIX" \
  --usage-name "run_metal_spinning_cube.sh" \
  --shader-source "$ROOT_DIR/examples/metal/spinning_cube/cube.metal" \
  --host-source "$ROOT_DIR/examples/metal/spinning_cube/metal_host.mm" \
  --out-subdir "spinning-cube-metal" \
  --host-bin-name "spinning_cube_metal_host" \
  --host-description "metal host" \
  "$@"
