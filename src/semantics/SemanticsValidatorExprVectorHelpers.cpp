#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {
namespace {

bool isVectorCompatibilityHelperName(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

bool isVectorHelperReceiverName(const Expr &candidate,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals) {
  if (candidate.kind != Expr::Kind::Name) {
    return false;
  }
  std::string typeName;
  for (const auto &param : params) {
    if (param.name == candidate.name) {
      typeName = normalizeBindingTypeName(param.binding.typeName);
      break;
    }
  }
  if (typeName.empty()) {
    auto it = locals.find(candidate.name);
    if (it != locals.end()) {
      typeName = normalizeBindingTypeName(it->second.typeName);
    }
  }
  return typeName == "vector" || typeName == "soa_vector";
}

void appendUniqueIndex(std::vector<size_t> &indices, size_t index, size_t limit) {
  if (index >= limit) {
    return;
  }
  for (size_t existing : indices) {
    if (existing == index) {
      return;
    }
  }
  indices.push_back(index);
}

} // namespace

bool SemanticsValidator::isVectorBuiltinName(const Expr &candidate, const char *helper) const {
  if (isSimpleCallName(candidate, helper)) {
    return true;
  }
  std::string namespacedCollection;
  std::string namespacedHelper;
  if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
    return false;
  }
  if (!(namespacedCollection == "vector" && namespacedHelper == helper)) {
    return false;
  }
  if ((namespacedHelper != "count" && namespacedHelper != "capacity") ||
      resolveCalleePath(candidate) != "/std/collections/vector/" + namespacedHelper) {
    return true;
  }
  return defMap_.find("/std/collections/vector/" + namespacedHelper) != defMap_.end() ||
         defMap_.find("/vector/" + namespacedHelper) != defMap_.end();
}

bool SemanticsValidator::resolveVectorHelperMethodTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiver,
    const std::string &helperName,
    std::string &resolvedOut) {
  resolvedOut.clear();
  auto resolveReceiverTypePath = [&](const std::string &typeName,
                                     const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      return "/" + normalizeBindingTypeName(typeName);
    }
    std::string resolvedType = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    return resolvedType;
  };
  if (receiver.kind == Expr::Kind::Name) {
    std::string typeName;
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
      }
    }
    if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
      return false;
    }
    const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
    if (resolvedType.empty()) {
      return false;
    }
    resolvedOut = resolvedType + "/" + helperName;
    return true;
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
      resolvedType = inferStructReturnPath(receiver, params, locals);
    }
    if (resolvedType.empty()) {
      return false;
    }
    resolvedOut = resolvedType + "/" + helperName;
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                                     const Expr &expr,
                                                     bool &hasResolutionOut,
                                                     std::string &resolvedPathOut,
                                                     size_t &receiverIndexOut) {
  hasResolutionOut = false;
  resolvedPathOut.clear();
  receiverIndexOut = 0;

  std::string vectorHelper;
  if (!getVectorStatementHelperName(expr, vectorHelper)) {
    return true;
  }

  std::string resolved = resolveCalleePath(expr);
  std::string namespacedCollection;
  std::string namespacedHelper;
  getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
  const bool isStdNamespacedVectorCanonicalHelperCall =
      !expr.isMethodCall && resolved.rfind("/std/collections/vector/", 0) == 0 &&
      isVectorCompatibilityHelperName(namespacedHelper);
  const bool hasImportedStdNamespacedVectorCanonicalHelper =
      isStdNamespacedVectorCanonicalHelperCall && hasImportedDefinitionPath(resolved);
  const std::string removedVectorCompatibilityPath = getDirectVectorHelperCompatibilityPath(expr);
  if (!removedVectorCompatibilityPath.empty()) {
    error_ = "unknown call target: " + removedVectorCompatibilityPath;
    return false;
  }
  if (isStdNamespacedVectorCanonicalHelperCall && !hasImportedStdNamespacedVectorCanonicalHelper &&
      defMap_.find(resolved) == defMap_.end()) {
    error_ = "unknown call target: " + resolved;
    return false;
  }

  size_t resolvedReceiverIndex = 0;
  const bool shouldProbeVectorHelperReceiver =
      !isStdNamespacedVectorCanonicalHelperCall && defMap_.find(resolved) == defMap_.end();
  if (shouldProbeVectorHelperReceiver && !expr.args.empty()) {
    std::vector<size_t> receiverIndices;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueIndex(receiverIndices, i, expr.args.size());
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendUniqueIndex(receiverIndices, 0, expr.args.size());
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendUniqueIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      appendUniqueIndex(receiverIndices, 0, expr.args.size());
    }

    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name &&
          !isVectorHelperReceiverName(expr.args.front(), params, locals)));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendUniqueIndex(receiverIndices, i, expr.args.size());
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
      }
      if (defMap_.count(methodTarget) > 0) {
        resolved = methodTarget;
        resolvedReceiverIndex = receiverIndex;
        break;
      }
    }
  }

  if (defMap_.find(resolved) == defMap_.end()) {
    if (!isStdNamespacedVectorCanonicalHelperCall) {
      error_ = vectorHelper + " is only supported as a statement";
      return false;
    }
    return true;
  }

  hasResolutionOut = true;
  resolvedPathOut = resolved;
  receiverIndexOut = resolvedReceiverIndex;
  return true;
}

} // namespace primec::semantics
