#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstddef>
#include <unordered_set>
#include <vector>
#include <optional>

#include "primec/Ir.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

// BEGIN IrLowererSharedTypes.h



struct LocalInfo {
  int32_t index = 0;
  bool isMutable = false;
  enum class Kind { Value, Pointer, Reference, Array, Vector, Map, Buffer } kind = Kind::Value;
  enum class ValueKind { Unknown, Int32, Int64, UInt64, Float32, Float64, Bool, String } valueKind = ValueKind::Unknown;
  std::string structTypeName;
  int32_t structFieldCount = 0;
  ValueKind mapKeyKind = ValueKind::Unknown;
  ValueKind mapValueKind = ValueKind::Unknown;
  Kind argsPackElementKind = Kind::Value;
  int32_t structSlotCount = 0;
  bool isFileHandle = false;
  bool isFileError = false;
  std::string errorTypeName;
  std::string errorHelperNamespacePath;
  bool isResult = false;
  bool resultHasValue = false;
  ValueKind resultValueKind = ValueKind::Unknown;
  std::string resultErrorType;
  enum class StringSource { None, TableIndex, ArgvIndex, RuntimeIndex } stringSource = StringSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  bool isArgsPack = false;
  int32_t argsPackElementCount = -1;
  bool referenceToArray = false;
  bool pointerToArray = false;
  bool referenceToVector = false;
  bool pointerToVector = false;
  bool referenceToBuffer = false;
  bool pointerToBuffer = false;
  bool referenceToMap = false;
  bool pointerToMap = false;
  bool isUninitializedStorage = false;
  bool targetsUninitializedStorage = false;
  bool isSoaVector = false;
};

using LocalMap = std::unordered_map<std::string, LocalInfo>;

struct ReturnInfo {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind kind = LocalInfo::ValueKind::Unknown;
  bool isResult = false;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
};

// END IrLowererSharedTypes.h

// BEGIN IrLowererCallHelpers.h




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
enum class NativeCallTailDispatchResult {
  NotHandled,
  Emitted,
  Error,
};
enum class BufferBuiltinDispatchResult {
  NotHandled,
  Emitted,
  Error,
};
enum class MapAccessLookupEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class StringTableAccessEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class NonLiteralStringAccessTargetResult {
  Continue,
  Stop,
  Error,
};
struct MapAccessTargetInfo {
  bool isMapTarget = false;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
};
struct ArrayVectorAccessTargetInfo {
  bool isArrayOrVectorTarget = false;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  bool isVectorTarget = false;
  bool isSoaVector = false;
  bool isArgsPackTarget = false;
  LocalInfo::Kind argsPackElementKind = LocalInfo::Kind::Value;
  int32_t elemSlotCount = 0;
  std::string structTypeName;
};
using ResolveCallMapAccessTargetInfoFn = std::function<bool(const Expr &, MapAccessTargetInfo &)>;
using ResolveCallArrayVectorAccessTargetInfoFn =
    std::function<bool(const Expr &, ArrayVectorAccessTargetInfo &)>;
struct MapLookupLoopLocals {
  int32_t countLocal = -1;
  int32_t indexLocal = -1;
};
struct MapLookupLoopConditionAnchors {
  size_t loopStart = 0;
  size_t jumpLoopEnd = 0;
};
struct MapLookupLoopMatchAnchors {
  size_t jumpNotMatch = 0;
  size_t jumpFound = 0;
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
InlineCallDispatchResult tryEmitInlineCallDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    std::string &error);
bool getUnsupportedVectorHelperName(const Expr &expr, std::string &helperName);
UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
BufferBuiltinDispatchResult tryEmitBufferBuiltinDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target,
                                               const LocalMap &localsIn,
                                               const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo);
MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn);
bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn);
bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo, std::string &error);
MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
StringTableAccessEmitResult tryEmitStringTableAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error);
bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
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
bool emitMapLookupKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &keyLocalOut,
    std::string &error);
bool emitMapLookupTargetPointerLocal(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &ptrLocalOut);
MapLookupLoopLocals emitMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
bool emitMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
void emitStringAccessLoad(
    const std::string &accessName,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    size_t stringLength,
    int32_t stringIndex,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitArrayVectorAccessLoad(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    bool isVectorTarget,
    uint64_t arrayHeaderSlots,
    int32_t elementSlotCount,
    bool loadElementValue,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
MapLookupLoopLocals emitMapLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
MapLookupLoopConditionAnchors emitMapLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
MapLookupLoopMatchAnchors emitMapLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void emitMapLookupLoopAdvanceAndPatch(
    size_t jumpNotMatch,
    size_t jumpLoopEnd,
    size_t jumpFound,
    size_t loopStart,
    int32_t indexLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupAtKeyNotFoundGuard(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupValueLoad(
    int32_t ptrLocal,
    int32_t indexLocal,
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
    std::string &error,
    std::function<bool(const Expr &)> isCollectionAccessReceiverExpr = {});

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
                                     const LocalMap &callerLocals,
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::vector<const Expr *> &packedArgsOut,
                                     size_t &packedParamIndexOut,
                                     std::string &error);
const Definition *resolveDefinitionByPath(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);
std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);

// END IrLowererCallHelpers.h

// BEGIN IrLowererBindingTransformHelpers.h




bool isBindingMutable(const Expr &expr);
bool isBindingQualifierName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool extractFirstBindingTypeTransform(const Expr &expr,
                                      std::string &typeNameOut,
                                      std::vector<std::string> &templateArgsOut);
bool extractUninitializedTemplateArg(const Expr &expr, std::string &typeTextOut);
bool isArgsPackBinding(const Expr &expr);
bool isEntryArgsParam(const Expr &param);

// END IrLowererBindingTransformHelpers.h

// BEGIN IrLowererBindingTypeHelpers.h




using BindingKindFromTransformsFn = std::function<LocalInfo::Kind(const Expr &)>;
using IsBindingTypeFn = std::function<bool(const Expr &)>;
using BindingValueKindFromTransformsFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using SetReferenceArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;

struct BindingTypeAdapters {
  BindingKindFromTransformsFn bindingKind;
  IsBindingTypeFn isStringBinding;
  IsBindingTypeFn isFileErrorBinding;
  BindingValueKindFromTransformsFn bindingValueKind;
  SetReferenceArrayInfoFn setReferenceArrayInfo;
};

BindingTypeAdapters makeBindingTypeAdapters();
BindingKindFromTransformsFn makeBindingKindFromTransforms();
IsBindingTypeFn makeIsStringBindingType();
IsBindingTypeFn makeIsFileErrorBindingType();
BindingValueKindFromTransformsFn makeBindingValueKindFromTransforms();
SetReferenceArrayInfoFn makeSetReferenceArrayInfoFromTransforms();

std::string normalizeCollectionBindingTypeName(const std::string &name);
LocalInfo::Kind bindingKindFromTransforms(const Expr &expr);
bool isStringTypeName(const std::string &name);
bool isStringBindingType(const Expr &expr);
bool isFileErrorBindingType(const Expr &expr);
LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind);
void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info);

// END IrLowererBindingTypeHelpers.h

// BEGIN IrLowererFileWriteHelpers.h




using ResolveStringTableTargetForWriteFn = std::function<bool(const Expr &, int32_t &, size_t &)>;
using InferExprKindForWriteFn = std::function<LocalInfo::ValueKind(const Expr &)>;
using EmitExprForWriteFn = std::function<bool(const Expr &)>;
using EmitInstructionForWriteFn = std::function<void(IrOpcode, uint64_t)>;
using AllocTempLocalForWriteFn = std::function<int32_t()>;
using GetInstructionCountForWriteFn = std::function<size_t()>;
using PatchInstructionImmForWriteFn = std::function<void(size_t, int32_t)>;
using EmitFileWriteStepFn = std::function<bool(const Expr &, int32_t)>;
using ResolveStringTableTargetWithLocalsForWriteFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using InferExprKindWithLocalsForWriteFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using EmitExprWithLocalsForWriteFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsEntryArgsNameWithLocalsForWriteFn = std::function<bool(const Expr &, const LocalMap &)>;
using ShouldBypassBuiltinFileMethodWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;

enum class FileHandleMethodCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class FileConstructorCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};

bool resolveFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool resolveDynamicFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool emitFileOpenCall(const std::string &mode,
                      int32_t stringIndex,
                      const EmitInstructionForWriteFn &emitInstruction,
                      std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const IsEntryArgsNameWithLocalsForWriteFn &isEntryArgsName,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error);
bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut);
bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const EmitExprForWriteFn &emitExpr,
                       const EmitInstructionForWriteFn &emitInstruction,
                       std::string &error);
bool emitFileWriteCall(const Expr &expr,
                       int32_t handleIndex,
                       const EmitFileWriteStepFn &emitWriteStep,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction,
                       const GetInstructionCountForWriteFn &getInstructionCount,
                       const PatchInstructionImmForWriteFn &patchInstructionImm);
bool emitFileWriteByteCall(const Expr &expr,
                           int32_t handleIndex,
                           const EmitExprForWriteFn &emitExpr,
                           const EmitInstructionForWriteFn &emitInstruction,
                           std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const AllocTempLocalForWriteFn &allocTempLocal,
                          const EmitInstructionForWriteFn &emitInstruction,
                          const GetInstructionCountForWriteFn &getInstructionCount,
                          const PatchInstructionImmForWriteFn &patchInstructionImm,
                          std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const EmitInstructionForWriteFn &emitInstruction,
                          std::string &error);
bool emitFileWriteBytesCall(const Expr &expr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm,
                            std::string &error);
bool emitFileWriteBytesLoop(const Expr &bytesExpr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm);
FileHandleMethodCallEmitResult tryEmitFileHandleMethodCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ShouldBypassBuiltinFileMethodWithLocalsFn &shouldBypassBuiltin,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const AllocTempLocalForWriteFn &allocTempLocal,
    const EmitInstructionForWriteFn &emitInstruction,
    const GetInstructionCountForWriteFn &getInstructionCount,
    const PatchInstructionImmForWriteFn &patchInstructionImm,
    std::string &error);
void emitFileFlushCall(int32_t handleIndex, const EmitInstructionForWriteFn &emitInstruction);
void emitFileCloseCall(int32_t handleIndex,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction);

// END IrLowererFileWriteHelpers.h

// BEGIN IrLowererFlowHelpers.h




struct OnErrorHandler {
  std::string errorType;
  std::string handlerPath;
  std::vector<Expr> boundArgs = {};
};

struct ResultReturnInfo {
  bool isResult = false;
  bool hasValue = false;
};

class OnErrorScope {
 public:
  OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next);
  ~OnErrorScope();

 private:
  std::optional<OnErrorHandler> &target;
  std::optional<OnErrorHandler> previous;
};

class ResultReturnScope {
 public:
  ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next);
  ~ResultReturnScope();

 private:
  std::optional<ResultReturnInfo> &target;
  std::optional<ResultReturnInfo> previous;
};

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex);
void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope);
void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack);
bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount);
bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const std::function<int32_t()> &allocTempLocal);
void emitDisarmTemporaryStructAfterCopy(const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
                                        int32_t srcPtrLocal,
                                        const std::string &structTypeName);
bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error);
bool emitFloatLiteral(std::vector<IrInstruction> &instructions, const Expr &expr, std::string &error);
bool emitReturnForDefinition(std::vector<IrInstruction> &instructions,
                             const std::string &defPath,
                             const ReturnInfo &returnInfo,
                             std::string &error);
