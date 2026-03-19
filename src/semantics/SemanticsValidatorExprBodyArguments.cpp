#include "SemanticsValidator.h"

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
  return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
}

void appendUniqueReceiverIndex(std::vector<size_t> &receiverIndices, size_t index, size_t limit) {
  if (index >= limit) {
    return;
  }
  for (size_t existing : receiverIndices) {
    if (existing == index) {
      return;
    }
  }
  receiverIndices.push_back(index);
}

} // namespace

bool SemanticsValidator::validateExprBodyArguments(const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                                   const Expr &expr,
                                                   std::string &resolved,
                                                   bool &resolvedMethod,
                                                   const std::vector<Expr> *enclosingStatements,
                                                   size_t statementIndex) {
  if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) || isBuiltinBlockCall(expr)) {
    return true;
  }

  auto resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractMapKeyValueTypes(*paramBinding, keyType, valueType);
      }
      auto it = locals.find(target.name);
      return it != locals.end() && extractMapKeyValueTypes(it->second, keyType, valueType);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }

    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        collectionTypePath == "/map") {
      return true;
    }

    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return returnsMapCollectionType(transform.templateArgs.front());
      }
    }

    BindingInfo inferredReturn;
    return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
           extractMapKeyValueTypes(inferredReturn, keyType, valueType);
  };

  auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {
    auto helperSuffix = [](const std::string &candidate, const char *prefix) -> std::string_view {
      const size_t prefixLen = std::char_traits<char>::length(prefix);
      if (candidate.rfind(prefix, 0) != 0 || candidate.size() <= prefixLen) {
        return std::string_view();
      }
      return std::string_view(candidate).substr(prefixLen);
    };
    std::string_view helper = helperSuffix(path, "/vector/");
    if (helper.empty()) {
      helper = helperSuffix(path, "/array/");
    }
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

  auto resolveBareMapCallBodyArgumentTarget = [&]() -> bool {
    if (expr.isMethodCall || expr.name.empty()) {
      return false;
    }
    std::string normalized = expr.name;
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
    } else if (resolved == "/count") {
      helperName = "count";
    } else if (resolved == "/at") {
      helperName = "at";
    } else if (resolved == "/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return false;
    }
    if (defMap_.count("/" + helperName) > 0 || expr.args.empty()) {
      return false;
    }

    std::vector<size_t> receiverIndices;
    if (hasNamedArguments(expr.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        for (size_t i = 0; i < expr.args.size(); ++i) {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      if (!resolveMapTarget(expr.args[receiverIndex])) {
        continue;
      }
      const std::string canonical = "/std/collections/map/" + helperName;
      const std::string alias = "/map/" + helperName;
      if (defMap_.count(canonical) > 0) {
        resolved = canonical;
      } else if (defMap_.count(alias) > 0) {
        resolved = alias;
      } else {
        resolved = canonical;
      }
      return true;
    }
    return false;
  };

  auto remapWrappedMapMethodBodyArgumentTarget = [&]() -> bool {
    auto isWrappedMapReceiverCall = [&](const Expr &receiverExpr) {
      if (receiverExpr.kind != Expr::Kind::Call) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(receiverExpr));
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return returnsMapCollectionType(transform.templateArgs.front());
      }
      return false;
    };

    if (!expr.isMethodCall || expr.args.empty() ||
        !(resolveMapTarget(expr.args.front()) || isWrappedMapReceiverCall(expr.args.front()))) {
      return false;
    }

    std::string helperName;
    if (resolved == "/Reference/count" || resolved == "/Pointer/count") {
      helperName = "count";
    } else if (resolved == "/Reference/at" || resolved == "/Pointer/at") {
      helperName = "at";
    } else if (resolved == "/Reference/at_unsafe" || resolved == "/Pointer/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return false;
    }

    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (defMap_.count(canonical) > 0) {
      resolved = canonical;
    } else if (defMap_.count(alias) > 0) {
      resolved = alias;
    } else {
      resolved = canonical;
    }
    resolvedMethod = false;
    return true;
  };

  if (!resolvedMethod) {
    if (!resolveBareMapCallBodyArgumentTarget() && !remapWrappedMapMethodBodyArgumentTarget() &&
        !shouldPreserveBodyArgumentTarget(resolved)) {
      resolved = preferVectorStdlibHelperPath(resolved);
    }
  } else {
    remapWrappedMapMethodBodyArgumentTarget();
  }

  if (resolvedMethod || defMap_.find(resolved) == defMap_.end()) {
    error_ = "block arguments require a definition target: " + resolved;
    return false;
  }

  std::unordered_map<std::string, BindingInfo> blockLocals = locals;
  std::vector<Expr> livenessStatements = expr.bodyArguments;
  if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
    for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
      livenessStatements.push_back((*enclosingStatements)[idx]);
    }
  }
  OnErrorScope onErrorScope(*this, std::nullopt);
  BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
  for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size(); ++bodyIndex) {
    const Expr &bodyExpr = expr.bodyArguments[bodyIndex];
    if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown, false, true, nullptr,
                           expr.namespacePrefix, &expr.bodyArguments, bodyIndex)) {
      return false;
    }
    expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
  }
  return true;
}

} // namespace primec::semantics
