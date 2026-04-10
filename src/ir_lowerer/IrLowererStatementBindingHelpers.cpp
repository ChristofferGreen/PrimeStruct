#include "IrLowererStatementBindingHelpers.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "IrLowererStatementBindingInternal.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isSpecializedExperimentalMapTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

bool isSpecializedExperimentalVectorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool resolveSpecializedExperimentalVectorElementKind(const std::string &typeText,
                                                     const std::function<const Definition *(const Expr &)>
                                                         &resolveDefinitionCall,
                                                     LocalInfo::ValueKind &elemKindOut) {
  elemKindOut = LocalInfo::ValueKind::Unknown;
  if (!isSpecializedExperimentalVectorTypeText(typeText) || !resolveDefinitionCall) {
    return false;
  }
  Expr syntheticExpr;
  syntheticExpr.kind = Expr::Kind::Call;
  syntheticExpr.name = trimTemplateTypeText(typeText);
  if (!syntheticExpr.name.empty() && syntheticExpr.name.front() != '/') {
    syntheticExpr.name.insert(syntheticExpr.name.begin(), '/');
  }
  const Definition *structDef = resolveDefinitionCall(syntheticExpr);
  if (structDef == nullptr || !isStructDefinition(*structDef)) {
    return false;
  }
  for (const auto &fieldExpr : structDef->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    std::string typeName;
    std::vector<std::string> templateArgs;
    if (!extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
        normalizeCollectionBindingTypeName(typeName) != "Pointer" || templateArgs.size() != 1) {
      continue;
    }
    std::string elementType = trimTemplateTypeText(templateArgs.front());
    if (!extractTopLevelUninitializedTypeText(elementType, elementType)) {
      continue;
    }
    elemKindOut = valueKindFromTypeName(elementType);
    return elemKindOut != LocalInfo::ValueKind::Unknown;
  }
  return false;
}

bool resolveSpecializedExperimentalMapTypeKinds(const std::string &typeText,
                                                const std::function<const Definition *(const Expr &)>
                                                    &resolveDefinitionCall,
                                                LocalInfo::ValueKind &keyKindOut,
                                                LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  if (!isSpecializedExperimentalMapTypeText(typeText) || !resolveDefinitionCall) {
    return false;
  }
  Expr syntheticExpr;
  syntheticExpr.kind = Expr::Kind::Call;
  syntheticExpr.name = trimTemplateTypeText(typeText);
  if (!syntheticExpr.name.empty() && syntheticExpr.name.front() != '/') {
    syntheticExpr.name.insert(syntheticExpr.name.begin(), '/');
  }
  const Definition *structDef = resolveDefinitionCall(syntheticExpr);
  if (structDef == nullptr || !isStructDefinition(*structDef)) {
    return false;
  }
  for (const auto &fieldExpr : structDef->statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    std::string typeName;
    std::vector<std::string> templateArgs;
    if (!extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
        normalizeCollectionBindingTypeName(typeName) != "vector") {
      continue;
    }
    LocalInfo::ValueKind fieldKind = LocalInfo::ValueKind::Unknown;
    if (templateArgs.size() == 1) {
      fieldKind = valueKindFromTypeName(trimTemplateTypeText(templateArgs.front()));
    } else if (!resolveSpecializedExperimentalVectorElementKind(typeName, resolveDefinitionCall, fieldKind)) {
      continue;
    }
    if (fieldKind == LocalInfo::ValueKind::Unknown) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyKindOut = fieldKind;
    } else if (fieldExpr.name == "payloads") {
      valueKindOut = fieldKind;
    }
  }
  return keyKindOut != LocalInfo::ValueKind::Unknown &&
         valueKindOut != LocalInfo::ValueKind::Unknown;
}

