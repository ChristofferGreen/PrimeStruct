#!/usr/bin/env bash
# Launches a PrimeStruct app bundle once, invisibly, so its first real launch does not wait for Gatekeeper
# (TODO-5755, docs/NativeUiPlan.md section 1).
#
# The first exec of code macOS has not seen is held until syspolicyd has assessed it (a notarization-ticket lookup over
# the network, then an XProtect scan): 0.2 to 0.5 s for each new executable and dylib, seconds on a slow network. The
# verdict is kept for that file: a moved bundle (`mv`) keeps it, a copy (`ditto`, `cp`) does not. So run this on the
# bundle where it will be launched from: `scripts/bundle_macos_app.sh` runs it on what it builds, and an install that
# copies the bundle elsewhere runs it on the installed copy after its last `codesign`.
#
# The app runs in snapshot mode: no window on screen, no focus, no Dock icon, no preferences or session; it exits by
# itself once its window has been drawn. The script runner beside a native editor is started once without arguments
# (it prints its usage). macOS only; elsewhere this does nothing.
#
# Usage: scripts/warm_macos_app.sh <App.app>
set -euo pipefail

APP="${1:-}"
if [[ -z "$APP" || ! -d "$APP" ]]; then
  echo "Usage: $0 <App.app>" >&2
  exit 64
fi
[[ "$(uname -s)" == "Darwin" ]] || exit 0

EXE_NAME="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")"
WARM_DIR="$(mktemp -d -t primestruct_warmup)"
trap 'rm -rf "$WARM_DIR"' EXIT

PRIMESTRUCT_UI_SNAPSHOT="$WARM_DIR/warmup.png" "$APP/Contents/MacOS/$EXE_NAME" >/dev/null 2>&1 &
WARM_PID=$!
for ((i = 0; i < 300; i++)); do
  kill -0 "$WARM_PID" 2>/dev/null || break
  sleep 0.1
done
if kill -0 "$WARM_PID" 2>/dev/null; then
  kill "$WARM_PID" 2>/dev/null || true
  echo "warning: the warm-up launch of $APP did not end within 30 s (a program that shows no window?)" >&2
fi
wait "$WARM_PID" 2>/dev/null || true
if [[ -x "$APP/Contents/MacOS/PrimeScriptRunner" ]]; then
  "$APP/Contents/MacOS/PrimeScriptRunner" >/dev/null 2>&1 || true
fi
