#include "primec/support/BenchmarkSink.h"

#include <iostream>
#include <mutex>
#include <utility>

namespace primec::support {

namespace {

std::mutex &sinkMutex() {
  static std::mutex mutex;
  return mutex;
}

BenchmarkSinkFn &sinkOverride() {
  static BenchmarkSinkFn sink;
  return sink;
}

} // namespace

void emitBenchmarkLine(std::string_view line) {
  std::lock_guard<std::mutex> lock(sinkMutex());
  if (sinkOverride()) {
    sinkOverride()(line);
    return;
  }
  std::cerr << line << std::endl;
}

BenchmarkSinkFn setBenchmarkSink(BenchmarkSinkFn sink) {
  std::lock_guard<std::mutex> lock(sinkMutex());
  BenchmarkSinkFn previous = std::move(sinkOverride());
  sinkOverride() = std::move(sink);
  return previous;
}

} // namespace primec::support
