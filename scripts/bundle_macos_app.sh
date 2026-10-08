#!/usr/bin/env bash
# Builds Name.app from a PrimeStruct native UI program: the bytecode-only runner,
# the program compiled to bytecode (no compiler ships in the bundle), an
# Info.plist and an ad-hoc signature for local runs. Notarization is out of scope.
#
# Usage: scripts/bundle_macos_app.sh <app.prime> <Name> [output-dir] [build-dir]
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <app.prime> <Name> [output-dir] [build-dir]" >&2
  exit 64
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROGRAM="$1"
NAME="$2"
OUT_DIR="${3:-$ROOT_DIR/build-release/apps}"
BUILD_DIR="${4:-$ROOT_DIR/build-release}"
BUNDLE_ID="com.primestruct.${NAME//[^A-Za-z0-9]/-}"

for tool in primec primestruct_app_runtime; do
  if [[ ! -x "$BUILD_DIR/$tool" ]]; then
    echo "missing $BUILD_DIR/$tool; build it first (cmake --build $BUILD_DIR --target $tool)" >&2
    exit 1
  fi
done

APP="$OUT_DIR/$NAME.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

"$BUILD_DIR/primec" --emit=ir "$PROGRAM" -o "$APP/Contents/Resources/app.psir"
cp "$BUILD_DIR/primestruct_app_runtime" "$APP/Contents/MacOS/$NAME"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>$NAME</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSSupportsAutomaticTermination</key><false/>
  <key>NSSupportsSuddenTermination</key><false/>
</dict>
</plist>
PLIST

codesign --force --sign - "$APP" >/dev/null 2>&1 || echo "warning: ad-hoc signing failed" >&2
echo "$APP"
