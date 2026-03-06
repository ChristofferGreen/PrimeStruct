#!/usr/bin/env bash
set -euo pipefail

PREFIX="[native-window-preflight]"

fail() {
  echo "${PREFIX} ERROR: $1" >&2
  exit 2
}

if [[ $# -gt 0 ]]; then
  fail "unknown arg: $1"
fi

if ! command -v xcrun >/dev/null 2>&1; then
  fail "xcrun unavailable; install Xcode Command Line Tools and rerun."
fi

metalPath="$(xcrun --find metal 2>/dev/null || true)"
if [[ -z "$metalPath" ]]; then
  fail "xcrun metal unavailable; run 'xcrun --find metal' after installing Command Line Tools."
fi

metallibPath="$(xcrun --find metallib 2>/dev/null || true)"
if [[ -z "$metallibPath" ]]; then
  fail "xcrun metallib unavailable; run 'xcrun --find metallib' after installing Command Line Tools."
fi

if ! command -v launchctl >/dev/null 2>&1; then
  fail "launchctl unavailable; native window host requires a macOS GUI login session."
fi

userId="$(id -u 2>/dev/null || true)"
if [[ -z "$userId" ]]; then
  fail "unable to determine uid for GUI session validation."
fi

if ! launchctl print "gui/$userId" >/dev/null 2>&1; then
  fail "GUI session unavailable; log in via macOS desktop session and rerun."
fi

echo "${PREFIX} xcrun metal: ${metalPath}"
echo "${PREFIX} xcrun metallib: ${metallibPath}"
echo "${PREFIX} GUI session: gui/${userId}"
echo "${PREFIX} PASS: prerequisites satisfied"
