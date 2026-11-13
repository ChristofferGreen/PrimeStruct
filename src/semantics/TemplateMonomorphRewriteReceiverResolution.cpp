#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/support/Ascii.h"
#include "primec/frontend/StringLiteral.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphCallableRewrites.h"
#include "TemplateMonomorphTextViewConversions.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include <sstream>
#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"
#include "TemplateMonomorphExpressionReferenceRules.h"
#include "TemplateMonomorphExpressionRewriteContext.h"

namespace primec {

// What a collection helper call's receiver is (TemplateMonomorph rewriteExpr): builtin key-value, vector and
// SoA receivers and their template arguments. Each takes the expression's context explicitly.

const Expr *collectionHelperReceiverExpr([[maybe_unused]] ExprRewriteContext &rc,
                                         const Expr &candidate) {

  if (candidate.isMethodCall) {
    return candidate.args.empty() ? nullptr : &candidate.args.front();
  }
  if (candidate.args.empty()) {
    return nullptr;
  }
  if (semantics::hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return &candidate.args[i];
      }
    }
  }
  return &candidate.args.front();
}

Expr *mutableCollectionHelperReceiverExpr([[maybe_unused]] ExprRewriteContext &rc,
                                          Expr &candidate) {

  if (candidate.isMethodCall) {
    return candidate.args.empty() ? nullptr : &candidate.args.front();
  }
  if (candidate.args.empty()) {
    return nullptr;
  }
  if (semantics::hasNamedArguments(candidate.argNames)) {
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return &candidate.args[i];
      }
    }
  }
  return &candidate.args.front();
}

bool resolvesBuiltinKeyValueReceiver(ExprRewriteContext &rc, const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (receiverExpr == nullptr) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
    std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
    if ((receiverType == "Reference" || receiverType == "Pointer") &&
        !receiverInfo.typeTemplateArg.empty()) {
      std::string innerBase;
      std::string innerArgText;
      if (semantics::splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
        receiverType = normalizeCollectionReceiverTypeName(innerBase);
      }
    }
    if (receiverType == "map") {
      return true;
    }
  }
  const std::string inferredReceiverType = inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  if (inferredReceiverType.empty()) {
    return false;
  }
  std::string receiverBase;
  std::string receiverArgText;
  if (semantics::splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
    return normalizeCollectionReceiverTypeName(receiverBase) == "map";
  }
  return normalizeCollectionReceiverTypeName(inferredReceiverType) == "map";
}

bool resolvesBuiltinVectorReceiver(ExprRewriteContext &rc, const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (receiverExpr == nullptr) {
    return false;
  }
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
    std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
    if ((receiverType == "Reference" || receiverType == "Pointer") &&
        !receiverInfo.typeTemplateArg.empty()) {
      std::string innerBase;
      std::string innerArgText;
      if (semantics::splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
        receiverType = normalizeCollectionReceiverTypeName(innerBase);
      }
    }
    if (receiverType == "vector") {
      return true;
    }
  }
  const std::string inferredReceiverType = inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  if (inferredReceiverType.empty()) {
    return false;
  }
  std::string receiverBase;
  std::string receiverArgText;
  if (semantics::splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
    return normalizeCollectionReceiverTypeName(receiverBase) == "vector";
  }
  return normalizeCollectionReceiverTypeName(inferredReceiverType) == "vector";
}

std::string inferCollectionReceiverFamilyForRewrite(ExprRewriteContext &rc,
                                                    const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (receiverExpr == nullptr) {
    return std::string{};
  }
  auto familyFromBinding = [](const semantics::BindingInfo &binding) {
    std::string typeText = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeText += "<" + binding.typeTemplateArg + ">";
    }
    std::string base;
    std::string argText;
    if (semantics::splitTemplateTypeName(typeText, base, argText) &&
        (semantics::normalizeBindingTypeName(base) == "Reference" ||
         semantics::normalizeBindingTypeName(base) == "Pointer")) {
      std::vector<std::string> args;
      if (semantics::splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
        typeText = trimAscii(args.front());
      }
    }
    return normalizeCollectionReceiverTypeName(typeText);
  };
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
    return familyFromBinding(receiverInfo);
  }
  const std::string inferredReceiverType = inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  if (!inferredReceiverType.empty()) {
    return normalizeCollectionReceiverTypeName(inferredReceiverType);
  }
  return std::string{};
}

