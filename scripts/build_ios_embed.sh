#!/usr/bin/env bash
# Builds the PrimeStruct embedding libraries for iOS and packages XCFrameworks.
# macOS with Xcode only; NOT verified in the Linux CI (see docs/Embedding.md,
# "iOS"). Produces, under build-ios/:
#   PrimeStructEmbedRuntime.xcframework  - VM + bytecode loader only (recommended)
#   PrimeStructEmbed.xcframework         - also compiles scripts on device
#
#   scripts/build_ios_embed.sh [--deployment-target 15.0] [--no-simulator]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build-ios"
DEPLOYMENT_TARGET="15.0"
SIMULATOR=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --deployment-target) DEPLOYMENT_TARGET="$2"; shift 2 ;;
    --no-simulator) SIMULATOR=0; shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build_ios_embed.sh needs macOS and Xcode" >&2
  exit 1
fi

RUNTIME_LIBS=(primec_embed_runtime_lib primec_runtime_lib primec_ir_core_lib primec_support_lib)
FULL_LIBS=(primec_embed_lib primec_ir_lib primec_frontend_lib "${RUNTIME_LIBS[@]}")

build_slice() {
  local name="$1" sysroot="$2" archs="$3"
  local dir="$OUT/$name"
  cmake -S "$ROOT" -B "$dir" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT="$sysroot" \
        -DCMAKE_OSX_ARCHITECTURES="$archs" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPRIMESTRUCT_EMBED_ONLY=ON \
        -DPRIMESTRUCT_EMBED_NO_PROCESS=ON \
        -DPRIMESTRUCT_USE_MIMALLOC=OFF
  cmake --build "$dir" --target primec_embed_lib primec_embed_runtime_lib -j "$(sysctl -n hw.ncpu)"
  for set in runtime full; do
    local libs=()
    if [[ "$set" == runtime ]]; then libs=("${RUNTIME_LIBS[@]}"); else libs=("${FULL_LIBS[@]}"); fi
    local inputs=()
    for lib in "${libs[@]}"; do inputs+=("$dir/lib${lib}.a"); done
    libtool -static -o "$dir/libPrimeStructEmbed-$set.a" "${inputs[@]}"
  done
}

rm -rf "$OUT"
mkdir -p "$OUT"
build_slice device iphoneos arm64
if [[ $SIMULATOR -eq 1 ]]; then
  build_slice simulator iphonesimulator "arm64;x86_64"
fi

HEADERS="$OUT/headers"
mkdir -p "$HEADERS/primec/embed"
cp "$ROOT"/include/primec/embed/*.h "$HEADERS/primec/embed/"

for set in runtime full; do
  name="PrimeStructEmbed"; [[ "$set" == runtime ]] && name="PrimeStructEmbedRuntime"
  args=(-library "$OUT/device/libPrimeStructEmbed-$set.a" -headers "$HEADERS")
  if [[ $SIMULATOR -eq 1 ]]; then
    args+=(-library "$OUT/simulator/libPrimeStructEmbed-$set.a" -headers "$HEADERS")
  fi
  xcodebuild -create-xcframework "${args[@]}" -output "$OUT/$name.xcframework"
done

# The full library needs the stdlib at run time; ship it as a folder reference.
cp -R "$ROOT/stdlib" "$OUT/stdlib"
echo "Done: $OUT/PrimeStructEmbedRuntime.xcframework and $OUT/PrimeStructEmbed.xcframework"
echo "Add the stdlib folder ($OUT/stdlib) to the app bundle and call ScriptEngine::setStdlibPath(...) if you compile on device."
