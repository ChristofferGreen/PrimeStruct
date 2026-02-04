#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec {

class Emitter {
public:
  std::string emitCpp(const Program &program, const std::string &entryPath) const;

private:
  std::string toCppName(const std::string &fullPath) const;
  std::string emitExpr(const Expr &expr,
                       const std::unordered_map<std::string, std::string> &nameMap,
                       const std::unordered_map<std::string, std::vector<std::string>> &paramMap,
                       const std::unordered_map<std::string, std::string> &localTypes) const;
};

} // namespace primec