const char *resolveGpuBuiltinLocalName(const std::string &gpuBuiltin);
bool emitGpuBuiltinLoad(
    const std::string &gpuBuiltin,
    const std::function<std::optional<int32_t>(const char *)> &resolveLocalIndex,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
enum class UnaryPassthroughCallResult {
  NotMatched,
  Emitted,
  Error,
};
enum class BufferBuiltinCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class VectorStatementHelperEmitResult {
  NotMatched,
  Emitted,
  Error,
};
struct CountedLoopControl {
  int32_t counterLocal = -1;
  LocalInfo::ValueKind countKind = LocalInfo::ValueKind::Unknown;
  size_t checkIndex = 0;
  size_t jumpEndIndex = 0;
};
UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(const Expr &expr,
                                                       const char *callName,
                                                       const std::function<bool(const Expr &)> &emitExpr,
                                                       std::string &error);
bool resolveCountedLoopKind(LocalInfo::ValueKind inferredKind,
                            bool allowBool,
                            const char *errorMessage,
                            LocalInfo::ValueKind &countKindOut,
                            std::string &error);
bool emitCountedLoopPrologue(
    LocalInfo::ValueKind countKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, int32_t)> &patchInstructionImm,
    const std::function<void()> &emitLoopCountNegative,
    CountedLoopControl &out,
    std::string &error);
void emitCountedLoopIterationStep(
    const CountedLoopControl &control,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void patchCountedLoopEnd(
    const CountedLoopControl &control,
    const std::function<size_t()> &instructionCount,
    const std::function<void(size_t, int32_t)> &patchInstructionImm);
bool emitBodyStatements(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement);
bool emitBodyStatementsWithFileScope(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement,
    const std::function<bool()> &emitAfterBody,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope);
bool declareForConditionBinding(
    const Expr &binding,
    LocalMap &locals,
    int32_t &nextLocal,
    const std::function<bool(const Expr &)> &isBindingMutable,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const Expr &)> &hasExplicitBindingTypeTransform,
    const std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)> &bindingValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructArrayInfo,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructValueInfo,
    std::string &error);
bool emitForConditionBindingInit(
    const Expr &binding,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error);
struct BufferInitInfo {
  int32_t count = 0;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  IrOpcode zeroOpcode = IrOpcode::PushI32;
};
bool resolveBufferInitInfo(const Expr &expr,
                           const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
                           BufferInitInfo &out,
                           std::string &error);
struct BufferLoadInfo {
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
};
bool resolveBufferLoadInfo(
    const Expr &expr,
    const std::function<std::optional<LocalInfo::ValueKind>(const Expr &)> &resolveBufferElemKind,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &)> &inferExprKind,
    BufferLoadInfo &out,
    std::string &error);
bool emitBufferLoadCall(const Expr &expr,
                        LocalInfo::ValueKind indexKind,
                        const std::function<bool(const Expr &)> &emitExpr,
                        const std::function<int32_t()> &allocTempLocal,
                        const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
BufferBuiltinCallEmitResult tryEmitBufferBuiltinCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);

// END IrLowererFlowHelpers.h

// BEGIN IrLowererHelpers.h




constexpr const char *kGpuGlobalIdXName = "__gpu_global_id_x";
constexpr const char *kGpuGlobalIdYName = "__gpu_global_id_y";
constexpr const char *kGpuGlobalIdZName = "__gpu_global_id_z";
constexpr int32_t kVectorLocalDynamicCapacityLimit = 256;
std::string vectorLocalCapacityLimitExceededMessage();
std::string vectorReserveExceedsLocalCapacityLimitMessage();
std::string vectorLiteralExceedsLocalCapacityLimitMessage();
std::string vectorPushAllocationFailedMessage();
std::string vectorReserveAllocationFailedMessage();

bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool isReturnCall(const Expr &expr);
bool isIfCall(const Expr &expr);
bool isMatchCall(const Expr &expr);
bool isLoopCall(const Expr &expr);
bool isWhileCall(const Expr &expr);
bool isForCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool isBlockCall(const Expr &expr);

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);

bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error);

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

struct PathSpaceBuiltin {
  std::string name;
  size_t argumentCount = 0;
};

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out);

bool getBuiltinOperatorName(const Expr &expr, std::string &out);
bool getBuiltinComparisonName(const Expr &expr, std::string &out);
bool getBuiltinClampName(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinLerpName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinFmaName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinHypotName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinCopysignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAngleName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinTrigName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinTrig2Name(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinArcTrigName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinHyperbolicName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinArcHyperbolicName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinExpName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinLogName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinPowName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathPredicateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinRoundingName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinRootName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinConvertName(const Expr &expr);
bool getBuiltinMemoryName(const Expr &expr, std::string &out);
bool getBuiltinGpuName(const Expr &expr, std::string &out);
bool getBuiltinArrayAccessName(const Expr &expr, std::string &out);
bool getBuiltinPointerName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);

// END IrLowererHelpers.h

// BEGIN IrLowererCountAccessHelpers.h




using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsArrayCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsVectorCapacityCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsStringCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;

struct CountAccessClassifiers {
  IsEntryArgsNameFn isEntryArgsName;
  IsArrayCountCallFn isArrayCountCall;
  IsVectorCapacityCallFn isVectorCapacityCall;
  IsStringCountCallFn isStringCountCall;
};

struct EntryCountAccessSetup {
  bool hasEntryArgs = false;
  std::string entryArgsName;
  CountAccessClassifiers classifiers;
};
enum class StringCountCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class CountAccessCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error);
bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error);
CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName);
IsEntryArgsNameFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName);
IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName);
IsVectorCapacityCallFn makeIsVectorCapacityCall();
IsStringCountCallFn makeIsStringCountCall();
bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn);
bool isStringCountCall(const Expr &expr, const LocalMap &localsIn);
StringCountCallEmitResult tryEmitStringCountCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    std::string &error);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicCollectionCountTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCountTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCapacityTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);

// END IrLowererCountAccessHelpers.h

// BEGIN IrLowererIndexKindHelpers.h



LocalInfo::ValueKind normalizeIndexKind(LocalInfo::ValueKind kind);
bool isSupportedIndexKind(LocalInfo::ValueKind kind);

IrOpcode pushZeroForIndex(LocalInfo::ValueKind kind);
IrOpcode cmpLtForIndex(LocalInfo::ValueKind kind);
IrOpcode cmpGeForIndex(LocalInfo::ValueKind kind);
IrOpcode pushOneForIndex(LocalInfo::ValueKind kind);
IrOpcode addForIndex(LocalInfo::ValueKind kind);
IrOpcode mulForIndex(LocalInfo::ValueKind kind);

// END IrLowererIndexKindHelpers.h

// BEGIN IrLowererOnErrorHelpers.h




using OnErrorByDefinition = std::unordered_map<std::string, std::optional<OnErrorHandler>>;
struct EntryCallOnErrorSetup {
  CallResolutionAdapters callResolutionAdapters;
  bool hasTailExecution = false;
  OnErrorByDefinition onErrorByDefinition;
};
struct EntryCountCallOnErrorSetup {
  EntryCountAccessSetup countAccessSetup;
  EntryCallOnErrorSetup callOnErrorSetup;
};

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
bool buildOnErrorByDefinitionFromCallResolutionAdapters(
    const Program &program,
    const CallResolutionAdapters &callResolutionAdapters,
    OnErrorByDefinition &out,
    std::string &error);
bool buildEntryCallOnErrorSetup(const Program &program,
                                const Definition &entryDef,
                                bool definitionReturnsVoid,
                                const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::unordered_map<std::string, std::string> &importAliases,
                                EntryCallOnErrorSetup &out,
                                std::string &error);
bool buildEntryCountCallOnErrorSetup(const Program &program,
                                     const Definition &entryDef,
                                     bool definitionReturnsVoid,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const std::unordered_map<std::string, std::string> &importAliases,
                                     EntryCountCallOnErrorSetup &out,
                                     std::string &error);

// END IrLowererOnErrorHelpers.h

// BEGIN IrLowererInlineCallContextHelpers.h




struct InlineDefinitionCallContextSetup {
  ReturnInfo returnInfo;
  bool structDefinition = false;
  std::optional<OnErrorHandler> scopedOnError;
  std::optional<ResultReturnInfo> scopedResult;
};

bool prepareInlineDefinitionCallContext(
    const Definition &callee,
    bool requireValue,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Definition &)> &isStructDefinition,
    std::unordered_set<std::string> &inlineStack,
    std::unordered_set<std::string> &loweredCallTargets,
    const OnErrorByDefinition &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error);

// END IrLowererInlineCallContextHelpers.h

// BEGIN IrLowererStructFieldBindingHelpers.h




struct LayoutFieldBinding {
  std::string typeName;
  std::string typeTemplateArg;
};

const Expr *getEnvelopeValueExpr(const Expr &candidate, bool allowAnyName);
bool extractExplicitLayoutFieldBinding(const Expr &expr, LayoutFieldBinding &bindingOut);
bool inferPrimitiveFieldBinding(const Expr &initializer,
                                const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
                                LayoutFieldBinding &bindingOut);
bool resolveLayoutFieldBinding(
    const Definition &def,
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut,
    std::string &errorOut);
bool collectStructLayoutFieldBindings(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut);
bool collectStructLayoutFieldBindingsFromProgramContext(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut);
std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding);

// END IrLowererStructFieldBindingHelpers.h

// BEGIN IrLowererStructTypeHelpers.h




using ResolveStructTypeNameFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using InferStructExprPathFn = std::function<std::string(const Expr &)>;
using InferStructExprWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using IsKnownStructPathFn = std::function<bool(const std::string &)>;
using InferDefinitionStructReturnPathFn = std::function<std::string(const std::string &)>;

struct StructArrayFieldInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructArrayTypeInfo {
  std::string structPath;
  LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
  int32_t fieldCount = 0;
};

using ResolveStructArrayTypeInfoFn = std::function<bool(const std::string &, StructArrayTypeInfo &)>;
using ApplyStructArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructValueInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using CollectStructArrayFieldsFn = std::function<bool(const std::string &, std::vector<StructArrayFieldInfo> &)>;

struct StructTypeResolutionAdapters {
  ResolveStructTypeNameFn resolveStructTypeName;
  ApplyStructValueInfoFn applyStructValueInfo;
};
using SetupCombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
struct SetupTypeAndStructTypeAdapters {
  ValueKindFromTypeNameFn valueKindFromTypeName;
  SetupCombineNumericKindsFn combineNumericKinds;
  StructTypeResolutionAdapters structTypeResolutionAdapters;
};

struct StructArrayInfoAdapters {
  ResolveStructArrayTypeInfoFn resolveStructArrayTypeInfoFromPath;
  ApplyStructArrayInfoFn applyStructArrayInfo;
};

