#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

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
  bool isWrappedMapTarget = false;
  std::string structTypeName;
};

struct ArrayVectorAccessTargetInfo {
  bool isArrayOrVectorTarget = false;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  bool isVectorTarget = false;
  bool isSoaVector = false;
  bool isArgsPackTarget = false;
  bool isMapTarget = false;
  bool isWrappedMapTarget = false;
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

} // namespace primec::ir_lowerer
