// soa-surface-audit: exempt
#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <vector>

namespace primec::ir_lowerer {

namespace {

LocalInfo::ValueKind semanticElementKindFromCollectionTypeText(
    const std::string &typeText,
    bool bufferOnly) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return LocalInfo::ValueKind::Unknown;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  std::vector<std::string> args;
  if ((base == "Reference" || base == "Pointer") &&
      splitTemplateArgs(argText, args) && args.size() == 1) {
    return semanticElementKindFromCollectionTypeText(args.front(), bufferOnly);
  }
  if (base == "Buffer") {
    return valueKindFromTypeName(trimTemplateTypeText(argText));
  }
  if (bufferOnly) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (base == "array" || base == "vector" || base == "soa_vector") {
    return valueKindFromTypeName(trimTemplateTypeText(argText));
  }
  if (base == "map" && splitTemplateArgs(argText, args) && args.size() == 2) {
    return valueKindFromTypeName(trimTemplateTypeText(args.back()));
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind semanticElementKindFromCollectionFact(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &fact,
    bool bufferOnly) {
  const std::string family = normalizeCollectionBindingTypeName(
      resolveSemanticProductTypeText(
          semanticProgram, fact.collectionFamily, fact.collectionFamilyId));
  if (family == "Buffer") {
    return valueKindFromTypeName(resolveSemanticProductTypeText(
        semanticProgram, fact.elementTypeText, fact.elementTypeTextId));
  }
  if (bufferOnly) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (family == "array" || family == "vector" || family == "soa_vector") {
    return valueKindFromTypeName(resolveSemanticProductTypeText(
        semanticProgram, fact.elementTypeText, fact.elementTypeTextId));
  }
  if (family == "map") {
    return valueKindFromTypeName(resolveSemanticProductTypeText(
        semanticProgram, fact.valueTypeText, fact.valueTypeTextId));
  }
  return LocalInfo::ValueKind::Unknown;
}

bool inferSemanticElementKindForName(
    const Expr &expr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool bufferOnly,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Name || semanticProgram == nullptr ||
      semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  if (const SemanticProgramCollectionSpecialization *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, expr);
      collectionFact != nullptr) {
    kindOut = semanticElementKindFromCollectionFact(
        semanticProgram, *collectionFact, bufferOnly);
    return true;
  }
  if (const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, expr);
      bindingFact != nullptr) {
    kindOut = semanticElementKindFromCollectionTypeText(
        resolveSemanticProductTypeText(
            semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId),
        bufferOnly);
    return true;
  }
  if (const SemanticProgramLocalAutoFact *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, expr);
      localAutoFact != nullptr) {
    kindOut = semanticElementKindFromCollectionTypeText(
        resolveSemanticProductTypeText(
            semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId),
        bufferOnly);
    return true;
  }
  if (const SemanticProgramQueryFact *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
      queryFact != nullptr) {
    std::string typeText = resolveSemanticProductTypeText(
        semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (typeText.empty()) {
      typeText = resolveSemanticProductTypeText(
          semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
    }
    kindOut = semanticElementKindFromCollectionTypeText(typeText, bufferOnly);
    return true;
  }
  return false;
}

} // namespace

bool runLowerInferenceArrayKindSetup(const LowerInferenceArrayKindSetupInput &input,
                                     LowerInferenceSetupBootstrapState &stateInOut,
                                     std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference array-kind setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference array-kind setup dependency: resolveExprPath";
    return false;
  }
  if (!input.resolveStructArrayInfoFromPath) {
    errorOut = "native backend missing inference array-kind setup dependency: resolveStructArrayInfoFromPath";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference array-kind setup dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing inference array-kind setup dependency: isStringCountCall";
    return false;
  }
  if (!stateInOut.resolveMethodCallDefinition) {
    errorOut = "native backend missing inference array-kind setup state: resolveMethodCallDefinition";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  const auto resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isStringCountCall = input.isStringCountCall;
  const auto *semanticProgram = stateInOut.semanticProgram;
  const auto *semanticIndex = stateInOut.semanticIndex;
  const auto resolveMethodCallDefinitionNoProbeError =
      [&stateInOut, &errorOut](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
    const std::string priorError = errorOut;
    const Definition *resolved = stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
    errorOut = priorError;
    return resolved;
  };
  auto isZeroFieldStructDef = [](const Definition &def) -> bool {
    if (!isStructDefinition(def)) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
      bool isStaticField = false;
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          isStaticField = true;
          break;
        }
      }
      if (!isStaticField) {
        return false;
      }
    }
    return true;
  };

  stateInOut.inferBufferElementKind = [&stateInOut](const Expr &expr,
                                                    const LocalMap &localsIn) -> LocalInfo::ValueKind {
    LocalInfo::ValueKind semanticKind = LocalInfo::ValueKind::Unknown;
    if (inferSemanticElementKindForName(
            expr, stateInOut.semanticProgram, stateInOut.semanticIndex, true, semanticKind)) {
      return semanticKind;
    }
    return inferBufferElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return stateInOut.inferArrayElementKind(candidate, candidateLocals);
        });
  };

  stateInOut.inferArrayElementKind = [defMap,
                                      resolveExprPath,
                                      resolveStructArrayInfoFromPath,
                                      isArrayCountCall,
                                      isStringCountCall,
                                      isZeroFieldStructDef,
                                      resolveMethodCallDefinitionNoProbeError,
                                      semanticProgram,
                                      semanticIndex,
                                      &stateInOut](const Expr &expr,
                                                   const LocalMap &localsIn) -> LocalInfo::ValueKind {
    LocalInfo::ValueKind semanticKind = LocalInfo::ValueKind::Unknown;
    if (inferSemanticElementKindForName(
            expr, semanticProgram, semanticIndex, false, semanticKind)) {
      return semanticKind;
    }
    return inferArrayElementValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, const LocalMap &candidateLocals) {
          return stateInOut.inferBufferElementKind(candidate, candidateLocals);
        },
        [&](const Expr &candidate) { return resolveExprPath(candidate); },
        [&](const std::string &structPath, LocalInfo::ValueKind &kindOut) {
          StructArrayTypeInfo structInfo;
          if (resolveStructArrayInfoFromPath(structPath, structInfo)) {
            kindOut = structInfo.elementKind;
            return true;
          }
          auto defIt = defMap->find(structPath);
          if (defIt != defMap->end() && defIt->second != nullptr && isZeroFieldStructDef(*defIt->second)) {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
          return false;
        },
        [&](const Expr &candidate, const LocalMap &, LocalInfo::ValueKind &kindOut) {
          return resolveDefinitionCallReturnKind(
              candidate, *defMap, resolveExprPath, stateInOut.getReturnInfo, true, kindOut);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveCountMethodCallReturnKind(candidate,
                                                  candidateLocals,
                                                  isArrayCountCall,
                                                  isStringCountCall,
                                                  resolveMethodCallDefinitionNoProbeError,
                                                  stateInOut.getReturnInfo,
                                                  true,
                                                  kindOut,
                                                  nullptr,
                                                  stateInOut.inferExprKind,
                                                  stateInOut.semanticProgram,
                                                  stateInOut.semanticIndex);
        },
        [&](const Expr &candidate, const LocalMap &candidateLocals, LocalInfo::ValueKind &kindOut) {
          return resolveMethodCallReturnKind(
              candidate,
              candidateLocals,
              resolveMethodCallDefinitionNoProbeError,
              [&](const Expr &callExpr) {
                return ir_lowerer::resolveDefinitionCall(callExpr, *defMap, resolveExprPath);
              },
              stateInOut.getReturnInfo,
              true,
              kindOut,
              nullptr,
              stateInOut.semanticProgram,
              stateInOut.semanticIndex);
        });
  };
  return true;
}

} // namespace primec::ir_lowerer
