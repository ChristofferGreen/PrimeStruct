#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

bool isDirectInferMapConstructorPath(std::string_view resolvedCandidate) {
  return isResolvedPublishedMapConstructorPath(std::string(resolvedCandidate));
}

} // namespace

bool SemanticsValidator::resolveInferArgsPackCountTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    std::string &elemType) const {
  elemType.clear();
  auto resolveBinding = [&](const BindingInfo &binding) {
    return getArgsPackElementType(binding, elemType);
  };
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return resolveBinding(*paramBinding);
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      return resolveBinding(it->second);
    }
  }
  return false;
}

bool SemanticsValidator::extractInferExperimentalMapFieldTypes(const BindingInfo &binding,
                                                               std::string &keyTypeOut,
                                                               std::string &valueTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) -> bool {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        if (base == "Map" || base == "std/collections/experimental_map/Map") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
            return false;
          }
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      return extractExperimentalMapFieldTypesFromStructPath(resolvedPath, keyTypeOut, valueTypeOut);
    }
  };

  keyTypeOut.clear();
  valueTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool SemanticsValidator::resolveInferExperimentalMapTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    std::string &keyTypeOut,
    std::string &valueTypeOut) {
  keyTypeOut.clear();
  valueTypeOut.clear();

  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractInferExperimentalMapFieldTypes(*paramBinding, keyTypeOut, valueTypeOut);
    }
    auto it = locals.find(target.name);
    return it != locals.end() &&
           extractInferExperimentalMapFieldTypes(it->second, keyTypeOut, valueTypeOut);
  }
  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  const std::string resolvedTarget = resolveCalleePath(target);
  if (isDirectInferMapConstructorPath(resolvedTarget)) {
    std::vector<std::string> args;
    if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
      keyTypeOut = args[0];
      valueTypeOut = args[1];
      return true;
    }
  }

  auto defIt = defMap_.find(resolvedTarget);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  BindingInfo inferredReturn;
  return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
         extractInferExperimentalMapFieldTypes(inferredReturn, keyTypeOut, valueTypeOut);
}

bool SemanticsValidator::resolveInferExperimentalMapValueTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &target,
    std::string &keyTypeOut,
    std::string &valueTypeOut) {
  auto extractValueBinding = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "Reference" || normalizedType == "Pointer") {
      return false;
    }
    return extractInferExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };

  keyTypeOut.clear();
  valueTypeOut.clear();
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractValueBinding(*paramBinding);
    }
    auto it = locals.find(target.name);
    return it != locals.end() && extractValueBinding(it->second);
  }

  BindingInfo fieldBinding;
  if (target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1 &&
      resolveStructFieldBinding(params, locals, target.args.front(), target.name, fieldBinding)) {
    return extractValueBinding(fieldBinding);
  }
  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  auto defIt = defMap_.find(resolveCalleePath(target));
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  BindingInfo inferredReturn;
  return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
         extractValueBinding(inferredReturn);
}

} // namespace primec::semantics