bool resolvesSoaReceiverForRewrite(ExprRewriteContext &rc, const Expr &receiverExpr) {

  // [&](auto &&...args) { return inferCollectionReceiverFamilyForRewrite(rc, std::forward<decltype(args)>(args)...); } normalizes a builtin `soa<T>`
  // receiver's family to the plain "soa" spelling (the public/current
  // type name - see normalizeCollectionReceiverTypeName), not the
  // "soa_vector" legacy internal name isTemplateMonomorphSoaReceiverType
  // checks against. Accept both here rather than widening
  // normalizeCollectionReceiverTypeName itself, which many other,
  // unrelated same-path-shadow/precedence call sites also depend on.
  const std::string receiverFamily = inferCollectionReceiverFamilyForRewrite(rc, &receiverExpr);
  return isTemplateMonomorphSoaReceiverType(receiverFamily) || receiverFamily == "soa" ||
         resolvesExperimentalSoaReceiverForRewrite(rc, receiverExpr);
}

bool resolveExperimentalSoaVectorReceiverTemplateArgs(ExprRewriteContext &rc,
                                                      const Expr *receiverExpr,
                                                      std::vector<std::string> &templateArgsOut) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  templateArgsOut.clear();
  if (receiverExpr == nullptr) {
    return false;
  }
  auto inferFromTypeText = [&](std::string receiverTypeText) {
    if (receiverTypeText.empty()) {
      return false;
    }
    receiverTypeText = semantics::normalizeBindingTypeName(receiverTypeText);
    while (true) {
      std::string base;
      std::string argText;
      if (!semantics::splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
        break;
      }
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        std::vector<std::string> wrappedArgs;
        if (!semantics::splitTopLevelTemplateArgs(argText, wrappedArgs) ||
            wrappedArgs.size() != 1) {
          return false;
        }
        receiverTypeText = semantics::normalizeBindingTypeName(wrappedArgs.front());
        continue;
      }
      if ((isTemplateMonomorphSoaReceiverType(
               normalizeCollectionReceiverTypeName(normalizedBase)) ||
           semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedBase)) &&
          !argText.empty()) {
        return semantics::splitTopLevelTemplateArgs(argText, templateArgsOut) &&
               templateArgsOut.size() == 1;
      }
      return false;
    }
    std::string resolvedPath = receiverTypeText;
    if (!resolvedPath.empty() && resolvedPath.front() != '/') {
      resolvedPath.insert(resolvedPath.begin(), '/');
    }
    const std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
    if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
      return false;
    }
    for (const auto &[cacheKey, specializedPath] : ctx.specializationCache) {
      if (semantics::normalizeBindingTypeName(specializedPath) !=
          semantics::normalizeBindingTypeName(resolvedPath)) {
        continue;
      }
      std::string base;
      std::string argText;
      if (!semantics::splitTemplateTypeName(cacheKey, base, argText) || base.empty()) {
        continue;
      }
      const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
      if (!isTemplateMonomorphSoaReceiverType(normalizedBase)) {
        continue;
      }
      if (!semantics::splitTopLevelTemplateArgs(argText, templateArgsOut) ||
          templateArgsOut.size() != 1) {
        return false;
      }
      for (std::string &arg : templateArgsOut) {
        arg = stripMangledTemplateArgKindPrefix(std::move(arg));
      }
      return true;
    }
    return false;
  };
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      inferFromTypeText(bindingTypeToString(receiverInfo))) {
    return true;
  }
  if (receiverExpr->kind == Expr::Kind::Call && !receiverExpr->isBinding) {
    std::vector<std::string> receiverCandidatePaths;
    auto appendReceiverCandidatePath = [&](const std::string &candidatePath) {
      if (!candidatePath.empty() &&
          std::find(receiverCandidatePaths.begin(), receiverCandidatePaths.end(), candidatePath) ==
              receiverCandidatePaths.end()) {
        receiverCandidatePaths.push_back(candidatePath);
      }
    };
    std::string resolvedReceiverPath;
    if (receiverExpr->isMethodCall) {
      if (resolveMethodCallTemplateTarget(*receiverExpr, locals, ctx, resolvedReceiverPath)) {
        appendReceiverCandidatePath(resolvedReceiverPath);
      }
    } else {
      appendReceiverCandidatePath(resolveCalleePath(*receiverExpr, namespacePrefix, ctx));
      if (!receiverExpr->name.empty() && receiverExpr->name.front() == '/') {
        appendReceiverCandidatePath(receiverExpr->name);
      } else {
        if (!receiverExpr->namespacePrefix.empty()) {
          appendReceiverCandidatePath(receiverExpr->namespacePrefix + "/" + receiverExpr->name);
        }
        appendReceiverCandidatePath("/" + receiverExpr->name);
        appendReceiverCandidatePath(receiverExpr->name);
      }
    }
    for (const std::string &receiverCandidatePath : receiverCandidatePaths) {
      auto defIt = ctx.sourceDefs.find(receiverCandidatePath);
      if (defIt == ctx.sourceDefs.end()) {
        continue;
      }
      semantics::BindingInfo inferredReturn;
      if (inferDefinitionReturnBindingForTemplatedFallback(
              defIt->second, allowMathBare, ctx, inferredReturn) &&
          inferFromTypeText(bindingTypeToString(inferredReturn))) {
        return true;
      }
    }
  }
  return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
}

