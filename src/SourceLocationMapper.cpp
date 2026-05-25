#include "primec/SourceLocationMapper.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

namespace primec {
namespace {

bool isBefore(int leftLine, int leftColumn, int rightLine, int rightColumn) {
  return leftLine < rightLine || (leftLine == rightLine && leftColumn < rightColumn);
}

bool isBeforeOrEqual(int leftLine, int leftColumn, int rightLine, int rightColumn) {
  return leftLine < rightLine || (leftLine == rightLine && leftColumn <= rightColumn);
}

bool containsOpenPoint(const SourceSegment &segment, int line, int column) {
  return isBeforeOrEqual(segment.flattenedStartLine,
                         segment.flattenedStartColumn,
                         line,
                         column) &&
         isBefore(line,
                  column,
                  segment.flattenedEndLine,
                  segment.flattenedEndColumn);
}

bool containsClosedEndPoint(const SourceSegment &segment, int line, int column) {
  return segment.flattenedEndLine == line && segment.flattenedEndColumn == column;
}

bool hasOriginalSourceLocation(const SourceUnit &unit,
                               const SourceSegment &segment) {
  return unit.kind != SourceUnitKind::Generated &&
         segment.originalStartLine > 0 &&
         segment.originalStartColumn > 0;
}

std::string sourceUnitDisplayFile(const SourceUnit &unit) {
  if (!unit.displayPath.empty()) {
    return unit.displayPath;
  }
  return unit.moduleKey;
}

std::optional<std::size_t> findSegmentIndexForPoint(const ExpandedSource &source,
                                                    int line,
                                                    int column) {
  for (std::size_t index = 0; index < source.segments.size(); ++index) {
    const SourceSegment &segment = source.segments[index];
    if (segment.unitId >= source.units.size()) {
      continue;
    }
    if (!hasOriginalSourceLocation(source.units[segment.unitId], segment)) {
      continue;
    }
    if (containsOpenPoint(segment, line, column)) {
      return index;
    }
  }

  for (std::size_t index = source.segments.size(); index > 0; --index) {
    const SourceSegment &segment = source.segments[index - 1];
    if (segment.unitId >= source.units.size()) {
      continue;
    }
    if (!hasOriginalSourceLocation(source.units[segment.unitId], segment)) {
      continue;
    }
    if (containsClosedEndPoint(segment, line, column)) {
      return index - 1;
    }
  }
  return std::nullopt;
}

SourceUnitLocation mapLocationWithinSegment(const ExpandedSource &source,
                                            const SourceSegment &segment,
                                            int flattenedLine,
                                            int flattenedColumn) {
  const SourceUnit &unit = source.units[segment.unitId];
  SourceUnitLocation location;
  location.unitId = unit.id;
  location.unitKind = unit.kind;
  location.file = sourceUnitDisplayFile(unit);
  location.moduleKey = unit.moduleKey;
  location.line = segment.originalStartLine +
                  (flattenedLine - segment.flattenedStartLine);
  if (flattenedLine == segment.flattenedStartLine) {
    location.column = segment.originalStartColumn +
                      (flattenedColumn - segment.flattenedStartColumn);
  } else {
    location.column = flattenedColumn;
  }
  if (location.column <= 0) {
    location.column = 1;
  }
  return location;
}

int diagnosticOrderCoordinate(int value) {
  return value > 0 ? value : std::numeric_limits<int>::max();
}

struct DiagnosticRecordOrderKey {
  std::size_t unitId = std::numeric_limits<std::size_t>::max();
  int line = std::numeric_limits<int>::max();
  int column = std::numeric_limits<int>::max();
  std::string file;
  std::string message;
};

DiagnosticRecordOrderKey orderKeyForRecord(const ExpandedSource &source,
                                           const DiagnosticSinkRecord &record) {
  DiagnosticRecordOrderKey key;
  key.message = record.message;
  if (!record.hasPrimarySpan) {
    return key;
  }
  key.file = record.primarySpan.file;
  key.line = diagnosticOrderCoordinate(record.primarySpan.line);
  key.column = diagnosticOrderCoordinate(record.primarySpan.column);
  const std::optional<SourceUnitLocation> location =
      mapExpandedSourceLocation(source,
                                record.primarySpan.line,
                                record.primarySpan.column);
  if (!location.has_value()) {
    return key;
  }
  key.unitId = location->unitId;
  key.file = location->file;
  key.line = diagnosticOrderCoordinate(location->line);
  key.column = diagnosticOrderCoordinate(location->column);
  return key;
}

void refreshReportFromRecords(DiagnosticSinkReport &report) {
  if (report.records.empty()) {
    report.message.clear();
    report.primarySpan = {};
    report.relatedSpans.clear();
    report.hasPrimarySpan = false;
    return;
  }
  const DiagnosticSinkRecord &first = report.records.front();
  report.message = first.message;
  report.primarySpan = first.primarySpan;
  report.relatedSpans = first.relatedSpans;
  report.hasPrimarySpan = first.hasPrimarySpan;
}

} // namespace

std::optional<SourceUnitLocation> mapExpandedSourceLocation(
    const ExpandedSource &source,
    int flattenedLine,
    int flattenedColumn) {
  if (flattenedLine <= 0 || flattenedColumn <= 0) {
    return std::nullopt;
  }
  const std::optional<std::size_t> segmentIndex =
      findSegmentIndexForPoint(source, flattenedLine, flattenedColumn);
  if (!segmentIndex.has_value()) {
    return std::nullopt;
  }
  return mapLocationWithinSegment(source,
                                  source.segments[*segmentIndex],
                                  flattenedLine,
                                  flattenedColumn);
}

DiagnosticSpan mapDiagnosticSpanToSourceUnit(const ExpandedSource &source,
                                             const DiagnosticSpan &span) {
  const std::optional<SourceUnitLocation> start =
      mapExpandedSourceLocation(source, span.line, span.column);
  if (!start.has_value()) {
    return span;
  }

  DiagnosticSpan mapped = span;
  mapped.file = start->file;
  mapped.line = start->line;
  mapped.column = start->column;

  const int endLine = span.endLine > 0 ? span.endLine : span.line;
  const int endColumn = span.endColumn > 0 ? span.endColumn : span.column;
  const std::optional<SourceUnitLocation> end =
      mapExpandedSourceLocation(source, endLine, endColumn);
  if (end.has_value() && end->unitId == start->unitId) {
    mapped.endLine = end->line;
    mapped.endColumn = end->column;
  } else {
    mapped.endLine = mapped.line;
    mapped.endColumn = mapped.column;
  }
  return mapped;
}

void mapDiagnosticRecordSpansToSourceUnits(const ExpandedSource &source,
                                           DiagnosticSinkRecord &record) {
  if (record.hasPrimarySpan) {
    record.primarySpan = mapDiagnosticSpanToSourceUnit(source,
                                                       record.primarySpan);
  }
  for (auto &related : record.relatedSpans) {
    related.span = mapDiagnosticSpanToSourceUnit(source, related.span);
  }
}

void mapAndSortDiagnosticRecordsToSourceUnits(
    const ExpandedSource &source,
    std::vector<DiagnosticSinkRecord> &records) {
  std::stable_sort(records.begin(),
                   records.end(),
                   [&](const DiagnosticSinkRecord &left,
                       const DiagnosticSinkRecord &right) {
                     const DiagnosticRecordOrderKey leftKey =
                         orderKeyForRecord(source, left);
                     const DiagnosticRecordOrderKey rightKey =
                         orderKeyForRecord(source, right);
                     return std::tie(leftKey.unitId,
                                     leftKey.line,
                                     leftKey.column,
                                     leftKey.file,
                                     leftKey.message) <
                            std::tie(rightKey.unitId,
                                     rightKey.line,
                                     rightKey.column,
                                     rightKey.file,
                                     rightKey.message);
                   });
  for (auto &record : records) {
    mapDiagnosticRecordSpansToSourceUnits(source, record);
  }
}

void mapAndSortDiagnosticReportSpansToSourceUnits(
    const ExpandedSource &source,
    DiagnosticSinkReport &report) {
  if (!report.records.empty()) {
    mapAndSortDiagnosticRecordsToSourceUnits(source, report.records);
    refreshReportFromRecords(report);
    return;
  }
  if (report.hasPrimarySpan) {
    report.primarySpan = mapDiagnosticSpanToSourceUnit(source,
                                                       report.primarySpan);
  }
  for (auto &related : report.relatedSpans) {
    related.span = mapDiagnosticSpanToSourceUnit(source, related.span);
  }
}

} // namespace primec
