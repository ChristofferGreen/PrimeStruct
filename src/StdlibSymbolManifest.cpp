#include "primec/StdlibSymbolManifest.h"

#include "primec/Ast.h"
#include "primec/AstPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/TextFilterPipeline.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace primec {

namespace {

std::string trimAscii(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

bool readFile(const std::string &path, std::string &out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  out = buffer.str();
  return true;
}

} // namespace

uint64_t fnv1a64(std::string_view text) {
  uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::vector<std::string> splitSourceIntoLines(const std::string &text) {
  std::vector<std::string> lines;
  lines.emplace_back(); // 1-indexed: lines[0] unused
  std::string current;
  for (const char c : text) {
    if (c == '\n') {
      lines.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  lines.push_back(current);
  return lines;
}

bool applyDefaultStdlibTextFilter(const std::string &rawSource,
                                  std::string &filteredSource,
                                  std::string &error) {
  TextFilterPipeline textPipeline;
  return textPipeline.apply(rawSource, filteredSource, error);
}

std::string wrapDefinitionInNamespace(std::string_view fullPath, const std::string &body) {
  std::string parentPath;
  const size_t lastSlash = fullPath.find_last_of('/');
  if (lastSlash != std::string_view::npos) {
    parentPath = std::string(fullPath.substr(0, lastSlash));
  }

  std::vector<std::string> segments;
  std::string current;
  for (const char c : parentPath) {
    if (c == '/') {
      if (!current.empty()) {
        segments.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    segments.push_back(current);
  }

  std::ostringstream wrapped;
  for (const std::string &segment : segments) {
    wrapped << "namespace " << segment << " {\n";
  }
  wrapped << body << "\n";
  for (size_t i = 0; i < segments.size(); ++i) {
    wrapped << "}\n";
  }
  return wrapped.str();
}

bool readStdlibSymbolManifest(const std::string &manifestPath,
                              std::vector<StdlibSymbolManifestEntry> &out,
                              std::string &error) {
  std::ifstream input(manifestPath);
  if (!input) {
    error = "failed to read stdlib symbol manifest: " + manifestPath;
    return false;
  }

  bool inSymbol = false;
  StdlibSymbolManifestEntry current;
  bool haveStartLine = false;
  bool haveEndLine = false;
  bool haveContentHash = false;

  auto flushSymbol = [&]() -> bool {
    if (!inSymbol) {
      return true;
    }
    if (current.path.empty()) {
      error = "invalid stdlib symbol manifest " + manifestPath + ": symbol entry missing path";
      return false;
    }
    if (!haveStartLine || !haveEndLine || !haveContentHash) {
      error = "invalid stdlib symbol manifest " + manifestPath +
              ": symbol entry " + current.path +
              " missing start_line/end_line/content_hash";
      return false;
    }
    if (current.endLine < current.startLine || current.startLine < 1) {
      error = "invalid stdlib symbol manifest " + manifestPath +
              ": symbol entry " + current.path + " has invalid line bounds";
      return false;
    }
    out.push_back(current);
    current = StdlibSymbolManifestEntry{};
    haveStartLine = false;
    haveEndLine = false;
    haveContentHash = false;
    return true;
  };

  std::string line;
  while (std::getline(input, line)) {
    const std::size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
      line.resize(commentPos);
    }
    const std::string trimmed = trimAscii(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed == "[symbol]") {
      if (!flushSymbol()) {
        return false;
      }
      inSymbol = true;
      continue;
    }
    if (!inSymbol) {
      error = "invalid stdlib symbol manifest " + manifestPath +
              ": expected [symbol] before entries";
      return false;
    }
    const std::size_t equalsPos = trimmed.find('=');
    if (equalsPos == std::string::npos) {
      error = "invalid stdlib symbol manifest " + manifestPath +
              ": expected key = value, got: " + trimmed;
      return false;
    }
    const std::string key = trimAscii(std::string_view(trimmed).substr(0, equalsPos));
    const std::string value = trimAscii(std::string_view(trimmed).substr(equalsPos + 1));
    if (key == "path") {
      current.path = value;
    } else if (key == "start_line") {
      current.startLine = std::atoi(value.c_str());
      haveStartLine = true;
    } else if (key == "end_line") {
      current.endLine = std::atoi(value.c_str());
      haveEndLine = true;
    } else if (key == "content_hash") {
      current.contentHash = std::strtoull(value.c_str(), nullptr, 16);
      haveContentHash = true;
    } else {
      error = "invalid stdlib symbol manifest " + manifestPath + ": unknown key " + key;
      return false;
    }
  }
  if (!flushSymbol()) {
    return false;
  }
  return true;
}

bool extractAndVerifyManifestedSymbolSource(const StdlibSymbolManifestEntry &entry,
                                            const std::string &moduleSourcePath,
                                            std::string &outSourceText,
                                            std::string &error) {
  std::string rawSource;
  if (!readFile(moduleSourcePath, rawSource)) {
    error = "could not read stdlib module source: " + moduleSourcePath;
    return false;
  }

  std::string filteredSource;
  if (!applyDefaultStdlibTextFilter(rawSource, filteredSource, error)) {
    return false;
  }

  const std::vector<std::string> lines = splitSourceIntoLines(filteredSource);
  if (entry.startLine < 1 || entry.endLine < entry.startLine ||
      entry.endLine >= static_cast<int>(lines.size())) {
    error = "manifest entry " + entry.path +
            " has out-of-range line bounds for " + moduleSourcePath +
            " (module source may have shrunk since the manifest was generated)";
    return false;
  }

  std::ostringstream slice;
  for (int lineNum = entry.startLine; lineNum <= entry.endLine; ++lineNum) {
    if (lineNum > entry.startLine) {
      slice << "\n";
    }
    slice << lines[static_cast<size_t>(lineNum)];
  }
  const std::string sliceSource = slice.str();

  const std::string wrapped = wrapDefinitionInNamespace(entry.path, sliceSource);
  Lexer lexer(wrapped);
  const std::vector<Token> tokens = lexer.tokenize();
  Parser parser(tokens, true);
  Program program;
  std::string parseError;
  if (!parser.parse(program, parseError)) {
    error = "manifest entry " + entry.path + " failed to reparse from " +
            moduleSourcePath + ": " + parseError;
    return false;
  }

  const auto definitionIt =
      std::find_if(program.definitions.begin(), program.definitions.end(),
                   [&](const Definition &def) { return def.fullPath == entry.path; });
  if (definitionIt == program.definitions.end()) {
    error = "manifest entry " + entry.path +
            " did not reproduce its definition when reparsed from " + moduleSourcePath +
            " (module source has likely drifted since the manifest was generated; "
            "regenerate it with tools/generate_stdlib_manifest)";
    return false;
  }

  Program single;
  single.definitions.push_back(*definitionIt);
  AstPrinter printer;
  const std::string canonical = printer.print(single);
  const uint64_t hash = fnv1a64(canonical);
  if (hash != entry.contentHash) {
    error = "manifest entry " + entry.path + " content hash mismatch for " +
            moduleSourcePath +
            " (module source has drifted since the manifest was generated; "
            "regenerate it with tools/generate_stdlib_manifest)";
    return false;
  }

  // Return the namespace-wrapped form, not the bare slice: a definition
  // whose own name is a bare identifier (e.g. a struct or method declared
  // inside `namespace ImageError { ... }`) only resolves to its manifested
  // fullPath when reparsed with that namespace nesting present. Splicing
  // the wrapped form makes each symbol independently self-resolving
  // wherever it's spliced in, matching how re-opening the same `namespace
  // X { ... }` path across multiple already-spliced stdlib module files
  // works today.
  outSourceText = wrapped;
  return true;
}

} // namespace primec
