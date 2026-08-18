#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"

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
using semantics::isAssignCall;
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;



bool resolveExperimentalConstructorTargetTypeText(const Expr &targetExpr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const LocalTypeMap &locals,
                                                  bool allowMathBare,
                                                  const std::string &namespacePrefix,
                                                  Context &ctx,
                                                  std::string &targetTypeTextOut);



template <typename RewriteTargetValueFn>
void rewriteExperimentalAssignTargetValue(Expr &callExpr,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          bool allowMathBare,
                                          const std::string &namespacePrefix,
                                          Context &ctx,
                                          RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isAssignCall(callExpr) || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}



template <typename RewriteTargetValueFn>
void rewriteExperimentalInitTargetValue(Expr &callExpr,
                                        const std::vector<ParameterInfo> &params,
                                        const LocalTypeMap &locals,
                                        bool allowMathBare,
                                        const std::string &namespacePrefix,
                                        Context &ctx,
                                        RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isSimpleCallName(callExpr, "init") || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}



} // namespace primec
