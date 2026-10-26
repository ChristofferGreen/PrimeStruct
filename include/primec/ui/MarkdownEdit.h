#pragma once

// Markdown editing operations for the native UI's markdown toolbar (docs/NativeUiPlan.md section 17): the
// transformations a markdown editor offers as buttons and shortcuts (bold, a list, a table row, ...), written once as
// plain text in, plain text out, so every backend behaves the same and the behaviour is tested as strings. A backend
// asks for an operation on the text and the selection and applies the answer as one edit.
//
// Text is UTF-8 and offsets are byte offsets (the selection is [start, end), start == end is a caret). An operation
// that does not apply (a table operation with the cursor outside a table) answers `applied == false`.

#include <cstddef>
#include <string>
#include <string_view>

namespace primec::ui {

enum class MarkdownEditOp {
  Bold,          // ** around the selection or the word at the cursor, taken off again by a second use
  Italic,        // *
  Strike,        // ~~
  Code,          // `
  Link,          // [text](url) with the selection as the text, or as the URL when it is one
  Image,         // ![alt](url)
  Heading,       // a: level 1 to 6 on every line of the selection, 0 for a paragraph; the same level again removes it
  Bullets,       // "- " on every non-blank line; converts other list kinds; removed when every line has it
  Numbers,       // "1. ", "2. ", ...
  Tasks,         // "- [ ] "
  Quote,         // "> "
  CodeBlock,     // fences around the selected lines, or an empty block; the fences go again when they are there
  Rule,          // --- on a line of its own between blank lines
  TableInsert,   // a: columns (1 to 12), b: rows below the header (1 to 50)
  TableFormat,   // aligns the columns of the table around the cursor
  TableAddRow,   // below the cursor's row (the first body row from the header)
  TableAddColumn, // after the cursor's column
  TableDeleteRow,    // a body row
  TableDeleteColumn, // not the last column
  TableAlign,        // a: 0 none, 1 left, 2 center, 3 right, for the cursor's column
  TableNextCell,     // selects the next cell's text (a new row after the last cell)
  TablePreviousCell, // not applicable in the first cell
};

struct MarkdownEditResult {
  bool applied = false;
  // The bytes [replaceStart, replaceEnd) of the text become `replacement`; everything else is unchanged.
  size_t replaceStart = 0;
  size_t replaceEnd = 0;
  std::string replacement;
  // The selection in the changed text.
  size_t selectionStart = 0;
  size_t selectionEnd = 0;
};

MarkdownEditResult markdownEdit(std::string_view text, size_t selectionStart, size_t selectionEnd, MarkdownEditOp op,
                                int a = 0, int b = 0);

// The text with the change applied.
std::string applyMarkdownEdit(std::string_view text, const MarkdownEditResult &result);

} // namespace primec::ui
