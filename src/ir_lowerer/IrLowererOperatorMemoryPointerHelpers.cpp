#include "IrLowererOperatorConversionsAndCallsInternal.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {
namespace {

std::string inferPointerStructTypePath(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return "";
    }
    if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.structTypeName;
    }
    return "";
  }
  if (expr.kind != Expr::Kind::Call) {
    return "";
  }

  if (expr.isFieldAccess && expr.args.size() == 1) {
    auto inferArgsPackReceiverStruct = [&](const Expr &candidate) {
      std::string accessName;
      const Expr *receiver = nullptr;
      if (getBuiltinArrayAccessName(candidate, accessName) &&
          candidate.args.size() == 2 &&
          (accessName == "at" || accessName == "at_unsafe")) {
        receiver = &candidate.args.front();
      }
      if (receiver == nullptr || receiver->kind != Expr::Kind::Name) {
        return std::string{};
      }
      auto it = localsIn.find(receiver->name);
      if (it == localsIn.end() || !it->second.isArgsPack) {
        return std::string{};
      }
      return it->second.structTypeName;
    };

    std::string receiverStruct;
    if (inferStructExprPath) {
      receiverStruct = inferStructExprPath(expr.args.front(), localsIn);
    }
    if (receiverStruct.empty()) {
      receiverStruct = inferArgsPackReceiverStruct(expr.args.front());
    }
    if (receiverStruct.empty()) {
      return "";
    }

    int32_t slotOffset = 0;
    int32_t slotCount = 0;
    std::string fieldStructPath;
    if (resolveStructFieldInfo &&
        resolveStructFieldInfo(receiverStruct, expr.name, slotOffset, slotCount, fieldStructPath) &&
        !fieldStructPath.empty()) {
      return fieldStructPath;
    }
    return "";
  }

  std::string memoryBuiltin;
  if (getBuiltinMemoryName(expr, memoryBuiltin)) {
    if (memoryBuiltin == "alloc" && expr.templateArgs.size() == 1) {
      std::string resolvedStruct;
      const std::string targetType = unwrapTopLevelUninitializedTypeText(expr.templateArgs.front());
      if (resolveStructTypeName(targetType, expr.namespacePrefix, resolvedStruct)) {
        return resolvedStruct;
      }
      return "";
    }
    if (memoryBuiltin == "realloc" && expr.args.size() == 2) {
      return inferPointerStructTypePath(
          expr.args.front(), localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
    }
    if (memoryBuiltin == "at" && expr.args.size() == 3) {
      return inferPointerStructTypePath(
          expr.args.front(), localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
    }
    if (memoryBuiltin == "at_unsafe" && expr.args.size() == 2) {
      return inferPointerStructTypePath(
          expr.args.front(), localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
    }
    if (memoryBuiltin == "reinterpret" && expr.args.size() == 1) {
      return "";
    }
  }

  std::string builtinName;
  if (getBuiltinOperatorName(expr, builtinName) &&
      (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return inferPointerStructTypePath(
        expr.args.front(), localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
  }

  if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
    return inferPointerStructTypePath(
        expr.args.front(), localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
  }

  return "";
}

bool isAggregatePointerLikeLocal(const LocalInfo &info, bool fromArgsPack) {
  const LocalInfo::Kind kind = fromArgsPack ? info.argsPackElementKind : info.kind;
  if (kind != LocalInfo::Kind::Pointer && kind != LocalInfo::Kind::Reference) {
    return false;
  }
  if (info.isResult || info.isFileHandle || info.isFileError) {
    return false;
  }
  return !info.structTypeName.empty() || info.referenceToArray || info.pointerToArray || info.referenceToVector ||
         info.pointerToVector || info.referenceToMap || info.pointerToMap || info.referenceToBuffer ||
         info.pointerToBuffer;
}

bool isAggregatePointerLikeExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && isAggregatePointerLikeLocal(it->second, false);
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && isAggregatePointerLikeLocal(it->second, true);
  }

  std::string builtinPointer;
  if (getBuiltinPointerName(expr, builtinPointer) && builtinPointer == "location" && expr.args.size() == 1) {
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           (!it->second.structTypeName.empty() || it->second.kind == LocalInfo::Kind::Array ||
            it->second.kind == LocalInfo::Kind::Vector || it->second.kind == LocalInfo::Kind::Map ||
            it->second.kind == LocalInfo::Kind::Buffer);
  }

  return false;
}

bool isAggregatePointerLikeReturnType(
    const std::string &returnType,
    const std::string &namespacePrefix,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName) {
  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(trimTemplateTypeText(returnType), base, argList)) {
    return false;
  }
  base = normalizeCollectionBindingTypeName(base);
  if (base != "Reference" && base != "Pointer") {
    return false;
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argList, args) || args.size() != 1) {
    return false;
  }

  const std::string targetType = trimTemplateTypeText(args.front());
  std::string targetBase;
  std::string targetArgList;
  if (splitTemplateTypeName(targetType, targetBase, targetArgList)) {
    targetBase = normalizeCollectionBindingTypeName(targetBase);
    if (targetBase == "array" || targetBase == "vector" ||
        targetBase == "map" || targetBase == "soa_vector") {
      return true;
    }
  }

  std::string resolvedStruct;
  return resolveStructTypeName(targetType, namespacePrefix, resolvedStruct);
}

