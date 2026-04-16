#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include <algorithm>
#include <optional>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateLambdaExpr(const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                            const Expr &expr,
                                            const std::vector<Expr> *enclosingStatements,
                                            size_t statementIndex) {
  auto failLambdaDiagnostic = [&](const Expr &diagnosticExpr,
                                  std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  auto addCapturedBinding = [&](std::unordered_map<std::string, BindingInfo> &lambdaLocals,
                                const std::string &name) -> bool {
    if (lambdaLocals.count(name) > 0) {
      return failLambdaDiagnostic(expr, "duplicate lambda capture: " + name);
    }
    if (currentValidationState_.movedBindings.count(name) > 0) {
      return failLambdaDiagnostic(expr, "use-after-move: " + name);
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
    return failLambdaDiagnostic(expr, "unknown capture: " + name);
  };

  if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
    return failLambdaDiagnostic(expr, "lambda requires a body");
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
        return failLambdaDiagnostic(expr, "invalid lambda capture");
      }
      if (tokens.size() == 1) {
        const std::string &token = tokens[0];
        if (token == "=" || token == "&") {
          if (!captureAllToken.empty()) {
            return failLambdaDiagnostic(expr, "invalid lambda capture");
          }
          captureAllToken = token;
          captureAll = true;
          continue;
        }
        if (!explicitNames.insert(token).second) {
          return failLambdaDiagnostic(expr,
                                      "duplicate lambda capture: " + token);
        }
        captureNames.push_back(token);
        continue;
      }
      if (tokens.size() == 2) {
        const std::string &qualifier = tokens[0];
        const std::string &name = tokens[1];
        if (qualifier != "value" && qualifier != "ref") {
          return failLambdaDiagnostic(expr, "invalid lambda capture");
        }
        if (name == "=" || name == "&") {
          return failLambdaDiagnostic(expr, "invalid lambda capture");
        }
        if (!explicitNames.insert(name).second) {
          return failLambdaDiagnostic(expr,
                                      "duplicate lambda capture: " + name);
        }
        captureNames.push_back(name);
        continue;
      }
      return failLambdaDiagnostic(expr, "invalid lambda capture");
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
        return failLambdaDiagnostic(expr, "unknown capture: " + name);
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
      return failLambdaDiagnostic(param,
                                  "lambda parameters must use binding syntax");
    }
    if (param.hasBodyArguments || !param.bodyArguments.empty()) {
      return failLambdaDiagnostic(
          param, "lambda parameter does not accept block arguments: " +
                     param.name);
    }
    if (!seen.insert(param.name).second) {
      return failLambdaDiagnostic(param, "duplicate parameter: " + param.name);
    }
    if (lambdaLocals.count(param.name) > 0) {
      return failLambdaDiagnostic(param,
                                  "duplicate binding name: " + param.name);
    }
    BindingInfo binding;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(param, expr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
      return false;
    }
    if (param.args.size() > 1) {
      return failLambdaDiagnostic(
          param, "lambda parameter defaults accept at most one argument: " +
                     param.name);
    }
    if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition)) {
      if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
        return failLambdaDiagnostic(
            param, "lambda parameter default does not accept named arguments: " +
                       param.name);
      } else {
        return failLambdaDiagnostic(
            param,
            "lambda parameter default must be a literal or pure expression: " +
                param.name);
      }
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
        return failLambdaDiagnostic(param,
                                    "restrict type does not match binding type");
      }
    }
    insertLocalBinding(lambdaLocals, info.name, info.binding);
    lambdaParams.push_back(std::move(info));
  }

  const std::vector<Expr> *postMergeStatements = enclosingStatements;
  const size_t postMergeStartIndex = enclosingStatements == nullptr
                                         ? 0
                                         : std::min(statementIndex + 1, enclosingStatements->size());
  std::vector<BorrowLivenessRange> livenessRanges;
  livenessRanges.reserve(2);
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
    livenessRanges.clear();
    livenessRanges.push_back(BorrowLivenessRange{&expr.bodyArguments, stmtIndex + 1});
    if (postMergeStatements != nullptr && postMergeStartIndex < postMergeStatements->size()) {
      livenessRanges.push_back(BorrowLivenessRange{postMergeStatements, postMergeStartIndex});
    }
    expireReferenceBorrowsForRanges(lambdaParams, lambdaLocals, livenessRanges);
  }
  return true;
}

} // namespace primec::semantics
