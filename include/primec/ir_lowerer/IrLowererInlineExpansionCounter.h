#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace primec::ir_lowerer {

// Counts what each call site of a definition emits, for the inline report. A call site brackets its emission with
// `InlineExpansionScope`; the instructions of a nested call site count for that call site only, so the totals of
// all definitions add up to the instructions the call sites emitted.
class InlineExpansionCounter {
public:
  struct Totals {
    uint64_t callSites = 0;
    uint64_t added = 0;
    uint64_t withCallees = 0;
  };

  void enter(size_t instructionCount);
  void leave(const std::string &calleePath, size_t instructionCount);
  // Sorted by path, so the report does not depend on hash order.
  const std::map<std::string, Totals> &totals() const {
    return totals_;
  }

private:
  struct Frame {
    size_t start = 0;
    uint64_t calleeInstructions = 0;
  };
  std::vector<Frame> frames_;
  std::map<std::string, Totals> totals_;
};

// Brackets one call site of `calleePath`; `instructionCount` reads the size of the function being emitted.
template <typename CountFn> class InlineExpansionScope {
public:
  InlineExpansionScope(InlineExpansionCounter &counter,
                       const std::string &calleePath,
                       CountFn instructionCount)
      : counter_(counter), calleePath_(calleePath), instructionCount_(std::move(instructionCount)) {
    counter_.enter(instructionCount_());
  }
  ~InlineExpansionScope() {
    counter_.leave(calleePath_, instructionCount_());
  }
  InlineExpansionScope(const InlineExpansionScope &) = delete;
  InlineExpansionScope &operator=(const InlineExpansionScope &) = delete;

private:
  InlineExpansionCounter &counter_;
  const std::string &calleePath_;
  CountFn instructionCount_;
};

} // namespace primec::ir_lowerer
