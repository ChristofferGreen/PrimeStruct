#include "IrLowererReturnInferenceHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <functional>

namespace primec::ir_lowerer {

bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error) {
  out = EntryReturnConfig{};
  for (const auto &transform : entryDef.transforms) {
    if (transform.name != "return") {
      continue;
    }
    out.hasReturnTransform = true;
    if (transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void") {
      out.returnsVoid = true;
    }
    if (transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &typeName = transform.templateArgs.front();
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
      if (valueKindFromTypeName(trimTemplateTypeText(arg)) == LocalInfo::ValueKind::String) {
        error = "native backend does not support string array return types on " + entryPath;
        return false;
      }
    }
    bool resultHasValue = false;
    LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
    std::string resultErrorType;
    if (parseResultTypeName(typeName, resultHasValue, resultValueKind, resultErrorType)) {
      out.resultInfo.isResult = true;
      out.resultInfo.hasValue = resultHasValue;
      out.hasResultInfo = true;
    }
  }
  if (!out.hasReturnTransform && !entryDef.returnExpr.has_value()) {
    out.returnsVoid = true;
  }
  return true;
}

void analyzeDeclaredReturnTransforms(const Definition &def,
                                     const ResolveStructTypeNameForReturnFn &resolveStructTypeName,
                                     const ResolveStructArrayInfoForReturnFn &resolveStructArrayInfoFromPath,
                                     ReturnInfo &info,
                                     bool &hasReturnTransform,
                                     bool &hasReturnAuto) {
  hasReturnTransform = false;
  hasReturnAuto = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
        transform.name == "gpu_lane" || transform.name == "no_padding" ||
        transform.name == "platform_independent_padding") {
      info.returnsVoid = true;
      hasReturnTransform = true;
      break;
    }
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &typeName = transform.templateArgs.front();
    if (typeName == "auto") {
      hasReturnAuto = true;
      continue;
    }
    bool resultHasValue = false;
    LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
    std::string resultErrorType;
    if (parseResultTypeName(typeName, resultHasValue, resultValueKind, resultErrorType)) {
      info.returnsArray = false;
      info.returnsVoid = false;
      info.isResult = true;
      info.resultHasValue = resultHasValue;
      info.resultErrorType = resultErrorType;
      info.kind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      break;
    }
    if (typeName == "void") {
      info.returnsVoid = true;
      break;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
      info.returnsArray = true;
      info.kind = valueKindFromTypeName(arg);
      info.returnsVoid = false;
      break;
    }
    std::string structPath;
    StructArrayInfo structInfo;
    if (resolveStructTypeName(typeName, def.namespacePrefix, structPath) &&
        resolveStructArrayInfoFromPath(structPath, structInfo)) {
      info.returnsArray = true;
      info.kind = structInfo.elementKind;
      info.returnsVoid = false;
      break;
    }
    info.returnsArray = false;
    info.kind = valueKindFromTypeName(typeName);
    info.returnsVoid = false;
    break;
  }
}

