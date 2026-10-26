#include "MarkdownEditInternal.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace primec::ui::markdown_edit {

namespace {

struct Line {
  size_t start = 0;
  size_t end = 0; // before the line break
};

std::vector<Line> splitLines(std::string_view text) {
  std::vector<Line> lines;
  size_t pos = 0;
  while (true) {
    const size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) {
      lines.push_back({pos, text.size()});
      break;
    }
    lines.push_back({pos, end});
    pos = end + 1;
  }
  return lines;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return s;
}

bool hasUnescapedPipe(std::string_view s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '|' && (i == 0 || s[i - 1] != '\\')) {
      return true;
    }
  }
  return false;
}

// The cells of a row, trimmed; the outer pipes are optional and a pipe in a cell is escaped as \|.
std::vector<std::string> splitCells(std::string_view row) {
  std::string_view s = trim(row);
  if (!s.empty() && s.front() == '|') {
    s.remove_prefix(1);
  }
  if (!s.empty() && s.back() == '|' && (s.size() < 2 || s[s.size() - 2] != '\\')) {
    s.remove_suffix(1);
  }
  std::vector<std::string> cells;
  std::string current;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '|') {
      current += "\\|";
      ++i;
    } else if (s[i] == '|') {
      cells.emplace_back(trim(current));
      current.clear();
    } else {
      current += s[i];
    }
  }
  cells.emplace_back(trim(current));
  return cells;
}

bool isDelimiterCell(std::string_view cell) {
  if (!cell.empty() && cell.front() == ':') {
    cell.remove_prefix(1);
  }
  if (!cell.empty() && cell.back() == ':') {
    cell.remove_suffix(1);
  }
  return !cell.empty() && std::all_of(cell.begin(), cell.end(), [](char c) { return c == '-'; });
}

bool isDelimiterRow(std::string_view row) {
  if (!hasUnescapedPipe(row)) {
    return false;
  }
  const std::vector<std::string> cells = splitCells(row);
  return std::all_of(cells.begin(), cells.end(), [](const std::string &c) { return isDelimiterCell(c); });
}

int alignOf(std::string_view cell) {
  const bool left = !cell.empty() && cell.front() == ':';
  const bool right = !cell.empty() && cell.back() == ':';
  return left && right ? 2 : (left ? 1 : (right ? 3 : 0));
}

// Columns a text takes: code points, wide characters (East Asian, emoji) count two, combining marks none.
size_t displayWidth(std::string_view s) {
  size_t width = 0;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    uint32_t cp = c;
    size_t n = 1;
    if (c >= 0xF0 && i + 3 < s.size() + 0) {
      cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
      n = 4;
    } else if (c >= 0xE0 && i + 2 < s.size()) {
      cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
      n = 3;
    } else if (c >= 0xC0 && i + 1 < s.size()) {
      cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
      n = 2;
    }
    i += n;
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0xFE00 && cp <= 0xFE0F)) {
      continue;
    }
    const bool wide = (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
                      (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
                      (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF) || (cp >= 0x20000 && cp <= 0x3FFFD);
    width += wide ? 2 : 1;
  }
  return width;
}

struct Table {
  size_t headerLine = 0;
  size_t lastLine = 0;
  std::vector<std::string> header;
  std::vector<int> align;
  std::vector<std::vector<std::string>> body;
};

// The table the line `caretLine` belongs to: a block of lines with pipes whose second line is a delimiter row with
// as many cells as the header (GitHub's rule), outside fenced code.
std::optional<Table> findTable(std::string_view text, const std::vector<Line> &lines, size_t caretLine) {
  std::vector<bool> inFence(lines.size(), false);
  bool fenced = false;
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string_view line = trim(text.substr(lines[i].start, lines[i].end - lines[i].start));
    if (line.substr(0, 3) == "```" || line.substr(0, 3) == "~~~") {
      inFence[i] = true;
      fenced = !fenced;
    } else {
      inFence[i] = fenced;
    }
  }
  const auto candidate = [&](size_t i) {
    const std::string_view line = text.substr(lines[i].start, lines[i].end - lines[i].start);
    return !inFence[i] && !trim(line).empty() && hasUnescapedPipe(line);
  };
  if (caretLine >= lines.size() || !candidate(caretLine)) {
    return std::nullopt;
  }
  size_t first = caretLine;
  while (first > 0 && candidate(first - 1)) {
    --first;
  }
  size_t last = caretLine;
  while (last + 1 < lines.size() && candidate(last + 1)) {
    ++last;
  }
  const auto lineText = [&](size_t i) { return text.substr(lines[i].start, lines[i].end - lines[i].start); };
  for (size_t h = first; h < last; ++h) {
    if (!isDelimiterRow(lineText(h + 1)) || splitCells(lineText(h)).size() != splitCells(lineText(h + 1)).size()) {
      continue;
    }
    if (caretLine < h) {
      return std::nullopt;
    }
    Table table;
    table.headerLine = h;
    table.lastLine = last;
    table.header = splitCells(lineText(h));
    for (const std::string &cell : splitCells(lineText(h + 1))) {
      table.align.push_back(alignOf(cell));
    }
    const size_t columns = table.header.size();
    for (size_t i = h + 2; i <= last; ++i) {
      std::vector<std::string> row = splitCells(lineText(i));
      row.resize(columns); // missing cells are empty, extra cells are not part of the table
      table.body.push_back(std::move(row));
    }
    return table;
  }
  return std::nullopt;
}

