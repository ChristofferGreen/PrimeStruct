#include "SemanticsValidator.h"
#include "primec/StdlibSurfaceRegistry.h"

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
  auto failPostAccessPrecheckDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto isTypeQualifiedStaticHelperCall = [&]() -> bool {
    if (!usedMethodTarget || expr.args.empty()) {
      return false;
    }
    const Expr &receiver = expr.args.front();
    if (receiver.kind != Expr::Kind::Name) {
      return false;
    }
    if (findParamBinding(params, receiver.name) != nullptr ||
        locals.find(receiver.name) != locals.end()) {
      return false;
    }
    if (expr.name.empty()) {
      return false;
    }
    const std::string resolvedReceiverType =
        resolveTypePath(receiver.name, receiver.namespacePrefix);
    if (resolvedReceiverType.empty()) {
      return false;
    }
    return resolved.rfind(resolvedReceiverType + "/" + expr.name, 0) == 0;
  };

  if (usedMethodTarget && !resolvedMethod) {
    auto defIt = defMap_.find(resolved);
    if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second) &&
        !isTypeQualifiedStaticHelperCall()) {
      return failPostAccessPrecheckDiagnostic("static helper does not accept method-call syntax: " +
                                              resolved);
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
      return failPostAccessPrecheckDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failPostAccessPrecheckDiagnostic("gpu builtins do not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failPostAccessPrecheckDiagnostic("gpu builtins do not accept block arguments");
    }
    if (!expr.args.empty()) {
      return failPostAccessPrecheckDiagnostic("gpu builtins do not accept arguments");
    }
    if (!currentValidationState_.context.definitionIsCompute) {
      return failPostAccessPrecheckDiagnostic("gpu builtins require a compute definition");
    }
    return true;
  }

  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) &&
      defMap_.find(resolved) == defMap_.end()) {
    return failPostAccessPrecheckDiagnostic(pathSpaceBuiltin.name + " is statement-only");
  }

  if (!resolvedMethod && resolved == "/file_error/why" &&
      defMap_.find(resolved) == defMap_.end()) {
    resolvedMethod = true;
  }

  const std::string canonicalGfxBufferHelper =
      stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::GfxBufferHelpers, resolved);
  const bool isStdlibBufferLoadWrapperCall =
      canonicalGfxBufferHelper == "/std/gfx/Buffer/load";
  const bool isStdlibBufferStoreWrapperCall =
      canonicalGfxBufferHelper == "/std/gfx/Buffer/store";
  if (!currentValidationState_.context.definitionIsCompute &&
      (defMap_.find(resolved) != defMap_.end() ||
       hasDeclaredDefinitionPath(resolved) ||
       hasImportedDefinitionPath(resolved)) &&
      (isStdlibBufferLoadWrapperCall || isStdlibBufferStoreWrapperCall)) {
    return failPostAccessPrecheckDiagnostic(
        isStdlibBufferLoadWrapperCall ? "buffer_load requires a compute definition"
                                      : "buffer_store requires a compute definition");
  }

  return true;
}

} // namespace primec::semantics
