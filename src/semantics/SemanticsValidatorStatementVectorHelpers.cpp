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

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "size" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

bool isSoaMutatorName(std::string_view helperName) {
  return helperName == "push" || helperName == "reserve";
}

bool isSoaAccessName(std::string_view helperName) {
  return helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref";
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

std::string explicitOldSoaAccessPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  const std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa_vector" && isSoaAccessName(normalizedName)) {
    return "/soa_vector/" + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa_vector/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName =
      std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaAccessName(helperName)) {
    return "";
  }
  return "/soa_vector/" + std::string(helperName);
}

} // namespace

std::string SemanticsValidator::preferVectorStdlibHelperPath(const std::string &path) const {
  auto hasVisibleDefinitionPath = [&](const std::string &candidate) {
    if (candidate.rfind("/map/", 0) == 0 &&
        defMap_.count(candidate) == 0 &&
        !hasDeclaredDefinitionPath(candidate)) {
      return false;
    }
    return hasImportedDefinitionPath(candidate) ||
           hasDeclaredDefinitionPath(candidate) ||
           defMap_.count(candidate) > 0;
  };
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      const std::string mapAlias = "/map/" + suffix;
      if (hasVisibleDefinitionPath(mapAlias)) {
        preferred = mapAlias;
      }
    }
  }
  if (preferred.rfind("/soa_vector/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/soa_vector/").size());
    const std::string stdlibAlias = "/std/collections/soa_vector/" + suffix;
    if (hasVisibleDefinitionPath(stdlibAlias)) {
      preferred = stdlibAlias;
    }
  }
  if (preferred.rfind("/std/collections/soa_vector/", 0) == 0 &&
      !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix =
        preferred.substr(std::string("/std/collections/soa_vector/").size());
    const std::string samePath =
        (suffix == "to_aos" || suffix == "to_aos_ref")
            ? "/" + suffix
            : "/soa_vector/" + suffix;
    if (hasVisibleDefinitionPath(samePath)) {
      preferred = samePath;
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
           normalized == "std/collections/internal_soa_storage/SoaFieldView";
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
  auto isSoaGrowthBinding = [&](const BindingInfo &binding) {
    if (normalizeBindingTypeName(binding.typeName) == "soa_vector") {
      return true;
    }
    std::string experimentalElemType;
    return extractExperimentalSoaVectorElementType(binding, experimentalElemType);
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
          const std::string normalizedType =
              normalizeBindingTypeName(binding->typeName);
          if (normalizedType != "Reference" &&
              normalizedType != "Pointer" &&
              isSoaGrowthBinding(*binding)) {
            borrowRootOut = receiverExpr.name;
            return true;
          }
          if (normalizedType == "Reference" || normalizedType == "Pointer") {
            std::string base;
            std::string arg;
            const std::string normalizedTarget =
                normalizeBindingTypeName(binding->typeTemplateArg);
            const bool targetsBuiltinSoa =
                !binding->typeTemplateArg.empty() &&
                (splitTemplateTypeName(normalizedTarget, base, arg)
                     ? normalizeBindingTypeName(base) == "soa_vector"
                     : normalizedTarget == "soa_vector");
            std::string experimentalElemType;
            if (!targetsBuiltinSoa &&
                !extractExperimentalSoaVectorElementType(*binding,
                                                        experimentalElemType)) {
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
  const bool vectorHelperIsPush = vectorHelper == "push";
  const bool vectorHelperIsReserve = vectorHelper == "reserve";
  const bool vectorHelperIsRemoveAt = vectorHelper == "remove_at";
  const bool vectorHelperIsRemoveSwap = vectorHelper == "remove_swap";
  const bool vectorHelperIsClear = vectorHelper == "clear";
  const bool vectorHelperIsPop = vectorHelper == "pop";
  const bool vectorHelperIsIndexedRemoval = vectorHelperIsRemoveAt || vectorHelperIsRemoveSwap;
  const bool vectorHelperNeedsStandaloneSoaBorrowCheck =
      vectorHelperIsPush || vectorHelperIsReserve || vectorHelperIsIndexedRemoval || vectorHelperIsClear;
  const bool hasHeapAllocEffect =
      currentValidationState_.context.activeEffects.count("heap_alloc") != 0;
  const bool hasNamedStatementArgs = hasNamedArguments(stmt.argNames);
  const size_t namedValuesStatementArgIndex = [&]() -> size_t {
    if (!hasNamedStatementArgs) {
      return stmt.args.size();
    }
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        return i;
      }
    }
    return stmt.args.size();
  }();
  const bool hasNamedValuesStatementArg = namedValuesStatementArgIndex < stmt.args.size();
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
  auto canonicalizeSoaMutatorHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  const std::string vectorHelperResolvedCanonical =
      canonicalizeSoaMutatorHelperPath(vectorHelperResolved);
  const bool isStdNamespacedSoaCanonicalMutatorHelperCall =
      !stmt.isMethodCall &&
      vectorHelperResolvedCanonical.rfind("/std/collections/soa_vector/", 0) == 0 &&
      (isLegacyOrCanonicalSoaHelperPath(vectorHelperResolvedCanonical, "push") ||
       isLegacyOrCanonicalSoaHelperPath(vectorHelperResolvedCanonical, "reserve"));
  const bool isStdNamespacedVectorCanonicalHelperCall =
      !stmt.isMethodCall && vectorHelperResolved.rfind("/std/collections/vector/", 0) == 0 &&
      (namespacedHelper == "count" || namespacedHelper == "capacity" || namespacedHelper == "at" ||
       namespacedHelper == "at_unsafe" || namespacedHelper == "push" || namespacedHelper == "pop" ||
       namespacedHelper == "reserve" || namespacedHelper == "clear" || namespacedHelper == "remove_at" ||
       namespacedHelper == "remove_swap");
  const bool isStdNamespacedCanonicalBuiltinHelperCall =
      isStdNamespacedVectorCanonicalHelperCall ||
      isStdNamespacedSoaCanonicalMutatorHelperCall;
  const std::string normalizedStatementNamespacePrefix =
      std::string(trimLeadingSlash(stmt.namespacePrefix));
  const std::string normalizedStatementName =
      std::string(trimLeadingSlash(stmt.name));
  auto isVectorMutatorName = [](std::string_view helperName) {
    return helperName == "push" || helperName == "pop" || helperName == "reserve" ||
           helperName == "clear" || helperName == "remove_at" || helperName == "remove_swap";
  };
  auto explicitCanonicalStdVectorMutatorCallPath = [&]() -> std::string {
    if (stmt.isMethodCall) {
      return "";
    }
    if (normalizedStatementNamespacePrefix == "std/collections/vector" &&
        isVectorMutatorName(normalizedStatementName)) {
      return "/std/collections/vector/" + normalizedStatementName;
    }
    constexpr std::string_view kCanonicalPrefix = "std/collections/vector/";
    if (normalizedStatementName.rfind(kCanonicalPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName =
        std::string_view(normalizedStatementName).substr(kCanonicalPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/std/collections/vector/" + std::string(helperName);
  }();
  auto explicitCanonicalStdVectorMutatorMethodPath = [&]() -> std::string {
    if (!stmt.isMethodCall) {
      return "";
    }
    if (normalizedStatementNamespacePrefix == "std/collections/vector" &&
        isVectorMutatorName(normalizedStatementName)) {
      return "/std/collections/vector/" + normalizedStatementName;
    }
    constexpr std::string_view kCanonicalPrefix = "std/collections/vector/";
    if (normalizedStatementName.rfind(kCanonicalPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName =
        std::string_view(normalizedStatementName).substr(kCanonicalPrefix.size());
    if (!isVectorMutatorName(helperName)) {
      return "";
    }
    return "/std/collections/vector/" + std::string(helperName);
  }();
  auto bareBuiltinVectorMutatorPreferredPath = [&]() -> std::string {
    const bool isBareMutatorCall =
        !stmt.isMethodCall && normalizedStatementNamespacePrefix.empty() &&
        normalizedStatementName == vectorHelper;
    const bool isBareMutatorMethod =
        stmt.isMethodCall && normalizedStatementNamespacePrefix.empty() &&
        normalizedStatementName == vectorHelper;
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
    auto tryResolveReceiverIndex = [&](size_t receiverIndex) -> bool {
      if (receiverIndex >= stmt.args.size()) {
        return false;
      }
      return isBuiltinVectorReceiver(stmt.args[receiverIndex]);
    };

    const bool probePositionalReorderedReceiver =
        !stmt.isMethodCall && !hasNamedStatementArgs && stmt.args.size() > 1 &&
        (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
         stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
         (stmt.args.front().kind == Expr::Kind::Name && !isBuiltinVectorReceiver(stmt.args.front())));
    if (tryResolveReceiverIndex(0)) {
      return "/std/collections/vector/" + vectorHelper;
    }
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        if (tryResolveReceiverIndex(i)) {
          return "/std/collections/vector/" + vectorHelper;
        }
      }
    }
    return "";
  }();
  const bool isBareUnqualifiedVectorMutatorCall =
      !stmt.isMethodCall && normalizedStatementNamespacePrefix.empty() &&
      normalizedStatementName == vectorHelper;
  const bool shouldAllowStdNamespacedVectorHelperCompatibilityFallback = false;
  if (!bareBuiltinVectorMutatorPreferredPath.empty()) {
    vectorHelperResolved = bareBuiltinVectorMutatorPreferredPath;
  }
  const bool isUserMethodTarget =
      stmt.isMethodCall && defMap_.find(vectorHelperResolved) != defMap_.end() &&
      vectorHelperResolved.rfind("/std/collections/vector/", 0) != 0 &&
      vectorHelperResolved.rfind("/soa_vector/", 0) != 0;
  if (isUserMethodTarget) {
    if (!stmt.args.empty() && vectorHelperNeedsStandaloneSoaBorrowCheck) {
      std::string borrowRoot;
      std::string ignoreBorrowName;
      if (resolveStandaloneSoaGrowthRoot(stmt.args.front(), borrowRoot,
                                         ignoreBorrowName) &&
          hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
        const std::string borrowSink =
            !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
        return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
      }
    }
    return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
  }
  const std::string oldExplicitSoaAccessPath = explicitOldSoaAccessPath(stmt);
  const bool hasVisibleOldExplicitSoaAccessHelper =
      !oldExplicitSoaAccessPath.empty() &&
      (hasDeclaredDefinitionPath(oldExplicitSoaAccessPath) ||
       hasImportedDefinitionPath(oldExplicitSoaAccessPath));
  if (!oldExplicitSoaAccessPath.empty() && !hasVisibleOldExplicitSoaAccessHelper) {
    if (hasNamedStatementArgs) {
      return failStatementDiagnostic(
          soaUnavailableMethodDiagnostic(oldExplicitSoaAccessPath));
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic(vectorHelper + " does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic(vectorHelper + " does not accept block arguments");
    }
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("argument count mismatch for builtin " + vectorHelper);
    }
    return true;
  }
  const std::string oldExplicitSoaPath = explicitOldSoaMutatorPath(stmt);
  const std::string oldExplicitSoaCanonicalPath =
      oldExplicitSoaPath.empty() ? "" : "/std/collections/soa_vector/" + vectorHelper;
  const bool hasVisibleOldExplicitSoaHelper =
      !oldExplicitSoaPath.empty() &&
      (hasDeclaredDefinitionPath(oldExplicitSoaPath) || hasImportedDefinitionPath(oldExplicitSoaPath));
  if (!oldExplicitSoaPath.empty() && !hasVisibleOldExplicitSoaHelper) {
    if (hasNamedStatementArgs) {
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
    if (!hasHeapAllocEffect) {
      return failStatementDiagnostic(vectorHelper + " requires heap_alloc effect");
    }
    if (!validateExpr(params, locals, stmt.args[1])) {
      return false;
    }
    if (vectorHelperIsPush) {
      if (!validateVectorStatementElementType(params, locals, stmt.args[1], receiverBinding.typeTemplateArg)) {
        return false;
      }
    } else if (!isVectorStatementIntegerExpr(params, locals, stmt.args[1])) {
      return failStatementDiagnostic("reserve requires integer capacity");
    }
    return validateVectorRelocationHelperElementType(
        receiverBinding, vectorHelper, namespacePrefix, definitionTemplateArgs);
  }
  auto hasDeclaredOrImportedPath = [&](const std::string &path) {
    return hasDeclaredDefinitionPath(path) || hasImportedDefinitionPath(path);
  };
  if (!explicitCanonicalStdVectorMutatorCallPath.empty() &&
      !hasDeclaredOrImportedPath(explicitCanonicalStdVectorMutatorCallPath)) {
    return failStatementDiagnostic("unknown call target: " + explicitCanonicalStdVectorMutatorCallPath);
  }
  if (!explicitCanonicalStdVectorMutatorMethodPath.empty() &&
      !hasDeclaredOrImportedPath(explicitCanonicalStdVectorMutatorMethodPath)) {
    return failStatementDiagnostic("unknown method: " + explicitCanonicalStdVectorMutatorMethodPath);
  }
  if (!bareBuiltinVectorMutatorPreferredPath.empty() &&
      !hasDeclaredOrImportedPath(bareBuiltinVectorMutatorPreferredPath)) {
    return failStatementDiagnostic(std::string(stmt.isMethodCall ? "unknown method: " : "unknown call target: ") +
                                   bareBuiltinVectorMutatorPreferredPath);
  }
  const bool hasResolvedVectorHelperDefinition = defMap_.find(vectorHelperResolved) != defMap_.end();
  bool hasResolvedReceiverIndex = false;
  size_t resolvedReceiverIndex = 0;
  if (stmt.isMethodCall && !stmt.args.empty()) {
    hasResolvedReceiverIndex = true;
    resolvedReceiverIndex = 0;
  }
  const bool shouldProbeVectorHelperReceiver =
      !(isStdNamespacedCanonicalBuiltinHelperCall && !hasResolvedVectorHelperDefinition &&
        !shouldAllowStdNamespacedVectorHelperCompatibilityFallback) &&
      (!hasResolvedVectorHelperDefinition || isNamespacedVectorHelperCall);
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
    auto tryResolveVectorHelperReceiverIndex = [&](size_t receiverIndex) -> bool {
      if (receiverIndex >= stmt.args.size()) {
        return false;
      }
      const Expr &receiverCandidate = stmt.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorStatementHelperTargetPath(params, locals, receiverCandidate, vectorHelper, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
      }
      if (!hasVisibleDefinitionPath(methodTarget)) {
        return false;
      }
      vectorHelperResolved = methodTarget;
      hasResolvedReceiverIndex = true;
      resolvedReceiverIndex = receiverIndex;
      return true;
    };
    if (hasNamedStatementArgs) {
      if (hasNamedValuesStatementArg) {
        (void)tryResolveVectorHelperReceiverIndex(namedValuesStatementArgIndex);
      } else if (!hasResolvedReceiverIndex) {
        if (tryResolveVectorHelperReceiverIndex(0)) {
          // Keep probing order equivalent to the prior staged index list.
        }
        for (size_t i = 1; !hasResolvedReceiverIndex && i < stmt.args.size(); ++i) {
          if (tryResolveVectorHelperReceiverIndex(i)) {
            break;
          }
        }
      }
    } else {
      const bool probePositionalReorderedReceiver =
          stmt.args.size() > 1 &&
          (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
           stmt.args.front().kind == Expr::Kind::FloatLiteral ||
           stmt.args.front().kind == Expr::Kind::StringLiteral ||
           (stmt.args.front().kind == Expr::Kind::Name && !isVectorHelperReceiverName(stmt.args.front())));
      if (!tryResolveVectorHelperReceiverIndex(0) && probePositionalReorderedReceiver) {
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          if (tryResolveVectorHelperReceiverIndex(i)) {
            break;
          }
        }
      }
    }
  }
  const bool hasDeclaredResolvedVectorHelper = hasDeclaredDefinitionPath(vectorHelperResolved);
  const bool hasImportedResolvedVectorHelper = hasImportedDefinitionPath(vectorHelperResolved);
  const bool hasVisibleResolvedVectorHelper =
      hasDeclaredResolvedVectorHelper || hasImportedResolvedVectorHelper ||
      hasResolvedVectorHelperDefinition;
  const bool isResolvedStdNamespacedVectorMutatorHelper =
      vectorHelperResolved == "/std/collections/vector/push" ||
      vectorHelperResolved == "/std/collections/vector/pop" ||
      vectorHelperResolved == "/std/collections/vector/reserve" ||
      vectorHelperResolved == "/std/collections/vector/clear" ||
      vectorHelperResolved == "/std/collections/vector/remove_at" ||
      vectorHelperResolved == "/std/collections/vector/remove_swap";
  if (isBareUnqualifiedVectorMutatorCall &&
      isResolvedStdNamespacedVectorMutatorHelper &&
      hasResolvedReceiverIndex && resolvedReceiverIndex < stmt.args.size() &&
      stmt.args[resolvedReceiverIndex].isFieldAccess) {
    return failStatementDiagnostic("unknown call target: " + vectorHelperResolved);
  }
  const bool canonicalBuiltinCompatibilityHelper =
      (isStdNamespacedCanonicalBuiltinHelperCall ||
       isResolvedStdNamespacedVectorMutatorHelper) &&
      hasVisibleResolvedVectorHelper;
  const bool canonicalCompatibilityAllowsSoaVectorTarget =
      vectorHelperIsPush || vectorHelperIsReserve;
  bool shouldUseCanonicalBuiltinCompatibilityFallback = false;
  size_t canonicalBuiltinCompatibilityReceiverIndex = 0;
  if (canonicalBuiltinCompatibilityHelper && !stmt.args.empty()) {
    auto tryResolveCanonicalBuiltinCompatibilityReceiverIndex = [&](size_t receiverIndex) -> bool {
      if (receiverIndex >= stmt.args.size()) {
        return false;
      }
      BindingInfo receiverBinding;
      if (!resolveVectorStatementBinding(params, locals, stmt.args[receiverIndex], receiverBinding)) {
        return false;
      }
      if (receiverBinding.typeName != "vector" &&
          !(canonicalCompatibilityAllowsSoaVectorTarget && receiverBinding.typeName == "soa_vector")) {
        return false;
      }
      shouldUseCanonicalBuiltinCompatibilityFallback = true;
      canonicalBuiltinCompatibilityReceiverIndex = receiverIndex;
      return true;
    };
    if (hasNamedStatementArgs) {
      if (hasNamedValuesStatementArg) {
        (void)tryResolveCanonicalBuiltinCompatibilityReceiverIndex(namedValuesStatementArgIndex);
      } else if (!shouldUseCanonicalBuiltinCompatibilityFallback) {
        if (tryResolveCanonicalBuiltinCompatibilityReceiverIndex(0)) {
          // Keep probing order equivalent to the prior staged index list.
        }
        for (size_t i = 1; !shouldUseCanonicalBuiltinCompatibilityFallback && i < stmt.args.size(); ++i) {
          if (tryResolveCanonicalBuiltinCompatibilityReceiverIndex(i)) {
            break;
          }
        }
      }
    } else {
      (void)tryResolveCanonicalBuiltinCompatibilityReceiverIndex(0);
    }
  }
  if (!shouldUseCanonicalBuiltinCompatibilityFallback &&
      isStdNamespacedSoaCanonicalMutatorHelperCall &&
      !stmt.args.empty()) {
    auto tryResolveSoaCanonicalMutatorReceiverIndex = [&](size_t receiverIndex) -> bool {
      if (receiverIndex >= stmt.args.size()) {
        return false;
      }
      BindingInfo receiverBinding;
      if (!resolveVectorStatementBinding(params, locals, stmt.args[receiverIndex], receiverBinding)) {
        return false;
      }
      if (receiverBinding.typeName != "soa_vector") {
        return false;
      }
      shouldUseCanonicalBuiltinCompatibilityFallback = true;
      canonicalBuiltinCompatibilityReceiverIndex = receiverIndex;
      return true;
    };
    if (hasNamedStatementArgs) {
      if (hasNamedValuesStatementArg) {
        (void)tryResolveSoaCanonicalMutatorReceiverIndex(namedValuesStatementArgIndex);
      } else if (!shouldUseCanonicalBuiltinCompatibilityFallback) {
        if (tryResolveSoaCanonicalMutatorReceiverIndex(0)) {
          // Keep probing order equivalent to the prior staged index list.
        }
        for (size_t i = 1; !shouldUseCanonicalBuiltinCompatibilityFallback && i < stmt.args.size(); ++i) {
          if (tryResolveSoaCanonicalMutatorReceiverIndex(i)) {
            break;
          }
        }
      }
    } else {
      (void)tryResolveSoaCanonicalMutatorReceiverIndex(0);
    }
  }
  const bool isCanonicalStdVectorMutatorMethodCall =
      stmt.isMethodCall && isResolvedStdNamespacedVectorMutatorHelper;
  const bool isResolvedExperimentalVectorHelper =
      vectorHelperResolved.rfind("/std/collections/experimental_vector/", 0) == 0;
  const bool isResolvedSoaHelper =
      vectorHelperResolved.rfind("/std/collections/soa_vector/", 0) == 0 ||
      vectorHelperResolved.rfind("/soa_vector/", 0) == 0;
  const bool isBareCanonicalIndexedRemovalExperimentalVectorBridgeCall =
      !stmt.isMethodCall &&
      stmt.namespacePrefix.empty() &&
      stmt.name == vectorHelper &&
      vectorHelperIsIndexedRemoval &&
      isResolvedExperimentalVectorHelper;
  if (isCanonicalStdVectorMutatorMethodCall &&
      !hasDeclaredResolvedVectorHelper &&
      !hasImportedResolvedVectorHelper) {
    return failStatementDiagnostic("unknown method: " + vectorHelperResolved);
  }
  if (!isBareCanonicalIndexedRemovalExperimentalVectorBridgeCall &&
      !shouldUseCanonicalBuiltinCompatibilityFallback &&
      bareBuiltinVectorMutatorPreferredPath.empty() &&
      hasVisibleResolvedVectorHelper) {
    const size_t receiverIndex = hasResolvedReceiverIndex ? resolvedReceiverIndex : 0;
    if (receiverIndex < stmt.args.size() && vectorHelperNeedsStandaloneSoaBorrowCheck) {
      std::string borrowRoot;
      std::string ignoreBorrowName;
      if (resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,
                                         ignoreBorrowName) &&
          hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
        const std::string borrowSink =
            !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
        return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
      }
    }
    Expr helperCall = stmt;
    helperCall.name = vectorHelperResolved;
    helperCall.isMethodCall = false;
    if (helperCall.templateArgs.empty() &&
        (isResolvedExperimentalVectorHelper || isResolvedSoaHelper)) {
      if (receiverIndex < stmt.args.size()) {
        const Expr &receiverExpr = stmt.args[receiverIndex];
        BindingInfo receiverBinding;
        if (resolveVectorStatementBinding(params, locals, receiverExpr, receiverBinding) &&
            !receiverBinding.typeTemplateArg.empty()) {
          helperCall.templateArgs = {receiverBinding.typeTemplateArg};
        }
      }
    }
    if (hasResolvedReceiverIndex && resolvedReceiverIndex > 0 &&
        resolvedReceiverIndex < helperCall.args.size()) {
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
    if (!hasNamedStatementArgs || stmt.args.empty()) {
      return true;
    }
    const bool helperAllowsSoaVectorTarget = helperName == "push" || helperName == "reserve";
    size_t receiverIndex = hasNamedValuesStatementArg ? namedValuesStatementArgIndex : 0;
    const bool hasExplicitValuesReceiver = hasNamedValuesStatementArg;
    if (!hasExplicitValuesReceiver) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        BindingInfo candidateBinding;
        if (!resolveVectorStatementBinding(params, locals, stmt.args[i], candidateBinding)) {
          continue;
        }
        if (candidateBinding.typeName == "vector" ||
            (helperAllowsSoaVectorTarget && candidateBinding.typeName == "soa_vector")) {
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
    if (hasNamedStatementArgs) {
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
  if (hasNamedStatementArgs) {
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
  if (!bareBuiltinVectorMutatorPreferredPath.empty() &&
      !shouldUseCanonicalBuiltinCompatibilityFallback && !stmt.args.empty()) {
    return failExprDiagnostic(stmt.args.front(),
                              vectorHelper +
                                  " requires mutable vector binding");
  }
  auto failIfStandaloneSoaGrowthBorrowed = [&](const BindingInfo &binding, const Expr &receiverExpr) -> bool {
    if (!isSoaGrowthBinding(binding)) {
      return false;
    }
    std::string borrowRoot;
    std::string ignoreBorrowName;
    if (!resolveStandaloneSoaGrowthRoot(receiverExpr, borrowRoot, ignoreBorrowName) ||
        !hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
      return false;
    }
    const std::string borrowSink =
        !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
    return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
  };
  const size_t builtinReceiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                          ? canonicalBuiltinCompatibilityReceiverIndex
                                          : 0;
  auto resolveBuiltinOperandIndex = [&](std::string_view namedArg) -> size_t {
    return shouldUseCanonicalBuiltinCompatibilityFallback
               ? findCanonicalBuiltinCompatibilityOperandIndex(namedArg)
               : 1;
  };
  if (vectorHelperIsPush) {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("push requires exactly two arguments");
    }
    const size_t receiverIndex = builtinReceiverIndex;
    const size_t valueIndex = resolveBuiltinOperandIndex("value");
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
    if (failIfStandaloneSoaGrowthBorrowed(binding, stmt.args[receiverIndex])) {
      return false;
    }
    if (!hasHeapAllocEffect) {
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
  if (vectorHelperIsReserve) {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("reserve requires exactly two arguments");
    }
    const size_t receiverIndex = builtinReceiverIndex;
    const size_t capacityIndex = resolveBuiltinOperandIndex("capacity");
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
    if (failIfStandaloneSoaGrowthBorrowed(binding, stmt.args[receiverIndex])) {
      return false;
    }
    if (!hasHeapAllocEffect) {
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
  if (vectorHelperIsIndexedRemoval) {
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic(vectorHelper + " requires exactly two arguments");
    }
    const size_t receiverIndex = builtinReceiverIndex;
    const size_t indexArgIndex = resolveBuiltinOperandIndex("index");
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
    if (failIfStandaloneSoaGrowthBorrowed(binding, stmt.args[receiverIndex])) {
      return false;
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
  if (vectorHelperIsPop || vectorHelperIsClear) {
    if (stmt.args.size() != 1) {
      return failStatementDiagnostic(vectorHelper + " requires exactly one argument");
    }
    const size_t receiverIndex = builtinReceiverIndex;
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
    if (vectorHelperIsClear &&
        failIfStandaloneSoaGrowthBorrowed(binding, stmt.args[receiverIndex])) {
      return false;
    }
    return true;
  }
  return true;
}

} // namespace primec::semantics
