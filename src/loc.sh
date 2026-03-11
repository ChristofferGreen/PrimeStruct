#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
targetDir="${1:-$scriptDir}"

if [[ ! -d "$targetDir" ]]; then
  echo "error: directory not found: $targetDir" >&2
  exit 1
fi

countLines() {
  local path="$1"
  shift
  local extraArgs=("$@")
  rg --files "$path" "${extraArgs[@]}" -0 | xargs -0 wc -l | awk 'END { print $1 }'
}

allFiles="$(rg --files "$targetDir" | wc -l | awk '{ print $1 }')"
allLines="$(countLines "$targetDir")"

codePattern='*.{c,cc,cpp,cxx,h,hh,hpp,hxx,tpp,inl}'
codeFiles="$(rg --files "$targetDir" -g "$codePattern" | wc -l | awk '{ print $1 }')"
codeLines=0
if [[ "$codeFiles" -gt 0 ]]; then
  codeLines="$(countLines "$targetDir" -g "$codePattern")"
fi

echo "Target: $targetDir"
echo "Files (all): $allFiles"
echo "Lines (all): $allLines"
echo "Files (C/C++): $codeFiles"
echo "Lines (C/C++): $codeLines"
