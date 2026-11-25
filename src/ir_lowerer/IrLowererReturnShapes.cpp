#include "IrLowererReturnShapes.h"

#include "IrLowererCollectionRecordShapes.h"
#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

std::string packedResultStructValuePath(const Definition &def,
                                        const std::unordered_set<std::string> &structNames) {
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    std::vector<std::string> args;
    if (!splitTemplateTypeName(
            trimTemplateTypeText(transform.templateArgs.front()), base, argText) ||
        trimTemplateTypeText(base) != "Result" || !splitTopLevelTemplateArgs(argText, args) ||
        args.size() != 2) {
      return {};
    }
    const std::string value = trimTemplateTypeText(args.front());
    std::string path;
    std::string valueBase;
    std::string valueArgText;
    std::vector<std::string> valueArgs;
    if (splitTemplateTypeName(value, valueBase, valueArgText)) {
      // A generic struct (`Buffer<i32>`): the specialization is the struct whose path ends with the leaf of the
      // generic and the suffix of its type arguments.
      const std::string leaf =
          "/" + trimTemplateTypeText(valueBase.substr(valueBase.find_last_of('/') + 1));
      if (leaf == "/Buffer") {
        // A `Buffer` is an array of the producer's frame (count and elements), not a block of a fixed size.
        return {};
      }
      if (!splitTopLevelTemplateArgs(valueArgText, valueArgs)) {
        return {};
      }
      const std::string suffix = mangleTemplateTypeArgsSuffix(valueArgs);
      for (const std::string &candidate : structNames) {
        if (candidate.size() > suffix.size() + leaf.size() &&
            candidate.compare(candidate.size() - suffix.size(), suffix.size(), suffix) == 0 &&
            candidate.compare(candidate.size() - suffix.size() - leaf.size(), leaf.size(), leaf) ==
                0 &&
            (path.empty() || candidate < path)) {
          path = candidate;
        }
      }
    } else if (!isSupportedScalarTypeName(value)) {
      path = resolveStructTypeName(value, def.namespacePrefix, structNames);
    }
    return path.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0 ? std::string{} : path;
  }
  return {};
}

void materializePackedSingleSlotPayload(
    std::vector<IrInstruction> &instructions,
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    int32_t &nextLocal,
    const Int32ProviderFn &allocTempLocal) {
  PackedResultStructPayloadInfo payloadInfo;
  if (!resolvePackedResultStructPayloadInfo(structType, resolveStructSlotLayout, payloadInfo) ||
      !payloadInfo.isPackedSingleSlot) {
    return;
  }
  const int32_t baseLocal = nextLocal;
  nextLocal += payloadInfo.slotCount;
  const int32_t ptrLocal = allocTempLocal();
  instructions.push_back(
      {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
  instructions.push_back(
      {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + payloadInfo.fieldOffset)});
  instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
}

void emitPackedResultBlockReturnValue(
    std::vector<IrInstruction> &instructions,
    int32_t valueLocal,
    int32_t callerBlockLocal,
    int32_t slots,
    const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs) {
  constexpr uint64_t HalfShift = 4294967296ull;
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::PushI64, HalfShift});
  instructions.push_back({IrOpcode::DivI64, 0});
  instructions.push_back({IrOpcode::PushI64, HalfShift});
  instructions.push_back({IrOpcode::MulI64, 0});
  instructions.push_back({IrOpcode::SubI64, 0});
  const size_t jumpError = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  (void)copyFromPtrs(callerBlockLocal, valueLocal, slots);
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(callerBlockLocal)});
  const size_t jumpEnd = instructions.size();
  instructions.push_back({IrOpcode::Jump, 0});
  instructions[jumpError].imm = static_cast<int32_t>(instructions.size());
  // The error is rebuilt from its code: the frame-address validation follows adds and subtracts only.
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::PushI64, HalfShift});
  instructions.push_back({IrOpcode::DivI64, 0});
  instructions.push_back({IrOpcode::PushI64, HalfShift});
  instructions.push_back({IrOpcode::MulI64, 0});
  instructions[jumpEnd].imm = static_cast<int32_t>(instructions.size());
}

ActionFn makePackedResultBlockReturnAdjust(
    int32_t slots,
    std::vector<IrInstruction> &instructions,
    const Int32ProviderFn &allocTempLocal,
    int32_t callerBlockLocal,
    const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs) {
  if (slots <= 0) {
    return {};
  }
  return [&instructions, &allocTempLocal, &copyFromPtrs, slots, callerBlockLocal]() {
    emitPackedResultBlockReturnValue(
        instructions, allocTempLocal(), callerBlockLocal, slots, copyFromPtrs);
  };
}

int32_t
realCallPackedResultSlots(const Definition &def,
                          const std::unordered_set<std::string> &structNames,
                          const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
                              &resolveStructSlotLayout) {
  const std::string payloadPath = packedResultStructValuePath(def, structNames);
  if (payloadPath.empty()) {
    return 0;
  }
  PackedResultStructPayloadInfo payloadInfo;
  if (!resolvePackedResultStructPayloadInfo(payloadPath, resolveStructSlotLayout, payloadInfo) ||
      payloadInfo.isPackedSingleSlot) {
    return 0;
  }
  return payloadInfo.slotCount;
}

