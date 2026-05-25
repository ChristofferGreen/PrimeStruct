#pragma once

#include "primec/Diagnostics.h"
#include "primec/ExpandedSource.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace primec {

struct SourceUnitLocation {
  std::size_t unitId = 0;
  SourceUnitKind unitKind = SourceUnitKind::Unknown;
  std::string file;
  std::string moduleKey;
  int line = 0;
  int column = 0;
};

std::optional<SourceUnitLocation> mapExpandedSourceLocation(
    const ExpandedSource &source,
    int flattenedLine,
    int flattenedColumn);

DiagnosticSpan mapDiagnosticSpanToSourceUnit(const ExpandedSource &source,
                                             const DiagnosticSpan &span);

void mapDiagnosticRecordSpansToSourceUnits(const ExpandedSource &source,
                                           DiagnosticSinkRecord &record);

void mapAndSortDiagnosticRecordsToSourceUnits(
    const ExpandedSource &source,
    std::vector<DiagnosticSinkRecord> &records);

void mapAndSortDiagnosticReportSpansToSourceUnits(
    const ExpandedSource &source,
    DiagnosticSinkReport &report);

} // namespace primec
