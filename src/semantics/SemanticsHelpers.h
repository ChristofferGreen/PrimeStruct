#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

enum class ReturnKind {
  Unknown,
  Int,
  Int64,
  UInt64,
  Float32,
  Float64,
  Integer,
  Decimal,
  Complex,
  Bool,
  String,
  Void,
  Array
};

struct BindingInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isMutable = false;
  bool isEntryArgString = false;
  bool isUnsafeReference = false;
  std::string referenceRoot;
};

struct ParameterInfo {
  std::string name;
  BindingInfo binding;
  const Expr *defaultExpr = nullptr;
};

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

enum class PathSpaceTarget { Notify, Insert, Take, Bind, Unbind, Schedule };

struct PathSpaceBuiltin {
  PathSpaceTarget target = PathSpaceTarget::Notify;
  std::string name;
  std::string requiredEffect;
  size_t argumentCount = 0;
};

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
std::string resolveStructTypePath(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structTypes);

bool isBindingQualifierName(const std::string &name);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool isPrimitiveBindingTypeName(const std::string &name);
bool isSoftwareNumericTypeName(const std::string &name);
std::optional<std::string> findSoftwareNumericType(const std::string &typeName);
std::string normalizeBindingTypeName(const std::string &name);
std::string unwrapReferencePointerTypeText(const std::string &typeText);
bool isBuiltinMapComparableKeyTypeName(const std::string &name);
bool validateBuiltinMapKeyType(const std::string &typeName,
                               const std::vector<std::string> *templateArgs,
                               std::string &error);
bool validateBuiltinMapKeyType(const BindingInfo &binding,
                               const std::vector<std::string> *templateArgs,
                               std::string &error);
bool isMapCollectionTypeName(const std::string &name);
bool returnsMapCollectionType(const std::string &typeText);
bool extractMapKeyValueTypesFromTypeText(const std::string &typeText,
                                         std::string &keyTypeOut,
                                         std::string &valueTypeOut);
bool extractMapKeyValueTypes(const BindingInfo &binding, std::string &keyTypeOut, std::string &valueTypeOut);
bool getArgsPackElementType(const BindingInfo &binding, std::string &elementTypeOut);
bool isArgsPackBinding(const BindingInfo &binding);
bool resolveArgsPackElementTypeForExpr(const Expr &expr,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       std::string &elementTypeOut);
bool findTrailingArgsPackParameter(const std::vector<ParameterInfo> &params,
                                   size_t &indexOut,
                                   std::string *elementTypeOut = nullptr);
std::string joinTemplateArgs(const std::vector<std::string> &args);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);
bool isBuiltinTemplateTypeName(const std::string &name);
bool restrictMatchesBinding(const std::string &restrictType,
                            const std::string &typeName,
                            const std::string &typeTemplateArg,
                            bool typeHasTemplate,
                            const std::string &namespacePrefix);
ReturnKind returnKindForTypeName(const std::string &name);
ReturnKind getReturnKind(const Definition &def,
                         const std::unordered_set<std::string> &structNames,
                         const std::unordered_map<std::string, std::string> &importAliases,
                         std::string &error);

