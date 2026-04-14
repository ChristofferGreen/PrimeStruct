#include "SemanticsValidator.h"

#include <cctype>
#include <cstdint>
#include <sstream>

namespace primec::semantics {

namespace {

bool isRemovedMapCompatibilitySuffix(std::string_view suffix) {
  return suffix == "count" || suffix == "count_ref" ||
         suffix == "contains" || suffix == "contains_ref" ||
         suffix == "tryAt" || suffix == "tryAt_ref" ||
         suffix == "at" || suffix == "at_ref" ||
         suffix == "at_unsafe" || suffix == "at_unsafe_ref" ||
         suffix == "insert" || suffix == "insert_ref";
}

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

std::string normalizeStructReturnHelperPath(const std::string &path) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }
  return normalizedPath;
}

void eraseStructReturnCandidate(std::vector<std::string> &candidates, const std::string &candidate) {
  for (auto it = candidates.begin(); it != candidates.end();) {
    if (*it == candidate) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace

std::string SemanticsValidator::inferStructReturnCollectionPath(const std::string &typeName,
                                                                const std::string &typeTemplateArg) const {
  if (typeName.empty()) {
    return {};
  }

  std::string normalizedTypeName = typeName;
  std::string normalizedTypeTemplateArg = typeTemplateArg;
  if ((normalizedTypeName == "Reference" || normalizedTypeName == "Pointer") &&
      !normalizedTypeTemplateArg.empty()) {
    normalizedTypeName = normalizeBindingTypeName(normalizedTypeTemplateArg);
    normalizedTypeTemplateArg.clear();
  } else {
    normalizedTypeName = normalizeBindingTypeName(normalizedTypeName);
  }

  if (normalizedTypeName == "string") {
    return "/string";
  }
  if ((normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "soa_vector") &&
      !normalizedTypeTemplateArg.empty()) {
    return "/" + normalizedTypeName;
  }
  if (isMapCollectionTypeName(normalizedTypeName) && !normalizedTypeTemplateArg.empty()) {
    return "/map";
  }

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedTypeName, base, argText)) {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(argText, args)) {
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        return "/" + base;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        return "/map";
      }
    }
  }

  return {};
}

std::string SemanticsValidator::resolveInferStructTypePath(const std::string &typeName,
                                                           const std::string &namespacePrefix) const {
  if (typeName.empty()) {
    return {};
  }
  if (typeName.front() == '/') {
    return structNames_.count(typeName) > 0 ? typeName : std::string{};
  }
  const std::string resolved = resolveMethodStructTypePath(typeName, namespacePrefix);
  return structNames_.count(resolved) > 0 ? resolved : std::string{};
}

bool SemanticsValidator::isInferStructReturnNumericScalarExpr(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  const ReturnKind kind = inferExprReturnKind(expr, params, locals);
  const bool isSoftwareNumeric =
      kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex;
  if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
      kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || isSoftwareNumeric) {
    return true;
  }
  if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array) {
    return false;
  }
  if (kind == ReturnKind::Unknown) {
    if (expr.kind == Expr::Kind::StringLiteral || expr.kind == Expr::Kind::BoolLiteral) {
      return false;
    }
    if (!inferStructReturnPath(expr, params, locals).empty()) {
      return false;
    }
    return true;
  }
  return false;
}

std::string SemanticsValidator::inferStructReturnPointerTargetTypeText(
    const Expr &pointerExpr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto resolveBindingTargetTypeText = [&](const Expr &target) -> std::string {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return bindingTypeText(*paramBinding);
      }
      auto localIt = locals.find(target.name);
      if (localIt != locals.end()) {
        return bindingTypeText(localIt->second);
      }
      return {};
    }
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
      return {};
    }
    const std::string receiverStruct = inferStructReturnPath(target.args.front(), params, locals);
    if (receiverStruct.empty() || structNames_.count(receiverStruct) == 0) {
      return {};
    }
    auto defIt = defMap_.find(receiverStruct);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return {};
    }
    for (const auto &stmt : defIt->second->statements) {
      if (!stmt.isBinding || stmt.name != target.name) {
        continue;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
        return {};
      }
      return bindingTypeText(fieldBinding);
    }
    return {};
  };

  if (pointerExpr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
      if ((paramBinding->typeName == "Reference" || paramBinding->typeName == "Pointer") &&
          !paramBinding->typeTemplateArg.empty()) {
        return paramBinding->typeTemplateArg;
      }
      return {};
    }
    auto localIt = locals.find(pointerExpr.name);
    if (localIt != locals.end() &&
        (localIt->second.typeName == "Reference" || localIt->second.typeName == "Pointer") &&
        !localIt->second.typeTemplateArg.empty()) {
      return localIt->second.typeTemplateArg;
    }
    return {};
  }
  if (pointerExpr.kind != Expr::Kind::Call) {
    return {};
  }

  std::string pointerBuiltin;
  if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerBuiltin == "location" &&
      pointerExpr.args.size() == 1) {
    return resolveBindingTargetTypeText(pointerExpr.args.front());
  }

  auto defIt = defMap_.find(resolveCalleePath(pointerExpr));
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return {};
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
    if (!splitTemplateTypeName(normalizedReturnType, base, argText) || argText.empty()) {
      return {};
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      return argText;
    }
    return {};
  }
  return {};
}