// Which column the cursor is in: the pipes before it, not counting a leading one.
size_t columnOf(std::string_view line, size_t offset, size_t columns) {
  size_t lead = 0;
  while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t')) {
    ++lead;
  }
  const bool leadingPipe = lead < line.size() && line[lead] == '|';
  size_t pipes = 0;
  for (size_t k = 0; k < offset && k < line.size(); ++k) {
    if (line[k] == '|' && (k == 0 || line[k - 1] != '\\')) {
      ++pipes;
    }
  }
  if (leadingPipe && offset > lead && pipes > 0) {
    --pipes;
  }
  return std::min(pipes, columns == 0 ? 0 : columns - 1);
}

struct Span {
  size_t start = 0;
  size_t length = 0;
};

struct Formatted {
  std::string text;
  std::vector<std::vector<Span>> cells; // [row][column], row 0 is the header and 1... the body; offsets in `text`
};

Formatted format(const std::vector<std::string> &header, const std::vector<int> &align,
                 const std::vector<std::vector<std::string>> &body) {
  const size_t columns = header.size();
  std::vector<size_t> widths(columns, 3);
  const auto widen = [&](const std::vector<std::string> &row) {
    for (size_t c = 0; c < columns; ++c) {
      widths[c] = std::max(widths[c], displayWidth(row[c]));
    }
  };
  widen(header);
  for (const auto &row : body) {
    widen(row);
  }
  std::vector<std::string> lines;
  std::vector<std::vector<Span>> spans;
  const auto addRow = [&](const std::vector<std::string> &row) {
    std::string line = "|";
    std::vector<Span> rowSpans;
    for (size_t c = 0; c < columns; ++c) {
      line += ' ';
      const size_t padding = widths[c] - displayWidth(row[c]);
      const size_t left = align[c] == 3 ? padding : (align[c] == 2 ? padding / 2 : 0);
      line.append(left, ' ');
      rowSpans.push_back({line.size(), row[c].size()});
      line += row[c];
      line.append(padding - left, ' ');
      line += " |";
    }
    lines.push_back(std::move(line));
    spans.push_back(std::move(rowSpans));
  };
  addRow(header);
  std::string delimiter = "|";
  for (size_t c = 0; c < columns; ++c) {
    std::string dashes(widths[c], '-');
    if (align[c] == 1 || align[c] == 2) {
      dashes.front() = ':';
    }
    if (align[c] == 2 || align[c] == 3) {
      dashes.back() = ':';
    }
    delimiter += " " + dashes + " |";
  }
  lines.push_back(std::move(delimiter));
  spans.emplace_back(); // the delimiter row has no cells to select
  for (const auto &row : body) {
    addRow(row);
  }
  Formatted out;
  size_t offset = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    for (Span &span : spans[i]) {
      span.start += offset;
    }
    out.text += (i == 0 ? "" : "\n") + lines[i];
    offset += lines[i].size() + 1;
  }
  out.cells.push_back(std::move(spans[0]));
  for (size_t i = 2; i < spans.size(); ++i) {
    out.cells.push_back(std::move(spans[i]));
  }
  return out;
}

Edit emit(std::string_view text, const std::vector<Line> &lines, const Table &table, size_t row, size_t column, bool whole) {
  const Formatted f = format(table.header, table.align, table.body);
  const size_t blockStart = lines[table.headerLine].start;
  const size_t blockEnd = lines[table.lastLine].end;
  std::string out(text.substr(0, blockStart));
  out += f.text;
  out += text.substr(blockEnd);
  row = std::min(row, f.cells.size() - 1);
  column = std::min(column, f.cells[row].size() - 1);
  const Span span = f.cells[row][column];
  Edit edit;
  edit.applied = true;
  edit.text = std::move(out);
  edit.selectionStart = blockStart + span.start;
  edit.selectionEnd = whole ? edit.selectionStart + span.length : edit.selectionStart;
  return edit;
}

Edit insertTable(std::string_view text, const std::vector<Line> &lines, size_t caretLine, int a, int b) {
  const size_t columns = static_cast<size_t>(std::clamp(a == 0 ? 3 : a, 1, 12));
  const size_t rows = static_cast<size_t>(std::clamp(b == 0 ? 2 : b, 1, 50));
  std::vector<std::string> header;
  for (size_t c = 0; c < columns; ++c) {
    header.push_back("Column " + std::to_string(c + 1));
  }
  const std::vector<int> align(columns, 0);
  const std::vector<std::vector<std::string>> body(rows, std::vector<std::string>(columns));
  const Formatted f = format(header, align, body);

  const std::optional<Table> inside = findTable(text, lines, caretLine);
  const size_t at = inside ? lines[inside->lastLine].end : lines[caretLine].end;
  const bool lineBlank = !inside && trim(text.substr(lines[caretLine].start, lines[caretLine].end - lines[caretLine].start)).empty();
  std::string lead;
  if (!lineBlank) {
    lead = "\n\n";
  } else if (caretLine > 0 && !trim(text.substr(lines[caretLine - 1].start, lines[caretLine - 1].end - lines[caretLine - 1].start)).empty()) {
    lead = "\n";
  }
  std::string trail;
  if (!(at >= text.size())) {
    const size_t next = caretLine + 1 + (inside ? inside->lastLine - caretLine : 0);
    if (next < lines.size() && !trim(text.substr(lines[next].start, lines[next].end - lines[next].start)).empty()) {
      trail = "\n";
    }
  }
  std::string out(text.substr(0, at));
  out += lead;
  out += f.text;
  out += trail;
  out += text.substr(at);
  Edit edit;
  edit.applied = true;
  edit.text = std::move(out);
  const Span first = f.cells[0][0];
  edit.selectionStart = at + lead.size() + first.start;
  edit.selectionEnd = edit.selectionStart + first.length;
  return edit;
}

} // namespace

Edit tableEdit(std::string_view text, size_t selectionStart, size_t selectionEnd, MarkdownEditOp op, int a, int b) {
  (void)selectionEnd;
  const std::vector<Line> lines = splitLines(text);
  size_t caretLine = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].start <= selectionStart && selectionStart <= lines[i].end) {
      caretLine = i;
      break;
    }
  }
  if (op == MarkdownEditOp::TableInsert) {
    return insertTable(text, lines, caretLine, a, b);
  }
  std::optional<Table> found = findTable(text, lines, caretLine);
  if (!found) {
    return Edit{};
  }
  Table table = std::move(*found);
  const size_t columns = table.header.size();
  const bool onDelimiter = caretLine == table.headerLine + 1;
  const size_t row = caretLine <= table.headerLine + 1 ? 0 : caretLine - table.headerLine - 1; // 0 header, 1... body
  const std::string_view caretText = text.substr(lines[caretLine].start, lines[caretLine].end - lines[caretLine].start);
  const size_t column = columnOf(caretText, selectionStart - lines[caretLine].start, columns);
  (void)onDelimiter;
  const auto emptyRow = [&] { return std::vector<std::string>(columns); };

  switch (op) {
  case MarkdownEditOp::TableFormat:
    return emit(text, lines, table, row, column, false);
  case MarkdownEditOp::TableAddRow: {
    table.body.insert(table.body.begin() + static_cast<std::ptrdiff_t>(row), emptyRow());
    return emit(text, lines, table, row + 1, 0, false);
  }
  case MarkdownEditOp::TableDeleteRow: {
    if (row == 0) {
      return Edit{};
    }
    table.body.erase(table.body.begin() + static_cast<std::ptrdiff_t>(row - 1));
    return emit(text, lines, table, std::min(row, table.body.size()), column, false);
  }
  case MarkdownEditOp::TableAddColumn: {
    const size_t at = column + 1;
    table.header.insert(table.header.begin() + static_cast<std::ptrdiff_t>(at), "Column " + std::to_string(columns + 1));
    table.align.insert(table.align.begin() + static_cast<std::ptrdiff_t>(at), 0);
    for (auto &r : table.body) {
      r.insert(r.begin() + static_cast<std::ptrdiff_t>(at), std::string());
    }
    return emit(text, lines, table, 0, at, true);
  }
  case MarkdownEditOp::TableDeleteColumn: {
    if (columns < 2) {
      return Edit{};
    }
    table.header.erase(table.header.begin() + static_cast<std::ptrdiff_t>(column));
    table.align.erase(table.align.begin() + static_cast<std::ptrdiff_t>(column));
    for (auto &r : table.body) {
      r.erase(r.begin() + static_cast<std::ptrdiff_t>(column));
    }
    return emit(text, lines, table, row, std::min(column, columns - 2), false);
  }
  case MarkdownEditOp::TableAlign:
    table.align[column] = std::clamp(a, 0, 3);
    return emit(text, lines, table, row, column, false);
  case MarkdownEditOp::TableNextCell: {
    size_t targetRow = row;
    size_t targetColumn = column + 1;
    if (targetColumn >= columns) {
      targetColumn = 0;
      ++targetRow;
    }
    if (targetRow > table.body.size()) {
      table.body.push_back(emptyRow()); // after the last cell: a new row
    }
    return emit(text, lines, table, targetRow, targetColumn, true);
  }
  case MarkdownEditOp::TablePreviousCell: {
    if (column == 0 && row == 0) {
      return Edit{};
    }
    const size_t targetRow = column == 0 ? row - 1 : row;
    const size_t targetColumn = column == 0 ? columns - 1 : column - 1;
    return emit(text, lines, table, targetRow, targetColumn, true);
  }
  default:
    return Edit{};
  }
}

} // namespace primec::ui::markdown_edit
