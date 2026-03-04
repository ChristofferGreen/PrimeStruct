#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using ResolveExprPathFn = std::function<std::string(const Expr &)>;
using ResolveDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using IsTailCallCandidateFn = std::function<bool(const Expr &)>;
using DefinitionExistsFn = std::function<bool(const std::string &)>;

struct CallResolutionAdapters {
  ResolveExprPathFn resolveExprPath;
  IsTailCallCandidateFn isTailCallCandidate;
  DefinitionExistsFn definitionExists;
};
struct EntryCallResolutionSetup {
  CallResolutionAdapters adapters;
  bool hasTailExecution = false;
};

const Definition *resolveDefinitionCall(const Expr &callExpr,
                                        const std::unordered_map<std::string, const Definition *> &defMap,
                                        const ResolveExprPathFn &resolveExprPath);
ResolveDefinitionCallFn makeResolveDefinitionCall(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath);

CallResolutionAdapters makeCallResolutionAdapters(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);
EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);
ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);
IsTailCallCandidateFn makeIsTailCallCandidate(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath);
DefinitionExistsFn makeDefinitionExistsByPath(
    const std::unordered_map<std::string, const Definition *> &defMap);

std::string resolveCallPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);

bool isTailCallCandidate(const Expr &expr,
                         const std::unordered_map<std::string, const Definition *> &defMap,
                         const ResolveExprPathFn &resolveExprPath);

bool hasTailExecutionCandidate(const std::vector<Expr> &statements,
                               bool definitionReturnsVoid,
                               const IsTailCallCandidateFn &isTailCallCandidate);
enum class CountMethodFallbackResult {
  NotHandled,
  NoCallee,
  Emitted,
  Error,
};
enum class ResolvedInlineCallResult {
  NoCallee,
  Emitted,
  Error,
};
enum class InlineCallDispatchResult {
  NotHandled,
  Emitted,
  Error,
};
enum class UnsupportedNativeCallResult {
  NotHandled,
  Error,
};
struct MapAccessTargetInfo {
  bool isMapTarget = false;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
};
ResolvedInlineCallResult emitResolvedInlineDefinitionCall(
    const Expr &callExpr,
    const Definition *callee,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error);
InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error);
bool getUnsupportedVectorHelperName(const Expr &expr, std::string &helperName);
UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error);
MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn);
bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error);
CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error);

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error);
bool definitionHasTransform(const Definition &def, const std::string &transformName);
bool isStructTransformName(const std::string &name);
bool isStructDefinition(const Definition &def);
bool isStructHelperDefinition(const Definition &def,
                              const std::unordered_set<std::string> &structNames,
                              std::string &parentStructPathOut);
Expr makeStructHelperThisParam(const std::string &structPath, bool isMutable);
bool isStaticFieldBinding(const Expr &expr);
bool collectInstanceStructFieldParams(const Definition &structDef,
                                      std::vector<Expr> &paramsOut,
                                      std::string &error);
bool buildInlineCallParameterList(const Definition &callee,
                                  const std::unordered_set<std::string> &structNames,
                                  std::vector<Expr> &paramsOut,
                                  std::string &error);
bool buildInlineCallOrderedArguments(const Expr &callExpr,
                                     const Definition &callee,
                                     const std::unordered_set<std::string> &structNames,
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::string &error);
const Definition *resolveDefinitionByPath(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);
std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);

} // namespace primec::ir_lowerer
