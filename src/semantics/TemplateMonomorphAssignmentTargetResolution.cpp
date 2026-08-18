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


bool inferCallTargetBinding(const Expr &bindingExpr,
                            bool allowMathBare,
                            Context &ctx,
                            BindingInfo &bindingOut) {
  const bool hasExplicitBinding = extractExplicitBindingType(bindingExpr, bindingOut);
  if (hasExplicitBinding && bindingOut.typeName != "auto") {
    return true;
  }
  if (bindingExpr.args.size() != 1) {
    return hasExplicitBinding;
  }
  BindingInfo inferredBinding;
  if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), {}, {}, allowMathBare, ctx, inferredBinding)) {
    Expr rewrittenInitializer = bindingExpr.args.front();
    std::string rewriteError;
    LocalTypeMap emptyLocals;
    std::vector<ParameterInfo> emptyParams;
    if (!rewriteExpr(rewrittenInitializer,
                     SubstMap{},
                     {},
                     bindingExpr.namespacePrefix,
                     ctx,
                     rewriteError,
                     emptyLocals,
                     emptyParams,
                     allowMathBare) ||
        !inferBindingTypeForMonomorph(rewrittenInitializer, {}, {}, allowMathBare, ctx, inferredBinding)) {
      return hasExplicitBinding;
    }
  }
  bindingOut = inferredBinding;
  return true;
}

bool resolveFieldBindingTarget(const Expr &target,
                               const std::vector<ParameterInfo> &params,
                               const LocalTypeMap &locals,
                               bool allowMathBare,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               BindingInfo &bindingOut) {
  if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
    return false;
  }
  const Expr &receiver = target.args.front();
  std::string receiverTypeText;
  BindingInfo receiverInfo;
  if (resolveAssignmentTargetBinding(receiver, params, locals, allowMathBare, namespacePrefix, ctx, receiverInfo)) {
    receiverTypeText = bindingTypeToString(receiverInfo);
  } else if (inferBindingTypeForMonomorph(receiver, params, locals, allowMathBare, ctx, receiverInfo)) {
    receiverTypeText = bindingTypeToString(receiverInfo);
  }
  if (receiverTypeText.empty()) {
    receiverTypeText =
        inferExprTypeTextForTemplatedVectorFallback(receiver, locals, namespacePrefix, ctx, allowMathBare);
  }
  if (receiverTypeText.empty()) {
    return false;
  }
  receiverTypeText = normalizeBindingTypeName(receiverTypeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
      break;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      receiverTypeText = base;
      break;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    receiverTypeText = normalizeBindingTypeName(args.front());
  }
  std::string receiverStructPath = receiverTypeText;
  std::string receiverBase;
  std::string receiverArgText;
  if (splitTemplateTypeName(receiverStructPath, receiverBase, receiverArgText) && !receiverBase.empty()) {
    receiverStructPath = normalizeBindingTypeName(receiverBase);
  }
  if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
    receiverStructPath = resolveTypePath(receiverStructPath, receiver.namespacePrefix);
  }
  auto structIt = ctx.sourceDefs.find(receiverStructPath);
  if (structIt == ctx.sourceDefs.end() || !isStructDefinition(structIt->second)) {
    return false;
  }
  for (const auto &fieldStmt : structIt->second.statements) {
    if (!fieldStmt.isBinding || fieldStmt.name != target.name) {
      continue;
    }
    return inferCallTargetBinding(fieldStmt, allowMathBare, ctx, bindingOut);
  }
  return false;
}

bool resolveDereferenceBindingTarget(const Expr &target,
                                     const std::vector<ParameterInfo> &params,
                                     const LocalTypeMap &locals,
                                     bool allowMathBare,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     BindingInfo &bindingOut) {
  if (target.kind != Expr::Kind::Call || target.args.size() != 1) {
    return false;
  }
  std::string pointerBuiltin;
  if (!getBuiltinPointerName(target, pointerBuiltin) || pointerBuiltin != "dereference") {
    return false;
  }
  auto inferPointerBinding = [&](const Expr &pointerExpr, BindingInfo &pointerOut) -> bool {
    if (inferBindingTypeForMonomorph(pointerExpr, params, locals, allowMathBare, ctx, pointerOut)) {
      return true;
    }
    if (pointerExpr.kind != Expr::Kind::Call || pointerExpr.args.size() != 1) {
      return false;
    }
    std::string nestedPointerBuiltin;
    if (!getBuiltinPointerName(pointerExpr, nestedPointerBuiltin) || nestedPointerBuiltin != "location") {
      return false;
    }
    BindingInfo pointeeInfo;
    if (!resolveAssignmentTargetBinding(
            pointerExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, pointeeInfo)) {
      return false;
    }
    const std::string pointeeTypeText = bindingTypeToString(pointeeInfo);
    if (pointeeTypeText.empty()) {
      return false;
    }
    pointerOut.typeName = "Reference";
    pointerOut.typeTemplateArg = pointeeTypeText;
    return true;
  };
  BindingInfo pointerInfo;
  if (!inferPointerBinding(target.args.front(), pointerInfo)) {
    return false;
  }
  const std::string normalizedPointerType = normalizeBindingTypeName(pointerInfo.typeName);
  if ((normalizedPointerType != "Reference" && normalizedPointerType != "Pointer") ||
      pointerInfo.typeTemplateArg.empty()) {
    return false;
  }
  std::string pointeeBase;
  std::string pointeeArgText;
  if (splitTemplateTypeName(pointerInfo.typeTemplateArg, pointeeBase, pointeeArgText) && !pointeeBase.empty()) {
    bindingOut.typeName = pointeeBase;
    bindingOut.typeTemplateArg = pointeeArgText;
  } else {
    bindingOut.typeName = pointerInfo.typeTemplateArg;
    bindingOut.typeTemplateArg.clear();
  }
  return true;
}

bool resolveAssignmentTargetBinding(const Expr &target,
                                    const std::vector<ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx,
                                    BindingInfo &bindingOut) {
  return inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx, bindingOut) ||
         resolveFieldBindingTarget(target, params, locals, allowMathBare, namespacePrefix, ctx, bindingOut) ||
         resolveDereferenceBindingTarget(target, params, locals, allowMathBare, namespacePrefix, ctx, bindingOut);
}


} // namespace primec
