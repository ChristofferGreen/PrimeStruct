#include "primec/ui/MarkdownEdit.h"

#include "MarkdownEditInternal.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace primec::ui {

namespace {

using markdown_edit::Edit;

bool isSpace(char c) { return c == ' ' || c == '\t'; }

bool isWordByte(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return std::isalnum(u) != 0 || c == '_' || u >= 0x80;
}

size_t lineStartOf(std::string_view text, size_t pos) {
  while (pos > 0 && text[pos - 1] != '\n') {
    --pos;
  }
  return pos;
}

size_t lineEndOf(std::string_view text, size_t pos) {
  while (pos < text.size() && text[pos] != '\n') {
    ++pos;
  }
  return pos;
}

bool isBlank(std::string_view line) {
  return std::all_of(line.begin(), line.end(), [](char c) { return isSpace(c) || c == '\r'; });
}

Edit made(std::string text, size_t selectionStart, size_t selectionEnd) {
  Edit edit;
  edit.applied = true;
  edit.text = std::move(text);
  edit.selectionStart = selectionStart;
  edit.selectionEnd = selectionEnd;
  return edit;
}

std::string splice(std::string_view text, size_t from, size_t to, std::string_view with) {
  std::string out(text.substr(0, from));
  out += with;
  out += text.substr(to);
  return out;
}

// ---- inline marks ---------------------------------------------------------------------------------------------

size_t countBefore(std::string_view text, size_t pos, char c) {
  size_t n = 0;
  while (n < pos && text[pos - 1 - n] == c) {
    ++n;
  }
  return n;
}

size_t countAfter(std::string_view text, size_t pos, char c) {
  size_t n = 0;
  while (pos + n < text.size() && text[pos + n] == c) {
    ++n;
  }
  return n;
}

// `ch` repeated `m` times around the selection (or the word at the cursor): added, or taken off when it is there.
// Stars are counted: of the stars around a selection an odd number means italic and two or more mean bold, so bold
// and italic together (three stars) are told apart.
Edit inlineMark(std::string_view text, size_t ss, size_t se, char ch, size_t m) {
  const size_t caret = ss;
  if (ss == se) {
    while (ss > 0 && isWordByte(text[ss - 1])) {
      --ss;
    }
    while (se < text.size() && isWordByte(text[se])) {
      ++se;
    }
  }
  while (ss < se && isSpace(text[ss])) {
    ++ss;
  }
  while (se > ss && isSpace(text[se - 1])) {
    --se;
  }
  const std::string marker(m, ch);
  if (ss == se) {
    const size_t at = std::min(caret, text.size());
    return made(splice(text, at, at, marker + marker), at + m, at + m);
  }
  const size_t before = countBefore(text, ss, ch);
  const size_t after = countAfter(text, se, ch);
  const size_t around = std::min(before, after);
  bool present = false;
  if (ch == '*') {
    present = m == 2 ? around >= 2 : around % 2 == 1;
  } else if (ch == '~') {
    present = around >= 2;
  } else {
    present = around >= 1;
  }
  if (present) {
    std::string out(text.substr(0, ss - m));
    out += text.substr(ss, se - ss);
    out += text.substr(se + m);
    return made(std::move(out), ss - m, se - m);
  }
  // The selection holds the marks itself: take them off.
  const size_t length = se - ss;
  bool inside = false;
  if (length >= 2 * m && text.substr(ss, m) == marker && text.substr(se - m, m) == marker) {
    inside = true;
    if (ch == '*' && m == 1) {
      inside = text[ss + 1] != '*' && (length < 3 || text[se - 2] != '*');
    }
  }
  if (inside) {
    std::string out(text.substr(0, ss));
    out += text.substr(ss + m, length - 2 * m);
    out += text.substr(se);
    return made(std::move(out), ss, se - 2 * m);
  }
  std::string out(text.substr(0, ss));
  out += marker;
  out += text.substr(ss, length);
  out += marker;
  out += text.substr(se);
  return made(std::move(out), ss + m, se + m);
}

bool looksLikeUrl(std::string_view s) {
  for (const char *prefix : {"http://", "https://", "mailto:", "ftp://"}) {
    if (s.rfind(prefix, 0) == 0 && s.find(' ') == std::string_view::npos) {
      return true;
    }
  }
  return false;
}

Edit linkLike(std::string_view text, size_t ss, size_t se, bool image) {
  if (ss == se) {
    while (ss > 0 && isWordByte(text[ss - 1])) {
      --ss;
    }
    while (se < text.size() && isWordByte(text[se])) {
      ++se;
    }
  }
  const std::string open = image ? "![" : "[";
  const std::string selected(text.substr(ss, se - ss));
  const std::string placeholder = image ? "alt" : "text";
  if (selected.empty() || looksLikeUrl(selected)) {
    const std::string target = selected.empty() ? "url" : selected;
    const std::string link = open + placeholder + "](" + target + ")";
    return made(splice(text, ss, se, link), ss + open.size(), ss + open.size() + placeholder.size());
  }
  const std::string link = open + selected + "](url)";
  const size_t urlStart = ss + open.size() + selected.size() + 2;
  return made(splice(text, ss, se, link), urlStart, urlStart + 3);
}

// ---- line formats ---------------------------------------------------------------------------------------------

enum class LineKind { None, Heading, Bullet, Task, Number, Quote };

struct LinePrefix {
  LineKind kind = LineKind::None;
  size_t indent = 0; // bytes of leading spaces
  size_t length = 0; // bytes up to the content (the indent and the marker)
  int level = 0;     // heading level
};

LinePrefix parsePrefix(std::string_view line) {
  LinePrefix prefix;
  size_t i = 0;
  while (i < line.size() && isSpace(line[i])) {
    ++i;
  }
  prefix.indent = i;
  prefix.length = i;
  if (i >= line.size()) {
    return prefix;
  }
  const char c = line[i];
  if (c == '#') {
    size_t j = i;
    while (j < line.size() && line[j] == '#') {
      ++j;
    }
    if (j - i <= 6 && (j == line.size() || line[j] == ' ')) {
      prefix.kind = LineKind::Heading;
      prefix.level = static_cast<int>(j - i);
      prefix.length = j + (j < line.size() ? 1 : 0);
    }
    return prefix;
  }
  if (c == '>') {
    prefix.kind = LineKind::Quote;
    prefix.length = i + 1 + (i + 1 < line.size() && line[i + 1] == ' ' ? 1 : 0);
    return prefix;
  }
  if ((c == '-' || c == '*' || c == '+') && i + 1 < line.size() && line[i + 1] == ' ') {
    const size_t j = i + 2;
    const bool task = line.size() >= j + 3 && line[j] == '[' && (line[j + 1] == ' ' || line[j + 1] == 'x' || line[j + 1] == 'X') &&
                      line[j + 2] == ']' && (line.size() == j + 3 || line[j + 3] == ' ');
    if (task) {
      prefix.kind = LineKind::Task;
      prefix.length = j + 3 + (line.size() > j + 3 ? 1 : 0);
    } else {
      prefix.kind = LineKind::Bullet;
      prefix.length = j;
    }
    return prefix;
  }
  size_t j = i;
  while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j])) != 0) {
    ++j;
  }
  if (j > i && j < line.size() && (line[j] == '.' || line[j] == ')') && (j + 1 == line.size() || line[j + 1] == ' ')) {
    prefix.kind = LineKind::Number;
    prefix.length = j + 1 + (j + 1 < line.size() ? 1 : 0);
  }
  return prefix;
}

