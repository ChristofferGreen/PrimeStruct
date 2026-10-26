#pragma once

#include "primec/ui/MarkdownEdit.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace primec::ui::markdown_edit {

// An operation's answer before it is cut down to the part of the text that changed: the whole new text.
struct Edit {
  bool applied = false;
  std::string text;
  size_t selectionStart = 0;
  size_t selectionEnd = 0;
};

// The table operations (MarkdownTable.cpp).
Edit tableEdit(std::string_view text, size_t selectionStart, size_t selectionEnd, MarkdownEditOp op, int a, int b);

} // namespace primec::ui::markdown_edit
