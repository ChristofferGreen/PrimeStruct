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
  bool backtickStrings = false;   // JavaScript templates, Go raw strings: may span lines
  bool annotations = false;       // @name
  bool rustMarkup = false;        // #[attribute], 'lifetime, name! macros
  bool dollarVariables = false;   // $name and ${name}
  bool ignoreCase = false;        // keywords and types match in any case (SQL, CMake)
  const Words *keywords = nullptr;
  const Words *types = nullptr;
};

std::string lowered(std::string_view word) {
  std::string out(word);
  for (char &c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

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
    if (syntax.rustMarkup && c == '#' && i + 1 < size && (text[i + 1] == '[' || text[i + 1] == '!')) {
      // #[derive(Debug)] and #![allow(...)]: up to the matching bracket on the line.
      const size_t close = text.find(']', i);
      const size_t eol = text.find('\n', i);
      if (close != std::string_view::npos && (eol == std::string_view::npos || close < eol)) {
        out.add(i, close + 1, colors::Annotation);
        i = close + 1;
        continue;
      }
    }
    if (syntax.annotations && c == '@' && i + 1 < size && isIdentStart(static_cast<unsigned char>(text[i + 1]))) {
      size_t j = i + 1;
      while (j < size && (isIdentPart(static_cast<unsigned char>(text[j])) || text[j] == '.')) {
        ++j;
      }
      out.add(i, j, colors::Annotation);
      i = j;
      continue;
    }
    if (syntax.dollarVariables && c == '$' && i + 1 < size) {
      size_t j = i + 1;
      if (text[j] == '{') {
        const size_t close = text.find('}', j);
        j = close == std::string_view::npos ? size : close + 1;
      } else if (text[j] == '(' || text[j] == '?' || text[j] == '#' || text[j] == '@' || text[j] == '*' ||
                 text[j] == '!' || text[j] == '$') {
        ++j;
      } else {
        while (j < size && isIdentPart(static_cast<unsigned char>(text[j]))) {
          ++j;
        }
      }
      if (j > i + 1) {
        out.add(i, j, colors::Variable);
        i = j;
        continue;
      }
    }
    if (syntax.rustMarkup && c == '\'' && i + 1 < size && isIdentStart(static_cast<unsigned char>(text[i + 1]))) {
      // 'a is a lifetime unless it closes right away as the character literal 'a'.
      size_t j = i + 1;
      while (j < size && isIdentPart(static_cast<unsigned char>(text[j]))) {
        ++j;
      }
      const bool charLiteral = j < size && text[j] == '\'' && (j - i - 1 == 1 || static_cast<unsigned char>(text[i + 1]) >= 0x80);
      if (!charLiteral) {
        out.add(i, j, colors::Annotation);
        i = j;
        continue;
      }
    }
    if (syntax.backtickStrings && c == '`') {
      size_t end = text.find('`', i + 1);
      end = end == std::string_view::npos ? size : end + 1;
      out.add(i, end, colors::String);
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
      const std::string folded = syntax.ignoreCase ? lowered(word) : std::string();
      const std::string_view key = syntax.ignoreCase ? std::string_view(folded) : word;
      const bool followedByCall = j < size && text[j] == '(';
      const bool macroCall = syntax.rustMarkup && j + 1 < size && text[j] == '!' && text[j + 1] != '=';
      if (syntax.keywords != nullptr && syntax.keywords->count(key) != 0) {
        out.add(i, j, colors::Keyword);
      } else if ((syntax.types != nullptr && syntax.types->count(key) != 0) ||
                 (syntax.upperIsType && std::isupper(c) != 0 && !followedByCall)) {
        out.add(i, j, colors::Type);
      } else if (followedByCall) {
        out.add(i, j, colors::Function);
      } else if (macroCall) {
        out.add(i, j + 1, colors::Function);
        j += 1;
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

const Words JsKeywords{"abstract", "as",         "async",    "await",   "break",    "case",     "catch",   "class",
                       "const",    "continue",   "debugger", "declare", "default",  "delete",   "do",      "else",
                       "enum",     "export",     "extends",  "false",   "finally",  "for",      "from",    "function",
                       "get",      "if",         "implements", "import", "in",      "instanceof", "interface", "keyof",
                       "let",      "namespace",  "new",      "null",    "of",       "package",  "private", "protected",
                       "public",   "readonly",   "return",   "set",     "static",   "super",    "switch",  "this",
                       "throw",    "true",       "try",      "type",    "typeof",   "undefined", "var",    "void",
                       "while",    "with",       "yield"};
const Words JsTypes{"string", "number", "boolean", "any", "unknown", "never", "object", "symbol", "bigint"};

const Words RustKeywords{"as",    "async", "await", "break",  "const", "continue", "crate", "dyn",   "else",  "enum",
                         "extern", "false", "fn",    "for",    "if",    "impl",     "in",    "let",   "loop",  "match",
                         "mod",   "move",  "mut",   "pub",    "ref",   "return",   "self",  "static", "struct", "super",
                         "trait", "true",  "type",  "union",  "unsafe", "use",     "where", "while"};
const Words RustTypes{"i8",  "i16", "i32", "i64", "i128", "isize", "u8",   "u16",    "u32",   "u64",
                      "u128", "usize", "f32", "f64", "bool", "char",  "str", "String", "Vec",   "Option",
                      "Result", "Box", "Self", "Some", "None", "Ok",   "Err"};

const Words GoKeywords{"break",  "case", "chan", "const", "continue", "default", "defer",  "else", "fallthrough",
                       "for",    "func", "go",   "goto",  "if",       "import",  "interface", "map", "package",
                       "range",  "return", "select", "struct", "switch", "type", "var",   "true", "false",
                       "nil",    "iota"};
const Words GoTypes{"bool",   "byte",    "complex64", "complex128", "error", "float32", "float64", "int",
                    "int8",   "int16",   "int32",     "int64",      "rune",  "string",  "uint",    "uint8",
                    "uint16", "uint32",  "uint64",    "uintptr",    "any"};

const Words JavaKeywords{"abstract", "assert",  "break",     "case",     "catch",   "class",   "const",     "continue",
                         "default",  "do",      "else",      "enum",     "extends", "final",   "finally",   "for",
                         "goto",     "if",      "implements", "import",  "instanceof", "interface", "native", "new",
                         "package",  "private", "protected", "public",   "record",  "return",  "sealed",    "permits",
                         "static",   "strictfp", "super",    "switch",   "synchronized", "this", "throw",   "throws",
                         "transient", "try",    "var",       "volatile", "while",   "yield",   "true",      "false",
                         "null"};
const Words JavaTypes{"boolean", "byte", "char", "double", "float", "int", "long", "short", "void"};

const Words KotlinKeywords{"abstract", "as",       "break",   "by",       "catch",   "class",    "companion", "const",
                           "constructor", "continue", "data", "do",       "else",    "enum",     "false",     "finally",
                           "for",      "fun",      "if",      "import",   "in",      "init",     "inner",     "interface",
                           "internal", "is",       "lateinit", "null",    "object",  "open",     "override",  "package",
                           "private",  "protected", "public", "return",   "sealed",  "super",    "suspend",   "this",
                           "throw",    "true",     "try",     "typealias", "val",   "var",      "when",      "while"};
const Words KotlinTypes{"Int", "Long", "Short", "Byte", "Float", "Double", "Boolean", "Char", "String", "Unit", "Any", "Nothing"};

const Words SwiftKeywords{"actor",   "any",     "associatedtype", "async",  "await",   "break",   "case",     "catch",
                          "class",   "continue", "default",      "defer",  "deinit",  "do",      "else",     "enum",
                          "extension", "fallthrough", "false",   "fileprivate", "for", "func",   "guard",    "if",
                          "import",  "in",      "init",          "inout",  "internal", "is",      "let",      "nil",
                          "open",    "operator", "private",      "protocol", "public", "repeat",  "rethrows", "return",
                          "self",    "Self",    "some",          "static", "struct",  "subscript", "super",  "switch",
                          "throw",   "throws",  "true",          "try",    "typealias", "var",    "where",    "while"};
const Words SwiftTypes{"Int", "Int8", "Int16", "Int32", "Int64", "UInt", "Double", "Float", "Bool", "String", "Character", "Void"};

const Words ShellKeywords{"if",     "then",   "else",  "elif",  "fi",    "for",    "while", "until", "do",    "done",
                          "case",   "esac",   "in",    "function", "select", "time", "return", "exit", "break", "continue",
                          "local",  "export", "readonly", "declare", "unset", "shift", "source", "alias", "set",  "trap",
                          "eval",   "exec"};

const Words SqlKeywords{"select",  "from",   "where",   "insert",  "into",   "values", "update",  "set",     "delete",
                        "create",  "table",  "drop",    "alter",   "add",    "column", "index",   "view",    "join",
                        "inner",   "left",   "right",   "outer",   "full",   "cross",  "on",      "group",   "by",
                        "order",   "having", "limit",   "offset",  "union",  "all",    "distinct", "as",     "and",
                        "or",      "not",    "null",    "is",      "in",     "like",   "between", "exists",  "case",
                        "when",    "then",   "else",    "end",     "primary", "key",   "foreign", "references", "default",
                        "unique",  "check",  "constraint", "begin", "commit", "rollback", "transaction", "with", "returning",
                        "asc",     "desc",   "true",    "false"};
const Words SqlTypes{"int",     "integer", "bigint", "smallint", "tinyint", "decimal", "numeric", "float",    "real",
                     "double",  "varchar", "char",   "text",     "boolean", "bool",    "date",    "time",     "timestamp",
                     "datetime", "blob",   "json",   "uuid",     "serial"};

const Words CMakeKeywords{"if",       "elseif",   "else",   "endif",   "foreach",  "endforeach", "while",  "endwhile",
                          "function", "endfunction", "macro", "endmacro", "return", "break",     "continue", "not",
                          "and",      "or",       "true",   "false",   "on",       "off",        "yes",    "no"};

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

bool isNameChar(unsigned char c) {
  return std::isalnum(c) != 0 || c == '_' || c == '-' || c == ':' || c == '.' || c >= 0x80;
}

// HTML and XML: comments, declarations, tag names (with their brackets), attribute names and values, entities.
void lexHtml(std::string_view text, Emitter &out) {
  const size_t size = text.size();
  size_t i = 0;
  while (i < size) {
    const char c = text[i];
    if (c == '<') {
      if (text.compare(i, 4, "<!--") == 0) {
        size_t end = text.find("-->", i + 4);
        end = end == std::string_view::npos ? size : end + 3;
        out.add(i, end, colors::Comment, PS_UI_STYLE_ITALIC);
        i = end;
        continue;
      }
      if (i + 1 < size && (text[i + 1] == '!' || text[i + 1] == '?')) {
        size_t end = text.find('>', i);
        end = end == std::string_view::npos ? size : end + 1;
        out.add(i, end, colors::Keyword);
        i = end;
        continue;
      }
      size_t j = i + 1;
      if (j < size && text[j] == '/') {
        ++j;
      }
      if (j < size && std::isalpha(static_cast<unsigned char>(text[j])) != 0) {
        while (j < size && isNameChar(static_cast<unsigned char>(text[j]))) {
          ++j;
        }
        out.add(i, j, colors::Keyword);
        bool closed = false;
        while (j < size && !closed) {
          const char ch = text[j];
          if (ch == '>') {
            out.add(j, j + 1, colors::Keyword);
            ++j;
            closed = true;
          } else if (ch == '/' && j + 1 < size && text[j + 1] == '>') {
            out.add(j, j + 2, colors::Keyword);
            j += 2;
            closed = true;
          } else if (ch == '"' || ch == '\'') {
            size_t end = text.find(ch, j + 1);
            end = end == std::string_view::npos ? size : end + 1;
            out.add(j, end, colors::String);
            j = end;
          } else if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '@' || ch == ':') {
            size_t end = j + 1;
            while (end < size && isNameChar(static_cast<unsigned char>(text[end]))) {
              ++end;
            }
            out.add(j, end, colors::Type);
            j = end;
          } else {
            ++j;
          }
        }
        i = j;
        continue;
      }
    } else if (c == '&') {
      size_t j = i + 1;
      while (j < size && j - i < 10 && (std::isalnum(static_cast<unsigned char>(text[j])) != 0 || text[j] == '#')) {
        ++j;
      }
      if (j < size && text[j] == ';' && j > i + 1) {
        out.add(i, j + 1, colors::Number);
        i = j + 1;
        continue;
      }
    }
    ++i;
  }
}

// CSS values: strings, numbers with units, hex colours, !important, function names.
void lexCssValue(std::string_view text, size_t begin, size_t end, Emitter &out) {
  size_t i = begin;
  while (i < end) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '/' && i + 1 < end && text[i + 1] == '*') {
      size_t close = text.find("*/", i + 2);
      close = close == std::string_view::npos ? end : std::min(close + 2, end);
      out.add(i, close, colors::Comment, PS_UI_STYLE_ITALIC);
      i = close;
    } else if (c == '"' || c == '\'') {
      size_t j = i + 1;
      while (j < end && text[j] != static_cast<char>(c)) {
        j += text[j] == '\\' ? 2 : 1;
      }
      j = std::min(j + 1, end);
      out.add(i, j, colors::String);
      i = j;
    } else if (c == '#' && i + 1 < end && std::isxdigit(static_cast<unsigned char>(text[i + 1])) != 0) {
      size_t j = i + 1;
      while (j < end && std::isxdigit(static_cast<unsigned char>(text[j])) != 0) {
        ++j;
      }
      out.add(i, j, colors::Number);
      i = j;
    } else if (c == '!' && text.compare(i, 10, "!important") == 0) {
      out.add(i, i + 10, colors::Keyword);
      i += 10;
    } else if (std::isdigit(c) != 0 || ((c == '.' || c == '-' || c == '+') && i + 1 < end &&
                                       (std::isdigit(static_cast<unsigned char>(text[i + 1])) != 0 ||
                                        (text[i + 1] == '.' && i + 2 < end)))) {
      size_t j = i + 1;
      while (j < end && (std::isdigit(static_cast<unsigned char>(text[j])) != 0 || text[j] == '.')) {
        ++j;
      }
      while (j < end && (std::isalpha(static_cast<unsigned char>(text[j])) != 0 || text[j] == '%')) {
        ++j;
      }
      out.add(i, j, colors::Number);
      i = j;
    } else if (isIdentStart(c) || c == '-') {
      size_t j = i + 1;
      while (j < end && (isIdentPart(static_cast<unsigned char>(text[j])) || text[j] == '-')) {
        ++j;
      }
      if (j < end && text[j] == '(') {
        out.add(i, j, colors::Function);
      }
      i = j;
    } else {
      ++i;
    }
  }
}

void lexCss(std::string_view text, Emitter &out) {
  const size_t size = text.size();
  size_t i = 0;
  while (i < size) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (std::isspace(c) != 0 || c == '{' || c == '}' || c == ';') {
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < size && text[i + 1] == '*') {
      size_t end = text.find("*/", i + 2);
      end = end == std::string_view::npos ? size : end + 2;
      out.add(i, end, colors::Comment, PS_UI_STYLE_ITALIC);
      i = end;
      continue;
    }
    // The statement runs to the next "{", ";" or "}" (skipping strings and comments).
    size_t stop = i;
    while (stop < size && text[stop] != '{' && text[stop] != ';' && text[stop] != '}') {
      if (text[stop] == '"' || text[stop] == '\'') {
        const char quote = text[stop];
        ++stop;
        while (stop < size && text[stop] != quote) {
          stop += text[stop] == '\\' ? 2 : 1;
        }
      } else if (text[stop] == '/' && stop + 1 < size && text[stop + 1] == '*') {
        const size_t close = text.find("*/", stop + 2);
        stop = close == std::string_view::npos ? size : close + 1;
      }
      ++stop;
    }
    stop = std::min(stop, size);
    if (c == '@') {
      size_t j = i + 1;
      while (j < stop && (isIdentPart(static_cast<unsigned char>(text[j])) || text[j] == '-')) {
        ++j;
      }
      out.add(i, j, colors::Keyword);
      lexCssValue(text, j, stop, out);
    } else if (stop < size && text[stop] == '{') {
      // Selector: one run per simple selector (".a", "#b", "ul", ":hover", "[x=y]").
      size_t j = i;
      while (j < stop) {
        const unsigned char d = static_cast<unsigned char>(text[j]);
        if (std::isspace(d) != 0 || d == ',' || d == '>' || d == '+' || d == '~') {
          ++j;
          continue;
        }
        size_t k = j;
        int brackets = 0;
        while (k < stop) {
          const char e = text[k];
          if (e == '[' || e == '(') {
            ++brackets;
          } else if (e == ']' || e == ')') {
            --brackets;
          } else if (brackets <= 0 && (std::isspace(static_cast<unsigned char>(e)) != 0 || e == ',' || e == '>' ||
                                       e == '+' || e == '~')) {
            break;
          }
          ++k;
        }
        out.add(j, k, colors::Type);
        j = k;
      }
    } else {
      // Declaration: "property: value".
      const size_t colon = text.find(':', i);
      if (colon != std::string_view::npos && colon < stop) {
        size_t nameEnd = colon;
        while (nameEnd > i && std::isspace(static_cast<unsigned char>(text[nameEnd - 1])) != 0) {
          --nameEnd;
        }
        out.add(i, nameEnd, colors::Keyword);
        lexCssValue(text, colon + 1, stop, out);
      } else {
        lexCssValue(text, i, stop, out);
      }
    }
    i = std::max(stop, i + 1);
  }
}

