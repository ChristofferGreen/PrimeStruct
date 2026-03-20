#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprCollectionLiteralBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  std::string builtinName;
  if (!getBuiltinCollectionName(expr, builtinName)) {
    return true;
  }

  handledOut = true;
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{
      .resolveBindingTarget =
          [&](const Expr &target, BindingInfo &bindingOut) -> bool {
            if (!(target.kind == Expr::Kind::Call && target.isFieldAccess &&
                  target.args.size() == 1)) {
              return false;
            }
            return resolveStructFieldBinding(params, locals, target.args.front(),
                                             target.name, bindingOut);
          },
      .inferCallBinding =
          [&](const Expr &target, BindingInfo &bindingOut) -> bool {
            if (target.kind != Expr::Kind::Call) {
              return false;
            }
            auto defIt = defMap_.find(resolveCalleePath(target));
            return defIt != defMap_.end() && defIt->second != nullptr &&
                   inferDefinitionReturnBinding(*defIt->second, bindingOut);
          }};
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals,
                                            builtinCollectionDispatchResolverAdapters);
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    error_ = builtinName + " literal does not accept block arguments";
    return false;
  }
  if (builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      error_ = "soa_vector literal requires exactly one template argument";
      return false;
    }
    if (!isSoaVectorStructElementType(expr.templateArgs.front(),
                                      expr.namespacePrefix, structNames_,
                                      importAliases_)) {
      error_ = "soa_vector literal requires struct element type";
      return false;
    }
    if (!validateSoaVectorElementFieldEnvelopes(expr.templateArgs.front(),
                                                expr.namespacePrefix)) {
      return false;
    }
    if (!expr.args.empty() && currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "soa_vector literal requires heap_alloc effect";
      return false;
    }
  }
  if (builtinName == "vector" && !expr.args.empty()) {
    if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "vector literal requires heap_alloc effect";
      return false;
    }
  }
  if (builtinName == "array" || builtinName == "vector" ||
      builtinName == "soa_vector") {
    if (expr.templateArgs.size() != 1) {
      if (builtinName == "array" && expr.templateArgs.size() > 1) {
        error_ =
            "array<T, N> is unsupported; use array<T> (runtime-count array)";
        return false;
      }
      error_ = builtinName + " literal requires exactly one template argument";
      return false;
    }
  } else {
    if (expr.templateArgs.size() != 2) {
      error_ = "map literal requires exactly two template arguments";
      return false;
    }
    if (expr.args.size() % 2 != 0) {
      error_ = "map literal requires an even number of arguments";
      return false;
    }
  }
  for (const auto &arg : expr.args) {
    if (!validateExpr(params, locals, arg)) {
      return false;
    }
  }
  if ((builtinName == "array" || builtinName == "vector" ||
       builtinName == "soa_vector") &&
      !expr.templateArgs.empty()) {
    const std::string &elemType = expr.templateArgs.front();
    for (const auto &arg : expr.args) {
      if (!this->validateCollectionElementType(
              arg, elemType, builtinName + " literal requires element type ",
              params, locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  if (builtinName == "map" && expr.templateArgs.size() == 2) {
    if (!validateBuiltinMapKeyType(expr.templateArgs.front(), nullptr, error_)) {
      return false;
    }
    const std::string &keyType = expr.templateArgs[0];
    const std::string &valueType = expr.templateArgs[1];
    for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
      if (!this->validateCollectionElementType(
              expr.args[i], keyType, "map literal requires key type ", params,
              locals, builtinCollectionDispatchResolvers)) {
        return false;
      }
      if (!this->validateCollectionElementType(
              expr.args[i + 1], valueType,
              "map literal requires value type ", params, locals,
              builtinCollectionDispatchResolvers)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