bool inferReturnInferenceBindingIntoLocals(const Expr &bindingExpr,
                                           bool isParameter,
                                           const std::string &definitionPath,
                                           LocalMap &activeLocals,
                                           const IsBindingMutableForInferenceFn &isBindingMutable,
                                           const BindingKindForInferenceFn &bindingKind,
                                           const HasExplicitBindingTypeTransformForInferenceFn
                                               &hasExplicitBindingTypeTransform,
                                           const BindingValueKindForInferenceFn &bindingValueKind,
                                           const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                                           const IsFileErrorBindingForInferenceFn &isFileErrorBinding,
                                           const ApplyStructInfoForInferenceFn &applyStructArrayInfo,
                                           const ApplyStructInfoForInferenceFn &applyStructValueInfo,
                                           const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                                           const IsStringBindingForInferenceFn &isStringBinding,
                                           std::string &error) {
  LocalInfo bindingInfo;
  bindingInfo.index = 0;
  bindingInfo.isMutable = isBindingMutable(bindingExpr);
  bindingInfo.kind = bindingKind(bindingExpr);
  if (hasExplicitBindingTypeTransform(bindingExpr)) {
    bindingInfo.valueKind = bindingValueKind(bindingExpr, bindingInfo.kind);
  } else if (bindingExpr.args.size() == 1 && bindingInfo.kind == LocalInfo::Kind::Value) {
    bindingInfo.valueKind = inferExprKindFromLocals(bindingExpr.args.front(), activeLocals);
    if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
      bindingInfo.valueKind = LocalInfo::ValueKind::Int32;
    }
  } else if (isParameter) {
    bindingInfo.valueKind = bindingValueKind(bindingExpr, bindingInfo.kind);
  } else {
    bindingInfo.valueKind = LocalInfo::ValueKind::Unknown;
  }
  bindingInfo.isFileError = isFileErrorBinding(bindingExpr);
  applyStructArrayInfo(bindingExpr, bindingInfo);
  applyStructValueInfo(bindingExpr, bindingInfo);
  if (!isParameter && bindingInfo.structTypeName.empty() && bindingInfo.kind == LocalInfo::Kind::Value &&
      bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && bindingExpr.args.size() == 1) {
    std::string inferredStruct = inferStructExprPathFromLocals(bindingExpr.args.front(), activeLocals);
    if (!inferredStruct.empty()) {
      bindingInfo.structTypeName = inferredStruct;
    }
  }
  if (isStringBinding(bindingExpr) && bindingInfo.kind != LocalInfo::Kind::Value) {
    error = "native backend does not support string pointers or references";
    return false;
  }
  if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && bindingInfo.structTypeName.empty()) {
    if (isParameter) {
      error = "native backend requires typed parameters on " + definitionPath;
    } else {
      error = "native backend requires typed bindings on " + definitionPath;
    }
    return false;
  }
  activeLocals.emplace(bindingExpr.name, bindingInfo);
  return true;
}

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error) {
  LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
  bool inferredArray = false;
  LocalInfo::ValueKind inferredArrayKind = LocalInfo::ValueKind::Unknown;
  bool sawReturn = false;
  bool inferredVoid = false;

  auto recordInferredReturn = [&](const Expr &valueExpr, const LocalMap &activeLocals) -> bool {
    const LocalInfo::ValueKind arrayKind = inferArrayElementKindFromLocals(valueExpr, activeLocals);
    if (arrayKind != LocalInfo::ValueKind::Unknown) {
      if (arrayKind == LocalInfo::ValueKind::String) {
        error = "native backend does not support string array return types on " + def.fullPath;
        return false;
      }
      if (!inferredArray && inferred == LocalInfo::ValueKind::Unknown) {
        inferredArray = true;
        inferredArrayKind = arrayKind;
        return true;
      }
      if (inferredArray && inferredArrayKind == arrayKind) {
        return true;
      }
      error = "conflicting return types on " + def.fullPath;
      return false;
    }

    const LocalInfo::ValueKind kind = inferExprKindFromLocals(valueExpr, activeLocals);
    if (kind == LocalInfo::ValueKind::Unknown) {
      error = "unable to infer return type on " + def.fullPath;
      return false;
    }
    if (inferredArray) {
      error = "conflicting return types on " + def.fullPath;
      return false;
    }
    if (inferred == LocalInfo::ValueKind::Unknown) {
      inferred = kind;
      return true;
    }
    if (inferred != kind) {
      error = "conflicting return types on " + def.fullPath;
      return false;
    }
    return true;
  };

  std::function<bool(const Expr &, LocalMap &)> inferStatement;
  inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
    if (stmt.isBinding) {
      return inferBindingIntoLocals(stmt, false, activeLocals, error);
    }

    if (isReturnCall(stmt)) {
      sawReturn = true;
      if (stmt.args.empty()) {
        inferredVoid = true;
        return true;
      }
      return recordInferredReturn(stmt.args.front(), activeLocals);
    }

    if (isMatchCall(stmt)) {
      Expr expanded;
      if (!expandMatchToIf(stmt, expanded, error)) {
        return false;
      }
      return inferStatement(expanded, activeLocals);
    }

    if (isIfCall(stmt) && stmt.args.size() == 3) {
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      auto walkBlock = [&](const Expr &block) -> bool {
        LocalMap blockLocals = activeLocals;
        for (const auto &bodyStmt : block.bodyArguments) {
          if (!inferStatement(bodyStmt, blockLocals)) {
            return false;
          }
        }
        return true;
      };
      if (!walkBlock(thenBlock)) {
        return false;
      }
      if (!walkBlock(elseBlock)) {
        return false;
      }
      return true;
    }

    if (isRepeatCall(stmt)) {
      LocalMap blockLocals = activeLocals;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!inferStatement(bodyStmt, blockLocals)) {
          return false;
        }
      }
    }
    return true;
  };

  for (const auto &param : def.parameters) {
    if (!inferBindingIntoLocals(param, true, localsForInference, error)) {
      return false;
    }
  }

  LocalMap locals = localsForInference;
  for (const auto &stmt : def.statements) {
    if (!inferStatement(stmt, locals)) {
      return false;
    }
  }

  if (options.includeDefinitionReturnExpr && def.returnExpr.has_value()) {
    sawReturn = true;
    if (!recordInferredReturn(*def.returnExpr, locals)) {
      return false;
    }
  }

  if (!sawReturn) {
    if (options.missingReturnBehavior == MissingReturnBehavior::Void) {
      outInfo.returnsVoid = true;
      outInfo.returnsArray = false;
      outInfo.kind = LocalInfo::ValueKind::Unknown;
      return true;
    }
    error = "unable to infer return type on " + def.fullPath;
    return false;
  }

  if (inferredVoid) {
    if (inferred != LocalInfo::ValueKind::Unknown || inferredArray) {
      error = "conflicting return types on " + def.fullPath;
      return false;
    }
    outInfo.returnsVoid = true;
    outInfo.returnsArray = false;
    outInfo.kind = LocalInfo::ValueKind::Unknown;
    return true;
  }

  outInfo.returnsVoid = false;
  if (inferredArray) {
    outInfo.returnsArray = true;
    outInfo.kind = inferredArrayKind;
  } else {
    outInfo.returnsArray = false;
    outInfo.kind = inferred;
  }
  if (outInfo.kind == LocalInfo::ValueKind::Unknown) {
    error = "unable to infer return type on " + def.fullPath;
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
