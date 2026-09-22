#!/usr/bin/env bash
# TODO-5235 arena poison-audit driver.
#
# Runs a doctest binary that was built with -DPRIMESTRUCT_ARENA_POISON_AUDIT=ON
# (which also forces -fsanitize=address and PRIMEC_TEST_ARENA_RESET_PER_CASE)
# as a series of SHORT-LIVED, SHARDED processes rather than one long-running
# process.
#
# Why sharding: a single process running 1700-3000 TEST_CASEs under full ASan
# instrumentation accumulates redzone/shadow state for the whole run and gets
# OOM-killed by this environment's memory cgroup (~14GB anon-rss observed, see
# docs/CompilerArenaAllocator.md's 2026-08-22 notes). Each shard is a fresh
# process with a bounded footprint, so the same exhaustive coverage is reached
# without any single process surviving the whole suite.
#
# A shard is CLEAN when its only ASan output is a LeakSanitizer
# "byte(s) leaked" summary - the compile arena never frees its chunks by
# design, so leak reports are expected and harmless. A shard FAILS when ASan
# reports use-after-poison (a stale read of memory an arena reset reclaimed),
# which is exactly what this audit exists to find.
#
# Usage:
#   scripts/run_arena_poison_audit.sh <test-binary> [shard-size] [build-dir]
#
# Examples:
#   scripts/run_arena_poison_audit.sh PrimeStruct_semantics_tests
#   scripts/run_arena_poison_audit.sh PrimeStruct_backend_ir_tests 200 build-audit
#
# Per-shard logs land in <build-dir>/arena-poison-audit/<binary>-<first>-<last>.log.
# Heavy-command rule (AGENTS.md): shards run strictly one at a time.

set -uo pipefail

BINARY_NAME="${1:-}"
SHARD_SIZE="${2:-200}"
BUILD_DIR="${3:-build-audit}"

if [[ -z "${BINARY_NAME}" ]]; then
  echo "usage: $0 <test-binary> [shard-size] [build-dir]" >&2
  exit 2
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}/${BUILD_DIR}" || {
  echo "error: build dir ${BUILD_DIR} not found (configure it with -DPRIMESTRUCT_ARENA_POISON_AUDIT=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo)" >&2
  exit 2
}

if [[ ! -x "./${BINARY_NAME}" ]]; then
  echo "error: ./${BINARY_NAME} not found in ${BUILD_DIR}" >&2
  exit 2
fi

LOG_DIR="arena-poison-audit"
mkdir -p "${LOG_DIR}"

# doctest --count prints the number of matching test cases.
TOTAL="$(./"${BINARY_NAME}" --count 2>/dev/null | grep -oE '[0-9]+' | tail -1)"
if [[ -z "${TOTAL}" ]]; then
  echo "error: could not determine TEST_CASE count for ${BINARY_NAME}" >&2
  exit 2
fi

echo "poison audit: ${BINARY_NAME}, ${TOTAL} test cases, shard size ${SHARD_SIZE}"

# symbolize=0 keeps the external llvm-symbolizer subprocess out of the shard's
# memory budget; halt_on_error=1 stops at the first poisoned access so the
# reported stack is the first (and therefore causally relevant) one. Re-run a
# failing shard WITHOUT symbolize=0 to get an authoritative symbolized stack -
# manually reconstructing frames from raw offsets with addr2line has produced
# misleading stacks in this investigation before.
export ASAN_OPTIONS="symbolize=0:halt_on_error=1"

FAILED_SHARDS=()
first=1
while (( first <= TOTAL )); do
  last=$(( first + SHARD_SIZE - 1 ))
  (( last > TOTAL )) && last="${TOTAL}"
  log="${LOG_DIR}/${BINARY_NAME}-${first}-${last}.log"
  ./"${BINARY_NAME}" --first="${first}" --last="${last}" >"${log}" 2>&1
  status=$?
  # Treat ANY AddressSanitizer error except LeakSanitizer's expected
  # "detected memory leaks" as a failure. Matching only "use-after-poison"
  # is not enough: a stack-buffer-overflow caused by an ODR violation is
  # reported as "unknown-crash" (the first byte written is addressable, the
  # tail of the write lands in a stack redzone), which an audit looking only
  # for poison hits silently passes over. TODO-5235 lost a round to exactly
  # that gap.
  if grep -q "ERROR: AddressSanitizer:" "${log}"; then
    echo "  shard ${first}-${last}: POISONED ACCESS (see ${BUILD_DIR}/${log})"
    FAILED_SHARDS+=("${first}-${last}")
  elif (( status != 0 )) && ! grep -q "byte(s) leaked" "${log}"; then
    echo "  shard ${first}-${last}: exit ${status}, no leak summary (see ${BUILD_DIR}/${log})"
    FAILED_SHARDS+=("${first}-${last}")
  else
    echo "  shard ${first}-${last}: clean"
  fi
  first=$(( last + 1 ))
done

if (( ${#FAILED_SHARDS[@]} > 0 )); then
  echo "poison audit FAILED for ${BINARY_NAME}: ${FAILED_SHARDS[*]}"
  exit 1
fi

echo "poison audit CLEAN for ${BINARY_NAME} (all shards)"