bool yamlWord(std::string_view word) {
  static const Words words{"true", "false", "null", "yes", "no", "on", "off", "~", "True", "False", "Null", "TRUE",
                           "FALSE", "NULL", "Yes", "No"};
  return words.count(word) != 0;
}

bool looksNumeric(std::string_view token) {
  if (token.empty()) {
    return false;
  }
  size_t i = (token[0] == '-' || token[0] == '+') ? 1 : 0;
  if (i >= token.size() || std::isdigit(static_cast<unsigned char>(token[i])) == 0) {
    return false;
  }
  for (; i < token.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(token[i]);
    if (std::isalnum(c) == 0 && c != '_' && c != '.' && c != '-' && c != ':' && c != '+') {
      return false;
    }
  }
  return true;
}

// The value part of a "key: value" line (or a bare flow/sequence item) of YAML or TOML.
void lexConfigValue(std::string_view text, size_t begin, size_t end, bool yaml, Emitter &out, bool semicolonComments) {
  size_t i = begin;
  while (i < end) {
    const char c = text[i];
    if (c == ' ' || c == '\t' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == '=') {
      ++i;
      continue;
    }
    if (c == '#' || (semicolonComments && c == ';')) {
      out.add(i, end, colors::Comment, PS_UI_STYLE_ITALIC);
      return;
    }
    if (c == '"' || c == '\'') {
      size_t j = i + 1;
      while (j < end && text[j] != c) {
        j += (text[j] == '\\' && c == '"') ? 2 : 1;
      }
      j = std::min(j + 1, end);
      out.add(i, j, colors::String);
      i = j;
      continue;
    }
    size_t j = i;
    while (j < end && text[j] != ' ' && text[j] != '\t' && text[j] != ',' && text[j] != ']' && text[j] != '}' &&
           text[j] != '[' && text[j] != '{') {
      ++j;
    }
    const std::string_view token = text.substr(i, j - i);
    if (yaml && (c == '&' || c == '*' || c == '!')) {
      out.add(i, j, colors::Annotation);
    } else if (yaml && (token == "|" || token == ">" || token == "|-" || token == ">-")) {
      out.add(i, j, colors::Keyword);
    } else if (looksNumeric(token)) {
      out.add(i, j, colors::Number);
    } else if (yaml ? yamlWord(token) : (token == "true" || token == "false")) {
      out.add(i, j, colors::Keyword);
    }
    i = std::max(j, i + 1);
  }
}