bool resolveSpecializedExperimentalMapStructPathFromTypeText(const std::string &typeText,
                                                             std::string &structPathOut) {
  structPathOut.clear();
  std::string normalizedType = trimTemplateTypeText(typeText);
  if (!normalizedType.empty() && normalizedType.front() != '/') {
    normalizedType.insert(normalizedType.begin(), '/');
  }
  if (normalizedType.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    structPathOut = normalizedType;
    return true;
  }

  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(typeText, base, argList) ||
      normalizeCollectionBindingTypeName(base) != "map" ||
      argList.empty()) {
    return false;
  }

  std::vector<std::string> templateArgs;
  if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
    return false;
  }

  std::string canonicalArgs = joinTemplateArgsText(templateArgs);
  canonicalArgs.erase(
      std::remove_if(canonicalArgs.begin(),
                     canonicalArgs.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
      canonicalArgs.end());

  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  structPathOut = specializedPath.str();
  return true;
}

bool isIfBlockEnvelopeForBindingTypeInfo(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
    return false;
  }
  return candidate.hasBodyArguments || !candidate.bodyArguments.empty();
}

const Expr *findIfBranchValueExprForBindingTypeInfo(const Expr &candidate) {
  if (!isIfBlockEnvelopeForBindingTypeInfo(candidate)) {
    return &candidate;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &bodyExpr : candidate.bodyArguments) {
    if (bodyExpr.isBinding) {
      continue;
    }
    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return nullptr;
      }
      return &bodyExpr.args.front();
    }
    valueExpr = &bodyExpr;
  }
  return valueExpr;
}

bool populateBindingTypeInfoFromTypeText(
    const std::string &typeText,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    StatementBindingTypeInfo &infoOut) {
  const std::string normalizedTypeText = trimTemplateTypeText(typeText);
  if (normalizedTypeText.empty()) {
    return false;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedTypeText, base, argText)) {
    const LocalInfo::ValueKind scalarKind =
        valueKindFromTypeName(normalizeCollectionBindingTypeName(normalizedTypeText));
    if (scalarKind != LocalInfo::ValueKind::Unknown) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = scalarKind;
      infoOut.structTypeName.clear();
      return true;
    }
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Unknown;
    infoOut.structTypeName = normalizedTypeText;
    return true;
  }

  const std::string normalizedBase =
      normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "soa_vector") {
    infoOut.kind = normalizedBase == "array" ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
    const std::string elementType = trimTemplateTypeText(argText);
    infoOut.valueKind = valueKindFromTypeName(elementType);
    infoOut.structTypeName =
        infoOut.valueKind == LocalInfo::ValueKind::Unknown ? elementType : std::string{};
    return true;
  }
  if (normalizedBase == "map") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 2) {
      return false;
    }
    infoOut.kind = LocalInfo::Kind::Map;
    infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
    infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
    infoOut.valueKind = infoOut.mapValueKind;
    resolveSpecializedExperimentalMapStructPathFromTypeText(normalizedTypeText, infoOut.structTypeName);
    return true;
  }
  if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
    infoOut.kind =
        normalizedBase == "Pointer" ? LocalInfo::Kind::Pointer : LocalInfo::Kind::Reference;
    const std::string targetType =
        unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(argText));
    infoOut.valueKind = valueKindFromTypeName(targetType);
    infoOut.structTypeName =
        infoOut.valueKind == LocalInfo::ValueKind::Unknown ? targetType : std::string{};
    return true;
  }
  if (normalizedBase == "Buffer") {
    infoOut.kind = LocalInfo::Kind::Buffer;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(argText));
    return true;
  }
  if (normalizedBase == "Result") {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    return true;
  }
  if (resolveDefinitionCall) {
    LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
    if (resolveSpecializedExperimentalVectorElementKind(normalizedTypeText, resolveDefinitionCall, elemKind)) {
      infoOut.kind = LocalInfo::Kind::Vector;
      infoOut.valueKind = elemKind;
      infoOut.structTypeName.clear();
      return true;
    }
    LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
    if (resolveSpecializedExperimentalMapTypeKinds(normalizedTypeText, resolveDefinitionCall, keyKind, valueKind)) {
      infoOut.kind = LocalInfo::Kind::Map;
      infoOut.mapKeyKind = keyKind;
      infoOut.mapValueKind = valueKind;
      infoOut.valueKind = valueKind;
      infoOut.structTypeName = normalizedTypeText;
      return true;
    }
  }

  infoOut.kind = LocalInfo::Kind::Value;
  infoOut.valueKind = LocalInfo::ValueKind::Unknown;
  infoOut.structTypeName = normalizedTypeText;
  return true;
}

