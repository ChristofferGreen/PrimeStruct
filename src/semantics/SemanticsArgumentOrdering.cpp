#include "SemanticsHelpers.h"

namespace primec::semantics {
namespace {

size_t resolveNamedParamIndex(const std::vector<ParameterInfo> &params, const std::string &name) {
  for (size_t p = 0; p < params.size(); ++p) {
    if (params[p].name == name) {
      return p;
    }
  }
  if (name == "index") {
    bool hasIndexParam = false;
    for (const auto &param : params) {
      if (param.name == "index") {
        hasIndexParam = true;
        break;
      }
    }
    if (!hasIndexParam) {
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == "key") {
          return p;
        }
      }
    }
  }
  return params.size();
}

} // namespace

bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::string &error) {
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = params.size();
  return buildOrderedArguments(params, args, argNames, ordered, packedArgs, packedParamIndex, error);
}

bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::vector<const Expr *> &packedArgs,
                           size_t &packedParamIndex,
                           std::string &error) {
  packedArgs.clear();
  packedParamIndex = params.size();
  (void)findTrailingArgsPackParameter(params, packedParamIndex);
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      const std::string &name = *argNames[i];
      const size_t index = resolveNamedParamIndex(params, name);
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (index == packedParamIndex) {
        error = "named arguments cannot bind variadic parameter: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &args[i];
      continue;
    }
    if (args[i].isSpread) {
      if (packedParamIndex >= params.size()) {
        error = "spread argument requires variadic parameter";
        return false;
      }
      packedArgs.push_back(&args[i]);
      continue;
    }
    while (positionalIndex < params.size() && positionalIndex != packedParamIndex &&
           ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (packedParamIndex < params.size() && positionalIndex >= packedParamIndex) {
      packedArgs.push_back(&args[i]);
      continue;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    ordered[positionalIndex] = &args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < params.size(); ++i) {
    if (i == packedParamIndex) {
      continue;
    }
    if (ordered[i] != nullptr) {
      continue;
    }
    if (params[i].defaultExpr) {
      ordered[i] = params[i].defaultExpr;
      continue;
    }
    error = "argument count mismatch";
    return false;
  }
  return true;
}

bool validateNamedArgumentsAgainstParams(const std::vector<ParameterInfo> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error) {
  if (argNames.empty()) {
    return true;
  }
  size_t packedParamIndex = params.size();
  (void)findTrailingArgsPackParameter(params, packedParamIndex);
  std::vector<bool> bound(params.size(), false);
  size_t positionalIndex = 0;
  for (const auto &argName : argNames) {
    if (argName.has_value()) {
      const std::string &name = *argName;
      const size_t index = resolveNamedParamIndex(params, name);
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (index == packedParamIndex) {
        error = "named arguments cannot bind variadic parameter: " + name;
        return false;
      }
      if (bound[index]) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      bound[index] = true;
      continue;
    }
    while (positionalIndex < params.size() && positionalIndex != packedParamIndex && bound[positionalIndex]) {
      ++positionalIndex;
    }
    if (packedParamIndex < params.size() && positionalIndex >= packedParamIndex) {
      continue;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    bound[positionalIndex] = true;
    ++positionalIndex;
  }
  return true;
}

} // namespace primec::semantics