struct Transformed {
  std::string text;
  size_t oldPrefix = 0;
  size_t newPrefix = 0;
};

struct Block {
  size_t start = 0; // the first byte of the first line
  size_t end = 0;   // the end of the last line (before its line break)
  std::vector<std::string> lines;
};

Block blockOf(std::string_view text, size_t ss, size_t se) {
  Block block;
  block.start = lineStartOf(text, ss);
  size_t last = se;
  if (se > ss && se > 0 && text[se - 1] == '\n') {
    last = se - 1; // the selection ends after a line break: that line is not part of it
  }
  block.end = lineEndOf(text, last);
  size_t pos = block.start;
  while (true) {
    const size_t end = lineEndOf(text, pos);
    block.lines.emplace_back(text.substr(pos, std::min(end, block.end) - pos));
    if (end >= block.end) {
      break;
    }
    pos = end + 1;
  }
  return block;
}

// Applies `transform` to the non-blank lines of the block (to the first line when they are all blank).
template <class Transform> Edit lineEdit(std::string_view text, size_t ss, size_t se, Transform transform) {
  Block block = blockOf(text, ss, se);
  const bool allBlank = std::all_of(block.lines.begin(), block.lines.end(), [](const std::string &l) { return isBlank(l); });
  std::string joined;
  size_t caretLine = 0;
  Transformed caretTransformed;
  for (size_t i = 0; i < block.lines.size(); ++i) {
    const bool apply = allBlank ? i == 0 : !isBlank(block.lines[i]);
    Transformed t = apply ? transform(block.lines[i], i) : Transformed{block.lines[i], 0, 0};
    if (i == caretLine) {
      caretTransformed = t;
    }
    joined += (i == 0 ? "" : "\n") + t.text;
  }
  std::string out = splice(text, block.start, block.end, joined);
  if (ss != se) {
    return made(std::move(out), block.start, block.start + joined.size());
  }
  const size_t column = ss - block.start;
  const size_t newColumn =
      column <= caretTransformed.oldPrefix ? caretTransformed.newPrefix : column - caretTransformed.oldPrefix + caretTransformed.newPrefix;
  const size_t at = block.start + std::min(newColumn, joined.size());
  return made(std::move(out), at, at);
}

