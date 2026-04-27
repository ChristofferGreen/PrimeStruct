#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/rerun_backend_shard.sh [--run] [--build-dir DIR] <slice>
       scripts/rerun_backend_shard.sh --list

Focused release-mode backend rerun helper.

Slices:
  vm-math    VM math compile-run backend shard

By default the helper prints the focused rerun commands. Pass --run to execute
the CTest rerun from the release build directory.
USAGE
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/build-release"
run=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        echo "error: --build-dir requires a value" >&2
        exit 2
      fi
      build_dir="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --list)
      echo "vm-math"
      exit 0
      ;;
    --run)
      run=1
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -ne 1 ]]; then
  usage >&2
  exit 2
fi

slice="$1"
if [[ "$build_dir" != /* ]]; then
  build_dir="$repo_root/$build_dir"
fi

case "$slice" in
  vm-math)
    ctest_regex='^PrimeStruct_primestruct_compile_run_vm_math_math_helpers_'
    binary='PrimeStruct_compile_run_tests'
    suite='primestruct.compile.run.vm.math'
    source_file='*test_compile_run_vm_math.cpp'
    first_case='1'
    last_case='30'
    ;;
  *)
    echo "error: unknown backend slice: $slice" >&2
    echo "known slices: vm-math" >&2
    exit 2
    ;;
esac

cat <<INFO
Slice: $slice
Build cwd: $build_dir
Release binary: $binary
CTest rerun:
  cd "$build_dir" && ctest -R '$ctest_regex' --output-on-failure
Direct binary rerun:
  cd "$build_dir" && ./$binary --test-suite=$suite --order-by=file --source-file='$source_file' --first=$first_case --last=$last_case
INFO

if [[ "$run" -eq 1 ]]; then
  cd "$build_dir"
  exec ctest -R "$ctest_regex" --output-on-failure
fi
