#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
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
#include "TemplateMonomorphExpressionRewriteState.h"
#include "TemplateMonomorphUsings.h"

namespace primec {

bool isCompileTimeTypeofPredicateArg(const std::string &arg) {
  const std::string trimmed = trimWhitespace(arg);
  return trimmed.rfind("typeof<", 0) == 0 && trimmed.size() > 8 &&
         trimmed.back() == '>';
}

bool rewriteCompileTimePredicateExpr(Expr &expr,
                                     const SubstMap &mapping,
                                     const std::unordered_set<std::string> &allowedParams,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     std::string &error) {
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return false;
  }
  for (std::string &templateArg : expr.templateArgs) {
    if (isCompileTimeTypeofPredicateArg(templateArg)) {
      continue;
    }
    ResolvedType resolved =
        resolveTypeString(templateArg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    templateArg = std::move(resolved.text);
  }
  for (Expr &arg : expr.args) {
    if (!rewriteCompileTimePredicateExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error)) {
      return false;
    }
  }
  return true;
}

namespace {

// TODO-4751: the public key/value wrapper struct lives at this path. Calls to
// the canonical key/value helper family (`/std/collections/map/<helper>` and
// its `<helper>_ref` borrowed form) whose receiver is that wrapper (or a
// Reference/Pointer to it) are routed to the wrapper's own methods instead of
// the MapValue-typed free helpers. This deliberately avoids adding wrapper
// overloads to the canonical helper family.
constexpr std::string_view KeyValueWrapperStructPath = collection_helpers::kCanonicalMapMapType;
constexpr std::string_view KeyValueHelperRoot = collection_helpers::kCanonicalMapPrefix;

bool isKeyValueWrapperMethodHelperName(std::string_view name) {
  return name == "count" || name == "contains" || name == "tryAt" ||
         name == "at" || name == "at_unsafe" || name == "insert";
}

bool isKeyValueWrapperStructTypeText(std::string typeText,
                                     const std::string &namespacePrefix,
                                     Context &ctx) {
  typeText = normalizeBindingTypeName(typeText);
  std::string base = typeText;
  std::string argText;
  if (splitTemplateTypeName(typeText, base, argText)) {
    base = normalizeBindingTypeName(base);
  }
  if (base.empty()) {
    return false;
  }
  const std::string wrapperPath(KeyValueWrapperStructPath);
  if (base.front() != '/') {
    base = resolveNameToPath(base,
                             namespacePrefix,
                             scopedImportAliasesForNamespace(namespacePrefix, ctx),
                             ctx.sourceDefs);
  }
  return base == wrapperPath || base.rfind(wrapperPath + "__", 0) == 0;
}

} // namespace

bool rewriteKeyValueWrapperHelperCallToMethod(Expr &expr,
                                              const std::string &namespacePrefix,
                                              Context &ctx,
                                              const LocalTypeMap &locals,
                                              const std::vector<ParameterInfo> &params,
                                              bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      expr.isFieldAccess || expr.args.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
    return false;
  }
  std::string resolved =
      stripCollectionConstructorSuffixes(resolveCalleePath(expr, namespacePrefix, ctx));
  if (resolved.rfind(KeyValueHelperRoot, 0) != 0) {
    return false;
  }
  std::string helperName = resolved.substr(KeyValueHelperRoot.size());
  bool borrowedHelper = false;
  constexpr std::string_view RefSuffix = "_ref";
  if (helperName.size() > RefSuffix.size() &&
      helperName.compare(helperName.size() - RefSuffix.size(), RefSuffix.size(), RefSuffix) == 0) {
    helperName.erase(helperName.size() - RefSuffix.size());
    borrowedHelper = true;
  }
  if (!isKeyValueWrapperMethodHelperName(helperName)) {
    return false;
  }
  BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(
          expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
    return false;
  }
  const std::string receiverBase = normalizeBindingTypeName(receiverInfo.typeName);
  const bool receiverIsBorrowed =
      receiverBase == "Reference" || receiverBase == "Pointer";
  if (borrowedHelper && !receiverIsBorrowed) {
    return false;
  }
  const std::string receiverTypeText =
      receiverIsBorrowed ? receiverInfo.typeTemplateArg : bindingTypeToString(receiverInfo);
  if (!isKeyValueWrapperStructTypeText(receiverTypeText, namespacePrefix, ctx)) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = helperName;
  expr.isMethodCall = true;
  expr.templateArgs.clear();
  expr.templateArgDetails.clear();
  return true;
}

