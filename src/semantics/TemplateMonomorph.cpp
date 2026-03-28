#include "SemanticsHelpers.h"
#include "primec/Ast.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

struct ResolvedType {
  std::string text;
  bool concrete = true;
};

using SubstMap = std::unordered_map<std::string, std::string>;

struct TemplateRootInfo {
  std::string fullPath;
  std::vector<std::string> params;
};

struct HelperOverloadEntry {
  std::string internalPath;
  size_t parameterCount = 0;
};

struct Context {
  Program &program;
  std::unordered_map<std::string, Definition> sourceDefs;
  std::unordered_set<std::string> templateDefs;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<HelperOverloadEntry>> helperOverloads;
  std::unordered_map<std::string, std::string> helperOverloadInternalToPublic;
  std::unordered_map<std::string, std::string> specializationCache;
  std::unordered_set<std::string> outputPaths;
  std::vector<Definition> outputDefs;
  std::vector<Execution> outputExecs;
  std::unordered_set<std::string> implicitTemplateDefs;
  std::unordered_map<std::string, std::vector<std::string>> implicitTemplateParams;
  std::unordered_set<std::string> returnInferenceStack;
};

using LocalTypeMap = std::unordered_map<std::string, BindingInfo>;

ResolvedType resolveTypeString(std::string input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error);

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack);

bool isGeneratedTemplateSpecializationPath(std::string_view path);
bool isEnclosingTemplateParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx);

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error);

bool resolvesExperimentalMapValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx);

bool isStructDefinition(const Definition &def);

#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"

bool isStructDefinition(const Definition &def) {
  bool isStruct = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isStructTransformName(transform.name)) {
      isStruct = true;
    }
  }
  if (isStruct) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx);

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut);

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut);

bool resolveFieldBindingTarget(const Expr &target,
                               const std::vector<ParameterInfo> &params,
                               const LocalTypeMap &locals,
                               bool allowMathBare,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               BindingInfo &bindingOut);

