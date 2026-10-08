#include "primec/ui/SyntaxHighlight.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <string>

namespace primec::ui {
namespace {

namespace colors = highlight_colors;

using Words = std::set<std::string_view>;

struct Emitter {
  std::vector<HighlightRun> runs;
  void add(size_t start, size_t end, int32_t rgb, int32_t flags = 0) {
    if (end > start) {
      runs.push_back(HighlightRun{static_cast<int32_t>(start), static_cast<int32_t>(end), rgb, flags});
    }
  }
};

bool isIdentStart(unsigned char c) { return std::isalpha(c) != 0 || c == '_' || c >= 0x80; }
bool isIdentPart(unsigned char c) { return std::isalnum(c) != 0 || c == '_' || c >= 0x80; }

struct CodeSyntax {
  std::string_view lineComment;
  bool blockComments = false;
  bool tripleQuotes = false;
  bool singleQuotes = false;
  bool preprocessor = false;
  bool upperIsType = false;
  const Words *keywords = nullptr;
  const Words *types = nullptr;
};

// Scans C-like or Python-like source. Strings and comments are one run each (to
// the closing delimiter, or the end of the line / text when unterminated).
void lexCode(std::string_view text, const CodeSyntax &syntax, Emitter &out) {
  const size_t size = text.size();
  size_t i = 0;
  bool lineStart = true;
  while (i < size) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '\n') {
      lineStart = true;
      ++i;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      ++i;
      continue;
    }
    const bool atLineStart = lineStart;
    lineStart = false;

    if (syntax.preprocessor && atLineStart && c == '#') {
      size_t end = text.find('\n', i);
      end = end == std::string_view::npos ? size : end;
      out.add(i, end, colors::Keyword);
      i = end;
      continue;
    }
    if (!syntax.lineComment.empty() && text.compare(i, syntax.lineComment.size(), syntax.lineComment) == 0) {
      size_t end = text.find('\n', i);
      end = end == std::string_view::npos ? size : end;
      out.add(i, end, colors::Comment, PS_UI_STYLE_ITALIC);
      i = end;
      continue;
    }
    // "/*" glued to a path segment is a wildcard path such as `import /std/math/*`, not a comment.
    if (syntax.blockComments && c == '/' && i + 1 < size && text[i + 1] == '*' &&
        (i == 0 || !isIdentPart(static_cast<unsigned char>(text[i - 1])))) {
      size_t end = text.find("*/", i + 2);
      end = end == std::string_view::npos ? size : end + 2;
      out.add(i, end, colors::Comment, PS_UI_STYLE_ITALIC);
      i = end;
      continue;
    }
    if (c == '"' || (c == '\'' && syntax.singleQuotes)) {
      const char quote = static_cast<char>(c);
      if (syntax.tripleQuotes && text.compare(i, 3, std::string(3, quote)) == 0) {
        size_t end = text.find(std::string(3, quote), i + 3);
        end = end == std::string_view::npos ? size : end + 3;
        out.add(i, end, colors::String);
        i = end;
        continue;
      }
      size_t j = i + 1;
      while (j < size && text[j] != quote && text[j] != '\n') {
        j += text[j] == '\\' && j + 1 < size && text[j + 1] != '\n' ? 2 : 1;
      }
      if (j < size && text[j] == quote) {
        ++j;
      }
      out.add(i, std::min(j, size), colors::String);
      i = std::min(j, size);
      continue;
    }
    if (std::isdigit(c) != 0 || (c == '.' && i + 1 < size && std::isdigit(static_cast<unsigned char>(text[i + 1])) != 0)) {
      size_t j = i + 1;
      while (j < size && (std::isalnum(static_cast<unsigned char>(text[j])) != 0 || text[j] == '_' || text[j] == '.' ||
                          text[j] == '\'')) {
        ++j;
      }
      out.add(i, j, colors::Number);
      i = j;
      continue;
    }
    if (isIdentStart(c)) {
      size_t j = i + 1;
      while (j < size && isIdentPart(static_cast<unsigned char>(text[j]))) {
        ++j;
      }
      const std::string_view word = text.substr(i, j - i);
      if (syntax.keywords != nullptr && syntax.keywords->count(word) != 0) {
        out.add(i, j, colors::Keyword);
      } else if ((syntax.types != nullptr && syntax.types->count(word) != 0) ||
                 (syntax.upperIsType && std::isupper(c) != 0)) {
        out.add(i, j, colors::Type);
      }
      i = j;
      continue;
    }
    ++i;
  }
}

const Words PrimeStructKeywords{"return", "if",   "else",   "while", "for",  "loop",  "import", "namespace", "pick",
                                "true",   "false", "mut",   "struct", "sum", "enum",  "this",   "include"};
