#include "SemanticsValidator.h"

#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {

size_t SemanticsValidator::mapHelperReceiverIndex(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return i;
      }
    }
  }
  if (dispatchResolvers.resolveMapTarget == nullptr || candidate.args.empty()) {
    return 0;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args.front(), keyType, valueType)) {
    for (size_t i = 1; i < candidate.args.size(); ++i) {
      if (dispatchResolvers.resolveMapTarget(candidate.args[i], keyType, valueType)) {
        return i;
      }
    }
  }
  return 0;
}

bool SemanticsValidator::bareMapHelperOperandIndices(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    size_t &receiverIndexOut,
    size_t &keyIndexOut) const {
  receiverIndexOut = 0;
  keyIndexOut = 0;
  if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 2 ||
      dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  receiverIndexOut = mapHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndexOut >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args[receiverIndexOut], keyType, valueType)) {
    return false;
  }
  keyIndexOut = receiverIndexOut == 0 ? 1 : 0;
  return keyIndexOut < candidate.args.size();
}

std::string SemanticsValidator::preferredBareMapHelperTarget(std::string_view helperName) const {
  const std::string canonical = "/std/collections/map/" + std::string(helperName);
  if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  const std::string alias = "/map/" + std::string(helperName);
  if (hasDefinitionPath(alias)) {
    return alias;
  }
  return canonical;
}

std::string SemanticsValidator::specializedExperimentalMapHelperTarget(
    std::string_view helperName,
    const std::string &keyType,
    const std::string &valueType) const {
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  auto stripWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };

  const std::string basePath = preferredCanonicalExperimentalMapHelperTarget(helperName);
  std::ostringstream specializedPath;
  specializedPath << basePath
                  << "__t"
                  << std::hex
                  << fnv1a64(stripWhitespace(joinTemplateArgs({keyType, valueType})));
  if (defMap_.count(specializedPath.str()) > 0) {
    return specializedPath.str();
  }
  return basePath;
}

std::string SemanticsValidator::preferredBareVectorHelperTarget(std::string_view helperName) const {
  const std::string canonical = "/std/collections/vector/" + std::string(helperName);
  if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
    return canonical;
  }
  const std::string alias = "/vector/" + std::string(helperName);
  if (hasDefinitionPath(alias)) {
    return alias;
  }
  return canonical;
}

bool SemanticsValidator::tryRewriteBareMapHelperCall(
    const Expr &candidate,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  if (candidate.name == "/std/collections/map/" + std::string(helperName) ||
      candidate.name == "/map/" + std::string(helperName)) {
    return false;
  }
  const size_t receiverIndex = mapHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
    return false;
  }
  rewrittenOut = candidate;
  if (dispatchResolvers.resolveExperimentalMapTarget != nullptr &&
      dispatchResolvers.resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
    rewrittenOut.name = specializedExperimentalMapHelperTarget(helperName, keyType, valueType);
    rewrittenOut.templateArgs.clear();
  } else {
    rewrittenOut.name = preferredBareMapHelperTarget(helperName);
  }
  rewrittenOut.namespacePrefix.clear();
  return true;
}

bool SemanticsValidator::tryRewriteBareVectorHelperCall(
    const Expr &candidate,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveVectorTarget == nullptr) {
    return false;
  }
  if (candidate.name != helperName || candidate.name.find('/') != std::string::npos ||
      !candidate.namespacePrefix.empty()) {
    return false;
  }
  if (defMap_.find("/" + std::string(helperName)) != defMap_.end()) {
    return false;
  }
  const size_t receiverIndex = mapHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string elemType;
  if (!dispatchResolvers.resolveVectorTarget(candidate.args[receiverIndex], elemType)) {
    return false;
  }
  rewrittenOut = candidate;
  rewrittenOut.name = preferredBareVectorHelperTarget(helperName);
  rewrittenOut.namespacePrefix.clear();
  return true;
}

