#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/IR.h"
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
struct MapLookupLoopLocals {
  int32_t countLocal = -1;
  int32_t indexLocal = -1;
};
enum class MapLookupStringKeyResult {
  NotHandled,
  Resolved,
  Error,
};
enum class MapLookupKeyLocalEmitResult {
  NotHandled,
  Emitted,
  Error,
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
IrOpcode mapKeyCompareOpcode(LocalInfo::ValueKind mapKeyKind);
MapLookupStringKeyResult tryResolveMapLookupStringKey(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    int32_t &stringIndexOut,
    std::string &error);
MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
bool emitMapLookupNonStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
MapLookupLoopLocals emitMapLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool validateMapLookupKeyKind(LocalInfo::ValueKind mapKeyKind,
                              LocalInfo::ValueKind lookupKeyKind,
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