struct StructSlotFieldInfo {
  std::string name;
  int32_t slotOffset = -1;
  int32_t slotCount = 0;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct StructLayoutFieldInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructSlotLayoutInfo {
  std::string structPath;
  int32_t totalSlots = 0;
  std::vector<StructSlotFieldInfo> fields;
};

using ResolveStructFieldSlotFn =
    std::function<bool(const std::string &, const std::string &, StructSlotFieldInfo &)>;
using ResolveStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using ResolveStructSlotFieldsFn =
    std::function<bool(const std::string &, std::vector<StructSlotFieldInfo> &)>;
struct StructSlotResolutionAdapters {
  ResolveStructSlotLayoutFn resolveStructSlotLayout;
  ResolveStructFieldSlotFn resolveStructFieldSlot;
};
struct StructLayoutResolutionAdapters {
  StructArrayInfoAdapters structArrayInfo;
  StructSlotResolutionAdapters structSlotResolution;
};
using CollectStructLayoutFieldsFn =
    std::function<bool(const std::string &, std::vector<StructLayoutFieldInfo> &)>;
using ResolveDefinitionNamespacePrefixByPathFn =
    std::function<bool(const std::string &, std::string &)>;
using StructSlotLayoutCache = std::unordered_map<std::string, StructSlotLayoutInfo>;
using StructLayoutFieldIndex = std::unordered_map<std::string, std::vector<StructLayoutFieldInfo>>;
using AppendStructLayoutFieldFn =
    std::function<void(const std::string &, const StructLayoutFieldInfo &)>;
using EnumerateStructLayoutFieldsFn =
    std::function<void(const AppendStructLayoutFieldFn &)>;

std::string joinTemplateArgsText(const std::vector<std::string> &args);

StructTypeResolutionAdapters makeStructTypeResolutionAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
SetupTypeAndStructTypeAdapters makeSetupTypeAndStructTypeAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
StructArrayInfoAdapters makeStructArrayInfoAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
ResolveStructTypeNameFn makeResolveStructTypePathFromScope(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
bool isWildcardImportPath(const std::string &path, std::string &prefixOut);
void buildDefinitionMapAndStructNames(
    const std::vector<Definition> &definitions,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut);
void appendStructLayoutFieldsFromFieldBindings(
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const AppendStructLayoutFieldFn &appendStructLayoutField);
std::unordered_map<std::string, std::string> buildImportAliasesFromProgram(
    const std::vector<std::string> &imports,
    const std::vector<Definition> &definitions,
    const std::unordered_map<std::string, const Definition *> &defMap);
bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut);
std::string resolveStructTypePathCandidateFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
std::string resolveStructLayoutExprPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);
bool resolveDefinitionNamespacePrefixFromMap(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &defPath,
    std::string &namespacePrefixOut);
StructLayoutFieldIndex buildStructLayoutFieldIndex(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields);
bool collectStructLayoutFieldsFromIndex(const StructLayoutFieldIndex &fieldIndex,
                                        const std::string &structPath,
                                        std::vector<StructLayoutFieldInfo> &out);
bool collectStructArrayFieldsFromLayoutIndex(const StructLayoutFieldIndex &fieldIndex,
                                             const std::string &structPath,
                                             std::vector<StructArrayFieldInfo> &out);
bool resolveStructArrayTypeInfoFromLayoutFieldIndex(const std::string &structPath,
                                                    const StructLayoutFieldIndex &fieldIndex,
                                                    const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                                    StructArrayTypeInfo &out);
bool resolveStructArrayTypeInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructArrayTypeInfo &out);
ResolveStructArrayTypeInfoFn makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
bool resolveStructArrayTypeInfoFromPath(const std::string &structPath,
                                        const CollectStructArrayFieldsFn &collectStructArrayFields,
                                        const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                        StructArrayTypeInfo &out);
bool resolveStructArrayTypeInfoFromBinding(const Expr &expr,
                                           const ResolveStructTypeNameFn &resolveStructTypeName,
                                           const CollectStructArrayFieldsFn &collectStructArrayFields,
                                           const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                           StructArrayTypeInfo &out);
void applyStructArrayInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     const CollectStructArrayFieldsFn &collectStructArrayFields,
                                     const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                     LocalInfo &info);
void applyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    LocalInfo &info);
ApplyStructArrayInfoFn makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
bool resolveStructSlotFieldByName(const std::vector<StructSlotFieldInfo> &fields,
                                  const std::string &fieldName,
                                  StructSlotFieldInfo &out);
bool resolveStructFieldSlotFromLayout(const std::string &structPath,
                                      const std::string &fieldName,
                                      const ResolveStructSlotFieldsFn &resolveStructSlotFields,
                                      StructSlotFieldInfo &out);
bool resolveStructSlotLayoutFromDefinitionFields(
    const std::string &structPath,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error);
bool resolveStructSlotLayoutFromDefinitionFieldIndex(
    const std::string &structPath,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error);
bool resolveStructFieldSlotFromDefinitionFields(
    const std::string &structPath,
    const std::string &fieldName,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error);
bool resolveStructFieldSlotFromDefinitionFieldIndex(
    const std::string &structPath,
    const std::string &fieldName,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error);
ResolveStructSlotLayoutFn makeResolveStructSlotLayoutFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
ResolveStructFieldSlotFn makeResolveStructFieldSlotFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
StructSlotResolutionAdapters makeStructSlotResolutionAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
StructSlotResolutionAdapters makeStructSlotResolutionAdaptersWithOwnedState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error);
StructLayoutResolutionAdapters makeStructLayoutResolutionAdaptersWithOwnedSlotState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error);
ApplyStructValueInfoFn makeApplyStructValueInfoFromBinding(
    const ResolveStructTypeNameFn &resolveStructTypeName);
void applyStructValueInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     LocalInfo &info);
std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath);
std::string inferStructPathFromCallTarget(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const IsKnownStructPathFn &isKnownStructPath,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath);
std::string inferStructPathFromNameExpr(const Expr &expr, const LocalMap &localsIn);
std::string inferStructPathFromFieldAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferStructExprWithLocalsFn &inferStructExprPath,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);

// END IrLowererStructTypeHelpers.h

// BEGIN IrLowererInlineParamHelpers.h




using InferInlineParameterLocalInfoFn = std::function<bool(const Expr &, LocalInfo &, std::string &)>;
using IsInlineParameterStringBindingFn = std::function<bool(const Expr &)>;
using EmitInlineParameterStringValueFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::StringSource &, int32_t &, bool &)>;
using InferInlineParameterStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using InferInlineParameterExprKindFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveInlineParameterStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitInlineParameterExprFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitInlineParameterStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using AllocInlineParameterTempLocalFn = std::function<int32_t()>;
using EmitInlineParameterInstructionFn = std::function<void(IrOpcode, uint64_t)>;
using TrackInlineParameterFileHandleFn = std::function<void(int32_t)>;

bool emitInlineDefinitionCallParameters(
    const std::vector<Expr> &callParams,
    const std::vector<const Expr *> &orderedArgs,
    const std::vector<const Expr *> &packedArgs,
    size_t packedParamIndex,
    const LocalMap &callerLocals,
    int32_t &nextLocal,
    LocalMap &calleeLocals,
    const InferInlineParameterLocalInfoFn &inferCallParameterLocalInfo,
    const IsInlineParameterStringBindingFn &isStringBinding,
    const EmitInlineParameterStringValueFn &emitStringValueForCall,
    const InferInlineParameterStructExprPathFn &inferStructExprPath,
    const InferInlineParameterExprKindFn &inferExprKind,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const EmitInlineParameterExprFn &emitExpr,
    const EmitInlineParameterStructCopySlotsFn &emitStructCopySlots,
    const AllocInlineParameterTempLocalFn &allocTempLocal,
    const EmitInlineParameterInstructionFn &emitInstruction,
    const TrackInlineParameterFileHandleFn &trackFileHandleLocal,
    std::string &error);

// END IrLowererInlineParamHelpers.h

// BEGIN IrLowererInlineStructArgHelpers.h




using ResolveInlineStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using InferInlineStructExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using InferInlineStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using EmitInlineStructExprFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitInlineStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using AllocInlineStructTempLocalFn = std::function<int32_t()>;
using EmitInlineStructInstructionFn = std::function<void(IrOpcode, uint64_t)>;

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const InferInlineStructExprKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const EmitInlineStructExprFn &emitExpr,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const AllocInlineStructTempLocalFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error);

// END IrLowererInlineStructArgHelpers.h

// BEGIN IrLowererLowerEffects.h




bool findEntryDefinition(const Program &program,
                         const std::string &entryPath,
                         const Definition *&entryDefOut,
                         std::string &error);

bool validateNoSoftwareNumericTypes(const Program &program, std::string &error);
bool validateNoRuntimeReflectionQueries(const Program &program, std::string &error);

bool effectBitForName(const std::string &name, uint64_t &outBit);
bool isSupportedEffect(const std::string &name);

std::unordered_set<std::string> resolveActiveEffects(const std::vector<Transform> &transforms,
                                                     bool isEntry,
                                                     const std::vector<std::string> &defaultEffects,
                                                     const std::vector<std::string> &entryDefaultEffects);

bool validateEffectsTransforms(const std::vector<Transform> &transforms,
                               const std::string &context,
                               std::string &error);

bool validateActiveEffects(const std::vector<Transform> &transforms,
                           const std::string &context,
                           bool isEntry,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           std::string &error);

bool validateProgramEffects(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
                            std::string &error);

bool resolveEntryMetadataMasks(const Definition &entryDef,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error);

bool resolveEffectMask(const std::vector<Transform> &transforms,
                       bool isEntry,
                       const std::vector<std::string> &defaultEffects,
                       const std::vector<std::string> &entryDefaultEffects,
                       uint64_t &maskOut,
                       std::string &error);

bool resolveCapabilityMask(const std::vector<Transform> &transforms,
                           const std::unordered_set<std::string> &effects,
                           const std::string &duplicateContext,
                           uint64_t &maskOut,
                           std::string &error);

// END IrLowererLowerEffects.h

// BEGIN IrLowererLowerEntrySetup.h




bool runLowerEntrySetup(const Program &program,
                        const std::string &entryPath,
                        const std::vector<std::string> &defaultEffects,
                        const std::vector<std::string> &entryDefaultEffects,
                        const Definition *&entryDefOut,
                        uint64_t &entryEffectMaskOut,
                        uint64_t &entryCapabilityMaskOut,
                        std::string &error);

// END IrLowererLowerEntrySetup.h

// BEGIN IrLowererStatementCallHelpers.h




enum class BufferStoreStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class DispatchStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class DirectCallStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class AssignOrExprStatementEmitResult {
  Emitted,
  Error,
};
enum class CallableDefinitionOrchestrationResult {
  Emitted,
  Error,
};
enum class EntryCallableExecutionResult {
  Emitted,
  Error,
};
enum class FunctionTableFinalizationResult {
  Emitted,
  Error,
};

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);
DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &)> &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error);
DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error);
inline DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  return tryEmitDirectCallStatement(
      stmt,
      localsIn,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      [](const Expr &, const LocalMap &) { return true; },
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      getReturnInfo,
      emitInlineDefinitionCall,
      instructions,
      error);
}
AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    std::vector<IrInstruction> &instructions);
bool buildCallableDefinitionCallContext(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    int32_t &nextLocal,
    LocalMap &definitionLocals,
    Expr &callExpr,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)> &inferParameterLocalInfo,
    std::string &error);
CallableDefinitionOrchestrationResult lowerCallableDefinitionOrchestration(
    const Program &program,
    const Definition &entryDef,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const std::string &, const ReturnInfo &)> &appendReturnForDefinition,
    IrFunction &function,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    std::string &error);
EntryCallableExecutionResult emitEntryCallableExecutionWithCleanup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    bool &sawReturn,
    std::optional<OnErrorHandler> &currentOnError,
    const std::optional<OnErrorHandler> &entryOnError,
    std::optional<ResultReturnInfo> &currentReturnResult,
    const std::optional<ResultReturnInfo> &entryResult,
    const std::function<bool(const Expr &)> &emitStatement,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope,
    std::vector<IrInstruction> &instructions,
    std::string &error);
FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(
    const Program &program,
    const Definition &entryDef,
    IrFunction &entryFunction,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error);

// END IrLowererStatementCallHelpers.h

// BEGIN IrLowererLowerInlineCallActiveContextStep.h




struct LowerInlineCallActiveContextStepInput {
  const Definition *callee = nullptr;
  bool structDefinition = false;
  bool definitionReturnsVoid = false;
  std::function<void()> activateInlineContext;
  std::function<void()> restoreInlineContext;
  std::function<bool(const Expr &)> emitInlineStatement;
  std::function<bool()> runInlineCleanup;
};

bool runLowerInlineCallActiveContextStep(const LowerInlineCallActiveContextStepInput &input,
                                         std::string &errorOut);

// END IrLowererLowerInlineCallActiveContextStep.h

// BEGIN IrLowererLowerExprEmitSetup.h