std::vector<std::string> SemanticsValidator::inferStructReturnCollectionHelperPathCandidates(
    const std::string &path) const {
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    for (const auto &existing : candidates) {
      if (existing == candidate) {
        return;
      }
    }
    candidates.push_back(candidate);
  };

  const std::string normalizedPath = normalizeStructReturnHelperPath(path);
  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
        suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
        suffix != "remove_at" && suffix != "remove_swap") {
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && !isRemovedMapCompatibilitySuffix(suffix)) {
      appendUnique("/map/" + suffix);
    }
  }
  return candidates;
}

void SemanticsValidator::pruneInferStructReturnMapAccessCompatibilityCandidates(
    const std::string &path,
    std::vector<std::string> &candidates) const {
  const std::string normalizedPath = normalizeStructReturnHelperPath(path);
  if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (suffix == "at" || suffix == "at_ref" ||
        suffix == "at_unsafe" || suffix == "at_unsafe_ref") {
      eraseStructReturnCandidate(candidates, "/std/collections/map/" + suffix);
    }
    return;
  }
  if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix == "at" || suffix == "at_ref" ||
        suffix == "at_unsafe" || suffix == "at_unsafe_ref") {
      eraseStructReturnCandidate(candidates, "/map/" + suffix);
    }
  }
}

void SemanticsValidator::pruneInferStructReturnBuiltinVectorAccessCandidates(
    const Expr &candidate,
    const std::string &path,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::vector<std::string> &candidates) const {
  const std::string normalizedPath = normalizeStructReturnHelperPath(path);
  if (normalizedPath.rfind("/std/collections/vector/", 0) != 0) {
    return;
  }
  const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
  if (suffix != "at" && suffix != "at_unsafe") {
    return;
  }
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
    return;
  }

  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      receiverIndex = 0;
    }
  }
  if (receiverIndex >= candidate.args.size()) {
    return;
  }

  std::string elemType;
  if (!dispatchResolvers.resolveVectorTarget(candidate.args[receiverIndex], elemType) &&
      !dispatchResolvers.resolveArrayTarget(candidate.args[receiverIndex], elemType) &&
      !dispatchResolvers.resolveStringTarget(candidate.args[receiverIndex])) {
    return;
  }
  eraseStructReturnCandidate(candidates, "/std/collections/vector/" + suffix);
}

bool SemanticsValidator::isExplicitMapAccessStructReturnCompatibilityCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
    return false;
  }

  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }

  std::string helperName;
  if (normalized == "map/at") {
    helperName = "at";
  } else if (normalized == "map/at_unsafe") {
    helperName = "at_unsafe";
  } else {
    return false;
  }
  if (hasDefinitionPath("/map/" + helperName) || candidate.args.empty()) {
    return false;
  }

  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      receiverIndex = 0;
    }
  }
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }

  std::string keyType;
  std::string valueType;
  return dispatchResolvers.resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
}

std::string SemanticsValidator::specializedExperimentalMapStructReturnPath(
    const std::vector<std::string> &templateArgs) const {
  auto stripWhitespace = [](const std::string &text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        result.push_back(static_cast<char>(ch));
      }
    }
    return result;
  };
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= 1099511628211ULL;
    }
    return hash;
  };

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t"
                  << std::hex
                  << fnv1a64(stripWhitespace(joinTemplateArgs(templateArgs)));
  return specializedPath.str();
}

} // namespace primec::semantics