bool SemanticsValidator::tryRewriteCanonicalExperimentalMapHelperCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    Expr &rewrittenOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.args.empty() ||
      dispatchResolvers.resolveExperimentalMapTarget == nullptr ||
      dispatchResolvers.resolveExperimentalMapValueTarget == nullptr) {
    return false;
  }

  std::string canonicalPath;
  std::string helperName;
  Expr canonicalCandidate = candidate;
  if (candidate.isMethodCall) {
    std::string normalizedMethod = candidate.name;
    if (!normalizedMethod.empty() && normalizedMethod.front() == '/') {
      normalizedMethod.erase(normalizedMethod.begin());
    }
    if (normalizedMethod != "count" && normalizedMethod != "contains" && normalizedMethod != "tryAt" &&
        normalizedMethod != "at" && normalizedMethod != "at_unsafe") {
      return false;
    }
    helperName = normalizedMethod;
    canonicalPath = "/std/collections/map/" + normalizedMethod;
    canonicalCandidate.isMethodCall = false;
    canonicalCandidate.name = canonicalPath;
    canonicalCandidate.namespacePrefix.clear();
  } else if (!canonicalExperimentalMapHelperPath(resolveCalleePath(candidate), canonicalPath, helperName)) {
    return false;
  }

  const size_t receiverIndex =
      candidate.isMethodCall ? 0 : mapHelperReceiverIndex(canonicalCandidate, dispatchResolvers);
  if (receiverIndex >= canonicalCandidate.args.size()) {
    return false;
  }
  const Expr &receiverExpr = canonicalCandidate.args[receiverIndex];
  if (candidate.isMethodCall &&
      receiverExpr.kind == Expr::Kind::Call &&
      !receiverExpr.isBinding &&
      !receiverExpr.isMethodCall) {
    return false;
  }
  if (!candidate.isMethodCall &&
      !candidate.templateArgs.empty() &&
      receiverExpr.kind == Expr::Kind::Call &&
      !receiverExpr.isBinding &&
      !receiverExpr.isMethodCall) {
    return false;
  }

  std::string keyType;
  std::string valueType;
  if (!dispatchResolvers.resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType) &&
      !dispatchResolvers.resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
    return false;
  }
  rewrittenOut = canonicalCandidate;
  if (rewrittenOut.templateArgs.empty()) {
    rewrittenOut.templateArgs = {keyType, valueType};
  }
  if (!candidate.isMethodCall) {
    rewrittenOut.name = specializedExperimentalMapHelperTarget(helperName, keyType, valueType);
    rewrittenOut.namespacePrefix.clear();
    rewrittenOut.templateArgs.clear();
  }
  return true;
}

bool SemanticsValidator::explicitCanonicalExperimentalMapBorrowedHelperPath(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &resolvedPathOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty() ||
      dispatchResolvers.resolveExperimentalMapTarget == nullptr ||
      dispatchResolvers.resolveExperimentalMapValueTarget == nullptr) {
    return false;
  }
  std::string helperName;
  if (!canonicalExperimentalMapHelperPath(resolveCalleePath(candidate), resolvedPathOut, helperName)) {
    return false;
  }
  const size_t receiverIndex = mapHelperReceiverIndex(candidate, dispatchResolvers);
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return dispatchResolvers.resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
         !dispatchResolvers.resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
}

bool SemanticsValidator::hasResolvableMapHelperPath(const std::string &path) const {
  return hasDefinitionPath(path) || hasImportedDefinitionPath(path);
}

bool SemanticsValidator::resolveMapKeyType(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &keyTypeOut) const {
  keyTypeOut.clear();
  if (dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  std::string valueType;
  return dispatchResolvers.resolveMapTarget(target, keyTypeOut, valueType);
}

bool SemanticsValidator::resolveMapValueType(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &valueTypeOut) const {
  valueTypeOut.clear();
  if (dispatchResolvers.resolveMapTarget == nullptr) {
    return false;
  }
  std::string keyType;
  return dispatchResolvers.resolveMapTarget(target, keyType, valueTypeOut);
}

} // namespace primec::semantics