#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut);

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare);

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphTypeResolution.h"
#include "TemplateMonomorphCollectionHelperInference.h"
#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphTemplateSpecialization.h"

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut) {
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, infoOut, allowMathBare)) {
    return true;
  }
  if (resolveFieldBindingTarget(initializer,
                                params,
                                locals,
                                allowMathBare,
                                initializer.namespacePrefix,
                                ctx,
                                infoOut)) {
    return true;
  }
  bool handledCallInference = false;
  if (inferCallBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut,
                                       handledCallInference)) {
    return true;
  }
  if (handledCallInference) {
    return false;
  }
  return inferBlockBodyBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut);
}

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error) {
  if (def.templateArgs.empty()) {
    return false;
  }
  const bool isStdlibCollectionHelper =
      def.fullPath.rfind("/std/collections/vector/", 0) == 0 ||
      def.fullPath.rfind("/std/collections/map/", 0) == 0 ||
      def.fullPath.rfind("/map/", 0) == 0;
  std::unordered_set<std::string> implicitSet;
  auto implicitIt = ctx.implicitTemplateParams.find(def.fullPath);
  if (implicitIt != ctx.implicitTemplateParams.end()) {
    const auto &implicitParams = implicitIt->second;
    implicitSet.insert(implicitParams.begin(), implicitParams.end());
  } else {
    implicitSet.insert(def.templateArgs.begin(), def.templateArgs.end());
  }
  std::unordered_map<std::string, std::string> inferred;
  inferred.reserve(def.templateArgs.size());
  std::unordered_set<std::string> wrappedTemplateInferenceParams;
  wrappedTemplateInferenceParams.reserve(def.templateArgs.size());
  if (!callExpr.templateArgs.empty()) {
    if (callExpr.templateArgs.size() > def.templateArgs.size()) {
      error = "template argument count mismatch for " + def.fullPath;
      return false;
    }
    for (size_t i = 0; i < callExpr.templateArgs.size(); ++i) {
      ResolvedType resolvedArg =
          resolveTypeString(callExpr.templateArgs[i], mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (!resolvedArg.concrete) {
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
      }
      inferred.emplace(def.templateArgs[i], resolvedArg.text);
    }
  }
  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(param));
  }
  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = callParams.size();
  size_t callArgStart = 0;
  size_t paramIndexOffset = 0;
  const bool hasLeadingReceiverParam = [&]() {
    if (callParams.empty()) {
      return false;
    }
    const size_t lastSlash = def.fullPath.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
      return false;
    }
    const size_t ownerSlash = def.fullPath.find_last_of('/', lastSlash - 1);
    if (ownerSlash == std::string::npos) {
      return false;
    }
    return normalizeBindingTypeName(callParams.front().binding.typeName) ==
           def.fullPath.substr(ownerSlash + 1, lastSlash - ownerSlash - 1);
  }();
  if (hasLeadingReceiverParam && callExpr.args.size() + 1 == callParams.size()) {
    // Some member-helper monomorphization paths have already stripped the
    // receiver value from the call arguments, so align the parameter view to
    // the post-receiver shape before argument ordering.
    callParams.erase(callParams.begin());
    paramIndexOffset = 1;
  } else if (callExpr.isMethodCall && !callParams.empty()) {
    if (callExpr.args.size() == callParams.size() + 1) {
      // Method-call sugar prepends the receiver expression.
      callArgStart = 1;
    }
  }
  std::vector<Expr> reorderedArgs;
  std::vector<std::optional<std::string>> reorderedArgNames;
  const std::vector<Expr> *orderedCallArgs = &callExpr.args;
  const std::vector<std::optional<std::string>> *orderedCallArgNames = &callExpr.argNames;
  if (callArgStart > 0) {
    reorderedArgs.assign(callExpr.args.begin() + static_cast<std::ptrdiff_t>(callArgStart), callExpr.args.end());
    if (!callExpr.argNames.empty()) {
      reorderedArgNames.assign(callExpr.argNames.begin() + static_cast<std::ptrdiff_t>(callArgStart), callExpr.argNames.end());
    }
    orderedCallArgs = &reorderedArgs;
    orderedCallArgNames = &reorderedArgNames;
  }
  if (!buildOrderedArguments(callParams, *orderedCallArgs, *orderedCallArgNames, orderedArgs, packedArgs, packedParamIndex, error)) {
    if (error.find("argument count mismatch") != std::string::npos) {
      error = "argument count mismatch for " + def.fullPath;
    }
    return false;
  }

  for (size_t i = 0; i < def.parameters.size(); ++i) {
    const Expr &param = def.parameters[i];
    BindingInfo paramInfo;
    if (!extractExplicitBindingType(param, paramInfo)) {
      continue;
    }
    if (paramInfo.typeName == "auto" && param.args.size() == 1 &&
        inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, paramInfo)) {
      // Auto parameters participate in implicit-template inference through
      // their default initializer shape.
    }
    if (i < paramIndexOffset) {
      continue;
    }
    const size_t callParamIndex = i - paramIndexOffset;
    const bool inferFromPackedArgs = callParamIndex == packedParamIndex && isArgsPackBinding(paramInfo) &&
                                     implicitSet.count(paramInfo.typeTemplateArg) > 0;
    bool inferFromWrappedTemplateArgs = false;
    std::vector<std::string> inferredParamNames;
    if (inferFromPackedArgs) {
      inferredParamNames.push_back(paramInfo.typeTemplateArg);
    } else {
      if (implicitSet.count(paramInfo.typeName) == 0) {
        std::vector<std::string> wrappedTemplateArgs;
        if (paramInfo.typeTemplateArg.empty() ||
            !splitTopLevelTemplateArgs(paramInfo.typeTemplateArg, wrappedTemplateArgs) ||
            wrappedTemplateArgs.empty()) {
          continue;
        }
        bool allImplicit = true;
        for (std::string &wrappedArg : wrappedTemplateArgs) {
          wrappedArg = trimWhitespace(wrappedArg);
          if (wrappedArg.empty() || wrappedArg.find('<') != std::string::npos ||
              implicitSet.count(wrappedArg) == 0) {
            allImplicit = false;
            break;
          }
        }
        if (!allImplicit) {
          continue;
        }
        inferFromWrappedTemplateArgs = true;
        inferredParamNames = std::move(wrappedTemplateArgs);
      } else {
        inferredParamNames.push_back(paramInfo.typeName);
      }
    }
    std::vector<const Expr *> argsToInfer;
    if (inferFromPackedArgs) {
      argsToInfer = packedArgs;
    } else {
      const Expr *argExpr = callParamIndex < orderedArgs.size() ? orderedArgs[callParamIndex] : nullptr;
      if (!argExpr && param.args.size() == 1) {
        argExpr = &param.args.front();
      }
      if (argExpr) {
        argsToInfer.push_back(argExpr);
      }
    }
    if (argsToInfer.empty()) {
      error = "implicit template arguments require values on " + def.fullPath;
      return false;
    }
    for (const Expr *argExpr : argsToInfer) {
      BindingInfo argInfo;
      if (!argExpr) {
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      const bool usesDefaultArgBinding =
          !inferFromPackedArgs && callParamIndex >= orderedArgs.size() && param.args.size() == 1 &&
          argExpr == &param.args.front();
      if (usesDefaultArgBinding) {
        argInfo = paramInfo;
      } else if (argExpr->isSpread) {
        std::string spreadElementType;
        if (!resolveArgsPackElementTypeForExpr(*argExpr, params, locals, spreadElementType)) {
          error = "spread argument requires args<T> value";
          return false;
        }
        argInfo.typeName = normalizeBindingTypeName(spreadElementType);
        argInfo.typeTemplateArg.clear();
        std::string spreadBase;
        std::string spreadArgs;
        if (splitTemplateTypeName(spreadElementType, spreadBase, spreadArgs)) {
          argInfo.typeName = normalizeBindingTypeName(spreadBase);
          argInfo.typeTemplateArg = spreadArgs;
        }
      } else if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      if (inferFromWrappedTemplateArgs) {
        if (normalizeBindingTypeName(argInfo.typeName) != normalizeBindingTypeName(paramInfo.typeName)) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        std::vector<std::string> argTemplateArgs;
        if (argInfo.typeTemplateArg.empty() ||
            !splitTopLevelTemplateArgs(argInfo.typeTemplateArg, argTemplateArgs) ||
            argTemplateArgs.size() != inferredParamNames.size()) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        for (size_t templateIndex = 0; templateIndex < inferredParamNames.size(); ++templateIndex) {
          ResolvedType resolvedArg =
              resolveTypeString(argTemplateArgs[templateIndex], mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }
          if (!resolvedArg.concrete) {
            error = "implicit template arguments must be concrete on " + def.fullPath;
            return false;
          }
          auto it = inferred.find(inferredParamNames[templateIndex]);
          if (it != inferred.end() && it->second != resolvedArg.text) {
            if (wrappedTemplateInferenceParams.count(inferredParamNames[templateIndex]) == 0) {
              error = "implicit template arguments conflict on " + def.fullPath;
              return false;
            }
            continue;
          }
          inferred[inferredParamNames[templateIndex]] = resolvedArg.text;
          wrappedTemplateInferenceParams.insert(inferredParamNames[templateIndex]);
        }
        continue;
      }
      std::string argType = bindingTypeToString(argInfo);
      if (argType.empty() || inferredParamNames.empty()) {
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      ResolvedType resolvedArg = resolveTypeString(argType, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (!resolvedArg.concrete) {
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
      }
      auto it = inferred.find(inferredParamNames.front());
      if (it != inferred.end() && it->second != resolvedArg.text) {
        if (wrappedTemplateInferenceParams.count(inferredParamNames.front()) == 0) {
          error = "implicit template arguments conflict on " + def.fullPath;
          return false;
        }
        continue;
      }
      inferred[inferredParamNames.front()] = resolvedArg.text;
    }
  }

  outArgs.clear();
  outArgs.reserve(def.templateArgs.size());
  for (const auto &paramName : def.templateArgs) {
    auto it = inferred.find(paramName);
    if (it == inferred.end()) {
      // Leave error empty so specialized fallback inference can run (for example,
      // stdlib collection helpers that infer T from vector<T> receiver shape).
      return false;
    }
    outArgs.push_back(it->second);
  }
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
  expr.namespacePrefix = namespacePrefix;
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return true;
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
        return false;
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
        return false;
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
    return true;
  }

  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return false;
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
  }

  auto isCanonicalBuiltinMapHelperPath = [](const std::string &path) {
    return path == "/std/collections/map/count" || path == "/std/collections/map/contains" ||
           path == "/std/collections/map/tryAt" || path == "/std/collections/map/at" ||
           path == "/std/collections/map/at_unsafe";
  };
  auto isCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return true;
    }
    return path == "/std/collections/vector/count" || path == "/std/collections/vector/capacity" ||
           path == "/std/collections/vector/push" || path == "/std/collections/vector/pop" ||
           path == "/std/collections/vector/reserve" || path == "/std/collections/vector/clear" ||
           path == "/std/collections/vector/remove_at" || path == "/std/collections/vector/remove_swap" ||
           path == "/std/collections/vector/at" || path == "/std/collections/vector/at_unsafe";
  };
  auto mapHelperReceiverExpr = [&](const Expr &candidate) -> const Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  auto mutableMapHelperReceiverExpr = [&](Expr &candidate) -> Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  auto resolvesBuiltinMapReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        std::string innerBase;
        std::string innerArgText;
        if (splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
          receiverType = normalizeCollectionReceiverTypeName(innerBase);
        }
      }
      if (receiverType == "map") {
        return true;
      }
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverType.empty()) {
      return false;
    }
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
      return normalizeCollectionReceiverTypeName(receiverBase) == "map";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "map";
  };
  auto resolvesBuiltinVectorReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        std::string innerBase;
        std::string innerArgText;
        if (splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
          receiverType = normalizeCollectionReceiverTypeName(innerBase);
        }
      }
      if (receiverType == "vector") {
        return true;
      }
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverType.empty()) {
      return false;
    }
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
      return normalizeCollectionReceiverTypeName(receiverBase) == "vector";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "vector";
  };
  auto shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty() || !isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
      if (path.rfind("/std/collections/vector/", 0) == 0 &&
          !resolvesBuiltinVectorReceiver(mapHelperReceiverExpr(expr)) &&
          !resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        return true;
      }
      return false;
    }
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return resolvesBuiltinMapReceiver(mapHelperReceiverExpr(expr)) && ctx.templateDefs.count(path) == 0;
    }
    return true;
  };
  std::function<bool(Expr &)> rewriteNestedExperimentalMapConstructorValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalMapResultOkPayloadValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalVectorConstructorValue;
  std::function<bool(const std::string &, Expr &)> rewriteMapTargetValueForResolvedType;
  std::function<bool(const std::string &, Expr &)> rewriteVectorTargetValueForResolvedType;

  rewriteMapTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalMapTargetValueForType(typeText,
                                                    valueExpr,
                                                    mapping,
                                                    allowedParams,
                                                    namespacePrefix,
                                                    ctx,
                                                    rewriteNestedExperimentalMapConstructorValue,
                                                    rewriteNestedExperimentalMapResultOkPayloadValue);
  };
  rewriteVectorTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalVectorTargetValueForType(typeText,
                                                       valueExpr,
                                                       rewriteNestedExperimentalVectorConstructorValue);
  };

  auto rewriteCanonicalExperimentalMapConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalMapValueTypeText(bindingTypeText,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx);
        },
        "map",
        rewriteMapTargetValueForResolvedType);
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalVectorValueTypeText(bindingTypeText);
        },
        "vector",
        rewriteVectorTargetValueForResolvedType);
  };
  rewriteNestedExperimentalMapConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return rewriteCanonicalExperimentalMapConstructorExpr(current,
                                                            locals,
                                                            params,
                                                            mapping,
                                                            allowedParams,
                                                            namespacePrefix,
                                                            ctx,
                                                            allowMathBare,
                                                            error);
    });
  };

  rewriteNestedExperimentalMapResultOkPayloadValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalMapResultOkPayloadTree(candidate, rewriteNestedExperimentalMapConstructorValue);
  };
  rewriteNestedExperimentalVectorConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return rewriteCanonicalExperimentalVectorConstructorExpr(current,
                                                               locals,
                                                               params,
                                                               mapping,
                                                               allowedParams,
                                                               namespacePrefix,
                                                               ctx,
                                                               allowMathBare,
                                                               error);
    });
  };

  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return false;
    }
    if (!rewriteCanonicalExperimentalMapConstructorBinding(expr)) {
      return false;
    }
  }

  bool allConcrete = true;
  for (auto &templArg : expr.templateArgs) {
    ResolvedType resolvedArg = resolveTypeString(templArg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolvedArg.concrete) {
      allConcrete = false;
    }
    templArg = resolvedArg.text;
  }

  if (!expr.isMethodCall && !expr.isBinding) {
    if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteExpr(*receiverExpr,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return false;
      }
    }
    std::string resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
    const std::string borrowedCanonicalMapUnknownTarget = canonicalMapHelperUnknownTargetPath(resolvedPath);
    if (!borrowedCanonicalMapUnknownTarget.empty() &&
        resolvesExperimentalMapBorrowedReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      error = "unknown call target: " + borrowedCanonicalMapUnknownTarget;
      return false;
    }
    const std::string experimentalMapPath = experimentalMapHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalMapPath.empty() && ctx.sourceDefs.count(experimentalMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalMapPath;
      expr.name = experimentalMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalWrapperMapPath = experimentalMapHelperPathForWrapperHelper(resolvedPath);
    if (!experimentalWrapperMapPath.empty() && ctx.sourceDefs.count(experimentalWrapperMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalWrapperMapPath;
      expr.name = experimentalWrapperMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalVectorPath = experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0 &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      resolvedPath = experimentalVectorPath;
      expr.name = experimentalVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalMapValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalVectorValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    const std::string originalResolvedPath = resolvedPath;
    const std::string preferredPath = preferVectorStdlibHelperPath(resolvedPath, ctx.sourceDefs);
    if (preferredPath != resolvedPath && ctx.sourceDefs.count(preferredPath) > 0) {
      resolvedPath = preferredPath;
      expr.name = preferredPath;
    }
    const bool explicitCompatibilityAliasToCanonicalTemplate =
        expr.templateArgs.empty() && isExplicitCollectionCompatibilityAliasPath(originalResolvedPath) &&
        preferredPath != originalResolvedPath && ctx.templateDefs.count(preferredPath) > 0;
    const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isBuiltinMapCountPath =
        resolvedPath == "/std/collections/map/count" || resolvedPath == "/map/count";
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
    if (!expr.templateArgs.empty() && !resolvedWasTemplate && !isKnownDef && isBuiltinMapCountPath) {
      error = "count does not accept template arguments";
      return false;
    }
    if (!expr.templateArgs.empty() && !resolvedWasTemplate) {
      if (!shouldPreserveCompatibilityTemplatePath(resolvedPath, ctx) &&
          !shouldPreserveCanonicalMapTemplatePath(resolvedPath, ctx)) {
        const std::string templatePreferredPath = preferVectorStdlibTemplatePath(resolvedPath, ctx);
        if (templatePreferredPath != resolvedPath) {
          resolvedPath = templatePreferredPath;
          expr.name = templatePreferredPath;
        }
      }
    }
    if (expr.templateArgs.empty() && !explicitCompatibilityAliasToCanonicalTemplate) {
      const std::string implicitTemplatePreferredPath =
          preferVectorStdlibImplicitTemplatePath(expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
      if (implicitTemplatePreferredPath != resolvedPath) {
        resolvedPath = implicitTemplatePreferredPath;
        expr.name = implicitTemplatePreferredPath;
      }
    }
    if (ctx.helperOverloadInternalToPublic.count(resolvedPath) > 0) {
      expr.name = resolvedPath;
      expr.namespacePrefix.clear();
    }
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
    if (isTemplateDef) {
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      const bool shouldInferImplicitTemplateTail =
          defIt != ctx.sourceDefs.end() &&
          !expr.templateArgs.empty() &&
          ctx.implicitTemplateDefs.count(resolvedPath) > 0 &&
          expr.templateArgs.size() < defIt->second.templateArgs.size();
      if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
        if (defIt != ctx.sourceDefs.end()) {
          std::vector<std::string> inferredArgs;
          if (inferImplicitTemplateArgs(defIt->second,
                                        expr,
                                        locals,
                                        params,
                                        mapping,
                                        allowedParams,
                                        namespacePrefix,
                                        ctx,
                                        allowMathBare,
                                        inferredArgs,
                                        error)) {
            expr.templateArgs = std::move(inferredArgs);
            allConcrete = true;
          } else {
            if (error.empty() &&
                inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                        expr,
                                                        locals,
                                                        params,
                                                        mapping,
                                                        allowedParams,
                                                        namespacePrefix,
                                                        ctx,
                                                        allowMathBare,
                                                        inferredArgs)) {
              allConcrete = inferredTemplateArgsAreConcrete(defIt->second, inferredArgs);
              expr.templateArgs = std::move(inferredArgs);
            } else if (!error.empty()) {
              return false;
            }
          }
        }
      }
      if (expr.templateArgs.empty()) {
        if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
          return true;
        }
        error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
        return false;
      }
      if (allConcrete) {
        std::string specializedPath;
        if (!instantiateTemplate(resolvedPath, expr.templateArgs, ctx, error, specializedPath)) {
          return false;
        }
        expr.name = specializedPath;
        expr.templateArgs.clear();
      }
    } else if (isKnownDef && !expr.templateArgs.empty()) {
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return false;
    }
    auto defIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
    if (defIt != ctx.sourceDefs.end()) {
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteVectorTargetValueForResolvedType(typeText, argExpr);
              })) {
        return false;
      }
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteMapTargetValueForResolvedType(typeText, argExpr);
              })) {
        return false;
      }
    }
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteMapTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteMapTargetValueForResolvedType(targetTypeText, valueExpr);
        });
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalMapConstructorValue(expr.args.front())) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(expr.args.front())) {
        return false;
      }
    }
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const std::string experimentalVectorMethodPath =
          experimentalVectorHelperPathForCanonicalHelper(methodPath);
      const bool shouldRewriteCanonicalVectorMethodToExperimental =
          ctx.sourceDefs.count(methodPath) == 0 && ctx.helperOverloads.count(methodPath) == 0 &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath);
      if (shouldRewriteCanonicalVectorMethodToExperimental &&
          !experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalVectorValueReceiverTemplateArgs(
                  mapHelperReceiverExpr(expr),
                  params,
                  locals,
                  allowMathBare,
                  namespacePrefix,
                  ctx,
                  receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      const bool methodWasTemplate = ctx.templateDefs.count(methodPath) > 0;
      if (!expr.templateArgs.empty() && !methodWasTemplate) {
        if (!shouldPreserveCompatibilityTemplatePath(methodPath, ctx) &&
            !shouldPreserveCanonicalMapTemplatePath(methodPath, ctx)) {
          methodPath = preferVectorStdlibTemplatePath(methodPath, ctx);
        }
      }
      if (expr.templateArgs.empty()) {
        methodPath =
            preferVectorStdlibImplicitTemplatePath(expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      if (expr.templateArgs.empty() &&
          isExperimentalVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr),
                params,
                locals,
                allowMathBare,
                namespacePrefix,
                ctx,
                receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (methodPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (methodPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      const bool isStaticFileErrorHelperCall =
          expr.isMethodCall && !expr.args.empty() &&
          expr.args.front().kind == Expr::Kind::Name &&
          normalizeBindingTypeName(expr.args.front().name) == "FileError" &&
          methodPath.rfind("/std/file/FileError/", 0) == 0;
      if (ctx.helperOverloadInternalToPublic.count(methodPath) > 0) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
      }
      if (isStaticFileErrorHelperCall) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
        expr.isMethodCall = false;
        if (!expr.args.empty()) {
          expr.args.erase(expr.args.begin());
        }
        if (!expr.argNames.empty()) {
          expr.argNames.erase(expr.argNames.begin());
        }
      }
      const bool isTemplateDef = ctx.templateDefs.count(methodPath) > 0;
      const bool isKnownDef = ctx.sourceDefs.count(methodPath) > 0;
      if (isTemplateDef) {
        auto defIt = ctx.sourceDefs.find(methodPath);
        const bool shouldInferImplicitTemplateTail =
            defIt != ctx.sourceDefs.end() &&
            !expr.templateArgs.empty() &&
            ctx.implicitTemplateDefs.count(methodPath) > 0 &&
            expr.templateArgs.size() < defIt->second.templateArgs.size();
        if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
          if (defIt != ctx.sourceDefs.end()) {
            std::vector<std::string> inferredArgs;
            if (inferImplicitTemplateArgs(defIt->second,
                                          expr,
                                          locals,
                                          params,
                                          mapping,
                                          allowedParams,
                                          namespacePrefix,
                                          ctx,
                                          allowMathBare,
                                          inferredArgs,
                                          error)) {
              expr.templateArgs = std::move(inferredArgs);
              allConcrete = true;
            } else {
              if (error.empty() &&
                  inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                          expr,
                                                          locals,
                                                          params,
                                                          mapping,
                                                          allowedParams,
                                                          namespacePrefix,
                                                          ctx,
                                                          allowMathBare,
                                                          inferredArgs)) {
                allConcrete = inferredTemplateArgsAreConcrete(defIt->second, inferredArgs);
                expr.templateArgs = std::move(inferredArgs);
              } else if (!error.empty()) {
                return false;
              }
            }
          }
        }
        if (expr.templateArgs.empty()) {
          if (shouldDeferStdlibCollectionHelperTemplateRewrite(methodPath)) {
            return true;
          }
          error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
          return false;
        }
        if (allConcrete) {
          std::string specializedPath;
          if (!instantiateTemplate(methodPath, expr.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return false;
      }
      std::string rewrittenMethodPath =
          expr.isMethodCall ? methodPath : resolveCalleePath(expr, namespacePrefix, ctx);
      auto methodDefIt = ctx.sourceDefs.find(rewrittenMethodPath);
      if (methodDefIt == ctx.sourceDefs.end()) {
        methodDefIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
      }
      if (methodDefIt != ctx.sourceDefs.end()) {
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteVectorTargetValueForResolvedType(typeText, argExpr);
                })) {
          return false;
        }
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteMapTargetValueForResolvedType(typeText, argExpr);
                })) {
          return false;
        }
      }
    }
  }

  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
  }
  LocalTypeMap bodyLocals = locals;
  for (auto &arg : expr.bodyArguments) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, bodyLocals, params, allowMathBare)) {
      return false;
    }
    BindingInfo info;
    if (extractExplicitBindingType(arg, info)) {
      if (info.typeName == "auto" && arg.args.size() == 1 &&
          inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      } else {
        bodyLocals[arg.name] = info;
      }
    } else if (arg.isBinding && arg.args.size() == 1) {
      if (inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      }
    }
  }
  return true;
}

