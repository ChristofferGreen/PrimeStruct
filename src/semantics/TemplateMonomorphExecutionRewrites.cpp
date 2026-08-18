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


bool rewriteExecutionEntry(Execution &exec, Context &ctx, std::string &error) {
  if (!rewriteTransforms(exec.transforms, SubstMap{}, {}, exec.namespacePrefix, ctx, error)) {
    return false;
  }
  bool allConcrete = true;
  for (auto &templArg : exec.templateArgs) {
    ResolvedType resolved = resolveTypeString(templArg, SubstMap{}, {}, exec.namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolved.concrete) {
      allConcrete = false;
    }
    templArg = resolved.text;
  }

  Expr execExpr;
  execExpr.kind = Expr::Kind::Call;
  execExpr.name = exec.name;
  execExpr.namespacePrefix = exec.namespacePrefix;
  execExpr.templateArgs = exec.templateArgs;
  execExpr.templateArgDetails = exec.templateArgDetails;
  execExpr.args = exec.arguments;
  execExpr.argNames = exec.argumentNames;
  execExpr.bodyArguments = exec.bodyArguments;
  execExpr.hasBodyArguments = exec.hasBodyArguments;

  LocalTypeMap emptyLocals;
  const bool allowMathBare = hasMathImport(ctx);
  if (!rewriteExpr(execExpr, SubstMap{}, {}, exec.namespacePrefix, ctx, error, emptyLocals, {}, allowMathBare)) {
    return false;
  }

  exec.name = execExpr.name;
  exec.namespacePrefix = execExpr.namespacePrefix;
  exec.templateArgs = execExpr.templateArgs;
  exec.templateArgDetails = execExpr.templateArgDetails;
  exec.arguments = execExpr.args;
  exec.argumentNames = execExpr.argNames;
  exec.bodyArguments = execExpr.bodyArguments;
  exec.hasBodyArguments = execExpr.hasBodyArguments;

  if (ctx.templateDefs.count(resolveCalleePath(execExpr, exec.namespacePrefix, ctx)) > 0 && exec.templateArgs.empty()) {
    error = "template arguments required for " +
            helperOverloadDisplayPath(resolveCalleePath(execExpr, exec.namespacePrefix, ctx), ctx);
    return false;
  }
  if (ctx.templateDefs.count(resolveCalleePath(execExpr, exec.namespacePrefix, ctx)) > 0 && !allConcrete) {
    error = "template arguments must be concrete on executions";
    return false;
  }
  return true;
}


} // namespace primec
