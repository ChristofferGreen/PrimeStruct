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

struct SegmentLineCandidate {
  int line = 0;
  std::size_t segmentIndex = 0;
};

bool candidateLineThenSegmentAscending(const SegmentLineCandidate &left,
                                       const SegmentLineCandidate &right) {
  return std::tie(left.line, left.segmentIndex) <
         std::tie(right.line, right.segmentIndex);
}

bool candidateLineThenSegmentDescending(const SegmentLineCandidate &left,
                                        const SegmentLineCandidate &right) {
  if (left.line != right.line) {
    return left.line < right.line;
  }
  return left.segmentIndex > right.segmentIndex;
}

bool isValidFlattenedRange(const SourceSegment &segment) {
  if (segment.flattenedStartLine <= 0 || segment.flattenedStartColumn <= 0 ||
      segment.flattenedEndLine <= 0 || segment.flattenedEndColumn <= 0) {
    return false;
  }
  return isBeforeOrEqual(segment.flattenedStartLine,
                         segment.flattenedStartColumn,
                         segment.flattenedEndLine,
                         segment.flattenedEndColumn);
}

void appendOpenLineCandidates(std::vector<SegmentLineCandidate> &entries,
                              const SourceSegment &segment,
                              std::size_t segmentIndex) {
  for (int line = segment.flattenedStartLine;; ++line) {
    entries.push_back(SegmentLineCandidate{.line = line,
                                           .segmentIndex = segmentIndex});
    if (line == segment.flattenedEndLine) {
      break;
    }
  }
}