bool isPackedResultReturnValue(
    const Expr &valueExpr,
    const LocalMap &locals,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Definition &)> &returnsStdlibResultSum,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferKind) {
  if (valueExpr.kind == Expr::Kind::Name) {
    const auto localIt = locals.find(valueExpr.name);
    if (localIt == locals.end() || !localIt->second.structTypeName.empty() ||
        localIt->second.kind != LocalInfo::Kind::Value) {
      return false;
    }
    if (localIt->second.isResult) {
      return true;
    }
  } else if (valueExpr.kind == Expr::Kind::Call) {
    if (const Definition *callee = resolveDefinitionCall(valueExpr);
        callee != nullptr && returnsStdlibResultSum(*callee)) {
      return false;
    }
  } else if (valueExpr.kind != Expr::Kind::Literal) {
    return false;
  }
  if (!inferStructPath(valueExpr, locals).empty()) {
    return false;
  }
  const LocalInfo::ValueKind kind = inferKind(valueExpr, locals);
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         (kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::UInt8);
}

bool isTaskVectorWait(const Expr &expr, const LocalMap &locals) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name != "wait" ||
      expr.args.size() != 1 || expr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  const auto handleIt = locals.find(expr.args.front().name);
  return handleIt != locals.end() && !handleIt->second.taskResultVectorElementText.empty();
}

bool definitionReturnsBuiltinVector(
    const std::unordered_map<std::string, const Definition *> &defMap, const std::string &path) {
  const auto it = defMap.find(path);
  return it != defMap.end() && it->second != nullptr &&
         !declaredBuiltinVectorReturnElementText(*it->second).empty();
}

bool emitRealCallVectorReturn(const Expr &returnStmt,
                              const LocalMap &locals,
                              int32_t resultBlockLocal,
                              const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
                              const std::function<int32_t()> &allocTempLocal,
                              std::vector<IrInstruction> &instructions,
                              Expr &rewrittenStmt,
                              LocalMap &rewrittenLocals,
                              std::string &error) {
  constexpr int32_t VectorRecordSlots = 5;
  const Expr &valueExpr = returnStmt.args.front();
  const auto vectorIt =
      valueExpr.kind == Expr::Kind::Name ? locals.find(valueExpr.name) : locals.end();
  if (vectorIt == locals.end() || vectorIt->second.kind != LocalInfo::Kind::Vector ||
      vectorIt->second.dropFlagLocal < 0) {
    error = "a real call that returns a vector returns a local vector that owns its buffer";
    return false;
  }
  const int32_t srcPtrLocal = allocTempLocal();
  if (!emitExpr(valueExpr, locals)) {
    return false;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
  if (!emitStructCopyFromPtrs(instructions, resultBlockLocal, srcPtrLocal, VectorRecordSlots)) {
    return false;
  }
  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back(
      {IrOpcode::StoreLocal, static_cast<uint64_t>(vectorIt->second.dropFlagLocal)});
  rewrittenLocals = locals;
  LocalInfo returnInfo;
  returnInfo.kind = LocalInfo::Kind::Value;
  returnInfo.valueKind = LocalInfo::ValueKind::Int64;
  returnInfo.index = resultBlockLocal;
  const std::string tempName = "__native_return_vector_" + std::to_string(srcPtrLocal);
  rewrittenLocals.emplace(tempName, returnInfo);
  rewrittenStmt = returnStmt;
  Expr nameExpr;
  nameExpr.kind = Expr::Kind::Name;
  nameExpr.name = tempName;
  rewrittenStmt.args.front() = std::move(nameExpr);
  return true;
}

bool emitSumStorageReturn(int32_t ptrLocal,
                          int32_t totalSlots,
                          int32_t realCallResultBlockLocal,
                          const std::optional<ReturnStatementInlineContext> &inlineContext,
                          std::vector<IrInstruction> &instructions,
                          const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs,
                          const SharedReturnExitFn &emitSharedReturn,
                          const std::function<void()> &cleanupAll,
                          bool &sawReturn,
                          std::string &error) {
  if (realCallResultBlockLocal >= 0 && !inlineContext.has_value()) {
    if (!copyFromPtrs(realCallResultBlockLocal, ptrLocal, totalSlots)) {
      return false;
    }
    ptrLocal = realCallResultBlockLocal;
  }
  if (inlineContext.has_value()) {
    if (inlineContext->returnLocal < 0) {
      error = "native backend missing inline return local";
      return false;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back(
        {IrOpcode::StoreLocal, static_cast<uint64_t>(inlineContext->returnLocal)});
    const size_t jumpIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    if (inlineContext->returnJumps != nullptr) {
      inlineContext->returnJumps->push_back(jumpIndex);
    }
    return true;
  }
  if (!emitSharedReturn(IrOpcode::ReturnI64, ptrLocal)) {
    cleanupAll();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::ReturnI64, 0});
  }
  sawReturn = true;
  return true;
}

} // namespace primec::ir_lowerer
