#!/usr/bin/env bash
# Verifies the embedding libraries build and pass their tests without process
# spawning (the iOS constraint), and that no forbidden symbol is referenced.
# Optional CI helper; not part of the default gate (it configures two extra
# build trees: build-embed-noproc and build-embed-only).
#
#   scripts/check_embed_no_process.sh [--jobs N]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
if [[ "${1:-}" == "--jobs" ]]; then JOBS="$2"; fi

FORBIDDEN='posix_spawn|posix_spawnp|fork|vfork|execv|execvp|execve|system|popen|waitpid|mprotect|dlopen'

check_symbols() {
  local build="$1"
  local found=0
  for lib in primec_support_lib primec_frontend_lib primec_ir_core_lib primec_ir_lib primec_runtime_lib \
             primec_embed_runtime_lib primec_embed_lib; do
    local archive="$build/lib${lib}.a"
    [[ -f "$archive" ]] || { echo "missing $archive" >&2; exit 1; }
    if nm -A "$archive" 2>/dev/null | grep -E " U _?(${FORBIDDEN})\$" ; then
      found=1
    fi
  done
  if [[ $found -ne 0 ]]; then
    echo "FAIL: forbidden process/executable-memory symbols referenced (see above)" >&2
    exit 1
  fi
  echo "ok: no forbidden symbols in $build"
}

echo "== build with PRIMESTRUCT_EMBED_NO_PROCESS=ON (tests enabled)"
cmake -S "$ROOT" -B "$ROOT/build-embed-noproc" -DCMAKE_BUILD_TYPE=Release -DPRIMESTRUCT_EMBED_NO_PROCESS=ON >/dev/null
cmake --build "$ROOT/build-embed-noproc" --target PrimeStruct_embed_tests PrimeStruct_embed_runtime_tests -j "$JOBS"
check_symbols "$ROOT/build-embed-noproc"
(cd "$ROOT/build-embed-noproc" && ctest -R "PrimeStruct_embed_(script_engine|diagnostics|exports|bytecode|threads|lifetime|host_calls|host_language|no_process|runtime_only)$" --output-on-failure -j "$JOBS")

echo "== build with PRIMESTRUCT_EMBED_ONLY=ON (no CLI tools, no tests)"
cmake -S "$ROOT" -B "$ROOT/build-embed-only" -DCMAKE_BUILD_TYPE=Release \
      -DPRIMESTRUCT_EMBED_ONLY=ON -DPRIMESTRUCT_EMBED_NO_PROCESS=ON >/dev/null
cmake --build "$ROOT/build-embed-only" --target primec_embed_lib primec_embed_runtime_lib -j "$JOBS"
check_symbols "$ROOT/build-embed-only"
if cmake --build "$ROOT/build-embed-only" --target help | grep -E "^\.\.\. (primec|primevm|PrimeStruct_[a-z_]*tests)$"; then
  echo "FAIL: embed-only build still defines CLI/test targets" >&2
  exit 1
fi
echo "ok: embed-only build defines no CLI or test targets"
echo "check_embed_no_process: all checks passed"