bool resolvesExperimentalSoaVectorReceiver(ExprRewriteContext &rc, const Expr *receiverExpr) {

  std::vector<std::string> receiverTemplateArgs;
  return resolveExperimentalSoaVectorReceiverTemplateArgs(rc, receiverExpr, receiverTemplateArgs);
}

bool resolvesBorrowedExperimentalSoaVectorReceiver(ExprRewriteContext &rc,
                                                   const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (receiverExpr == nullptr) {
    return false;
  }
  auto inferFromTypeText = [&](std::string receiverTypeText) {
    if (receiverTypeText.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(
            semantics::normalizeBindingTypeName(receiverTypeText), base, argText)) {
      return false;
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if ((normalizedBase != "Reference" && normalizedBase != "Pointer") || argText.empty()) {
      return false;
    }
    std::vector<std::string> wrappedArgs;
    if (!semantics::splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
      return false;
    }
    std::vector<std::string> receiverTemplateArgs;
    return extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(
        wrappedArgs.front(), ctx, receiverTemplateArgs);
  };
  auto definitionReturnsBorrowedExperimentalSoaVector = [&](const Definition &definition) {
    semantics::BindingInfo inferredReturn;
    if (inferDefinitionReturnBindingForTemplatedFallback(
            definition, allowMathBare, ctx, inferredReturn) &&
        inferFromTypeText(bindingTypeToString(inferredReturn))) {
      return true;
    }
    for (const auto &transform : definition.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          inferFromTypeText(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  };
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      inferFromTypeText(bindingTypeToString(receiverInfo))) {
    return true;
  }
  if (receiverExpr->kind == Expr::Kind::Call && !receiverExpr->isBinding) {
    std::vector<std::string> receiverCandidatePaths;
    auto appendReceiverCandidatePath = [&](const std::string &candidatePath) {
      if (!candidatePath.empty() &&
          std::find(receiverCandidatePaths.begin(), receiverCandidatePaths.end(), candidatePath) ==
              receiverCandidatePaths.end()) {
        receiverCandidatePaths.push_back(candidatePath);
      }
    };
    std::string resolvedReceiverPath;
    if (receiverExpr->isMethodCall) {
      if (resolveMethodCallTemplateTarget(*receiverExpr, locals, ctx, resolvedReceiverPath)) {
        appendReceiverCandidatePath(resolvedReceiverPath);
      }
    } else {
      appendReceiverCandidatePath(resolveCalleePath(*receiverExpr, namespacePrefix, ctx));
      if (!receiverExpr->name.empty() && receiverExpr->name.front() == '/') {
        appendReceiverCandidatePath(receiverExpr->name);
      } else {
        if (!receiverExpr->namespacePrefix.empty()) {
          appendReceiverCandidatePath(receiverExpr->namespacePrefix + "/" + receiverExpr->name);
        }
        appendReceiverCandidatePath("/" + receiverExpr->name);
        appendReceiverCandidatePath(receiverExpr->name);
      }
    }
    for (const std::string &receiverCandidatePath : receiverCandidatePaths) {
      auto defIt = ctx.sourceDefs.find(receiverCandidatePath);
      if (defIt == ctx.sourceDefs.end()) {
        continue;
      }
      if (definitionReturnsBorrowedExperimentalSoaVector(defIt->second)) {
        return true;
      }
    }
    if (!receiverExpr->name.empty()) {
      for (const auto &[defPath, definition] : ctx.sourceDefs) {
        const size_t slash = defPath.find_last_of('/');
        const std::string leaf = slash == std::string::npos ? defPath : defPath.substr(slash + 1);
        if (leaf == receiverExpr->name &&
            definitionReturnsBorrowedExperimentalSoaVector(definition)) {
          return true;
        }
      }
    }
  }
  return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
}

bool resolvesConcreteExperimentalSoaVectorReceiver(ExprRewriteContext &rc,
                                                   const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (receiverExpr == nullptr) {
    return false;
  }
  auto inferFromTypeText = [&](std::string receiverTypeText) {
    if (receiverTypeText.empty()) {
      return false;
    }
    receiverTypeText = semantics::normalizeBindingTypeName(receiverTypeText);
    while (true) {
      std::string base;
      std::string argText;
      if (!semantics::splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
        break;
      }
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        std::vector<std::string> wrappedArgs;
        if (!semantics::splitTopLevelTemplateArgs(argText, wrappedArgs) ||
            wrappedArgs.size() != 1) {
          return false;
        }
        receiverTypeText = semantics::normalizeBindingTypeName(wrappedArgs.front());
        continue;
      }
      return semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedBase);
    }
    if (!receiverTypeText.empty() && receiverTypeText.front() != '/') {
      receiverTypeText.insert(receiverTypeText.begin(), '/');
    }
    return semantics::isExperimentalSoaVectorSpecializedTypePath(
        semantics::normalizeBindingTypeName(receiverTypeText));
  };
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      inferFromTypeText(bindingTypeToString(receiverInfo))) {
    return true;
  }
  return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
}

