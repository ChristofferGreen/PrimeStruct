#pragma once

#include "IrLowererRecursionAnalysis.h"
#include "primec/ir/Ir.h"
#include "primec/ir_lowerer/IrLowererInlineExpansionCounter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::ir_lowerer {

// What the report needs of the lowering that ran.
struct InlineReportInput {
  const InlineExpansionCounter *counter = nullptr;
  const std::unordered_map<std::string, const Definition *> *definitionsByPath = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  const std::unordered_map<std::string, uint64_t> *realCallReservationIndex = nullptr;
  const Definition *entryDefinition = nullptr;
  bool allowStructs = false;
  bool allowStrings = false;
};

// One entry per definition the counter saw, with `realCall` or the blocker the shape check names. A definition with
// no blocker that is still expanded is a tiny leaf (cheaper inline than called), the entry point, or one whose
// real-call reservation the target refused.
std::vector<IrInlineReportEntry> buildInlineReport(const InlineReportInput &input);

} // namespace primec::ir_lowerer