bool populateBindingTypeInfoFromSemanticBindingFact(
    const Expr &expr,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets,
    StatementBindingTypeInfo &infoOut) {
  if (semanticProductTargets == nullptr || !semanticProductTargets->hasSemanticProduct ||
      expr.semanticNodeId == 0) {
    return false;
  }
  const auto *bindingFact = findSemanticProductBindingFact(*semanticProductTargets, expr);
  return bindingFact != nullptr && !bindingFact->bindingTypeText.empty() &&
         populateBindingTypeInfoFromTypeText(
             bindingFact->bindingTypeText, resolveDefinitionCall, infoOut);
}

bool inferExprBindingTypeInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const InferBindingExprKindFn &inferExprKind,
                              const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
                              const SemanticProductTargetAdapter *semanticProductTargets,
                              StatementBindingTypeInfo &infoOut) {
  infoOut = {};
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return false;
    }
    infoOut.kind = it->second.kind;
    infoOut.valueKind = it->second.valueKind;
    infoOut.mapKeyKind = it->second.mapKeyKind;
    infoOut.mapValueKind = it->second.mapValueKind;
    infoOut.structTypeName = it->second.structTypeName;
    return true;
  }
  if (expr.kind == Expr::Kind::Literal) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = expr.isUnsigned ? LocalInfo::ValueKind::UInt64
                                        : (expr.intWidth == 64 ? LocalInfo::ValueKind::Int64
                                                               : LocalInfo::ValueKind::Int32);
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Bool;
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind =
        expr.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::String;
    return true;
  }
  if (populateBindingTypeInfoFromSemanticBindingFact(
          expr, resolveDefinitionCall, semanticProductTargets, infoOut)) {
    return true;
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    const Expr *thenValue = findIfBranchValueExprForBindingTypeInfo(expr.args[1]);
    const Expr *elseValue = findIfBranchValueExprForBindingTypeInfo(expr.args[2]);
    if (thenValue == nullptr || elseValue == nullptr) {
      return false;
    }
    StatementBindingTypeInfo thenInfo;
    StatementBindingTypeInfo elseInfo;
    if (!inferExprBindingTypeInfo(
            *thenValue, localsIn, inferExprKind, resolveDefinitionCall, semanticProductTargets, thenInfo) ||
        !inferExprBindingTypeInfo(
            *elseValue, localsIn, inferExprKind, resolveDefinitionCall, semanticProductTargets, elseInfo)) {
      return false;
    }
    if (thenInfo.kind != elseInfo.kind) {
      return false;
    }
    if (thenInfo.kind == LocalInfo::Kind::Map) {
      if (thenInfo.mapKeyKind != elseInfo.mapKeyKind || thenInfo.mapValueKind != elseInfo.mapValueKind) {
        return false;
      }
    } else if (thenInfo.kind == LocalInfo::Kind::Value) {
      if (!thenInfo.structTypeName.empty() || !elseInfo.structTypeName.empty()) {
        if (thenInfo.structTypeName.empty() || thenInfo.structTypeName != elseInfo.structTypeName) {
          return false;
        }
      } else if (thenInfo.valueKind == LocalInfo::ValueKind::Unknown ||
                 thenInfo.valueKind != elseInfo.valueKind) {
        return false;
      }
    } else if (thenInfo.valueKind != elseInfo.valueKind ||
               thenInfo.structTypeName != elseInfo.structTypeName) {
      return false;
    }
    infoOut = thenInfo;
    return true;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  std::string collection;
  if (getBuiltinCollectionName(expr, collection)) {
    if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
        expr.templateArgs.size() == 1) {
      infoOut.kind = collection == "array" ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
      const std::string elementType = trimTemplateTypeText(expr.templateArgs.front());
      infoOut.valueKind = valueKindFromTypeName(elementType);
      infoOut.structTypeName =
          infoOut.valueKind == LocalInfo::ValueKind::Unknown ? elementType : std::string{};
      return true;
    }
    if (collection == "map" && expr.templateArgs.size() == 2) {
      infoOut.kind = LocalInfo::Kind::Map;
      infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs[0]));
      infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs[1]));
      infoOut.valueKind = infoOut.mapValueKind;
      return true;
    }
  }

  if (resolveDefinitionCall) {
    if (const Definition *callee = resolveDefinitionCall(expr); callee != nullptr) {
      for (const auto &transform : callee->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1 ||
            transform.templateArgs.front() == "auto") {
          continue;
        }
        return populateBindingTypeInfoFromTypeText(
            transform.templateArgs.front(), resolveDefinitionCall, infoOut);
      }
    }
  }

  const ResolveMethodCallWithLocalsFn noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const LookupReturnInfoFn noopLookupReturnInfo =
      [](const std::string &, ReturnInfo &) { return false; };
  ResultExprInfo resultInfo;
  if (resolveResultExprInfoFromLocals(expr,
                                      localsIn,
                                      noopResolveMethodCall,
                                      resolveDefinitionCall,
                                      noopLookupReturnInfo,
                                      inferExprKind,
                                      resultInfo,
                                      semanticProductTargets) &&
      resultInfo.isResult) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind =
        resultInfo.hasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    infoOut.structTypeName.clear();
    return true;
  }

  const LocalInfo::ValueKind scalarKind = inferExprKind(expr, localsIn);
  if (scalarKind == LocalInfo::ValueKind::Unknown) {
    return false;
  }
  infoOut.kind = LocalInfo::Kind::Value;
  infoOut.valueKind = scalarKind;
  return true;
}

} // namespace

