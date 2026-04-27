#include "SemanticsValidator.h"
#include "primec/StdlibSurfaceRegistry.h"

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
  return primec::stdlibSurfaceCanonicalHelperPath(
             primec::StdlibSurfaceId::GfxBufferHelpers, path) ==
         "/std/gfx/Buffer/load";
}

bool isStdlibBufferStoreWrapperDefinitionPath(std::string_view path) {
  return primec::stdlibSurfaceCanonicalHelperPath(
             primec::StdlibSurfaceId::GfxBufferHelpers, path) ==
         "/std/gfx/Buffer/store";
}

} // namespace

bool SemanticsValidator::validateExprGpuBufferBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  auto failGpuBufferDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
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
    if (currentValidationState_.context.definitionIsCompute) {
      return failGpuBufferDiagnostic("dispatch is not allowed in compute definitions");
    }
    if (currentValidationState_.context.activeEffects.count("gpu_dispatch") == 0) {
      return failGpuBufferDiagnostic("dispatch requires gpu_dispatch effect");
    }
    if (!expr.templateArgs.empty()) {
      return failGpuBufferDiagnostic("dispatch does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("dispatch does not accept block arguments");
    }
    if (expr.args.size() < 4) {
      return failGpuBufferDiagnostic("dispatch requires kernel and three dimension arguments");
    }
    if (expr.args.front().kind != Expr::Kind::Name) {
      return failGpuBufferDiagnostic("dispatch requires kernel name as first argument");
    }
    const Expr &kernelExpr = expr.args.front();
    const std::string kernelPath = resolveCalleePath(kernelExpr);
    auto defIt = defMap_.find(kernelPath);
    if (defIt == defMap_.end()) {
      return failGpuBufferDiagnostic("unknown kernel: " + kernelPath);
    }
    bool isCompute = false;
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "compute") {
        isCompute = true;
        break;
      }
    }
    if (!isCompute) {
      return failGpuBufferDiagnostic("dispatch requires compute definition: " + kernelPath);
    }
    for (size_t i = 1; i <= 3; ++i) {
      if (!validateExpr(params, locals, expr.args[i])) {
        return false;
      }
      ReturnKind dimKind = inferExprReturnKind(expr.args[i], params, locals);
      if (!isIntegerReturnKind(dimKind)) {
        return failGpuBufferDiagnostic("dispatch dimensions require integer expressions");
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
      return failGpuBufferDiagnostic("dispatch argument count mismatch for " + kernelPath);
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
    if (currentValidationState_.context.definitionIsCompute) {
      return failGpuBufferDiagnostic("buffer is not allowed in compute definitions");
    }
    if (currentValidationState_.context.activeEffects.count("gpu_dispatch") == 0) {
      return failGpuBufferDiagnostic("buffer requires gpu_dispatch effect");
    }
    if (expr.templateArgs.size() != 1) {
      return failGpuBufferDiagnostic("buffer requires exactly one template argument");
    }
    if (expr.args.size() != 1) {
      return failGpuBufferDiagnostic("buffer requires exactly one argument");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("buffer does not accept block arguments");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(expr.args.front(), params, locals);
    if (!isIntegerReturnKind(countKind)) {
      return failGpuBufferDiagnostic("buffer size requires integer expression");
    }
    ReturnKind elemKind = returnKindForTypeName(expr.templateArgs.front());
    if (!isNumericOrBoolReturnKind(elemKind)) {
      return failGpuBufferDiagnostic("buffer requires numeric/bool element type");
    }
    return true;
  }
  if (isSimpleCallName(expr, "upload")) {
    handledOut = true;
    if (currentValidationState_.context.definitionIsCompute) {
      return failGpuBufferDiagnostic("upload is not allowed in compute definitions");
    }
    if (currentValidationState_.context.activeEffects.count("gpu_dispatch") == 0) {
      return failGpuBufferDiagnostic("upload requires gpu_dispatch effect");
    }
    if (!expr.templateArgs.empty()) {
      return failGpuBufferDiagnostic("upload does not accept template arguments");
    }
    if (expr.args.size() != 1) {
      return failGpuBufferDiagnostic("upload requires exactly one argument");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("upload does not accept block arguments");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    std::string elemType;
    if (!resolveArrayElemType(expr.args.front(), elemType)) {
      return failGpuBufferDiagnostic("upload requires array input");
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      return failGpuBufferDiagnostic("upload requires numeric/bool element type");
    }
    return true;
  }
  if (isSimpleCallName(expr, "readback")) {
    handledOut = true;
    if (currentValidationState_.context.definitionIsCompute) {
      return failGpuBufferDiagnostic("readback is not allowed in compute definitions");
    }
    if (currentValidationState_.context.activeEffects.count("gpu_dispatch") == 0) {
      return failGpuBufferDiagnostic("readback requires gpu_dispatch effect");
    }
    if (!expr.templateArgs.empty()) {
      return failGpuBufferDiagnostic("readback does not accept template arguments");
    }
    if (expr.args.size() != 1) {
      return failGpuBufferDiagnostic("readback requires exactly one argument");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("readback does not accept block arguments");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args.front(), elemType)) {
      return failGpuBufferDiagnostic("readback requires Buffer input");
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      return failGpuBufferDiagnostic("readback requires numeric/bool element type");
    }
    return true;
  }
  if (isSimpleCallName(expr, "buffer_load")) {
    handledOut = true;
    if (!currentValidationState_.context.definitionIsCompute &&
        !isStdlibBufferLoadWrapperDefinitionPath(currentValidationState_.context.definitionPath)) {
      return failGpuBufferDiagnostic("buffer_load requires a compute definition");
    }
    if (!expr.templateArgs.empty()) {
      return failGpuBufferDiagnostic("buffer_load does not accept template arguments");
    }
    if (expr.args.size() != 2) {
      return failGpuBufferDiagnostic("buffer_load requires buffer and index arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("buffer_load does not accept block arguments");
    }
    if (!validateExpr(params, locals, expr.args[0]) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args[0], elemType)) {
      return failGpuBufferDiagnostic("buffer_load requires Buffer input");
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      return failGpuBufferDiagnostic("buffer_load requires numeric/bool element type");
    }
    ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
    if (!isIntegerReturnKind(indexKind)) {
      return failGpuBufferDiagnostic("buffer_load requires integer index");
    }
    return true;
  }
  if (isSimpleCallName(expr, "buffer_store")) {
    handledOut = true;
    if (!currentValidationState_.context.definitionIsCompute &&
        !isStdlibBufferStoreWrapperDefinitionPath(currentValidationState_.context.definitionPath)) {
      return failGpuBufferDiagnostic("buffer_store requires a compute definition");
    }
    if (!expr.templateArgs.empty()) {
      return failGpuBufferDiagnostic("buffer_store does not accept template arguments");
    }
    if (expr.args.size() != 3) {
      return failGpuBufferDiagnostic("buffer_store requires buffer, index, and value arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failGpuBufferDiagnostic("buffer_store does not accept block arguments");
    }
    if (!validateExpr(params, locals, expr.args[0]) ||
        !validateExpr(params, locals, expr.args[1]) ||
        !validateExpr(params, locals, expr.args[2])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(expr.args[0], elemType)) {
      return failGpuBufferDiagnostic("buffer_store requires Buffer input");
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolReturnKind(elemKind)) {
      return failGpuBufferDiagnostic("buffer_store requires numeric/bool element type");
    }
    ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
    if (!isIntegerReturnKind(indexKind)) {
      return failGpuBufferDiagnostic("buffer_store requires integer index");
    }
    ReturnKind valueKind = inferExprReturnKind(expr.args[2], params, locals);
    if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
      return failGpuBufferDiagnostic("buffer_store value type mismatch");
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
