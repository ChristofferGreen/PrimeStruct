#include "primec/frontend/ImportAllowlist.h"

#include "primec/frontend/Lexer.h"
#include "primec/support/SourceLocationMapper.h"

#include <filesystem>
#include <optional>

namespace primec {
namespace {

std::string stripTrailingWildcard(std::string path) {
  if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
    path.erase(path.size() - 2);
  }
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();
  }
  return path;
}

bool isStdlibImportPath(const std::string &path) {
  return path == "/std" || path.rfind("/std/", 0) == 0;
}

bool isUnderStdlibRoot(const std::string &file, const std::vector<std::string> &stdlibRoots) {
  const std::string candidateText = std::filesystem::absolute(file).lexically_normal().generic_string();
  for (const std::string &root : stdlibRoots) {
    const std::string stdText = (std::filesystem::absolute(root) / "std").lexically_normal().generic_string();
    if (candidateText.rfind(stdText + "/", 0) == 0) {
      return true;
    }
  }
  return false;
}

std::string joinPrefixes(const std::vector<std::string> &prefixes) {
  if (prefixes.empty()) {
    return "none";
  }
  std::string text;
  for (const std::string &prefix : prefixes) {
    if (!text.empty()) {
      text += ",";
    }
    text += prefix;
  }
  return text;
}

} // namespace

bool importPathAllowed(const std::string &importPath, const std::vector<std::string> &allowedPrefixes) {
  const std::string path = stripTrailingWildcard(importPath);
  for (const std::string &allowed : allowedPrefixes) {
    const std::string prefix = stripTrailingWildcard(allowed);
    if (path == prefix) {
      return true;
    }
    if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0 &&
        (prefix.back() == '/' || path[prefix.size()] == '/')) {
      return true;
    }
  }
  return false;
}

std::vector<ImportAllowlistViolation>
findImportAllowlistViolations(const ExpandedSource &source,
                              const std::vector<std::string> &allowedPrefixes,
                              const std::vector<std::string> &stdlibRoots) {
  std::vector<ImportAllowlistViolation> violations;
  Lexer lexer(source.text);
  const std::vector<Token> tokens = lexer.tokenize();
  auto skipIgnorable = [&](size_t cursor) {
    while (cursor < tokens.size() && (tokens[cursor].kind == TokenKind::Comment ||
                                      tokens[cursor].kind == TokenKind::Comma ||
                                      tokens[cursor].kind == TokenKind::Semicolon)) {
      ++cursor;
    }
    return cursor;
  };
  for (size_t scan = 0; scan < tokens.size(); ++scan) {
    if (tokens[scan].kind != TokenKind::KeywordImport) {
      continue;
    }
    const std::optional<SourceUnitLocation> location =
        mapExpandedSourceLocation(source, tokens[scan].line, tokens[scan].column);
    if (location.has_value()) {
      if (location->unitKind == SourceUnitKind::Stdlib || location->unitKind == SourceUnitKind::Generated) {
        continue;
      }
      if (location->unitKind == SourceUnitKind::Import && isUnderStdlibRoot(location->file, stdlibRoots)) {
        continue;
      }
    }
    size_t cursor = skipIgnorable(scan + 1);
    while (cursor < tokens.size()) {
      if (tokens[cursor].kind != TokenKind::Identifier || tokens[cursor].text.empty() ||
          tokens[cursor].text[0] != '/') {
        break;
      }
      std::string path = tokens[cursor].text;
      const int line = tokens[cursor].line;
      const int column = tokens[cursor].column;
      const size_t next = skipIgnorable(cursor + 1);
      if (path.back() == '/' && next < tokens.size() && tokens[next].kind == TokenKind::Star) {
        path.pop_back();
        path += "/*";
        cursor = next + 1;
      } else {
        ++cursor;
      }
      if (isStdlibImportPath(path) && !importPathAllowed(path, allowedPrefixes)) {
        ImportAllowlistViolation violation;
        violation.message = "import " + path + " is outside the allowed imports: " + joinPrefixes(allowedPrefixes);
        violation.line = line;
        violation.column = column;
        violations.push_back(std::move(violation));
      }
      cursor = skipIgnorable(cursor);
    }
  }
  return violations;
}

} // namespace primec