// TODO-5381: `count(dereference(r))` / `dereference(r).count()` where `r` is a
// `Reference<vector<T>>` means the borrowed helper applied to `r` itself;
// unwrap the dereference so the borrowed-vector routing below applies.
void unwrapDereferencedBorrowedVectorReceiver(Expr &expr,
                                              const std::vector<ParameterInfo> &params,
                                              const LocalTypeMap &locals,
                                              bool allowMathBare,
                                              Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isFieldAccess || expr.args.empty() ||
      hasNamedCallArguments(expr) || !expr.templateArgs.empty()) {
    return;
  }
  std::string helperName = expr.name;
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (collection_helpers::borrowedVectorHelperLeaf(helperName).empty()) {
    return;
  }
  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isMethodCall || receiver.isBinding ||
      receiver.args.size() != 1 || !isSimpleCallName(receiver, "dereference")) {
    return;
  }
  BindingInfo pointerInfo;
  if (!inferBindingTypeForMonomorph(receiver.args.front(), params, locals, allowMathBare, ctx,
                                    pointerInfo) ||
      normalizeBindingTypeName(pointerInfo.typeName) != "Reference") {
    return;
  }
  std::string vectorBase;
  std::string elementType;
  if (!splitTemplateTypeName(normalizeBindingTypeName(pointerInfo.typeTemplateArg), vectorBase,
                             elementType) ||
      normalizeBindingTypeName(vectorBase) != "vector") {
    return;
  }
  Expr unwrapped = receiver.args.front();
  expr.args.front() = std::move(unwrapped);
}