std::vector<int> collectBucketLines(
    const std::vector<SegmentLineCandidate> &openEntries,
    const std::vector<SegmentLineCandidate> &closedEntries) {
  std::vector<int> lines;
  lines.reserve(openEntries.size() + closedEntries.size());
  for (const auto &entry : openEntries) {
    lines.push_back(entry.line);
  }
  for (const auto &entry : closedEntries) {
    lines.push_back(entry.line);
  }
  std::sort(lines.begin(), lines.end());
  lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
  return lines;
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

DiagnosticRecordOrderKey orderKeyForRecord(const SourceLocationMapper &mapper,
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
      mapper.mapExpandedSourceLocation(record.primarySpan.line,
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

SourceLocationMapper::SourceLocationMapper(const ExpandedSource &source)
    : source_(source) {
  std::vector<SegmentLineCandidate> openEntries;
  std::vector<SegmentLineCandidate> closedEntries;
  for (std::size_t index = 0; index < source_.segments.size(); ++index) {
    const SourceSegment &segment = source_.segments[index];
    if (segment.unitId >= source_.units.size()) {
      continue;
    }
    if (!hasOriginalSourceLocation(source_.units[segment.unitId], segment)) {
      continue;
    }
    if (!isValidFlattenedRange(segment)) {
      continue;
    }

    ++lookupStats_.indexedSegmentCount;
    appendOpenLineCandidates(openEntries, segment, index);
    closedEntries.push_back(SegmentLineCandidate{
        .line = segment.flattenedEndLine,
        .segmentIndex = index,
    });
  }

  std::sort(openEntries.begin(),
            openEntries.end(),
            candidateLineThenSegmentAscending);
  std::sort(closedEntries.begin(),
            closedEntries.end(),
            candidateLineThenSegmentDescending);
  const std::vector<int> lines = collectBucketLines(openEntries, closedEntries);
  lineBuckets_.reserve(lines.size());
  for (const int line : lines) {
    lineBuckets_.push_back(LineBucket{.line = line});
  }

  auto findBucket = [&](int line) {
    return std::lower_bound(lineBuckets_.begin(),
                            lineBuckets_.end(),
                            line,
                            [](const LineBucket &bucket, int targetLine) {
                              return bucket.line < targetLine;
                            });
  };

  for (const auto &entry : openEntries) {
    auto bucket = findBucket(entry.line);
    if (bucket != lineBuckets_.end() && bucket->line == entry.line) {
      bucket->openSegmentIndices.push_back(entry.segmentIndex);
    }
  }
  for (const auto &entry : closedEntries) {
    auto bucket = findBucket(entry.line);
    if (bucket != lineBuckets_.end() && bucket->line == entry.line) {
      bucket->closedEndSegmentIndices.push_back(entry.segmentIndex);
    }
  }
  lookupStats_.lineBucketCount = lineBuckets_.size();
}

const SourceLocationMapper::LineBucket *
SourceLocationMapper::bucketForLine(int line) const {
  const auto bucket = std::lower_bound(
      lineBuckets_.begin(),
      lineBuckets_.end(),
      line,
      [](const LineBucket &candidate, int targetLine) {
        return candidate.line < targetLine;
      });
  if (bucket == lineBuckets_.end() || bucket->line != line) {
    return nullptr;
  }
  return &*bucket;
}

std::optional<std::size_t> SourceLocationMapper::findSegmentIndexForPoint(
    int line,
    int column) const {
  if (line <= 0 || column <= 0) {
    return std::nullopt;
  }

  ++lookupStats_.indexedLookupCount;
  std::size_t candidateVisits = 0;
  auto finishLookup = [&]() {
    lookupStats_.maxSegmentCandidatesVisitedPerLookup =
        std::max(lookupStats_.maxSegmentCandidatesVisitedPerLookup,
                 candidateVisits);
  };

  const LineBucket *bucket = bucketForLine(line);
  if (bucket == nullptr) {
    finishLookup();
    return std::nullopt;
  }

  for (const std::size_t index : bucket->openSegmentIndices) {
    ++candidateVisits;
    ++lookupStats_.segmentCandidateVisitCount;
    if (index < source_.segments.size() &&
        containsOpenPoint(source_.segments[index], line, column)) {
      finishLookup();
      return index;
    }
  }

  for (const std::size_t index : bucket->closedEndSegmentIndices) {
    ++candidateVisits;
    ++lookupStats_.segmentCandidateVisitCount;
    if (index < source_.segments.size() &&
        containsClosedEndPoint(source_.segments[index], line, column)) {
      finishLookup();
      return index;
    }
  }

  finishLookup();
  return std::nullopt;
}

SourceUnitLocation SourceLocationMapper::mapLocationWithinSegment(
    const SourceSegment &segment,
    int flattenedLine,
    int flattenedColumn) const {
  const SourceUnit &unit = source_.units[segment.unitId];
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

std::optional<SourceUnitLocation> SourceLocationMapper::mapExpandedSourceLocation(
    int flattenedLine,
    int flattenedColumn) const {
  const std::optional<std::size_t> segmentIndex =
      findSegmentIndexForPoint(flattenedLine, flattenedColumn);
  if (!segmentIndex.has_value()) {
    return std::nullopt;
  }
  return mapLocationWithinSegment(source_.segments[*segmentIndex],
                                  flattenedLine,
                                  flattenedColumn);
}

DiagnosticSpan SourceLocationMapper::mapDiagnosticSpanToSourceUnit(
    const DiagnosticSpan &span) const {
  const std::optional<SourceUnitLocation> start =
      mapExpandedSourceLocation(span.line, span.column);
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
      mapExpandedSourceLocation(endLine, endColumn);
  if (end.has_value() && end->unitId == start->unitId) {
    mapped.endLine = end->line;
    mapped.endColumn = end->column;
  } else {
    mapped.endLine = mapped.line;
    mapped.endColumn = mapped.column;
  }
  return mapped;
}

void SourceLocationMapper::mapDiagnosticRecordSpansToSourceUnits(
    DiagnosticSinkRecord &record) const {
  if (record.hasPrimarySpan) {
    record.primarySpan = mapDiagnosticSpanToSourceUnit(record.primarySpan);
  }
  for (auto &related : record.relatedSpans) {
    related.span = mapDiagnosticSpanToSourceUnit(related.span);
  }
}

void SourceLocationMapper::mapAndSortDiagnosticRecordsToSourceUnits(
    std::vector<DiagnosticSinkRecord> &records) const {
  struct RecordWithOrderKey {
    DiagnosticSinkRecord record;
    DiagnosticRecordOrderKey key;
  };

  std::vector<RecordWithOrderKey> keyedRecords;
  keyedRecords.reserve(records.size());
  for (auto &record : records) {
    DiagnosticRecordOrderKey key = orderKeyForRecord(*this, record);
    keyedRecords.push_back(RecordWithOrderKey{
        .record = std::move(record),
        .key = std::move(key),
    });
  }

  std::stable_sort(
      keyedRecords.begin(),
      keyedRecords.end(),
      [](const RecordWithOrderKey &left, const RecordWithOrderKey &right) {
        return std::tie(left.key.unitId,
                        left.key.line,
                        left.key.column,
                        left.key.file,
                        left.key.message) <
               std::tie(right.key.unitId,
                        right.key.line,
                        right.key.column,
                        right.key.file,
                        right.key.message);
      });

  records.clear();
  records.reserve(keyedRecords.size());
  for (auto &entry : keyedRecords) {
    mapDiagnosticRecordSpansToSourceUnits(entry.record);
    records.push_back(std::move(entry.record));
  }
}

void SourceLocationMapper::mapAndSortDiagnosticReportSpansToSourceUnits(
    DiagnosticSinkReport &report) const {
  if (!report.records.empty()) {
    mapAndSortDiagnosticRecordsToSourceUnits(report.records);
    refreshReportFromRecords(report);
    return;
  }
  if (report.hasPrimarySpan) {
    report.primarySpan = mapDiagnosticSpanToSourceUnit(report.primarySpan);
  }
  for (auto &related : report.relatedSpans) {
    related.span = mapDiagnosticSpanToSourceUnit(related.span);
  }
}

const SourceLocationMapperLookupStats &SourceLocationMapper::lookupStats()
    const {
  return lookupStats_;
}

std::optional<SourceUnitLocation> mapExpandedSourceLocation(
    const ExpandedSource &source,
    int flattenedLine,
    int flattenedColumn) {
  const SourceLocationMapper mapper(source);
  return mapper.mapExpandedSourceLocation(flattenedLine, flattenedColumn);
}

DiagnosticSpan mapDiagnosticSpanToSourceUnit(const ExpandedSource &source,
                                             const DiagnosticSpan &span) {
  const SourceLocationMapper mapper(source);
  return mapper.mapDiagnosticSpanToSourceUnit(span);
}

void mapDiagnosticRecordSpansToSourceUnits(const ExpandedSource &source,
                                           DiagnosticSinkRecord &record) {
  const SourceLocationMapper mapper(source);
  mapper.mapDiagnosticRecordSpansToSourceUnits(record);
}

void mapAndSortDiagnosticRecordsToSourceUnits(
    const ExpandedSource &source,
    std::vector<DiagnosticSinkRecord> &records) {
  const SourceLocationMapper mapper(source);
  mapper.mapAndSortDiagnosticRecordsToSourceUnits(records);
}

void mapAndSortDiagnosticReportSpansToSourceUnits(
    const ExpandedSource &source,
    DiagnosticSinkReport &report) {
  const SourceLocationMapper mapper(source);
  mapper.mapAndSortDiagnosticReportSpansToSourceUnits(report);
}

} // namespace primec
