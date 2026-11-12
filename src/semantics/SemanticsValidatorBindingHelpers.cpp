#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorBindingContext.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

// What the binding rules look up while they run: the named binding of a scope, the borrow and
// pointer roots of an expression, and the target-typed sum initializer. They were `std::function` members of
// the binding-state struct; each takes the binding's context explicitly.

namespace {

std::string pointerAliasRootForBinding(const std::string &bindingName, const BindingInfo &binding) {

  std::string referenceRoot = referenceRootForBorrowBinding(bindingName, binding);
  if (!referenceRoot.empty()) {
    return referenceRoot;
  }
  if (binding.typeName == "Pointer" && !binding.referenceRoot.empty()) {
    return binding.referenceRoot;
  }
  return "";
}

} // namespace

bool SemanticsValidator::failBindingDiagnostic(BindingContext &ctx, std::string message) {
  const Expr &stmt = ctx.stmt;

  return failExprDiagnostic(stmt, std::move(message));
}

const BindingInfo *SemanticsValidator::resolveNamedBinding(BindingContext &ctx,
                                                           const std::string &name) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
    return paramBinding;
  }
  auto it = locals.find(name);
  if (it == locals.end()) {
    return nullptr;
  }
  return &it->second;
}

bool SemanticsValidator::resolveStorageRootExpr(BindingContext &ctx,
                                                const Expr &expr,
                                                std::string &rootOut) {

  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = resolveNamedBinding(ctx, expr.name);
    if (binding == nullptr) {
      return false;
    }
    std::string aliasRoot = pointerAliasRootForBinding(expr.name, *binding);
    if (!aliasRoot.empty()) {
      rootOut = std::move(aliasRoot);
    } else {
      rootOut = expr.name;
    }
    return true;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "dereference" &&
      expr.args.size() == 1) {
    return resolvePointerRoot(ctx, expr.args.front(), rootOut);
  }
  if (expr.isFieldAccess && expr.args.size() == 1) {
    std::string receiverRoot;
    if (!resolveStorageRootExpr(ctx, expr.args.front(), receiverRoot) || receiverRoot.empty()) {
      return false;
    }
    rootOut = receiverRoot + "." + expr.name;
    return true;
  }
  return false;
}

