#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprPostAccessPrechecks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &resolved,
    bool &resolvedMethod,
    bool usedMethodTarget,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex,
    bool &handledOut) {
  handledOut = false;

  if (usedMethodTarget && !resolvedMethod) {
    auto defIt = defMap_.find(resolved);
    if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second)) {
      error_ = "static helper does not accept method-call syntax: " + resolved;
      return false;
    }
  }

  if (!validateExprBodyArguments(params, locals, expr, resolved, resolvedMethod,
                                 dispatchResolverAdapters, enclosingStatements,
                                 statementIndex)) {
    return false;
  }

  std::string gpuBuiltin;
  if (getBuiltinGpuName(expr, gpuBuiltin)) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = "gpu builtins do not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = "gpu builtins do not accept block arguments";
      return false;
    }
    if (!expr.args.empty()) {
      error_ = "gpu builtins do not accept arguments";
      return false;
    }
    if (!currentValidationContext_.definitionIsCompute) {
      error_ = "gpu builtins require a compute definition";
      return false;
    }
    return true;
  }

  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) &&
      defMap_.find(resolved) == defMap_.end()) {
    error_ = pathSpaceBuiltin.name + " is statement-only";
    return false;
  }

  if (!resolvedMethod && resolved == "/file_error/why" &&
      defMap_.find(resolved) == defMap_.end()) {
    resolvedMethod = true;
  }

  const bool isStdlibBufferLoadWrapperCall =
      resolved.rfind("/std/gfx/Buffer/load", 0) == 0 ||
      resolved.rfind("/std/gfx/experimental/Buffer/load", 0) == 0;
  const bool isStdlibBufferStoreWrapperCall =
      resolved.rfind("/std/gfx/Buffer/store", 0) == 0 ||
      resolved.rfind("/std/gfx/experimental/Buffer/store", 0) == 0;
  if (!currentValidationContext_.definitionIsCompute &&
      (defMap_.find(resolved) != defMap_.end() ||
       hasDeclaredDefinitionPath(resolved) ||
       hasImportedDefinitionPath(resolved)) &&
      (isStdlibBufferLoadWrapperCall || isStdlibBufferStoreWrapperCall)) {
    error_ = isStdlibBufferLoadWrapperCall
                 ? "buffer_load requires a compute definition"
                 : "buffer_store requires a compute definition";
    return false;
  }

  return true;
}

} // namespace primec::semantics
