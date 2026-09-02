// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

bool SemanticsValidator::resolveDeclaredSumMethodTarget(
    const std::string &sumPath,
    const std::string &normalizedMethodName,
    std::string &resolvedOut,
    bool &isBuiltinOut) const {
  if (sumPath.empty() || sumNames_.count(sumPath) == 0) {
    return false;
  }
  auto stripGeneratedLeafSuffix = [](std::string path) {
    const size_t leafStart = path.find_last_of('/');
    const size_t searchStart =
        leafStart == std::string::npos ? 0 : leafStart + 1;
    const size_t generatedSuffix = path.find("__", searchStart);
    if (generatedSuffix != std::string::npos) {
      path.erase(generatedSuffix);
    }
    return path;
  };
  auto appendCandidate = [](std::vector<std::string> &candidates,
                            std::string candidate) {
    if (!candidate.empty() &&
        std::find(candidates.begin(), candidates.end(), candidate) ==
            candidates.end()) {
      candidates.push_back(std::move(candidate));
    }
  };

  std::vector<std::string> candidates;
  const std::string canonicalSumPath = stripGeneratedLeafSuffix(sumPath);
  if (canonicalSumPath != sumPath) {
    appendCandidate(candidates,
                    canonicalSumPath + "/" + normalizedMethodName);
  }
  appendCandidate(candidates, sumPath + "/" + normalizedMethodName);
  const size_t leafStart = canonicalSumPath.find_last_of('/');
  const std::string leafName = leafStart == std::string::npos
                                   ? canonicalSumPath
                                   : canonicalSumPath.substr(leafStart + 1);
  if (!leafName.empty()) {
    appendCandidate(candidates, "/" + leafName + "/" + normalizedMethodName);
  }

  for (const std::string &candidate : candidates) {
    if (hasDefinitionFamilyPath(candidate) ||
        hasDefinitionPath(candidate) ||
        hasImportedDefinitionPath(candidate)) {
      resolvedOut = candidate;
      isBuiltinOut = false;
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::resolveSumTypePath(
    const std::string &typeText, const std::string &namespacePrefix) const {
  if (const Definition *sumDef =
          resolveSumDefinitionForTypeText(typeText, namespacePrefix)) {
    if (typeText.find('<') != std::string::npos &&
        sumDef->fullPath.find("__t") == std::string::npos) {
      std::string typeBase;
      std::string typeArgs;
      if (splitTemplateTypeName(normalizeBindingTypeName(typeText),
                                typeBase, typeArgs) &&
          !typeArgs.empty()) {
        uint64_t hash = 1469598103934665603ULL;
        for (const char ch : typeArgs) {
          if (std::isspace(static_cast<unsigned char>(ch))) {
            continue;
          }
          hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
          hash *= 1099511628211ULL;
        }
        std::ostringstream candidate;
        candidate << sumDef->fullPath << "__t" << std::hex << hash;
        if (sumNames_.count(candidate.str()) > 0) {
          return candidate.str();
        }
      }
      if (auto specializedIt =
              uniqueSpecializationPathByBase_.find(sumDef->fullPath);
          specializedIt != uniqueSpecializationPathByBase_.end() &&
          sumNames_.count(specializedIt->second) > 0) {
        return specializedIt->second;
      }
    }
    return sumDef->fullPath;
  }
  return "";
}

bool SemanticsValidator::maybeFailRetiredMaybeMutableHelperForType(
    const std::string &typeName, const std::string &typeTemplateArg,
    const std::string &normalizedMethodName, const Expr &receiver,
    bool &handledOut) {
  handledOut = false;
  if (!isRetiredMaybeMutableHelperName(normalizedMethodName)) {
    return false;
  }
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  std::string normalizedBaseTypeName = normalizedTypeName;
  if (!normalizedBaseTypeName.empty() && normalizedBaseTypeName.front() == '/') {
    normalizedBaseTypeName.erase(normalizedBaseTypeName.begin());
  }
  const std::string typeTextForResolution =
      (typeName.empty() || typeTemplateArg.empty())
          ? typeName
          : typeName + "<" + typeTemplateArg + ">";
  std::string resolvedMaybeType =
      resolveSumTypePath(typeTextForResolution, receiver.namespacePrefix);
  if (resolvedMaybeType.empty()) {
    resolvedMaybeType = resolveSumTypePath(typeName, receiver.namespacePrefix);
  }
  if (!isMaybeSumTypePath(resolvedMaybeType) &&
      !isMaybeSumTypePath(normalizedTypeName) &&
      !isMaybeSumTypePath(normalizedBaseTypeName)) {
    return false;
  }
  handledOut = true;
  std::string replacement;
  if (normalizedMethodName == "set") {
    replacement = "use some<T>(value) or Maybe<T>{[some] value} instead";
  } else if (normalizedMethodName == "clear") {
    replacement = "use Maybe<T>{} or none<T>() instead";
  } else {
    replacement = "use pick(value) and rebind the Maybe explicitly instead";
  }
  return failExprDiagnostic(receiver,
      "sum-backed Maybe<T> has no mutable helper " + normalizedMethodName +
      "; " + replacement);
}

bool SemanticsValidator::resolveRetiredMaybeMutableHelperMethodTarget(
    const Expr &receiver, const std::string &normalizedMethodName,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    bool &handledOut) {
  handledOut = false;
  if (!isRetiredMaybeMutableHelperName(normalizedMethodName)) {
    return false;
  }
  std::string receiverTypeName;
  std::string receiverTypeTemplateArg;
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      receiverTypeName = paramBinding->typeName;
      receiverTypeTemplateArg = paramBinding->typeTemplateArg;
    } else if (auto it = locals.find(receiver.name); it != locals.end()) {
      receiverTypeName = it->second.typeName;
      receiverTypeTemplateArg = it->second.typeTemplateArg;
    }
  } else if (receiver.kind == Expr::Kind::Call) {
    BindingInfo inferredReceiverBinding;
    if (withPreservedError([&]() {
          return inferBindingTypeFromInitializer(
              receiver, params, locals, inferredReceiverBinding);
        }) &&
        !inferredReceiverBinding.typeName.empty()) {
      receiverTypeName = normalizeBindingTypeName(inferredReceiverBinding.typeName);
      receiverTypeTemplateArg = inferredReceiverBinding.typeTemplateArg;
    }
  }
  return maybeFailRetiredMaybeMutableHelperForType(
      receiverTypeName, receiverTypeTemplateArg, normalizedMethodName, receiver, handledOut);
}

std::string SemanticsValidator::resolveMethodTargetStructTypePath(
    const std::string &typeName, const std::string &namespacePrefix) const {
  if (typeName.empty()) {
    return "";
  }
  if (!typeName.empty() && typeName[0] == '/') {
    return typeName;
  }
  std::string current = namespacePrefix;
  while (true) {
    if (!current.empty()) {
      std::string scoped = current + "/" + typeName;
      if (structNames_.count(scoped) > 0) {
        return scoped;
      }
      if (current.size() > typeName.size()) {
        const size_t start = current.size() - typeName.size();
        if (start > 0 && current[start - 1] == '/' &&
            current.compare(start, typeName.size(), typeName) == 0 &&
            structNames_.count(current) > 0) {
          return current;
        }
      }
    } else {
      std::string root = "/" + typeName;
      if (structNames_.count(root) > 0) {
        return root;
      }
    }
    if (current.empty()) {
      break;
    }
    const size_t slash = current.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      current.clear();
    } else {
      current.erase(slash);
    }
  }
  auto importIt = importAliases_.find(typeName);
  if (importIt != importAliases_.end()) {
    return importIt->second;
  }
  return "";
}

bool SemanticsValidator::resolveFieldBindingTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals, const Expr &target,
    BindingInfo &bindingOut) {
  if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
    return false;
  }
  return resolveStructFieldBinding(params, locals, target.args.front(), target.name, bindingOut);
}

bool SemanticsValidator::extractWrappedPointeeType(const std::string &typeText,
                                                    std::string &pointeeTypeOut) const {
  pointeeTypeOut.clear();
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if (base != "Reference" && base != "Pointer") {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  pointeeTypeOut = args.front();
  return !pointeeTypeOut.empty();
}

} // namespace primec::semantics