using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using LowerExprEmitUnaryPassthroughCallFn =
    std::function<UnaryPassthroughCallResult(const Expr &, const LocalMap &, const EmitExprWithLocalsFn &, std::string &)>;
using LowerExprEmitMovePassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitUploadPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;
using LowerExprEmitReadbackPassthroughCallFn = LowerExprEmitUnaryPassthroughCallFn;

struct LowerExprEmitSetupInput {};

bool runLowerExprEmitSetup(const LowerExprEmitSetupInput &input,
                           LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCallOut,
                           LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCallOut,
                           LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCallOut,
                           std::string &errorOut);
UnaryPassthroughCallResult runLowerExprEmitMovePassthroughStep(
    const Expr &expr,
    const LocalMap &localsIn,
    const LowerExprEmitMovePassthroughCallFn &emitMovePassthroughCall,
    const EmitExprWithLocalsFn &emitExpr,
    std::string &errorOut);
UnaryPassthroughCallResult runLowerExprEmitUploadReadbackPassthroughStep(
    const Expr &expr,
    const LocalMap &localsIn,
    const LowerExprEmitUploadPassthroughCallFn &emitUploadPassthroughCall,
    const LowerExprEmitReadbackPassthroughCallFn &emitReadbackPassthroughCall,
    const EmitExprWithLocalsFn &emitExpr,
    std::string &errorOut);

// END IrLowererLowerExprEmitSetup.h

// BEGIN IrLowererReturnInferenceHelpers.h




enum class MissingReturnBehavior { Error, Void };

struct ReturnInferenceOptions {
  MissingReturnBehavior missingReturnBehavior = MissingReturnBehavior::Error;
  bool includeDefinitionReturnExpr = false;
  bool deferUnknownReturnDependencyErrors = false;
};

struct EntryReturnConfig {
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  bool hasResultInfo = false;
  ResultReturnInfo resultInfo;
};

using InferBindingIntoLocalsFn = std::function<bool(const Expr &, bool, LocalMap &, std::string &)>;
using InferValueKindFromLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ExpandMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using IsBindingMutableForInferenceFn = std::function<bool(const Expr &)>;
using BindingKindForInferenceFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasExplicitBindingTypeTransformForInferenceFn = std::function<bool(const Expr &)>;
using BindingValueKindForInferenceFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using IsFileErrorBindingForInferenceFn = std::function<bool(const Expr &)>;
using ApplyStructInfoForInferenceFn = std::function<void(const Expr &, LocalInfo &)>;
using SetReferenceArrayInfoForInferenceFn = std::function<void(const Expr &, LocalInfo &)>;
using InferStructExprPathFromLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using IsStringBindingForInferenceFn = std::function<bool(const Expr &)>;
using ResolveStructTypeNameForReturnFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ResolveStructArrayInfoForReturnFn = std::function<bool(const std::string &, StructArrayTypeInfo &)>;

bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);
void analyzeDeclaredReturnTransforms(const Definition &def,
                                     const ResolveStructTypeNameForReturnFn &resolveStructTypeName,
                                     const ResolveStructArrayInfoForReturnFn &resolveStructArrayInfoFromPath,
                                     ReturnInfo &info,
                                     bool &hasReturnTransform,
                                     bool &hasReturnAuto);

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferReturnInferenceBindingIntoLocals(const Expr &bindingExpr,
                                           bool isParameter,
                                           const std::string &definitionPath,
                                           LocalMap &activeLocals,
                                           const IsBindingMutableForInferenceFn &isBindingMutable,
                                           const BindingKindForInferenceFn &bindingKind,
                                           const HasExplicitBindingTypeTransformForInferenceFn
                                               &hasExplicitBindingTypeTransform,
                                           const BindingValueKindForInferenceFn &bindingValueKind,
                                           const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                                           const IsFileErrorBindingForInferenceFn &isFileErrorBinding,
                                           const SetReferenceArrayInfoForInferenceFn &setReferenceArrayInfo,
                                           const ApplyStructInfoForInferenceFn &applyStructArrayInfo,
                                           const ApplyStructInfoForInferenceFn &applyStructValueInfo,
                                           const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                                           const IsStringBindingForInferenceFn &isStringBinding,
                                           std::string &error);

// END IrLowererReturnInferenceHelpers.h

// BEGIN IrLowererSetupMathHelpers.h




using GetSetupMathBuiltinNameFn = std::function<bool(const Expr &, std::string &)>;
using GetSetupMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;

struct SetupMathResolvers {
  GetSetupMathBuiltinNameFn getMathBuiltinName;
  GetSetupMathConstantNameFn getMathConstantName;
};
struct SetupMathAndBindingAdapters {
  SetupMathResolvers setupMathResolvers;
  BindingTypeAdapters bindingTypeAdapters;
};

bool isMathImportPath(const std::string &path);
bool hasProgramMathImport(const std::vector<std::string> &imports);
bool isSupportedMathBuiltinName(const std::string &name);
SetupMathResolvers makeSetupMathResolvers(bool hasMathImport);
SetupMathAndBindingAdapters makeSetupMathAndBindingAdapters(bool hasMathImport);
GetSetupMathBuiltinNameFn makeGetSetupMathBuiltinName(bool hasMathImport);
GetSetupMathConstantNameFn makeGetSetupMathConstantName(bool hasMathImport);
bool getSetupMathBuiltinName(const Expr &expr, bool hasMathImport, std::string &out);
bool getSetupMathConstantName(const std::string &name, bool hasMathImport, std::string &out);

// END IrLowererSetupMathHelpers.h

// BEGIN IrLowererSetupInferenceHelpers.h




using GetSetupInferenceBuiltinOperatorNameFn = std::function<bool(const Expr &, std::string &)>;
using InferSetupInferenceValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveSetupInferenceExprPathFn = std::function<std::string(const Expr &)>;
using ResolveSetupInferenceArrayElementKindByPathFn =
    std::function<bool(const std::string &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceArrayReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceCallReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &, bool &)>;
using ResolveSetupInferenceCallCollectionAccessValueKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using IsSetupInferenceEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsSetupInferenceBindingMutableFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasSetupInferenceExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingValueKindFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using ApplySetupInferenceStructInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using InferSetupInferenceStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using SetupInferenceCombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using LowerSetupInferenceMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using InferSetupInferenceBodyValueKindFn =
    std::function<LocalInfo::ValueKind(const std::vector<Expr> &, const LocalMap &)>;
using IsSetupInferenceKnownDefinitionPathFn = std::function<bool(const std::string &)>;
using IsSetupInferenceMethodCountLikeCallFn = std::function<bool(const Expr &, const LocalMap &)>;

enum class CallExpressionReturnKindResolution {
  NotResolved,
  Resolved,
  MatchedButUnsupported,
};
enum class ArrayMapAccessElementKindResolution {
  NotMatched,
  Resolved,
};
enum class MathBuiltinReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class NonMathScalarCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class ControlFlowCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class PointerBuiltinCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class ComparisonOperatorCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class GpuBufferCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class CountCapacityCallReturnKindResolution {
  NotMatched,
  Resolved,
};

LocalInfo::ValueKind inferPointerTargetValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const GetSetupInferenceBuiltinOperatorNameFn &getBuiltinOperatorName);
LocalInfo::ValueKind inferBufferElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferArrayElementKind);
LocalInfo::ValueKind inferArrayElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const ResolveSetupInferenceArrayElementKindByPathFn &resolveStructArrayElementKindByPath,
    const ResolveSetupInferenceArrayReturnKindFn &resolveDirectCallArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveCountMethodArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveMethodCallArrayReturnKind);
CallExpressionReturnKindResolution resolveCallExpressionReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceCallReturnKindFn &resolveDefinitionCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveCountMethodCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveMethodCallReturnKind,
    LocalInfo::ValueKind &kindOut);
ArrayMapAccessElementKindResolution resolveArrayMapAccessElementKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceEntryArgsNameFn &isEntryArgsName,
    LocalInfo::ValueKind &kindOut,
    const ResolveSetupInferenceCallCollectionAccessValueKindFn &resolveCallCollectionAccessValueKind =
        ResolveSetupInferenceCallCollectionAccessValueKindFn{});
LocalInfo::ValueKind inferBodyValueKindWithLocalsScaffolding(
    const std::vector<Expr> &bodyExpressions,
    const LocalMap &localsBase,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const IsSetupInferenceBindingMutableFn &isBindingMutable,
    const SetupInferenceBindingKindFn &bindingKind,
    const HasSetupInferenceExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const SetupInferenceBindingValueKindFn &bindingValueKind,
    const ApplySetupInferenceStructInfoFn &applyStructArrayInfo,
    const ApplySetupInferenceStructInfoFn &applyStructValueInfo,
    const InferSetupInferenceStructExprPathFn &inferStructExprPath);
MathBuiltinReturnKindResolution inferMathBuiltinReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
NonMathScalarCallReturnKindResolution inferNonMathScalarCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ControlFlowCallReturnKindResolution inferControlFlowCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const LowerSetupInferenceMatchToIfFn &lowerMatchToIf,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    const InferSetupInferenceBodyValueKindFn &inferBodyValueKind,
    const IsSetupInferenceKnownDefinitionPathFn &isKnownDefinitionPath,
    std::string &error,
    LocalInfo::ValueKind &kindOut);
PointerBuiltinCallReturnKindResolution inferPointerBuiltinCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ComparisonOperatorCallReturnKindResolution inferComparisonOperatorCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
GpuBufferCallReturnKindResolution inferGpuBufferCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    LocalInfo::ValueKind &kindOut);
CountCapacityCallReturnKindResolution inferCountCapacityCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceMethodCountLikeCallFn &isArrayCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isStringCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isVectorCapacityCall,
    LocalInfo::ValueKind &kindOut);

// END IrLowererSetupInferenceHelpers.h

// BEGIN IrLowererLowerInferenceSetup.h





struct UninitializedStorageAccessInfo;

struct LowerInferenceSetupBootstrapState {
  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferBufferElementKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferLiteralOrNameExprKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprBaseKind;
  std::function<CallExpressionReturnKindResolution(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprDirectReturnKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprCountAccessGpuFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprOperatorFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, std::string &, LocalInfo::ValueKind &)>
      inferCallExprControlFlowFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprPointerFallbackKind;

  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
};

struct LowerInferenceSetupBootstrapInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;

  IsArrayCountCallFn isArrayCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  ResolveExprPathFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
};

struct LowerInferenceArrayKindSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
};

struct LowerInferenceExprKindBaseSetupInput {
  GetSetupMathConstantNameFn getMathConstantName = {};
};

struct LowerInferenceExprKindCallBaseSetupInput {
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
};
struct LowerInferenceExprKindCallReturnSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
};
struct LowerInferenceExprKindCallFallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
};
struct LowerInferenceExprKindCallOperatorFallbackSetupInput {
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
};
struct LowerInferenceExprKindCallControlFlowFallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  ResolveSetupInferenceExprPathFn resolveExprPath = {};
  LowerSetupInferenceMatchToIfFn lowerMatchToIf = {};
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  IsSetupInferenceBindingMutableFn isBindingMutable = {};
  SetupInferenceBindingKindFn bindingKind = {};
  HasSetupInferenceExplicitBindingTypeTransformFn hasExplicitBindingTypeTransform = {};
  SetupInferenceBindingValueKindFn bindingValueKind = {};
  ApplySetupInferenceStructInfoFn applyStructArrayInfo = {};
  ApplySetupInferenceStructInfoFn applyStructValueInfo = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
};
struct LowerInferenceExprKindCallPointerFallbackSetupInput {};
struct LowerInferenceExprKindDispatchSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  ResolveExprPathFn resolveExprPath = {};
  std::string *error = nullptr;
};

