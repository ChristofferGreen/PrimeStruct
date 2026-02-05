#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec {

class Emitter {
public:
  struct BindingInfo {
    std::string typeName;
    std::string typeTemplateArg;
    bool isMutable = false;
  };
  enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, Void };
  std::string emitCpp(const Program &program, const std::string &entryPath) const;

private:
  std::string toCppName(const std::string &fullPath) const;
  std::string emitExpr(const Expr &expr,
                       const std::unordered_map<std::string, std::string> &nameMap,
                       const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                       const std::unordered_map<std::string, BindingInfo> &localTypes,
                       const std::unordered_map<std::string, ReturnKind> &returnKinds) const;
};

} // namespace primec