void lexYaml(std::string_view text, Emitter &out) {
  const size_t size = text.size();
  size_t lineStart = 0;
  while (lineStart < size) {
    size_t lineEnd = text.find('\n', lineStart);
    lineEnd = lineEnd == std::string_view::npos ? size : lineEnd;
    size_t pos = lineStart;
    while (pos < lineEnd && (text[pos] == ' ' || text[pos] == '\t')) {
      ++pos;
    }
    if (pos < lineEnd && text[pos] == '#') {
      out.add(pos, lineEnd, colors::Comment, PS_UI_STYLE_ITALIC);
    } else if (pos < lineEnd) {
      if (pos == lineStart && (text.compare(pos, 3, "---") == 0 || text.compare(pos, 3, "...") == 0)) {
        out.add(pos, pos + 3, colors::Keyword);
        pos += 3;
      }
      while (pos < lineEnd && text[pos] == '-' && (pos + 1 == lineEnd || text[pos + 1] == ' ')) {
        out.add(pos, pos + 1, colors::Marker, PS_UI_STYLE_BOLD);
        ++pos;
        while (pos < lineEnd && text[pos] == ' ') {
          ++pos;
        }
      }
      // A key is a quoted or plain scalar followed by ":" and a space or the end of the line.
      size_t colon = std::string_view::npos;
      size_t keyEnd = pos;
      if (pos < lineEnd && (text[pos] == '"' || text[pos] == '\'')) {
        const size_t close = text.find(text[pos], pos + 1);
        if (close != std::string_view::npos && close < lineEnd) {
          size_t after = close + 1;
          while (after < lineEnd && text[after] == ' ') {
            ++after;
          }
          if (after < lineEnd && text[after] == ':') {
            colon = after;
            keyEnd = close + 1;
          }
        }
      } else if (pos < lineEnd && std::string_view("[{&*!|>%@`#").find(text[pos]) == std::string_view::npos) {
        for (size_t k = pos; k < lineEnd; ++k) {
          if (text[k] == ':' && (k + 1 == lineEnd || text[k + 1] == ' ')) {
            colon = k;
            keyEnd = k;
            break;
          }
          if (text[k] == '#' && k > pos && text[k - 1] == ' ') {
            break;
          }
        }
      }
      if (colon != std::string_view::npos) {
        while (keyEnd > pos && text[keyEnd - 1] == ' ') {
          --keyEnd;
        }
        out.add(pos, keyEnd, colors::Type);
        pos = colon + 1;
      }
      lexConfigValue(text, pos, lineEnd, true, out, false);
    }
    lineStart = lineEnd + 1;
  }
}