struct LowerInferenceReturnInfoSetupInput {
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  InferValueKindFromLocalsFn inferExprKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  InferValueKindFromLocalsFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};
struct LowerInferenceGetReturnInfoStepInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
};
struct LowerInferenceGetReturnInfoCallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
  std::string *error = nullptr;
};
struct LowerInferenceGetReturnInfoSetupInput {
  const Program *program = nullptr;
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  InferValueKindFromLocalsFn inferExprKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  InferValueKindFromLocalsFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
  std::string *error = nullptr;
};
struct LowerInferenceSetupInput {
  const Program *program = nullptr;
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;

  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  ResolveExprPathFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  GetSetupMathConstantNameFn getMathConstantName = {};

  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut);
bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut);
bool runLowerInferenceArrayKindSetup(const LowerInferenceArrayKindSetupInput &input,
                                     LowerInferenceSetupBootstrapState &stateInOut,
                                     std::string &errorOut);
bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut);
bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceExprKindCallReturnSetup(const LowerInferenceExprKindCallReturnSetupInput &input,
                                              LowerInferenceSetupBootstrapState &stateInOut,
                                              std::string &errorOut);
bool runLowerInferenceExprKindCallFallbackSetup(const LowerInferenceExprKindCallFallbackSetupInput &input,
                                                LowerInferenceSetupBootstrapState &stateInOut,
                                                std::string &errorOut);
