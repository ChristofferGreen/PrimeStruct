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

// The rule that settles the borrow, pointer and restrict facts of a binding.

PhaseStatus SemanticsValidator::resolveBindingStorageRoots(BindingContext &ctx) {
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  auto &definitionTemplateArgs = ctx.definitionTemplateArgs;
  auto &info = ctx.info;
  auto &restrictType = ctx.restrictType;
  auto &entryArgInit = ctx.entryArgInit;
  auto &entryArgStringInit = ctx.entryArgStringInit;
  const Expr &initializer = *ctx.initializer;
  if (info.typeName == "uninitialized") {
    if (info.typeTemplateArg.empty()) {
      return ctx.done(
          failBindingDiagnostic(ctx, "uninitialized requires exactly one template argument"));
    }
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
      return ctx.done(failBindingDiagnostic(
          ctx, "uninitialized bindings require uninitialized<T>() initializer"));
    }
    if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
      return ctx.done(failBindingDiagnostic(
          ctx, "uninitialized bindings require uninitialized<T>() initializer"));
    }
    if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() ||
        !initializer.args.empty()) {
      return ctx.done(failBindingDiagnostic(ctx, "uninitialized does not accept arguments"));
    }
    if (initializer.templateArgs.size() != 1 ||
        !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
      return ctx.done(failBindingDiagnostic(ctx, "uninitialized initializer type mismatch"));
    }
  }
  if (restrictType.has_value()) {
    const bool hasTemplate = !info.typeTemplateArg.empty();
    if (!restrictMatchesBinding(
            *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
      return ctx.done(failBindingDiagnostic(ctx, "restrict type does not match binding type"));
    }
  }
  if (entryArgInit || entryArgStringInit) {
    if (normalizeBindingTypeName(info.typeName) != "string") {
      return ctx.done(failBindingDiagnostic(ctx, "entry argument strings require string bindings"));
    }
    info.isEntryArgString = true;
  }
  if (isExperimentalSoaColumnBindingType(info) && info.referenceRoot.empty()) {
    std::string storageRoot;
    if (resolveStorageRootExpr(ctx, initializer, storageRoot) && !storageRoot.empty()) {
      info.referenceRoot = std::move(storageRoot);
    }
  }
  if (info.typeName == "Pointer") {
    std::string pointerRoot;
    if (resolvePointerRoot(ctx, initializer, pointerRoot)) {
      info.referenceRoot = std::move(pointerRoot);
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
