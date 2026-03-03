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
