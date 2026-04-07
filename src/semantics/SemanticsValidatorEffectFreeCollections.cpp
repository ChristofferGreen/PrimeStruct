#include "SemanticsValidator.h"

#include <vector>

namespace primec::semantics {

std::string SemanticsValidator::normalizeEffectFreeCollectionMethodName(const std::string &receiverPath,
                                                                        std::string methodName) const {
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  if (receiverPath == "/vector" || receiverPath == "/array") {
    const std::string vectorPrefix = "vector/";
    const std::string arrayPrefix = "array/";
    const std::string stdVectorPrefix = "std/collections/vector/";
    if (methodName.rfind(vectorPrefix, 0) == 0) {
      return methodName.substr(vectorPrefix.size());
    }
    if (methodName.rfind(arrayPrefix, 0) == 0) {
      return methodName.substr(arrayPrefix.size());
    }
    if (methodName.rfind(stdVectorPrefix, 0) == 0) {
      return methodName.substr(stdVectorPrefix.size());
    }
  }
  if (receiverPath == "/map") {
    const std::string mapPrefix = "map/";
    const std::string stdMapPrefix = "std/collections/map/";
    if (methodName.rfind(mapPrefix, 0) == 0) {
      return methodName.substr(mapPrefix.size());
    }
    if (methodName.rfind(stdMapPrefix, 0) == 0) {
      return methodName.substr(stdMapPrefix.size());
    }
  }
  return methodName;
}

std::vector<std::string> SemanticsValidator::effectFreeMethodPathCandidatesForReceiver(
    const std::string &receiverPath,
    const std::string &methodName) const {
  if (receiverPath == "/vector") {
    if (methodName == "count") {
      return {"/vector/" + methodName, "/std/collections/vector/" + methodName};
    }
    return {"/vector/" + methodName, "/std/collections/vector/" + methodName, "/array/" + methodName};
  }
  if (receiverPath == "/array") {
    if (methodName == "count") {
      return {"/array/" + methodName};
    }
    return {"/array/" + methodName, "/vector/" + methodName, "/std/collections/vector/" + methodName};
  }
  if (receiverPath == "/map") {
    if (methodName == "count" || methodName == "contains" || methodName == "tryAt" ||
        methodName == "at" || methodName == "at_unsafe" || methodName == "insert") {
      return {"/std/collections/map/" + methodName, "/map/" + methodName};
    }
    return {"/map/" + methodName, "/std/collections/map/" + methodName};
  }
  return {receiverPath + "/" + methodName};
}

std::string SemanticsValidator::preferEffectFreeCollectionHelperPath(const std::string &path) const {
  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (defMap_.count(vectorAlias) > 0) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defMap_.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defMap_.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (defMap_.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (defMap_.count(vectorAlias) > 0) {
        preferred = vectorAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (defMap_.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (defMap_.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe" && suffix != "insert") {
      const std::string mapAlias = "/map/" + suffix;
      if (defMap_.count(mapAlias) > 0) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

std::vector<std::string> SemanticsValidator::effectFreeCollectionHelperPathCandidates(const std::string &path) const {
  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    if (candidate.empty()) {
      return;
    }
    for (const auto &existing : candidates) {
      if (existing == candidate) {
        return;
      }
    }
    candidates.push_back(candidate);
  };

  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }

  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
      appendUnique("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe" && suffix != "insert") {
      appendUnique("/map/" + suffix);
    }
  }
  return candidates;
}

std::string SemanticsValidator::effectFreeCollectionPathFromType(const std::string &typeName,
                                                                 const std::string &typeTemplateArg) const {
  if (typeName == "string") {
    return "/string";
  }
  if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector") && !typeTemplateArg.empty()) {
    return "/" + typeName;
  }
  if (isMapCollectionTypeName(typeName) && !typeTemplateArg.empty()) {
    return "/map";
  }
  std::string base;
  std::string argsText;
  if (!splitTemplateTypeName(typeName, base, argsText)) {
    return "";
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argsText, args)) {
    return "";
  }
  if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
    return "/" + base;
  }
  if (isMapCollectionTypeName(base) && args.size() == 2) {
    return "/map";
  }
  if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
    return effectFreeCollectionPathFromType(normalizeBindingTypeName(args.front()), "");
  }
  return "";
}

std::string SemanticsValidator::effectFreeCollectionPathFromBinding(const BindingInfo &binding) const {
  if ((binding.typeName == "Reference" || binding.typeName == "Pointer") && !binding.typeTemplateArg.empty()) {
    return effectFreeCollectionPathFromType(normalizeBindingTypeName(binding.typeTemplateArg), "");
  }
  return effectFreeCollectionPathFromType(normalizeBindingTypeName(binding.typeName), binding.typeTemplateArg);
}

std::string SemanticsValidator::effectFreeCollectionPathFromCallExpr(const Expr &callExpr) const {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return "";
  }
  std::string builtinCollection;
  if (getBuiltinCollectionName(callExpr, builtinCollection)) {
    if (builtinCollection == "string") {
      return "/string";
    }
    if ((builtinCollection == "array" || builtinCollection == "vector" || builtinCollection == "soa_vector") &&
        callExpr.templateArgs.size() == 1) {
      return "/" + builtinCollection;
    }
    if (builtinCollection == "map" && callExpr.templateArgs.size() == 2) {
      return "/map";
    }
  }

  auto resolvedCandidates = effectFreeCollectionHelperPathCandidates(resolveCalleePath(callExpr));
  if (resolvedCandidates.empty()) {
    resolvedCandidates.push_back(resolveCalleePath(callExpr));
  }
  for (const auto &candidate : resolvedCandidates) {
    auto defIt = defMap_.find(candidate);
    if (defIt == defMap_.end() || !defIt->second) {
      continue;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string collectionPath =
          effectFreeCollectionPathFromType(normalizeBindingTypeName(transform.templateArgs.front()), "");
      if (!collectionPath.empty()) {
        return collectionPath;
      }
    }
  }
  return "";
}

std::string SemanticsValidator::resolveEffectFreeBareMapCallPath(const Expr &callExpr,
                                                                 const EffectFreeContext &ctx) const {
  if (callExpr.isMethodCall || callExpr.args.empty()) {
    return "";
  }
  std::string helperName;
  std::string normalizedName = callExpr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName == "count") {
    helperName = "count";
  } else if (normalizedName == "at") {
    helperName = "at";
  } else if (normalizedName == "at_unsafe") {
    helperName = "at_unsafe";
  } else {
    const std::string resolved = resolveCalleePath(callExpr);
    if (resolved == "/count") {
      helperName = "count";
    } else if (resolved == "/at") {
      helperName = "at";
    } else if (resolved == "/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return "";
    }
  }
  if (defMap_.count("/" + helperName) > 0) {
    return "";
  }
  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= callExpr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };
  if (hasNamedArguments(callExpr.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
          *callExpr.argNames[i] == "values") {
        appendReceiverIndex(i);
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  for (size_t receiverIndex : receiverIndices) {
    const Expr &receiver = callExpr.args[receiverIndex];
    if (receiver.kind == Expr::Kind::Name) {
      auto it = ctx.locals.find(receiver.name);
      if (it != ctx.locals.end() && effectFreeCollectionPathFromBinding(it->second) == "/map") {
        return "/std/collections/map/" + helperName;
      }
    }
    if (effectFreeCollectionPathFromCallExpr(receiver) == "/map") {
      return "/std/collections/map/" + helperName;
    }
  }
  return "";
}

} // namespace primec::semantics
