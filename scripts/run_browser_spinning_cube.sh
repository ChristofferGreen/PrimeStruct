#!/usr/bin/env bash
set -euo pipefail

PREFIX="[browser-launcher]"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$ROOT_DIR/scripts/run_canonical_browser_sample.sh" \
  --launcher-prefix "$PREFIX" \
  --usage-name "run_browser_spinning_cube.sh" \
  --prime-source "$ROOT_DIR/examples/web/spinning_cube/cube.prime" \
  --entry "/main" \
  --sample-dir "$ROOT_DIR/examples/web/spinning_cube" \
  --shared-runtime "$ROOT_DIR/examples/web/shared/browser_runtime_shared.js" \
  --sample-subdir "spinning_cube" \
  --out-subdir "spinning-cube-browser" \
  --expected-status-text "Host running with cube.wasm and cube.wgsl bootstrap." \
  "$@"
