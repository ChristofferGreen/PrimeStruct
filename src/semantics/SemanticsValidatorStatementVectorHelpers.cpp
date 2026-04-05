#include "SemanticsValidator.h"

#include <string>
#include <string_view>
#include <utility>

namespace primec::semantics {
namespace {

std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool allowsVectorStdlibCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool isSoaMutatorName(std::string_view helperName) {
  return helperName == "push" || helperName == "reserve";
}

std::string explicitOldSoaMutatorPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  const std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa_vector" && isSoaMutatorName(normalizedName)) {
    return "/soa_vector/" + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa_vector/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName = std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaMutatorName(helperName)) {
    return "";
  }
  return "/soa_vector/" + std::string(helperName);
}

} // namespace

std::string SemanticsValidator::preferVectorStdlibHelperPath(const std::string &path) const {
  auto hasVisibleDefinitionPath = [&](const std::string &candidate) {
    if ((candidate.rfind("/vector/", 0) == 0 ||
         candidate.rfind("/map/", 0) == 0) &&
        defMap_.count(candidate) == 0 &&
        !hasDeclaredDefinitionPath(candidate)) {
      return false;
    }
    return hasImportedDefinitionPath(candidate) || defMap_.count(candidate) > 0;
  };
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (hasVisibleDefinitionPath(vectorAlias)) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/vector/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        preferred = stdlibAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (hasVisibleDefinitionPath(arrayAlias)) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (hasVisibleDefinitionPath(vectorAlias)) {
        preferred = vectorAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (hasVisibleDefinitionPath(arrayAlias)) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string mapAlias = "/map/" + suffix;
      if (hasVisibleDefinitionPath(mapAlias)) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

bool SemanticsValidator::validateVectorStatementHelper(const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       const Expr &stmt,
                                                       const std::string &namespacePrefix,
                                                       const std::vector<std::string> *definitionTemplateArgs,
                                                       const std::vector<Expr> *enclosingStatements,
                                                       size_t statementIndex,
                                                       bool &handled) {
  handled = false;
  auto publishStatementDiagnostic = [&]() -> bool {
    captureExprContext(stmt);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failStatementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  auto publishExistingStatementDiagnostic = [&]() -> bool {
    if (error_.empty()) {
      return false;
    }
    return publishStatementDiagnostic();
  };
  auto failBorrowedBindingDiagnostic =
      [&](const std::string &borrowRoot, const std::string &sinkName) -> bool {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        return failStatementDiagnostic("borrowed binding: " + borrowRoot +
                                       " (root: " + borrowRoot +
                                       ", sink: " + sink + ")");
      };
  auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      return &it->second;
    }
    return nullptr;
  };
  auto isSoaFieldViewBindingType = [&](const BindingInfo &binding) -> bool {
    std::string normalized = normalizeBindingTypeName(binding.typeName);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalized, base, arg)) {
      normalized = normalizeBindingTypeName(base);
    }
    return normalized == "SoaFieldView" ||
           normalized == "std/collections/experimental_soa_storage/SoaFieldView";
  };
  auto referenceRootForBorrowBinding =
      [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
        if (binding.typeName != "Reference" &&
            !isSoaFieldViewBindingType(binding)) {
          return "";
        }
        if (!binding.referenceRoot.empty()) {
          return binding.referenceRoot;
        }
        return bindingName;
      };
  auto hasActiveBorrowForRoot =
      [&](const std::string &borrowRoot,
          const std::string &ignoreBorrowName = std::string()) -> bool {
        if (borrowRoot.empty() || currentValidationState_.context.definitionIsUnsafe) {
          return false;
        }
        auto hasBorrowFrom =
            [&](const std::string &bindingName, const BindingInfo &binding) -> bool {
              if (!ignoreBorrowName.empty() && bindingName == ignoreBorrowName) {
                return false;
              }
              if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
                return false;
              }
              const std::string root = referenceRootForBorrowBinding(bindingName, binding);
              return !root.empty() && root == borrowRoot;
            };
        for (const auto &param : params) {
          if (hasBorrowFrom(param.name, param.binding)) {
            return true;
          }
        }
        for (const auto &entry : locals) {
          if (hasBorrowFrom(entry.first, entry.second)) {
            return true;
          }
        }
        return false;
      };
  std::function<bool(const Expr &, std::string &, std::string &)>
      resolveStandaloneSoaGrowthRoot;
  resolveStandaloneSoaGrowthRoot =
      [&](const Expr &receiverExpr,
          std::string &borrowRootOut,
          std::string &ignoreBorrowNameOut) -> bool {
        borrowRootOut.clear();
        ignoreBorrowNameOut.clear();
        if (receiverExpr.kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(receiverExpr.name);
          if (binding == nullptr) {
            return false;
          }
          if (binding->typeName == "soa_vector") {
            borrowRootOut = receiverExpr.name;
            return true;
          }
          const std::string normalizedType =
              normalizeBindingTypeName(binding->typeName);
          if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
              !binding->typeTemplateArg.empty()) {
            std::string base;
            std::string arg;
            const std::string normalizedTarget =
                normalizeBindingTypeName(binding->typeTemplateArg);
            if (splitTemplateTypeName(normalizedTarget, base, arg)) {
              if (normalizeBindingTypeName(base) != "soa_vector") {
                return false;
              }
            } else if (normalizedTarget != "soa_vector") {
              return false;
            }
            ignoreBorrowNameOut = receiverExpr.name;
            if (!binding->referenceRoot.empty()) {
              borrowRootOut = binding->referenceRoot;
            } else {
              borrowRootOut = receiverExpr.name;
            }
            return true;
          }
          return false;
        }
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string builtinName;
        if (getBuiltinPointerName(receiverExpr, builtinName) &&
            builtinName == "dereference" && receiverExpr.args.size() == 1) {
          const Expr &pointerExpr = receiverExpr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            const BindingInfo *binding = resolveNamedBinding(pointerExpr.name);
            if (binding == nullptr || binding->referenceRoot.empty()) {
              return false;
            }
            ignoreBorrowNameOut = pointerExpr.name;
            borrowRootOut = binding->referenceRoot;
            return true;
          }
          if (getBuiltinPointerName(pointerExpr, builtinName) &&
              builtinName == "location" && pointerExpr.args.size() == 1 &&
              pointerExpr.args.front().kind == Expr::Kind::Name) {
            borrowRootOut = pointerExpr.args.front().name;
            return true;
          }
        }
        return false;
      };
  std::string vectorHelper;
  if (!getVectorStatementHelperName(stmt, vectorHelper)) {
    return true;
  }
  handled = true;
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };

  std::string vectorHelperResolved = resolveCalleePath(stmt);
  std::string namespacedCollection;
  std::string namespacedHelper;
  const bool isNamespacedCollectionHelperCall =
      getNamespacedCollectionHelperName(stmt, namespacedCollection, namespacedHelper);
  const bool isNamespacedVectorHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "vector";
  const bool isStdNamespacedSoaCanonicalMutatorHelperCall =
      !stmt.isMethodCall &&
      (vectorHelperResolved.rfind("/std/collections/soa_vector/push", 0) == 0 ||
       vectorHelperResolved.rfind("/std/collections/soa_vector/reserve", 0) == 0);
  const bool isStdNamespacedVectorCanonicalHelperCall =
      !stmt.isMethodCall && vectorHelperResolved.rfind("/std/collections/vector/", 0) == 0 &&
      (namespacedHelper == "count" || namespacedHelper == "capacity" || namespacedHelper == "at" ||
       namespacedHelper == "at_unsafe" || namespacedHelper == "push" || namespacedHelper == "pop" ||
       namespacedHelper == "reserve" || namespacedHelper == "clear" || namespacedHelper == "remove_at" ||
       namespacedHelper == "remove_swap");
  const bool isStdNamespacedCanonicalBuiltinHelperCall =
      isStdNamespacedVectorCanonicalHelperCall ||
      isStdNamespacedSoaCanonicalMutatorHelperCall;
  auto isVectorMutatorName = [](std::string_view helperName) {
    return helperName == "push" || helperName == "pop" || helperName == "reserve" ||
           helperName == "clear" || helperName == "remove_at" || helperName == "remove_swap";
  };
  auto explicitCanonicalStdVectorMutatorCallPath = [&]() -> std::string {
    if (stmt.isMethodCall) {
      return "";
    }
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    if (normalizedPrefix == "std/collections/vector" && isVectorMutatorName(normalizedName)) {
      return "/std/collections/vector/" + normalizedName;
    }
    constexpr std::string_view kCanonicalPrefix = "std/collections/vector/";
    if (normalizedName.rfind(kCanonicalPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName = std::string_view(normalizedName).substr(kCanonicalPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/std/collections/vector/" + std::string(helperName);
  }();
  auto explicitAliasVectorMutatorCallPath = [&]() -> std::string {
    if (stmt.isMethodCall) {
      return "";
    }
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    if (normalizedPrefix == "vector" && isVectorMutatorName(normalizedName)) {
      return "/vector/" + normalizedName;
    }
    constexpr std::string_view kAliasPrefix = "vector/";
    if (normalizedName.rfind(kAliasPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName = std::string_view(normalizedName).substr(kAliasPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/vector/" + std::string(helperName);
  }();
  auto explicitCanonicalStdVectorMutatorMethodPath = [&]() -> std::string {
    if (!stmt.isMethodCall) {
      return "";
    }
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    if (normalizedPrefix == "std/collections/vector" && isVectorMutatorName(normalizedName)) {
      return "/std/collections/vector/" + normalizedName;
    }
    constexpr std::string_view kCanonicalPrefix = "std/collections/vector/";
    if (normalizedName.rfind(kCanonicalPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName = std::string_view(normalizedName).substr(kCanonicalPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/std/collections/vector/" + std::string(helperName);
  }();
  auto explicitAliasVectorMutatorMethodPath = [&]() -> std::string {
    if (!stmt.isMethodCall) {
      return "";
    }
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    if (normalizedPrefix == "vector" && isVectorMutatorName(normalizedName)) {
      return "/vector/" + normalizedName;
    }
    constexpr std::string_view kAliasPrefix = "vector/";
    if (normalizedName.rfind(kAliasPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName = std::string_view(normalizedName).substr(kAliasPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/vector/" + std::string(helperName);
  }();
  auto bareBuiltinVectorMutatorPreferredPath = [&]() -> std::string {
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    const bool isBareMutatorCall = !stmt.isMethodCall && normalizedPrefix.empty() && normalizedName == vectorHelper;
    const bool isBareMutatorMethod = stmt.isMethodCall && normalizedPrefix.empty() && normalizedName == vectorHelper;
    if ((!isBareMutatorCall && !isBareMutatorMethod) || stmt.args.empty()) {
      return "";
    }
    auto isBuiltinVectorReceiver = [&](const Expr &candidate) -> bool {
      BindingInfo binding;
      if (!resolveVectorStatementBinding(params, locals, candidate, binding)) {
        return false;
      }
      return binding.typeName == "vector";
    };
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    appendReceiverIndex(0);
    const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
    const bool probePositionalReorderedReceiver =
        !stmt.isMethodCall && !hasNamedArgs && stmt.args.size() > 1 &&
        (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
         stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
         (stmt.args.front().kind == Expr::Kind::Name && !isBuiltinVectorReceiver(stmt.args.front())));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      if (isBuiltinVectorReceiver(stmt.args[receiverIndex])) {
        return preferredBareVectorHelperTarget(vectorHelper);
      }
    }
    return "";
  }();
  const bool shouldAllowStdNamespacedVectorHelperCompatibilityFallback =
      isStdNamespacedVectorCanonicalHelperCall && !namespacedHelper.empty() &&
      hasVisibleDefinitionPath("/vector/" + namespacedHelper);
  const bool isUserMethodTarget =
      stmt.isMethodCall && defMap_.find(vectorHelperResolved) != defMap_.end() &&
      vectorHelperResolved.rfind("/vector/", 0) != 0 && vectorHelperResolved.rfind("/soa_vector/", 0) != 0;
  if (isUserMethodTarget) {
    return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
  }
  const std::string oldExplicitSoaPath = explicitOldSoaMutatorPath(stmt);
  const std::string oldExplicitSoaCanonicalPath =
      oldExplicitSoaPath.empty() ? "" : "/std/collections/soa_vector/" + vectorHelper;
  const bool hasVisibleOldExplicitSoaHelper =
      !oldExplicitSoaPath.empty() &&
      (hasDeclaredDefinitionPath(oldExplicitSoaPath) || hasImportedDefinitionPath(oldExplicitSoaPath));
  if (!oldExplicitSoaPath.empty() && !hasVisibleOldExplicitSoaHelper) {
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic(vectorHelper + " does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic(vectorHelper + " does not accept block arguments");
    }
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic(vectorHelper + " requires exactly two arguments");
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args.front(), vectorHelper)) {
      return false;
    }
    BindingInfo receiverBinding;
    if (!resolveVectorStatementBinding(params, locals, stmt.args.front(), receiverBinding)) {
      return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
    }
    if (receiverBinding.typeName != "soa_vector") {
      return failStatementDiagnostic(std::string(stmt.isMethodCall ? "unknown method: " : "unknown call target: ") +
                                     oldExplicitSoaCanonicalPath);
    }
    if (!validateVectorStatementHelperTarget(
            params, locals, stmt.args.front(), vectorHelper.c_str(), receiverBinding)) {
      return false;
    }
    if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failStatementDiagnostic(vectorHelper + " requires heap_alloc effect");
    }
    if (!validateExpr(params, locals, stmt.args[1])) {
      return false;
    }
    if (vectorHelper == "push") {
      if (!validateVectorStatementElementType(params, locals, stmt.args[1], receiverBinding.typeTemplateArg)) {
        return false;
      }
    } else if (!isVectorStatementIntegerExpr(params, locals, stmt.args[1])) {
      return failStatementDiagnostic("reserve requires integer capacity");
    }
    return validateVectorRelocationHelperElementType(
        receiverBinding, vectorHelper, namespacePrefix, definitionTemplateArgs);
  }
  if (!explicitCanonicalStdVectorMutatorCallPath.empty() &&
      !hasDeclaredDefinitionPath(explicitCanonicalStdVectorMutatorCallPath) &&
      !hasImportedDefinitionPath(explicitCanonicalStdVectorMutatorCallPath)) {
    return failStatementDiagnostic("unknown call target: " + explicitCanonicalStdVectorMutatorCallPath);
  }
  if (!explicitAliasVectorMutatorCallPath.empty() &&
      !hasDeclaredDefinitionPath(explicitAliasVectorMutatorCallPath) &&
      !hasImportedDefinitionPath(explicitAliasVectorMutatorCallPath)) {
    return failStatementDiagnostic("unknown call target: " + explicitAliasVectorMutatorCallPath);
  }
  if (!explicitCanonicalStdVectorMutatorMethodPath.empty() &&
      !hasDeclaredDefinitionPath(explicitCanonicalStdVectorMutatorMethodPath) &&
      !hasImportedDefinitionPath(explicitCanonicalStdVectorMutatorMethodPath)) {
    return failStatementDiagnostic("unknown method: " + explicitCanonicalStdVectorMutatorMethodPath);
  }
  if (!explicitAliasVectorMutatorMethodPath.empty() &&
      !hasDeclaredDefinitionPath(explicitAliasVectorMutatorMethodPath) &&
      !hasImportedDefinitionPath(explicitAliasVectorMutatorMethodPath)) {
    return failStatementDiagnostic("unknown method: " + explicitAliasVectorMutatorMethodPath);
  }
  if (!bareBuiltinVectorMutatorPreferredPath.empty() &&
      !hasDeclaredDefinitionPath(bareBuiltinVectorMutatorPreferredPath) &&
      !hasImportedDefinitionPath(bareBuiltinVectorMutatorPreferredPath)) {
    return failStatementDiagnostic(std::string(stmt.isMethodCall ? "unknown method: " : "unknown call target: ") +
                                   bareBuiltinVectorMutatorPreferredPath);
  }
  bool hasResolvedReceiverIndex = false;
  size_t resolvedReceiverIndex = 0;
  if (stmt.isMethodCall && !stmt.args.empty()) {
    hasResolvedReceiverIndex = true;
    resolvedReceiverIndex = 0;
  }
  const bool shouldProbeVectorHelperReceiver =
      !(isStdNamespacedCanonicalBuiltinHelperCall && defMap_.find(vectorHelperResolved) == defMap_.end() &&
        !shouldAllowStdNamespacedVectorHelperCompatibilityFallback) &&
      (defMap_.find(vectorHelperResolved) == defMap_.end() || isNamespacedVectorHelperCall) &&
      !(isStdNamespacedCanonicalBuiltinHelperCall && defMap_.find(vectorHelperResolved) != defMap_.end());
  if (shouldProbeVectorHelperReceiver && !stmt.args.empty()) {
    auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Name) {
        return false;
      }
      BindingInfo binding;
      if (!resolveVectorStatementBinding(params, locals, candidate, binding)) {
        return false;
      }
      if (binding.typeName == "soa_vector") {
        return true;
      }
      std::string experimentalElemType;
      return (binding.typeName == "vector" || extractExperimentalVectorElementType(binding, experimentalElemType)) &&
             binding.isMutable;
    };
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
    }
    const bool probePositionalReorderedReceiver =
        !isStdNamespacedCanonicalBuiltinHelperCall &&
        !hasNamedArgs && stmt.args.size() > 1 &&
        (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
         stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
         (stmt.args.front().kind == Expr::Kind::Name && !isVectorHelperReceiverName(stmt.args.front())));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      if (receiverIndex >= stmt.args.size()) {
        continue;
      }
      const Expr &receiverCandidate = stmt.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorStatementHelperTargetPath(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
      }
      if (hasVisibleDefinitionPath(methodTarget)) {
        vectorHelperResolved = methodTarget;
        hasResolvedReceiverIndex = true;
        resolvedReceiverIndex = receiverIndex;
        break;
      }
    }
  }
  const bool canonicalBuiltinCompatibilityHelper =
      isStdNamespacedCanonicalBuiltinHelperCall &&
      (hasDeclaredDefinitionPath(vectorHelperResolved) || hasImportedDefinitionPath(vectorHelperResolved));
  bool shouldUseCanonicalBuiltinCompatibilityFallback = false;
  size_t canonicalBuiltinCompatibilityReceiverIndex = 0;
  if (canonicalBuiltinCompatibilityHelper && !stmt.args.empty()) {
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    if (hasNamedArguments(stmt.argNames)) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
      const bool probePositionalReorderedReceiver =
          !isStdNamespacedCanonicalBuiltinHelperCall &&
          stmt.args.size() > 1 &&
          (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
           stmt.args.front().kind == Expr::Kind::FloatLiteral ||
           stmt.args.front().kind == Expr::Kind::StringLiteral);
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      BindingInfo receiverBinding;
      if (!resolveVectorStatementBinding(params, locals, stmt.args[receiverIndex], receiverBinding)) {
        continue;
      }
      const bool allowSoaVectorTarget = vectorHelper == "push" || vectorHelper == "reserve";
      if (receiverBinding.typeName == "vector" ||
          (allowSoaVectorTarget && receiverBinding.typeName == "soa_vector")) {
        shouldUseCanonicalBuiltinCompatibilityFallback = true;
        canonicalBuiltinCompatibilityReceiverIndex = receiverIndex;
        break;
      }
    }
  }
  if (!shouldUseCanonicalBuiltinCompatibilityFallback &&
      isStdNamespacedSoaCanonicalMutatorHelperCall &&
      !stmt.args.empty()) {
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    if (hasNamedArguments(stmt.argNames)) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
    }
    for (size_t receiverIndex : receiverIndices) {
      BindingInfo receiverBinding;
      if (!resolveVectorStatementBinding(params, locals, stmt.args[receiverIndex], receiverBinding)) {
        continue;
      }
      if (receiverBinding.typeName == "soa_vector") {
        shouldUseCanonicalBuiltinCompatibilityFallback = true;
        canonicalBuiltinCompatibilityReceiverIndex = receiverIndex;
        break;
      }
    }
  }
  const bool isCanonicalStdVectorMutatorMethodCall =
      stmt.isMethodCall &&
      (vectorHelperResolved == "/std/collections/vector/push" ||
       vectorHelperResolved == "/std/collections/vector/pop" ||
       vectorHelperResolved == "/std/collections/vector/reserve" ||
       vectorHelperResolved == "/std/collections/vector/clear" ||
       vectorHelperResolved == "/std/collections/vector/remove_at" ||
       vectorHelperResolved == "/std/collections/vector/remove_swap");
  const bool isBareCanonicalIndexedRemovalExperimentalVectorBridgeCall =
      !stmt.isMethodCall &&
      stmt.namespacePrefix.empty() &&
      stmt.name == vectorHelper &&
      (vectorHelper == "remove_at" || vectorHelper == "remove_swap") &&
      vectorHelperResolved.rfind("/std/collections/experimental_vector/", 0) == 0;
  if (isCanonicalStdVectorMutatorMethodCall &&
      !hasDeclaredDefinitionPath(vectorHelperResolved) &&
      !hasImportedDefinitionPath(vectorHelperResolved)) {
    return failStatementDiagnostic("unknown method: " + vectorHelperResolved);
  }
  if (!isBareCanonicalIndexedRemovalExperimentalVectorBridgeCall &&
      !shouldUseCanonicalBuiltinCompatibilityFallback &&
      (hasDeclaredDefinitionPath(vectorHelperResolved) || hasImportedDefinitionPath(vectorHelperResolved))) {
    Expr helperCall = stmt;
    helperCall.name = vectorHelperResolved;
    helperCall.isMethodCall = false;
    if (helperCall.templateArgs.empty() &&
        vectorHelperResolved.rfind("/std/collections/experimental_vector/", 0) == 0) {
      const size_t receiverIndex = hasResolvedReceiverIndex ? resolvedReceiverIndex : 0;
      if (receiverIndex < stmt.args.size()) {
        BindingInfo inferredBinding;
        bool hasInferredBinding = false;
        const Expr &receiverExpr = stmt.args[receiverIndex];
        if (receiverExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
            inferredBinding = *paramBinding;
            hasInferredBinding = true;
          } else if (auto it = locals.find(receiverExpr.name); it != locals.end()) {
            inferredBinding = it->second;
            hasInferredBinding = true;
          }
        }
        if (!hasInferredBinding) {
          std::string receiverTypeText;
          if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText)) {
            std::string base;
            std::string argText;
            const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
            if (splitTemplateTypeName(normalizedType, base, argText)) {
              inferredBinding.typeName = normalizeBindingTypeName(base);
              inferredBinding.typeTemplateArg = argText;
            } else {
              inferredBinding.typeName = normalizedType;
            }
            hasInferredBinding = true;
          }
        }
        if (hasInferredBinding) {
          std::string experimentalElemType;
          if (extractExperimentalVectorElementType(inferredBinding, experimentalElemType)) {
            helperCall.templateArgs = {experimentalElemType};
          }
        }
      }
    }
    if (!isStdNamespacedCanonicalBuiltinHelperCall &&
        hasResolvedReceiverIndex && resolvedReceiverIndex > 0 && resolvedReceiverIndex < helperCall.args.size()) {
      std::swap(helperCall.args[0], helperCall.args[resolvedReceiverIndex]);
      if (helperCall.argNames.size() < helperCall.args.size()) {
        helperCall.argNames.resize(helperCall.args.size());
      }
      std::swap(helperCall.argNames[0], helperCall.argNames[resolvedReceiverIndex]);
    }
    return validateExpr(params, locals, helperCall, enclosingStatements, statementIndex);
  }
  if (isStdNamespacedCanonicalBuiltinHelperCall &&
      !shouldAllowStdNamespacedVectorHelperCompatibilityFallback &&
      !shouldUseCanonicalBuiltinCompatibilityFallback) {
    return failStatementDiagnostic("unknown call target: " + vectorHelperResolved);
  }
  auto validateBuiltinNamedReceiverShape = [&](const std::string &helperName) -> bool {
    if (!hasNamedArguments(stmt.argNames) || stmt.args.empty()) {
      return true;
    }
    size_t receiverIndex = 0;
    bool hasExplicitValuesReceiver = false;
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        receiverIndex = i;
        hasExplicitValuesReceiver = true;
        break;
      }
    }
    if (!hasExplicitValuesReceiver) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        BindingInfo candidateBinding;
        if (!resolveVectorStatementBinding(params, locals, stmt.args[i], candidateBinding)) {
          continue;
        }
        const bool allowSoaVectorTarget = helperName == "push" || helperName == "reserve";
        if (candidateBinding.typeName == "vector" ||
            (allowSoaVectorTarget && candidateBinding.typeName == "soa_vector")) {
          receiverIndex = i;
          break;
        }
      }
    }
    if (receiverIndex >= stmt.args.size()) {
      receiverIndex = 0;
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args[receiverIndex], helperName)) {
      return false;
    }
    BindingInfo binding;
    if (!validateVectorStatementHelperTarget(
            params, locals, stmt.args[receiverIndex], helperName.c_str(), binding)) {
      return publishExistingStatementDiagnostic();
    }
    return true;
  };
  auto findCanonicalBuiltinCompatibilityOperandIndex = [&](std::string_view namedArg) -> size_t {
    if (!shouldUseCanonicalBuiltinCompatibilityFallback || stmt.args.empty()) {
      return 0;
    }
    if (hasNamedArguments(stmt.argNames)) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i == canonicalBuiltinCompatibilityReceiverIndex) {
          continue;
        }
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == namedArg) {
          return i;
        }
      }
    }
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i != canonicalBuiltinCompatibilityReceiverIndex) {
        return i;
      }
    }
    return stmt.args.size();
  };
  if (hasNamedArguments(stmt.argNames)) {
    if (!validateBuiltinNamedReceiverShape(vectorHelper)) {
      return false;
    }
    if (shouldUseCanonicalBuiltinCompatibilityFallback) {
      // Imported canonical vector mutators still lower through builtin vector ops on builtin vector receivers.
    } else {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
  }
  if (!stmt.templateArgs.empty()) {
    return failStatementDiagnostic(vectorHelper + " does not accept template arguments");
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    return failStatementDiagnostic(vectorHelper + " does not accept block arguments");
  }
  if (vectorHelper == "push") {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("push requires exactly two arguments");
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t valueIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                  ? findCanonicalBuiltinCompatibilityOperandIndex("value")
                                  : 1;
    if (receiverIndex >= stmt.args.size() || valueIndex >= stmt.args.size()) {
      return failStatementDiagnostic("push requires exactly two arguments");
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args[receiverIndex], "push")) {
      return false;
    }
    BindingInfo binding;
    if (!validateVectorStatementHelperTarget(params, locals, stmt.args[receiverIndex], "push", binding)) {
      return publishExistingStatementDiagnostic();
    }
    std::string borrowRoot;
    std::string ignoreBorrowName;
    if (binding.typeName == "soa_vector" &&
        resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,
                                       ignoreBorrowName) &&
        hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
      const std::string borrowSink =
          !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
      return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
    }
    if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failStatementDiagnostic("push requires heap_alloc effect");
    }
    if (!validateExpr(params, locals, stmt.args[valueIndex])) {
      return false;
    }
    if (!validateVectorStatementElementType(params, locals, stmt.args[valueIndex], binding.typeTemplateArg)) {
      return publishExistingStatementDiagnostic();
    }
    if (!validateVectorRelocationHelperElementType(binding, "push", namespacePrefix, definitionTemplateArgs)) {
      return publishExistingStatementDiagnostic();
    }
    return true;
  }
  if (vectorHelper == "reserve") {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("reserve requires exactly two arguments");
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t capacityIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? findCanonicalBuiltinCompatibilityOperandIndex("capacity")
                                     : 1;
    if (receiverIndex >= stmt.args.size() || capacityIndex >= stmt.args.size()) {
      return failStatementDiagnostic("reserve requires exactly two arguments");
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args[receiverIndex], "reserve")) {
      return false;
    }
    BindingInfo binding;
    if (!validateVectorStatementHelperTarget(params, locals, stmt.args[receiverIndex], "reserve", binding)) {
      return publishExistingStatementDiagnostic();
    }
    std::string borrowRoot;
    std::string ignoreBorrowName;
    if (binding.typeName == "soa_vector" &&
        resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,
                                       ignoreBorrowName) &&
        hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
      const std::string borrowSink =
          !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
      return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
    }
    if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failStatementDiagnostic("reserve requires heap_alloc effect");
    }
    if (!validateExpr(params, locals, stmt.args[capacityIndex])) {
      return false;
    }
    if (!isVectorStatementIntegerExpr(params, locals, stmt.args[capacityIndex])) {
      return failStatementDiagnostic("reserve requires integer capacity");
    }
    if (!validateVectorRelocationHelperElementType(binding, "reserve", namespacePrefix, definitionTemplateArgs)) {
      return publishExistingStatementDiagnostic();
    }
    return true;
  }
  if (vectorHelper == "remove_at" || vectorHelper == "remove_swap") {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic(vectorHelper + " requires exactly two arguments");
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t indexArgIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? findCanonicalBuiltinCompatibilityOperandIndex("index")
                                     : 1;
    if (receiverIndex >= stmt.args.size() || indexArgIndex >= stmt.args.size()) {
      return failStatementDiagnostic(vectorHelper + " requires exactly two arguments");
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args[receiverIndex], vectorHelper)) {
      return false;
    }
    BindingInfo binding;
    if (!validateVectorStatementHelperTarget(
            params, locals, stmt.args[receiverIndex], vectorHelper.c_str(), binding)) {
      return publishExistingStatementDiagnostic();
    }
    std::string borrowRoot;
    std::string ignoreBorrowName;
    if (binding.typeName == "soa_vector" &&
        resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,
                                       ignoreBorrowName) &&
        hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
      const std::string borrowSink =
          !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
      return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
    }
    if (!validateExpr(params, locals, stmt.args[indexArgIndex])) {
      return false;
    }
    if (!isVectorStatementIntegerExpr(params, locals, stmt.args[indexArgIndex])) {
      return failStatementDiagnostic(vectorHelper + " requires integer index");
    }
    if (!validateVectorIndexedRemovalHelperElementType(binding, vectorHelper, namespacePrefix, definitionTemplateArgs)) {
      return publishExistingStatementDiagnostic();
    }
    return true;
  }
  if (vectorHelper == "pop" || vectorHelper == "clear") {
    if (stmt.args.size() != 1) {
      return failStatementDiagnostic(vectorHelper + " requires exactly one argument");
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    if (receiverIndex >= stmt.args.size()) {
      return failStatementDiagnostic(vectorHelper + " requires exactly one argument");
    }
    if (!validateVectorStatementHelperReceiver(params, locals, stmt.args[receiverIndex], vectorHelper)) {
      return false;
    }
    BindingInfo binding;
    if (!validateVectorStatementHelperTarget(
            params, locals, stmt.args[receiverIndex], vectorHelper.c_str(), binding)) {
      return publishExistingStatementDiagnostic();
    }
    std::string borrowRoot;
    std::string ignoreBorrowName;
    if (binding.typeName == "soa_vector" && vectorHelper == "clear" &&
        resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,
                                       ignoreBorrowName) &&
        hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
      const std::string borrowSink =
          !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
      return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
    }
    return true;
  }
  return true;
}

} // namespace primec::semantics