bool SemanticsValidator::resolvePointerRoot(BindingContext &ctx,
                                            const Expr &expr,
                                            std::string &rootOut) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;

  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = resolveNamedBinding(ctx, expr.name);
    if (binding == nullptr) {
      return false;
    }
    rootOut = pointerAliasRootForBinding(expr.name, *binding);
    if (rootOut.empty() && binding->typeName == "Pointer") {
      rootOut = expr.name;
    }
    return !rootOut.empty();
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" &&
      expr.args.size() == 1) {
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(ctx, target.name);
      if (binding != nullptr) {
        std::string root = pointerAliasRootForBinding(target.name, *binding);
        if (!root.empty()) {
          rootOut = std::move(root);
        } else {
          rootOut = target.name;
        }
        return true;
      }
      return false;
    }
    return resolvePointerRoot(ctx, target, rootOut);
  }
  if (expr.isFieldAccess && expr.args.size() == 1) {
    std::string receiverRoot;
    if (!resolvePointerRoot(ctx, expr.args.front(), receiverRoot) || receiverRoot.empty()) {
      return false;
    }
    rootOut = receiverRoot + "." + expr.name;
    return true;
  }
  std::string opName;
  if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") &&
      expr.args.size() == 2) {
    if (isPointerLikeExpr(expr.args[1], params, locals)) {
      return false;
    }
    return resolvePointerRoot(ctx, expr.args[0], rootOut);
  }
  std::string resolvedCallPath = preferredCollectionHelperResolvedPath(expr);
  if (resolvedCallPath.empty()) {
    resolvedCallPath = resolveCalleePath(expr);
  }
  if (const std::string concreteResolvedCallPath =
          resolveExprConcreteCallPath(params, locals, expr, resolvedCallPath);
      !concreteResolvedCallPath.empty()) {
    resolvedCallPath = concreteResolvedCallPath;
  }
  const bool isSoaColumnSlotUnsafe = isExperimentalSoaColumnSlotHelperPath(resolvedCallPath);
  const bool isVectorSlotUnsafe =
      resolvedCallPath.rfind(legacyExperimentalVectorCompatibilityPrefix() + std::string("vector") +
                                 "SlotUnsafe",
                             0) == 0;
  auto isCanonicalVectorSlotUnsafe = [](std::string path) {
    const size_t specializationSuffix = path.find("__");
    if (specializationSuffix != std::string::npos) {
      path.erase(specializationSuffix);
    }
    return path.rfind(
               collection_paths::memberPath(collection_paths::kVectorFolder, "vectorSlotUnsafe"),
               0) == 0;
  };
  if ((isSoaColumnSlotUnsafe || isVectorSlotUnsafe ||
       isCanonicalVectorSlotUnsafe(resolvedCallPath)) &&
      !expr.args.empty()) {
    std::string storageRoot;
    if (!resolveStorageRootExpr(ctx, expr.args.front(), storageRoot) || storageRoot.empty()) {
      return false;
    }
    rootOut = storageRoot + ".data";
    return true;
  }
  auto isPointerRootPreservingCall = [](const std::string &name) {
    std::string normalizedName = name;
    if (const auto slash = normalizedName.find_last_of('/'); slash != std::string::npos) {
      normalizedName = normalizedName.substr(slash + 1);
    }
    if (const auto generatedSuffix = normalizedName.find("__");
        generatedSuffix != std::string::npos) {
      normalizedName = normalizedName.substr(0, generatedSuffix);
    }
    return normalizedName == "at" || normalizedName == "at_unsafe" ||
           normalizedName == "reinterpret" || normalizedName == "bufferOffsetUnsafe" ||
           normalizedName == "bufferOffsetChecked" || normalizedName == "bufferReinterpret" ||
           normalizedName == "bufferReinterpretBytes" ||
           normalizedName == "bufferOffsetBytesUnsafe" ||
           normalizedName == "bufferOffsetBytesChecked" ||
           normalizedName == "bufferReinterpretFromBytes";
  };
  if (isPointerRootPreservingCall(resolvedCallPath) && !expr.args.empty()) {
    return resolvePointerRoot(ctx, expr.args.front(), rootOut);
  }
  auto defIt = defMap_.find(resolvedCallPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  bool returnsPointer = false;
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(
            normalizeBindingTypeName(transform.templateArgs.front()), base, arg)) {
      continue;
    }
    if (normalizeBindingTypeName(base) == "Pointer" && !arg.empty()) {
      returnsPointer = true;
      break;
    }
  }
  if (!returnsPointer) {
    return false;
  }
  const auto paramsIt = paramsByDef_.find(resolvedCallPath);
  if (paramsIt == paramsByDef_.end()) {
    return false;
  }
  const auto &nestedParams = paramsIt->second;
  std::string nestedArgError;
  std::vector<const Expr *> nestedOrderedArgs;
  if (!buildOrderedArguments(
          nestedParams, expr.args, expr.argNames, nestedOrderedArgs, nestedArgError)) {
    return false;
  }
  const Expr *returnedValueExpr = nullptr;
  const Definition &nestedDef = *defIt->second;
  for (const auto &stmtExpr : nestedDef.statements) {
    if (isReturnCall(stmtExpr) && stmtExpr.args.size() == 1) {
      returnedValueExpr = &stmtExpr.args.front();
    }
  }
  if (nestedDef.returnExpr.has_value()) {
    returnedValueExpr = &*nestedDef.returnExpr;
  }
  if (returnedValueExpr == nullptr) {
    return false;
  }
  auto resolveNestedArgPointerRoot = [&](const Expr &nestedArg, std::string &nestedRootOut) {
    if (nestedArg.kind == Expr::Kind::Name) {
      for (size_t index = 0; index < nestedParams.size() && index < nestedOrderedArgs.size();
           ++index) {
        if (nestedParams[index].name == nestedArg.name && nestedOrderedArgs[index] != nullptr) {
          return resolvePointerRoot(ctx, *nestedOrderedArgs[index], nestedRootOut);
        }
      }
    }
    return resolvePointerRoot(ctx, nestedArg, nestedRootOut);
  };
  if (returnedValueExpr->kind == Expr::Kind::Name) {
    return resolveNestedArgPointerRoot(*returnedValueExpr, rootOut);
  }
  if (returnedValueExpr->kind != Expr::Kind::Call) {
    return false;
  }
  std::string returnedOpName;
  if (getBuiltinOperatorName(*returnedValueExpr, returnedOpName) &&
      (returnedOpName == "plus" || returnedOpName == "minus") &&
      returnedValueExpr->args.size() == 2) {
    return resolveNestedArgPointerRoot(returnedValueExpr->args.front(), rootOut);
  }
  std::string returnedCallPath = preferredCollectionHelperResolvedPath(*returnedValueExpr);
  if (returnedCallPath.empty()) {
    returnedCallPath = resolveCalleePath(*returnedValueExpr);
  }
  if (isPointerRootPreservingCall(returnedCallPath) && !returnedValueExpr->args.empty()) {
    return resolveNestedArgPointerRoot(returnedValueExpr->args.front(), rootOut);
  }
  return false;
}

