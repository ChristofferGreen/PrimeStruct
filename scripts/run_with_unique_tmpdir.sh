#!/usr/bin/env bash

set -euo pipefail

base_tmpdir="${TMPDIR:-${TMP:-${TEMP:-/tmp}}}"
mkdir -p "$base_tmpdir"

unique_tmpdir="$(mktemp -d "${base_tmpdir%/}/primestruct-ctest.XXXXXX")"
cleanup() {
  rm -rf "$unique_tmpdir"
}
trap cleanup EXIT

export TMPDIR="$unique_tmpdir"
export TMP="$unique_tmpdir"
export TEMP="$unique_tmpdir"
export TEMPDIR="$unique_tmpdir"

"$@"
