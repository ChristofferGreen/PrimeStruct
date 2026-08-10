// Generates a per-module symbol manifest for a stdlib module, per TODO-5224
// (docs/LibrarySymbolManifestLazyImports.md, Phase 1). Reads a single stdlib
// .prime file directly (bypassing ImportResolver, so no other modules get
// spliced in), locates each top-level definition's exact source span via the
// real Lexer/Parser token stream, differentially re-parses each slice to
// prove it reproduces the same definition in isolation, and writes a
// key=value manifest file colocated with the module source.
//
// Usage:
//   generate_stdlib_manifest <source.prime> <module-root-path> <output.psmeta>
//
// Example:
//   generate_stdlib_manifest stdlib/std/image/image.prime /std/image
//       stdlib/std/image/image.psmeta

#include "primec/Ast.h"
#include "primec/AstPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/TextFilterPipeline.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using primec::Definition;
using primec::Lexer;
using primec::Parser;
using primec::Program;
using primec::Token;
using primec::TokenKind;

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

std::vector<std::string> splitLines(const std::string &text) {
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

bool isBlankLine(const std::string &line) {
  return line.find_first_not_of(" \t\r") == std::string::npos;
}

bool belongsToModule(const std::string &fullPath, const std::string &moduleRoot) {
  if (fullPath == moduleRoot) {
    return true;
  }
  return fullPath.size() > moduleRoot.size() &&
         fullPath.compare(0, moduleRoot.size(), moduleRoot) == 0 &&
         fullPath[moduleRoot.size()] == '/';
}

// FNV-1a 64-bit - a stable, dependency-free content hash. Not cryptographic;
// only needs to detect content drift, per the manifest's purpose.
uint64_t fnv1a64(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

// Re-wraps an isolated definition slice in the same `namespace X { ... }`
// nesting it originally appeared under (per Definition::namespacePrefix),
// so its fullPath resolves identically when reparsed alone. Definitions
// whose own name is already an absolute `/a/b/c` path ignore the wrapping
// namespace when computing fullPath (see Parser::makeFullPath), so this is
// harmless for those too.
std::string wrapWithNamespace(const std::string &namespacePrefix, const std::string &body) {
  std::vector<std::string> segments;
  std::string current;
  for (const char c : namespacePrefix) {
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

struct SymbolEntry {
  std::string path;
  int startLine = 0;
  int endLine = 0;
  std::string sourceSlice;
  uint64_t contentHash = 0;
};

// Finds the token whose position matches a definition's recorded name
// position (sourceLine/sourceColumn set by the parser to the name token).
int findNameTokenIndex(const std::vector<Token> &tokens, const Definition &def) {
  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];
    if (token.kind == TokenKind::Identifier && token.line == def.sourceLine &&
        token.column == def.sourceColumn && token.text == def.name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// From the name token, scans forward to the first '{' (the definition's
// body) and then brace-matches to find the closing '}'. Returns the line of
// that closing brace, or -1 if the token stream runs out unbalanced.
int findDefinitionBodyEndLine(const std::vector<Token> &tokens, int nameTokenIndex) {
  size_t i = static_cast<size_t>(nameTokenIndex);
  while (i < tokens.size() && tokens[i].kind != TokenKind::LBrace) {
    ++i;
  }
  if (i >= tokens.size()) {
    return -1;
  }
  int depth = 0;
  for (; i < tokens.size(); ++i) {
    if (tokens[i].kind == TokenKind::LBrace) {
      ++depth;
    } else if (tokens[i].kind == TokenKind::RBrace) {
      --depth;
      if (depth == 0) {
        return tokens[i].line;
      }
    }
  }
  return -1;
}

// Finds where this definition's own textual slice should start: the line
// right after the nearest preceding structural boundary (a brace, or a
// `namespace` keyword introducing a wrapping block). This deliberately
// excludes any enclosing `namespace X { ... }` opener lines, which wrap many
// sibling definitions and aren't part of any single one of them - only the
// definition's own `[transforms]` header and signature line should be
// included, matching how fullPath already encodes the namespace prefix.
int findDefinitionStartLine(const std::vector<Token> &tokens, int nameTokenIndex) {
  for (int i = nameTokenIndex - 1; i >= 0; --i) {
    const TokenKind kind = tokens[static_cast<size_t>(i)].kind;
    if (kind == TokenKind::LBrace || kind == TokenKind::RBrace ||
        kind == TokenKind::KeywordNamespace) {
      return tokens[static_cast<size_t>(i + 1)].line;
    }
  }
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "usage: generate_stdlib_manifest <source.prime> "
                 "<module-root-path> <output.psmeta>\n";
    return 1;
  }
  const std::string sourcePath = argv[1];
  const std::string moduleRoot = argv[2];
  const std::string outputPath = argv[3];

  std::string rawSource;
  if (!readFile(sourcePath, rawSource)) {
    std::cerr << "error: could not read " << sourcePath << "\n";
    return 1;
  }

  // The real compile pipeline runs TextFilterPipeline (surface-syntax
  // rewrites: collections/operators/implicit-utf8) between import expansion
  // and lexing (see runCompilePipelineTransformStage in CompilePipeline.cpp).
  // Definition::sourceLine/sourceColumn are positions in that filtered text,
  // so slicing must operate on the same filtered text to stay aligned - all
  // line numbers recorded below are filtered-text line numbers, not raw
  // .prime file line numbers.
  primec::TextFilterPipeline textPipeline;
  std::string source;
  std::string filterError;
  if (!textPipeline.apply(rawSource, source, filterError)) {
    std::cerr << "error: text filter pipeline failed for " << sourcePath
               << ": " << filterError << "\n";
    return 1;
  }

  Lexer lexer(source);
  const std::vector<Token> tokens = lexer.tokenize();
  Parser parser(tokens, true);
  Program program;
  std::string parseError;
  if (!parser.parse(program, parseError)) {
    std::cerr << "error: failed to parse " << sourcePath << ": " << parseError
               << "\n";
    return 1;
  }

  std::vector<const Definition *> moduleDefs;
  for (const Definition &def : program.definitions) {
    if (belongsToModule(def.fullPath, moduleRoot)) {
      moduleDefs.push_back(&def);
    }
  }
  std::stable_sort(moduleDefs.begin(), moduleDefs.end(),
                    [](const Definition *a, const Definition *b) {
                      return a->sourceLine < b->sourceLine;
                    });

  const std::vector<std::string> lines = splitLines(source);
  std::vector<SymbolEntry> entries;

  for (const Definition *def : moduleDefs) {
    const int nameTokenIndex = findNameTokenIndex(tokens, *def);
    if (nameTokenIndex < 0) {
      std::cerr << "error: could not locate name token for " << def->fullPath
                 << " at line " << def->sourceLine << "\n";
      return 1;
    }
    const int endLine = findDefinitionBodyEndLine(tokens, nameTokenIndex);
    if (endLine < 0) {
      std::cerr << "error: could not find closing brace for "
                 << def->fullPath << "\n";
      return 1;
    }

    int startLine = findDefinitionStartLine(tokens, nameTokenIndex);
    while (startLine < endLine && startLine < static_cast<int>(lines.size()) &&
           isBlankLine(lines[startLine])) {
      ++startLine;
    }
    if (startLine < 1) {
      startLine = 1;
    }

    std::ostringstream slice;
    for (int lineNum = startLine; lineNum <= endLine; ++lineNum) {
      if (lineNum > startLine) {
        slice << "\n";
      }
      slice << lines[static_cast<size_t>(lineNum)];
    }

    // Differential check: re-parse the isolated slice (rewrapped in its
    // original namespace nesting, so fullPath resolves the same way) and
    // confirm it produces the *same AST shape* as the whole-file parse -
    // TODO-5224's acceptance criteria - not just a matching path.
    const std::string sliceSource = slice.str();
    const std::string wrappedSlice = wrapWithNamespace(def->namespacePrefix, sliceSource);
    Lexer sliceLexer(wrappedSlice);
    const std::vector<Token> sliceTokens = sliceLexer.tokenize();
    Parser sliceParser(sliceTokens, true);
    Program sliceProgram;
    std::string sliceError;
    if (!sliceParser.parse(sliceProgram, sliceError)) {
      std::cerr << "error: slice for " << def->fullPath
                 << " failed to reparse: " << sliceError << "\n";
      return 1;
    }
    const auto slicedDefIt =
        std::find_if(sliceProgram.definitions.begin(), sliceProgram.definitions.end(),
                     [&](const Definition &slicedDef) {
                       return slicedDef.fullPath == def->fullPath;
                     });
    if (slicedDefIt == sliceProgram.definitions.end()) {
      std::cerr << "error: slice for " << def->fullPath
                 << " did not reproduce the same definition in isolation "
                    "(lines "
                 << startLine << "-" << endLine << ")\n";
      return 1;
    }

    primec::AstPrinter printer;
    Program originalSingle;
    originalSingle.definitions.push_back(*def);
    Program slicedSingle;
    slicedSingle.definitions.push_back(*slicedDefIt);
    const std::string originalCanonical = printer.print(originalSingle);
    const std::string slicedCanonical = printer.print(slicedSingle);
    if (originalCanonical != slicedCanonical) {
      std::cerr << "error: slice for " << def->fullPath
                 << " parses to a different AST shape in isolation than in "
                    "the whole file\n";
      return 1;
    }

    // Content hash: hash the isolated definition's own canonical AST
    // printout, so whitespace/comment differences in the source don't
    // change the hash, per the content-addressed design in
    // docs/LibrarySymbolManifestLazyImports.md.
    SymbolEntry entry;
    entry.path = def->fullPath;
    entry.startLine = startLine;
    entry.endLine = endLine;
    entry.sourceSlice = sliceSource;
    entry.contentHash = fnv1a64(slicedCanonical);
    entries.push_back(std::move(entry));
  }

  std::ofstream out(outputPath, std::ios::binary);
  if (!out) {
    std::cerr << "error: could not write " << outputPath << "\n";
    return 1;
  }
  out << "# Auto-generated by tools/generate_stdlib_manifest - do not hand-edit.\n";
  out << "# Module: " << moduleRoot << "\n";
  out << "# Source: " << sourcePath << "\n";
  out << "# Line numbers below are post-text-filter-pipeline line numbers\n";
  out << "# (collections/operators/implicit-utf8 surface rewrites applied),\n";
  out << "# matching Definition::sourceLine's coordinate space - not raw\n";
  out << "# file line numbers.\n";
  for (const SymbolEntry &entry : entries) {
    out << "\n[symbol]\n";
    out << "path = " << entry.path << "\n";
    out << "start_line = " << entry.startLine << "\n";
    out << "end_line = " << entry.endLine << "\n";
    out << "content_hash = " << std::hex << entry.contentHash << std::dec
        << "\n";
  }

  std::cerr << "wrote " << entries.size() << " symbols to " << outputPath
             << "\n";
  return 0;
}
