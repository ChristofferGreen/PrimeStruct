#!/usr/bin/env bash
# Builds Name.app from a PrimeStruct native UI program. By default the bundle holds the
# bytecode-only runner and the program compiled to bytecode (no compiler ships in the bundle);
# with --native it holds the program compiled to a native executable by primec's own native
# backend (scripts/build_native_app.sh --direct: no VM, no bytecode, no C++ compiler) and the AppKit
# backend as a dylib next to it in Contents/MacOS. Either way: an Info.plist and an ad-hoc
# signature for local runs. Notarization is out of scope.
#
# Usage: scripts/bundle_macos_app.sh [--native] <app.prime> <Name> [output-dir] [build-dir] [icon.icns]
# (scripts/make_app_icon.py draws the notepad's icon.)
set -euo pipefail

NATIVE=0
if [[ "${1:-}" == "--native" ]]; then
  NATIVE=1
  shift
fi

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 [--native] <app.prime> <Name> [output-dir] [build-dir] [icon.icns]" >&2
  exit 64
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROGRAM="$1"
NAME="$2"
OUT_DIR="${3:-$ROOT_DIR/build-release/apps}"
BUILD_DIR="${4:-$ROOT_DIR/build-release}"
ICON="${5:-}"
BUNDLE_ID="com.primestruct.${NAME//[^A-Za-z0-9]/-}"

TOOLS=(primec)
if [[ $NATIVE -eq 0 ]]; then
  TOOLS+=(primestruct_app_runtime)
fi
for tool in "${TOOLS[@]}"; do
  if [[ ! -x "$BUILD_DIR/$tool" ]]; then
    echo "missing $BUILD_DIR/$tool; build it first (cmake --build $BUILD_DIR --target $tool)" >&2
    exit 1
  fi
done

APP="$OUT_DIR/$NAME.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

if [[ $NATIVE -eq 1 ]]; then
  bash "$ROOT_DIR/scripts/build_native_app.sh" "$PROGRAM" "$APP/Contents/MacOS/$NAME" \
    --backend appkit --direct --build-dir "$BUILD_DIR" >/dev/null
else
  "$BUILD_DIR/primec" --emit=ir "$PROGRAM" -o "$APP/Contents/Resources/app.psir"
  cp "$BUILD_DIR/primestruct_app_runtime" "$APP/Contents/MacOS/$NAME"
fi

# The script runner (docs/NativeUiPlan.md section 18): the interpreter behind Run, loaded on the first run from beside
# the backend's dylib, and the stdlib the scripts import from.
if [[ $NATIVE -eq 1 && -f "$BUILD_DIR/libps_script.dylib" ]]; then
  cp "$BUILD_DIR/libps_script.dylib" "$APP/Contents/MacOS/libps_script.dylib"
  cp -R "$ROOT_DIR/stdlib" "$APP/Contents/Resources/stdlib"
fi

ICON_KEY=""
if [[ -n "$ICON" ]]; then
  cp "$ICON" "$APP/Contents/Resources/AppIcon.icns"
  ICON_KEY="<key>CFBundleIconFile</key><string>AppIcon</string>"
fi

# The file types the program declares (file_types.txt next to it, or PS_DOCUMENT_TYPES) become the bundle's
# document types and the types it exports (scripts/document_types_plist.py). PS_NO_DOCUMENT_TYPES=1 leaves them out:
# a bundle that declares types can become the system's handler of the user's files, so tests that do not look at
# the types do not declare them.
DOCUMENT_KEYS=""
TYPES_FILE="${PS_DOCUMENT_TYPES:-$(dirname "$PROGRAM")/file_types.txt}"
if [[ -z "${PS_NO_DOCUMENT_TYPES:-}" && -f "$TYPES_FILE" ]]; then
  DOCUMENT_KEYS="$(python3 "$ROOT_DIR/scripts/document_types_plist.py" "$TYPES_FILE")"
fi

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>$NAME</string>
  $ICON_KEY
  $DOCUMENT_KEYS
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

# The dylib of a native bundle is signed first, then the bundle seals it with the executable.
if [[ $NATIVE -eq 1 ]]; then
  codesign --force --sign - "$APP/Contents/MacOS/libps_ui_appkit.dylib" >/dev/null 2>&1 \
    || echo "warning: ad-hoc signing of the dylib failed" >&2
  if [[ -f "$APP/Contents/MacOS/libps_script.dylib" ]]; then
    codesign --force --sign - "$APP/Contents/MacOS/libps_script.dylib" >/dev/null 2>&1 \
      || echo "warning: ad-hoc signing of the script runner failed" >&2
  fi
fi
codesign --force --sign - "$APP" >/dev/null 2>&1 || echo "warning: ad-hoc signing failed" >&2
echo "$APP"
