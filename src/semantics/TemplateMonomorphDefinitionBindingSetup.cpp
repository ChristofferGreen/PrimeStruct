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

namespace primec {

using semantics::isPickCall;
using semantics::canonicalizeLegacySoaToAosHelperPath;
using semantics::isVectorCompatibilityHelperName;
using semantics::isExperimentalSoaGetLikeHelperPath;
using semantics::isExperimentalSoaRefLikeHelperPath;
using semantics::isPublishedVectorMutatorHelperName;

using semantics::isBindingAuxTransformName;
using semantics::isRootBuiltinName;
using semantics::isCanonicalVectorCompatibilityPath;
using semantics::getBuiltinArrayAccessName;
using semantics::isExperimentalSoaVectorTypePath;
using semantics::soaUnavailableMethodDiagnostic;
using semantics::trimLeadingSlash;

using semantics::canonicalizeLegacySoaRefHelperPath;
using semantics::isCompileTimeTypeBinding;
using semantics::isExperimentalSoaVectorHelperFamilyPath;
using semantics::isKeyValueCollectionTypeName;
using semantics::isLegacyExperimentalVectorCompatibilityPath;
using semantics::isLegacyExperimentalVectorCompatibilitySpecializedTypePath;
using semantics::legacyExperimentalVectorCompatibilityPrefix;
using semantics::preferredPublishedCollectionLoweringPath;
using semantics::resolveCanonicalVectorHelperNameFromResolvedPath;
using semantics::resolveVectorCompatibilityHelperNameFromResolvedPath;
using semantics::vectorHelperSurfaceMetadata;

using semantics::hasNamedArguments;
using semantics::getBuiltinPointerName;
using semantics::vectorConstructorSurfaceMetadata;
using semantics::canonicalVectorCompatibilityPrefixOrFallback;

using semantics::joinTemplateArgs;
using semantics::canonicalVectorTypeIdentityPrefix;
using semantics::legacyExperimentalVectorCompatibilityTypeText;
using semantics::returnKindForTypeName;
using semantics::resolveTypePath;
using semantics::mapCollectionAliasToken;
using semantics::stripUnrootedCanonicalVectorCompatibilityPrefix;
using semantics::publicSoaHelperTargetPath;
using semantics::isUnrootedCanonicalVectorCompatibilityPath;
using semantics::isSoftwareNumericTypeName;
using semantics::isLegacyOrCanonicalSoaHelperPath;
using semantics::isIfCall;
using semantics::isCanonicalSoaRefLikeHelperPath;
using semantics::compatibilitySoaHelperTargetPath;
using semantics::canonicalizeLegacySoaGetHelperPath;
using semantics::canonicalVectorCompatibilityHelperPathOrFallback;

using semantics::BindingInfo;
using semantics::ParameterInfo;
using semantics::ReturnKind;
using semantics::buildOrderedArguments;
using semantics::extractKeyValueCollectionTypesFromTypeText;
using semantics::getBuiltinCollectionName;
using semantics::isExperimentalSoaVectorSpecializedTypePath;
using semantics::isPrimitiveBindingTypeName;
using semantics::isReturnCall;
using semantics::isSimpleCallName;
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;


bool tryAppendDefinitionParameterBinding(Expr &param,
                                         bool allowMathBare,
                                         Context &ctx,
                                         LocalTypeMap &locals,
                                         std::vector<ParameterInfo> &paramsOut) {
  BindingInfo info;
  if (isCompileTimeTypeBinding(param)) {
    return false;
  }
  if (extractExplicitBindingType(param, info)) {
    if (info.typeName == "auto" && param.args.size() == 1 &&
        inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
      locals[param.name] = info;
    } else {
      locals[param.name] = info;
    }
    ParameterInfo paramInfo;
    paramInfo.name = param.name;
    paramInfo.binding = info;
    if (param.args.size() == 1) {
      paramInfo.defaultExpr = &param.args.front();
    }
    paramsOut.push_back(std::move(paramInfo));
    return true;
  }
  if (!param.isBinding || param.args.size() != 1) {
    return false;
  }
  if (!inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
    return false;
  }
  locals[param.name] = info;
  ParameterInfo paramInfo;
  paramInfo.name = param.name;
  paramInfo.binding = info;
  paramInfo.defaultExpr = &param.args.front();
  paramsOut.push_back(std::move(paramInfo));
  return true;
}

bool rewriteDefinitionParameters(std::vector<Expr> &parameters,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 LocalTypeMap &locals,
                                 std::vector<ParameterInfo> &paramsOut,
                                 bool allowMathBare) {
  for (auto &param : parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, namespacePrefix, ctx, error, locals, paramsOut, allowMathBare)) {
      return false;
    }
    tryAppendDefinitionParameterBinding(param, allowMathBare, ctx, locals, paramsOut);
  }
  return true;
}

void recordDefinitionStatementBindingLocal(Expr &stmt,
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           LocalTypeMap &localsOut) {
  BindingInfo info;
  if (isCompileTimeTypeBinding(stmt)) {
    info.typeName = "type";
    localsOut[stmt.name] = info;
    return;
  }
  if (extractExplicitBindingType(stmt, info)) {
    if (info.typeName == "auto" && stmt.args.size() == 1 &&
        inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
      localsOut[stmt.name] = info;
    } else {
      localsOut[stmt.name] = info;
    }
    return;
  }
  if (!stmt.isBinding || stmt.args.size() != 1) {
    return;
  }
  if (inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
    localsOut[stmt.name] = info;
  }
}


} // namespace primec
