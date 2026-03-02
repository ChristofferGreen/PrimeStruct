#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "IrLowererFlowHelpers.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using ResolveExprPathFn = std::function<std::string(const Expr &)>;
using DefinitionExistsFn = std::function<bool(const std::string &)>;
using OnErrorByDefinition = std::unordered_map<std::string, std::optional<OnErrorHandler>>;

bool parseTransformArgumentExpr(const std::string &text,
                                const std::string &namespacePrefix,
                                Expr &out,
                                std::string &error);

bool parseOnErrorTransform(const std::vector<Transform> &transforms,
                           const std::string &namespacePrefix,
                           const std::string &context,
                           const ResolveExprPathFn &resolveExprPath,
                           const DefinitionExistsFn &definitionExists,
                           std::optional<OnErrorHandler> &out,
                           std::string &error);

bool buildOnErrorByDefinition(const Program &program,
                              const ResolveExprPathFn &resolveExprPath,
                              const DefinitionExistsFn &definitionExists,
                              OnErrorByDefinition &out,
                              std::string &error);

} // namespace primec::ir_lowerer