bool getBuiltinOperatorName(const Expr &expr, std::string &out);
bool getBuiltinComparisonName(const Expr &expr, std::string &out);
bool getBuiltinMutationName(const Expr &expr, std::string &out);
bool isRootBuiltinName(const std::string &name);
bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverPath, std::string rawMethodName);
bool isExplicitRemovedCollectionCallAlias(std::string rawPath);
bool isLifecycleHelperName(const std::string &fullPath);
bool getBuiltinClampName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstant(const std::string &name, bool allowBare);
bool isMathBuiltinName(const std::string &name);
bool getBuiltinGpuName(const Expr &expr, std::string &out);
bool getBuiltinMemoryName(const Expr &expr, std::string &out);
bool getBuiltinPointerName(const Expr &expr, std::string &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool getBuiltinArrayAccessName(const Expr &expr, std::string &out);
bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut);
std::string soaFieldViewHelperPath(std::string_view fieldName);
bool splitSoaFieldViewHelperPath(std::string_view path, std::string *fieldNameOut = nullptr);
std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path);
std::string canonicalizeLegacySoaRefHelperPath(std::string_view path);
std::string canonicalizeLegacySoaGetHelperPath(std::string_view path);
bool isLegacyOrCanonicalSoaHelperPath(std::string_view path, std::string_view helperName);
bool isCanonicalStdlibSoaHelperPath(std::string_view path, std::string_view helperName);
bool isCanonicalSoaRefLikeHelperPath(std::string_view path);
bool isExperimentalSoaBorrowedHelperPath(std::string_view path);
bool isExperimentalSoaGetLikeHelperPath(std::string_view path);
bool isExperimentalSoaGrowthHelperPath(std::string_view path);
bool isExperimentalSoaFieldViewHelperPath(std::string_view path);
bool isExperimentalSoaFieldViewReadHelperPath(std::string_view path);
bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path);
bool isExperimentalSoaColumnSlotHelperPath(std::string_view path);
bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path);
bool isExperimentalSoaMethodRefLikeHelperPath(std::string_view path);
bool isExperimentalSoaRefLikeHelperPath(std::string_view path);
bool isExperimentalSoaVectorHelperFamilyPath(std::string_view path);
bool isExperimentalSoaVectorTypePath(std::string_view path);
bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path);
bool isExperimentalSoaVectorConversionFamilyPath(std::string_view path);
std::string soaUnavailableMethodDiagnostic(std::string_view resolvedPath);
bool isSoaVectorStructElementType(const std::string &typeArg,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structTypes,
                                  const std::unordered_map<std::string, std::string> &importAliases);
bool isAssignCall(const Expr &expr);
const BindingInfo *findBinding(const std::vector<ParameterInfo> &params,
                               const std::unordered_map<std::string, BindingInfo> &locals,
                               const std::string &name);
bool isPointerExpr(const Expr &expr,
                   const std::vector<ParameterInfo> &params,
                   const std::unordered_map<std::string, BindingInfo> &locals);
bool isPointerLikeExpr(const Expr &expr,
                       const std::vector<ParameterInfo> &params,
                       const std::unordered_map<std::string, BindingInfo> &locals);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool isIfCall(const Expr &expr);
bool isMatchCall(const Expr &expr);
bool isReturnCall(const Expr &expr);
bool isLoopCall(const Expr &expr);
bool isWhileCall(const Expr &expr);
bool isForCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool isBlockCall(const Expr &expr);

bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error);

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out);

bool parseBindingInfo(const Expr &expr,
                      const std::string &namespacePrefix,
                      const std::unordered_set<std::string> &structTypes,
                      const std::unordered_map<std::string, std::string> &importAliases,
                      BindingInfo &info,
                      std::optional<std::string> &restrictTypeOut,
                      std::string &error);
bool isEffectName(const std::string &text);
bool validateEffectsTransform(const Transform &transform, const std::string &context, std::string &error);
bool validateCapabilitiesTransform(const Transform &transform, const std::string &context, std::string &error);
bool parsePositiveIntArg(const std::string &text, int &value);

bool isStructTransformName(const std::string &name);
bool isReflectionTransformName(const std::string &name);
bool isSupportedReflectionGeneratorName(const std::string &name);
bool isReflectionMetadataQueryName(const std::string &name);
bool isReflectionMetadataQueryPath(const std::string &path);
bool isRuntimeReflectionPath(const std::string &path);
bool validateNamedArguments(const std::vector<Expr> &args,
                            const std::vector<std::optional<std::string>> &argNames,
                            const std::string &context,
                            std::string &error);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isDefaultExprAllowed(const Expr &expr);
bool isDefaultExprAllowed(const Expr &expr, const std::function<bool(const Expr &)> &resolvesToDefinition);
std::string inferPrimitiveBindingTypeFromInitializer(const Expr &expr);
bool tryInferBindingTypeFromInitializer(const Expr &initializer,
                                        const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                        BindingInfo &bindingOut,
                                        bool allowMathBare);
bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::string &error);
bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::vector<const Expr *> &packedArgs,
                           size_t &packedParamIndex,
                           std::string &error);
bool validateNamedArgumentsAgainstParams(const std::vector<ParameterInfo> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error);

} // namespace primec::semantics
