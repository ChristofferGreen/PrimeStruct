#include "SemanticsValidator.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

bool isIntegerReturnKind(ReturnKind kind) {
  return kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
         kind == ReturnKind::UInt64;
}

bool isNumericOrBoolReturnKind(ReturnKind kind) {
  return kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
         kind == ReturnKind::UInt64 || kind == ReturnKind::Float32 ||
         kind == ReturnKind::Float64 || kind == ReturnKind::Bool;
}

bool isStdlibBufferLoadWrapperDefinitionPath(std::string_view path) {
  return path.rfind("/std/gfx/Buffer/load", 0) == 0 ||
         path.rfind("/std/gfx/experimental/Buffer/load", 0) == 0;
}

bool isStdlibBufferStoreWrapperDefinitionPath(std::string_view path) {
  return path.rfind("/std/gfx/Buffer/store", 0) == 0 ||
         path.rfind("/std/gfx/experimental/Buffer/store", 0) == 0;
}

} // namespace

bool SemanticsValidator::validateExprGpuBufferBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;

  auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "array" &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "array" &&
            !itLocal->second.typeTemplateArg.empty()) {
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
      if (getBuiltinCollectionName(arg, collection) && collection == "array" &&
          arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
    }
    return false;
  };
  auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    auto resolveReferenceBufferType =
        [&](const std::string &typeName, const std::string &typeTemplateArg,
            std::string &elemTypeOut) -> bool {
      if ((typeName != "Reference" && typeName != "Pointer") ||
          typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          normalizeBindingTypeName(base) != "Buffer" || nestedArg.empty()) {
        return false;
      }
      elemTypeOut = nestedArg;
      return true;
    };
    auto resolveArgsPackReferenceBufferType =
        [&](const std::string &typeName, const std::string &typeTemplateArg,
            std::string &elemTypeOut) -> bool {
      if (typeName != "args" || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          (normalizeBindingTypeName(base) != "Reference" &&
           normalizeBindingTypeName(base) != "Pointer") ||
          nestedArg.empty()) {
        return false;
      }
      return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackReferenceBuffer =
        [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call ||
          !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 ||
          targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (resolveArgsPackReferenceBufferType(paramBinding->typeName,
                                               paramBinding->typeTemplateArg,
                                               elemTypeOut)) {
          return true;
        }
      }
      auto itLocal = locals.find(targetName);
      return itLocal != locals.end() &&
             resolveArgsPackReferenceBufferType(itLocal->second.typeName,
                                                itLocal->second.typeTemplateArg,
                                                elemTypeOut);
    };
    auto resolveIndexedArgsPackValueBuffer =
        [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call ||
          !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 ||
          targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      auto resolveBinding = [&](const BindingInfo &binding) {
        std::string packElemType;
        std::string base;
        return getArgsPackElementType(binding, packElemType) &&
               splitTemplateTypeName(normalizeBindingTypeName(packElemType),
                                     base, elemTypeOut) &&
               normalizeBindingTypeName(base) == "Buffer";
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        return resolveBinding(*paramBinding);
      }
      auto itLocal = locals.find(targetName);
      return itLocal != locals.end() && resolveBinding(itLocal->second);
    };

    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (normalizeBindingTypeName(paramBinding->typeName) == "Buffer" &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (normalizeBindingTypeName(itLocal->second.typeName) == "Buffer" &&
            !itLocal->second.typeTemplateArg.empty()) {
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
          if (const BindingInfo *paramBinding =
                  findParamBinding(params, targetExpr.name)) {
            if (resolveReferenceBufferType(paramBinding->typeName,
                                           paramBinding->typeTemplateArg,
                                           elemType)) {
              return true;
            }
          }
          auto itLocal = locals.find(targetExpr.name);
          if (itLocal != locals.end() &&
              resolveReferenceBufferType(itLocal->second.typeName,
                                         itLocal->second.typeTemplateArg,
                                         elemType)) {
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

  if (isSimpleCallName(expr, "dispatch")) {
    handledOut = true;
    if (currentValidationContext_.definitionIsCompute) {
      error_ = "dispatch is not allowed in compute definitions";
      return false;
    }
    if (currentValidationContext_.activeEffects.count("gpu_dispatch") == 0) {
      error_ = "dispatch requires gpu_dispatch effect";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "dispatch does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "dispatch does not accept block arguments";
      return false;
    }
    if (expr.args.size() < 4) {
      error_ = "dispatch requires kernel and three dimension arguments";
      return false;
    }
    if (expr.args.front().kind != Expr::Kind::Name) {
      error_ = "dispatch requires kernel name as first argument";
      return false;
    }
    const Expr &kernelExpr = expr.args.front();
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
      if (!validateExpr(params, locals, expr.args[i])) {
        return false;
      }
      ReturnKind dimKind = inferExprReturnKind(expr.args[i], params, locals);
      if (!isIntegerReturnKind(dimKind)) {
        error_ = "dispatch dimensions require integer expressions";
        return false;
      }
    }
    const auto &kernelParams = paramsByDef_[kernelPath];
    size_t trailingArgsPackIndex = kernelParams.size();
    const bool hasTrailingArgsPack =
        findTrailingArgsPackParameter(kernelParams, trailingArgsPackIndex, nullptr);
    const size_t minDispatchArgs =
        (hasTrailingArgsPack ? trailingArgsPackIndex : kernelParams.size()) + 4;
    if ((!hasTrailingArgsPack && expr.args.size() != minDispatchArgs) ||
        (hasTrailingArgsPack && expr.args.size() < minDispatchArgs)) {
      error_ = "dispatch argument count mismatch for " + kernelPath;
      return false;
    }
    for (size_t i = 4; i < expr.args.size(); ++i) {
      if (!validateExpr(params, locals, expr.args[i])) {
        return false;
      }
    }
    return true;
  }
  if (isSimpleCallName(expr, "buffer")) {
    handledOut = true;
    if (currentValidationContext_.definitionIsCompute) {
      error_ = "buffer is not allowed in compute definitions";
      return false;
    }
    if (currentValidationContext_.activeEffects.count("gpu_dispatch") == 0) {
      error_ = "buffer requires gpu_dispatch effect";
      return false;
    }
    if (expr.templateArgs.size() != 1) {
      error_ = "buffer requires exactly one template argument";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "buffer requires exactly one argument";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "buffer does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(expr.args.front(), params, locals);
    if (!isIntegerReturnKind(countKind)) {
      error_ = "buffer size requires integer expression";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(expr.templateArgs.front());
    if (!isNumericOrBoolReturnKind(elemKind)) {
      error_ = "buffer requires numeric/bool element type";
      return false;
    }
    return true;
  }
  if (isSimpleCallName(expr, "upload")) {
    handledOut = true;
    if (currentValidationContext_.definitionIsCompute) {
      error_ = "upload is not allowed in compute definitions";
      return false;
    }
    if (currentValidationContext_.activeEffects.count("gpu_dispatch") == 0) {
      error_ = "upload requires gpu_dispatch effect";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "upload does not accept template arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "upload requires exactly one argument";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "upload does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    std::string elemType;
    if (!resolveArrayElemType(expr.args.front(), elemType)) {
      error_ = "upload requires array input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      error_ = "upload requires numeric/bool element type";
      return false;
    }
    return true;
  }
  if (isSimpleCallName(expr, "readback")) {
    handledOut = true;
    if (currentValidationContext_.definitionIsCompute) {
      error_ = "readback is not allowed in compute definitions";
      return false;
    }
    if (currentValidationContext_.activeEffects.count("gpu_dispatch") == 0) {
      error_ = "readback requires gpu_dispatch effect";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "readback does not accept template arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "readback requires exactly one argument";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "readback does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args.front(), elemType)) {
      error_ = "readback requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      error_ = "readback requires numeric/bool element type";
      return false;
    }
    return true;
  }
  if (isSimpleCallName(expr, "buffer_load")) {
    handledOut = true;
    if (!currentValidationContext_.definitionIsCompute &&
        !isStdlibBufferLoadWrapperDefinitionPath(currentValidationContext_.definitionPath)) {
      error_ = "buffer_load requires a compute definition";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "buffer_load does not accept template arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "buffer_load requires buffer and index arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "buffer_load does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, expr.args[0]) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args[0], elemType)) {
      error_ = "buffer_load requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      error_ = "buffer_load requires numeric/bool element type";
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
    if (!isIntegerReturnKind(indexKind)) {
      error_ = "buffer_load requires integer index";
      return false;
    }
    return true;
  }
  if (isSimpleCallName(expr, "buffer_store")) {
    handledOut = true;
    if (!currentValidationContext_.definitionIsCompute &&
        !isStdlibBufferStoreWrapperDefinitionPath(currentValidationContext_.definitionPath)) {
      error_ = "buffer_store requires a compute definition";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "buffer_store does not accept template arguments";
      return false;
    }
    if (expr.args.size() != 3) {
      error_ = "buffer_store requires buffer, index, and value arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "buffer_store does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, expr.args[0]) ||
        !validateExpr(params, locals, expr.args[1]) ||
        !validateExpr(params, locals, expr.args[2])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args[0], elemType)) {
      error_ = "buffer_store requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      error_ = "buffer_store requires numeric/bool element type";
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
    if (!isIntegerReturnKind(indexKind)) {
      error_ = "buffer_store requires integer index";
      return false;
    }
    ReturnKind valueKind = inferExprReturnKind(expr.args[2], params, locals);
    if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
      error_ = "buffer_store value type mismatch";
      return false;
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
