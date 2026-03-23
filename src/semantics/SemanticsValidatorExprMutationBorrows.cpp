#include "SemanticsValidator.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprMutationBorrowBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  auto isMutableBinding = [&](const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding->isMutable;
    }
    auto it = locals.find(name);
    return it != locals.end() && it->second.isMutable;
  };
  const BuiltinCollectionDispatchResolverAdapters
      builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(
          params, locals, builtinCollectionDispatchResolverAdapters);
  auto isVectorOrArrayIndexedTarget = [&](const Expr &target) -> bool {
    auto bindingTargetsVectorOrArray = [&](const BindingInfo &binding,
                                           auto &&bindingTargetsVectorOrArrayRef)
        -> bool {
      const std::string normalizedTypeName =
          normalizeBindingTypeName(binding.typeName);
      if ((normalizedTypeName == "vector" || normalizedTypeName == "array") &&
          !binding.typeTemplateArg.empty()) {
        return true;
      }
      if ((normalizedTypeName != "Reference" &&
           normalizedTypeName != "Pointer") ||
          binding.typeTemplateArg.empty()) {
        return false;
      }
      BindingInfo pointeeBinding;
      const std::string normalizedPointeeType =
          normalizeBindingTypeName(binding.typeTemplateArg);
      std::string pointeeBase;
      std::string pointeeArgs;
      if (splitTemplateTypeName(normalizedPointeeType, pointeeBase,
                                pointeeArgs)) {
        pointeeBinding.typeName = normalizeBindingTypeName(pointeeBase);
        pointeeBinding.typeTemplateArg = pointeeArgs;
      } else {
        pointeeBinding.typeName = normalizedPointeeType;
        pointeeBinding.typeTemplateArg.clear();
      }
      return bindingTargetsVectorOrArrayRef(
          pointeeBinding, bindingTargetsVectorOrArrayRef);
    };
    std::string elemType;
    if (builtinCollectionDispatchResolvers.resolveVectorTarget(target, elemType) ||
        builtinCollectionDispatchResolvers.resolveArrayTarget(target, elemType)) {
      return true;
    }
    if (target.kind == Expr::Kind::Call && target.isFieldAccess &&
        target.args.size() == 1) {
      BindingInfo fieldBinding;
      if (resolveStructFieldBinding(params, locals, target.args.front(),
                                    target.name, fieldBinding) &&
          bindingTargetsVectorOrArray(fieldBinding,
                                      bindingTargetsVectorOrArray)) {
        return true;
      }
    }
    std::string collectionTypePath;
    return resolveCallCollectionTypePath(target, params, locals,
                                         collectionTypePath) &&
           (collectionTypePath == "/vector" || collectionTypePath == "/array");
  };

  auto hasActiveBorrowForBinding =
      [&](const std::string &name,
          const std::string &ignoreBorrowName = std::string()) -> bool {
    if (currentValidationContext_.definitionIsUnsafe) {
      return false;
    }
    auto referenceRootForBinding =
        [](const std::string &bindingName,
           const BindingInfo &binding) -> std::string {
      if (binding.typeName != "Reference") {
        return "";
      }
      if (!binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return bindingName;
    };
    auto hasBorrowFrom =
        [&](const std::string &borrowName, const BindingInfo &binding) -> bool {
      if (borrowName == name) {
        return false;
      }
      if (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) {
        return false;
      }
      if (currentValidationContext_.endedReferenceBorrows.count(borrowName) >
          0) {
        return false;
      }
      const std::string root = referenceRootForBinding(borrowName, binding);
      return !root.empty() && root == name;
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
  auto formatBorrowedBindingError =
      [&](const std::string &borrowRoot, const std::string &sinkName) {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        error_ = "borrowed binding: " + borrowRoot + " (root: " +
                 borrowRoot + ", sink: " + sink + ")";
      };
  auto findNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto itBinding = locals.find(name);
    if (itBinding != locals.end()) {
      return &itBinding->second;
    }
    return nullptr;
  };
  std::function<bool(const Expr &, std::string &)>
      resolveLocationRootBindingName;
  resolveLocationRootBindingName =
      [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string pointerBuiltinName;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) &&
        pointerBuiltinName == "location" && pointerExpr.args.size() == 1) {
      if (pointerExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      rootNameOut = pointerExpr.args.front().name;
      return true;
    }
    std::string opName;
    if (!getBuiltinOperatorName(pointerExpr, opName) ||
        (opName != "plus" && opName != "minus") ||
        pointerExpr.args.size() != 2) {
      return false;
    }
    if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
      return false;
    }
    return resolveLocationRootBindingName(pointerExpr.args.front(),
                                          rootNameOut);
  };
  std::function<bool(const Expr &, std::string &, std::string &)>
      resolveMutablePointerWriteTarget;
  resolveMutablePointerWriteTarget = [&](const Expr &pointerExpr,
                                         std::string &borrowRootOut,
                                         std::string &ignoreBorrowNameOut)
      -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      const BindingInfo *pointerBinding = findNamedBinding(pointerExpr.name);
      if (!pointerBinding || !isPointerLikeExpr(pointerExpr, params, locals)) {
        return false;
      }
      ignoreBorrowNameOut = pointerExpr.name;
      if (!pointerBinding->referenceRoot.empty()) {
        borrowRootOut = pointerBinding->referenceRoot;
        return isMutableBinding(pointerBinding->referenceRoot);
      }
      return isMutableBinding(pointerExpr.name);
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string pointerBuiltinName;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) &&
        pointerBuiltinName == "location" && pointerExpr.args.size() == 1) {
      const Expr &locationTarget = pointerExpr.args.front();
      if (locationTarget.kind != Expr::Kind::Name ||
          !isMutableBinding(locationTarget.name)) {
        return false;
      }
      const BindingInfo *targetBinding = findNamedBinding(locationTarget.name);
      if (targetBinding == nullptr) {
        return false;
      }
      if (targetBinding->typeName == "Reference") {
        ignoreBorrowNameOut = locationTarget.name;
        if (!targetBinding->referenceRoot.empty()) {
          borrowRootOut = targetBinding->referenceRoot;
        } else {
          borrowRootOut = locationTarget.name;
        }
      } else {
        borrowRootOut = locationTarget.name;
      }
      return true;
    }
    std::string opName;
    if (!getBuiltinOperatorName(pointerExpr, opName) ||
        (opName != "plus" && opName != "minus") ||
        pointerExpr.args.size() != 2) {
      return false;
    }
    if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
      return false;
    }
    return resolveMutablePointerWriteTarget(pointerExpr.args.front(),
                                            borrowRootOut,
                                            ignoreBorrowNameOut);
  };

  if (isSimpleCallName(expr, "move")) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (expr.isMethodCall) {
      error_ = "move does not support method-call syntax";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "move does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "move does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "move requires exactly one argument";
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      error_ = "move requires a binding name";
      return false;
    }
    const BindingInfo *binding = findNamedBinding(target.name);
    if (!binding) {
      error_ = "move requires a local binding or parameter: " + target.name;
      return false;
    }
    if (binding->typeName == "Reference") {
      error_ = "move does not support Reference bindings: " + target.name;
      return false;
    }
    if (hasActiveBorrowForBinding(target.name)) {
      formatBorrowedBindingError(target.name, target.name);
      return false;
    }
    if (currentValidationContext_.movedBindings.count(target.name) > 0) {
      error_ = "use-after-move: " + target.name;
      return false;
    }
    currentValidationContext_.movedBindings.insert(target.name);
    return true;
  }

  if (isAssignCall(expr)) {
    handledOut = true;
    if (expr.args.size() != 2) {
      error_ = "assign requires exactly two arguments";
      return false;
    }
    const Expr &target = expr.args.front();
    const bool targetIsName = target.kind == Expr::Kind::Name;
    auto isLifecycleHelperPath = [](const std::string &fullPath) -> bool {
      static const std::array<std::string_view, 10> suffixes = {
          "/Create",        "/Destroy",        "/Copy",
          "/Move",          "/CreateStack",    "/DestroyStack",
          "/CreateHeap",    "/DestroyHeap",    "/CreateBuffer",
          "/DestroyBuffer"};
      for (std::string_view suffix : suffixes) {
        if (fullPath.size() < suffix.size()) {
          continue;
        }
        if (fullPath.compare(fullPath.size() - suffix.size(), suffix.size(),
                             suffix.data(), suffix.size()) == 0) {
          return true;
        }
      }
      return false;
    };
    auto isNamedFieldTarget =
        [&](const Expr &candidate, std::string_view rootName) -> bool {
      if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess ||
          candidate.args.size() != 1) {
        return false;
      }
      const Expr *receiver = &candidate.args.front();
      while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess &&
             receiver->args.size() == 1) {
        receiver = &receiver->args.front();
      }
      return receiver->kind == Expr::Kind::Name &&
             receiver->name == rootName;
    };
    auto resolveFieldTargetRootName =
        [&](const Expr &candidate, std::string &rootNameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess ||
          candidate.args.size() != 1) {
        return false;
      }
      const Expr *receiver = &candidate.args.front();
      while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess &&
             receiver->args.size() == 1) {
        receiver = &receiver->args.front();
      }
      if (receiver->kind != Expr::Kind::Name) {
        return false;
      }
      rootNameOut = receiver->name;
      return true;
    };
    auto validateMutableFieldAccessTarget = [&](const Expr &fieldTarget) -> bool {
      if (fieldTarget.kind != Expr::Kind::Call || !fieldTarget.isFieldAccess ||
          fieldTarget.args.size() != 1) {
        error_ = "assign target must be a mutable binding";
        return false;
      }
      if (!validateExpr(params, locals, fieldTarget.args.front())) {
        return false;
      }
      BindingInfo fieldBinding;
      if (!this->resolveStructFieldBinding(params, locals,
                                           fieldTarget.args.front(),
                                           fieldTarget.name, fieldBinding)) {
        if (error_.empty()) {
          error_ = "assign target must be a mutable binding";
        }
        return false;
      }
      std::string fieldTargetRootName;
      const bool allowMutableReceiverFieldWrite =
          resolveFieldTargetRootName(fieldTarget, fieldTargetRootName) &&
          isMutableBinding(fieldTargetRootName);
      const bool allowLifecycleFieldWrite =
          !fieldBinding.isMutable && isNamedFieldTarget(fieldTarget, "this") &&
          isLifecycleHelperPath(currentValidationContext_.definitionPath);
      if (!fieldBinding.isMutable && !allowLifecycleFieldWrite &&
          !allowMutableReceiverFieldWrite) {
        error_ = "assign target must be a mutable binding";
        return false;
      }
      if (fieldTarget.args.front().kind == Expr::Kind::Name &&
          hasActiveBorrowForBinding(fieldTarget.args.front().name)) {
        formatBorrowedBindingError(fieldTarget.args.front().name,
                                   fieldTarget.args.front().name);
        return false;
      }
      return true;
    };
    if (target.kind == Expr::Kind::Name) {
      if (!isMutableBinding(target.name)) {
        error_ = "assign target must be a mutable binding: " + target.name;
        return false;
      }
      if (hasActiveBorrowForBinding(target.name)) {
        formatBorrowedBindingError(target.name, target.name);
        return false;
      }
    } else if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
      if (!validateMutableFieldAccessTarget(target)) {
        return false;
      }
    } else if (target.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) &&
          target.args.size() == 2) {
        const Expr &collectionTarget = target.args.front();
        if (!isVectorOrArrayIndexedTarget(collectionTarget)) {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (collectionTarget.kind == Expr::Kind::Name) {
          if (!isMutableBinding(collectionTarget.name)) {
            error_ = "assign target must be a mutable binding: " +
                     collectionTarget.name;
            return false;
          }
          if (hasActiveBorrowForBinding(collectionTarget.name)) {
            formatBorrowedBindingError(collectionTarget.name,
                                       collectionTarget.name);
            return false;
          }
        } else if (collectionTarget.kind == Expr::Kind::Call &&
                   collectionTarget.isFieldAccess) {
          if (!validateMutableFieldAccessTarget(collectionTarget)) {
            return false;
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (!validateExpr(params, locals, target)) {
          return false;
        }
      } else {
        std::string pointerName;
        if (!getBuiltinPointerName(target, pointerName) ||
            pointerName != "dereference" || target.args.size() != 1) {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        const Expr &pointerExpr = target.args.front();
        if (pointerExpr.kind == Expr::Kind::Name &&
            !isMutableBinding(pointerExpr.name)) {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        std::string pointerBorrowRoot;
        std::string ignoreBorrowName;
        if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot,
                                             ignoreBorrowName)) {
          if (pointerExpr.kind == Expr::Kind::Name &&
              !isMutableBinding(pointerExpr.name)) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          std::string locationRootName;
          if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
              !isMutableBinding(locationRootName)) {
            error_ =
                "assign target must be a mutable binding: " + locationRootName;
            return false;
          }
          error_ = "assign target must be a mutable pointer binding";
          return false;
        }
        if (!pointerBorrowRoot.empty() &&
            hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
          const std::string borrowSink =
              !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
          formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
          return false;
        }
        std::string escapeSink;
        bool hasEscapeSink = false;
        if (pointerExpr.kind == Expr::Kind::Name) {
          hasEscapeSink =
              resolveReferenceEscapeSink(params, locals, pointerExpr.name,
                                         escapeSink);
        }
        if (!hasEscapeSink) {
          std::string locationRootName;
          if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
            hasEscapeSink =
                resolveReferenceEscapeSink(params, locals, locationRootName,
                                           escapeSink);
          }
        }
        if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
          if (const BindingInfo *rootParam =
                  findParamBinding(params, pointerBorrowRoot);
              rootParam != nullptr && rootParam->typeName == "Reference") {
            hasEscapeSink = true;
            escapeSink = pointerBorrowRoot;
          }
        }
        if (currentValidationContext_.definitionIsUnsafe) {
          if (isUnsafeReferenceExpr(params, locals, expr.args[1]) &&
              hasEscapeSink &&
              reportReferenceAssignmentEscape(params, locals, escapeSink,
                                              expr.args[1])) {
            return false;
          }
        } else if (hasEscapeSink &&
                   reportReferenceAssignmentEscape(params, locals, escapeSink,
                                                   expr.args[1])) {
          return false;
        }
        if (!validateExpr(params, locals, pointerExpr)) {
          return false;
        }
      }
    } else {
      error_ = "assign target must be a mutable binding";
      return false;
    }
    if (targetIsName) {
      std::string escapeSink;
      if (resolveReferenceEscapeSink(params, locals, target.name, escapeSink)) {
        if (currentValidationContext_.definitionIsUnsafe) {
          if (isUnsafeReferenceExpr(params, locals, expr.args[1]) &&
              reportReferenceAssignmentEscape(params, locals, escapeSink,
                                              expr.args[1])) {
            return false;
          }
        } else if (reportReferenceAssignmentEscape(params, locals, escapeSink,
                                                   expr.args[1])) {
          return false;
        }
      }
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    if (targetIsName) {
      currentValidationContext_.movedBindings.erase(target.name);
    }
    return true;
  }

  std::string mutateName;
  if (getBuiltinMutationName(expr, mutateName)) {
    handledOut = true;
    if (expr.args.size() != 1) {
      error_ = mutateName + " requires exactly one argument";
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      if (!isMutableBinding(target.name)) {
        error_ = mutateName + " target must be a mutable binding: " +
                 target.name;
        return false;
      }
      if (hasActiveBorrowForBinding(target.name)) {
        formatBorrowedBindingError(target.name, target.name);
        return false;
      }
    } else if (target.kind == Expr::Kind::Call) {
      std::string pointerName;
      if (!getBuiltinPointerName(target, pointerName) ||
          pointerName != "dereference" || target.args.size() != 1) {
        error_ = mutateName + " target must be a mutable binding";
        return false;
      }
      const Expr &pointerExpr = target.args.front();
      std::string pointerBorrowRoot;
      std::string ignoreBorrowName;
      if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot,
                                           ignoreBorrowName)) {
        if (pointerExpr.kind == Expr::Kind::Name &&
            !isMutableBinding(pointerExpr.name)) {
          error_ = mutateName + " target must be a mutable binding";
          return false;
        }
        std::string locationRootName;
        if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
            !isMutableBinding(locationRootName)) {
          error_ = mutateName + " target must be a mutable binding: " +
                   locationRootName;
          return false;
        }
        error_ = mutateName + " target must be a mutable pointer binding";
        return false;
      }
      if (!pointerBorrowRoot.empty() &&
          hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
        const std::string borrowSink =
            !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
        formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
        return false;
      }
      if (!validateExpr(params, locals, pointerExpr)) {
        return false;
      }
    } else {
      error_ = mutateName + " target must be a mutable binding";
      return false;
    }
    if (!isNumericExpr(params, locals, target)) {
      error_ = mutateName + " requires numeric operand";
      return false;
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