std::string indentOf(std::string_view line, const LinePrefix &p) { return std::string(line.substr(0, p.indent)); }

bool everyNonBlank(const Block &block, LineKind kind, int level = -1) {
  bool any = false;
  for (const std::string &line : block.lines) {
    if (isBlank(line)) {
      continue;
    }
    const LinePrefix p = parsePrefix(line);
    if (p.kind != kind || (level >= 0 && p.level != level)) {
      return false;
    }
    any = true;
  }
  return any;
}

Edit headingEdit(std::string_view text, size_t ss, size_t se, int level) {
  level = std::clamp(level, 0, 6);
  const Block block = blockOf(text, ss, se);
  const bool remove = level == 0 || everyNonBlank(block, LineKind::Heading, level);
  return lineEdit(text, ss, se, [&](const std::string &line, size_t) {
    const LinePrefix p = parsePrefix(line);
    const std::string content = line.substr(p.kind == LineKind::None ? p.indent : p.length);
    if (remove) {
      if (p.kind != LineKind::Heading) {
        return Transformed{line, 0, 0};
      }
      return Transformed{content, p.length, 0};
    }
    const std::string marker = std::string(static_cast<size_t>(level), '#') + " ";
    return Transformed{marker + content, p.length, marker.size()};
  });
}

enum class ListKind { Bullets, Numbers, Tasks };

Edit listEdit(std::string_view text, size_t ss, size_t se, ListKind kind) {
  const Block block = blockOf(text, ss, se);
  const LineKind own = kind == ListKind::Bullets ? LineKind::Bullet : kind == ListKind::Numbers ? LineKind::Number : LineKind::Task;
  const bool remove = everyNonBlank(block, own);
  size_t number = 0;
  return lineEdit(text, ss, se, [&](const std::string &line, size_t) {
    const LinePrefix p = parsePrefix(line);
    const std::string indent = indentOf(line, p);
    const std::string content = line.substr(p.length);
    if (remove) {
      return Transformed{indent + content, p.length, indent.size()};
    }
    if (kind == ListKind::Tasks && p.kind == LineKind::Task) {
      ++number;
      return Transformed{line, 0, 0};
    }
    std::string marker;
    if (kind == ListKind::Bullets) {
      marker = "- ";
    } else if (kind == ListKind::Tasks) {
      marker = "- [ ] ";
    } else {
      marker = std::to_string(++number) + ". ";
    }
    return Transformed{indent + marker + content, p.length, indent.size() + marker.size()};
  });
}

Edit quoteEdit(std::string_view text, size_t ss, size_t se) {
  const Block block = blockOf(text, ss, se);
  const bool remove = everyNonBlank(block, LineKind::Quote);
  return lineEdit(text, ss, se, [&](const std::string &line, size_t) {
    if (remove) {
      const LinePrefix p = parsePrefix(line);
      return Transformed{line.substr(p.length), p.length, 0};
    }
    return Transformed{"> " + line, 0, 2};
  });
}

bool isFence(std::string_view line) {
  size_t i = 0;
  while (i < line.size() && isSpace(line[i])) {
    ++i;
  }
  return line.substr(i, 3) == "```" || line.substr(i, 3) == "~~~";
}

Edit codeBlockEdit(std::string_view text, size_t ss, size_t se) {
  const size_t ls = lineStartOf(text, ss);
  if (ss == se) {
    const size_t le = lineEndOf(text, ss);
    if (ls == le) {
      return made(splice(text, ls, le, "```\n\n```"), ls + 4, ls + 4);
    }
    return made(splice(text, le, le, "\n\n```\n\n```"), le + 6, le + 6);
  }
  const Block block = blockOf(text, ss, se);
  if (block.start > 0 && block.end < text.size()) {
    const size_t previousStart = lineStartOf(text, block.start - 1);
    const size_t nextEnd = lineEndOf(text, block.end + 1);
    if (isFence(text.substr(previousStart, block.start - 1 - previousStart)) && isFence(text.substr(block.end + 1, nextEnd - block.end - 1))) {
      std::string out(text.substr(0, previousStart));
      out += text.substr(block.start, block.end - block.start);
      out += text.substr(nextEnd);
      return made(std::move(out), previousStart, previousStart + (block.end - block.start));
    }
  }
  std::string out(text.substr(0, block.start));
  out += "```\n";
  out += text.substr(block.start, block.end - block.start);
  out += "\n```";
  out += text.substr(block.end);
  return made(std::move(out), block.start + 4, block.start + 4 + (block.end - block.start));
}

