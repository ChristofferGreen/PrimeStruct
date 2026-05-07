#include "IrLowererStringLiteralHelpers.h"

#include <memory>
#include <utility>

#include "IrLowererCallHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "primec/StringLiteral.h"

namespace primec::ir_lowerer {

int32_t internLowererString(const std::string &text, std::vector<std::string> &stringTable) {
  for (size_t i = 0; i < stringTable.size(); ++i) {
    if (stringTable[i] == text) {
      return static_cast<int32_t>(i);
    }
  }
  stringTable.push_back(text);
  return static_cast<int32_t>(stringTable.size() - 1);
}

bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error) {
  ParsedStringLiteral parsed;
  if (!parseStringLiteralToken(text, parsed, error)) {
    return false;
  }
  if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
    error = "ascii string literal contains non-ASCII characters";
    return false;
  }
  decoded = std::move(parsed.decoded);
  return true;
}

StringLiteralHelperContext makeStringLiteralHelperContext(
    std::vector<std::string> &stringTable,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  StringLiteralHelperContext context;
  context.internString = makeInternLowererString(stringTable);
  context.resolveStringTableTarget =
      makeResolveStringTableTarget(stringTable, context.internString, error, semanticProgram);
  return context;
}

InternStringLiteralFn makeInternLowererString(std::vector<std::string> &stringTable) {
  auto *stringTablePtr = &stringTable;
  return [stringTablePtr](const std::string &text) {
    return internLowererString(text, *stringTablePtr);
  };
}

ResolveStringTableTargetFn makeResolveStringTableTarget(const std::vector<std::string> &stringTable,
                                                        const InternStringLiteralFn &internString,
                                                        std::string &error,
                                                        const SemanticProgram *semanticProgram) {
  const auto *stringTablePtr = &stringTable;
  auto internStringFn = internString;
  auto *errorPtr = &error;
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [stringTablePtr, internStringFn, errorPtr, semanticProgram, semanticIndex](
             const Expr &expr, const LocalMap &localsIn, int32_t &stringIndexOut, size_t &lengthOut) {
    return resolveStringTableTarget(
        expr,
        localsIn,
        *stringTablePtr,
        internStringFn,
        stringIndexOut,
        lengthOut,
        *errorPtr,
        semanticProgram,
        semanticIndex.get());
  };
}

bool resolveStringTableTarget(const Expr &expr,
                              const LocalMap &localsIn,
                              const std::vector<std::string> &stringTable,
                              const InternStringLiteralFn &internString,
                              int32_t &stringIndexOut,
                              size_t &lengthOut,
                              std::string &error,
                              const SemanticProgram *semanticProgram,
                              const SemanticProductIndex *semanticIndex) {
  stringIndexOut = -1;
  lengthOut = 0;
  if (expr.kind == Expr::Kind::StringLiteral) {
    std::string decoded;
    if (!parseLowererStringLiteral(expr.stringValue, decoded, error)) {
      return false;
    }
    stringIndexOut = internString(decoded);
    lengthOut = decoded.size();
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    const SemanticStringAccessTargetKind semanticStringKind =
        classifyAccessTargetSemanticStringKind(expr, semanticProgram, semanticIndex);
    if (semanticStringKind == SemanticStringAccessTargetKind::NonString) {
      return false;
    }
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.valueKind != LocalInfo::ValueKind::String ||
        it->second.stringSource != LocalInfo::StringSource::TableIndex) {
      return false;
    }
    if (it->second.stringIndex < 0) {
      error = "native backend missing string table data for: " + expr.name;
      return false;
    }
    if (static_cast<size_t>(it->second.stringIndex) >= stringTable.size()) {
      error = "native backend encountered invalid string table index";
      return false;
    }
    stringIndexOut = it->second.stringIndex;
    lengthOut = stringTable[static_cast<size_t>(stringIndexOut)].size();
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
