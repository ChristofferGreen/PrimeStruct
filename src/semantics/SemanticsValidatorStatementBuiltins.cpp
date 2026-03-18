#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validatePathSpaceComputeBuiltinStatement(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &stmt,
    bool &handled) {
  handled = false;

  auto isIntegerKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64;
  };
  auto isNumericOrBoolKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
           kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool;
  };
  auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "array" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "array" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string collection;
      if (defMap_.find(resolveCalleePath(arg)) != defMap_.end()) {
        return false;
      }
      if (getBuiltinCollectionName(arg, collection) && collection == "array" && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
    }
    return false;
  };
  auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    auto resolveReferenceBufferType = [&](const std::string &typeName,
                                          const std::string &typeTemplateArg,
                                          std::string &elemTypeOut) -> bool {
      if ((typeName != "Reference" && typeName != "Pointer") || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) || normalizeBindingTypeName(base) != "Buffer" ||
          nestedArg.empty()) {
        return false;
      }
      elemTypeOut = nestedArg;
      return true;
    };
    auto resolveArgsPackReferenceBufferType = [&](const std::string &typeName,
                                                  const std::string &typeTemplateArg,
                                                  std::string &elemTypeOut) -> bool {
      if (typeName != "args" || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          (normalizeBindingTypeName(base) != "Reference" && normalizeBindingTypeName(base) != "Pointer") ||
          nestedArg.empty()) {
        return false;
      }
      return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackValueBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      auto resolveBinding = [&](const BindingInfo &binding) {
        std::string packElemType;
        std::string base;
        return getArgsPackElementType(binding, packElemType) &&
               splitTemplateTypeName(normalizeBindingTypeName(packElemType), base, elemTypeOut) &&
               normalizeBindingTypeName(base) == "Buffer";
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        return resolveBinding(*paramBinding);
      }
      auto itLocal = locals.find(targetName);
      return itLocal != locals.end() && resolveBinding(itLocal->second);
    };
    auto resolveIndexedArgsPackReferenceBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (resolveArgsPackReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemTypeOut)) {
          return true;
        }
      }
      auto itLocal = locals.find(targetName);
      return itLocal != locals.end() &&
             resolveArgsPackReferenceBufferType(itLocal->second.typeName, itLocal->second.typeTemplateArg, elemTypeOut);
    };
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "Buffer" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (resolveIndexedArgsPackValueBuffer(arg, elemType)) {
        return true;
      }
      if (isSimpleCallName(arg, "dereference") && arg.args.size() == 1) {
        const Expr &targetExpr = arg.args.front();
        if (targetExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, targetExpr.name)) {
            if (resolveReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemType)) {
              return true;
            }
          }
          auto itLocal = locals.find(targetExpr.name);
          if (itLocal != locals.end() &&
              resolveReferenceBufferType(itLocal->second.typeName, itLocal->second.typeTemplateArg, elemType)) {
            return true;
          }
        }
        if (resolveIndexedArgsPackReferenceBuffer(targetExpr, elemType)) {
          return true;
        }
      }
      if (isSimpleCallName(arg, "buffer") && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(arg, "upload") && arg.args.size() == 1) {
        return resolveArrayElemType(arg.args.front(), elemType);
      }
    }
    return false;
  };

  if (isSimpleCallName(stmt, "dispatch")) {
    handled = true;
    if (currentDefinitionIsCompute_) {
      error_ = "dispatch is not allowed in compute definitions";
      return false;
    }
    if (activeEffects_.count("gpu_dispatch") == 0) {
      error_ = "dispatch requires gpu_dispatch effect";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "dispatch does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "dispatch does not accept block arguments";
      return false;
    }
    if (stmt.args.size() < 4) {
      error_ = "dispatch requires kernel and three dimension arguments";
      return false;
    }
    if (stmt.args.front().kind != Expr::Kind::Name) {
      error_ = "dispatch requires kernel name as first argument";
      return false;
    }
    const Expr &kernelExpr = stmt.args.front();
    const std::string kernelPath = resolveCalleePath(kernelExpr);
    auto defIt = defMap_.find(kernelPath);
    if (defIt == defMap_.end()) {
      error_ = "unknown kernel: " + kernelPath;
      return false;
    }
    bool isCompute = false;
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "compute") {
        isCompute = true;
        break;
      }
    }
    if (!isCompute) {
      error_ = "dispatch requires compute definition: " + kernelPath;
      return false;
    }
    for (size_t i = 1; i <= 3; ++i) {
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
      ReturnKind dimKind = inferExprReturnKind(stmt.args[i], params, locals);
      if (!isIntegerKind(dimKind)) {
        error_ = "dispatch dimensions require integer expressions";
        return false;
      }
    }
    const auto &kernelParams = paramsByDef_[kernelPath];
    size_t trailingArgsPackIndex = kernelParams.size();
    const bool hasTrailingArgsPack = findTrailingArgsPackParameter(kernelParams, trailingArgsPackIndex, nullptr);
    const size_t minDispatchArgs = (hasTrailingArgsPack ? trailingArgsPackIndex : kernelParams.size()) + 4;
    if ((!hasTrailingArgsPack && stmt.args.size() != minDispatchArgs) ||
        (hasTrailingArgsPack && stmt.args.size() < minDispatchArgs)) {
      error_ = "dispatch argument count mismatch for " + kernelPath;
      return false;
    }
    for (size_t i = 4; i < stmt.args.size(); ++i) {
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
    }
    return true;
  }

  if (isSimpleCallName(stmt, "buffer_store")) {
    handled = true;
    if (!currentDefinitionIsCompute_) {
      error_ = "buffer_store requires a compute definition";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "buffer_store does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "buffer_store requires buffer, index, and value arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "buffer_store does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[0]) || !validateExpr(params, locals, stmt.args[1]) ||
        !validateExpr(params, locals, stmt.args[2])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(stmt.args[0], elemType)) {
      error_ = "buffer_store requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolKind(elemKind)) {
      error_ = "buffer_store requires numeric/bool element type";
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(stmt.args[1], params, locals);
    if (!isIntegerKind(indexKind)) {
      error_ = "buffer_store requires integer index";
      return false;
    }
    ReturnKind valueKind = inferExprReturnKind(stmt.args[2], params, locals);
    if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
      error_ = "buffer_store value type mismatch";
      return false;
    }
    return true;
  }

  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && defMap_.find(resolveCalleePath(stmt)) == defMap_.end()) {
    handled = true;
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error_ = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
               " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return false;
    }
    if (activeEffects_.count(pathSpaceBuiltin.requiredEffect) == 0) {
      error_ = pathSpaceBuiltin.name + " requires " + pathSpaceBuiltin.requiredEffect + " effect";
      return false;
    }
    auto isStringExpr = [&](const Expr &candidate) -> bool {
      if (candidate.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      auto isStringTypeName = [&](const std::string &typeName) -> bool {
        return normalizeBindingTypeName(typeName) == "string";
      };
      auto isStringArrayBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "array" && binding.typeName != "vector") {
          return false;
        }
        return isStringTypeName(binding.typeTemplateArg);
      };
      auto isStringMapBinding = [&](const BindingInfo &binding) -> bool {
        std::string keyType;
        std::string valueType;
        if (!extractMapKeyValueTypes(binding, keyType, valueType)) {
          return false;
        }
        return isStringTypeName(valueType);
      };
      auto isStringCollectionTarget = [&](const Expr &target) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            return isStringArrayBinding(*paramBinding) || isStringMapBinding(*paramBinding);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            return isStringArrayBinding(it->second) || isStringMapBinding(it->second);
          }
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
            return false;
          }
          if (!getBuiltinCollectionName(target, collection)) {
            return false;
          }
          if ((collection == "array" || collection == "vector") && target.templateArgs.size() == 1) {
            return isStringTypeName(target.templateArgs.front());
          }
          if (collection == "map" && target.templateArgs.size() == 2) {
            return isStringTypeName(target.templateArgs[1]);
          }
        }
        return false;
      };
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          return isStringTypeName(paramBinding->typeName);
        }
        auto it = locals.find(candidate.name);
        return it != locals.end() && isStringTypeName(it->second.typeName);
      }
      if (candidate.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(candidate)) == defMap_.end() &&
            getBuiltinArrayAccessName(candidate, accessName) && candidate.args.size() == 2) {
          return isStringCollectionTarget(candidate.args.front());
        }
      }
      return false;
    };
    if (!isStringExpr(stmt.args.front())) {
      error_ = pathSpaceBuiltin.name + " requires string path argument";
      return false;
    }
    for (const auto &arg : stmt.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
