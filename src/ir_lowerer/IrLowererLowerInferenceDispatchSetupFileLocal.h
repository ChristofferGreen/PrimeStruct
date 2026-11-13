#pragma once

// Helpers shared by the IrLowererLowerInferenceDispatchSetup*.cpp units (split out of
// IrLowererLowerInferenceDispatchSetup.cpp without changes).
#include "primec/ir_lowerer/IrLowererLowerInferenceSetup.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererLowerInferenceSharedHelpers.h"
#include <vector>

namespace primec::ir_lowerer {

namespace ir_lowerer_lower_inference_dispatch_setup_file_local {


inline LocalInfo::ValueKind inferDispatchSetupNameValueKindFromTypeText(
    const std::string &typeText) {
  std::string normalizedType = trimTemplateTypeText(typeText);
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  if (parseResultTypeName(
          normalizedType, resultHasValue, resultValueKind, resultErrorType)) {
    return resultHasValue ? LocalInfo::ValueKind::Int64
                          : LocalInfo::ValueKind::Int32;
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") &&
        splitTemplateArgs(argText, args) && args.size() == 1) {
      return valueKindFromTypeName(trimTemplateTypeText(args.front()));
    }
    if (base == "Result") {
      return splitTemplateArgs(argText, args) && args.size() == 1
                 ? LocalInfo::ValueKind::Int32
                 : LocalInfo::ValueKind::Int64;
    }
    if (base == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    return valueKindFromTypeName(base);
  }
  return valueKindFromTypeName(normalizeCollectionBindingTypeName(normalizedType));
}

inline bool isDispatchSetupResultTypeMethodCall(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
         expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result" &&
         (expr.name == "ok" || expr.name == "error" || expr.name == "why");
}

inline bool hasSemanticProductDispatchResultMethodFactContext(const Expr &expr,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex) {
  if (semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0) {
    return true;
  }
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.name == "ok" &&
         expr.args.size() > 1 && expr.args[1].semanticNodeId != 0;
}



inline const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataForDispatchSetup() {
  return keyValueHelperSurfaceMetadata();
}

inline std::string canonicalKeyValueHelperPathForDispatchSetup(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataForDispatchSetup();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

inline bool isDispatchSetupKeyValueFamilyText(const std::string &familyText) {
  std::string normalized = trimTemplateTypeText(familyText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return isBuiltinCollectionTypeName(normalized, "map") ||
         isExperimentalCollectionTypeName(normalized, "map", "Map");
}

inline bool inferDispatchSetupKeyValueKindsFromTypeText(const std::string &typeText,
                                                 LocalInfo::ValueKind &keyKindOut,
                                                 LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText) ||
      !isDispatchSetupKeyValueFamilyText(base)) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 2) {
    return false;
  }
  keyKindOut = valueKindFromTypeName(trimTemplateTypeText(args.front()));
  valueKindOut = valueKindFromTypeName(trimTemplateTypeText(args.back()));
  return keyKindOut != LocalInfo::ValueKind::Unknown &&
         valueKindOut != LocalInfo::ValueKind::Unknown;
}

inline bool inferDispatchSetupSemanticKeyValueReceiverKind(const Expr &receiver,
                                                    bool containsResult,
                                                    const SemanticProgram *semanticProgram,
                                                    const SemanticProductIndex *semanticIndex,
                                                    LocalInfo::ValueKind &kindOut,
                                                    bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      receiver.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, receiver)) {
    hasSemanticReceiverOut = true;
    const std::string familyText = resolveInferenceSemanticFactTypeText(
        *semanticProgram, collectionFact->collectionFamily, collectionFact->collectionFamilyId);
    if (!isDispatchSetupKeyValueFamilyText(familyText)) {
      return false;
    }
    const LocalInfo::ValueKind keyKind = valueKindFromTypeName(resolveInferenceSemanticFactTypeText(
        *semanticProgram, collectionFact->keyTypeText, collectionFact->keyTypeTextId));
    const LocalInfo::ValueKind valueKind =
        valueKindFromTypeName(resolveInferenceSemanticFactTypeText(
            *semanticProgram, collectionFact->valueTypeText, collectionFact->valueTypeTextId));
    if (keyKind == LocalInfo::ValueKind::Unknown ||
        valueKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    kindOut = containsResult ? LocalInfo::ValueKind::Bool : valueKind;
    return true;
  }
  std::string receiverType;
  if (!resolveInferenceSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  if (!inferDispatchSetupKeyValueKindsFromTypeText(receiverType, keyKind, valueKind)) {
    return false;
  }
  kindOut = containsResult ? LocalInfo::ValueKind::Bool : valueKind;
  return true;
}


inline bool inferDispatchSetupSemanticBindingFactKind(const Expr &expr,
                                               const SemanticProgram *semanticProgram,
                                               const SemanticProductIndex *semanticIndex,
                                               LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  const auto *bindingFact =
      findInferenceSemanticBindingFactForExpr(expr, semanticProgram, semanticIndex);
  if (bindingFact == nullptr || semanticProgram == nullptr) {
    return false;
  }
  kindOut = valueKindFromTypeName(resolveInferenceSemanticFactTypeText(
      *semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId));
  return kindOut != LocalInfo::ValueKind::Unknown;
}

inline bool inferDispatchSetupSemanticNameKind(const Expr &expr,
                                        const SemanticProgram *semanticProgram,
                                        const SemanticProductIndex *semanticIndex,
                                        LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  std::string typeText;
  if (!resolveInferenceSemanticReceiverTypeText(expr, semanticProgram, semanticIndex, typeText)) {
    return false;
  }
  kindOut = inferDispatchSetupNameValueKindFromTypeText(typeText);
  return true;
}

} // namespace ir_lowerer_lower_inference_dispatch_setup_file_local
} // namespace primec::ir_lowerer
