#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace primec::support {

// Single destination for `[benchmark-...] {json}` instrumentation lines
// (TODO-5407). Production code never writes to the standard streams directly; it formats
// one line (without trailing newline) and calls emitBenchmarkLine. The line
// format is parsed by scripts/benchmark*.sh and must not change.
void emitBenchmarkLine(std::string_view line);

// Replaces the destination (nullptr restores stderr). Intended for tests that
// capture instrumentation output; returns the previous override.
using BenchmarkSinkFn = std::function<void(std::string_view)>;
BenchmarkSinkFn setBenchmarkSink(BenchmarkSinkFn sink);

} // namespace primec::support