bool rewriteExecution(Execution &exec, Context &ctx, std::string &error) {
  return rewriteExecutionEntry(exec, ctx, error);
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  auto defIt = ctx.sourceDefs.find(basePath);
  if (defIt == ctx.sourceDefs.end()) {
    error = "unknown template definition: " + basePath;
    return false;
  }
  const Definition &baseDef = defIt->second;
  if (baseDef.templateArgs.empty()) {
    error = "template arguments are only supported on templated definitions: " +
            helperOverloadDisplayPath(basePath, ctx);
    return false;
  }
  if (baseDef.templateArgs.size() != resolvedArgs.size()) {
    std::ostringstream out;
    out << "template argument count mismatch for " << helperOverloadDisplayPath(basePath, ctx) << ": expected "
        << baseDef.templateArgs.size()
        << ", got " << resolvedArgs.size();
    error = out.str();
    return false;
  }
  const std::string key = basePath + "<" + stripWhitespace(joinTemplateArgs(resolvedArgs)) + ">";
  auto cacheIt = ctx.specializationCache.find(key);
  if (cacheIt != ctx.specializationCache.end()) {
    specializedPathOut = cacheIt->second;
    return true;
  }

  const size_t lastSlash = basePath.find_last_of('/');
  const std::string baseName = lastSlash == std::string::npos ? basePath : basePath.substr(lastSlash + 1);
  const std::string suffix = mangleTemplateArgs(resolvedArgs);
  const std::string specializedName = baseName + suffix;
  const std::string specializedBasePath = (lastSlash == std::string::npos)
                                              ? specializedName
                                              : basePath.substr(0, lastSlash + 1) + specializedName;
  if (ctx.sourceDefs.count(specializedBasePath) > 0) {
    error = "template specialization conflicts with existing definition: " + specializedBasePath;
    return false;
  }
  ctx.specializationCache.emplace(key, specializedBasePath);
  if (!specializeTemplateDefinitionFamily(
          basePath, baseDef.templateArgs, resolvedArgs, specializedBasePath, specializedName, key, ctx, error)) {
    return false;
  }

  specializedPathOut = specializedBasePath;
  return true;
}

#include "TemplateMonomorphFinalOrchestration.h"

} // namespace

bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error) {
  Context ctx = makeTemplateMonomorphContext(program);

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  program.definitions = std::move(ctx.outputDefs);
  program.executions = std::move(ctx.outputExecs);
  return true;
}

} // namespace primec::semantics
