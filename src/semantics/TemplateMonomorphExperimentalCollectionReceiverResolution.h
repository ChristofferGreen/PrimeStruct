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

std::string experimentalCollectionValueBindingTypeText(const BindingInfo &binding);

std::string experimentalCollectionBorrowedBindingTypeText(const BindingInfo &binding);

std::string experimentalKeyValueBackingLeafForReceiverResolution(std::string typeName);

bool isUnspecializedExperimentalKeyValueBackingTypeForReceiverResolution(
    std::string typeName);

bool isSpecializedExperimentalKeyValueBackingTypeForReceiverResolution(
    std::string typeName);

bool isSpecializedCanonicalKeyValueBackingTypeForReceiverResolution(
    std::string typeName);

bool isRootMapConstructorReceiverExpr(const Expr *receiverExpr);

bool isPublishedMapConstructorReceiverExpr(const Expr *receiverExpr,
                                           const std::string &namespacePrefix,
                                           Context &ctx);

bool inferPublishedMapConstructorReceiverTemplateArgs(
    const Expr *receiverExpr,
    const std::vector<ParameterInfo> &params,
    const LocalTypeMap &locals,
    bool allowMathBare,
    const std::string &namespacePrefix,
    Context &ctx,
    std::vector<std::string> &templateArgsOut);

bool extractCollectionVectorElementTypeFromTypeText(const std::string &typeText,
                                                      const Context &ctx,
                                                      std::string &valueTypeOut);

bool extractExperimentalKeyValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                 const Context &ctx,
                                                                 std::vector<std::string> &templateArgsOut);

bool extractCollectionVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                    const Context &ctx,
                                                                    std::vector<std::string> &templateArgsOut);

bool extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                       const Context &ctx,
                                                                       std::vector<std::string> &templateArgsOut);

bool resolvesExperimentalKeyValueReceiver(const Expr *receiverExpr,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          bool allowMathBare,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx);

bool resolvesExperimentalKeyValueBorrowedReceiver(const Expr *receiverExpr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const LocalTypeMap &locals,
                                                  bool allowMathBare,
                                                  const SubstMap &mapping,
                                                  const std::unordered_set<std::string> &allowedParams,
                                                  const std::string &namespacePrefix,
                                                  Context &ctx);

bool resolvesCollectionVectorValueReceiver(const Expr *receiverExpr,
                                             const std::vector<ParameterInfo> &params,
                                             const LocalTypeMap &locals,
                                             bool allowMathBare,
                                             const std::string &namespacePrefix,
                                             Context &ctx);

const StdlibSurfaceMetadata *templateMonomorphKeyValueHelperSurfaceMetadata();

bool resolveTemplateMonomorphKeyValueHelperName(
    std::string path,
    std::string &helperNameOut);

bool isTemplateMonomorphMapImportAliasHelperPath(std::string_view path);

std::string templateMonomorphCanonicalKeyValueHelperPath(std::string_view spelling);

bool resolveTemplateMonomorphCanonicalKeyValueHelperName(
    std::string path,
    std::string &helperNameOut);

bool isTemplateMonomorphCanonicalKeyValueHelperPath(const std::string &path);

bool isTemplateMonomorphCanonicalKeyValueAccessPath(const std::string &path);

bool isTemplateMonomorphCanonicalKeyValueCountPath(const std::string &path);

std::string templateMonomorphPreferredKeyValueHelperSpellingForMember(
    std::string_view spelling,
    std::string_view preferredPrefix);

std::string canonicalKeyValueHelperUnknownTargetPath(const std::string &resolvedPath);

bool resolveExperimentalKeyValueReceiverTemplateArgs(const Expr *receiverExpr,
                                                     const std::vector<ParameterInfo> &params,
                                                     const LocalTypeMap &locals,
                                                     bool allowMathBare,
                                                     const std::string &namespacePrefix,
                                                     Context &ctx,
                                                     std::vector<std::string> &templateArgsOut);

std::string experimentalKeyValueHelperPathForCanonicalHelper(const std::string &path);

std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path);

std::string experimentalSoaVectorHelperPathForCanonicalHelper(const std::string &path);

bool isCollectionVectorPublicHelperPath(const std::string &path);

bool isExperimentalSoaVectorPublicHelperPath(const std::string &path);

bool hasVisibleStdCollectionsImportForPath(const Context &ctx, const std::string &path);

std::string experimentalKeyValueHelperPathForWrapperHelper(
    const std::string &path);

bool resolveCollectionVectorValueReceiverTemplateArgs(const Expr *receiverExpr,
                                                        const std::vector<ParameterInfo> &params,
                                                        const LocalTypeMap &locals,
                                                        bool allowMathBare,
                                                        const std::string &namespacePrefix,
                                                        Context &ctx,
                                                        std::vector<std::string> &templateArgsOut);

} // namespace primec
