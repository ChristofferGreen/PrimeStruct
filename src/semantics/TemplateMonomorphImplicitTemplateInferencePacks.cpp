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
#include "TemplateMonomorphImplicitTemplateInferencePacks.h"
#include "TemplateMonomorphImplicitTemplateInferenceLambdas.h"
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

namespace primec {

bool extractSpecializedSumTemplateArgsFromTypeText(std::string typeText, std::string paramBaseType, const std::vector<std::string> &paramNames, std::vector<std::string> &templateArgsOut, Context &ctx) {
  templateArgsOut.clear();
  if (paramNames.size() != 1) {
    return false;
  }
  typeText = semantics::normalizeBindingTypeName(typeText);
  paramBaseType = semantics::normalizeBindingTypeName(paramBaseType);
  if (typeText.empty() || paramBaseType.empty()) {
    return false;
  }
  if (typeText.front() != '/') {
    typeText.insert(typeText.begin(), '/');
  }
  if (paramBaseType.front() != '/') {
    paramBaseType.insert(paramBaseType.begin(), '/');
  }
  const size_t paramLeafStart = paramBaseType.find_last_of('/');
  const size_t paramSearchStart =
      paramLeafStart == std::string::npos ? 0 : paramLeafStart + 1;
  if (const size_t generatedSuffix = paramBaseType.find("__", paramSearchStart);
      generatedSuffix != std::string::npos) {
    paramBaseType.erase(generatedSuffix);
  }
  if (typeText.rfind(paramBaseType + "__t", 0) != 0) {
    const size_t typeLeafStart = typeText.find_last_of('/');
    const std::string typeLeaf =
        typeLeafStart == std::string::npos
            ? typeText
            : typeText.substr(typeLeafStart + 1);
    const size_t paramLeafStartForMatch = paramBaseType.find_last_of('/');
    const std::string paramLeaf =
        paramLeafStartForMatch == std::string::npos
            ? paramBaseType
            : paramBaseType.substr(paramLeafStartForMatch + 1);
    if (typeLeaf.rfind(paramLeaf + "__t", 0) != 0) {
      return false;
    }
  }
  auto defIt = ctx.sourceDefs.find(typeText);
  if (defIt == ctx.sourceDefs.end() || !isSumDefinitionForMonomorphRefresh(defIt->second)) {
    return false;
  }
  std::string payloadType;
  for (const SumVariant &variant : defIt->second.sumVariants) {
    if (!variant.hasPayload) {
      continue;
    }
    std::string candidate = !variant.payloadTypeText.empty()
                                ? variant.payloadTypeText
                                : [&]() {
                                    semantics::BindingInfo payloadBinding;
                                    payloadBinding.typeName = variant.payloadType;
                                    payloadBinding.typeTemplateArg =
                                        semantics::joinTemplateArgs(variant.payloadTemplateArgs);
                                    return bindingTypeToString(payloadBinding);
                                  }();
    candidate = semantics::normalizeBindingTypeName(candidate);
    if (candidate.empty()) {
      continue;
    }
    if (!payloadType.empty() && payloadType != candidate) {
      return false;
    }
    payloadType = std::move(candidate);
  }
  if (payloadType.empty()) {
    return false;
  }
  templateArgsOut.push_back(payloadType);
  return true;
}

bool buildTypePackOrderedArguments(const Definition &def,
                                   const std::vector<semantics::ParameterInfo> &callParams,
                                   const std::vector<Expr> *orderedCallArgs,
                                   const std::vector<std::optional<std::string>> *orderedCallArgNames,
                                   size_t typePackParamIndex,
                                   std::vector<const Expr *> &orderedArgs,
                                   std::vector<const Expr *> &packedArgs,
                                   size_t &packedParamIndex,
                                   std::string &error) {
  orderedArgs.assign(callParams.size(), nullptr);
  packedArgs.clear();
  packedParamIndex = typePackParamIndex;
  size_t positionalIndex = 0;
  for (size_t i = 0; i < orderedCallArgs->size(); ++i) {
    const Expr &arg = (*orderedCallArgs)[i];
    if (i < orderedCallArgNames->size() && (*orderedCallArgNames)[i].has_value()) {
      const std::string &name = *(*orderedCallArgNames)[i];
      size_t namedIndex = callParams.size();
      for (size_t paramIndex = 0; paramIndex < callParams.size(); ++paramIndex) {
        if (callParams[paramIndex].name == name) {
          namedIndex = paramIndex;
          break;
        }
      }
      if (namedIndex >= callParams.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (namedIndex == typePackParamIndex) {
        error = "named arguments cannot bind heterogeneous value-pack parameter: " +
                name;
        return false;
      }
      if (orderedArgs[namedIndex] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      orderedArgs[namedIndex] = &arg;
      continue;
    }
    if (arg.isSpread) {
      error = "heterogeneous value-pack inference does not support spread forwarding on " +
              def.fullPath;
      return false;
    }
    while (positionalIndex < typePackParamIndex &&
           orderedArgs[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= typePackParamIndex) {
      packedArgs.push_back(&arg);
      continue;
    }
    orderedArgs[positionalIndex] = &arg;
    ++positionalIndex;
  }
  for (size_t i = 0; i < typePackParamIndex; ++i) {
    if (orderedArgs[i] != nullptr) {
      continue;
    }
    if (callParams[i].defaultExpr != nullptr) {
      orderedArgs[i] = callParams[i].defaultExpr;
      continue;
    }
    error = "argument count mismatch for " + def.fullPath;
    return false;
  }
  return true;
}

} // namespace primec
