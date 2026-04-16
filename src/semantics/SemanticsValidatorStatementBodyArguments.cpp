#include "SemanticsValidator.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

} // namespace

bool SemanticsValidator::validateStatementBodyArguments(const std::vector<ParameterInfo> &params,
                                                        std::unordered_map<std::string, BindingInfo> &locals,
                                                        const Expr &stmt,
                                                        ReturnKind returnKind,
                                                        const std::string &namespacePrefix,
                                                        const std::vector<Expr> *enclosingStatements,
                                                        size_t statementIndex,
                                                        bool &handled) {
  handled = false;
  auto failStatementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  if (!(stmt.hasBodyArguments || !stmt.bodyArguments.empty()) || isBuiltinBlockCall(stmt) || stmt.isLambda) {
    return true;
  }
  handled = true;

  std::string collectionName;
  if (defMap_.find(resolveCalleePath(stmt)) == defMap_.end() && getBuiltinCollectionName(stmt, collectionName)) {
    return failStatementDiagnostic(collectionName + " literal does not accept block arguments");
  }

  auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {
    auto helperSuffix = [](const std::string &candidate, const char *prefix) -> std::string_view {
      const size_t prefixLen = std::char_traits<char>::length(prefix);
      if (candidate.rfind(prefix, 0) != 0 || candidate.size() <= prefixLen) {
        return std::string_view();
      }
      return std::string_view(candidate).substr(prefixLen);
    };

    std::string_view helper = helperSuffix(path, "/array/");
    if (helper.empty()) {
      helper = helperSuffix(path, "/std/collections/vector/");
    }
    if (!helper.empty()) {
      return isRemovedVectorCompatibilityHelper(helper);
    }

    helper = helperSuffix(path, "/map/");
    if (helper.empty()) {
      helper = helperSuffix(path, "/std/collections/map/");
    }
    return !helper.empty() && isRemovedMapCompatibilityHelper(helper);
  };

  auto normalizeBodyArgumentTarget = [&](const std::string &path) {
    if (shouldPreserveBodyArgumentTarget(path)) {
      return path;
    }
    return preferVectorStdlibHelperPath(path);
  };

  auto preferredMapBodyArgumentTarget = [&](const std::string &helperName) {
    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (isRemovedMapCompatibilityHelper(helperName)) {
      return canonical;
    }
    if (defMap_.count(canonical) > 0) {
      return canonical;
    }
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    return canonical;
  };

  auto isMapReceiverExpr = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        std::string keyType;
        std::string valueType;
        return extractMapKeyValueTypes(*paramBinding, keyType, valueType);
      }

      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return extractMapKeyValueTypes(it->second, keyType, valueType);
    }

    if (target.kind != Expr::Kind::Call) {
      return false;
    }

    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return returnsMapCollectionType(transform.templateArgs.front());
      }
      return false;
    }

    std::string builtinCollectionName;
    return getBuiltinCollectionName(target, builtinCollectionName) &&
           builtinCollectionName == "map" &&
           target.templateArgs.size() == 2;
  };

  auto resolveBodyArgumentTarget = [&](const Expr &callExpr, std::string &resolvedOut) {
    auto resolveBareMapCallBodyArgumentTarget = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
        return false;
      }

      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }

      std::string helperName;
      if (normalized == "count") {
        helperName = "count";
      } else if (normalized == "at") {
        helperName = "at";
      } else if (normalized == "at_unsafe") {
        helperName = "at_unsafe";
      } else {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/count") {
          helperName = "count";
        } else if (resolvedPath == "/at") {
          helperName = "at";
        } else if (resolvedPath == "/at_unsafe") {
          helperName = "at_unsafe";
        } else {
          return false;
        }
      }

      if (defMap_.count("/" + helperName) > 0 || candidate.args.empty()) {
        return false;
      }

      if (hasNamedArguments(candidate.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            foundValues = true;
            if (isMapReceiverExpr(candidate.args[i])) {
              resolvedOut = preferredMapBodyArgumentTarget(helperName);
              return true;
            }
            break;
          }
        }
        if (!foundValues) {
          for (size_t i = 0; i < candidate.args.size(); ++i) {
            if (isMapReceiverExpr(candidate.args[i])) {
              resolvedOut = preferredMapBodyArgumentTarget(helperName);
              return true;
            }
          }
        }
      } else {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (isMapReceiverExpr(candidate.args[i])) {
            resolvedOut = preferredMapBodyArgumentTarget(helperName);
            return true;
          }
        }
      }
      return false;
    };

    if (!callExpr.isMethodCall) {
      if (resolveBareMapCallBodyArgumentTarget(callExpr)) {
        return;
      }
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }

    if (callExpr.args.empty()) {
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }

    const Expr &receiver = callExpr.args.front();
    std::string methodName = callExpr.name;
    const bool isExplicitMethodPath = !methodName.empty() && methodName.front() == '/';
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }

    std::string ignoredCollection;
    std::string namespacedHelper;
    if (getNamespacedCollectionHelperName(callExpr, ignoredCollection, namespacedHelper) &&
        !namespacedHelper.empty()) {
      methodName = namespacedHelper;
    }

    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
      const std::string resolvedType = resolveCalleePath(receiver);
      if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
        resolvedOut = normalizeBodyArgumentTarget(resolvedType + "/" + methodName);
        return;
      }
      if (!isExplicitMethodPath &&
          isRemovedMapCompatibilityHelper(methodName) &&
          isMapReceiverExpr(receiver)) {
        resolvedOut =
            preferredMapMethodTargetForCall(params, locals, receiver, methodName);
        if (resolvedOut.empty()) {
          resolvedOut = preferredMapBodyArgumentTarget(methodName);
        }
        return;
      }
    }

    std::string typeName;
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        typeName = paramBinding->typeName;
      } else {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          typeName = it->second.typeName;
        }
      }
    }

    if (typeName.empty()) {
      typeName = inferPointerLikeCallReturnType(receiver, params, locals);
    }

    if (typeName.empty()) {
      ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
      std::string inferredTypeName;
      if (inferredKind == ReturnKind::Array) {
        inferredTypeName = inferStructReturnPath(receiver, params, locals);
        if (inferredTypeName.empty()) {
          inferredTypeName = typeNameForReturnKind(inferredKind);
        }
      } else {
        inferredTypeName = typeNameForReturnKind(inferredKind);
      }
      if (!inferredTypeName.empty()) {
        typeName = inferredTypeName;
      }
    }

    auto shouldPreferCanonicalMapBodyArgumentTarget = [&](const std::string &candidateType) {
      if (isExplicitMethodPath) {
        return false;
      }
      if (!isRemovedMapCompatibilityHelper(methodName)) {
        return false;
      }
      std::string normalized = normalizeBindingTypeName(candidateType);
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      return isMapCollectionTypeName(normalized);
    };

    if (typeName.empty()) {
      if (isPointerExpr(receiver, params, locals)) {
        typeName = "Pointer";
      } else if (isPointerLikeExpr(receiver, params, locals)) {
        typeName = "Reference";
      }
    }

    if (typeName == "Pointer" || typeName == "Reference") {
      if (!isExplicitMethodPath &&
          isRemovedMapCompatibilityHelper(methodName) &&
          isMapReceiverExpr(receiver)) {
        resolvedOut =
            preferredMapMethodTargetForCall(params, locals, receiver, methodName);
        if (resolvedOut.empty()) {
          resolvedOut = preferredMapBodyArgumentTarget(methodName);
        }
        return;
      }
      resolvedOut = "/" + typeName + "/" + methodName;
      return;
    }

    if (typeName.empty()) {
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }

    if (isPrimitiveBindingTypeName(typeName)) {
      resolvedOut = normalizeBodyArgumentTarget("/" + normalizeBindingTypeName(typeName) + "/" + methodName);
      return;
    }

    std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }

    if (shouldPreferCanonicalMapBodyArgumentTarget(resolvedType.empty() ? typeName : resolvedType)) {
      resolvedOut =
          preferredMapMethodTargetForCall(params, locals, receiver, methodName);
      if (resolvedOut.empty()) {
        resolvedOut = preferredMapBodyArgumentTarget(methodName);
      }
      return;
    }

    resolvedOut = normalizeBodyArgumentTarget(resolvedType + "/" + methodName);
  };

  std::string resolved;
  resolveBodyArgumentTarget(stmt, resolved);
  if (defMap_.count(resolved) == 0) {
    return failStatementDiagnostic("block arguments require a definition target: " + resolved);
  }

  Expr call = stmt;
  call.bodyArguments.clear();
  call.hasBodyArguments = false;
  if (!validateExpr(params, locals, call)) {
    return false;
  }

  LocalBindingScope blockScope(*this, locals);
  const std::vector<Expr> *postMergeStatements = enclosingStatements;
  const size_t postMergeStartIndex = enclosingStatements == nullptr
                                         ? 0
                                         : std::min(statementIndex + 1, enclosingStatements->size());
  std::vector<BorrowLivenessRange> livenessRanges;
  livenessRanges.reserve(2);

  BorrowEndScope borrowScope(*this, currentValidationState_.endedReferenceBorrows);
  for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
    const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
    if (!validateStatement(params,
                           locals,
                           bodyExpr,
                           returnKind,
                           false,
                           true,
                           nullptr,
                           namespacePrefix,
                           &stmt.bodyArguments,
                           bodyIndex)) {
      return false;
    }
    livenessRanges.clear();
    livenessRanges.push_back(BorrowLivenessRange{&stmt.bodyArguments, bodyIndex + 1});
    if (postMergeStatements != nullptr && postMergeStartIndex < postMergeStatements->size()) {
      livenessRanges.push_back(BorrowLivenessRange{postMergeStatements, postMergeStartIndex});
    }
    expireReferenceBorrowsForRanges(params, locals, livenessRanges);
  }
  return true;
}

} // namespace primec::semantics
