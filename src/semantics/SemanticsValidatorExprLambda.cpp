#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include <optional>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateLambdaExpr(const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                            const Expr &expr,
                                            const std::vector<Expr> *enclosingStatements,
                                            size_t statementIndex) {
  auto addCapturedBinding = [&](std::unordered_map<std::string, BindingInfo> &lambdaLocals,
                                const std::string &name) -> bool {
    if (lambdaLocals.count(name) > 0) {
      error_ = "duplicate lambda capture: " + name;
      return false;
    }
    if (currentValidationContext_.movedBindings.count(name) > 0) {
      error_ = "use-after-move: " + name;
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      lambdaLocals.emplace(name, *paramBinding);
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      lambdaLocals.emplace(name, it->second);
      return true;
    }
    error_ = "unknown capture: " + name;
    return false;
  };

  if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
    error_ = "lambda requires a body";
    return false;
  }

  std::unordered_map<std::string, BindingInfo> lambdaLocals;
  if (!expr.lambdaCaptures.empty()) {
    bool captureAll = false;
    std::string captureAllToken;
    std::vector<std::string> captureNames;
    std::unordered_set<std::string> explicitNames;
    captureNames.reserve(expr.lambdaCaptures.size());
    explicitNames.reserve(expr.lambdaCaptures.size());
    for (const auto &capture : expr.lambdaCaptures) {
      std::vector<std::string> tokens = runSemanticsValidatorExprCaptureSplitStep(capture);
      if (tokens.empty()) {
        error_ = "invalid lambda capture";
        return false;
      }
      if (tokens.size() == 1) {
        const std::string &token = tokens[0];
        if (token == "=" || token == "&") {
          if (!captureAllToken.empty()) {
            error_ = "invalid lambda capture";
            return false;
          }
          captureAllToken = token;
          captureAll = true;
          continue;
        }
        if (!explicitNames.insert(token).second) {
          error_ = "duplicate lambda capture: " + token;
          return false;
        }
        captureNames.push_back(token);
        continue;
      }
      if (tokens.size() == 2) {
        const std::string &qualifier = tokens[0];
        const std::string &name = tokens[1];
        if (qualifier != "value" && qualifier != "ref") {
          error_ = "invalid lambda capture";
          return false;
        }
        if (name == "=" || name == "&") {
          error_ = "invalid lambda capture";
          return false;
        }
        if (!explicitNames.insert(name).second) {
          error_ = "duplicate lambda capture: " + name;
          return false;
        }
        captureNames.push_back(name);
        continue;
      }
      error_ = "invalid lambda capture";
      return false;
    }
    if (captureAll) {
      for (const auto &param : params) {
        if (!addCapturedBinding(lambdaLocals, param.name)) {
          return false;
        }
      }
      for (const auto &entry : locals) {
        if (!addCapturedBinding(lambdaLocals, entry.first)) {
          return false;
        }
      }
      for (const auto &name : captureNames) {
        if (findParamBinding(params, name) || locals.count(name) > 0) {
          continue;
        }
        error_ = "unknown capture: " + name;
        return false;
      }
    } else {
      for (const auto &name : captureNames) {
        if (!addCapturedBinding(lambdaLocals, name)) {
          return false;
        }
      }
    }
  }

  MovedScope movedScope(*this, {});
  BorrowEndScope borrowScope(*this, {});
  std::unordered_set<std::string> seen;
  std::vector<ParameterInfo> lambdaParams;
  auto defaultResolvesToDefinition = [&](const Expr &candidate) -> bool {
    return candidate.kind == Expr::Kind::Call && defMap_.find(resolveCalleePath(candidate)) != defMap_.end();
  };
  lambdaParams.reserve(expr.args.size());
  for (const auto &param : expr.args) {
    if (!param.isBinding) {
      error_ = "lambda parameters must use binding syntax";
      return false;
    }
    if (param.hasBodyArguments || !param.bodyArguments.empty()) {
      error_ = "lambda parameter does not accept block arguments: " + param.name;
      return false;
    }
    if (!seen.insert(param.name).second) {
      error_ = "duplicate parameter: " + param.name;
      return false;
    }
    if (lambdaLocals.count(param.name) > 0) {
      error_ = "duplicate binding name: " + param.name;
      return false;
    }
    BindingInfo binding;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(param, expr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
      return false;
    }
    if (param.args.size() > 1) {
      error_ = "lambda parameter defaults accept at most one argument: " + param.name;
      return false;
    }
    if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition)) {
      if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
        error_ = "lambda parameter default does not accept named arguments: " + param.name;
      } else {
        error_ = "lambda parameter default must be a literal or pure expression: " + param.name;
      }
      return false;
    }
    if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
      (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
    }
    ParameterInfo info;
    info.name = param.name;
    info.binding = std::move(binding);
    if (param.args.size() == 1) {
      info.defaultExpr = &param.args.front();
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.binding.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType,
                                  info.binding.typeName,
                                  info.binding.typeTemplateArg,
                                  hasTemplate,
                                  expr.namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    lambdaLocals.emplace(info.name, info.binding);
    lambdaParams.push_back(std::move(info));
  }

  std::vector<Expr> lambdaLivenessStatements = expr.bodyArguments;
  if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
    for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
      lambdaLivenessStatements.push_back((*enclosingStatements)[idx]);
    }
  }
  bool sawReturn = false;
  for (size_t stmtIndex = 0; stmtIndex < expr.bodyArguments.size(); ++stmtIndex) {
    const Expr &stmt = expr.bodyArguments[stmtIndex];
    if (!validateStatement(lambdaParams,
                           lambdaLocals,
                           stmt,
                           ReturnKind::Unknown,
                           true,
                           true,
                           &sawReturn,
                           expr.namespacePrefix,
                           &expr.bodyArguments,
                           stmtIndex)) {
      return false;
    }
    expireReferenceBorrowsForRemainder(lambdaParams, lambdaLocals, lambdaLivenessStatements, stmtIndex + 1);
  }
  return true;
}

} // namespace primec::semantics
