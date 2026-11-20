#pragma once

#include "primec/support/Diagnostics.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/support/TextFilterOffsetMap.h"

#include <cstddef>
#include <memory>
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

struct SourceLocationMapperLookupStats {
  std::size_t indexedLookupCount = 0;
  std::size_t indexedSegmentCount = 0;
  std::size_t lineBucketCount = 0;
  std::size_t segmentCandidateVisitCount = 0;
  std::size_t maxSegmentCandidatesVisitedPerLookup = 0;
};

class SourceLocationMapper {
public:
  explicit SourceLocationMapper(const ExpandedSource &source);

  std::optional<SourceUnitLocation> mapExpandedSourceLocation(
      int flattenedLine,
      int flattenedColumn) const;

  // The same for a position of the parser's text (`ExpandedSource::textFilterOptions`): it is moved to the text the
  // user wrote first, which builds the offset map on the first call. Diagnostics use this; the instruction source
  // ranges of the lowerer keep `mapExpandedSourceLocation` and never build the map.
  std::optional<SourceUnitLocation> mapFilteredSourceLocation(int filteredLine,
                                                              int filteredColumn) const;

  DiagnosticSpan mapDiagnosticSpanToSourceUnit(
      const DiagnosticSpan &span) const;

  void mapDiagnosticRecordSpansToSourceUnits(
      DiagnosticSinkRecord &record) const;

  void mapAndSortDiagnosticRecordsToSourceUnits(
      std::vector<DiagnosticSinkRecord> &records) const;

  void mapAndSortDiagnosticReportSpansToSourceUnits(
      DiagnosticSinkReport &report) const;

  const SourceLocationMapperLookupStats &lookupStats() const;

private:
  struct LineBucket {
    int line = 0;
    std::vector<std::size_t> openSegmentIndices = {};
    std::vector<std::size_t> closedEndSegmentIndices = {};
  };

  const LineBucket *bucketForLine(int line) const;

  // Moves a position of the text-filtered source to the same place in the source text (see
  // `ExpandedSource::textFilterOptions`); a position the map does not know stays as it is.
  void unfilterPosition(int &line, int &column) const;

  std::optional<std::size_t> findSegmentIndexForPoint(
      int line,
      int column) const;

  SourceUnitLocation mapLocationWithinSegment(const SourceSegment &segment,
                                              int flattenedLine,
                                              int flattenedColumn) const;

  const ExpandedSource &source_;
  std::vector<LineBucket> lineBuckets_;
  mutable SourceLocationMapperLookupStats lookupStats_;
  mutable bool filterMapBuilt_ = false;
  mutable std::unique_ptr<TextFilterOffsetMap> filterMap_;
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
