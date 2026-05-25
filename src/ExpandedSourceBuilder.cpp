#include "ExpandedSourceBuilder.h"

#include <utility>

namespace primec {
namespace {

ExpandedSourceBuilder::Cursor cursorAfter(std::string_view text) {
  ExpandedSourceBuilder::Cursor cursor;
  for (char ch : text) {
    if (ch == '\n') {
      ++cursor.line;
      cursor.column = 1;
    } else {
      ++cursor.column;
    }
  }
  return cursor;
}

bool cursorBefore(int leftLine, int leftColumn, int rightLine, int rightColumn) {
  return leftLine < rightLine || (leftLine == rightLine && leftColumn < rightColumn);
}

} // namespace

ExpandedSourceBuilder::ExpandedSourceBuilder(ExpandedSource &source)
    : source_(source), cursor_(cursorAfter(source.text)) {}

std::size_t ExpandedSourceBuilder::addUnit(SourceUnitKind kind,
                                           std::string displayPath,
                                           std::string moduleKey,
                                           int originalStartLine,
                                           int originalStartColumn) {
  SourceUnit unit;
  unit.id = source_.units.size();
  unit.kind = kind;
  unit.displayPath = std::move(displayPath);
  unit.moduleKey = std::move(moduleKey);
  unit.originalStartLine = originalStartLine;
  unit.originalStartColumn = originalStartColumn;
  source_.units.push_back(std::move(unit));
  return source_.units.back().id;
}

void ExpandedSourceBuilder::appendSegment(std::size_t unitId,
                                          std::string_view text,
                                          int originalStartLine,
                                          int originalStartColumn) {
  if (text.empty() || unitId >= source_.units.size()) {
    return;
  }

  SourceSegment segment;
  segment.unitId = unitId;
  segment.flattenedStartLine = cursor_.line;
  segment.flattenedStartColumn = cursor_.column;
  segment.originalStartLine = originalStartLine;
  segment.originalStartColumn = originalStartColumn;

  source_.text.append(text.data(), text.size());
  advance(text);

  segment.flattenedEndLine = cursor_.line;
  segment.flattenedEndColumn = cursor_.column;
  source_.segments.push_back(segment);
  updateUnitRange(source_.units[unitId], segment);
}

std::size_t ExpandedSourceBuilder::appendGenerated(std::string_view text, std::string moduleKey) {
  const std::size_t unitId =
      addUnit(SourceUnitKind::Generated, {}, std::move(moduleKey), 0, 0);
  appendSegment(unitId, text, 0, 0);
  return unitId;
}

std::size_t ExpandedSourceBuilder::textSize() const {
  return source_.text.size();
}

bool ExpandedSourceBuilder::textEndsWithNewline() const {
  return !source_.text.empty() && source_.text.back() == '\n';
}

void ExpandedSourceBuilder::advance(std::string_view text) {
  for (char ch : text) {
    if (ch == '\n') {
      ++cursor_.line;
      cursor_.column = 1;
    } else {
      ++cursor_.column;
    }
  }
}

void ExpandedSourceBuilder::updateUnitRange(SourceUnit &unit, const SourceSegment &segment) {
  if (unit.flattenedStartLine == 0 ||
      cursorBefore(segment.flattenedStartLine,
                   segment.flattenedStartColumn,
                   unit.flattenedStartLine,
                   unit.flattenedStartColumn)) {
    unit.flattenedStartLine = segment.flattenedStartLine;
    unit.flattenedStartColumn = segment.flattenedStartColumn;
  }
  if (unit.flattenedEndLine == 0 ||
      cursorBefore(unit.flattenedEndLine,
                   unit.flattenedEndColumn,
                   segment.flattenedEndLine,
                   segment.flattenedEndColumn)) {
    unit.flattenedEndLine = segment.flattenedEndLine;
    unit.flattenedEndColumn = segment.flattenedEndColumn;
  }
}

} // namespace primec