bool isReferenceReturnType(const std::string &returnType) {
  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(trimTemplateTypeText(returnType), base, argList)) {
    return false;
  }
  return normalizeCollectionBindingTypeName(base) == "Reference";
}

std::string resolveSemanticReturnFactBindingType(
    const SemanticProgram *semanticProgram,
    const SemanticProgramReturnFact &returnFact) {
  if (semanticProgram != nullptr && returnFact.bindingTypeTextId != InvalidSymbolId) {
    std::string resolvedTypeText = std::string(
        semanticProgramResolveCallTargetString(*semanticProgram,
                                               returnFact.bindingTypeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(returnFact.bindingTypeText);
}

bool resolveAggregatePointerLikeCallExpr(
    const Expr &expr,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ResolveConversionsAndCallsStructTypeNameFn &resolveStructTypeName,
    const SemanticProductTargetAdapter *semanticProductTargets,
    bool &aggregatePointerLikeOut,
    std::string &error) {
  aggregatePointerLikeOut = false;
  if (expr.kind != Expr::Kind::Call || !resolveDefinitionCall) {
    return true;
  }

  const Definition *callee = resolveDefinitionCall(expr);
  if (callee == nullptr) {
    return true;
  }

  if (semanticProductTargets != nullptr &&
      semanticProductTargets->hasSemanticProduct &&
      expr.semanticNodeId != 0) {
    const SemanticProgramReturnFact *returnFact =
        findSemanticProductReturnFactByPath(*semanticProductTargets, callee->fullPath);
    std::string returnBindingType;
    if (returnFact != nullptr) {
      returnBindingType = resolveSemanticReturnFactBindingType(
          semanticProductTargets->semanticProgram, *returnFact);
    }
    if (returnFact == nullptr || returnBindingType.empty()) {
      error = "missing semantic-product aggregate pointer return metadata: " +
              callee->fullPath;
      return false;
    }
    aggregatePointerLikeOut = isAggregatePointerLikeReturnType(
        returnBindingType, callee->namespacePrefix, resolveStructTypeName);
    return true;
  }

  for (const auto &transform : callee->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }

    if (isAggregatePointerLikeReturnType(
            transform.templateArgs.front(), callee->namespacePrefix, resolveStructTypeName)) {
      aggregatePointerLikeOut = true;
      return true;
    }
  }

  return true;
}

bool resolveReferenceReturnCallExpr(
    const Expr &expr,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets,
    bool &referenceReturnOut,
    std::string &error) {
  referenceReturnOut = false;
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !resolveDefinitionCall) {
    return true;
  }

  const Definition *callee = resolveDefinitionCall(expr);
  if (callee == nullptr) {
    return true;
  }

  if (semanticProductTargets != nullptr &&
      semanticProductTargets->hasSemanticProduct &&
      expr.semanticNodeId != 0) {
    const SemanticProgramReturnFact *returnFact =
        findSemanticProductReturnFactByPath(*semanticProductTargets, callee->fullPath);
    std::string returnBindingType;
    if (returnFact != nullptr) {
      returnBindingType = resolveSemanticReturnFactBindingType(
          semanticProductTargets->semanticProgram, *returnFact);
    }
    if (returnFact == nullptr || returnBindingType.empty()) {
      error = "missing semantic-product location reference return metadata: " +
              callee->fullPath;
      return false;
    }
    referenceReturnOut = isReferenceReturnType(returnBindingType);
    return true;
  }

  for (const auto &transform : callee->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (isReferenceReturnType(transform.templateArgs.front())) {
      referenceReturnOut = true;
      return true;
    }
  }

  return true;
}

} // namespace

