#include "IrLowererCallHelpers.h"

#include "IrLowererCountAccessHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveNativeTailCallPathWithoutFallbackProbes(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

bool resolvePublishedNativeTailHelperName(const SemanticProgram *semanticProgram,
                                          const Expr &expr,
                                          StdlibSurfaceId surfaceId,
                                          std::string &helperNameOut) {
  return resolvePublishedSemanticStdlibSurfaceMemberName(
             semanticProgram, expr, surfaceId, helperNameOut) ||
         resolvePublishedStdlibSurfaceMemberName(
             resolveNativeTailCallPathWithoutFallbackProbes(expr),
             surfaceId,
             helperNameOut);
}

bool isExplicitDirectMapCountContainsTryAtCall(const SemanticProgram *semanticProgram,
                                               const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string helperName;
  return resolvePublishedNativeTailHelperName(
             semanticProgram,
             expr,
             StdlibSurfaceId::CollectionsMapHelpers,
             helperName) &&
         (helperName == "count" || helperName == "contains" || helperName == "tryAt");
}

bool isExplicitDirectSoaAccessCall(const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  const std::string rawPath = resolveNativeTailCallPathWithoutFallbackProbes(expr);
  return rawPath == "/soa_vector/get" ||
         rawPath == "/std/collections/soa_vector/get" ||
         rawPath == "/soa_vector/get_ref" ||
         rawPath == "/std/collections/soa_vector/get_ref";
}

} // namespace

