#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"

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

enum class SemanticStringAccessTargetKind {
  Unknown,
  String,
  NonString,
};

struct CollectionPairTypeInfo {
  bool isKeyValueTarget = false;
  LocalInfo::ValueKind keyValueKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind keyValueValueKind = LocalInfo::ValueKind::Unknown;
  bool isWrappedKeyValueTarget = false;
  std::string structTypeName;
};

struct ArrayVectorAccessTargetInfo {
  bool isArrayOrVectorTarget = false;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  bool isVectorTarget = false;
  bool isSoaVector = false;
  bool isArgsPackTarget = false;
  bool isKeyValueTarget = false;
  bool isWrappedKeyValueTarget = false;
  LocalInfo::Kind argsPackElementKind = LocalInfo::Kind::Value;
  int32_t elemSlotCount = 0;
  std::string structTypeName;
  // Set only when the receiver local is a record-boxed struct value (a
  // `Kind::Value` local whose `structTypeName` is the canonical Vector
  // backing-record path), as opposed to a raw primitive `Kind::Vector`
  // local. A record-boxed receiver's `at`/`at_unsafe` method call must
  // never be lowered as the primitive builtin array-access pattern (that
  // pattern assumes a raw vector pointer/index local, not a struct value)
  // - see TODO-4628's resolution note on the wrong-element-value bug this
  // exact mix-up produced previously.
  bool isStructBoxedRecordTarget = false;
};

using ResolveCallCollectionPairTypeInfoFn = std::function<bool(const Expr &, CollectionPairTypeInfo &)>;
using ResolveCallArrayVectorAccessTargetInfoFn =
    std::function<bool(const Expr &, ArrayVectorAccessTargetInfo &)>;

struct KeyValueLookupLoopLocals {
  int32_t countLocal = -1;
  int32_t indexLocal = -1;
};

struct KeyValueLookupLoopConditionAnchors {
  size_t loopStart = 0;
  size_t jumpLoopEnd = 0;
};

struct KeyValueLookupLoopMatchAnchors {
  size_t jumpNotMatch = 0;
  size_t jumpFound = 0;
};

enum class KeyValueLookupStringKeyResult {
  NotHandled,
  Resolved,
  Error,
};

enum class KeyValueLookupKeyLocalEmitResult {
  NotHandled,
  Emitted,
  Error,
};

} // namespace primec::ir_lowerer
