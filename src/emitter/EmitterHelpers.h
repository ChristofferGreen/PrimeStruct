#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Emitter.h"

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

std::string joinTemplateArgs(const std::vector<std::string> &args);
ReturnKind getReturnKind(const Definition &def);
bool isPrimitiveBindingTypeName(const std::string &name);
std::string normalizeBindingTypeName(const std::string &name);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isBindingQualifierName(const std::string &name);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
BindingInfo getBindingInfo(const Expr &expr);
std::string bindingTypeToCpp(const BindingInfo &info);
std::string bindingTypeToCpp(const std::string &typeName);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
std::string typeNameForReturnKind(ReturnKind kind);
ReturnKind returnKindForTypeName(const std::string &name);
ReturnKind combineNumericKinds(ReturnKind left, ReturnKind right);
ReturnKind inferPrimitiveReturnKind(const Expr &expr,
                                   const std::unordered_map<std::string, BindingInfo> &localTypes,
                                   const std::unordered_map<std::string, ReturnKind> &returnKinds,
                                   bool allowMathBare);

bool getBuiltinOperator(const Expr &expr, char &out);
bool getBuiltinComparison(const Expr &expr, const char *&out);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool isPathSpaceBuiltinName(const Expr &expr);
std::string stripStringLiteralSuffix(const std::string &token);
bool isBuiltinNegate(const Expr &expr);
bool isBuiltinClamp(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstantName(const std::string &name, bool allowBare);
bool getBuiltinPointerOperator(const Expr &expr, char &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
std::string resolveExprPath(const Expr &expr);
bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           std::string &resolvedOut);
bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
std::vector<const Expr *> orderCallArguments(const Expr &expr, const std::vector<Expr> &params);
bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isRepeatCall(const Expr &expr);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isReturnCall(const Expr &expr);

} // namespace primec::emitter