std::string inferCollectionReceiverFamily(ExprRewriteContext &rc, const Expr *receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  auto inferFromTypeText = [&](std::string receiverTypeText) -> std::string {
    if (receiverTypeText.empty()) {
      return std::string{};
    }
    while (true) {
      std::string base;
      std::string argText;
      if (!semantics::splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
        return normalizeCollectionReceiverTypeName(receiverTypeText);
      }
      const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
      if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
        return normalizedBase;
      }
      std::vector<std::string> receiverArgs;
      if (!semantics::splitTopLevelTemplateArgs(argText, receiverArgs) ||
          receiverArgs.size() != 1) {
        return {};
      }
      receiverTypeText = receiverArgs.front();
    }
  };
  if (receiverExpr == nullptr) {
    return {};
  }
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
    const std::string family = inferFromTypeText(bindingTypeToString(receiverInfo));
    if (!family.empty()) {
      return family;
    }
  }
  return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
      *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
}

bool resolvesExperimentalSoaReceiverForRewrite(ExprRewriteContext &rc, const Expr &receiverExpr) {
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  auto matchesExperimentalSoaType = [](std::string typeText) {
    typeText = semantics::normalizeBindingTypeName(typeText);
    while (!typeText.empty()) {
      std::string base;
      std::string argText;
      if (!semantics::splitTemplateTypeName(typeText, base, argText) || base.empty()) {
        return semantics::isExperimentalSoaVectorTypePath(typeText);
      }
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (semantics::isExperimentalSoaVectorTypePath(normalizedBase)) {
        return true;
      }
      if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
        return false;
      }
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      typeText = semantics::normalizeBindingTypeName(args.front());
    }
    return false;
  };
  semantics::BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(
          receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      matchesExperimentalSoaType(bindingTypeToString(receiverInfo))) {
    return true;
  }
  return matchesExperimentalSoaType(inferExprTypeTextForTemplatedVectorFallback(
      receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
}

} // namespace primec
