#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isMutable = false;
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

enum class PathSpaceTarget { Notify, Insert, Take };

struct PathSpaceBuiltin {
  PathSpaceTarget target = PathSpaceTarget::Notify;
  std::string name;
  std::string requiredEffect;
  size_t argumentCount = 0;
};

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);

bool isBindingQualifierName(const std::string &name);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool isPrimitiveBindingTypeName(const std::string &name);
bool isSoftwareNumericTypeName(const std::string &name);
std::optional<std::string> findSoftwareNumericType(const std::string &typeName);
std::string normalizeBindingTypeName(const std::string &name);
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
ReturnKind getReturnKind(const Definition &def, std::string &error);

bool getBuiltinOperatorName(const Expr &expr, std::string &out);
bool getBuiltinComparisonName(const Expr &expr, std::string &out);
bool isRootBuiltinName(const std::string &name);
bool getBuiltinClampName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstant(const std::string &name, bool allowBare);
bool getBuiltinPointerName(const Expr &expr, std::string &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool getBuiltinArrayAccessName(const Expr &expr, std::string &out);
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
bool isReturnCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool isBlockCall(const Expr &expr);

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

bool isStructTransformName(const std::string &name);
bool validateNamedArguments(const std::vector<Expr> &args,
                            const std::vector<std::optional<std::string>> &argNames,
                            const std::string &context,
                            std::string &error);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isDefaultExprAllowed(const Expr &expr);
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
bool validateNamedArgumentsAgainstParams(const std::vector<ParameterInfo> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error);

} // namespace primec::semantics
