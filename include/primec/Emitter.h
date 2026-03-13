#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec {

class Emitter {
public:
  enum class BindingVisibility { Private, Public };
  struct BindingInfo {
    std::string typeName;
    std::string typeTemplateArg;
    bool isMutable = false;
    bool isCopy = false;
    bool isStatic = false;
    BindingVisibility visibility = BindingVisibility::Public;
  };
  struct ResultInfo {
    bool isResult = false;
    bool hasValue = false;
    std::string valueType;
    std::string errorType;
  };
  enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, String, Void, Array };
  std::string emitCpp(const Program &program, const std::string &entryPath) const;

private:
  std::string toCppName(const std::string &fullPath) const;
  std::string emitExpr(const Expr &expr,
                       const std::unordered_map<std::string, std::string> &nameMap,
                       const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                       const std::unordered_map<std::string, const Definition *> &defMap,
                       const std::unordered_map<std::string, std::string> &structTypeMap,
                       const std::unordered_map<std::string, std::string> &importAliases,
                       const std::unordered_map<std::string, BindingInfo> &localTypes,
                       const std::unordered_map<std::string, ReturnKind> &returnKinds,
                       const std::unordered_map<std::string, ResultInfo> &resultInfos,
                       const std::unordered_map<std::string, std::string> &returnStructs,
                       bool allowMathBare) const;
};

} // namespace primec