// TODO-5375: bare `count(r)` / `push(r, x)` / ... where `r` is a
// `Reference<vector<T>>` routes to the canonical borrowed-vector helper with
// the element type as its template argument (method sugar does the same via
// resolveMethodCallTemplateTarget).
bool rewriteBorrowedVectorBareHelperCall(Expr &expr,
                                         const std::vector<ParameterInfo> &params,
                                         const LocalTypeMap &locals,
                                         bool allowMathBare,
                                         Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || expr.isFieldAccess ||
      expr.args.empty() || !expr.templateArgs.empty() || !expr.namespacePrefix.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() || hasNamedCallArguments(expr)) {
    return false;
  }
  const std::string_view leaf = collection_helpers::borrowedVectorHelperLeaf(expr.name);
  if (leaf.empty() || ctx.sourceDefs.count("/" + expr.name) > 0 ||
      ctx.helperOverloads.count("/" + expr.name) > 0) {
    return false;
  }
  const std::string borrowedPath =
      std::string(collection_helpers::kCanonicalVectorPrefix) + std::string(leaf);
  if (ctx.sourceDefs.count(borrowedPath) == 0 && ctx.helperOverloads.count(borrowedPath) == 0) {
    return false;
  }
  BindingInfo receiverInfo;
  if (!inferBindingTypeForMonomorph(expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo) ||
      normalizeBindingTypeName(receiverInfo.typeName) != "Reference") {
    return false;
  }
  std::string vectorBase;
  std::string elementType;
  if (!splitTemplateTypeName(normalizeBindingTypeName(receiverInfo.typeTemplateArg), vectorBase,
                             elementType) ||
      normalizeBindingTypeName(vectorBase) != "vector" || elementType.empty()) {
    return false;
  }
  if (expr.sourceName.empty()) {
    expr.sourceName = expr.name;
  }
  expr.name = borrowedPath;
  expr.templateArgs = {elementType};
  return true;
}

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare) {
    RewriteExprState st;
    st.allowMathBare = allowMathBare;
    if (rewriteExprPhase1(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase2(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase3(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase4(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase5(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase6(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase7(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (rewriteExprPhase8(expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st) == PhaseStatus::Done) {
      return st.result;
    }
    return st.result;
}

PhaseStatus rewriteExprPhase1([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  expr.namespacePrefix = namespacePrefix;
  st.hadExplicitTemplateArgsOnEntry = !expr.templateArgs.empty();
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return st.done(false);
  }
  if (expr.kind == Expr::Kind::Call && expr.name == "ct_if" &&
      expr.args.size() == 3) {
    if (!rewriteCompileTimePredicateExpr(
            expr.args[0], mapping, allowedParams, namespacePrefix, ctx, error)) {
      return st.done(false);
    }
    for (std::size_t branchIndex = 1; branchIndex < expr.args.size();
         ++branchIndex) {
      LocalTypeMap branchLocals = locals;
      for (Expr &bodyArg : expr.args[branchIndex].bodyArguments) {
        if (!rewriteExpr(bodyArg,
                         mapping,
                         allowedParams,
                         namespacePrefix,
                         ctx,
                         error,
                         branchLocals,
                         params,
                         allowMathBare)) {
          return st.done(false);
        }
      }
    }
    return st.done(true);
  }
  if (expr.kind == Expr::Kind::Name) {
    auto isRuntimeParameter = [&]() {
      for (const ParameterInfo &param : params) {
        if (param.name == expr.name) {
          return true;
        }
      }
      return false;
    };
    auto appendUniquePath = [](std::vector<std::string> &paths,
                               const std::string &path) {
      if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
      }
    };
    auto appendZeroArgCallablePathsForTarget =
        [&](const std::string &targetPath, std::vector<std::string> &paths) {
      auto sourceIt = ctx.sourceDefs.find(targetPath);
      if (sourceIt != ctx.sourceDefs.end()) {
        if (sourceIt->second.parameters.empty() &&
            sourceIt->second.templateArgs.empty() &&
            !isStructDefinition(sourceIt->second)) {
          appendUniquePath(paths, targetPath);
        }
        return;
      }
      auto overloadIt = ctx.helperOverloads.find(targetPath);
      if (overloadIt == ctx.helperOverloads.end()) {
        return;
      }
      for (const HelperOverloadEntry &entry : overloadIt->second) {
        if (entry.parameterCount != 0 || entry.isVariadic) {
          continue;
        }
        auto overloadDefIt = ctx.sourceDefs.find(entry.internalPath);
        if (overloadDefIt == ctx.sourceDefs.end() ||
            !overloadDefIt->second.templateArgs.empty() ||
            isStructDefinition(overloadDefIt->second)) {
          continue;
        }
        appendUniquePath(paths, entry.internalPath);
      }
    };
    auto visibleZeroArgDefinitionPaths = [&]() -> std::vector<std::string> {
      std::vector<std::string> paths;
      if (expr.name.empty() || expr.name.find('/') != std::string::npos) {
        return paths;
      }
      const std::string resolvedPath =
          resolveNameToPath(expr.name,
                            namespacePrefix,
                            scopedImportAliasesForNamespace(namespacePrefix, ctx),
                            ctx.sourceDefs);
      appendZeroArgCallablePathsForTarget(resolvedPath, paths);
      const auto &aliasTargets =
          scopedImportAliasTargetsForNamespace(namespacePrefix, ctx);
      auto aliasIt = aliasTargets.find(expr.name);
      if (aliasIt != aliasTargets.end()) {
        for (const std::string &targetPath : aliasIt->second) {
          appendZeroArgCallablePathsForTarget(targetPath, paths);
        }
      }
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      return paths;
    };
    auto appendPathList = [](std::string &message,
                             const std::vector<std::string> &paths) {
      for (size_t index = 0; index < paths.size(); ++index) {
        if (index == 0) {
          message += paths[index];
        } else if (index + 1 == paths.size()) {
          message += " and " + paths[index];
        } else {
          message += ", " + paths[index];
        }
      }
    };
    std::vector<std::string> zeroArgPaths = visibleZeroArgDefinitionPaths();
    const bool hasLocalValue = isRuntimeParameter() || locals.count(expr.name) > 0;
    if (hasLocalValue && !zeroArgPaths.empty()) {
      error = "ambiguous bare name: " + expr.name +
              " could refer to a local value or ";
      appendPathList(error, zeroArgPaths);
      return st.done(false);
    }
    if (zeroArgPaths.size() > 1) {
      error = "ambiguous bare name: " + expr.name + " could refer to ";
      appendPathList(error, zeroArgPaths);
      return st.done(false);
    }
    if (zeroArgPaths.size() == 1) {
      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = expr.name;
      callExpr.sourceName = expr.sourceName.empty() ? expr.name : expr.sourceName;
      callExpr.namespacePrefix = expr.namespacePrefix;
      callExpr.sourceLine = expr.sourceLine;
      callExpr.sourceColumn = expr.sourceColumn;
      callExpr.semanticNodeId = expr.semanticNodeId;
      expr = std::move(callExpr);
    }
  }
  if (expr.kind != Expr::Kind::Call) {
    return st.done(true);
  }
  if (expr.isLambda) {
    std::unordered_set<std::string> lambdaAllowed = allowedParams;
    for (const auto &param : expr.templateArgs) {
      lambdaAllowed.insert(param);
    }
    SubstMap lambdaMapping = mapping;
    for (const auto &param : expr.templateArgs) {
      lambdaMapping.erase(param);
    }
    LocalTypeMap lambdaLocals = locals;
    lambdaLocals.reserve(lambdaLocals.size() + expr.args.size() + expr.bodyArguments.size());
    for (auto &param : expr.args) {
      if (!rewriteExpr(param, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return st.done(false);
      }
      BindingInfo info;
      if (extractExplicitBindingType(param, info)) {
        if (info.typeName == "auto" && param.args.size() == 1 &&
            inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        } else {
          lambdaLocals[param.name] = info;
        }
      } else if (param.isBinding && param.args.size() == 1) {
        if (inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        }
      }
    }
    for (auto &bodyArg : expr.bodyArguments) {
      if (!rewriteExpr(bodyArg, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return st.done(false);
      }
      BindingInfo info;
      if (extractExplicitBindingType(bodyArg, info)) {
        if (info.typeName == "auto" && bodyArg.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        } else {
          lambdaLocals[bodyArg.name] = info;
        }
      } else if (bodyArg.isBinding && bodyArg.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        }
      }
    }
    return st.done(true);
  }
  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return st.done(false);
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
  }
  st.resolvePickSumDefinition = [&](const Expr &target) -> Definition * {
    BindingInfo targetInfo;
    if (!inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx,
                                      targetInfo)) {
      return nullptr;
    }
    std::string targetTypeText = bindingTypeToString(targetInfo);
    if (targetTypeText.empty()) {
      return nullptr;
    }
    std::string resolveError;
    ResolvedType resolvedType =
        resolveTypeString(targetTypeText, mapping, allowedParams, namespacePrefix,
                          ctx, resolveError);
    if (!resolveError.empty() || resolvedType.text.empty()) {
      return nullptr;
    }
    targetTypeText = normalizeBindingTypeName(resolvedType.text);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(targetTypeText, base, argText)) {
      targetTypeText = normalizeBindingTypeName(base);
    }
    std::string sumPath = targetTypeText;
    if (sumPath.empty()) {
      return nullptr;
    }
    if (sumPath.front() != '/') {
      sumPath = resolveNameToPath(sumPath,
                                  namespacePrefix,
                                  scopedImportAliasesForNamespace(namespacePrefix,
                                                                  ctx),
                                  ctx.sourceDefs);
    }
    auto sumIt = ctx.sourceDefs.find(sumPath);
    if (sumIt == ctx.sourceDefs.end() ||
        !isSumDefinitionForMonomorphRefresh(sumIt->second)) {
      return nullptr;
    }
    return &sumIt->second;
  };
  [[maybe_unused]] auto &resolvePickSumDefinition = st.resolvePickSumDefinition;
  st.appendPickPayloadLocal = [](const SumVariant &variant,
                                   const Expr &arm,
                                   LocalTypeMap &armLocals) {
    if (!variant.hasPayload || arm.args.size() != 1) {
      return;
    }
    const Expr &payloadBinder = arm.args.front();
    if (payloadBinder.kind != Expr::Kind::Name || payloadBinder.name.empty()) {
      return;
    }
    BindingInfo payloadInfo;
    payloadInfo.typeName = variant.payloadType;
    if (!variant.payloadTemplateArgs.empty()) {
      payloadInfo.typeTemplateArg = joinTemplateArgs(variant.payloadTemplateArgs);
    }
    armLocals[payloadBinder.name] = std::move(payloadInfo);
  };
  [[maybe_unused]] auto &appendPickPayloadLocal = st.appendPickPayloadLocal;
  st.recordBodyBindingLocal = [&](Expr &bodyExpr, LocalTypeMap &bodyLocals) {
    BindingInfo info;
    if (extractExplicitBindingType(bodyExpr, info)) {
      if (info.typeName == "auto" && bodyExpr.args.size() == 1 &&
          inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                       params,
                                       bodyLocals,
                                       allowMathBare,
                                       ctx,
                                       info)) {
        bodyLocals[bodyExpr.name] = info;
      } else {
        bodyLocals[bodyExpr.name] = info;
      }
      return;
    }
    if (bodyExpr.isBinding && bodyExpr.args.size() == 1 &&
        inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                     params,
                                     bodyLocals,
                                     allowMathBare,
                                     ctx,
                                     info)) {
      bodyLocals[bodyExpr.name] = info;
    }
  };
  [[maybe_unused]] auto &recordBodyBindingLocal = st.recordBodyBindingLocal;
  {
    auto isStdlibTuplePathEarly = [](const std::string &path) {
      return path == "/std/tuple/tuple" ||
             path.rfind("/std/tuple/tuple__t", 0) == 0;
    };
    auto resolveTupleBasePathEarly = [&](const std::string &base) -> std::string {
      std::string normalizedBase = normalizeBindingTypeName(base);
      if (normalizedBase.empty()) {
        return {};
      }
      if (isStdlibTuplePathEarly(normalizedBase)) {
        return normalizedBase;
      }
      if (!normalizedBase.empty() && normalizedBase.front() == '/') {
        return {};
      }
      return resolveNameToPath(normalizedBase,
                               namespacePrefix,
                               scopedImportAliasesForNamespace(namespacePrefix, ctx),
                               ctx.sourceDefs);
    };
    auto extractTupleReceiverTemplateArgsEarly =
        [&](std::string typeText,
            bool &borrowedOut,
            std::vector<std::string> &tupleArgsOut) -> bool {
      borrowedOut = false;
      tupleArgsOut.clear();
      typeText = normalizeBindingTypeName(std::move(typeText));
      while (true) {
        std::string wrapperBase;
        std::string wrapperArgText;
        if (!splitTemplateTypeName(typeText, wrapperBase, wrapperArgText)) {
          break;
        }
        wrapperBase = normalizeBindingTypeName(wrapperBase);
        if (wrapperBase != "Reference" && wrapperBase != "Pointer") {
          break;
        }
        if (wrapperBase == "Pointer") {
          return false;
        }
        std::vector<std::string> wrapperArgs;
        if (!splitTopLevelTemplateArgs(wrapperArgText, wrapperArgs) ||
            wrapperArgs.size() != 1) {
          return false;
        }
        borrowedOut = true;
        typeText = normalizeBindingTypeName(wrapperArgs.front());
      }

      std::string tupleBase;
      std::string tupleArgText;
      if (splitTemplateTypeName(typeText, tupleBase, tupleArgText)) {
        if (!isStdlibTuplePathEarly(resolveTupleBasePathEarly(tupleBase))) {
          return false;
        }
        if (tupleArgText.empty()) {
          return true;
        }
        return splitTopLevelTemplateArgs(tupleArgText, tupleArgsOut);
      }

      const std::string tuplePath = resolveTupleBasePathEarly(typeText);
      if (!isStdlibTuplePathEarly(tuplePath)) {
        return false;
      }
      auto defIt = ctx.sourceDefs.find(tuplePath);
      if (defIt == ctx.sourceDefs.end()) {
        return false;
      }
      for (const TemplatePackBinding &packBinding : defIt->second.templatePackBindings) {
        if (packBinding.parameterName == "Ts") {
          tupleArgsOut = packBinding.arguments;
          return true;
        }
      }
      return false;
    };
    auto rewriteTupleIndexCandidate = [&](Expr &candidate,
                                          const auto &self,
                                          bool &handledOut) -> bool {
      handledOut = false;
      std::string builtinAccessName;
      if (candidate.kind != Expr::Kind::Call) {
        return true;
      }
      if (getBuiltinArrayAccessName(candidate, builtinAccessName) &&
          candidate.args.size() == 2 && !hasNamedCallArguments(candidate) &&
          !candidate.hasBodyArguments && candidate.bodyArguments.empty() &&
          candidate.templateArgs.empty()) {
        BindingInfo receiverInfo;
        if (inferBindingTypeForMonomorph(candidate.args.front(),
                                         params,
                                         locals,
                                         allowMathBare,
                                         ctx,
                                         receiverInfo)) {
          bool borrowedReceiver = false;
          std::vector<std::string> tupleArgs;
          if (extractTupleReceiverTemplateArgsEarly(bindingTypeToString(receiverInfo),
                                                   borrowedReceiver,
                                                   tupleArgs)) {
            const Expr &indexExpr = candidate.args[1];
            if (indexExpr.kind != Expr::Kind::Literal ||
                indexExpr.isUnsigned ||
                (indexExpr.intWidth != 32 && indexExpr.intWidth != 64)) {
              error = "tuple index must be a compile-time integer";
              return false;
            }

            std::vector<std::string> templateArgs;
            templateArgs.reserve(tupleArgs.size() + 1);
            templateArgs.push_back(std::to_string(indexExpr.literalValue));
            templateArgs.insert(templateArgs.end(), tupleArgs.begin(), tupleArgs.end());

            std::vector<TemplateArgument> templateArgDetails;
            templateArgDetails.reserve(templateArgs.size());
            templateArgDetails.push_back(
                TemplateArgument::integer(templateArgs.front(), indexExpr.literalValue));
            for (size_t i = 1; i < templateArgs.size(); ++i) {
              templateArgDetails.push_back(TemplateArgument::type(templateArgs[i]));
            }

            Expr receiver = candidate.args.front();
            candidate.name = borrowedReceiver ? "/std/tuple/get_ref" : "/std/tuple/get";
            candidate.sourceName = candidate.name;
            candidate.sourceIsMethodCall = false;
            candidate.namespacePrefix.clear();
            candidate.args.clear();
            candidate.args.push_back(std::move(receiver));
            candidate.argNames.assign(1, std::nullopt);
            candidate.templateArgs = std::move(templateArgs);
            candidate.templateArgDetails = std::move(templateArgDetails);
            candidate.isMethodCall = false;
            candidate.isFieldAccess = false;
            handledOut = true;
            return rewriteExpr(candidate,
                               mapping,
                               allowedParams,
                               namespacePrefix,
                               ctx,
                               error,
                               locals,
                               params,
                               allowMathBare);
          }
        }
      }
      for (Expr &arg : candidate.args) {
        bool childHandled = false;
        if (!self(arg, self, childHandled)) {
          return false;
        }
      }
      for (Expr &arg : candidate.bodyArguments) {
        bool childHandled = false;
        if (!self(arg, self, childHandled)) {
          return false;
        }
      }
      return true;
    };
    bool handledTupleIndexAccess = false;
    if (!rewriteTupleIndexCandidate(expr,
                                    rewriteTupleIndexCandidate,
                                    handledTupleIndexAccess)) {
      return st.done(false);
    }
    if (handledTupleIndexAccess) {
      return st.done(true);
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec
