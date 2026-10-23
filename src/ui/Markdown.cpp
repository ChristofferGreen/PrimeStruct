#include "primec/ui/Markdown.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace primec::ui {
namespace {

// ---- characters ----------------------------------------------------------------------------------

bool isAsciiPunct(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return u < 128 && ((u >= '!' && u <= '/') || (u >= ':' && u <= '@') || (u >= '[' && u <= '`') ||
                     (u >= '{' && u <= '~'));
}

bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isAlnum(char c) {
  return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string escapeHtml(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    default:
      out += c;
    }
  }
  return out;
}

std::string_view trim(std::string_view text) {
  while (!text.empty() && isSpace(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && isSpace(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

// ---- links ---------------------------------------------------------------------------------------

// Whether a link or image target may be written into a page: no scheme (a relative reference or
// fragment), or a harmless one.
bool safeTarget(std::string_view target, bool image) {
  size_t i = 0;
  while (i < target.size() && (isAlnum(target[i]) || target[i] == '+' || target[i] == '-' || target[i] == '.')) {
    ++i;
  }
  if (i == 0 || i >= target.size() || target[i] != ':') {
    return true; // no scheme
  }
  std::string scheme(target.substr(0, i));
  std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  });
  if (scheme == "http" || scheme == "https") {
    return true;
  }
  if (!image && scheme == "mailto") {
    return true;
  }
  if (image && scheme == "data") {
    return startsWith(target.substr(i + 1), "image/png") || startsWith(target.substr(i + 1), "image/jpeg") ||
           startsWith(target.substr(i + 1), "image/gif") || startsWith(target.substr(i + 1), "image/webp");
  }
  return false;
}

std::string linkTarget(std::string_view target, bool image) {
  return safeTarget(target, image) ? escapeHtml(target) : std::string("#");
}

// ---- inline --------------------------------------------------------------------------------------

struct InlineNode {
  bool isDelimiter = false;
  std::string html;     // text: its HTML
  char mark = 0;        // delimiter: '*', '_' or '~'
  int count = 0;        // delimiter: how many are still unmatched
  int original = 0;     // delimiter: the length of the whole run
  bool canOpen = false;
  bool canClose = false;
};

std::string renderInline(std::string_view text);

// A valid character reference (&amp; &#35; &#x23;) goes through as it is.
size_t entityLength(std::string_view text, size_t start) {
  size_t i = start + 1;
  if (i < text.size() && text[i] == '#') {
    ++i;
    const bool hex = i < text.size() && (text[i] == 'x' || text[i] == 'X');
    if (hex) {
      ++i;
    }
    const size_t digits = i;
    while (i < text.size() && (hex ? (isDigit(text[i]) || (text[i] >= 'a' && text[i] <= 'f') ||
                                      (text[i] >= 'A' && text[i] <= 'F'))
                                   : isDigit(text[i]))) {
      ++i;
    }
    if (i == digits || i - digits > 7) {
      return 0;
    }
  } else {
    const size_t nameStart = i;
    while (i < text.size() && isAlnum(text[i])) {
      ++i;
    }
    if (i == nameStart || i - nameStart > 31) {
      return 0;
    }
  }
  return i < text.size() && text[i] == ';' ? i + 1 - start : 0;
}

// The index of the ']' that closes the '[' at `open`, or npos. Nested brackets, backslash escapes and
// code spans are skipped.
size_t findClosingBracket(std::string_view text, size_t open) {
  int depth = 0;
  for (size_t i = open; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\\') {
      ++i;
    } else if (c == '`') {
      size_t run = 0;
      while (i + run < text.size() && text[i + run] == '`') {
        ++run;
      }
      size_t j = i + run;
      bool closed = false;
      while (j < text.size()) {
        if (text[j] == '`') {
          size_t closing = 0;
          while (j + closing < text.size() && text[j + closing] == '`') {
            ++closing;
          }
          if (closing == run) {
            i = j + closing - 1;
            closed = true;
            break;
          }
          j += closing;
        } else {
          ++j;
        }
      }
      if (!closed) {
        i += run - 1;
      }
    } else if (c == '[') {
      ++depth;
    } else if (c == ']') {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }
  return std::string_view::npos;
}

// `(target "title")` starting at the '(' at `open`: the target, the title and the index after the ')'.
bool parseLinkTail(std::string_view text,
                   size_t open,
                   std::string &target,
                   std::string &title,
                   size_t &after) {
  size_t i = open + 1;
  while (i < text.size() && isSpace(text[i])) {
    ++i;
  }
  target.clear();
  title.clear();
  if (i < text.size() && text[i] == '<') {
    ++i;
    while (i < text.size() && text[i] != '>' && text[i] != '\n') {
      target += text[i++];
    }
    if (i >= text.size() || text[i] != '>') {
      return false;
    }
    ++i;
  } else {
    int depth = 0;
    while (i < text.size() && !isSpace(text[i])) {
      if (text[i] == '\\' && i + 1 < text.size() && isAsciiPunct(text[i + 1])) {
        target += text[i + 1];
        i += 2;
        continue;
      }
      if (text[i] == '(') {
        ++depth;
      } else if (text[i] == ')') {
        if (depth == 0) {
          break;
        }
        --depth;
      }
      target += text[i++];
    }
  }
  while (i < text.size() && isSpace(text[i])) {
    ++i;
  }
  if (i < text.size() && (text[i] == '"' || text[i] == '\'' || text[i] == '(')) {
    const char close = text[i] == '(' ? ')' : text[i];
    ++i;
    while (i < text.size() && text[i] != close) {
      if (text[i] == '\\' && i + 1 < text.size() && isAsciiPunct(text[i + 1])) {
        ++i;
      }
      title += text[i++];
    }
    if (i >= text.size()) {
      return false;
    }
    ++i;
    while (i < text.size() && isSpace(text[i])) {
      ++i;
    }
  }
  if (i >= text.size() || text[i] != ')') {
    return false;
  }
  after = i + 1;
  return true;
}

bool tryLink(std::string_view text, size_t start, bool image, std::string &html, size_t &after) {
  const size_t open = image ? start + 1 : start;
  const size_t close = findClosingBracket(text, open);
  if (close == std::string_view::npos || close + 1 >= text.size() || text[close + 1] != '(') {
    return false;
  }
  std::string target;
  std::string title;
  if (!parseLinkTail(text, close + 1, target, title, after)) {
    return false;
  }
  const std::string_view inner = text.substr(open + 1, close - open - 1);
  const std::string titleAttr = title.empty() ? std::string() : " title=\"" + escapeHtml(title) + "\"";
  if (image) {
    html = "<img src=\"" + linkTarget(target, true) + "\" alt=\"" + escapeHtml(inner) + "\"" + titleAttr + ">";
  } else {
    html = "<a href=\"" + linkTarget(target, false) + "\"" + titleAttr + ">" + renderInline(inner) + "</a>";
  }
  return true;
}

bool tryAutolink(std::string_view text, size_t start, std::string &html, size_t &after) {
  const size_t close = text.find('>', start + 1);
  if (close == std::string_view::npos) {
    return false;
  }
  const std::string_view inner = text.substr(start + 1, close - start - 1);
  if (inner.empty() || inner.find_first_of(" \t\n<") != std::string_view::npos) {
    return false;
  }
  const bool web = startsWith(inner, "http://") || startsWith(inner, "https://");
  const bool mail = startsWith(inner, "mailto:");
  const size_t at = inner.find('@');
  const bool email = !web && !mail && at != std::string_view::npos && at > 0 && at + 1 < inner.size();
  if (!web && !mail && !email) {
    return false;
  }
  const std::string target = email ? "mailto:" + std::string(inner) : std::string(inner);
  html = "<a href=\"" + escapeHtml(target) + "\">" + escapeHtml(inner) + "</a>";
  after = close + 1;
  return true;
}

bool tryBareUrl(std::string_view text, size_t start, std::string &html, size_t &after) {
  if (start > 0 && isAlnum(text[start - 1])) {
    return false;
  }
  const bool web = startsWith(text.substr(start), "http://") || startsWith(text.substr(start), "https://");
  if (!web) {
    return false;
  }
  size_t end = start;
  while (end < text.size() && !isSpace(text[end]) && text[end] != '<') {
    ++end;
  }
  // Trailing punctuation belongs to the sentence, and so does a ')' without its '('.
  while (end > start) {
    const char last = text[end - 1];
    if (last == '.' || last == ',' || last == ';' || last == ':' || last == '!' || last == '?' || last == '"' ||
        last == '\'') {
      --end;
    } else if (last == ')') {
      const std::string_view url = text.substr(start, end - start);
      if (std::count(url.begin(), url.end(), ')') > std::count(url.begin(), url.end(), '(')) {
        --end;
      } else {
        break;
      }
    } else {
      break;
    }
  }
  const std::string_view url = text.substr(start, end - start);
  if (url.size() <= std::string_view("https://").size()) {
    return false;
  }
  html = "<a href=\"" + escapeHtml(url) + "\">" + escapeHtml(url) + "</a>";
  after = end;
  return true;
}

bool tryCodeSpan(std::string_view text, size_t start, std::string &html, size_t &after) {
  size_t run = 0;
  while (start + run < text.size() && text[start + run] == '`') {
    ++run;
  }
  size_t j = start + run;
  while (j < text.size()) {
    if (text[j] != '`') {
      ++j;
      continue;
    }
    size_t closing = 0;
    while (j + closing < text.size() && text[j + closing] == '`') {
      ++closing;
    }
    if (closing == run) {
      std::string code(text.substr(start + run, j - start - run));
      std::replace(code.begin(), code.end(), '\n', ' ');
      const bool padded = code.size() >= 2 && code.front() == ' ' && code.back() == ' ' &&
                          code.find_first_not_of(' ') != std::string::npos;
      if (padded) {
        code = code.substr(1, code.size() - 2);
      }
      html = "<code>" + escapeHtml(code) + "</code>";
      after = j + closing;
      return true;
    }
    j += closing;
  }
  return false;
}

// Emphasis, strong and strikethrough by the delimiter-run method of CommonMark: a run of * _ or ~ may open
// or close depending on what surrounds it, and the innermost matching pairs are wrapped first.
void matchEmphasis(std::vector<InlineNode> &nodes) {
  for (size_t closer = 0; closer < nodes.size(); ++closer) {
    if (!nodes[closer].isDelimiter || !nodes[closer].canClose) {
      continue;
    }
    while (nodes[closer].count > 0 && nodes[closer].canClose) {
      size_t opener = nodes.size();
      for (size_t k = closer; k-- > 0;) {
        const InlineNode &candidate = nodes[k];
        if (!candidate.isDelimiter || candidate.mark != nodes[closer].mark || !candidate.canOpen ||
            candidate.count == 0) {
          continue;
        }
        // The rule of three: a run that can both open and close does not pair with one whose lengths
        // add up to a multiple of three unless both are.
        if (nodes[closer].mark != '~' && (candidate.canClose || nodes[closer].canOpen) &&
            (candidate.original + nodes[closer].original) % 3 == 0 &&
            !(candidate.original % 3 == 0 && nodes[closer].original % 3 == 0)) {
          continue;
        }
        opener = k;
        break;
      }
      if (opener == nodes.size()) {
        break;
      }
      int use = (nodes[opener].count >= 2 && nodes[closer].count >= 2) ? 2 : 1;
      if (nodes[closer].mark == '~') {
        if (nodes[opener].count < 2 || nodes[closer].count < 2) {
          break;
        }
        use = 2;
      }
      const char mark = nodes[closer].mark;
      const std::string open = mark == '~' ? "<del>" : (use == 2 ? "<strong>" : "<em>");
      const std::string close = mark == '~' ? "</del>" : (use == 2 ? "</strong>" : "</em>");
      nodes[opener].count -= use;
      nodes[closer].count -= use;
      InlineNode closeNode;
      closeNode.html = close;
      nodes.insert(nodes.begin() + static_cast<std::ptrdiff_t>(closer), closeNode);
      InlineNode openNode;
      openNode.html = open;
      nodes.insert(nodes.begin() + static_cast<std::ptrdiff_t>(opener) + 1, openNode);
      // Runs between the pair can no longer match anything outside it.
      for (size_t k = opener + 2; k <= closer; ++k) {
        if (nodes[k].isDelimiter) {
          nodes[k].canOpen = false;
          nodes[k].canClose = false;
        }
      }
      closer += 2;
    }
  }
}

std::string renderInline(std::string_view text) {
  std::vector<InlineNode> nodes;
  std::string pending;
  const auto flush = [&] {
    if (!pending.empty()) {
      InlineNode node;
      node.html = pending;
      nodes.push_back(node);
      pending.clear();
    }
  };
  const size_t n = text.size();
  size_t i = 0;
  while (i < n) {
    const char c = text[i];
    if (c == '\\') {
      if (i + 1 < n && text[i + 1] == '\n') {
        pending += "<br>\n";
        i += 2;
        while (i < n && text[i] == ' ') {
          ++i;
        }
        continue;
      }
      if (i + 1 < n && isAsciiPunct(text[i + 1])) {
        pending += escapeHtml(std::string_view(&text[i + 1], 1));
        i += 2;
        continue;
      }
      pending += '\\';
      ++i;
      continue;
    }
    if (c == '\n') {
      size_t spaces = 0;
      while (!pending.empty() && pending.back() == ' ') {
        pending.pop_back();
        ++spaces;
      }
      pending += spaces >= 2 ? "<br>\n" : "\n";
      ++i;
      while (i < n && text[i] == ' ') {
        ++i;
      }
      continue;
    }
    std::string html;
    size_t after = 0;
    if (c == '`') {
      if (tryCodeSpan(text, i, html, after)) {
        pending += html;
        i = after;
        continue;
      }
      while (i < n && text[i] == '`') {
        pending += '`';
        ++i;
      }
      continue;
    }
    if (c == '!' && i + 1 < n && text[i + 1] == '[' && tryLink(text, i, true, html, after)) {
      pending += html;
      i = after;
      continue;
    }
    if (c == '[' && tryLink(text, i, false, html, after)) {
      pending += html;
      i = after;
      continue;
    }
    if (c == '<') {
      if (tryAutolink(text, i, html, after)) {
        pending += html;
        i = after;
      } else {
        pending += "&lt;";
        ++i;
      }
      continue;
    }
    if (c == 'h' && tryBareUrl(text, i, html, after)) {
      pending += html;
      i = after;
      continue;
    }
    if (c == '&') {
      const size_t entity = entityLength(text, i);
      if (entity != 0) {
        pending += std::string(text.substr(i, entity));
        i += entity;
      } else {
        pending += "&amp;";
        ++i;
      }
      continue;
    }
    if (c == '*' || c == '_' || c == '~') {
      size_t run = 0;
      while (i + run < n && text[i + run] == c) {
        ++run;
      }
      const char before = i > 0 ? text[i - 1] : ' ';
      const char next = i + run < n ? text[i + run] : ' ';
      const bool beforeSpace = isSpace(before);
      const bool nextSpace = isSpace(next);
      const bool beforePunct = isAsciiPunct(before);
      const bool nextPunct = isAsciiPunct(next);
      const bool leftFlanking = !nextSpace && (!nextPunct || beforeSpace || beforePunct);
      const bool rightFlanking = !beforeSpace && (!beforePunct || nextSpace || nextPunct);
      InlineNode delimiter;
      delimiter.isDelimiter = true;
      delimiter.mark = c;
      delimiter.count = static_cast<int>(run);
      delimiter.original = static_cast<int>(run);
      if (c == '_') {
        delimiter.canOpen = leftFlanking && (!rightFlanking || beforePunct);
        delimiter.canClose = rightFlanking && (!leftFlanking || nextPunct);
      } else {
        delimiter.canOpen = leftFlanking;
        delimiter.canClose = rightFlanking;
      }
      flush();
      nodes.push_back(delimiter);
      i += run;
      continue;
    }
    if (c == '>') {
      pending += "&gt;";
    } else if (c == '"') {
      pending += "&quot;";
    } else {
      pending += c;
    }
    ++i;
  }
  // A trailing hard-break marker of the last line has nothing to break.
  while (!pending.empty() && pending.back() == ' ') {
    pending.pop_back();
  }
  flush();
  matchEmphasis(nodes);
  std::string out;
  for (const InlineNode &node : nodes) {
    if (node.isDelimiter) {
      out.append(static_cast<size_t>(node.count), node.mark);
    } else {
      out += node.html;
    }
  }
  return out;
}

// ---- blocks --------------------------------------------------------------------------------------

using Lines = std::vector<std::string>;

size_t leadingSpaces(const std::string &line) {
  size_t i = 0;
  while (i < line.size() && line[i] == ' ') {
    ++i;
  }
  return i;
}

bool isBlank(const std::string &line) {
  return line.find_first_not_of(" \t") == std::string::npos;
}

// An ATX heading: its level (1 to 6) and text, or level 0.
int parseAtxHeading(const std::string &line, std::string &text) {
  const size_t indent = leadingSpaces(line);
  if (indent > 3) {
    return 0;
  }
  size_t i = indent;
  size_t level = 0;
  while (i < line.size() && line[i] == '#') {
    ++level;
    ++i;
  }
  if (level == 0 || level > 6 || (i < line.size() && line[i] != ' ' && line[i] != '\t')) {
    return 0;
  }
  std::string_view rest = trim(std::string_view(line).substr(i));
  // A closing run of #s is not part of the text when a space precedes it.
  size_t end = rest.size();
  while (end > 0 && rest[end - 1] == '#') {
    --end;
  }
  if (end < rest.size() && (end == 0 || rest[end - 1] == ' ' || rest[end - 1] == '\t')) {
    rest = trim(rest.substr(0, end));
  }
  text = std::string(rest);
  return static_cast<int>(level);
}

bool isThematicBreak(const std::string &line) {
  const size_t indent = leadingSpaces(line);
  if (indent > 3 || indent >= line.size()) {
    return false;
  }
  const char mark = line[indent];
  if (mark != '-' && mark != '*' && mark != '_') {
    return false;
  }
  size_t count = 0;
  for (size_t i = indent; i < line.size(); ++i) {
    if (line[i] == mark) {
      ++count;
    } else if (line[i] != ' ' && line[i] != '\t') {
      return false;
    }
  }
  return count >= 3;
}

struct Fence {
  bool ok = false;
  char mark = 0;
  size_t length = 0;
  size_t indent = 0;
  std::string info;
};

Fence parseFence(const std::string &line) {
  Fence fence;
  const size_t indent = leadingSpaces(line);
  if (indent > 3 || indent >= line.size() || (line[indent] != '`' && line[indent] != '~')) {
    return fence;
  }
  const char mark = line[indent];
  size_t i = indent;
  while (i < line.size() && line[i] == mark) {
    ++i;
  }
  if (i - indent < 3) {
    return fence;
  }
  const std::string info(trim(std::string_view(line).substr(i)));
  if (mark == '`' && info.find('`') != std::string::npos) {
    return fence;
  }
  fence.ok = true;
  fence.mark = mark;
  fence.length = i - indent;
  fence.indent = indent;
  fence.info = info;
  return fence;
}

bool isBlockQuote(const std::string &line) {
  const size_t indent = leadingSpaces(line);
  return indent <= 3 && indent < line.size() && line[indent] == '>';
}

struct ListMarker {
  bool ok = false;
  bool ordered = false;
  char delimiter = 0;
  int number = 0;
  size_t indent = 0;
  size_t contentIndent = 0;
  bool empty = false;
};

ListMarker parseListMarker(const std::string &line) {
  ListMarker marker;
  size_t i = leadingSpaces(line);
  if (i > 3 || i >= line.size()) {
    return marker;
  }
  marker.indent = i;
  if (line[i] == '-' || line[i] == '+' || line[i] == '*') {
    marker.delimiter = line[i];
    ++i;
  } else {
    const size_t start = i;
    while (i < line.size() && isDigit(line[i])) {
      ++i;
    }
    if (i == start || i - start > 9 || i >= line.size() || (line[i] != '.' && line[i] != ')')) {
      return ListMarker{};
    }
    marker.ordered = true;
    marker.number = std::stoi(line.substr(start, i - start));
    marker.delimiter = line[i];
    ++i;
  }
  if (i < line.size() && line[i] != ' ') {
    return ListMarker{};
  }
  size_t j = i;
  while (j < line.size() && line[j] == ' ') {
    ++j;
  }
  if (j >= line.size()) {
    marker.empty = true;
    marker.contentIndent = i + 1;
  } else {
    const size_t spaces = j - i;
    marker.contentIndent = i + (spaces > 4 ? 1 : spaces);
  }
  marker.ok = true;
  return marker;
}

// Whether a line starts a block that can interrupt a paragraph.
bool startsBlock(const std::string &line) {
  std::string ignored;
  if (parseAtxHeading(line, ignored) != 0 || parseFence(line).ok || isThematicBreak(line) || isBlockQuote(line)) {
    return true;
  }
  const ListMarker marker = parseListMarker(line);
  return marker.ok && !marker.empty && (!marker.ordered || marker.number == 1);
}

// A setext underline: 1 for ===, 2 for ---, else 0.
int setextLevel(const std::string &line) {
  const size_t indent = leadingSpaces(line);
  if (indent > 3 || indent >= line.size()) {
    return 0;
  }
  const char mark = line[indent];
  if (mark != '=' && mark != '-') {
    return 0;
  }
  size_t i = indent;
  while (i < line.size() && line[i] == mark) {
    ++i;
  }
  return isBlank(line.substr(i)) ? (mark == '=' ? 1 : 2) : 0;
}

std::vector<std::string> splitTableRow(const std::string &line) {
  std::string_view row = trim(line);
  if (!row.empty() && row.front() == '|') {
    row.remove_prefix(1);
  }
  if (!row.empty() && row.back() == '|' && (row.size() < 2 || row[row.size() - 2] != '\\')) {
    row.remove_suffix(1);
  }
  std::vector<std::string> cells;
  std::string cell;
  for (size_t i = 0; i < row.size(); ++i) {
    if (row[i] == '\\' && i + 1 < row.size() && row[i + 1] == '|') {
      cell += '|';
      ++i;
    } else if (row[i] == '|') {
      cells.emplace_back(trim(cell));
      cell.clear();
    } else {
      cell += row[i];
    }
  }
  cells.emplace_back(trim(cell));
  return cells;
}

// A table delimiter row (`| --- | :-: |`): the alignment of each column, or empty.
std::vector<char> parseTableDelimiter(const std::string &line) {
  if (line.find('-') == std::string::npos) {
    return {};
  }
  std::vector<char> alignments;
  for (const std::string &cell : splitTableRow(line)) {
    if (cell.empty()) {
      return {};
    }
    size_t i = 0;
    const bool left = cell[i] == ':';
    if (left) {
      ++i;
    }
    const size_t dashes = i;
    while (i < cell.size() && cell[i] == '-') {
      ++i;
    }
    if (i == dashes) {
      return {};
    }
    const bool right = i < cell.size() && cell[i] == ':';
    if (right) {
      ++i;
    }
    if (i != cell.size()) {
      return {};
    }
    alignments.push_back(left && right ? 'c' : (right ? 'r' : (left ? 'l' : 0)));
  }
  return alignments;
}

void renderBlocks(const Lines &lines, bool tight, std::string &out);

// The list that starts at lines[i]; returns the index after it. Each item's lines, with the marker's
// indentation removed, are blocks of their own; the list is loose when blank lines separate its items or
// the blocks inside an item.
size_t renderList(const Lines &lines, size_t i, std::string &out) {
  const ListMarker first = parseListMarker(lines[i]);
  std::vector<Lines> items;
  bool loose = false;
  bool listEnded = false;
  size_t cur = i;
  const size_t n = lines.size();
  while (cur < n && !listEnded) {
    const ListMarker marker = parseListMarker(lines[cur]);
    if (!marker.ok || marker.ordered != first.ordered || marker.delimiter != first.delimiter) {
      break;
    }
    items.emplace_back();
    Lines &item = items.back();
    item.push_back(marker.empty ? std::string() : lines[cur].substr(std::min(marker.contentIndent, lines[cur].size())));
    ++cur;
    bool itemEnded = false;
    while (cur < n && !itemEnded) {
      const std::string &line = lines[cur];
      if (isBlank(line)) {
        size_t next = cur + 1;
        while (next < n && isBlank(lines[next])) {
          ++next;
        }
        if (next >= n) {
          cur = next;
          itemEnded = true;
          listEnded = true;
        } else if (leadingSpaces(lines[next]) >= marker.contentIndent) {
          item.insert(item.end(), next - cur, std::string());
          cur = next;
          loose = true;
        } else {
          const ListMarker sibling = parseListMarker(lines[next]);
          const bool continues = sibling.ok && sibling.ordered == first.ordered && sibling.delimiter == first.delimiter;
          loose = loose || continues;
          listEnded = !continues;
          cur = next;
          itemEnded = true;
        }
      } else if (leadingSpaces(line) >= marker.contentIndent) {
        item.push_back(line.substr(marker.contentIndent));
        ++cur;
      } else if (parseListMarker(line).ok) {
        itemEnded = true;
      } else if (!item.empty() && !item.back().empty() && !startsBlock(line)) {
        item.push_back(std::string(trim(line))); // a lazy continuation of the item's paragraph
        ++cur;
      } else {
        itemEnded = true;
        listEnded = true;
      }
    }
  }
  out += first.ordered ? (first.number == 1 ? "<ol>\n" : "<ol start=\"" + std::to_string(first.number) + "\">\n")
                       : "<ul>\n";
  for (Lines &item : items) {
    std::string task;
    if (!item.empty()) {
      const std::string &head = item.front();
      const bool unchecked = head == "[ ]" || startsWith(head, "[ ] ");
      const bool checked = head == "[x]" || head == "[X]" || startsWith(head, "[x] ") || startsWith(head, "[X] ");
      if (unchecked || checked) {
        task = std::string("<input type=\"checkbox\" disabled") + (checked ? " checked" : "") + "> ";
        item.front() = head.size() > 4 ? head.substr(4) : std::string();
      }
    }
    std::string body;
    renderBlocks(item, !loose, body);
    while (!body.empty() && body.back() == '\n') {
      body.pop_back();
    }
    out += "<li>" + task + body + "</li>\n";
  }
  out += first.ordered ? "</ol>\n" : "</ul>\n";
  return cur;
}

void renderBlocks(const Lines &lines, bool tight, std::string &out) {
  const size_t n = lines.size();
  size_t i = 0;
  while (i < n) {
    const std::string &line = lines[i];
    if (isBlank(line)) {
      ++i;
      continue;
    }
    // Fenced code.
    const Fence fence = parseFence(line);
    if (fence.ok) {
      std::string code;
      ++i;
      while (i < n) {
        const Fence closing = parseFence(lines[i]);
        if (closing.ok && closing.mark == fence.mark && closing.length >= fence.length && closing.info.empty()) {
          ++i;
          break;
        }
        const std::string &codeLine = lines[i];
        code += codeLine.substr(std::min(fence.indent, leadingSpaces(codeLine))) + "\n";
        ++i;
      }
      std::string language = fence.info.substr(0, fence.info.find_first_of(" \t"));
      out += "<pre><code";
      if (!language.empty()) {
        out += " class=\"language-" + escapeHtml(language) + "\"";
      }
      out += ">" + escapeHtml(code) + "</code></pre>\n";
      continue;
    }
    // Headings and breaks.
    std::string headingText;
    if (const int level = parseAtxHeading(line, headingText); level != 0) {
      out += "<h" + std::to_string(level) + ">" + renderInline(headingText) + "</h" + std::to_string(level) + ">\n";
      ++i;
      continue;
    }
    if (isThematicBreak(line)) {
      out += "<hr>\n";
      ++i;
      continue;
    }
    // Block quotes: the quoted lines, with the marker removed, are blocks of their own.
    if (isBlockQuote(line)) {
      Lines quoted;
      while (i < n) {
        if (isBlockQuote(lines[i])) {
          std::string stripped = lines[i].substr(leadingSpaces(lines[i]) + 1);
          if (!stripped.empty() && stripped.front() == ' ') {
            stripped.erase(0, 1);
          }
          quoted.push_back(stripped);
          ++i;
        } else if (!isBlank(lines[i]) && !quoted.empty() && !quoted.back().empty() && !startsBlock(lines[i])) {
          quoted.push_back(std::string(trim(lines[i]))); // a lazy continuation of the paragraph
          ++i;
        } else {
          break;
        }
      }
      std::string inner;
      renderBlocks(quoted, false, inner);
      out += "<blockquote>\n" + inner + "</blockquote>\n";
      continue;
    }
    // Lists.
    if (parseListMarker(line).ok) {
      i = renderList(lines, i, out);
      continue;
    }
    // Indented code.
    if (leadingSpaces(line) >= 4) {
      std::string code;
      size_t pending = 0;
      while (i < n && (isBlank(lines[i]) || leadingSpaces(lines[i]) >= 4)) {
        if (isBlank(lines[i])) {
          ++pending;
        } else {
          code.append(pending, '\n');
          pending = 0;
          code += lines[i].substr(4) + "\n";
        }
        ++i;
      }
      out += "<pre><code>" + escapeHtml(code) + "</code></pre>\n";
      continue;
    }
    // A table: a row with pipes followed by the delimiter row.
    if (i + 1 < n && line.find('|') != std::string::npos) {
      const std::vector<char> alignments = parseTableDelimiter(lines[i + 1]);
      const std::vector<std::string> header = splitTableRow(line);
      if (!alignments.empty() && alignments.size() == header.size()) {
        const auto cellTag = [&](const char *tag, size_t column, const std::string &text) {
          std::string style;
          if (column < alignments.size() && alignments[column] != 0) {
            const char kind = alignments[column];
            style = std::string(" style=\"text-align:") + (kind == 'c' ? "center" : (kind == 'r' ? "right" : "left")) +
                    "\"";
          }
          return std::string("<") + tag + style + ">" + renderInline(text) + "</" + tag + ">";
        };
        out += "<table>\n<thead>\n<tr>";
        for (size_t c = 0; c < header.size(); ++c) {
          out += cellTag("th", c, header[c]);
        }
        out += "</tr>\n</thead>\n<tbody>\n";
        i += 2;
        while (i < n && !isBlank(lines[i]) && !startsBlock(lines[i])) {
          const std::vector<std::string> cells = splitTableRow(lines[i]);
          out += "<tr>";
          for (size_t c = 0; c < header.size(); ++c) {
            out += cellTag("td", c, c < cells.size() ? cells[c] : std::string());
          }
          out += "</tr>\n";
          ++i;
        }
        out += "</tbody>\n</table>\n";
        continue;
      }
    }
    // A paragraph, or a setext heading when an underline follows it.
    std::string text;
    int setext = 0;
    while (i < n && !isBlank(lines[i])) {
      if (!text.empty()) {
        if (const int level = setextLevel(lines[i]); level != 0) {
          setext = level;
          ++i;
          break;
        }
        if (startsBlock(lines[i])) {
          break;
        }
        text += "\n";
      }
      text += std::string(trim(lines[i])) + (lines[i].size() >= 2 && lines[i].compare(lines[i].size() - 2, 2, "  ") == 0
                                                  ? "  "
                                                  : "");
      ++i;
    }
    if (setext != 0) {
      out += "<h" + std::to_string(setext) + ">" + renderInline(text) + "</h" + std::to_string(setext) + ">\n";
    } else if (tight) {
      out += renderInline(text) + "\n";
    } else {
      out += "<p>" + renderInline(text) + "</p>\n";
    }
  }
}

Lines splitLines(std::string_view markdown) {
  Lines lines;
  std::string line;
  for (size_t i = 0; i < markdown.size(); ++i) {
    const char c = markdown[i];
    if (c == '\r') {
      if (i + 1 < markdown.size() && markdown[i + 1] == '\n') {
        ++i;
      }
      lines.push_back(line);
      line.clear();
    } else if (c == '\n') {
      lines.push_back(line);
      line.clear();
    } else {
      line += c;
    }
  }
  lines.push_back(line);
  // Tabs that indent a line stand for four spaces.
  for (std::string &text : lines) {
    size_t tabs = 0;
    while (tabs < text.size() && (text[tabs] == '\t' || text[tabs] == ' ')) {
      ++tabs;
    }
    std::string expanded;
    for (size_t k = 0; k < tabs; ++k) {
      expanded += text[k] == '\t' ? std::string("    ") : std::string(1, text[k]);
    }
    text = expanded + text.substr(tabs);
  }
  return lines;
}

} // namespace

std::string markdownToHtml(std::string_view markdown) {
  std::string out;
  renderBlocks(splitLines(markdown), false, out);
  return out;
}

std::string markdownPreviewPage(std::string_view markdown) {
  static constexpr const char *Style =
      "<style>\n"
      ":root { color-scheme: light dark; }\n"
      "body { font: 15px/1.55 -apple-system, BlinkMacSystemFont, \"Helvetica Neue\", sans-serif; margin: 0;\n"
      "       padding: 16px 22px 40px; max-width: 46em; }\n"
      "h1, h2 { border-bottom: 1px solid rgba(128,128,128,0.35); padding-bottom: 0.25em; }\n"
      "h1, h2, h3, h4, h5, h6 { line-height: 1.25; margin: 1.4em 0 0.6em; }\n"
      "code, pre { font: 13px/1.45 ui-monospace, Menlo, monospace; }\n"
      "code { background: rgba(128,128,128,0.16); padding: 0.1em 0.35em; border-radius: 4px; }\n"
      "pre { background: rgba(128,128,128,0.14); padding: 10px 12px; border-radius: 6px; overflow-x: auto; }\n"
      "pre code { background: none; padding: 0; }\n"
      "blockquote { margin: 0.8em 0; padding: 0 1em; border-left: 4px solid rgba(128,128,128,0.45); opacity: 0.85; }\n"
      "table { border-collapse: collapse; margin: 0.8em 0; }\n"
      "th, td { border: 1px solid rgba(128,128,128,0.45); padding: 5px 11px; }\n"
      "th { background: rgba(128,128,128,0.14); }\n"
      "hr { border: 0; border-top: 1px solid rgba(128,128,128,0.4); margin: 1.4em 0; }\n"
      "img { max-width: 100%; }\n"
      "a { color: #0a64d6; }\n"
      "@media (prefers-color-scheme: dark) { a { color: #62a8ff; } }\n"
      "</style>\n";
  return std::string("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n"
                     "<meta name=\"viewport\" content=\"width=device-width\">\n") +
         Style + "</head>\n<body>\n" + markdownToHtml(markdown) + "</body>\n</html>\n";
}

} // namespace primec::ui
