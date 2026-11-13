#include "TemplateMonomorphAssignmentTargetResolution.h"
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

// The first rules of rewriteExpr: names, transforms and expected types, then literal folding and task calls.
// Each returns Done when it has decided the expression.

PhaseStatus rewriteNameAndTransforms(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;
  expr.namespacePrefix = namespacePrefix;
  rewriteCallableShapes(
      expr, mapping, allowedParams, namespacePrefix, ctx, locals, params, allowMathBare);
  rc.hadExplicitTemplateArgsOnEntry = !expr.templateArgs.empty();
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return rc.done(false);
  }
  {
    const FunctionValueConversion functionValues;
    const TextViewConversion textViews;
    const ExpectedTypeScope scope{
        mapping, allowedParams, namespacePrefix, ctx, locals, params, allowMathBare};
    if (!rewriteExpectedTypeSites(expr, {&functionValues, &textViews}, scope, error)) {
      return rc.done(false);
    }
  }
  if (expr.kind == Expr::Kind::Call && expr.name == "ct_if" && expr.args.size() == 3) {
    if (!rewriteCompileTimePredicateExpr(
            expr.args[0], mapping, allowedParams, namespacePrefix, ctx, error)) {
      return rc.done(false);
    }
    for (std::size_t branchIndex = 1; branchIndex < expr.args.size(); ++branchIndex) {
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
          return rc.done(false);
        }
      }
    }
    return rc.done(true);
  }
  if (expr.kind == Expr::Kind::Name) {
    auto isRuntimeParameter = [&]() {
      for (const semantics::ParameterInfo &param : params) {
        if (param.name == expr.name) {
          return true;
        }
      }
      return false;
    };
    auto appendUniquePath = [](std::vector<std::string> &paths, const std::string &path) {
      if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
      }
    };
    auto appendZeroArgCallablePathsForTarget = [&](const std::string &targetPath,
                                                   std::vector<std::string> &paths) {
      auto sourceIt = ctx.sourceDefs.find(targetPath);
      if (sourceIt != ctx.sourceDefs.end()) {
        if (sourceIt->second.parameters.empty() && sourceIt->second.templateArgs.empty() &&
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
        if (overloadDefIt == ctx.sourceDefs.end() || !overloadDefIt->second.templateArgs.empty() ||
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
      const auto &aliasTargets = scopedImportAliasTargetsForNamespace(namespacePrefix, ctx);
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
    auto appendPathList = [](std::string &message, const std::vector<std::string> &paths) {
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
      error = "ambiguous bare name: " + expr.name + " could refer to a local value or ";
      appendPathList(error, zeroArgPaths);
      return rc.done(false);
    }
    if (zeroArgPaths.size() > 1) {
      error = "ambiguous bare name: " + expr.name + " could refer to ";
      appendPathList(error, zeroArgPaths);
      return rc.done(false);
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
    return rc.done(true);
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
      if (!rewriteExpr(param,
                       lambdaMapping,
                       lambdaAllowed,
                       namespacePrefix,
                       ctx,
                       error,
                       lambdaLocals,
                       params,
                       allowMathBare)) {
        return rc.done(false);
      }
      semantics::BindingInfo info;
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
    for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size(); ++bodyIndex) {
      bool expandedDestructuring = false;
      if (!expandDestructuringStatementAt(expr.bodyArguments,
                                          bodyIndex,
                                          params,
                                          lambdaLocals,
                                          namespacePrefix,
                                          ctx,
                                          error,
                                          expandedDestructuring)) {
        return rc.done(false);
      }
      if (expandedDestructuring) {
        --bodyIndex;
        continue;
      }
      auto &bodyArg = expr.bodyArguments[bodyIndex];
      if (!rewriteExpr(bodyArg,
                       lambdaMapping,
                       lambdaAllowed,
                       namespacePrefix,
                       ctx,
                       error,
                       lambdaLocals,
                       params,
                       allowMathBare)) {
        return rc.done(false);
      }
      semantics::BindingInfo info;
      if (extractExplicitBindingType(bodyArg, info)) {
        if (info.typeName == "auto" && bodyArg.args.size() == 1 &&
            inferBindingTypeForMonomorph(
                bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        } else {
          lambdaLocals[bodyArg.name] = info;
        }
      } else if (bodyArg.isBinding && bodyArg.args.size() == 1) {
        if (inferBindingTypeForMonomorph(
                bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        }
      }
    }
    return rc.done(true);
  }
  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (semantics::splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!semantics::splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return rc.done(false);
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
  }
  {
    auto isStdlibTuplePathEarly = [](const std::string &path) {
      return path == "/std/tuple/tuple" || path.rfind("/std/tuple/tuple__t", 0) == 0;
    };
    auto resolveTupleBasePathEarly = [&](const std::string &base) -> std::string {
      std::string normalizedBase = semantics::normalizeBindingTypeName(base);
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
      typeText = semantics::normalizeBindingTypeName(std::move(typeText));
      while (true) {
        std::string wrapperBase;
        std::string wrapperArgText;
        if (!semantics::splitTemplateTypeName(typeText, wrapperBase, wrapperArgText)) {
          break;
        }
        wrapperBase = semantics::normalizeBindingTypeName(wrapperBase);
        if (wrapperBase != "Reference" && wrapperBase != "Pointer") {
          break;
        }
        if (wrapperBase == "Pointer") {
          return false;
        }
        std::vector<std::string> wrapperArgs;
        if (!semantics::splitTopLevelTemplateArgs(wrapperArgText, wrapperArgs) ||
            wrapperArgs.size() != 1) {
          return false;
        }
        borrowedOut = true;
        typeText = semantics::normalizeBindingTypeName(wrapperArgs.front());
      }

      std::string tupleBase;
      std::string tupleArgText;
      if (semantics::splitTemplateTypeName(typeText, tupleBase, tupleArgText)) {
        if (!isStdlibTuplePathEarly(resolveTupleBasePathEarly(tupleBase))) {
          return false;
        }
        if (tupleArgText.empty()) {
          return true;
        }
        return semantics::splitTopLevelTemplateArgs(tupleArgText, tupleArgsOut);
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
    auto rewriteTupleIndexCandidate =
        [&](Expr &candidate, const auto &self, bool &handledOut) -> bool {
      handledOut = false;
      std::string builtinAccessName;
      if (candidate.kind != Expr::Kind::Call) {
        return true;
      }
      if (semantics::getBuiltinArrayAccessName(candidate, builtinAccessName) &&
          candidate.args.size() == 2 && !hasNamedCallArguments(candidate) &&
          !candidate.hasBodyArguments && candidate.bodyArguments.empty() &&
          candidate.templateArgs.empty()) {
        semantics::BindingInfo receiverInfo;
        if (inferBindingTypeForMonomorph(
                candidate.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
          bool borrowedReceiver = false;
          std::vector<std::string> tupleArgs;
          if (extractTupleReceiverTemplateArgsEarly(
                  bindingTypeToString(receiverInfo), borrowedReceiver, tupleArgs)) {
            const Expr &indexExpr = candidate.args[1];
            if (indexExpr.kind != Expr::Kind::Literal || indexExpr.isUnsigned ||
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
    if (!rewriteTupleIndexCandidate(expr, rewriteTupleIndexCandidate, handledTupleIndexAccess)) {
      return rc.done(false);
    }
    if (handledTupleIndexAccess) {
      return rc.done(true);
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteLiteralAndTaskCalls(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;
  auto expandCurrentTypePackSpreadArguments =
      [&](std::vector<Expr> &arguments, std::vector<std::optional<std::string>> &argumentNames) {
        if (ctx.currentRewriteDefinition == nullptr ||
            ctx.currentRewriteDefinition->templatePackBindings.empty() || arguments.empty()) {
          return;
        }
        const TemplatePackBinding &packBinding =
            ctx.currentRewriteDefinition->templatePackBindings.front();
        std::vector<Expr> expanded;
        std::vector<std::optional<std::string>> expandedNames;
        expanded.reserve(arguments.size());
        if (!argumentNames.empty()) {
          expandedNames.reserve(argumentNames.size() + packBinding.arguments.size());
        }
        auto spreadSourceName = [](const Expr &arg) -> std::string {
          if (arg.kind == Expr::Kind::Name) {
            return arg.name;
          }
          if (arg.kind == Expr::Kind::Call && !arg.isBinding && !arg.isMethodCall &&
              !arg.isFieldAccess && arg.args.empty() && arg.argNames.empty() &&
              arg.bodyArguments.empty() && !arg.hasBodyArguments && arg.templateArgs.empty()) {
            return arg.name;
          }
          return {};
        };
        bool changed = false;
        for (size_t argIndex = 0; argIndex < arguments.size(); ++argIndex) {
          Expr &arg = arguments[argIndex];
          const bool hasArgName = !argumentNames.empty() && argIndex < argumentNames.size();
          const std::string spreadName = arg.isSpread ? spreadSourceName(arg) : std::string{};
          if (!arg.isSpread || spreadName.empty()) {
            expanded.push_back(std::move(arg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(hasArgName ? argumentNames[argIndex] : std::nullopt);
            }
            continue;
          }

          bool allGeneratedBindingsVisible = true;
          for (size_t packIndex = 0; packIndex < packBinding.arguments.size(); ++packIndex) {
            if (findBinding(params, locals, generatedPackFieldName(spreadName, packIndex)) ==
                nullptr) {
              allGeneratedBindingsVisible = false;
              break;
            }
          }
          if (!allGeneratedBindingsVisible) {
            expanded.push_back(std::move(arg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(hasArgName ? argumentNames[argIndex] : std::nullopt);
            }
            continue;
          }

          changed = true;
          for (size_t packIndex = 0; packIndex < packBinding.arguments.size(); ++packIndex) {
            Expr expandedArg;
            expandedArg.kind = Expr::Kind::Name;
            expandedArg.name = generatedPackFieldName(spreadName, packIndex);
            expandedArg.sourceName = expandedArg.name;
            expandedArg.namespacePrefix = namespacePrefix;
            expandedArg.sourceLine = arg.sourceLine;
            expandedArg.sourceColumn = arg.sourceColumn;
            expanded.push_back(std::move(expandedArg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(std::nullopt);
            }
          }
        }
        (void)changed;
        arguments = std::move(expanded);
        if (!argumentNames.empty()) {
          argumentNames = std::move(expandedNames);
        }
      };
  expandCurrentTypePackSpreadArguments(expr.args, expr.argNames);
  std::vector<std::optional<std::string>> bodyArgumentNames;
  expandCurrentTypePackSpreadArguments(expr.bodyArguments, bodyArgumentNames);
  (void)rewriteKeyValueWrapperHelperCallToMethod(
      expr, namespacePrefix, ctx, locals, params, allowMathBare);
  if (!foldLiteralTextCall(expr, error)) {
    return rc.done(false);
  }
  (void)rewriteVectorViewMethod(expr, ctx, locals, params, allowMathBare);
  (void)rewriteStructIndexAssign(expr, namespacePrefix, ctx, locals, params, allowMathBare);
  (void)rewriteStructOperatorCallToMethod(
      expr, namespacePrefix, ctx, locals, params, allowMathBare);
  if (!expr.isMethodCall && !expr.isBinding && !expr.isFieldAccess &&
      expr.kind == Expr::Kind::Call && expr.transforms.empty() &&
      semantics::isSimpleCallName(expr, "wait") && expr.args.size() > 1 && !expr.hasBodyArguments &&
      expr.bodyArguments.empty() && expr.templateArgs.empty()) {
    if (hasNamedCallArguments(expr)) {
      error = "wait does not accept named task handles";
      return rc.done(false);
    }
    std::vector<std::string> tupleArgs;
    tupleArgs.reserve(expr.args.size());
    size_t voidTaskCount = 0;
    for (Expr &arg : expr.args) {
      if (!rewriteExpr(arg,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return rc.done(false);
      }
      if (arg.kind != Expr::Kind::Name) {
        error = "wait requires a task handle binding";
        return rc.done(false);
      }
      semantics::BindingInfo taskBinding;
      if (!inferBindingTypeForMonomorph(arg, params, locals, allowMathBare, ctx, taskBinding)) {
        error = "wait requires a task handle binding";
        return rc.done(false);
      }
      std::string taskBase = semantics::normalizeBindingTypeName(taskBinding.typeName);
      std::string taskResultType = taskBinding.typeTemplateArg;
      if (taskResultType.empty()) {
        std::string splitBase;
        std::string splitArg;
        if (semantics::splitTemplateTypeName(taskBase, splitBase, splitArg)) {
          taskBase = semantics::normalizeBindingTypeName(splitBase);
          taskResultType = splitArg;
        }
      }
      if (taskBase != "Task" || taskResultType.empty()) {
        error = "wait requires a task handle binding";
        return rc.done(false);
      }
      ResolvedType resolvedArg =
          resolveTypeString(taskResultType, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return rc.done(false);
      }
      if (!resolvedArg.concrete) {
        error = "multi-wait task result types must be concrete";
        return rc.done(false);
      }
      if (semantics::normalizeBindingTypeName(resolvedArg.text) == "void") {
        ++voidTaskCount;
      }
      tupleArgs.push_back(std::move(resolvedArg.text));
    }
    if (voidTaskCount == expr.args.size()) {
      // Tasks without results: the wait stays a call that waits each handle in order and has no value
      // (docs/MultithreadingPrototype.md, Multi-Wait).
      return rc.done(true);
    }
    if (voidTaskCount > 0) {
      // A tuple has no void element, and dropping the void tasks would shift the positions of the others.
      error = "wait of several tasks cannot mix Task<void> with tasks that have a result: wait "
              "them separately";
      ctx.errorSourceLine = expr.sourceLine;
      ctx.errorSourceColumn = expr.sourceColumn;
      return rc.done(false);
    }
    if (ctx.sourceDefs.find("/std/tuple/tuple") == ctx.sourceDefs.end()) {
      // The result of a multi-wait is a stdlib tuple, which only a program that imports it has.
      error = "wait of several tasks returns a tuple: import /std/tuple/*";
      ctx.errorSourceLine = expr.sourceLine;
      ctx.errorSourceColumn = expr.sourceColumn;
      return rc.done(false);
    }
    std::string tuplePath;
    if (!instantiateTemplate("/std/tuple/tuple", tupleArgs, nullptr, ctx, error, tuplePath)) {
      return rc.done(false);
    }
    const std::string sourceName = expr.sourceName.empty() ? expr.name : expr.sourceName;
    expr.name = std::move(tuplePath);
    expr.sourceName = sourceName;
    expr.sourceIsMethodCall = false;
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.isBraceConstructor = true;
    return rc.done(true);
  }
  if (!expr.isMethodCall && !expr.isBinding && expr.kind == Expr::Kind::Call &&
      !expr.hasBodyArguments && expr.bodyArguments.empty() &&
      resolveCalleePath(expr, namespacePrefix, ctx) == "/std/tuple/make_tuple") {
    if (hasNamedCallArguments(expr)) {
      error = "named arguments cannot bind heterogeneous value-pack parameter: values";
      return rc.done(false);
    }
    for (Expr &arg : expr.args) {
      if (arg.isSpread) {
        error = "heterogeneous value-pack inference does not support spread forwarding on "
                "/std/tuple/make_tuple";
        return rc.done(false);
      }
      if (!rewriteExpr(arg,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return rc.done(false);
      }
    }
    std::vector<std::string> tupleArgs = expr.templateArgs;
    if (tupleArgs.empty()) {
      tupleArgs.reserve(expr.args.size());
      for (const Expr &arg : expr.args) {
        semantics::BindingInfo argInfo;
        if (!inferBindingTypeForMonomorph(arg, params, locals, allowMathBare, ctx, argInfo)) {
          error = "unable to infer implicit template arguments for /std/tuple/make_tuple";
          return rc.done(false);
        }
        const std::string argType = bindingTypeToString(argInfo);
        if (argType.empty()) {
          error = "unable to infer implicit template arguments for /std/tuple/make_tuple";
          return rc.done(false);
        }
        ResolvedType resolvedArg =
            resolveTypeString(argType, mapping, allowedParams, namespacePrefix, ctx, error);
        if (!error.empty()) {
          return rc.done(false);
        }
        if (!resolvedArg.concrete) {
          error = "implicit template arguments must be concrete on /std/tuple/make_tuple";
          return rc.done(false);
        }
        tupleArgs.push_back(resolvedArg.text);
      }
    } else {
      bool tupleArgsConcrete = true;
      if (!resolveTemplateArgumentList(
              tupleArgs, mapping, allowedParams, namespacePrefix, ctx, error, tupleArgsConcrete)) {
        return rc.done(false);
      }
      if (!tupleArgsConcrete) {
        error = "implicit template arguments must be concrete on /std/tuple/make_tuple";
        return rc.done(false);
      }
    }
    std::string tuplePath;
    if (!instantiateTemplate("/std/tuple/tuple", tupleArgs, nullptr, ctx, error, tuplePath)) {
      return rc.done(false);
    }
    expr.name = std::move(tuplePath);
    expr.sourceName = expr.name;
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isBraceConstructor = true;
    return rc.done(true);
  }
  if (semantics::isPickCall(expr)) {
    for (auto &arg : expr.args) {
      if (!rewriteExpr(arg,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return rc.done(false);
      }
    }

    Definition *sumDef =
        expr.args.empty() ? nullptr : resolvePickSumDefinition(rc, expr.args.front());
    for (auto &arm : expr.bodyArguments) {
      arm.namespacePrefix = namespacePrefix;
      if (!rewriteTransforms(arm.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
        return rc.done(false);
      }
      for (auto &armArg : arm.args) {
        armArg.namespacePrefix = namespacePrefix;
        if (!rewriteTransforms(
                armArg.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
          return rc.done(false);
        }
      }
      LocalTypeMap armLocals = locals;
      if (sumDef != nullptr) {
        for (const auto &variant : sumDef->sumVariants) {
          if (variant.name == arm.name) {
            appendPickPayloadLocal(rc, variant, arm, armLocals);
            break;
          }
        }
      }
      for (auto &bodyExpr : arm.bodyArguments) {
        if (!rewriteExpr(bodyExpr,
                         mapping,
                         allowedParams,
                         namespacePrefix,
                         ctx,
                         error,
                         armLocals,
                         params,
                         allowMathBare)) {
          return rc.done(false);
        }
        recordBodyBindingLocal(rc, bodyExpr, armLocals);
      }
    }
    return rc.done(true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec
