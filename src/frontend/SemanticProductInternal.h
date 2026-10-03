#pragma once

// Helpers shared by the SemanticProduct*.cpp units (split out of
// SemanticProduct.cpp without changes, ticket).
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/CompileArena.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>

namespace primec {
namespace semantic_product_detail {

inline std::string quoteSemanticString(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  out.push_back('"');
  return out;
}

inline std::string formatSemanticBool(bool value) {
  return value ? "true" : "false";
}

inline std::string formatSemanticStringList(const std::vector<std::string> &values) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << quoteSemanticString(values[i]);
  }
  out << "]";
  return out.str();
}

inline std::string formatSemanticTemplateParameterList(const std::vector<std::string> &values,
                                                const std::vector<bool> &isPack) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    std::string rendered = values[i];
    if (i < isPack.size() && isPack[i]) {
      rendered += "...";
    }
    out << quoteSemanticString(rendered);
  }
  out << "]";
  return out.str();
}

inline std::string formatSemanticTemplatePackBindingList(
    const std::vector<TemplatePackBinding> &values) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << values[i].parameterName << "="
        << formatSemanticStringList(values[i].arguments);
  }
  out << "]";
  return out.str();
}

inline std::string formatSemanticStdlibSurfaceId(StdlibSurfaceId id) {
  if (const auto *metadata = findStdlibSurfaceMetadata(id); metadata != nullptr) {
    return std::string(metadata->bridgeKey);
  }
  return std::to_string(static_cast<int>(id));
}

inline std::string formatSemanticSourceLocation(int line, int column);

inline std::string formatSemanticRequirementOperandList(
    const SemanticProgram &semanticProgram,
    const std::vector<SemanticProgramRequirementPredicateOperand> &operands) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < operands.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    const auto &operand = operands[i];
    const std::string_view kind =
        semanticProgramResolveCallTargetString(semanticProgram, operand.kindId);
    const std::string_view text =
        semanticProgramResolveCallTargetString(semanticProgram, operand.textId);
    const std::string_view stableHandle =
        semanticProgramResolveCallTargetString(semanticProgram, operand.stableHandleId);
    out << "{kind=" << quoteSemanticString(kind.empty() ? operand.kind : kind)
        << " text=" << quoteSemanticString(text.empty() ? operand.text : text)
        << " stable_handle="
        << quoteSemanticString(stableHandle.empty() ? operand.stableHandle : stableHandle)
        << " source="
        << quoteSemanticString(formatSemanticSourceLocation(operand.sourceLine,
                                                            operand.sourceColumn))
        << "}";
  }
  out << "]";
  return out.str();
}

inline void appendSemanticHeaderLine(std::ostringstream &out, std::string_view label, const std::string &value) {
  out << "  " << label << ": " << value << "\n";
}

inline void appendSemanticIndexedLine(std::ostringstream &out,
                               std::string_view label,
                               size_t index,
                               const std::string &value) {
  out << "  " << label << "[" << index << "]: " << value << "\n";
}

inline std::string formatSemanticSourceLocation(int line, int column) {
  return std::to_string(line) + ":" + std::to_string(column);
}

template <typename EntryT>
inline const EntryT *lookupPublishedSemanticEntryByIndex(const std::vector<EntryT> &entries, std::size_t entryIndex) {
  if (entryIndex >= entries.size()) {
    return nullptr;
  }
  return &entries[entryIndex];
}

inline uint64_t makeLocalAutoInitPathBindingNameKey(SymbolId initializerPathId, SymbolId bindingNameId) {
  return (static_cast<uint64_t>(initializerPathId) << 32) |
         static_cast<uint64_t>(bindingNameId);
}

inline uint64_t makeQueryFactResolvedPathCallNameKey(SymbolId resolvedPathId, SymbolId callNameId) {
  return (static_cast<uint64_t>(resolvedPathId) << 32) |
         static_cast<uint64_t>(callNameId);
}

inline uint64_t makeTryFactOperandPathSourceKey(SymbolId operandPathId, int sourceLine, int sourceColumn) {
  const uint64_t lineBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceLine > 0 ? sourceLine : 0));
  const uint64_t columnBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceColumn > 0 ? sourceColumn : 0));
  return (static_cast<uint64_t>(operandPathId) << 32) ^
         (lineBits * 1315423911ULL) ^
         columnBits;
}

inline uint64_t makeSumVariantMetadataSumPathVariantNameKey(SymbolId sumPathId, SymbolId variantNameId) {
  return (static_cast<uint64_t>(sumPathId) << 32) |
         static_cast<uint64_t>(variantNameId);
}

} // namespace semantic_product_detail

// Defined in SemanticProductLookups.cpp; also used by the formatter.
std::string formatSemanticStringListFromIds(const SemanticProgram &semanticProgram,
                                            const std::vector<SymbolId> &ids,
                                            const std::vector<std::string> &fallbackValues);

} // namespace primec
