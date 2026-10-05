#include "IrLowererOperatorConversionsAndCallsInternal.h"

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererVectorRecordLayoutHelpers.h"
#include "primec/ir/SoaPathHelpers.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererOperatorCollectionMutationHelpersFileLocal.h"

namespace primec::ir_lowerer {
using namespace ir_lowerer_operator_collection_mutation_helpers_file_local;

bool emitConversionsAndCallsCollectionAndMutationExpr(
    const Expr &expr,
    ConversionsAndCallsOperatorContext &context,
    bool &handled) {
  const auto &localsIn = context.localsIn;
  auto &nextLocal = context.nextLocal;
  const auto &emitExpr = context.emitExpr;
  const auto &inferExprKind = context.inferExprKind;
  const auto &allocTempLocal = context.allocTempLocal;
  const auto &emitArrayIndexOutOfBounds = context.emitArrayIndexOutOfBounds;
  const auto &resolveStringTableTarget = context.resolveStringTableTarget;
  const auto &valueKindFromTypeName = context.valueKindFromTypeName;
  const auto &inferStructExprPath = context.inferStructExprPath;
  const auto &resolveStructSlotCount = context.resolveStructSlotCount;
  const auto &resolveStructFieldInfo = context.resolveStructFieldInfo;
  const auto &resolveStructFieldBinding = context.resolveStructFieldBinding;
  const auto &emitStructCopyFromPtrs = context.emitStructCopyFromPtrs;
  const auto *semanticProductTargets = context.semanticProductTargets;
  auto &instructions = context.instructions;
  auto &error = context.error;

  handled = true;
  std::string builtin;
  if (getBuiltinCollectionName(expr, builtin)) {
    if (builtin == "array" || builtin == "vector" || builtin == "soa") {
      const bool isSoaVector = (builtin == "soa");
      const bool allowEmptyTemplateArgs = isSoaVector && expr.args.empty() && expr.templateArgs.empty();
      if (expr.templateArgs.size() != 1 && !allowEmptyTemplateArgs) {
        error = builtin + " literal requires exactly one template argument";
        return false;
      }
      if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        error = builtin + " literal too large for native backend";
        return false;
      }

      const bool isVectorLike = (builtin == "vector" || isSoaVector);
      LocalInfo::ValueKind elemKind = expr.templateArgs.empty() ? LocalInfo::ValueKind::Unknown : valueKindFromTypeName(expr.templateArgs.front());
      const bool isSoaStructLiteral =
          isSoaVector && !expr.args.empty() && elemKind == LocalInfo::ValueKind::Unknown;
      std::string soaStructPath;
      int32_t soaStructSlotCount = 0;
      if (isSoaStructLiteral) {
        soaStructPath = inferStructExprPath(expr.args.front(), localsIn);
        if (!resolveStructSlotCount(soaStructPath, soaStructSlotCount)) {
          return false;
        }
      }
      const bool isEmptyOpaqueCollectionLiteral =
          builtin == "vector" && elemKind == LocalInfo::ValueKind::Unknown &&
          expr.args.empty();
      if (!isSoaVector && elemKind == LocalInfo::ValueKind::Unknown &&
          !isEmptyOpaqueCollectionLiteral) {
        error = "native backend only supports numeric/bool/string " + builtin + " literals";
        return false;
      }

      const int32_t literalCount = static_cast<int32_t>(expr.args.size());
      if (!isSoaVector && isVectorLike && literalCount > kVectorLocalDynamicCapacityLimit) {
        error = collectionLiteralExceedsLocalCapacityLimitMessage();
        return false;
      }
      const int32_t storageCapacity =
          isVectorLike ? std::max(literalCount, kVectorLocalDynamicCapacityLimit) : literalCount;
      const int32_t baseLocal = nextLocal;
      int32_t dataBaseLocal = baseLocal + 1;
      VectorRecordFieldSlots vectorSlots;
      if (isVectorLike) {
        std::string vectorStructPath = inferStructExprPath(expr, localsIn);
        if (vectorStructPath.empty()) {
          vectorStructPath = defaultVectorRecordStructPath(builtin);
        }
        auto resolveVectorField =
            [&](const std::string &structPath,
                const char *fieldName,
                int32_t &slotOffset,
                int32_t &slotCount) {
              std::string fieldStructPath;
              return resolveStructFieldInfo(
                  structPath, fieldName, slotOffset, slotCount, fieldStructPath);
            };
        if (!resolveVectorRecordFieldSlotsFromFields(
                vectorStructPath, resolveVectorField, vectorSlots)) {
          const bool isSpecializedStdlibVector =
              vectorStructPath.find("/Vector__t") != std::string::npos ||
              vectorStructPath.find("Vector__t") == 0;
          if (isSpecializedStdlibVector) {
            vectorSlots.count = 1;
            vectorSlots.capacity = 2;
            vectorSlots.data = 3;
            vectorSlots.ownsData = 4;
            vectorSlots.totalSlots = 5;
          }
        }
        if (vectorSlots.totalSlots <= 0) {
          error = "native backend cannot resolve vector record layout: " +
                  vectorStructPath;
          return false;
        }
        nextLocal += vectorSlots.totalSlots;
        dataBaseLocal = baseLocal + vectorSlots.data;
        int32_t heapAllocSlots = storageCapacity;
        if (isSoaStructLiteral) {
          heapAllocSlots *= soaStructSlotCount;
        }
        // Empty soa literals must allocate at least 1 slot to match soaColumnNew's
        // invariant (soaColumnReserve/soaColumnDestroy free data unconditionally).
        const bool isSoaEmptyLiteral = isSoaVector && literalCount == 0;
        if (isSoaEmptyLiteral) {
          heapAllocSlots = 1;
        }
        // The record owns its storage whenever it allocated some.
        const bool nullDataPointer = !isSoaEmptyLiteral && isEmptyOpaqueCollectionLiteral;
        emitVectorRecordHeader(instructions,
                               baseLocal,
                               vectorSlots,
                               literalCount,
                               literalCount,
                               heapAllocSlots,
                               nullDataPointer,
                               !nullDataPointer);
      } else {
        nextLocal += 1 + storageCapacity;
        instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(literalCount)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
      }

      for (size_t i = 0; i < expr.args.size(); ++i) {
        const Expr &arg = expr.args[i];
        if (isSoaStructLiteral) {
          const std::string argStructPath = inferStructExprPath(arg, localsIn);
          if (argStructPath.empty() || !areCompatibleStructPaths(argStructPath, soaStructPath)) {
            error = "collection literal element type mismatch";
            return false;
          }
          const int32_t destPtrLocal = allocTempLocal();
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataBaseLocal)});
          const uint64_t offsetBytes =
              static_cast<uint64_t>(i) * static_cast<uint64_t>(soaStructSlotCount) * IrSlotBytes;
          if (offsetBytes != 0) {
            instructions.push_back({IrOpcode::PushI64, offsetBytes});
            instructions.push_back({IrOpcode::AddI64, 0});
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          if (!emitExpr(arg, localsIn)) {
            return false;
          }
          const int32_t srcPtrLocal = allocTempLocal();
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, soaStructSlotCount)) {
            return false;
          }
          continue;
        }
        if (elemKind == LocalInfo::ValueKind::String) {
          const LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
          int32_t stringIndex = -1;
          size_t length = 0;
          if ((argKind == LocalInfo::ValueKind::Unknown ||
               argKind == LocalInfo::ValueKind::String) &&
              resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
            if (isVectorLike) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataBaseLocal)});
              const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
              if (offsetBytes != 0) {
                instructions.push_back({IrOpcode::PushI32, offsetBytes});
                instructions.push_back({IrOpcode::AddI32, 0});
              }
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
              instructions.push_back({IrOpcode::StoreIndirect, 0});
            } else {
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataBaseLocal + static_cast<int32_t>(i))});
            }
            continue;
          }
          if (argKind != LocalInfo::ValueKind::String) {
            error = "native backend requires " + builtin +
                    " literal string elements to be string literals or literal-backed bindings";
            return false;
          }
          error = "native backend requires " + builtin +
                  " literal string elements to be string literals or literal-backed bindings";
          return false;
        }
        LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
        if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
          error = "native backend requires " + builtin + " literal elements to be numeric/bool values";
          return false;
        }
        if (argKind != elemKind) {
          error = builtin + " literal element type mismatch";
          return false;
        }
        if (isVectorLike) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataBaseLocal)});
          const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
          if (offsetBytes != 0) {
            instructions.push_back({IrOpcode::PushI64, offsetBytes});
            instructions.push_back({IrOpcode::AddI64, 0});
          }
          if (!emitExpr(arg, localsIn)) {
            return false;
          }
          instructions.push_back({IrOpcode::StoreIndirect, 0});
          instructions.push_back({IrOpcode::Pop, 0});
        } else {
          if (!emitExpr(arg, localsIn)) {
            return false;
          }
          instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(dataBaseLocal + static_cast<int32_t>(i))});
        }
      }

      instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
      return true;
    }
    if (builtin == "map") {
      handled = false;
      return true;
    }
    error = "native backend does not support " + builtin + " literals";
    return false;
  }

  if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
    const bool isIncrement = isSimpleCallName(expr, "increment");
    const char *mutateName = isIncrement ? "increment" : "decrement";
    if (expr.args.size() != 1) {
      error = std::string(mutateName) + " requires exactly one argument";
      return false;
    }
    const Expr &target = expr.args.front();
    auto validateDeltaKind = [&](LocalInfo::ValueKind kind) -> bool {
      if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
          kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Float64 ||
          kind == LocalInfo::ValueKind::Float32) {
        return true;
      }
      error = std::string(mutateName) + " requires numeric operand";
      return false;
    };
    auto emitDelta = [&](LocalInfo::ValueKind kind) -> bool {
      if (!validateDeltaKind(kind)) {
        return false;
      }
      if (kind == LocalInfo::ValueKind::Int32) {
        instructions.push_back({IrOpcode::PushI32, 1});
        instructions.push_back({isIncrement ? IrOpcode::AddI32 : IrOpcode::SubI32, 0});
        instructions.push_back({IrOpcode::SextI32, 0}); // i32 wraps at 32 bits
        return true;
      }
      if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
        instructions.push_back({IrOpcode::PushI64, 1});
        instructions.push_back({isIncrement ? IrOpcode::AddI64 : IrOpcode::SubI64, 0});
        return true;
      }
      if (kind == LocalInfo::ValueKind::Float64) {
        uint64_t bits = 0;
        double one = 1.0;
        std::memcpy(&bits, &one, sizeof(bits));
        instructions.push_back({IrOpcode::PushF64, bits});
        instructions.push_back({isIncrement ? IrOpcode::AddF64 : IrOpcode::SubF64, 0});
        return true;
      }
      if (kind == LocalInfo::ValueKind::Float32) {
        float one = 1.0f;
        uint32_t bits = 0;
        std::memcpy(&bits, &one, sizeof(bits));
        instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
        instructions.push_back({isIncrement ? IrOpcode::AddF32 : IrOpcode::SubF32, 0});
        return true;
      }
      return false;
    };
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it == localsIn.end()) {
        error = std::string(mutateName) + " target must be a known binding: " + target.name;
        return false;
      }
      if (!it->second.isMutable) {
        error = std::string(mutateName) + " target must be mutable: " + target.name;
        return false;
      }
      LocalInfo::ValueKind targetValueKind = it->second.valueKind;
      LocalInfo::ValueKind semanticValueKind = LocalInfo::ValueKind::Unknown;
      if (inferSemanticMutationTargetValueKind(
              target, context.semanticProductTargets, valueKindFromTypeName, semanticValueKind)) {
        targetValueKind = semanticValueKind;
      }
      if (it->second.kind == LocalInfo::Kind::Reference) {
        if (!validateDeltaKind(targetValueKind)) {
          return false;
        }
        const int32_t ptrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::LoadIndirect, 0});
        if (!emitDelta(targetValueKind)) {
          return false;
        }
        const int32_t valueLocal = allocTempLocal();
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        return true;
      }
      if (it->second.kind != LocalInfo::Kind::Value) {
        error = std::string(mutateName) + " target must be a mutable binding";
        return false;
      }
      if (!validateDeltaKind(targetValueKind)) {
        return false;
      }
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
      if (!emitDelta(targetValueKind)) {
        return false;
      }
      instructions.push_back({IrOpcode::Dup, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
      return true;
    }
    if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
      if (target.args.size() != 1) {
        error = "dereference requires exactly one argument";
        return false;
      }
      LocalInfo::ValueKind valueKind = inferExprKind(target, localsIn);
      const Expr &pointerExpr = target.args.front();
      LocalInfo::ValueKind semanticValueKind = LocalInfo::ValueKind::Unknown;
      if (inferSemanticMutationTargetValueKind(
              pointerExpr, context.semanticProductTargets, valueKindFromTypeName, semanticValueKind)) {
        valueKind = semanticValueKind;
      }
      if (valueKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Bool ||
          valueKind == LocalInfo::ValueKind::String) {
        error = std::string(mutateName) + " requires numeric operand";
        return false;
      }
      const int32_t ptrLocal = allocTempLocal();
      if (pointerExpr.kind == Expr::Kind::Name) {
        auto it = localsIn.find(pointerExpr.name);
        if (it == localsIn.end() || !it->second.isMutable) {
          error = std::string(mutateName) + " target must be a mutable binding";
          return false;
        }
        if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
          error = std::string(mutateName) + " target must be a mutable pointer binding";
          return false;
        }
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
      } else {
        if (!emitExpr(pointerExpr, localsIn)) {
          return false;
        }
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
      instructions.push_back({IrOpcode::LoadIndirect, 0});
      if (!emitDelta(valueKind)) {
        return false;
      }
      const int32_t valueLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      return true;
    }
    error = std::string(mutateName) + " target must be a mutable binding";
    return false;
  }

  if (isSimpleCallName(expr, "assign")) {
    if (expr.args.size() != 2) {
      error = "assign requires exactly two arguments";
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it == localsIn.end()) {
        error = "assign target must be a known binding: " + target.name;
        return false;
      }
      if (!it->second.isMutable) {
        error = "assign target must be mutable: " + target.name;
        return false;
      }
      if (it->second.kind == LocalInfo::Kind::Reference) {
        if (!it->second.structTypeName.empty()) {
          const Expr &rhsExpr = expr.args[1];
          std::string rhsStruct = inferStructExprPath(rhsExpr, localsIn);
          if (rhsStruct.empty() ||
              !areCompatibleStructPaths(rhsStruct, it->second.structTypeName)) {
            error = "assign requires matching struct value";
            return false;
          }
          int32_t structSlotCount = 0;
          if (!resolveStructSlotCount(it->second.structTypeName, structSlotCount)) {
            return false;
          }
          const int32_t destPtrLocal = allocTempLocal();
          instructions.push_back(
              {IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          if (!emitExpr(rhsExpr, localsIn)) {
            return false;
          }
          const int32_t srcPtrLocal = allocTempLocal();
          instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, structSlotCount)) {
            return false;
          }
          instructions.push_back(
              {IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          return true;
        }
        const int32_t ptrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        const int32_t valueLocal = allocTempLocal();
        if (!emitExpr(expr.args[1], localsIn)) {
          return false;
        }
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        return true;
      }
      if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
        const Expr &rhsExpr = expr.args[1];
        std::string rhsStruct = inferStructExprPath(rhsExpr, localsIn);
        if (rhsStruct.empty() || !areCompatibleStructPaths(rhsStruct, it->second.structTypeName)) {
          error = "assign requires matching struct value";
          return false;
        }
        int32_t structSlotCount = 0;
        if (!resolveStructSlotCount(it->second.structTypeName, structSlotCount)) {
          return false;
        }
        const int32_t destPtrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
        if (!emitExpr(rhsExpr, localsIn)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, structSlotCount)) {
          return false;
        }
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        return true;
      }
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::Dup, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
      return true;
    }
    if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
      if (target.args.size() != 1) {
        error = "dereference requires exactly one argument";
        return false;
      }
      const Expr &pointerExpr = target.args.front();
      const int32_t ptrLocal = allocTempLocal();
      if (pointerExpr.kind == Expr::Kind::Name) {
        auto it = localsIn.find(pointerExpr.name);
        if (it == localsIn.end() || !it->second.isMutable) {
          error = "assign target must be a mutable binding";
          return false;
        }
        if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
          error = "assign target must be a mutable pointer binding";
          return false;
        }
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        if (!it->second.structTypeName.empty()) {
          // Struct pointee: copy every slot instead of storing the source
          // struct's address into the first slot.
          const Expr &rhsExpr = expr.args[1];
          const std::string rhsStruct = inferStructExprPath(rhsExpr, localsIn);
          if (rhsStruct.empty() ||
              !areCompatibleStructPaths(rhsStruct, it->second.structTypeName)) {
            error = "assign requires matching struct value";
            return false;
          }
          int32_t structSlotCount = 0;
          if (!resolveStructSlotCount(it->second.structTypeName, structSlotCount)) {
            return false;
          }
          const int32_t destPtrLocal = allocTempLocal();
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          if (!emitExpr(rhsExpr, localsIn)) {
            return false;
          }
          const int32_t srcPtrLocal = allocTempLocal();
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, structSlotCount)) {
            return false;
          }
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
          return true;
        }
      } else {
        if (!emitExpr(pointerExpr, localsIn)) {
          return false;
        }
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
      const int32_t valueLocal = allocTempLocal();
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      return true;
    }
    std::string accessName;
    const bool hasArrayVectorAssignmentAccess =
        resolveArrayVectorAssignmentAccessName(target, accessName);
    const bool hasSemanticBareArrayVectorAssignmentAccess =
        !hasArrayVectorAssignmentAccess &&
        resolveSemanticBareArrayVectorAssignmentAccessName(target, accessName);
    if ((hasArrayVectorAssignmentAccess ||
         hasSemanticBareArrayVectorAssignmentAccess) &&
        target.args.size() == 2) {
      const Expr &collectionTarget = target.args.front();
      const Expr &indexExpr = target.args[1];
      ArrayVectorAccessTargetInfo targetInfo;
      bool hasSemanticArrayVectorFact = false;
      if (!resolveSemanticArrayVectorTargetInfo(collectionTarget,
                                                semanticProductTargets,
                                                valueKindFromTypeName,
                                                targetInfo,
                                                hasSemanticArrayVectorFact)) {
        targetInfo = hasSemanticArrayVectorFact
                         ? ArrayVectorAccessTargetInfo{}
                         : resolveArrayVectorAccessTargetInfo(collectionTarget, localsIn);
      }
      if (hasSemanticBareArrayVectorAssignmentAccess &&
          (!hasSemanticArrayVectorFact ||
           !targetInfo.isArrayOrVectorTarget ||
           targetInfo.isKeyValueTarget)) {
        error = "native backend only supports assign to array/vector elements";
        return false;
      }
      if (!targetInfo.isArrayOrVectorTarget && !hasSemanticArrayVectorFact) {
        const std::string collectionPath = inferStructExprPath(collectionTarget, localsIn);
        if (collection_helpers::isCollectionFamilyRoot(collectionPath, collection_helpers::CollectionFamily::Array) || collection_helpers::isCollectionFamilyRoot(collectionPath, collection_helpers::CollectionFamily::Vector)) {
          targetInfo.isArrayOrVectorTarget = true;
          targetInfo.isVectorTarget = (collection_helpers::isCollectionFamilyRoot(collectionPath, collection_helpers::CollectionFamily::Vector));
        }
      }
      if (!targetInfo.isArrayOrVectorTarget && !hasSemanticArrayVectorFact &&
          collectionTarget.kind == Expr::Kind::Call &&
          collectionTarget.isFieldAccess && collectionTarget.args.size() == 1) {
        const std::string receiverStruct = inferStructExprPath(collectionTarget.args.front(), localsIn);
        LayoutFieldBinding fieldBinding;
        if (!receiverStruct.empty() && resolveStructFieldBinding &&
            resolveStructFieldBinding(receiverStruct, collectionTarget.name, fieldBinding)) {
          classifyArrayVectorFieldBinding(fieldBinding, valueKindFromTypeName, targetInfo);
        }
      }
      if (!targetInfo.isArrayOrVectorTarget) {
        error = "native backend only supports assign to array/vector elements";
        return false;
      }

      const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "native backend requires integer indices for " + accessName;
        return false;
      }

      const int32_t ptrLocal = allocTempLocal();
      if (!emitExpr(collectionTarget, localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

      const int32_t indexLocal = allocTempLocal();
      if (!emitExpr(indexExpr, localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

      if (accessName == "at") {
        const int32_t countLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        if (targetInfo.isVectorTarget) {
          instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
          instructions.push_back({IrOpcode::AddI64, 0});
        }
        instructions.push_back({IrOpcode::LoadIndirect, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

        if (indexKind != LocalInfo::ValueKind::UInt64) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          instructions.push_back({pushZeroForIndex(indexKind), 0});
          instructions.push_back({cmpLtForIndex(indexKind), 0});
          const size_t jumpNonNegative = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          instructions[jumpNonNegative].imm = static_cast<uint64_t>(instructions.size());
        }

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        instructions.push_back({cmpGeForIndex(indexKind), 0});
        const size_t jumpInRange = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        instructions[jumpInRange].imm = static_cast<uint64_t>(instructions.size());
      }

      int32_t basePtrLocal = ptrLocal;
      if (targetInfo.isVectorTarget) {
        basePtrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(3 * IrSlotBytes)});
        instructions.push_back({IrOpcode::AddI64, 0});
        instructions.push_back({IrOpcode::LoadIndirect, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(basePtrLocal)});
      }

      const int32_t destPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(basePtrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
      if (!targetInfo.isVectorTarget) {
        instructions.push_back({pushOneForIndex(indexKind), 1});
        instructions.push_back({addForIndex(indexKind), 0});
      }
      instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
      instructions.push_back({mulForIndex(indexKind), 0});
      instructions.push_back({IrOpcode::AddI64, 0});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

      const int32_t valueLocal = allocTempLocal();
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      return true;
    }
    if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
      if (target.args.size() != 1) {
        error = "assign target must be a mutable binding";
        return false;
      }
      const Expr &receiverExpr = target.args.front();
      const std::string receiverStruct = inferStructExprPath(receiverExpr, localsIn);
      if (receiverStruct.empty()) {
        error = "assign target must be a mutable binding";
        return false;
      }
      int32_t fieldSlotOffset = 0;
      int32_t fieldSlotCount = 0;
      std::string fieldStructPath;
      if (!resolveStructFieldInfo(receiverStruct,
                                  target.name,
                                  fieldSlotOffset,
                                  fieldSlotCount,
                                  fieldStructPath)) {
        error = "assign target must be a mutable binding";
        return false;
      }
      auto resolveSoaRefWriteTarget = [&](const Expr &candidate,
                                          const Expr *&valuesExprOut,
                                          const Expr *&indexExprOut) -> bool {
        valuesExprOut = nullptr;
        indexExprOut = nullptr;
        auto matchesBuiltinRefPath = [](const std::string &path) {
          const std::string canonicalPath =
              soa_paths::canonicalizeLegacySoaRefHelperPath(path);
          return soa_paths::isExperimentalSoaRefLikeHelperPath(canonicalPath) ||
                 soa_paths::isCanonicalSoaRefLikeHelperPath(canonicalPath);
        };
        if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 2 ||
            !candidate.templateArgs.empty() || candidate.hasBodyArguments ||
            !candidate.bodyArguments.empty()) {
          return false;
        }
        if (!candidate.isMethodCall) {
          const Definition *callee = context.resolveDefinitionCall(candidate);
          if (callee == nullptr || !matchesBuiltinRefPath(callee->fullPath)) {
            return false;
          }
          valuesExprOut = &candidate.args.front();
          indexExprOut = &candidate.args[1];
          return true;
        }

        std::string normalizedMethodName = candidate.name;
        if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
          normalizedMethodName.erase(normalizedMethodName.begin());
        }
        const bool isBareRefMethodName = normalizedMethodName == "ref";
        const std::string methodPath = "/" + normalizedMethodName;
        const std::string canonicalMethodPath =
            soa_paths::canonicalizeLegacySoaRefHelperPath(methodPath);
        const bool isLegacyOrCanonicalRefMethodPath =
            soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalMethodPath, "ref");
        if (!isLegacyOrCanonicalRefMethodPath && !isBareRefMethodName) {
          return false;
        }

        valuesExprOut = &candidate.args.front();
        indexExprOut = &candidate.args[1];
        return true;
      };
      auto emitSoaRefFieldPointer = [&](const Expr &candidate) -> bool {
        const Expr *valuesExpr = nullptr;
        const Expr *indexExpr = nullptr;
        if (!resolveSoaRefWriteTarget(candidate, valuesExpr, indexExpr) || valuesExpr == nullptr ||
            indexExpr == nullptr) {
          return false;
        }

        const std::string valuesStructPath = inferStructExprPath(*valuesExpr, localsIn);
        if (valuesStructPath.empty()) {
          return false;
        }

        int32_t elementSlotCount = 0;
        if (!resolveStructSlotCount(receiverStruct, elementSlotCount)) {
          return false;
        }

        const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(*indexExpr, localsIn));
        if (!isSupportedIndexKind(indexKind)) {
          error = "assign target must be a mutable binding";
          return false;
        }

        const int32_t indexLocal = allocTempLocal();
        if (!emitExpr(*indexExpr, localsIn)) {
          return false;
        }
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

        const int32_t dataPtrLocal = allocTempLocal();
        const int32_t capacityLocal = allocTempLocal();
        if (isSpecializedExperimentalSoaVectorStructPath(valuesStructPath)) {
          int32_t storageSlotOffset = 0;
          int32_t storageSlotCount = 0;
          std::string storageStructPath;
          if (!resolveStructFieldInfo(valuesStructPath,
                                      "storage",
                                      storageSlotOffset,
                                      storageSlotCount,
                                      storageStructPath) ||
              storageStructPath.empty()) {
            error = "assign target must be a mutable binding";
            return false;
          }

          int32_t capacitySlotOffset = 0;
          int32_t capacitySlotCount = 0;
          std::string ignoredStructPath;
          if (!resolveStructFieldInfo(storageStructPath,
                                      "capacity",
                                      capacitySlotOffset,
                                      capacitySlotCount,
                                      ignoredStructPath)) {
            error = "assign target must be a mutable binding";
            return false;
          }

          int32_t dataSlotOffset = 0;
          int32_t dataSlotCount = 0;
          if (!resolveStructFieldInfo(storageStructPath,
                                      "data",
                                      dataSlotOffset,
                                      dataSlotCount,
                                      ignoredStructPath)) {
            error = "assign target must be a mutable binding";
            return false;
          }

          const int32_t valuesPtrLocal = allocTempLocal();
          if (!emitExpr(*valuesExpr, localsIn)) {
            return false;
          }
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valuesPtrLocal)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
          const uint64_t capacityOffsetBytes =
              static_cast<uint64_t>(storageSlotOffset + capacitySlotOffset) * IrSlotBytes;
          if (capacityOffsetBytes != 0) {
            instructions.push_back({IrOpcode::PushI64, capacityOffsetBytes});
            instructions.push_back({IrOpcode::AddI64, 0});
          }
          instructions.push_back({IrOpcode::LoadIndirect, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});

          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesPtrLocal)});
          const uint64_t dataOffsetBytes =
              static_cast<uint64_t>(storageSlotOffset + dataSlotOffset) * IrSlotBytes;
          if (dataOffsetBytes != 0) {
            instructions.push_back({IrOpcode::PushI64, dataOffsetBytes});
            instructions.push_back({IrOpcode::AddI64, 0});
          }
          instructions.push_back({IrOpcode::LoadIndirect, 0});
          instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataPtrLocal)});
        } else {
          return false;
        }

        if (indexKind != LocalInfo::ValueKind::UInt64) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          instructions.push_back({pushZeroForIndex(indexKind), 0});
          instructions.push_back({cmpLtForIndex(indexKind), 0});
          const size_t jumpNonNegative = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          instructions[jumpNonNegative].imm = static_cast<uint64_t>(instructions.size());
        }

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
        instructions.push_back({cmpGeForIndex(indexKind), 0});
        const size_t jumpInRange = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        instructions[jumpInRange].imm = static_cast<uint64_t>(instructions.size());

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        if (elementSlotCount != 1) {
          instructions.push_back({pushOneForIndex(indexKind), static_cast<uint64_t>(elementSlotCount)});
          instructions.push_back({mulForIndex(indexKind), 0});
        }
        instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
        instructions.push_back({mulForIndex(indexKind), 0});
        instructions.push_back({IrOpcode::AddI64, 0});
        return true;
      };
      auto emitReceiverPointer = [&]() -> bool {
        if (emitSoaRefFieldPointer(receiverExpr)) {
          return true;
        }
        if (receiverExpr.kind == Expr::Kind::Name) {
          auto it = localsIn.find(receiverExpr.name);
          if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference) {
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
        }
        if (receiverExpr.kind == Expr::Kind::Call && !receiverExpr.isMethodCall &&
            isSimpleCallName(receiverExpr, "dereference") && receiverExpr.args.size() == 1) {
          const Expr &pointerExpr = receiverExpr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it != localsIn.end() &&
                (it->second.kind == LocalInfo::Kind::Reference || it->second.kind == LocalInfo::Kind::Pointer)) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              return true;
            }
          }
          return emitExpr(pointerExpr, localsIn);
        }
        return emitExpr(receiverExpr, localsIn);
      };
      if (!emitReceiverPointer()) {
        return false;
      }
      const int32_t receiverPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(receiverPtrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
      const uint64_t fieldOffsetBytes = static_cast<uint64_t>(fieldSlotOffset) * IrSlotBytes;
      if (fieldOffsetBytes != 0) {
        instructions.push_back({IrOpcode::PushI64, fieldOffsetBytes});
        instructions.push_back({IrOpcode::AddI64, 0});
      }
      const int32_t destPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
      if (!fieldStructPath.empty()) {
        const Expr &rhsExpr = expr.args[1];
        const std::string rhsStruct = inferStructExprPath(rhsExpr, localsIn);
        if (rhsStruct.empty() || !areCompatibleStructPaths(rhsStruct, fieldStructPath)) {
          error = "assign requires matching struct value";
          return false;
        }
        if (!emitExpr(rhsExpr, localsIn)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, fieldSlotCount)) {
          return false;
        }
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
        return true;
      }
      const int32_t valueLocal = allocTempLocal();
      if (!emitExpr(expr.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      return true;
    }
    error = "native backend only supports assign to local names or dereference";
    return false;
  }

  handled = false;
  return true;
}

} // namespace primec::ir_lowerer
