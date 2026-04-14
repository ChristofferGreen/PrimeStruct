#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {
namespace {

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

std::vector<std::string> pointerLikeCallPathCandidates(const std::string &path) {
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
  auto canonicalizePath = [](const std::string &candidate) {
    const size_t instantiationPos = candidate.find("__t");
    if (instantiationPos == std::string::npos) {
      return candidate;
    }
    return candidate.substr(0, instantiationPos);
  };

  const std::string canonicalPath = canonicalizePath(path);
  appendUnique(path);
  appendUnique(canonicalPath);
  if (canonicalPath.rfind("/array/", 0) == 0) {
    const std::string suffix = canonicalPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (canonicalPath.rfind("/map/", 0) == 0) {
    const std::string suffix = canonicalPath.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      appendUnique("/std/collections/map/" + suffix);
    }
  } else if (canonicalPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = canonicalPath.substr(std::string("/std/collections/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      appendUnique("/map/" + suffix);
    }
  }

  return candidates;
}

} // namespace

std::string SemanticsValidator::normalizeCollectionMethodName(const std::string &methodName) const {
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    normalized = normalized.substr(vectorPrefix.size());
  } else if (normalized.rfind(arrayPrefix, 0) == 0) {
    normalized = normalized.substr(arrayPrefix.size());
  } else if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    normalized = normalized.substr(stdVectorPrefix.size());
  } else if (normalized.rfind(mapPrefix, 0) == 0) {
    normalized = normalized.substr(mapPrefix.size());
  } else if (normalized.rfind(stdMapPrefix, 0) == 0) {
    normalized = normalized.substr(stdMapPrefix.size());
  }
  return normalized;
}

std::string SemanticsValidator::inferPointerLikeCallReturnType(
    const Expr &receiverExpr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  (void)params;
  (void)locals;
  if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
    return "";
  }

  std::vector<std::string> candidatePaths;
  auto appendCandidatePaths = [&](const std::string &path) {
    for (const auto &candidate : pointerLikeCallPathCandidates(path)) {
      bool seen = false;
      for (const auto &existing : candidatePaths) {
        if (existing == candidate) {
          seen = true;
          break;
        }
      }
      if (!seen) {
        candidatePaths.push_back(candidate);
      }
    }
  };

  appendCandidatePaths(resolveCalleePath(receiverExpr));
  std::string rawPath = receiverExpr.name;
  if (!receiverExpr.namespacePrefix.empty()) {
    rawPath = receiverExpr.namespacePrefix + "/" + receiverExpr.name;
  }
  if (!rawPath.empty() && rawPath.front() != '/') {
    rawPath.insert(rawPath.begin(), '/');
  }
  appendCandidatePaths(rawPath);

  for (const auto &callPath : candidatePaths) {
    auto defIt = defMap_.find(callPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      continue;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
        continue;
      }
      if (base == "Pointer") {
        return "Pointer";
      }
      if (base == "Reference") {
        return "Reference";
      }
    }
  }

  return "";
}

bool SemanticsValidator::resolvePointerLikeMethodTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiverExpr,
    const std::string &methodName,
    std::string &resolvedOut) {
  std::string typeName;
  if (receiverExpr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
      typeName = paramBinding->typeName;
    } else {
      auto it = locals.find(receiverExpr.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
      }
    }
  }
  if (typeName.empty()) {
    typeName = inferPointerLikeCallReturnType(receiverExpr, params, locals);
  }
  if (typeName.empty()) {
    ReturnKind inferredKind = inferExprReturnKind(receiverExpr, params, locals);
    std::string inferred = typeNameForReturnKind(inferredKind);
    if (!inferred.empty()) {
      typeName = inferred;
    }
  }
  if (typeName.empty()) {
    if (isPointerExpr(receiverExpr, params, locals)) {
      typeName = "Pointer";
    } else if (isPointerLikeExpr(receiverExpr, params, locals)) {
      typeName = "Reference";
    }
  }
  if (typeName != "Pointer" && typeName != "Reference") {
    return false;
  }
  resolvedOut = "/" + typeName + "/" + normalizeCollectionMethodName(methodName);
  return true;
}

} // namespace primec::semantics
