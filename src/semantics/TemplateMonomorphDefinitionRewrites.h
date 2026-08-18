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
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;



bool isSumDefinitionForMonomorphRefresh(const Definition &def);

std::string sumPayloadTypeTextForMonomorphRefresh(const Transform &transform);

std::string stripGeneratedSumUnitVariantSuffix(std::string name);

std::string generatedSumUnitVariantName(const Expr &stmt, const Definition &def);

void refreshMonomorphizedSumVariants(Definition &def);

bool isTupleDestructuringIdentifier(std::string_view name);

bool isPrimitiveTupleDestructuringTypeName(const std::string &name);

bool isKnownTupleDestructuringBracketEntry(const Transform &transform,
                                           const std::string &namespacePrefix,
                                           const Context &ctx);

bool isStdlibTupleMonomorphPath(std::string_view path);

std::string resolveTupleMonomorphBasePath(const std::string &base,
                                          const std::string &namespacePrefix,
                                          const Context &ctx);

bool extractTupleDestructuringArgsFromTypeText(std::string typeText,
                                               const std::string &namespacePrefix,
                                               const Context &ctx,
                                               bool &borrowedOut,
                                               std::vector<std::string> &tupleArgsOut);

const BindingInfo *findTupleDestructuringOperandBinding(
    std::string_view name,
    const std::vector<ParameterInfo> &params,
    const LocalTypeMap &locals);

bool isTupleDestructuringStatementCandidate(const Expr &stmt,
                                            const std::vector<ParameterInfo> &params,
                                            const LocalTypeMap &locals);

Expr makeTupleDestructuringNameExpr(const std::string &name,
                                    const Expr &source);

Expr makeTupleDestructuringGetExpr(const std::string &receiverName,
                                   size_t index,
                                   const std::vector<std::string> &tupleArgs,
                                   const Expr &source);

Expr makeTupleDestructuringBindingExpr(const std::string &bindingName,
                                       const std::string &receiverName,
                                       size_t index,
                                       const std::vector<std::string> &tupleArgs,
                                       const Expr &source);

bool tryExpandTupleDestructuringStatement(const Expr &stmt,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          const std::string &namespacePrefix,
                                          Context &ctx,
                                          std::string &error,
                                          std::vector<Expr> &expandedOut);

bool hasTypePackExpansionTransform(const Expr &expr);

std::string generatedPackFieldName(const std::string &sourceName,
                                   size_t index);

std::string templateSpecializationBasePath(std::string path);

enum class TypePackBindingExpansionKind;

const char *typePackBindingExpansionLabel(TypePackBindingExpansionKind kind);

std::string typePackBindingExpansionPath(const Definition &def,
                                         const Expr &binding,
                                         TypePackBindingExpansionKind kind);

bool expandTypePackBindingList(Definition &def,
                               std::vector<Expr> &bindings,
                               TypePackBindingExpansionKind kind,
                               std::string &error);

bool expandTypePackHelperBindings(Definition &def, std::string &error);

bool rejectUnsupportedTypePackStatementExpansions(const Definition &def, std::string &error);

bool rewriteDefinition(Definition &def,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       Context &ctx,
                       std::string &error);



} // namespace primec