bool SemanticsValidator::isStandaloneSoaFieldViewInitializer(BindingContext &ctx) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &initializer = *ctx.initializer;

  if (const auto pendingPath = builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
    std::string pendingFieldName;
    if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
      return true;
    }
  }
  if (initializer.kind == Expr::Kind::Call) {
    std::string resolvedPath = preferredCollectionHelperResolvedPath(initializer);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(initializer);
    }
    if (isExperimentalSoaFieldViewHelperPath(resolvedPath)) {
      return true;
    }
  }
  return false;
}

std::optional<bool>
SemanticsValidator::validateAndRecordTargetTypedSumInitializer(BindingContext &ctx) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  auto &definitionTemplateArgs = ctx.definitionTemplateArgs;
  auto &info = ctx.info;
  auto &restrictType = ctx.restrictType;
  auto &hasExplicitType = ctx.hasExplicitType;
  auto &explicitAutoType = ctx.explicitAutoType;
  const Expr &initializer = *ctx.initializer;

  if (!hasExplicitType || explicitAutoType) {
    return std::nullopt;
  }
  bool handledSumInitializer = false;
  if (!validateTargetTypedSumInitializer(expectedBindingTypeText(info),
                                         initializer,
                                         params,
                                         locals,
                                         namespacePrefix,
                                         handledSumInitializer)) {
    return false;
  }
  if (!handledSumInitializer) {
    return std::nullopt;
  }
  if (restrictType.has_value()) {
    const bool hasTemplate = !info.typeTemplateArg.empty();
    if (!restrictMatchesBinding(
            *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
      return failBindingDiagnostic(ctx, "restrict type does not match binding type");
    }
  }
  if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
    return false;
  }
  insertLocalBinding(locals, stmt.name, std::move(info));
  return true;
}

bool SemanticsValidator::isTargetTypedSumInitializerSyntax(BindingContext &ctx) {
  const std::string &namespacePrefix = ctx.namespacePrefix;
  auto &info = ctx.info;
  auto &hasExplicitType = ctx.hasExplicitType;
  auto &explicitAutoType = ctx.explicitAutoType;
  const Expr &initializer = *ctx.initializer;

  if (initializer.kind == Expr::Kind::Call && isSimpleCallName(initializer, "move")) {
    return false;
  }
  if (initializer.kind == Expr::Kind::Name) {
    return hasExplicitType && !explicitAutoType &&
           resolveSumDefinitionForTypeText(expectedBindingTypeText(info), namespacePrefix) !=
               nullptr;
  }
  if (initializer.kind != Expr::Kind::Call) {
    return false;
  }
  if (initializer.isBraceConstructor || isEmptyBuiltinBlockInitializer(initializer)) {
    return true;
  }
  return hasExplicitType && !explicitAutoType && !initializer.isMethodCall &&
         !initializer.isFieldAccess && initializer.name == "block" &&
         hasNamedArguments(initializer.argNames) &&
         resolveSumDefinitionForTypeText(expectedBindingTypeText(info), namespacePrefix) != nullptr;
}

// An empty `block{}`-style call: no arguments, template arguments, names or body statements.
bool SemanticsValidator::isEmptyBuiltinBlockInitializer(const Expr &candidate) {
  if (!candidate.hasBodyArguments || !candidate.bodyArguments.empty()) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() ||
      hasNamedArguments(candidate.argNames)) {
    return false;
  }
  return isBuiltinBlockCall(candidate);
}

} // namespace primec::semantics
