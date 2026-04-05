#include "SemanticsValidator.h"

#include <cctype>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::exprUsesName(const Expr &expr, const std::string &name) const {
  if (name.empty()) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    return true;
  }
  if (expr.isLambda) {
    for (const auto &capture : expr.lambdaCaptures) {
      std::string token;
      std::vector<std::string> tokens;
      for (char c : capture) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
          if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
          }
          continue;
        }
        token.push_back(c);
      }
      if (!token.empty()) {
        tokens.push_back(token);
      }
      if (tokens.size() == 1 && tokens.front() == name) {
        return true;
      }
      if (tokens.size() == 2 && tokens.back() == name) {
        return true;
      }
    }
    for (const auto &param : expr.args) {
      for (const auto &defaultArg : param.args) {
        if (exprUsesName(defaultArg, name)) {
          return true;
        }
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesName(bodyExpr, name)) {
        return true;
      }
    }
    return false;
  }
  for (const auto &arg : expr.args) {
    if (exprUsesName(arg, name)) {
      return true;
    }
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (exprUsesName(bodyExpr, name)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::statementsUseNameFrom(const std::vector<Expr> &statements,
                                               size_t startIndex,
                                               const std::string &name) const {
  for (size_t index = startIndex; index < statements.size(); ++index) {
    if (exprUsesName(statements[index], name)) {
      return true;
    }
  }
  return false;
}

void SemanticsValidator::expireReferenceBorrowsForRemainder(const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            const std::vector<Expr> &statements,
                                                            size_t nextIndex) {
  std::function<bool(const Expr &, const std::string &, bool)> exprUsesNameOutsideWriteTarget;
  exprUsesNameOutsideWriteTarget = [&](const Expr &expr, const std::string &name, bool inWriteTarget) -> bool {
    if (name.empty()) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name && expr.name == name) {
      return !inWriteTarget;
    }
    if (expr.isLambda) {
      if (!inWriteTarget) {
        for (const auto &capture : expr.lambdaCaptures) {
          std::string token;
          std::vector<std::string> tokens;
          for (char c : capture) {
            if (std::isspace(static_cast<unsigned char>(c)) != 0) {
              if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
              }
              continue;
            }
            token.push_back(c);
          }
          if (!token.empty()) {
            tokens.push_back(token);
          }
          if ((tokens.size() == 1 && tokens.front() == name) || (tokens.size() == 2 && tokens.back() == name)) {
            return true;
          }
        }
      }
      for (const auto &param : expr.args) {
        for (const auto &defaultArg : param.args) {
          if (exprUsesNameOutsideWriteTarget(defaultArg, name, false)) {
            return true;
          }
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    if (isAssignCall(expr) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    for (const auto &arg : expr.args) {
      if (exprUsesNameOutsideWriteTarget(arg, name, inWriteTarget)) {
        return true;
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesNameOutsideWriteTarget(bodyExpr, name, inWriteTarget)) {
        return true;
      }
    }
    return false;
  };
  auto statementsUseNameOutsideWriteTargetsFrom =
      [&](const std::vector<Expr> &candidates, size_t startIndex, const std::string &name) -> bool {
    for (size_t index = startIndex; index < candidates.size(); ++index) {
      if (exprUsesNameOutsideWriteTarget(candidates[index], name, false)) {
        return true;
      }
    }
    return false;
  };
  auto isSoaFieldViewBindingType = [](const BindingInfo &binding) -> bool {
    std::string normalized = normalizeBindingTypeName(binding.typeName);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalized, base, arg)) {
      normalized = normalizeBindingTypeName(base);
    }
    return normalized == "SoaFieldView" ||
           normalized == "std/collections/experimental_soa_storage/SoaFieldView";
  };
  auto referenceRootForBinding = [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
    if (binding.typeName != "Reference" &&
        !isSoaFieldViewBindingType(binding)) {
      return "";
    }
    if (!binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return bindingName;
  };
  auto pointerAliasUsesReferenceRoot = [&](const std::string &referenceRoot) -> bool {
    if (referenceRoot.empty()) {
      return false;
    }
    auto pointerUsesRoot = [&](const std::string &bindingName, const BindingInfo &binding) -> bool {
      if (binding.typeName != "Pointer" || binding.referenceRoot != referenceRoot) {
        return false;
      }
      return statementsUseNameOutsideWriteTargetsFrom(statements, nextIndex, bindingName);
    };
    for (const auto &param : params) {
      if (pointerUsesRoot(param.name, param.binding)) {
        return true;
      }
    }
    for (const auto &entry : locals) {
      if (pointerUsesRoot(entry.first, entry.second)) {
        return true;
      }
    }
    return false;
  };
  auto updateName = [&](const std::string &bindingName, const BindingInfo &binding) {
    if (binding.typeName != "Reference" &&
        !isSoaFieldViewBindingType(binding)) {
      return;
    }
    const std::string referenceRoot = referenceRootForBinding(bindingName, binding);
    if (statementsUseNameFrom(statements, nextIndex, bindingName) ||
        pointerAliasUsesReferenceRoot(referenceRoot)) {
      currentValidationState_.endedReferenceBorrows.erase(bindingName);
      return;
    }
    currentValidationState_.endedReferenceBorrows.insert(bindingName);
  };
  for (const auto &param : params) {
    updateName(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    updateName(entry.first, entry.second);
  }
}

} // namespace primec::semantics