bool emitConversionsAndCallsMemoryAndPointerExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled) {
  const auto &localsIn = context.localsIn;
  const auto &emitExpr = context.emitExpr;
  const auto &inferExprKind = context.inferExprKind;
  const auto &allocTempLocal = context.allocTempLocal;
  const auto &emitPointerIndexOutOfBounds = context.emitPointerIndexOutOfBounds;
  const auto &valueKindFromTypeName = context.valueKindFromTypeName;
  const auto &inferStructExprPath = context.inferStructExprPath;
  const auto &resolveStructTypeName = context.resolveStructTypeName;
  const auto &resolveStructSlotCount = context.resolveStructSlotCount;
  const auto &resolveStructFieldInfo = context.resolveStructFieldInfo;
  const auto &resolveStructFieldBinding = context.resolveStructFieldBinding;
  auto &instructions = context.instructions;
  auto &error = context.error;
  const auto &resolveDefinitionCall = context.resolveDefinitionCall;
  const SemanticProductTargetAdapter *semanticProductTargets =
      context.semanticProductTargets;

  handled = true;
  std::string builtin;
  if (getBuiltinMemoryName(expr, builtin)) {
    auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
      instructions.push_back(
          {(kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64,
           static_cast<uint64_t>(value)});
    };
    if (builtin == "free") {
      if (!expr.templateArgs.empty()) {
        error = "free does not take template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error = "free requires exactly one argument";
        return false;
      }
      if (!emitExpr(expr.args.front(), localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::HeapFree, 0});
      return true;
    }
    if (builtin == "realloc") {
      if (!expr.templateArgs.empty()) {
        error = "realloc does not take template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error = "realloc requires exactly two arguments";
        return false;
      }
      const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
      if (!isSupportedIndexKind(countKind)) {
        error = "realloc requires integer count argument";
        return false;
      }
      if (!emitExpr(expr.args[0], localsIn)) {
        return false;
      }
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }

      int32_t slotCountMultiplier = 1;
      const std::string structTypeName = inferPointerStructTypePath(
          expr.args[0], localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
      if (!structTypeName.empty()) {
        if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
          return false;
        }
      }

      if (slotCountMultiplier != 1) {
        if (countKind == LocalInfo::ValueKind::Int32) {
          instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(slotCountMultiplier)});
          instructions.push_back({IrOpcode::MulI32, 0});
        } else {
          instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotCountMultiplier)});
          instructions.push_back({IrOpcode::MulI64, 0});
        }
      }
      instructions.push_back({IrOpcode::HeapRealloc, 0});
      return true;
    }
    if (builtin == "at") {
      if (!expr.templateArgs.empty()) {
        error = "at does not take template arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error = "at requires exactly three arguments";
        return false;
      }
      const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "at requires integer index argument";
        return false;
      }
      const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args[2], localsIn));
      if (!isSupportedIndexKind(countKind)) {
        error = "at requires integer element count argument";
        return false;
      }
      if (countKind != indexKind) {
        error = "at requires matching integer index and element count kinds";
        return false;
      }

      const int32_t ptrLocal = allocTempLocal();
      if (!emitExpr(expr.args[0], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

      const int32_t indexLocal = allocTempLocal();
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

      const int32_t countLocal = allocTempLocal();
      if (!emitExpr(expr.args[2], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

      if (indexKind != LocalInfo::ValueKind::UInt64) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        instructions.push_back({pushZeroForIndex(indexKind), 0});
        instructions.push_back({cmpLtForIndex(indexKind), 0});
        const size_t jumpIndexNonNegative = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitPointerIndexOutOfBounds();
        instructions[jumpIndexNonNegative].imm = static_cast<int32_t>(instructions.size());
      }

      if (countKind != LocalInfo::ValueKind::UInt64) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        instructions.push_back({pushZeroForIndex(countKind), 0});
        instructions.push_back({cmpLtForIndex(countKind), 0});
        const size_t jumpCountNonNegative = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitPointerIndexOutOfBounds();
        instructions[jumpCountNonNegative].imm = static_cast<int32_t>(instructions.size());
      }

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
      instructions.push_back({cmpGeForIndex(indexKind), 0});
      const size_t jumpIndexInRange = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      emitPointerIndexOutOfBounds();
      instructions[jumpIndexInRange].imm = static_cast<int32_t>(instructions.size());

      int32_t slotCountMultiplier = 1;
      const std::string structTypeName = inferPointerStructTypePath(
          expr.args[0], localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
      if (!structTypeName.empty()) {
        if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
          return false;
        }
      }

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      pushIndexConst(indexKind, slotCountMultiplier * IrSlotBytesI32);
      instructions.push_back({mulForIndex(indexKind), 0});
      instructions.push_back({IrOpcode::AddI64, 0});
      return true;
    }
    if (builtin == "at_unsafe") {
      if (!expr.templateArgs.empty()) {
        error = "at_unsafe does not take template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error = "at_unsafe requires exactly two arguments";
        return false;
      }
      const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "at_unsafe requires integer index argument";
        return false;
      }

      if (!emitExpr(expr.args[0], localsIn)) {
        return false;
      }
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }

      int32_t slotCountMultiplier = 1;
      const std::string structTypeName = inferPointerStructTypePath(
          expr.args[0], localsIn, inferStructExprPath, resolveStructTypeName, resolveStructFieldInfo);
      if (!structTypeName.empty()) {
        if (!resolveStructSlotCount(structTypeName, slotCountMultiplier)) {
          return false;
        }
      }

      pushIndexConst(indexKind, slotCountMultiplier * IrSlotBytesI32);
      instructions.push_back({mulForIndex(indexKind), 0});
      instructions.push_back({IrOpcode::AddI64, 0});
      return true;
    }
    if (builtin == "reinterpret") {
      if (expr.templateArgs.size() != 1) {
        error = "reinterpret requires exactly one template argument";
        return false;
      }
      if (expr.args.size() != 1) {
        error = "reinterpret requires exactly one argument";
        return false;
      }
      if (!emitExpr(expr.args.front(), localsIn)) {
        return false;
      }
      return true;
    }
    if (builtin != "alloc") {
      error = "native backend does not support memory intrinsic: " + builtin;
      return false;
    }
    if (expr.templateArgs.size() != 1) {
      error = "alloc requires exactly one template argument";
      return false;
    }
    if (expr.args.size() != 1) {
      error = "alloc requires exactly one argument";
      return false;
    }
    const LocalInfo::ValueKind countKind = normalizeIndexKind(inferExprKind(expr.args.front(), localsIn));
    if (!isSupportedIndexKind(countKind)) {
      error = "alloc requires integer count argument";
      return false;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return false;
    }

    int32_t slotCountMultiplier = 1;
    const std::string targetTypeName = unwrapTopLevelUninitializedTypeText(expr.templateArgs.front());
    LocalInfo::ValueKind targetKind = valueKindFromTypeName(targetTypeName);
    if (targetKind == LocalInfo::ValueKind::Unknown) {
      std::string resolvedStructType;
      if (!resolveStructTypeName(targetTypeName, expr.namespacePrefix, resolvedStructType)) {
        error = "native backend does not support alloc target type: " + targetTypeName;
        return false;
      }
      if (!resolveStructSlotCount(resolvedStructType, slotCountMultiplier)) {
        return false;
      }
    }

    if (slotCountMultiplier != 1) {
      if (countKind == LocalInfo::ValueKind::Int32) {
        instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(slotCountMultiplier)});
        instructions.push_back({IrOpcode::MulI32, 0});
      } else {
        instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotCountMultiplier)});
        instructions.push_back({IrOpcode::MulI64, 0});
      }
    }
    instructions.push_back({IrOpcode::HeapAlloc, 0});
    return true;
  }

  if (getBuiltinPointerName(expr, builtin)) {
    if (expr.args.size() != 1) {
      error = builtin + " requires exactly one argument";
      return false;
    }
    if (builtin == "location") {
      const Expr &target = expr.args.front();
      auto isBorrowedArgsPackAccessTarget = [&](const Expr &candidate) {
        const Expr *receiver = nullptr;
        std::string accessName;
        if (getBuiltinArrayAccessName(candidate, accessName) &&
            candidate.args.size() == 2 &&
            (accessName == "at" || accessName == "at_unsafe")) {
          receiver = &candidate.args.front();
        }
        if (receiver == nullptr || receiver->kind != Expr::Kind::Name) {
          return false;
        }
        auto it = localsIn.find(receiver->name);
        return it != localsIn.end() &&
               it->second.isArgsPack &&
               it->second.argsPackElementKind == LocalInfo::Kind::Reference;
      };
      if (target.kind == Expr::Kind::Name) {
        auto it = localsIn.find(target.name);
        if (it == localsIn.end()) {
          error = "location requires a local binding";
          return false;
        }
        if (it->second.kind == LocalInfo::Kind::Reference || !it->second.structTypeName.empty() ||
            it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
            it->second.isSoaVector ||
            it->second.kind == LocalInfo::Kind::Map || it->second.kind == LocalInfo::Kind::Buffer) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        } else {
          instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
        }
        return true;
      }
      if (target.kind == Expr::Kind::Call && isBorrowedArgsPackAccessTarget(target)) {
        return emitExpr(target, localsIn);
      }
      if (target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1) {
        const Expr &receiver = target.args.front();
        auto inferArgsPackReceiverStruct = [&](const Expr &candidate) {
          std::string accessName;
          const Expr *accessReceiver = nullptr;
          if (getBuiltinArrayAccessName(candidate, accessName) &&
              candidate.args.size() == 2 &&
              (accessName == "at" || accessName == "at_unsafe")) {
            accessReceiver = &candidate.args.front();
          }
          if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
            return std::string{};
          }
          auto it = localsIn.find(accessReceiver->name);
          if (it == localsIn.end() || !it->second.isArgsPack) {
            return std::string{};
          }
          return it->second.structTypeName;
        };
        std::string receiverStruct = inferStructExprPath(receiver, localsIn);
        if (receiverStruct.empty()) {
          receiverStruct = inferArgsPackReceiverStruct(receiver);
        }
        if (receiverStruct.empty()) {
          error = "field access requires struct receiver";
          return false;
        }
        LayoutFieldBinding fieldBinding;
        if (resolveStructFieldBinding &&
            resolveStructFieldBinding(receiverStruct, target.name, fieldBinding) &&
            normalizeCollectionBindingTypeName(fieldBinding.typeName) == "Reference") {
          int32_t slotOffset = 0;
          int32_t slotCount = 0;
          std::string fieldStructPath;
          if (!resolveStructFieldInfo ||
              !resolveStructFieldInfo(receiverStruct, target.name, slotOffset, slotCount, fieldStructPath)) {
            error = "unknown struct field: " + target.name;
            return false;
          }
          if (!emitExpr(receiver, localsIn)) {
            return false;
          }
          const int32_t ptrLocal = allocTempLocal();
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          const uint64_t offsetBytes = static_cast<uint64_t>(slotOffset) * IrSlotBytes;
          if (offsetBytes != 0) {
            instructions.push_back({IrOpcode::PushI64, offsetBytes});
            instructions.push_back({IrOpcode::AddI64, 0});
          }
          instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        int32_t slotOffset = 0;
        int32_t slotCount = 0;
        std::string fieldStructPath;
        if (!resolveStructFieldInfo ||
            !resolveStructFieldInfo(receiverStruct, target.name, slotOffset, slotCount, fieldStructPath)) {
          error = "unknown struct field: " + target.name;
          return false;
        }
        if (!emitExpr(receiver, localsIn)) {
          return false;
        }
        const int32_t ptrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        const uint64_t offsetBytes = static_cast<uint64_t>(slotOffset) * IrSlotBytes;
        if (offsetBytes != 0) {
          instructions.push_back({IrOpcode::PushI64, offsetBytes});
          instructions.push_back({IrOpcode::AddI64, 0});
        }
        return true;
      }
      if (target.kind == Expr::Kind::Call && !target.isMethodCall && resolveDefinitionCall != nullptr) {
        bool referenceReturn = false;
        if (!resolveReferenceReturnCallExpr(
                target, resolveDefinitionCall, semanticProductTargets, referenceReturn, error)) {
          return false;
        }
        if (referenceReturn) {
          return emitExpr(target, localsIn);
        }
      }
      error = "location requires a local binding";
      return false;
    }

    const Expr &pointerExpr = expr.args.front();
    bool aggregatePointerLike = isAggregatePointerLikeExpr(pointerExpr, localsIn);
    if (builtin == "dereference" && !aggregatePointerLike) {
      if (!resolveAggregatePointerLikeCallExpr(pointerExpr,
                                              resolveDefinitionCall,
                                              resolveStructTypeName,
                                              semanticProductTargets,
                                              aggregatePointerLike,
                                              error)) {
        return false;
      }
    }
    if (pointerExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(pointerExpr.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + pointerExpr.name;
        return false;
      }
      if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
        error = "dereference requires a pointer or reference";
        return false;
      }
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
    } else {
      if (!emitExpr(pointerExpr, localsIn)) {
        return false;
      }
    }
    if (builtin == "dereference" && aggregatePointerLike) {
      return true;
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    return true;
  }

  handled = false;
  return true;
}

} // namespace primec::ir_lowerer