bool runLowerInferenceExprKindCallOperatorFallbackSetup(
    const LowerInferenceExprKindCallOperatorFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallControlFlowFallbackSetup(
    const LowerInferenceExprKindCallControlFlowFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallPointerFallbackSetup(
    const LowerInferenceExprKindCallPointerFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceReturnInfoSetup(const LowerInferenceReturnInfoSetupInput &input,
                                      const Definition &definition,
                                      ReturnInfo &infoInOut,
                                      std::string &errorOut);
bool runLowerInferenceGetReturnInfoStep(const LowerInferenceGetReturnInfoStepInput &input,
                                        const std::string &path,
                                        ReturnInfo &outInfo,
                                        std::string &errorOut);
bool runLowerInferenceGetReturnInfoCallbackSetup(const LowerInferenceGetReturnInfoCallbackSetupInput &input,
                                                 std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                                 std::string &errorOut);
bool runLowerInferenceGetReturnInfoSetup(const LowerInferenceGetReturnInfoSetupInput &input,
                                         std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                         std::string &errorOut);

// END IrLowererLowerInferenceSetup.h

// BEGIN IrLowererLowerInlineCallCleanupStep.h




struct LowerInlineCallCleanupStepInput {
  IrFunction *function = nullptr;
  const std::vector<size_t> *returnJumps = nullptr;
  std::function<void()> emitCurrentFileScopeCleanup;
  std::function<void()> popFileScope;
};

bool runLowerInlineCallCleanupStep(const LowerInlineCallCleanupStepInput &input,
                                   std::string &errorOut);

// END IrLowererLowerInlineCallCleanupStep.h

// BEGIN IrLowererLowerInlineCallContextSetupStep.h




struct LowerInlineCallContextSetupStepInput {
  IrFunction *function = nullptr;
  const ReturnInfo *returnInfo = nullptr;
  std::function<int32_t()> allocTempLocal;
};

struct LowerInlineCallContextSetupStepOutput {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
};

bool runLowerInlineCallContextSetupStep(const LowerInlineCallContextSetupStepInput &input,
                                        LowerInlineCallContextSetupStepOutput &output,
                                        std::string &errorOut);

// END IrLowererLowerInlineCallContextSetupStep.h

// BEGIN IrLowererLowerInlineCallGpuLocalsStep.h




struct LowerInlineCallGpuLocalsStepInput {
  const LocalMap *callerLocals = nullptr;
  LocalMap *calleeLocals = nullptr;
};

bool runLowerInlineCallGpuLocalsStep(const LowerInlineCallGpuLocalsStepInput &input,
                                     std::string &errorOut);

// END IrLowererLowerInlineCallGpuLocalsStep.h

// BEGIN IrLowererLowerInlineCallReturnValueStep.h




struct LowerInlineCallReturnValueStepInput {
  IrFunction *function = nullptr;
  bool returnsVoid = true;
  int32_t returnLocal = -1;
  bool structDefinition = false;
  bool requireValue = false;
};

bool runLowerInlineCallReturnValueStep(const LowerInlineCallReturnValueStepInput &input,
                                       std::string &errorOut);

// END IrLowererLowerInlineCallReturnValueStep.h

// BEGIN IrLowererLowerImportsStructsSetup.h





bool runLowerImportsStructsSetup(
    const Program &program,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut);

// END IrLowererLowerImportsStructsSetup.h

// BEGIN IrLowererLowerInlineCallStatementStep.h




struct LowerInlineCallStatementStepInput {
  IrFunction *function = nullptr;
  std::function<bool(const Expr &)> emitStatement;
  std::function<void(const std::string &, const Expr &, size_t, size_t)> appendInstructionSourceRange;
};

bool runLowerInlineCallStatementStep(const LowerInlineCallStatementStepInput &input,
                                     const Expr &stmt,
                                     std::string &errorOut);

// END IrLowererLowerInlineCallStatementStep.h

// BEGIN IrLowererStringLiteralHelpers.h




using InternStringLiteralFn = std::function<int32_t(const std::string &)>;
using ResolveStringTableTargetFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;

struct StringLiteralHelperContext {
  InternStringLiteralFn internString;
  ResolveStringTableTargetFn resolveStringTableTarget;
};

int32_t internLowererString(const std::string &text, std::vector<std::string> &stringTable);
bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error);
StringLiteralHelperContext makeStringLiteralHelperContext(std::vector<std::string> &stringTable, std::string &error);
InternStringLiteralFn makeInternLowererString(std::vector<std::string> &stringTable);
ResolveStringTableTargetFn makeResolveStringTableTarget(const std::vector<std::string> &stringTable,
                                                        const InternStringLiteralFn &internString,
                                                        std::string &error);
bool resolveStringTableTarget(const Expr &expr,
                              const LocalMap &localsIn,
                              const std::vector<std::string> &stringTable,
                              const InternStringLiteralFn &internString,
                              int32_t &stringIndexOut,
                              size_t &lengthOut,
                              std::string &error);

// END IrLowererStringLiteralHelpers.h

// BEGIN IrLowererRuntimeErrorHelpers.h




using InternRuntimeErrorStringFn = std::function<int32_t(const std::string &)>;
using EmitRuntimeErrorFn = std::function<void()>;

struct RuntimeErrorEmitters {
  EmitRuntimeErrorFn emitArrayIndexOutOfBounds;
  EmitRuntimeErrorFn emitPointerIndexOutOfBounds;
  EmitRuntimeErrorFn emitStringIndexOutOfBounds;
  EmitRuntimeErrorFn emitMapKeyNotFound;
  EmitRuntimeErrorFn emitVectorIndexOutOfBounds;
  EmitRuntimeErrorFn emitVectorPopOnEmpty;
  EmitRuntimeErrorFn emitVectorCapacityExceeded;
  EmitRuntimeErrorFn emitVectorReserveNegative;
  EmitRuntimeErrorFn emitVectorReserveExceeded;
  EmitRuntimeErrorFn emitLoopCountNegative;
  EmitRuntimeErrorFn emitPowNegativeExponent;
  EmitRuntimeErrorFn emitFloatToIntNonFinite;
};

struct RuntimeErrorAndStringLiteralSetup {
  StringLiteralHelperContext stringLiteralHelpers;
  RuntimeErrorEmitters runtimeErrorEmitters;
};
enum class FileErrorWhyCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};

RuntimeErrorAndStringLiteralSetup makeRuntimeErrorAndStringLiteralSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    std::string &error);
RuntimeErrorEmitters makeRuntimeErrorEmitters(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitArrayIndexOutOfBounds(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitPointerIndexOutOfBounds(IrFunction &function,
                                                   const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitStringIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorCapacityExceeded(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorReserveNegative(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitVectorReserveExceeded(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString);
EmitRuntimeErrorFn makeEmitFloatToIntNonFinite(IrFunction &function,
                                                const InternRuntimeErrorStringFn &internString);

void emitArrayIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitPointerIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitStringIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorCapacityExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorReserveNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitVectorReserveExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitFloatToIntNonFinite(IrFunction &function, const InternRuntimeErrorStringFn &internString);
void emitFileErrorWhy(IrFunction &function, int32_t errorLocal, const InternRuntimeErrorStringFn &internString);
FileErrorWhyCallEmitResult tryEmitFileErrorWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(int32_t)> &emitFileErrorWhy,
    std::string &error);

// END IrLowererRuntimeErrorHelpers.h

// BEGIN IrLowererUninitializedTypeHelpers.h




struct UninitializedTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct UninitializedFieldBindingInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

using UninitializedFieldBindingIndex =
    std::unordered_map<std::string, std::vector<UninitializedFieldBindingInfo>>;
struct StructAndUninitializedFieldIndexes {
  StructLayoutFieldIndex structLayoutFieldIndex;
  UninitializedFieldBindingIndex uninitializedFieldBindingIndex;
};
using AppendUninitializedFieldBindingFn =
    std::function<void(const std::string &structPath, const UninitializedFieldBindingInfo &fieldBinding)>;
using EnumerateUninitializedFieldBindingsFn =
    std::function<void(const AppendUninitializedFieldBindingFn &appendFieldBinding)>;

using FindUninitializedFieldTemplateArgFn =
    std::function<bool(const std::string &structPath, const std::string &fieldName, std::string &typeTemplateArgOut)>;
using CollectUninitializedFieldBindingsFn =
    std::function<bool(const std::string &structPath, std::vector<UninitializedFieldBindingInfo> &fieldsOut)>;
using ResolveDefinitionNamespacePrefixFn = std::function<std::string(const std::string &structPath)>;
using ResolveUninitializedFieldTypeInfoFn =
    std::function<bool(const std::string &typeText, const std::string &namespacePrefix, UninitializedTypeInfo &out)>;
using InferStructReturnExprWithVisitedFn =
    std::function<std::string(const Expr &, std::unordered_set<std::string> &)>;
using InferDefinitionStructReturnPathWithVisitedFn =
    std::function<std::string(const std::string &, std::unordered_set<std::string> &)>;

struct UninitializedFieldStorageTypeInfo {
  const LocalInfo *receiver = nullptr;
  std::string structPath;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedFieldStorageAccessInfo {
  const LocalInfo *receiver = nullptr;
  StructSlotFieldInfo fieldSlot;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedStorageAccessInfo {
  enum class Location { Local, Field, Indirect } location = Location::Local;
  const LocalInfo *local = nullptr;
  const LocalInfo *pointer = nullptr;
  const Expr *pointerExpr = nullptr;
  const LocalInfo *receiver = nullptr;
  StructSlotFieldInfo fieldSlot;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedLocalStorageAccessInfo {
  const LocalInfo *local = nullptr;
  UninitializedTypeInfo typeInfo;
};

using ResolveUninitializedStorageAccessFromFieldIndexFn =
    std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>;

struct UninitializedResolutionAdapters {
  ResolveUninitializedFieldTypeInfoFn resolveUninitializedTypeInfo;
  ResolveUninitializedStorageAccessFromFieldIndexFn resolveUninitializedStorage;
  InferStructExprWithLocalsFn inferStructExprPath;
};

struct StructAndUninitializedResolutionSetup {
  StructAndUninitializedFieldIndexes fieldIndexes;
  StructLayoutResolutionAdapters structLayoutResolutionAdapters;
  UninitializedResolutionAdapters uninitializedResolutionAdapters;
};
struct SetupTypeStructAndUninitializedResolutionSetup {
  SetupTypeAndStructTypeAdapters setupTypeAndStructTypeAdapters;
  StructAndUninitializedResolutionSetup structAndUninitializedResolutionSetup;
};
struct SetupMathTypeStructAndUninitializedResolutionSetup {
  SetupMathAndBindingAdapters setupMathAndBindingAdapters;
  SetupTypeStructAndUninitializedResolutionSetup setupTypeStructAndUninitializedResolutionSetup;
};
struct EntrySetupMathTypeStructAndUninitializedResolutionSetup {
  EntryCountCallOnErrorSetup entryCountCallOnErrorSetup;
  SetupMathTypeStructAndUninitializedResolutionSetup setupMathTypeStructAndUninitializedResolutionSetup;
};
struct RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup {
  RuntimeErrorAndStringLiteralSetup runtimeErrorAndStringLiteralSetup;
  EntrySetupMathTypeStructAndUninitializedResolutionSetup entrySetupMathTypeStructAndUninitializedResolutionSetup;
};
struct EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup {
  EntryReturnConfig entryReturnConfig;
  RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup;
};

bool buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildSetupMathTypeStructAndUninitializedResolutionSetup(
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildSetupTypeStructAndUninitializedResolutionSetup(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildStructAndUninitializedResolutionSetup(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    const InferStructExprPathFn &resolveExprPath,
    StructAndUninitializedResolutionSetup &out,
    std::string &error);
UninitializedResolutionAdapters makeUninitializedResolutionAdapters(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error);
ResolveUninitializedFieldTypeInfoFn makeResolveUninitializedTypeInfo(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    std::string &error);
bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypeNameFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error);
bool resolveUninitializedTypeInfoFromLocalStorage(const LocalInfo &local, UninitializedTypeInfo &out);
bool resolveUninitializedLocalStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const LocalInfo *&localOut,
                                               UninitializedTypeInfo &typeInfoOut,
                                               bool &resolvedOut);
bool resolveUninitializedLocalStorageAccess(const Expr &storage,
                                            const LocalMap &localsIn,
                                            UninitializedLocalStorageAccessInfo &out,
                                            bool &resolvedOut);
StructAndUninitializedFieldIndexes buildStructAndUninitializedFieldIndexes(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields);
UninitializedFieldBindingIndex buildUninitializedFieldBindingIndex(
    std::size_t structReserveHint,
    const EnumerateUninitializedFieldBindingsFn &enumerateFieldBindings);
UninitializedFieldBindingIndex buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex);
bool hasUninitializedFieldBindingsForStructPath(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath);
std::string inferStructPathFromCallTargetWithFieldBindingIndex(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath);
std::string inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathWithVisitedFn &inferDefinitionStructReturnPath,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMapWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMap(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath);
std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex);
std::string inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);
InferStructExprWithLocalsFn makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);
bool collectUninitializedFieldBindingsFromIndex(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath,
                                                std::vector<UninitializedFieldBindingInfo> &fieldsOut);
bool resolveUninitializedFieldTemplateArg(const std::string &structPath,
                                          const std::string &fieldName,
                                          const CollectUninitializedFieldBindingsFn &collectFieldBindings,
                                          std::string &typeTemplateArgOut);
bool findUninitializedFieldTemplateArg(const std::vector<UninitializedFieldBindingInfo> &fields,
                                       const std::string &fieldName,
                                       std::string &typeTemplateArgOut);
bool resolveUninitializedFieldStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                               const LocalInfo *&receiverOut,
                                               std::string &structPathOut,
                                               std::string &typeTemplateArgOut);
bool resolveUninitializedFieldStorageTypeInfo(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    UninitializedFieldStorageTypeInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedFieldStorageAccess(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedFieldStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccess(const Expr &storage,
                                       const LocalMap &localsIn,
                                       const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                       const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
                                       const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
                                       const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                                       UninitializedStorageAccessInfo &out,
                                       bool &resolvedOut,
                                       std::string &error);
bool resolveUninitializedStorageAccessWithFieldBindings(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccessFromDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const Expr &storage,
    const LocalMap &localsIn,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
ResolveUninitializedStorageAccessFromFieldIndexFn
makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error);

// END IrLowererUninitializedTypeHelpers.h

// BEGIN IrLowererSetupLocalsHelpers.h



struct SetupLocalsOrchestration {
  EntryReturnConfig entryReturnConfig;
  RuntimeErrorAndStringLiteralSetup runtimeErrorAndStringLiteralSetup;
  EntryCountAccessSetup entryCountAccessSetup;
  EntryCallOnErrorSetup entryCallOnErrorSetup;
  SetupMathResolvers setupMathResolvers;
  BindingTypeAdapters bindingTypeAdapters;
  SetupTypeAndStructTypeAdapters setupTypeAndStructTypeAdapters;
  StructArrayInfoAdapters structArrayInfoAdapters;
  StructSlotResolutionAdapters structSlotResolutionAdapters;
  UninitializedResolutionAdapters uninitializedResolutionAdapters;
  ApplyStructValueInfoFn applyStructValueInfo;
};

SetupLocalsOrchestration unpackSetupLocalsOrchestration(
    const EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &setup);

// END IrLowererSetupLocalsHelpers.h

// BEGIN IrLowererLowerLocalsSetup.h





bool runLowerLocalsSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    SetupLocalsOrchestration &setupLocalsOrchestrationOut,
    std::string &errorOut);

// END IrLowererLowerLocalsSetup.h

// BEGIN IrLowererLowerReturnCallsSetup.h



using LowerReturnCallsEmitFileErrorWhyFn = std::function<bool(int32_t)>;

struct LowerReturnCallsSetupInput {
  IrFunction *function = nullptr;
  InternRuntimeErrorStringFn internString;
};

bool runLowerReturnCallsSetup(const LowerReturnCallsSetupInput &input,
                              LowerReturnCallsEmitFileErrorWhyFn &emitFileErrorWhyOut,
                              std::string &errorOut);
// END IrLowererLowerReturnCallsSetup.h

// BEGIN IrLowererLowerStatementsCallsStep.h




struct LowerStatementsCallsStepInput {
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<int32_t()> allocTempLocal;

  std::function<std::string(const Expr &)> resolveExprPath;
  std::function<const Definition *(const std::string &)> findDefinitionByPath;

  std::function<bool(const Expr &, const LocalMap &)> isArrayCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isStringCountCall;
  std::function<bool(const Expr &, const LocalMap &)> isVectorCapacityCall;
  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut);

// END IrLowererLowerStatementsCallsStep.h

// BEGIN IrLowererLowerStatementsEntryExecutionStep.h




struct LowerStatementsEntryExecutionStepInput {
  const Definition *entryDef = nullptr;
  bool returnsVoid = false;
  bool *sawReturn = nullptr;

  const std::unordered_map<std::string, std::optional<OnErrorHandler>> *onErrorByDef = nullptr;
  std::optional<OnErrorHandler> *currentOnError = nullptr;
  std::optional<ResultReturnInfo> *currentReturnResult = nullptr;

  bool entryHasResultInfo = false;
  const ResultReturnInfo *entryResultInfo = nullptr;

  std::function<bool(const Expr &)> emitEntryStatement;
  std::function<void()> pushFileScope;
  std::function<void()> emitCurrentFileScopeCleanup;
  std::function<void()> popFileScope;

  std::vector<IrInstruction> *instructions = nullptr;
};

bool runLowerStatementsEntryExecutionStep(const LowerStatementsEntryExecutionStepInput &input,
                                          std::string &errorOut);

// END IrLowererLowerStatementsEntryExecutionStep.h

// BEGIN IrLowererLowerStatementsEntryStatementStep.h




struct LowerStatementsEntryStatementStepInput {
  IrFunction *function = nullptr;
  std::function<bool(const Expr &)> emitStatement;
  std::function<void(const std::string &, const Expr &, size_t, size_t)> appendInstructionSourceRange;
};

bool runLowerStatementsEntryStatementStep(const LowerStatementsEntryStatementStepInput &input,
                                          const Expr &stmt,
                                          std::string &errorOut);

// END IrLowererLowerStatementsEntryStatementStep.h

// BEGIN IrLowererLowerStatementsFunctionTableStep.h




struct LowerStatementsFunctionTableStepInput {
  const Program *program = nullptr;
  const Definition *entryDef = nullptr;
  IrFunction *function = nullptr;
  const std::unordered_set<std::string> *loweredCallTargets = nullptr;

  std::function<bool(const Definition &)> isStructDefinition;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  const std::vector<std::string> *defaultEffects = nullptr;
  const std::vector<std::string> *entryDefaultEffects = nullptr;
  std::function<bool(const Expr &)> isTailCallCandidate;

  std::function<void()> resetDefinitionLoweringState;
  std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> buildDefinitionCallContext;
  std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> emitInlineDefinitionCall;

  int32_t *nextLocal = nullptr;
  std::vector<IrFunction> *outFunctions = nullptr;
  int32_t *entryIndex = nullptr;
};

bool runLowerStatementsFunctionTableStep(const LowerStatementsFunctionTableStepInput &input,
                                         std::string &errorOut);

// END IrLowererLowerStatementsFunctionTableStep.h

// BEGIN IrLowererLowerStatementsSourceMapStep.h




struct InstructionSourceRange {
  size_t beginIndex = 0;
  size_t endIndex = 0;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::CanonicalAst;
};

struct LowerStatementsSourceMapStepInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> *instructionSourceRangesByFunction = nullptr;
  std::vector<std::string> *stringTable = nullptr;
  IrModule *outModule = nullptr;
};

bool runLowerStatementsSourceMapStep(const LowerStatementsSourceMapStepInput &input,
                                     std::string &errorOut);

// END IrLowererLowerStatementsSourceMapStep.h

// BEGIN IrLowererOperatorArithmeticHelpers.h




enum class OperatorArithmeticEmitResult { Handled, NotHandled, Error };

using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using InferStructExprPathWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using CombineNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;

OperatorArithmeticEmitResult emitArithmeticOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitExprWithLocalsFn &emitExpr,
                                                        const InferExprKindWithLocalsFn &inferExprKind,
                                                        const InferStructExprPathWithLocalsFn &inferStructExprPath,
                                                        const CombineNumericKindsFn &combineNumericKinds,
                                                        const EmitInstructionFn &emitInstruction,
                                                        std::string &error);

// END IrLowererOperatorArithmeticHelpers.h

// BEGIN IrLowererOperatorArcHyperbolicHelpers.h




enum class OperatorArcHyperbolicEmitResult { Handled, NotHandled, Error };

using EmitArcHyperbolicExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferArcHyperbolicExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocArcHyperbolicTempLocalFn = std::function<int32_t()>;

OperatorArcHyperbolicEmitResult emitArcHyperbolicOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitArcHyperbolicExprWithLocalsFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const AllocArcHyperbolicTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

// END IrLowererOperatorArcHyperbolicHelpers.h

// BEGIN IrLowererOperatorClampMinMaxTrigHelpers.h




enum class OperatorClampMinMaxTrigEmitResult { Handled, NotHandled, Error };

using EmitClampMinMaxTrigExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferClampMinMaxTrigExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombineClampMinMaxTrigNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using AllocClampMinMaxTrigTempLocalFn = std::function<int32_t()>;

OperatorClampMinMaxTrigEmitResult emitClampMinMaxTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitClampMinMaxTrigExprWithLocalsFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const CombineClampMinMaxTrigNumericKindsFn &combineNumericKinds,
    const AllocClampMinMaxTrigTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

// END IrLowererOperatorClampMinMaxTrigHelpers.h

// BEGIN IrLowererOperatorComparisonHelpers.h




enum class OperatorComparisonEmitResult { Handled, NotHandled, Error };

using EmitComparisonExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferComparisonExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ComparisonKindFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitComparisonToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitComparisonExprWithLocalsFn &emitExpr,
                                                        const InferComparisonExprKindWithLocalsFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

// END IrLowererOperatorComparisonHelpers.h

// BEGIN IrLowererOperatorConversionsAndCallsHelpers.h




using EmitConversionsAndCallsExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitConversionsAndCallsStatementWithLocalsFn = std::function<bool(const Expr &, LocalMap &)>;
using InferConversionsAndCallsExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombineConversionsAndCallsNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitConversionsAndCallsCompareToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
using AllocConversionsAndCallsTempLocalFn = std::function<int32_t()>;
using EmitConversionsAndCallsFloatToIntNonFiniteFn = std::function<void()>;
using EmitConversionsAndCallsPointerIndexOutOfBoundsFn = std::function<void()>;
using EmitConversionsAndCallsArrayIndexOutOfBoundsFn = std::function<void()>;
using ResolveConversionsAndCallsStringTableTargetFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using ConversionsAndCallsValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using ConversionsAndCallsGetMathConstantNameFn = std::function<bool(const std::string &, std::string &)>;
using InferConversionsAndCallsStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using ResolveConversionsAndCallsStructTypeNameFn =
    std::function<bool(const std::string &, const std::string &, std::string &)>;
using ResolveConversionsAndCallsStructSlotCountFn = std::function<bool(const std::string &, int32_t &)>;
using ResolveConversionsAndCallsStructFieldInfoFn =
    std::function<bool(const std::string &, const std::string &, int32_t &, int32_t &, std::string &)>;
using EmitConversionsAndCallsStructCopyFromPtrsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using HasConversionsAndCallsNamedArgumentsFn =
    std::function<bool(const std::vector<std::optional<std::string>> &)>;
using ResolveConversionsAndCallsDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using ResolveConversionsAndCallsExprPathFn = std::function<std::string(const Expr &)>;
using LowerConversionsAndCallsMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using IsConversionsAndCallsBindingMutableFn = std::function<bool(const Expr &)>;
using ConversionsAndCallsBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasConversionsAndCallsExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using ConversionsAndCallsBindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using ApplyConversionsAndCallsStructArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyConversionsAndCallsStructValueInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using EnterConversionsAndCallsScopedBlockFn = std::function<void()>;
using ExitConversionsAndCallsScopedBlockFn = std::function<void()>;
using IsConversionsAndCallsReturnCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsBlockCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsMatchCallFn = std::function<bool(const Expr &)>;
using IsConversionsAndCallsIfCallFn = std::function<bool(const Expr &)>;

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const EmitConversionsAndCallsPointerIndexOutOfBoundsFn &emitPointerIndexOutOfBounds,
    const EmitConversionsAndCallsArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

bool emitConversionsAndCallsControlExprTail(
    const Expr &expr,
    const LocalMap &localsIn,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const EmitConversionsAndCallsStatementWithLocalsFn &emitStatement,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const CombineConversionsAndCallsNumericKindsFn &combineNumericKinds,
    const HasConversionsAndCallsNamedArgumentsFn &hasNamedArguments,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ResolveConversionsAndCallsExprPathFn &resolveExprPath,
    const LowerConversionsAndCallsMatchToIfFn &lowerMatchToIf,
    const IsConversionsAndCallsBindingMutableFn &isBindingMutable,
    const ConversionsAndCallsBindingKindFn &bindingKind,
    const HasConversionsAndCallsExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const ConversionsAndCallsBindingValueKindFn &bindingValueKind,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ApplyConversionsAndCallsStructArrayInfoFn &applyStructArrayInfo,
    const ApplyConversionsAndCallsStructValueInfoFn &applyStructValueInfo,
    const EnterConversionsAndCallsScopedBlockFn &enterScopedBlock,
    const ExitConversionsAndCallsScopedBlockFn &exitScopedBlock,
    const IsConversionsAndCallsReturnCallFn &isReturnCall,
    const IsConversionsAndCallsBlockCallFn &isBlockCall,
    const IsConversionsAndCallsMatchCallFn &isMatchCall,
    const IsConversionsAndCallsIfCallFn &isIfCall,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error);

// END IrLowererOperatorConversionsAndCallsHelpers.h

// BEGIN IrLowererOperatorPowAbsSignHelpers.h




enum class OperatorPowAbsSignEmitResult { Handled, NotHandled, Error };

using EmitPowAbsSignExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferPowAbsSignExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombinePowAbsSignNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using AllocPowAbsSignTempLocalFn = std::function<int32_t()>;
using EmitPowNegativeExponentFn = std::function<void()>;

OperatorPowAbsSignEmitResult emitPowAbsSignOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        bool hasMathImport,
                                                        const EmitPowAbsSignExprWithLocalsFn &emitExpr,
                                                        const InferPowAbsSignExprKindWithLocalsFn &inferExprKind,
                                                        const CombinePowAbsSignNumericKindsFn &combineNumericKinds,
                                                        const AllocPowAbsSignTempLocalFn &allocTempLocal,
                                                        const EmitPowNegativeExponentFn &emitPowNegativeExponent,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

// END IrLowererOperatorPowAbsSignHelpers.h

// BEGIN IrLowererOperatorSaturateRoundingRootsHelpers.h




enum class OperatorSaturateRoundingRootsEmitResult { Handled, NotHandled, Error };

using EmitSaturateExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferSaturateExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocSaturateTempLocalFn = std::function<int32_t()>;

OperatorSaturateRoundingRootsEmitResult emitSaturateRoundingRootsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitSaturateExprWithLocalsFn &emitExpr,
    const InferSaturateExprKindWithLocalsFn &inferExprKind,
    const AllocSaturateTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

// END IrLowererOperatorSaturateRoundingRootsHelpers.h

// BEGIN IrLowererResultHelpers.h




struct ResultExprInfo {
  bool isResult = false;
  bool hasValue = false;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  std::string errorType;
};

struct LocalResultInfo {
  bool found = false;
  bool isResult = false;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  bool isFileHandle = false;
};

using LookupLocalResultInfoFn = std::function<LocalResultInfo(const std::string &)>;
using ResolveCallDefinitionFn = std::function<const Definition *(const Expr &)>;
using LookupDefinitionResultInfoFn = std::function<bool(const std::string &, ResultExprInfo &)>;
using ResolveMethodCallWithLocalsFn = std::function<const Definition *(const Expr &, const LocalMap &)>;
using LookupReturnInfoFn = std::function<bool(const std::string &, ReturnInfo &)>;
using ResolveResultExprInfoWithLocalsFn =
    std::function<bool(const Expr &, const LocalMap &, ResultExprInfo &)>;
struct ResultWhyCallOps;
struct ResultWhyExprOps {
  std::function<bool()> emitEmptyString;
  std::function<Expr(LocalMap &, LocalInfo::ValueKind)> makeErrorValueExpr;
  std::function<Expr(LocalMap &)> makeBoolErrorExpr;
};

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out);
bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out);
ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind);
bool resolveResultWhyCallInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                              ResultExprInfo &resultInfo,
                              std::string &error);
