#include "IrLowererInlineReport.h"

namespace primec::ir_lowerer {

std::vector<IrInlineReportEntry> buildInlineReport(const InlineReportInput &input) {
  std::vector<IrInlineReportEntry> entries;
  if (input.counter == nullptr || input.definitionsByPath == nullptr ||
      input.structNames == nullptr) {
    return entries;
  }
  entries.reserve(input.counter->totals().size());
  for (const auto &[path, totals] : input.counter->totals()) {
    IrInlineReportEntry entry;
    entry.path = path;
    entry.callSites = totals.callSites;
    entry.instructionsAdded = totals.added;
    entry.instructionsWithCallees = totals.withCallees;
    const auto definitionIt = input.definitionsByPath->find(path);
    const Definition *definition =
        definitionIt == input.definitionsByPath->end() ? nullptr : definitionIt->second;
    const bool reserved = input.realCallReservationIndex != nullptr &&
                          input.realCallReservationIndex->count(path) != 0;
    if (reserved) {
      entry.realCall = true;
    } else if (definition != nullptr && definition == input.entryDefinition) {
      entry.blockerCode = "entry-point";
      entry.blockerText = "it is the program's entry point";
    } else if (definition == nullptr) {
      entry.blockerCode = "no-definition";
      entry.blockerText = "no definition of this path is in the program";
    } else {
      const RealCallBlocker blocker = realCallBlocker(*definition,
                                                      RealCallShapeContext{input.definitionsByPath,
                                                                           input.structNames,
                                                                           input.allowStructs,
                                                                           false,
                                                                           input.allowStrings,
                                                                           false,
                                                                           false});
      if (!blocker.code.empty()) {
        entry.blockerCode = blocker.code;
        entry.blockerText = blocker.text;
      } else if (isTinyInlineLeaf(*definition)) {
        entry.blockerCode = "tiny-leaf";
        entry.blockerText = "its body is small enough that expanding it costs less than calling it";
      } else {
        entry.blockerCode = "not-reserved";
        entry.blockerText = "its shape is a real call's, but the target did not reserve it";
      }
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

} // namespace primec::ir_lowerer