const Words PrimeStructTypes{"i8",  "i16",  "i32",  "i64",   "u8",     "u16",    "u32",  "u64",
                             "f32", "f64",  "int",  "float", "bool",   "string", "void", "array",
                             "vector", "map", "handle", "Result", "Maybe"};

const Words CppKeywords{"alignas", "alignof", "auto",     "break",     "case",      "catch",    "class",    "const",
                        "constexpr", "continue", "default", "delete",  "do",        "else",     "enum",     "explicit",
                        "export",  "extern",  "false",    "final",     "for",       "friend",   "goto",     "if",
                        "inline",  "mutable", "namespace", "new",      "noexcept",  "nullptr",  "operator", "override",
                        "private", "protected", "public", "return",    "sizeof",    "static",   "struct",   "switch",
                        "template", "this",   "throw",    "true",      "try",       "typedef",  "typename", "union",
                        "using",   "virtual", "volatile", "while",     "co_await",  "co_return", "concept", "requires"};
const Words CppTypes{"bool",     "char",   "char8_t",  "char16_t", "char32_t", "double",   "float",    "int",
                     "long",     "short",  "signed",   "unsigned", "void",    "wchar_t",  "size_t",   "int8_t",
                     "int16_t",  "int32_t", "int64_t", "uint8_t",  "uint16_t", "uint32_t", "uint64_t", "string"};

const Words PythonKeywords{"False",   "None",   "True",    "and",      "as",     "assert", "async", "await",
                           "break",   "class",  "continue", "def",     "del",    "elif",   "else",  "except",
                           "finally", "for",    "from",    "global",   "if",     "import", "in",    "is",
                           "lambda",  "nonlocal", "not",   "or",       "pass",   "raise",  "return", "try",
                           "while",   "with",   "yield",   "match",    "case"};
const Words PythonTypes{"int", "str", "float", "bool", "list", "dict", "set", "tuple", "bytes", "object", "self"};

void lexJson(std::string_view text, Emitter &out) {
  const size_t size = text.size();
  size_t i = 0;
  while (i < size) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '"') {
      size_t j = i + 1;
      while (j < size && text[j] != '"' && text[j] != '\n') {
        j += text[j] == '\\' && j + 1 < size && text[j + 1] != '\n' ? 2 : 1;
      }
      if (j < size && text[j] == '"') {
        ++j;
      }
      j = std::min(j, size);
      size_t k = j;
      while (k < size && (text[k] == ' ' || text[k] == '\t' || text[k] == '\r' || text[k] == '\n')) {
        ++k;
      }
      const bool isKey = k < size && text[k] == ':';
      out.add(i, j, isKey ? colors::Type : colors::String);
      i = j;
    } else if (std::isdigit(c) != 0 || c == '-') {
      size_t j = i + 1;
      while (j < size && (std::isdigit(static_cast<unsigned char>(text[j])) != 0 || text[j] == '.' || text[j] == 'e' ||
                          text[j] == 'E' || text[j] == '+' || text[j] == '-')) {
        ++j;
      }
      if (j > i + 1 || std::isdigit(c) != 0) {
        out.add(i, j, colors::Number);
      }
      i = j;
    } else if (std::isalpha(c) != 0) {
      size_t j = i + 1;
      while (j < size && std::isalpha(static_cast<unsigned char>(text[j])) != 0) {
        ++j;
      }
      const std::string_view word = text.substr(i, j - i);
      if (word == "true" || word == "false" || word == "null") {
        out.add(i, j, colors::Keyword);
      }
      i = j;
    } else {
      ++i;
    }
  }
}

// Inline spans of one Markdown line [start, end): `code`, **strong**, *emphasis*, _emphasis_, [text](url).
void lexMarkdownInline(std::string_view text, size_t start, size_t end, Emitter &out) {
  size_t i = start;
  while (i < end) {
    const char c = text[i];
    if (c == '`') {
      const size_t close = text.find('`', i + 1);
      if (close != std::string_view::npos && close < end) {
        out.add(i, close + 1, colors::Code);
        i = close + 1;
        continue;
      }
    } else if (c == '*' && i + 1 < end && text[i + 1] == '*') {
      const size_t close = text.find("**", i + 2);
      if (close != std::string_view::npos && close < end && close > i + 2) {
        out.add(i, close + 2, colors::Strong, PS_UI_STYLE_BOLD);
        i = close + 2;
        continue;
      }
    } else if ((c == '*' || c == '_') && i + 1 < end && text[i + 1] != ' ' && text[i + 1] != c) {
      const size_t close = text.find(c, i + 1);
      if (close != std::string_view::npos && close < end && close > i + 1 && text[close - 1] != ' ') {
        out.add(i, close + 1, colors::Marker, PS_UI_STYLE_ITALIC);
        i = close + 1;
        continue;
      }
    } else if (c == '[') {
      const size_t mid = text.find("](", i + 1);
      if (mid != std::string_view::npos && mid < end) {
        const size_t close = text.find(')', mid + 2);
        if (close != std::string_view::npos && close < end) {
          out.add(i, close + 1, colors::Link);
          i = close + 1;
          continue;
        }
      }
    }
    ++i;
  }
}