bool resolveResultErrorCallInfo(const Expr &expr,
                                const LocalMap &localsIn,
                                const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                                ResultExprInfo &resultInfo,
                                std::string &error);
enum class ResultErrorMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultWhyMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultWhyDispatchEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultOkMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
std::string unsupportedPackedResultValueKindError(const std::string &builtinName);
ResultOkMethodCallEmitResult tryEmitResultOkCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
ResultErrorMethodCallEmitResult tryEmitResultErrorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
bool emitResultWhyLocalsFromValueExpr(
    const Expr &valueExpr,
    const LocalMap &localsIn,
    bool resultHasValue,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t &errorLocalOut);
ResultWhyMethodCallEmitResult tryEmitResultWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error);
ResultWhyDispatchEmitResult tryEmitResultWhyDispatchCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error);
ResultWhyExprOps makeResultWhyExprOps(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t &onErrorTempCounter,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
ResultWhyCallOps makeResultWhyCallOps(
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<Expr(LocalMap &, LocalInfo::ValueKind)> &makeErrorValueExpr,
    const std::function<Expr(LocalMap &)> &makeBoolErrorExpr,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    const std::function<bool()> &emitEmptyString);
bool emitResultWhyCallWithComposedOps(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyExprOps &exprOps,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error);
bool isSupportedResultWhyErrorKind(LocalInfo::ValueKind kind);
std::string normalizeResultWhyErrorName(const std::string &errorType, LocalInfo::ValueKind errorKind);
void emitResultWhyErrorLocalFromResult(
    int32_t resultLocal,
    bool resultHasValue,
    int32_t errorLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool emitResultWhyEmptyString(
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
Expr makeResultWhyErrorValueExpr(int32_t errorLocal,
                                 LocalInfo::ValueKind valueKind,
                                 const std::string &namespacePrefix,
                                 int32_t tempOrdinal,
                                 LocalMap &callLocals);
Expr makeResultWhyBoolErrorExpr(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t tempOrdinal,
    LocalMap &callLocals,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
enum class ResultWhyCallEmitResult {
  Emitted,
  Error,
};
struct ResultWhyCallOps {
  std::function<bool(const std::string &, const std::string &, std::string &)> resolveStructTypeName;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<LocalInfo::Kind(const Expr &)> bindingKind;
  std::function<bool(const Expr &, std::string &, std::vector<std::string> &)> extractFirstBindingTypeTransform;
  std::function<bool(const std::string &, StructSlotLayoutInfo &)> resolveStructSlotLayout;
  std::function<LocalInfo::ValueKind(const std::string &)> valueKindFromTypeName;
  std::function<Expr(LocalMap &, LocalInfo::ValueKind)> makeErrorValueExpr;
  std::function<Expr(LocalMap &)> makeBoolErrorExpr;
  std::function<bool(const Expr &, const Definition &, const LocalMap &)> emitInlineDefinitionCall;
  std::function<bool(int32_t)> emitFileErrorWhy;
  std::function<bool()> emitEmptyString;
};
ResultWhyCallEmitResult emitResolvedResultWhyCall(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyCallOps &ops,
    std::string &error);

// END IrLowererResultHelpers.h

// BEGIN IrLowererStatementBindingHelpers.h




struct StructSlotLayoutInfo;
struct UninitializedStorageAccessInfo;
struct PrintBuiltin;

using HasExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using BindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferBindingExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using IsBindingMutableFn = std::function<bool(const Expr &)>;
using IsFileErrorBindingFn = std::function<bool(const Expr &)>;
using SetReferenceArrayInfoForBindingFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructBindingInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using IsStringBindingFn = std::function<bool(const Expr &)>;
using EmitExprForBindingFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitStatementForBindingFn = std::function<bool(const Expr &, LocalMap &)>;
using EmitBlockForBindingFn = std::function<bool(const Expr &, LocalMap &)>;
using ResolveUninitializedStorageForStatementFn =
    std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>;
using ResolveStructSlotLayoutForStatementFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitStructCopyFromPtrsForStatementFn = std::function<bool(int32_t, int32_t, int32_t)>;
using EmitPrintArgForStatementFn = std::function<bool(const Expr &, const LocalMap &, const PrintBuiltin &)>;
using ResolveDefinitionCallForStatementFn = std::function<const Definition *(const Expr &)>;

enum class UninitializedStorageInitDropEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class UninitializedStorageTakeEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class StatementPrintPathSpaceEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class ReturnStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class StatementMatchIfEmitResult {
  NotMatched,
  Emitted,
  Error,
};

struct ReturnStatementInlineContext {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
  std::vector<size_t> *returnJumps = nullptr;
};

struct StatementBindingTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structTypeName;
};

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind);

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error);
bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition = {},
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall = {},
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo = {});
bool emitStringStatementBindingInitializer(const Expr &stmt,
                                           const Expr &init,
                                           LocalMap &localsIn,
                                           int32_t &nextLocal,
                                           std::vector<IrInstruction> &instructions,
                                           const IsBindingMutableFn &isBindingMutable,
                                           const std::function<int32_t(const std::string &)> &internString,
                                           const EmitExprForBindingFn &emitExpr,
                                           const InferBindingExprKindFn &inferExprKind,
                                           const std::function<int32_t()> &allocTempLocal,
                                           const IsEntryArgsNameFn &isEntryArgsName,
                                           const std::function<void()> &emitArrayIndexOutOfBounds,
                                           std::string &error);
UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error);
UninitializedStorageTakeEmitResult tryEmitUninitializedStorageTakeStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr);
StatementPrintPathSpaceEmitResult tryEmitPrintPathSpaceStatementBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const EmitPrintArgForStatementFn &emitPrintArg,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const EmitExprForBindingFn &emitExpr,
    std::vector<IrInstruction> &instructions,
    std::string &error);
ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error);
StatementMatchIfEmitResult tryEmitMatchIfStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const EmitBlockForBindingFn &emitBlock,
    const EmitStatementForBindingFn &emitStatement,
    std::vector<IrInstruction> &instructions,
    std::string &error);

// END IrLowererStatementBindingHelpers.h

// BEGIN IrLowererStructLayoutHelpers.h




struct BindingTypeLayout {
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
};

uint32_t alignTo(uint32_t value, uint32_t alignment);
bool parsePositiveInt(const std::string &text, int &valueOut);
bool extractAlignment(const std::vector<Transform> &transforms,
                      const std::string &context,
                      uint32_t &alignmentOut,
                      bool &hasAlignment,
                      std::string &error);
bool classifyBindingTypeLayout(const LayoutFieldBinding &binding,
                               BindingTypeLayout &layoutOut,
                               std::string &structTypeNameOut,
                               std::string &errorOut);
bool resolveBindingTypeLayout(
    const LayoutFieldBinding &binding,
    const std::string &namespacePrefix,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &, std::string &)> &computeStructLayout,
    BindingTypeLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutWithCache(
    const std::string &structPath,
    std::unordered_map<std::string, IrStructLayout> &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    const std::function<bool(IrStructLayout &, std::string &)> &computeUncachedLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutUncached(
    const Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool computeStructLayoutFromFieldInfo(
    const Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut);
bool appendProgramStructLayouts(
    const Program &program,
    const std::function<bool(const Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut);
bool appendStructLayoutField(const std::string &structPath,
                             const Expr &fieldExpr,
                             const LayoutFieldBinding &binding,
                             const std::function<bool(const LayoutFieldBinding &,
                                                      BindingTypeLayout &,
                                                      std::string &)> &resolveTypeLayout,
                             uint32_t &offsetInOut,
                             uint32_t &structAlignmentInOut,
                             IrStructLayout &layoutOut,
                             std::string &errorOut);
bool isLayoutQualifierName(const std::string &name);
IrStructFieldCategory fieldCategory(const Expr &expr);
IrStructVisibility fieldVisibility(const Expr &expr);
bool isStaticField(const Expr &expr);

// END IrLowererStructLayoutHelpers.h

// BEGIN IrLowererStructReturnPathHelpers.h




using ResolveStructTypePathFn = std::function<std::string(const std::string &, const std::string &)>;
using ResolveStructLayoutExprPathFn = std::function<std::string(const Expr &)>;

std::string inferStructReturnPathFromDefinition(
    const std::string &defPath,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

std::string inferStructReturnPathFromExpr(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

// END IrLowererStructReturnPathHelpers.h

// BEGIN IrLowererSetupTypeHelpers.h





using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using CombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using InferReceiverExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveReceiverExprPathFn = std::function<std::string(const Expr &)>;
using IsMethodCallClassifierFn = std::function<bool(const Expr &, const LocalMap &)>;
using GetReturnInfoForPathFn = std::function<bool(const std::string &, ReturnInfo &)>;
using ResolveMethodCallDefinitionFn = std::function<const Definition *(const Expr &, const LocalMap &)>;
using ResolveDefinitionCallFn = std::function<const Definition *(const Expr &)>;

struct SetupTypeAdapters {
  ValueKindFromTypeNameFn valueKindFromTypeName;
  CombineNumericKindsFn combineNumericKinds;
};

SetupTypeAdapters makeSetupTypeAdapters();
ValueKindFromTypeNameFn makeValueKindFromTypeName();
CombineNumericKindsFn makeCombineNumericKinds();

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name);
LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
std::string typeNameForValueKind(LocalInfo::ValueKind kind);
std::string normalizeDeclaredCollectionTypeBase(const std::string &base);
bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut);
bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut);
bool resolveMethodCallReceiverExpr(const Expr &callExpr,
                                   const LocalMap &localsIn,
                                   const IsMethodCallClassifierFn &isArrayCountCall,
                                   const IsMethodCallClassifierFn &isVectorCapacityCall,
                                   const IsMethodCallClassifierFn &isEntryArgsName,
                                   const Expr *&receiverOut,
                                   std::string &errorOut);
bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut);
std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind);
std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames);
const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
bool resolveReturnInfoKindForPath(const std::string &path,
                                  const GetReturnInfoForPathFn &getReturnInfo,
                                  bool requireArrayReturn,
                                  LocalInfo::ValueKind &kindOut);
bool resolveMethodCallReturnKind(const Expr &methodCallExpr,
                                 const LocalMap &localsIn,
                                 const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                 const ResolveDefinitionCallFn &resolveDefinitionCall,
                                 const GetReturnInfoForPathFn &getReturnInfo,
                                 bool requireArrayReturn,
                                 LocalInfo::ValueKind &kindOut,
                                 bool *methodResolvedOut = nullptr);
bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut);
bool resolveDefinitionCallReturnKind(const Expr &callExpr,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const ResolveReceiverExprPathFn &resolveExprPath,
                                     const GetReturnInfoForPathFn &getReturnInfo,
                                     bool requireArrayReturn,
                                     LocalInfo::ValueKind &kindOut,
                                     bool *definitionMatchedOut = nullptr);
bool resolveCountMethodCallReturnKind(const Expr &callExpr,
                                      const LocalMap &localsIn,
                                      const IsMethodCallClassifierFn &isArrayCountCall,
                                      const IsMethodCallClassifierFn &isStringCountCall,
                                      const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                      const GetReturnInfoForPathFn &getReturnInfo,
                                      bool requireArrayReturn,
                                      LocalInfo::ValueKind &kindOut,
                                      bool *methodResolvedOut = nullptr,
                                      const InferReceiverExprKindFn &inferExprKind = {});
bool resolveCapacityMethodCallReturnKind(const Expr &callExpr,
                                         const LocalMap &localsIn,
                                         const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                         const GetReturnInfoForPathFn &getReturnInfo,
                                         bool requireArrayReturn,
                                         LocalInfo::ValueKind &kindOut,
                                         bool *methodResolvedOut = nullptr);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
bool resolveMethodReceiverTypeFromNameExpr(const Expr &receiverNameExpr,
                                           const LocalMap &localsIn,
                                           const std::string &methodName,
                                           std::string &typeNameOut,
                                           std::string &resolvedTypePathOut,
                                           std::string &errorOut);
bool resolveMethodReceiverTarget(const Expr &receiverExpr,
                                 const LocalMap &localsIn,
                                 const std::string &methodName,
                                 const std::unordered_map<std::string, std::string> &importAliases,
                                 const std::unordered_set<std::string> &structNames,
                                 const InferReceiverExprKindFn &inferExprKind,
                                 const ResolveReceiverExprPathFn &resolveExprPath,
                                 std::string &typeNameOut,
                                 std::string &resolvedTypePathOut,
                                 std::string &errorOut);

// END IrLowererSetupTypeHelpers.h

// BEGIN IrLowererStringCallHelpers.h




enum class StringCallSource { None, TableIndex, ArgvIndex, RuntimeIndex };

struct StringBindingInfo {
  bool found = false;
  bool isString = false;
  int32_t localIndex = 0;
  StringCallSource source = StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
};

enum class StringCallEmitResult { Handled, NotHandled, Error };

struct StringIndexOps {
  IrOpcode pushZero = IrOpcode::PushI64;
  IrOpcode cmpLt = IrOpcode::CmpLtI64;
  IrOpcode cmpGe = IrOpcode::CmpGeI64;
  bool skipNegativeCheck = false;
};

using InternStringFn = std::function<int32_t(const std::string &)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;
using LookupStringBindingFn = std::function<StringBindingInfo(const std::string &)>;
using ResolveArrayAccessNameFn = std::function<bool(const Expr &, std::string &)>;
using IsStringCallEntryArgsNameFn = std::function<bool(const Expr &)>;
using ResolveStringIndexOpsFn = std::function<bool(const Expr &, const std::string &, StringIndexOps &, std::string &)>;
using EmitExprFn = std::function<bool(const Expr &)>;
using InferCallReturnsStringFn = std::function<bool(const Expr &)>;
using AllocTempLocalFn = std::function<int32_t()>;
using EmitArrayIndexOutOfBoundsFn = std::function<void()>;
using GetInstructionCountFn = std::function<size_t()>;
using PatchInstructionImmFn = std::function<void(size_t, int32_t)>;

StringCallEmitResult emitLiteralOrBindingStringCallValue(const Expr &arg,
                                                         const InternStringFn &internString,
                                                         const EmitInstructionFn &emitInstruction,
                                                         const LookupStringBindingFn &lookupBinding,
                                                         StringCallSource &sourceOut,
                                                         int32_t &stringIndexOut,
                                                         bool &argvCheckedOut,
                                                         std::string &error);

StringCallEmitResult emitCallStringCallValue(const Expr &arg,
                                             const ResolveArrayAccessNameFn &resolveArrayAccessName,
                                             const IsStringCallEntryArgsNameFn &isEntryArgsName,
                                             const ResolveStringIndexOpsFn &resolveStringIndexOps,
                                             const EmitExprFn &emitExpr,
                                             const InferCallReturnsStringFn &inferCallReturnsString,
                                             const AllocTempLocalFn &allocTempLocal,
                                             const EmitInstructionFn &emitInstruction,
                                             const GetInstructionCountFn &getInstructionCount,
                                             const PatchInstructionImmFn &patchInstructionImm,
                                             const EmitArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
                                             StringCallSource &sourceOut,
                                             bool &argvCheckedOut,
                                             std::string &error);
bool emitStringValueForCallFromLocals(const Expr &arg,
                                      const LocalMap &callerLocals,
                                      const InternStringFn &internString,
                                      const EmitInstructionFn &emitInstruction,
                                      const ResolveArrayAccessNameFn &resolveArrayAccessName,
                                      const IsStringCallEntryArgsNameFn &isEntryArgsName,
                                      const ResolveStringIndexOpsFn &resolveStringIndexOps,
                                      const EmitExprFn &emitExpr,
                                      const InferCallReturnsStringFn &inferCallReturnsString,
                                      const AllocTempLocalFn &allocTempLocal,
                                      const GetInstructionCountFn &getInstructionCount,
                                      const PatchInstructionImmFn &patchInstructionImm,
                                      const EmitArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
                                      LocalInfo::StringSource &sourceOut,
                                      int32_t &stringIndexOut,
                                      bool &argvCheckedOut,
                                      std::string &error);

// END IrLowererStringCallHelpers.h

// BEGIN IrLowererTemplateTypeParseHelpers.h




std::string trimTemplateTypeText(const std::string &text);
bool splitTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool parseResultTypeName(const std::string &typeName,
                         bool &hasValue,
                         LocalInfo::ValueKind &valueKind,
                         std::string &errorType);

// END IrLowererTemplateTypeParseHelpers.h

} // namespace primec::ir_lowerer
