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
#include "TemplateMonomorphUsings.h"

namespace primec {

bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind);

std::string resolveStructLikeTypePathForTemplatedVectorFallback(const std::string &typeName,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx);

std::string resolveStructLikeExprPathForTemplatedVectorFallback(const Expr &expr,
                                                                const LocalTypeMap &locals,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx,
                                                                bool allowMathBare);

bool isUnspecializedExperimentalKeyValueBackingTypeForFallbackInference(
    std::string typeName);

bool isSpecializedExperimentalKeyValueBackingTypeForFallbackInference(
    std::string typeName);

bool resolvesExperimentalKeyValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx);

struct TemplatedFallbackQueryStateAdapterData {
  std::string queryTypeText;
  BindingInfo receiverBinding;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  std::string mismatchDiagnostic;
};

void populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(
    const std::string &queryTypeText,
    TemplatedFallbackQueryStateAdapterData &out);

bool inferDefinitionReturnBindingForTemplatedFallback(const Definition &def,
                                                      bool allowMathBare,
                                                      Context &ctx,
                                                      BindingInfo &infoOut);

std::string inferExprTypeTextForTemplatedVectorFallback(const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::string &namespacePrefix,
                                                        const Context &ctx,
                                                        bool allowMathBare);

bool inferTemplatedFallbackQueryStateAdapter(const Expr &expr,
                                             const LocalTypeMap &locals,
                                             const std::vector<ParameterInfo> &params,
                                             const std::string &namespacePrefix,
                                             Context &ctx,
                                             bool allowMathBare,
                                             TemplatedFallbackQueryStateAdapterData &out);

bool shouldPreferTemplatedVectorFallbackForTypeMismatch(const Definition &def,
                                                        const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<ParameterInfo> &params,
                                                        bool allowMathBare,
                                                        Context &ctx,
                                                        const std::string &namespacePrefix);

std::string preferVectorStdlibImplicitTemplatePath(const Expr &expr,
                                                   const std::string &path,
                                                   const LocalTypeMap &locals,
                                                   const std::vector<ParameterInfo> &params,
                                                   bool allowMathBare,
                                                   Context &ctx,
                                                   const std::string &namespacePrefix);

} // namespace primec