// TOML and INI: [tables], key = value, # (and, for INI, ;) comments.
void lexToml(std::string_view text, bool ini, Emitter &out) {
  const size_t size = text.size();
  size_t lineStart = 0;
  while (lineStart < size) {
    size_t lineEnd = text.find('\n', lineStart);
    lineEnd = lineEnd == std::string_view::npos ? size : lineEnd;
    size_t pos = lineStart;
    while (pos < lineEnd && (text[pos] == ' ' || text[pos] == '\t')) {
      ++pos;
    }
    if (pos < lineEnd && (text[pos] == '#' || (ini && text[pos] == ';'))) {
      out.add(pos, lineEnd, colors::Comment, PS_UI_STYLE_ITALIC);
    } else if (pos < lineEnd && text[pos] == '[') {
      size_t close = text.find(']', pos);
      close = close == std::string_view::npos || close >= lineEnd ? lineEnd : close + 1;
      if (close < lineEnd && text[close] == ']') {
        ++close; // the second bracket of [[array]]
      }
      out.add(pos, close, colors::Heading, PS_UI_STYLE_BOLD);
      lexConfigValue(text, close, lineEnd, false, out, ini);
    } else if (pos < lineEnd) {
      size_t eq = pos;
      while (eq < lineEnd && text[eq] != '=') {
        ++eq;
      }
      if (eq < lineEnd) {
        size_t keyEnd = eq;
        while (keyEnd > pos && (text[keyEnd - 1] == ' ' || text[keyEnd - 1] == '\t')) {
          --keyEnd;
        }
        out.add(pos, keyEnd, colors::Type);
        lexConfigValue(text, eq + 1, lineEnd, false, out, ini);
      }
    }
    lineStart = lineEnd + 1;
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
  if (name == "CMakeLists.txt") {
    return Language::CMake;
  }
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
      {"md", Language::Markdown},       {"markdown", Language::Markdown},
      {"js", Language::JavaScript},     {"mjs", Language::JavaScript}, {"cjs", Language::JavaScript},
      {"jsx", Language::JavaScript},    {"ts", Language::JavaScript},  {"tsx", Language::JavaScript},
      {"mts", Language::JavaScript},    {"cts", Language::JavaScript}, {"rs", Language::Rust},
      {"go", Language::Go},             {"java", Language::Java},      {"kt", Language::Kotlin},
      {"kts", Language::Kotlin},        {"swift", Language::Swift},    {"sh", Language::Shell},
      {"bash", Language::Shell},        {"zsh", Language::Shell},      {"ksh", Language::Shell},
      {"sql", Language::Sql},           {"cmake", Language::CMake},    {"html", Language::Html},
      {"htm", Language::Html},          {"xhtml", Language::Html},     {"xml", Language::Html},
      {"svg", Language::Html},          {"plist", Language::Html},     {"css", Language::Css},
      {"yml", Language::Yaml},          {"yaml", Language::Yaml},      {"toml", Language::Toml},
      {"ini", Language::Ini},           {"cfg", Language::Ini},        {"conf", Language::Ini}};
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
  case Language::JavaScript: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.backtickStrings = true;
    syntax.annotations = true;
    syntax.upperIsType = true;
    syntax.keywords = &JsKeywords;
    syntax.types = &JsTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Rust: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.rustMarkup = true;
    syntax.upperIsType = true;
    syntax.keywords = &RustKeywords;
    syntax.types = &RustTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Go: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.backtickStrings = true;
    syntax.keywords = &GoKeywords;
    syntax.types = &GoTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Java: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.tripleQuotes = true;
    syntax.annotations = true;
    syntax.upperIsType = true;
    syntax.keywords = &JavaKeywords;
    syntax.types = &JavaTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Kotlin: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.tripleQuotes = true;
    syntax.annotations = true;
    syntax.upperIsType = true;
    syntax.keywords = &KotlinKeywords;
    syntax.types = &KotlinTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Swift: {
    CodeSyntax syntax;
    syntax.lineComment = "//";
    syntax.blockComments = true;
    syntax.tripleQuotes = true;
    syntax.annotations = true;
    syntax.upperIsType = true;
    syntax.keywords = &SwiftKeywords;
    syntax.types = &SwiftTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Shell: {
    CodeSyntax syntax;
    syntax.lineComment = "#";
    syntax.singleQuotes = true;
    syntax.backtickStrings = true;
    syntax.dollarVariables = true;
    syntax.keywords = &ShellKeywords;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Sql: {
    CodeSyntax syntax;
    syntax.lineComment = "--";
    syntax.blockComments = true;
    syntax.singleQuotes = true;
    syntax.ignoreCase = true;
    syntax.keywords = &SqlKeywords;
    syntax.types = &SqlTypes;
    lexCode(text, syntax, out);
    break;
  }
  case Language::CMake: {
    CodeSyntax syntax;
    syntax.lineComment = "#";
    syntax.dollarVariables = true;
    syntax.ignoreCase = true;
    syntax.keywords = &CMakeKeywords;
    lexCode(text, syntax, out);
    break;
  }
  case Language::Html:
    lexHtml(text, out);
    break;
  case Language::Css:
    lexCss(text, out);
    break;
  case Language::Yaml:
    lexYaml(text, out);
    break;
  case Language::Toml:
    lexToml(text, false, out);
    break;
  case Language::Ini:
    lexToml(text, true, out);
    break;
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