bool isMapContainsHelperName(const Expr &expr);
bool isMapTryAtHelperName(const Expr &expr);
bool isVectorTarget(const Expr &expr, const LocalMap &localsIn);
MapAccessLookupEmitResult tryEmitMapContainsLookup(
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitMapLookupTryAt(
    LocalInfo::ValueKind mapKeyKind,
    const std::string &mapStructTypeName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error) {
  static const LocalMap emptyLocals;
  return emitUnsupportedNativeCallDiagnostic(expr, emptyLocals, tryGetPrintBuiltinName, error);
}

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error) {
  if (!expr.isMethodCall && isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
      isVectorTarget(expr.args.front(), localsIn)) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall && (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count"))) {
    std::string targetName = "<none>";
    if (!expr.args.empty()) {
      if (!expr.args.front().name.empty()) {
        targetName = expr.args.front().name;
      } else {
        targetName = "<expr>";
      }
    }
    error = "count requires array, vector, map, or string target (target=" + targetName + ")";
    return UnsupportedNativeCallResult::Error;
  }
  if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
      isVectorTarget(expr.args.front(), localsIn)) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity")) {
    error = "capacity requires vector target";
    return UnsupportedNativeCallResult::Error;
  }

  std::string printBuiltinName;
  if (tryGetPrintBuiltinName(expr, printBuiltinName)) {
    error = printBuiltinName + " is only supported as a statement in the native backend";
    return UnsupportedNativeCallResult::Error;
  }

  return UnsupportedNativeCallResult::NotHandled;
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  std::string mathName;
  if (tryGetMathBuiltinName(expr, mathName) && !isSupportedMathBuiltinName(mathName)) {
    error = "native backend does not support math builtin: " + mathName;
    return NativeCallTailDispatchResult::Error;
  }
  const auto countAccessResult = tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        if (resolveMapAccessTargetInfo(targetExpr, targetLocals, resolveCallMapAccessTargetInfo).isMapTarget) {
          return true;
        }
        return resolveArrayVectorAccessTargetInfo(
                   targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo)
            .isArrayOrVectorTarget;
      },
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        const auto targetInfo = resolveArrayVectorAccessTargetInfo(
            targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo);
        return targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget;
      },
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        const auto targetInfo = resolveArrayVectorAccessTargetInfo(
            targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo);
        return targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget;
      },
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
  if (countAccessResult == CountAccessCallEmitResult::Emitted) {
    return NativeCallTailDispatchResult::Emitted;
  }
  if (countAccessResult == CountAccessCallEmitResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  if (expr.isMethodCall && expr.name == "contains") {
    if (expr.args.size() != 2) {
      error = "contains requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto containsResult = tryEmitMapContainsLookup(
        expr.args.front(),
        expr.args[1],
        localsIn,
        allocTempLocal,
        emitExpr,
        resolveStringTableTarget,
        resolveCallMapAccessTargetInfo,
        inferExprKind,
        instructionCount,
        emitInstruction,
        patchInstructionImm,
        error);
    if (containsResult == MapAccessLookupEmitResult::Emitted) {
      return NativeCallTailDispatchResult::Emitted;
    }
    if (containsResult == MapAccessLookupEmitResult::Error) {
      return NativeCallTailDispatchResult::Error;
    }
    error = "contains requires map target";
    return NativeCallTailDispatchResult::Error;
  }

  if (!expr.isMethodCall && isMapContainsHelperName(expr)) {
    if (isExplicitDirectMapCountContainsTryAtCall(semanticProgram, expr)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (expr.args.size() != 2) {
      error = "contains requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto containsResult = tryEmitMapContainsLookup(
        expr.args.front(),
        expr.args[1],
        localsIn,
        allocTempLocal,
        emitExpr,
        resolveStringTableTarget,
        resolveCallMapAccessTargetInfo,
        inferExprKind,
        instructionCount,
        emitInstruction,
        patchInstructionImm,
        error);
    if (containsResult == MapAccessLookupEmitResult::Emitted) {
      return NativeCallTailDispatchResult::Emitted;
    }
    if (containsResult == MapAccessLookupEmitResult::Error) {
      return NativeCallTailDispatchResult::Error;
    }
    error = "contains requires map target";
    return NativeCallTailDispatchResult::Error;
  }

  if (expr.isMethodCall && expr.name == "tryAt") {
    if (expr.args.size() != 2) {
      error = "tryAt requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (!mapTargetInfo.isMapTarget) {
      error = "tryAt requires map target";
      return NativeCallTailDispatchResult::Error;
    }
    if (!validateMapAccessTargetInfo(mapTargetInfo, "tryAt", error)) {
      return NativeCallTailDispatchResult::Error;
    }
    if (!emitMapLookupTryAt(
            mapTargetInfo.mapKeyKind,
            mapTargetInfo.structTypeName,
            expr.args.front(),
            expr.args[1],
            localsIn,
            allocTempLocal,
            emitExpr,
            resolveStringTableTarget,
            inferExprKind,
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  if (!expr.isMethodCall && isMapTryAtHelperName(expr)) {
    if (isExplicitDirectMapCountContainsTryAtCall(semanticProgram, expr)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (expr.args.size() != 2) {
      error = "tryAt requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (!mapTargetInfo.isMapTarget) {
      error = "tryAt requires map target";
      return NativeCallTailDispatchResult::Error;
    }
    if (!validateMapAccessTargetInfo(mapTargetInfo, "tryAt", error)) {
      return NativeCallTailDispatchResult::Error;
    }
    if (!emitMapLookupTryAt(
            mapTargetInfo.mapKeyKind,
            mapTargetInfo.structTypeName,
            expr.args.front(),
            expr.args[1],
            localsIn,
            allocTempLocal,
            emitExpr,
            resolveStringTableTarget,
            inferExprKind,
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnostic(
      expr, localsIn, tryGetPrintBuiltinName, error);
  if (unsupportedCallResult == UnsupportedNativeCallResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  std::string accessName;
  if (isExplicitDirectMapCountContainsTryAtCall(semanticProgram, expr) &&
      !expr.args.empty() &&
      resolveMapAccessTargetInfo(expr.args.front(), localsIn, resolveCallMapAccessTargetInfo).isMapTarget) {
    return NativeCallTailDispatchResult::NotHandled;
  }
  if (getBuiltinArrayAccessName(expr, accessName)) {
    const auto arrayVectorTargetInfo = !expr.args.empty()
                                           ? resolveArrayVectorAccessTargetInfo(
                                                 expr.args.front(), localsIn, resolveCallArrayVectorAccessTargetInfo)
                                           : ArrayVectorAccessTargetInfo{};
    const std::string directHelperPath = resolveNativeTailCallPathWithoutFallbackProbes(expr);
    std::string explicitHelperName;
    const bool isExplicitVectorAccessCall =
        !expr.isMethodCall &&
        resolvePublishedNativeTailHelperName(
            semanticProgram,
            expr,
            StdlibSurfaceId::CollectionsVectorHelpers,
            explicitHelperName) &&
        (isCanonicalPublishedStdlibSurfaceHelperPath(
             directHelperPath,
             StdlibSurfaceId::CollectionsVectorHelpers) ||
         findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr) ==
             StdlibSurfaceId::CollectionsVectorHelpers) &&
        (explicitHelperName == "at" || explicitHelperName == "at_unsafe");
    if (isExplicitVectorAccessCall && arrayVectorTargetInfo.isVectorTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if ((isExplicitDirectSoaAccessCall(expr) ||
         (expr.isMethodCall &&
          (accessName == "get" || accessName == "get_ref"))) &&
        arrayVectorTargetInfo.isSoaVector) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const bool isExplicitMapAccessCall =
        !expr.isMethodCall &&
        resolvePublishedNativeTailHelperName(
            semanticProgram,
            expr,
            StdlibSurfaceId::CollectionsMapHelpers,
            explicitHelperName) &&
        (explicitHelperName == "at" || explicitHelperName == "at_ref" ||
         explicitHelperName == "at_unsafe" ||
         explicitHelperName == "at_unsafe_ref");
    if (isExplicitMapAccessCall && !expr.args.empty() &&
        resolveMapAccessTargetInfo(expr.args.front(), localsIn, resolveCallMapAccessTargetInfo).isMapTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (expr.args.size() != 2) {
      error = accessName + " requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    if (!emitBuiltinArrayAccess(accessName,
                                expr.args[0],
                                expr.args[1],
                                localsIn,
                                resolveStringTableTarget,
                                stringTableCount,
                                resolveCallMapAccessTargetInfo,
                                resolveCallArrayVectorAccessTargetInfo,
                                inferExprKind,
                                isEntryArgsName,
                                allocTempLocal,
                                emitExpr,
                                emitStringIndexOutOfBounds,
                                emitMapKeyNotFound,
                                emitArrayIndexOutOfBounds,
                                instructionCount,
                                emitInstruction,
                                patchInstructionImm,
                                error)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  return NativeCallTailDispatchResult::NotHandled;
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      {},
      {},
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      {},
      {},
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

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
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

} // namespace primec::ir_lowerer