Edit ruleEdit(std::string_view text, size_t ss) {
  const size_t ls = lineStartOf(text, ss);
  const size_t le = lineEndOf(text, ss);
  const bool lineBlank = isBlank(text.substr(ls, le - ls));
  std::string lead;
  if (!lineBlank) {
    lead = "\n\n";
  } else if (ls > 0) {
    const size_t previousStart = lineStartOf(text, ls - 1);
    if (!isBlank(text.substr(previousStart, ls - 1 - previousStart))) {
      lead = "\n";
    }
  }
  std::string trail;
  if (le < text.size()) {
    const size_t nextStart = le + 1;
    const size_t nextEnd = lineEndOf(text, nextStart);
    if (nextStart <= text.size() && nextStart < text.size() && !isBlank(text.substr(nextStart, nextEnd - nextStart))) {
      trail = "\n";
    }
  }
  // At the end of the text the caret gets a line of its own below the rule.
  const bool atEnd = le >= text.size();
  const std::string inserted = lead + "---" + trail + (atEnd ? "\n" : "");
  Edit edit = made(splice(text, le, le, inserted), 0, 0);
  const size_t caret = atEnd ? le + inserted.size() : le + lead.size() + 3 + 1;
  edit.selectionStart = edit.selectionEnd = std::min(caret, edit.text.size());
  return edit;
}

MarkdownEditResult finish(std::string_view oldText, const Edit &edit) {
  MarkdownEditResult result;
  if (!edit.applied) {
    return result;
  }
  result.applied = true;
  const std::string &now = edit.text;
  const size_t limit = std::min(oldText.size(), now.size());
  size_t prefix = 0;
  while (prefix < limit && oldText[prefix] == now[prefix]) {
    ++prefix;
  }
  // Not in the middle of a character.
  while (prefix > 0 && prefix < oldText.size() && (static_cast<unsigned char>(oldText[prefix]) & 0xC0) == 0x80) {
    --prefix;
  }
  size_t suffix = 0;
  while (suffix < limit - prefix && oldText[oldText.size() - 1 - suffix] == now[now.size() - 1 - suffix]) {
    ++suffix;
  }
  while (suffix > 0 && (static_cast<unsigned char>(oldText[oldText.size() - suffix]) & 0xC0) == 0x80) {
    --suffix;
  }
  result.replaceStart = prefix;
  result.replaceEnd = oldText.size() - suffix;
  result.replacement = now.substr(prefix, now.size() - prefix - suffix);
  result.selectionStart = edit.selectionStart;
  result.selectionEnd = edit.selectionEnd;
  return result;
}

} // namespace

MarkdownEditResult markdownEdit(std::string_view text, size_t selectionStart, size_t selectionEnd, MarkdownEditOp op, int a, int b) {
  size_t ss = std::min(selectionStart, text.size());
  size_t se = std::min(selectionEnd, text.size());
  if (ss > se) {
    std::swap(ss, se);
  }
  Edit edit;
  switch (op) {
  case MarkdownEditOp::Bold:
    edit = inlineMark(text, ss, se, '*', 2);
    break;
  case MarkdownEditOp::Italic:
    edit = inlineMark(text, ss, se, '*', 1);
    break;
  case MarkdownEditOp::Strike:
    edit = inlineMark(text, ss, se, '~', 2);
    break;
  case MarkdownEditOp::Code:
    edit = inlineMark(text, ss, se, '`', 1);
    break;
  case MarkdownEditOp::Link:
    edit = linkLike(text, ss, se, false);
    break;
  case MarkdownEditOp::Image:
    edit = linkLike(text, ss, se, true);
    break;
  case MarkdownEditOp::Heading:
    edit = headingEdit(text, ss, se, a);
    break;
  case MarkdownEditOp::Bullets:
    edit = listEdit(text, ss, se, ListKind::Bullets);
    break;
  case MarkdownEditOp::Numbers:
    edit = listEdit(text, ss, se, ListKind::Numbers);
    break;
  case MarkdownEditOp::Tasks:
    edit = listEdit(text, ss, se, ListKind::Tasks);
    break;
  case MarkdownEditOp::Quote:
    edit = quoteEdit(text, ss, se);
    break;
  case MarkdownEditOp::CodeBlock:
    edit = codeBlockEdit(text, ss, se);
    break;
  case MarkdownEditOp::Rule:
    edit = ruleEdit(text, ss);
    break;
  default:
    edit = markdown_edit::tableEdit(text, ss, se, op, a, b);
    break;
  }
  return finish(text, edit);
}

std::string applyMarkdownEdit(std::string_view text, const MarkdownEditResult &result) {
  if (!result.applied) {
    return std::string(text);
  }
  return splice(text, result.replaceStart, result.replaceEnd, result.replacement);
}

} // namespace primec::ui