bool resolveSpecializedExperimentalMapTypeKindsForBindingType(
    const std::string &typeText,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    LocalInfo::ValueKind &keyKindOut,
    LocalInfo::ValueKind &valueKindOut) {
  return resolveSpecializedExperimentalMapTypeKinds(
      typeText, resolveDefinitionCall, keyKindOut, valueKindOut);
}

bool resolveSpecializedExperimentalMapStructPathForBindingType(
    const std::string &typeText,
    std::string &structPathOut) {
  return resolveSpecializedExperimentalMapStructPathFromTypeText(typeText, structPathOut);
}

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind,
                                                       const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
                                                       const SemanticProductTargetAdapter *semanticProductTargets) {
  const ResolveDefinitionCallForStatementFn safeResolveDefinitionCall =
      resolveDefinitionCall ? resolveDefinitionCall
                            : ResolveDefinitionCallForStatementFn([](const Expr &) { return nullptr; });
  StatementBindingTypeInfo info;
  info.kind = bindingKind(stmt);
  const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  auto isRawStructBufferInit = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isBinding) {
      return false;
    }
    return candidate.name == "Buffer" || candidate.name == "/std/gfx/Buffer" ||
           candidate.name == "/std/gfx/experimental/Buffer" ||
           candidate.name.rfind("/std/gfx/Buffer__t", 0) == 0 ||
           candidate.name.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
  };
  if (hasExplicitType && info.kind == LocalInfo::Kind::Buffer && isRawStructBufferInit(init)) {
    info.kind = LocalInfo::Kind::Value;
  }
  if (!hasExplicitType) {
    StatementBindingTypeInfo semanticInfo;
    if (populateBindingTypeInfoFromSemanticBindingFact(
            stmt, safeResolveDefinitionCall, semanticProductTargets, semanticInfo)) {
      return semanticInfo;
    }
  }
  LocalInfo::ValueKind inferredInitValueKind = LocalInfo::ValueKind::Unknown;
  if (!hasExplicitType && info.kind == LocalInfo::Kind::Value) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end()) {
        info.kind = it->second.kind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      inferredInitValueKind = inferExprKind(init, localsIn);
      if (isPointerMemoryIntrinsicCall(init)) {
        info.kind = LocalInfo::Kind::Pointer;
      } else if (inferredInitValueKind == LocalInfo::ValueKind::Unknown) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection)) {
          if (collection == "array") {
            info.kind = LocalInfo::Kind::Array;
          } else if (collection == "vector") {
            info.kind = LocalInfo::Kind::Vector;
          } else if (collection == "map") {
            info.kind = LocalInfo::Kind::Map;
          }
        }
      }
    }
  }

  if (!hasExplicitType) {
    StatementBindingTypeInfo inferredExprInfo;
    if (inferExprBindingTypeInfo(
            init, localsIn, inferExprKind, safeResolveDefinitionCall, semanticProductTargets, inferredExprInfo)) {
      if (info.kind == LocalInfo::Kind::Value) {
        info.kind = inferredExprInfo.kind;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = inferredExprInfo.valueKind;
      }
      if (info.mapKeyKind == LocalInfo::ValueKind::Unknown) {
        info.mapKeyKind = inferredExprInfo.mapKeyKind;
      }
      if (info.mapValueKind == LocalInfo::ValueKind::Unknown) {
        info.mapValueKind = inferredExprInfo.mapValueKind;
      }
      if (info.structTypeName.empty()) {
        info.structTypeName = inferredExprInfo.structTypeName;
      }
    }
  }

  if (info.kind == LocalInfo::Kind::Map) {
    if (hasExplicitType) {
      for (const auto &transform : stmt.transforms) {
        const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
        if (normalizedName == "map" && transform.templateArgs.size() == 2) {
          info.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
          info.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
          if (info.structTypeName.empty()) {
            std::string declaredType = transform.name + "<" +
                                       trimTemplateTypeText(transform.templateArgs[0]) + ", " +
                                       trimTemplateTypeText(transform.templateArgs[1]) + ">";
            resolveSpecializedExperimentalMapStructPathFromTypeText(
                declaredType, info.structTypeName);
          }
          break;
        }
        if (normalizedName == "map" && transform.templateArgs.empty() &&
            resolveSpecializedExperimentalMapTypeKinds(
                transform.name, safeResolveDefinitionCall, info.mapKeyKind, info.mapValueKind)) {
          if (info.structTypeName.empty()) {
            resolveSpecializedExperimentalMapStructPathFromTypeText(
                transform.name, info.structTypeName);
          }
          break;
        }
      }
    } else if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
        info.mapKeyKind = it->second.mapKeyKind;
        info.mapValueKind = it->second.mapValueKind;
        if (info.structTypeName.empty()) {
          info.structTypeName = it->second.structTypeName;
        }
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
        info.mapKeyKind = valueKindFromTypeName(init.templateArgs[0]);
        info.mapValueKind = valueKindFromTypeName(init.templateArgs[1]);
        if (info.structTypeName.empty()) {
          std::string initType = "map<" + trimTemplateTypeText(init.templateArgs[0]) + ", " +
                                 trimTemplateTypeText(init.templateArgs[1]) + ">";
          resolveSpecializedExperimentalMapStructPathFromTypeText(
              initType, info.structTypeName);
        }
      }
    }
    info.valueKind = info.mapValueKind;
    return info;
  }

  if (hasExplicitType) {
    info.valueKind = bindingValueKind(stmt, info.kind);
    return info;
  }

  if (info.kind == LocalInfo::Kind::Value) {
    const LocalInfo::ValueKind specialInitValueKind = inferSpecialCallValueKind(init);
    info.valueKind = (specialInitValueKind != LocalInfo::ValueKind::Unknown)
                         ? specialInitValueKind
                         : ((inferredInitValueKind != LocalInfo::ValueKind::Unknown)
                                ? inferredInitValueKind
                                : inferExprKind(init, localsIn));
    return info;
  }

  if (info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        info.valueKind = it->second.valueKind;
      }
      info.structTypeName = (it != localsIn.end()) ? it->second.structTypeName : "";
    } else if (info.kind == LocalInfo::Kind::Pointer && init.kind == Expr::Kind::Call &&
               isPointerMemoryIntrinsicCall(init)) {
      info.valueKind = inferPointerMemoryIntrinsicValueKind(init, localsIn, inferExprKind);
      info.structTypeName = inferPointerMemoryIntrinsicStructType(init, localsIn);
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
        info.valueKind = it->second.valueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
          init.templateArgs.size() == 1) {
        info.valueKind = valueKindFromTypeName(init.templateArgs.front());
      }
    }
  }

  return info;
}

bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition,
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
                                 const SemanticProductTargetAdapter *semanticProductTargets) {
  infoOut.isMutable = isBindingMutable(param);
  infoOut.isSoaVector = hasSoaVectorTypeTransform(param);
  infoOut.isArgsPack = isArgsPackBinding(param);
  infoOut.kind = bindingKind(param);
  if (hasExplicitBindingTypeTransform(param)) {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value &&
             isPointerMemoryIntrinsicCall(param.args.front())) {
    infoOut.kind = LocalInfo::Kind::Pointer;
    infoOut.valueKind = inferPointerMemoryIntrinsicValueKind(param.args.front(), localsForKindInference, inferExprKind);
    infoOut.structTypeName = inferPointerMemoryIntrinsicStructType(param.args.front(), localsForKindInference);
    infoOut.targetsUninitializedStorage =
        inferPointerMemoryIntrinsicTargetsUninitializedStorage(param.args.front(), localsForKindInference);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value) {
    infoOut.valueKind = inferExprKind(param.args.front(), localsForKindInference);
    if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = LocalInfo::ValueKind::Int32;
    }
    ResultExprInfo inferredResultInfo;
    if (inferCallParameterDefaultResultInfo(
            param.args.front(),
            localsForKindInference,
            inferExprKind,
            resolveMethodCallDefinition,
            resolveDefinitionCall,
            getReturnInfo,
            inferredResultInfo,
            semanticProductTargets) &&
        inferredResultInfo.isResult) {
      infoOut.isResult = true;
      infoOut.resultHasValue = inferredResultInfo.hasValue;
      infoOut.resultValueKind = inferredResultInfo.valueKind;
      infoOut.resultValueCollectionKind = inferredResultInfo.valueCollectionKind;
      infoOut.resultValueMapKeyKind = inferredResultInfo.valueMapKeyKind;
      infoOut.resultValueIsFileHandle = inferredResultInfo.valueIsFileHandle;
      infoOut.resultValueStructType = inferredResultInfo.valueStructType;
      infoOut.resultErrorType = inferredResultInfo.errorType;
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    }
  } else {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  }

  if (infoOut.kind == LocalInfo::Kind::Map) {
    for (const auto &transform : param.transforms) {
      const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
      if (normalizedName == "map" && transform.templateArgs.size() == 2) {
        infoOut.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
        infoOut.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
        infoOut.valueKind = infoOut.mapValueKind;
        if (infoOut.structTypeName.empty()) {
          std::string declaredType = transform.name + "<" +
                                     trimTemplateTypeText(transform.templateArgs[0]) + ", " +
                                     trimTemplateTypeText(transform.templateArgs[1]) + ">";
          std::string specializedStructPath;
          if (resolveSpecializedExperimentalMapStructPathFromTypeText(
                  declaredType, specializedStructPath)) {
            infoOut.structTypeName = std::move(specializedStructPath);
          }
        }
        break;
      }
    }
  }
  for (const auto &transform : param.transforms) {
    if (transform.name == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
    } else if (applyErrorTypeMetadata(transform.name, infoOut)) {
      continue;
    } else if (transform.name == "Result") {
      infoOut.isResult = true;
      infoOut.resultHasValue = (transform.templateArgs.size() == 2);
      infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
      infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      if (infoOut.resultHasValue && !transform.templateArgs.empty()) {
        resolveSupportedResultCollectionType(
            transform.templateArgs.front(),
            infoOut.resultValueCollectionKind,
            infoOut.resultValueKind,
            &infoOut.resultValueMapKeyKind);
        if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value &&
            infoOut.resultValueKind == LocalInfo::ValueKind::Unknown &&
            resolveSpecializedExperimentalMapTypeKinds(
                transform.templateArgs.front(),
                resolveDefinitionCall,
                infoOut.resultValueMapKeyKind,
                infoOut.resultValueKind)) {
          infoOut.resultValueCollectionKind = LocalInfo::Kind::Map;
        } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          infoOut.resultValueKind = valueKindFromTypeName(transform.templateArgs.front());
        }
      }
      infoOut.resultValueIsFileHandle =
          infoOut.resultHasValue && !transform.templateArgs.empty() &&
          isFileHandleTypeText(transform.templateArgs.front());
      if (infoOut.resultValueIsFileHandle) {
        infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
      }
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      if (!transform.templateArgs.empty()) {
        infoOut.resultErrorType = transform.templateArgs.back();
      }
    } else if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
      const std::string originalTargetType = trimTemplateTypeText(transform.templateArgs.front());
      std::string targetType = originalTargetType;
      if (extractTopLevelUninitializedTypeText(originalTargetType, targetType)) {
        infoOut.targetsUninitializedStorage = true;
      }
      std::string wrappedBase;
      std::string wrappedArg;
      if (splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "File") {
        infoOut.isFileHandle = true;
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "array") {
        infoOut.pointerToArray = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "vector") {
        infoOut.pointerToVector = true;
        const std::string elementType = trimTemplateTypeText(wrappedArg);
        infoOut.valueKind = valueKindFromTypeName(elementType);
        if (infoOut.valueKind == LocalInfo::ValueKind::Unknown && infoOut.structTypeName.empty()) {
          infoOut.structTypeName = elementType;
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "soa_vector") {
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        const std::string elementType = trimTemplateTypeText(wrappedArg);
        infoOut.valueKind = valueKindFromTypeName(elementType);
        if (infoOut.valueKind == LocalInfo::ValueKind::Unknown && infoOut.structTypeName.empty()) {
          infoOut.structTypeName = elementType;
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "map") {
        std::vector<std::string> args;
        if (splitTemplateArgs(wrappedArg, args) && args.size() == 2) {
          infoOut.pointerToMap = true;
          infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
          infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
          infoOut.valueKind = infoOut.mapValueKind;
          if (infoOut.structTypeName.empty()) {
            resolveSpecializedExperimentalMapStructPathFromTypeText(targetType, infoOut.structTypeName);
          }
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "Buffer") {
        infoOut.pointerToBuffer = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      applyErrorTypeMetadata(targetType, infoOut);
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
        infoOut.isResult = true;
        infoOut.resultHasValue = resultHasValue;
        std::string resultValueType;
        infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
        infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
        infoOut.resultValueIsFileHandle =
            resultHasValue && extractResultValueTypeText(targetType, resultValueType) &&
            isFileHandleTypeText(resultValueType);
        if (resultHasValue && !resultValueType.empty()) {
          resolveSupportedResultCollectionType(
              resultValueType,
              infoOut.resultValueCollectionKind,
              infoOut.resultValueKind,
              &infoOut.resultValueMapKeyKind);
        }
        if (infoOut.resultValueIsFileHandle) {
          infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
        } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          infoOut.resultValueKind = resultValueKind;
        }
        infoOut.valueKind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = resultErrorType;
      }
    }
  }

  if (infoOut.isArgsPack) {
    std::string elementTypeText;
    if (extractArgsPackElementTypeText(param, elementTypeText)) {
      applyArgsPackElementMetadata(elementTypeText, infoOut);
      applyArgsPackElementStructMetadata(param, elementTypeText, applyStructArrayInfo, applyStructValueInfo, infoOut);
    }
  }

  if (infoOut.errorTypeName == "GfxError" && infoOut.errorHelperNamespacePath.empty() && param.args.size() == 1) {
    const Expr &initExpr = param.args.front();
    const Definition *initDef =
        initExpr.isMethodCall ? (resolveMethodCallDefinition ? resolveMethodCallDefinition(initExpr, localsForKindInference)
                                                             : nullptr)
                              : (resolveDefinitionCall ? resolveDefinitionCall(initExpr) : nullptr);
    if (initDef != nullptr) {
      if (initDef->fullPath.rfind("/std/gfx/experimental/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/experimental/GfxError";
      } else if (initDef->fullPath.rfind("/std/gfx/", 0) == 0 ||
                 initDef->fullPath.rfind("/GfxError/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/GfxError";
      }
    }
  }

  infoOut.isFileError = infoOut.isFileError || isFileErrorBinding(param);
  auto applySpecializedWrappedMapBindingInfo = [&](const Expr &bindingExpr, LocalInfo &bindingInfo) {
    if ((bindingInfo.kind != LocalInfo::Kind::Reference &&
         bindingInfo.kind != LocalInfo::Kind::Pointer) ||
        bindingInfo.referenceToMap || bindingInfo.pointerToMap) {
      return;
    }
    for (const auto &transform : bindingExpr.transforms) {
      if ((bindingInfo.kind == LocalInfo::Kind::Reference && transform.name != "Reference") ||
          (bindingInfo.kind == LocalInfo::Kind::Pointer && transform.name != "Pointer") ||
          transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string targetType = unwrapTopLevelUninitializedTypeText(transform.templateArgs.front());
      LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      if (!resolveSpecializedExperimentalMapTypeKindsForBindingType(
              targetType, resolveDefinitionCall, keyKind, valueKind)) {
        continue;
      }
      if (bindingInfo.kind == LocalInfo::Kind::Reference) {
        bindingInfo.referenceToMap = true;
      } else {
        bindingInfo.pointerToMap = true;
      }
      bindingInfo.mapKeyKind = keyKind;
      bindingInfo.mapValueKind = valueKind;
      bindingInfo.valueKind = valueKind;
      if (bindingInfo.structTypeName.empty()) {
        resolveSpecializedExperimentalMapStructPathForBindingType(
            targetType, bindingInfo.structTypeName);
      }
      return;
    }
  };
  setReferenceArrayInfo(param, infoOut);
  applySpecializedWrappedMapBindingInfo(param, infoOut);
  applyStructArrayInfo(param, infoOut);
  applyStructValueInfo(param, infoOut);
  if ((infoOut.kind == LocalInfo::Kind::Reference || infoOut.kind == LocalInfo::Kind::Pointer) &&
      (infoOut.referenceToVector || infoOut.pointerToVector)) {
    infoOut.structTypeName = infoOut.isSoaVector ? "/soa_vector" : "/vector";
  }
  if (infoOut.kind == LocalInfo::Kind::Value && !infoOut.structTypeName.empty()) {
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
  }
  const bool isUnsupportedStringPointerReferenceArgsPack = [&param]() {
    for (const auto &transform : param.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities" ||
          isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.name != "args" || transform.templateArgs.size() != 1) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg)) {
        return false;
      }
      const std::string normalizedBase = normalizeCollectionBindingTypeName(base);
      return (normalizedBase == "Pointer" || normalizedBase == "Reference") &&
             trimTemplateTypeText(arg) == "string";
    }
    return false;
  }();
  if (isUnsupportedStringPointerReferenceArgsPack) {
    error = "variadic args<T> does not support string pointers or references";
    return false;
  }
  if (!isStringBinding(param)) {
    return true;
  }
  if (infoOut.kind != LocalInfo::Kind::Value && infoOut.kind != LocalInfo::Kind::Map) {
    error = "native backend does not support string pointers or references";
    return false;
  }
  infoOut.valueKind = LocalInfo::ValueKind::String;
  infoOut.stringSource = LocalInfo::StringSource::RuntimeIndex;
  infoOut.stringIndex = -1;
  infoOut.argvChecked = true;
  return true;
}

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error) {
  zeroOp = IrOpcode::PushI32;
  zeroImm = 0;
  if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector || kind == LocalInfo::Kind::Map ||
      kind == LocalInfo::Kind::Buffer) {
    zeroOp = IrOpcode::PushI64;
    return true;
  }

  switch (valueKind) {
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
      zeroOp = IrOpcode::PushI64;
      return true;
    case LocalInfo::ValueKind::Float32:
      zeroOp = IrOpcode::PushF32;
      return true;
    case LocalInfo::ValueKind::Float64:
      zeroOp = IrOpcode::PushF64;
      return true;
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Bool:
      zeroOp = IrOpcode::PushI32;
      return true;
    case LocalInfo::ValueKind::String:
      zeroOp = IrOpcode::PushI64;
      return true;
    default:
      error = "native backend requires a concrete uninitialized storage type on " + bindingName;
      return false;
  }
}

} // namespace primec::ir_lowerer
