#pragma once

#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include <vector>

namespace primec::ir_lowerer {

inline const SemanticProgramBindingFact *
findInferenceSemanticBindingFactForExpr(const Expr &expr,
                                        const SemanticProgram *semanticProgram,
                                        const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return nullptr;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr)) {
      return bindingFact;
    }
  }
  if (expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return nullptr;
  }
  for (const SemanticProgramBindingFact &candidate : semanticProgram->bindingFacts) {
    if (candidate.name == expr.name && candidate.sourceLine == expr.sourceLine &&
        candidate.sourceColumn == expr.sourceColumn) {
      return &candidate;
    }
  }
  return nullptr;
}

inline bool isInferenceFileErrorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "FileError" || normalized == "std/file/FileError";
}

inline bool isInferenceFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = trimTemplateTypeText(base);
  if (!base.empty() && base.front() == '/') {
    base.erase(base.begin());
  }
  return base == "File" || base == "std/file/File";
}

inline std::string resolveInferenceSemanticFactTypeText(const SemanticProgram &semanticProgram,
                                                        const std::string &typeText,
                                                        SymbolId typeTextId) {
  if (typeTextId != InvalidSymbolId) {
    std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

} // namespace primec::ir_lowerer