void lexMarkdown(std::string_view text, Emitter &out) {
  const size_t size = text.size();
  size_t lineStart = 0;
  bool inFence = false;
  while (lineStart < size) {
    size_t lineEnd = text.find('\n', lineStart);
    lineEnd = lineEnd == std::string_view::npos ? size : lineEnd;
    const std::string_view line = text.substr(lineStart, lineEnd - lineStart);
    size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ') {
      ++indent;
    }
    const std::string_view body = line.substr(indent);
    const bool fence = body.substr(0, 3) == "```" || body.substr(0, 3) == "~~~";
    if (fence) {
      out.add(lineStart, lineEnd, colors::Code);
      inFence = !inFence;
    } else if (inFence) {
      out.add(lineStart, lineEnd, colors::Code);
    } else if (!body.empty() && body[0] == '#') {
      size_t hashes = 0;
      while (hashes < body.size() && body[hashes] == '#') {
        ++hashes;
      }
      if (hashes <= 6 && (hashes == body.size() || body[hashes] == ' ')) {
        out.add(lineStart, lineEnd, colors::Heading, PS_UI_STYLE_BOLD);
      }
    } else if (!body.empty() && body[0] == '>') {
      out.add(lineStart, lineEnd, colors::Comment, PS_UI_STYLE_ITALIC);
    } else {
      size_t contentStart = lineStart + indent;
      // List markers: "- ", "* ", "+ " or "1. ".
      size_t marker = 0;
      if (body.size() >= 2 && (body[0] == '-' || body[0] == '*' || body[0] == '+') && body[1] == ' ') {
        marker = 1;
      } else {
        size_t digits = 0;
        while (digits < body.size() && std::isdigit(static_cast<unsigned char>(body[digits])) != 0) {
          ++digits;
        }
        if (digits > 0 && digits + 1 < body.size() && body[digits] == '.' && body[digits + 1] == ' ') {
          marker = digits + 1;
        }
      }
      if (marker > 0) {
        out.add(contentStart, contentStart + marker, colors::Marker, PS_UI_STYLE_BOLD);
        contentStart += marker;
      }
      lexMarkdownInline(text, contentStart, lineEnd, out);
    }
    lineStart = lineEnd + 1;
  }
}

} // namespace

Language languageForPath(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot == std::string_view::npos || dot + 1 >= name.size()) {
    return Language::None;
  }
  std::string extension(name.substr(dot + 1));
  for (char &c : extension) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static const std::pair<const char *, Language> table[] = {
      {"prime", Language::PrimeStruct}, {"ps", Language::PrimeStruct}, {"c", Language::Cpp},
      {"h", Language::Cpp},             {"cc", Language::Cpp},         {"cpp", Language::Cpp},
      {"cxx", Language::Cpp},           {"hpp", Language::Cpp},        {"hh", Language::Cpp},
      {"hxx", Language::Cpp},           {"m", Language::Cpp},          {"mm", Language::Cpp},
      {"py", Language::Python},         {"pyw", Language::Python},     {"json", Language::Json},
      {"md", Language::Markdown},       {"markdown", Language::Markdown}};
  for (const auto &[ext, language] : table) {
    if (extension == ext) {
      return language;
    }
  }
  return Language::None;
}

std::vector<HighlightRun> highlight(Language language, std::string_view text) {
  Emitter out;
  switch (language) {
  case Language::PrimeStruct: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.upperIsType = true;
    syntax.keywords = &PrimeStructKeywords;
    syntax.types = &PrimeStructTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Cpp: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.preprocessor = true;
    syntax.keywords = &CppKeywords;
    syntax.types = &CppTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Python: {
    CodeSyntax syntax;
    syntax.lineComment = "#";
    syntax.tripleQuotes = true;
    syntax.singleQuotes = true;
    syntax.keywords = &PythonKeywords;
    syntax.types = &PythonTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Json:
    lexJson(text, out);
    break;
  case Language::Markdown:
    lexMarkdown(text, out);
    break;
  case Language::None:
    break;
  }
  return std::move(out.runs);
}

bool applyHighlight(uint64_t view, const char *path) {
  // The text comes back through the ABI so the same code serves every backend.
  const std::string text = ps_ui_text_view_get_text(view);
  if (!ps_ui_text_view_clear_styles(view)) {
    return false;
  }
  const Language language = languageForPath(path != nullptr ? path : "");
  if (language == Language::None || text.size() > MaxHighlightBytes) {
    return true;
  }
  for (const HighlightRun &run : highlight(language, text)) {
    ps_ui_text_view_add_style(view, run.start, run.end, run.rgb, run.flags);
  }
  return true;
}

} // namespace primec::ui
