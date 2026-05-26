#include "IrLowererStatementCallHelpers.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <optional>
#include <string_view>

namespace primec::ir_lowerer {

namespace {

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return stdCollectionsRoot() + "/experimental_" + std::string(collectionName) +
         "/" + std::string(typeName);
}

bool isCollectionVectorRecordTypePath(std::string_view path) {
  const std::string vectorTypePath = experimentalCollectionTypePath("vec" "tor", "Vector");
  return path == vectorTypePath || path.rfind(vectorTypePath + "__", 0) == 0;
}

std::string resolveStatementCallPathWithoutFallbackProbes(const Expr &expr) {
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

bool isFreeMemoryIntrinsicCall(const Expr &expr) {
  std::string builtinName;
  return expr.kind == Expr::Kind::Call && getBuiltinMemoryName(expr, builtinName) && builtinName == "free";
}

std::string resolveStatementCallSemanticTypeText(const SemanticProgram *semanticProgram,
                                                 SymbolId typeTextId,
                                                 const std::string &typeText) {
  if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
    const std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

static bool isStatementSoaVectorTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  const std::string normalizedTypeText =
      unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(typeText));
  if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
    const std::string normalizedBase =
        normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
        normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
      return isStatementSoaVectorTypeText(argText);
    }
    return normalizedBase == "soa" "_vector";
  }
  return normalizeCollectionBindingTypeName(normalizedTypeText) == "soa" "_vector";
}

static bool resolveStatementSoaVectorReceiverFromSemanticFacts(
    const Expr &receiverExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool &hasSemanticSoaVectorFactOut) {
  hasSemanticSoaVectorFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return false;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    return !resolvedTypeText.empty() && isStatementSoaVectorTypeText(resolvedTypeText);
  };

  if (receiverExpr.semanticNodeId != 0) {
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr);
        collectionFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      const std::string collectionFamily = resolveStatementCallSemanticTypeText(
          semanticProgram,
          collectionFact->collectionFamilyId,
          collectionFact->collectionFamily);
      return normalizeCollectionBindingTypeName(collectionFamily) == "soa" "_vector";
    }

    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr);
        queryFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(queryFact->bindingTypeTextId,
                                        queryFact->bindingTypeText) ||
             tryResolveSemanticTypeText(queryFact->queryTypeTextId,
                                        queryFact->queryTypeText) ||
             tryResolveSemanticTypeText(queryFact->receiverBindingTypeTextId,
                                        queryFact->receiverBindingTypeText);
    }

    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, receiverExpr);
        bindingFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText);
    }

    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr);
        localAutoFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText);
    }
  }

  if (receiverExpr.kind == Expr::Kind::Call && receiverExpr.args.size() == 1 &&
      (isSimpleCallName(receiverExpr, "location") ||
       isSimpleCallName(receiverExpr, "dereference"))) {
    return resolveStatementSoaVectorReceiverFromSemanticFacts(
        receiverExpr.args.front(),
        semanticProgram,
        semanticIndex,
        hasSemanticSoaVectorFactOut);
  }

  return false;
}

static bool isSoaVectorTargetExpr(const Expr &expr,
                                  const LocalMap &localsIn,
                                  const SemanticProgram *semanticProgram,
                                  const SemanticProductIndex *semanticIndex) {
  bool hasSemanticSoaVectorFact = false;
  if (resolveStatementSoaVectorReceiverFromSemanticFacts(expr,
                                                         semanticProgram,
                                                         semanticIndex,
                                                         hasSemanticSoaVectorFact)) {
    return true;
  }
  if (hasSemanticSoaVectorFact) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(expr, collection) && collection == "soa" "_vector";
  }
  return false;
}

static std::optional<LocalInfo::ValueKind> resolveBufferElementKindFromTypeText(
    const std::string &typeText,
    bool allowWrappedBuffer) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return std::nullopt;
  }

  const std::string normalizedBase = trimTemplateTypeText(base);
  if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
      normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
    if (!allowWrappedBuffer) {
      return std::nullopt;
    }
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
      return std::nullopt;
    }
    return resolveBufferElementKindFromTypeText(args.front(), true);
  }

  const bool isBufferBase =
      normalizedBase == "Buffer" || normalizedBase == "/Buffer" ||
      normalizedBase == "std/gfx/Buffer" || normalizedBase == "/std/gfx/Buffer" ||
      normalizedBase == "std/gfx/experimental/Buffer" ||
      normalizedBase == "/std/gfx/experimental/Buffer";
  if (!isBufferBase) {
    return std::nullopt;
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return std::nullopt;
  }
  const LocalInfo::ValueKind elementKind =
      valueKindFromTypeName(trimTemplateTypeText(args.front()));
  if (elementKind == LocalInfo::ValueKind::Unknown) {
    return std::nullopt;
  }
  return elementKind;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKindFromSemanticFacts(
    const Expr &bufferExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool allowWrappedBuffer,
    bool &hasSemanticBufferFactOut) {
  hasSemanticBufferFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return std::nullopt;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) -> std::optional<LocalInfo::ValueKind> {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    if (resolvedTypeText.empty()) {
      return std::nullopt;
    }
    return resolveBufferElementKindFromTypeText(resolvedTypeText, allowWrappedBuffer);
  };

  if (bufferExpr.semanticNodeId != 0) {
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, bufferExpr);
        queryFact != nullptr) {
      hasSemanticBufferFactOut = true;
      if (auto resolvedKind =
              tryResolveSemanticTypeText(queryFact->bindingTypeTextId, queryFact->bindingTypeText);
          resolvedKind.has_value()) {
        return resolvedKind;
      }
      if (auto resolvedKind =
              tryResolveSemanticTypeText(queryFact->queryTypeTextId, queryFact->queryTypeText);
          resolvedKind.has_value()) {
        return resolvedKind;
      }
      return std::nullopt;
    }

    if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, bufferExpr);
        bindingFact != nullptr) {
      hasSemanticBufferFactOut = true;
      return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText);
    }

    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, bufferExpr);
        localAutoFact != nullptr) {
      hasSemanticBufferFactOut = true;
      return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText);
    }
  }

  if (bufferExpr.kind == Expr::Kind::Call && bufferExpr.args.size() == 1 &&
      isSimpleCallName(bufferExpr, "dereference")) {
    return resolveBufferTargetElementKindFromSemanticFacts(
        bufferExpr.args.front(), semanticProgram, semanticIndex, true, hasSemanticBufferFactOut);
  }

  return std::nullopt;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKind(
    const Expr &bufferExpr,
    const LocalMap &localsIn,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  bool hasSemanticBufferFact = false;
  if (const auto semanticKind = resolveBufferTargetElementKindFromSemanticFacts(
          bufferExpr, semanticProgram, semanticIndex, false, hasSemanticBufferFact);
      semanticKind.has_value() || hasSemanticBufferFact) {
    return semanticKind;
  }

  if (bufferExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "buffer") &&
      bufferExpr.templateArgs.size() == 1) {
    return valueKindFromTypeName(bufferExpr.templateArgs.front());
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "dereference") &&
      bufferExpr.args.size() == 1) {
    const Expr &targetExpr = bufferExpr.args.front();
    if (targetExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.name);
      if (it != localsIn.end() &&
          ((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }

    std::string derefAccessName;
    if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
        targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack &&
          ((it->second.argsPackElementKind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.argsPackElementKind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }
  }

  std::string accessName;
  if (bufferExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(bufferExpr, accessName) &&
      bufferExpr.args.size() == 2 && bufferExpr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.args.front().name);
    if (it != localsIn.end() && it->second.isArgsPack && it->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  return std::nullopt;
}

static std::optional<LocalInfo::ValueKind> resolveStatementValueKindFromSemanticFacts(
    const Expr &expr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool &hasSemanticValueFactOut) {
  hasSemanticValueFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return std::nullopt;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) -> std::optional<LocalInfo::ValueKind> {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    if (resolvedTypeText.empty()) {
      return std::nullopt;
    }
    const LocalInfo::ValueKind valueKind = valueKindFromTypeName(resolvedTypeText);
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      return std::nullopt;
    }
    return valueKind;
  };

  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr);
      bindingFact != nullptr) {
    hasSemanticValueFactOut = true;
    return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                      bindingFact->bindingTypeText);
  }

  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, expr);
      localAutoFact != nullptr) {
    hasSemanticValueFactOut = true;
    return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                      localAutoFact->bindingTypeText);
  }

  if (const auto *queryFact = findSemanticProductQueryFact(semanticProgram, *semanticIndex, expr);
      queryFact != nullptr) {
    hasSemanticValueFactOut = true;
    if (auto valueKind =
            tryResolveSemanticTypeText(queryFact->bindingTypeTextId, queryFact->bindingTypeText);
        valueKind.has_value()) {
      return valueKind;
    }
    return tryResolveSemanticTypeText(queryFact->queryTypeTextId, queryFact->queryTypeText);
  }

  return std::nullopt;
}

} // namespace

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (const auto resolvedKind =
          resolveBufferTargetElementKind(stmt.args[0], localsIn, semanticProgram, semanticIndex);
      resolvedKind.has_value()) {
    elemKind = *resolvedKind;
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(
          stmt.args[1],
          localsIn,
          "buffer_store",
          inferExprKind,
          indexKind,
          error,
          semanticProgram,
          semanticIndex)) {
    error = "buffer_store requires integer index";
    return BufferStoreStatementEmitResult::Error;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(stmt.args[0], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  const int32_t valueLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({pushOneForIndex(indexKind), 1});
  instructions.push_back({addForIndex(indexKind), 0});
  instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
  instructions.push_back({mulForIndex(indexKind), 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return BufferStoreStatementEmitResult::Emitted;
}

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
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "dispatch")) {
    return DispatchStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() < 4) {
    error = "dispatch requires kernel and three dimension arguments";
    return DispatchStatementEmitResult::Error;
  }
  if (stmt.args.front().kind != Expr::Kind::Name) {
    error = "dispatch requires kernel name as first argument";
    return DispatchStatementEmitResult::Error;
  }

  const Expr &kernelExpr = stmt.args.front();
  const std::string kernelPath = resolveExprPath(kernelExpr);
  const Definition *kernelDef = findDefinitionByPath(kernelPath);
  if (!kernelDef) {
    error = "dispatch requires known kernel: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  bool isCompute = false;
  for (const auto &transform : kernelDef->transforms) {
    if (transform.name == "compute") {
      isCompute = true;
      break;
    }
  }
  if (!isCompute) {
    error = "dispatch requires compute definition: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  for (size_t i = 1; i <= 3; ++i) {
    bool hasSemanticDimensionFact = false;
    const std::optional<LocalInfo::ValueKind> semanticDimKind =
        resolveStatementValueKindFromSemanticFacts(
            stmt.args[i], semanticProgram, semanticIndex, hasSemanticDimensionFact);
    const LocalInfo::ValueKind dimKind =
        semanticDimKind.has_value()
            ? *semanticDimKind
            : (hasSemanticDimensionFact ? LocalInfo::ValueKind::Unknown
                                        : inferExprKind(stmt.args[i], localsIn));
    if (dimKind != LocalInfo::ValueKind::Int32) {
      error = "dispatch requires i32 dimensions";
      return DispatchStatementEmitResult::Error;
    }
  }

  const bool hasTrailingArgsPack =
      !kernelDef->parameters.empty() && isArgsPackBinding(kernelDef->parameters.back());
  const size_t fixedKernelParamCount =
      hasTrailingArgsPack ? kernelDef->parameters.size() - 1 : kernelDef->parameters.size();
  const size_t minDispatchArgs = fixedKernelParamCount + 4;
  if ((!hasTrailingArgsPack && stmt.args.size() != minDispatchArgs) ||
      (hasTrailingArgsPack && stmt.args.size() < minDispatchArgs)) {
    error = "dispatch argument count mismatch for " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  const int32_t gxLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gxLocal)});
  const int32_t gyLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gyLocal)});
  const int32_t gzLocal = allocTempLocal();
  if (!emitExpr(stmt.args[3], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gzLocal)});

  const int32_t xLocal = allocTempLocal();
  const int32_t yLocal = allocTempLocal();
  const int32_t zLocal = allocTempLocal();
  const int32_t gidXLocal = allocTempLocal();
  const int32_t gidYLocal = allocTempLocal();
  const int32_t gidZLocal = allocTempLocal();

  LocalMap dispatchLocals = localsIn;
  LocalInfo gidInfo;
  gidInfo.index = gidXLocal;
  gidInfo.kind = LocalInfo::Kind::Value;
  gidInfo.valueKind = LocalInfo::ValueKind::Int32;
  dispatchLocals.emplace(kGpuGlobalIdXName, gidInfo);
  gidInfo.index = gidYLocal;
  dispatchLocals.emplace(kGpuGlobalIdYName, gidInfo);
  gidInfo.index = gidZLocal;
  dispatchLocals.emplace(kGpuGlobalIdZName, gidInfo);

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  const size_t zCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gzLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t zJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  const size_t yCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gyLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t yJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  const size_t xCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gxLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t xJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidXLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidYLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidZLocal)});

  Expr kernelCall;
  kernelCall.kind = Expr::Kind::Call;
  kernelCall.name = kernelPath;
  kernelCall.namespacePrefix = kernelExpr.namespacePrefix;
  for (size_t i = 4; i < stmt.args.size(); ++i) {
    kernelCall.args.push_back(stmt.args[i]);
    kernelCall.argNames.push_back(std::nullopt);
  }
  if (!emitInlineDefinitionCall(kernelCall, *kernelDef, dispatchLocals, false)) {
    return DispatchStatementEmitResult::Error;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(xCheck)});
  const size_t xEnd = instructions.size();
  instructions[xJumpEnd].imm = static_cast<int32_t>(xEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(yCheck)});
  const size_t yEnd = instructions.size();
  instructions[yJumpEnd].imm = static_cast<int32_t>(yEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(zCheck)});
  const size_t zEnd = instructions.size();
  instructions[zJumpEnd].imm = static_cast<int32_t>(zEnd);

  return DispatchStatementEmitResult::Emitted;
}

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
    const std::function<void()> &emitArrayIndexOutOfBounds,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  auto resolveDefinitionPathAsDirectCall = [&](const Expr &callExpr,
                                               const std::string &path) -> const Definition * {
    if (path.empty()) {
      return nullptr;
    }
    Expr resolvedCall = callExpr;
    resolvedCall.name = path;
    resolvedCall.namespacePrefix.clear();
    resolvedCall.templateArgs.clear();
    resolvedCall.isMethodCall = false;
    return resolveDefinitionCall(resolvedCall);
  };
  auto matchesGeneratedLeafDefinition = [](const std::string &candidatePath,
                                           const std::string &basePath,
                                           const char *marker,
                                           size_t markerSize) {
    return !basePath.empty() &&
           candidatePath.rfind(basePath, 0) == 0 &&
           candidatePath.compare(basePath.size(), markerSize, marker) == 0 &&
           candidatePath.find('/', basePath.size() + markerSize) ==
               std::string::npos;
  };
  auto resolveGeneratedDefinitionPath = [&](const Expr &callExpr,
                                            const std::string &basePath) -> const Definition * {
    if (const Definition *callee =
            resolveDefinitionPathAsDirectCall(callExpr, basePath);
        callee != nullptr) {
      return callee;
    }
    if (semanticProgram == nullptr) {
      return nullptr;
    }
    for (const auto &semanticDef : semanticProgram->definitions) {
      const std::string &candidatePath = semanticDef.fullPath;
      if (matchesGeneratedLeafDefinition(candidatePath, basePath, "__t", 3) ||
          matchesGeneratedLeafDefinition(candidatePath, basePath, "__ov", 4) ||
          matchesGeneratedLeafDefinition(candidatePath, basePath, "<", 1)) {
        if (const Definition *callee =
                resolveDefinitionPathAsDirectCall(callExpr, candidatePath);
            callee != nullptr) {
          return callee;
        }
      }
    }
    return nullptr;
  };

  auto resolveDirectStatementDefinition = [&](const Expr &callExpr) -> const Definition * {
    if (const Definition *callee = resolveDefinitionCall(callExpr);
        callee != nullptr) {
      return callee;
    }
    const std::string rawPath =
        resolveStatementCallPathWithoutFallbackProbes(callExpr);
    if (const Definition *callee = resolveGeneratedDefinitionPath(callExpr, rawPath);
        callee != nullptr) {
      return callee;
    }
    const std::string resolvedTarget =
        findSemanticProductDirectCallTarget(semanticProgram, callExpr);
    if (resolvedTarget.empty()) {
      return nullptr;
    }
    return resolveGeneratedDefinitionPath(callExpr, resolvedTarget);
  };
  auto resolveMethodStatementDefinition = [&](const Expr &callExpr) -> const Definition * {
    if (const Definition *callee = resolveMethodCallDefinition(callExpr, localsIn);
        callee != nullptr) {
      return callee;
    }
    const std::string resolvedTarget =
        findSemanticProductMethodCallTarget(semanticProgram, callExpr);
    if (resolvedTarget.empty()) {
      return nullptr;
    }
    return resolveGeneratedDefinitionPath(callExpr, resolvedTarget);
  };

  auto emitVectorHeaderFieldAddress = [&](const Expr &receiver, int32_t slotOffset) {
    if (receiver.kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(receiver.name);
      if (localIt != localsIn.end() &&
          (localIt->second.kind == LocalInfo::Kind::Reference ||
           localIt->second.kind == LocalInfo::Kind::Pointer) &&
          !localIt->second.structTypeName.empty()) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
      } else if (!emitExpr(receiver, localsIn)) {
        return false;
      }
    } else if (!emitExpr(receiver, localsIn)) {
      return false;
    }
    if (slotOffset != 0) {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    return true;
  };
  auto emitVectorHeaderFieldLoad = [&](const Expr &receiver, int32_t slotOffset) {
    if (!emitVectorHeaderFieldAddress(receiver, slotOffset)) {
      return false;
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    return true;
  };
  auto emitBoundsTrapIfStackTrue = [&]() {
    const size_t okJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitArrayIndexOutOfBounds();
    instructions[okJump].imm = instructions.size();
  };
  auto isInternalSoaMetadataSetterCallee = [](const Definition &callee) {
    return callee.fullPath.rfind(
               "/std/collections/internal_soa_storage/SoaColumn", 0) == 0 ||
           callee.fullPath.rfind(
               "/std/collections/internal_soa_storage/SoaFieldView", 0) == 0;
  };
  auto isInternalVectorMetadataSetterCallee = [](const Definition &callee) {
    return isCollectionVectorRecordTypePath(callee.fullPath);
  };
  auto isStructDefinitionCallee = [](const Definition &definition) {
    for (const auto &transform : definition.transforms) {
      if (transform.name == "struct") {
        return true;
      }
    }
    return false;
  };
  auto isNonEntryKeyValueConstructorCallee = [](const Definition &callee) {
    const auto *metadata =
        findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors");
    if (metadata == nullptr) {
      return false;
    }
    std::string constructorName;
    return resolvePublishedStdlibSurfaceConstructorMemberName(
               callee.fullPath,
               metadata->id,
               constructorName) &&
           constructorName != "entry";
  };
  auto metadataCountSlotOffset = [&](const Definition &callee) {
    return isInternalVectorMetadataSetterCallee(callee) ? 0 : 1;
  };
  auto metadataCapacitySlotOffset = [&](const Definition &callee) {
    return isInternalVectorMetadataSetterCallee(callee) ? 1 : 2;
  };
  Expr directStmt = stmt;

  if (!directStmt.isMethodCall && directStmt.args.size() == 2) {
	if (const Definition *callee = resolveDefinitionCall(directStmt);
        callee != nullptr &&
        (isSimpleCallName(directStmt, "set_field_count") ||
         isSimpleCallName(directStmt, "set_field_capacity")) &&
        (isInternalSoaMetadataSetterCallee(*callee) ||
         isInternalVectorMetadataSetterCallee(*callee))) {
      const Expr &receiver = directStmt.args.front();
      const Expr &value = directStmt.args[1];
      const int32_t countSlotOffset = metadataCountSlotOffset(*callee);
      const int32_t capacitySlotOffset = metadataCapacitySlotOffset(*callee);
      if (!emitExpr(value, localsIn)) {
        return DirectCallStatementEmitResult::Error;
      }
      instructions.push_back({IrOpcode::PushI32, 0});
      instructions.push_back({IrOpcode::CmpLtI32, 0});
      emitBoundsTrapIfStackTrue();

      if (isSimpleCallName(directStmt, "set_field_count")) {
        if (!emitExpr(value, localsIn) ||
            !emitVectorHeaderFieldLoad(receiver, capacitySlotOffset)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
      } else {
        if (!emitVectorHeaderFieldLoad(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 1073741823});
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, capacitySlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
      }
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      instructions.push_back({IrOpcode::Pop, 0});
      return DirectCallStatementEmitResult::Emitted;
    }
  }

  if (!directStmt.isMethodCall &&
      directStmt.namespacePrefix.empty() &&
      directStmt.name.find('/') == std::string::npos &&
      directStmt.args.size() == 1 &&
      isSoaVectorTargetExpr(directStmt.args.front(), localsIn, semanticProgram, semanticIndex)) {
    Expr methodStmt = directStmt;
    methodStmt.isMethodCall = true;
    const std::string priorError = error;
    if (const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn)) {
      if (methodStmt.hasBodyArguments || !methodStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(methodStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error = priorError;
  }

  if (directStmt.isMethodCall) {
    const bool isBuiltinCountLikeMethod =
        isArrayCountCall(directStmt, localsIn) || isStringCountCall(directStmt, localsIn) ||
        isVectorCapacityCall(directStmt, localsIn);
    const bool isVectorMetadataSetterMethod =
        directStmt.args.size() == 2 &&
        (isSimpleCallName(directStmt, "set_field_count") ||
         isSimpleCallName(directStmt, "set_field_capacity"));
    const std::string priorError = error;
    const Definition *callee = resolveMethodStatementDefinition(directStmt);
    if (callee != nullptr && isVectorMetadataSetterMethod &&
        isStructDefinitionCallee(*callee) &&
        !isInternalVectorMetadataSetterCallee(*callee) &&
        !isInternalSoaMetadataSetterCallee(*callee)) {
      error = priorError;
      return DirectCallStatementEmitResult::NotMatched;
    }
    if (!callee && !isBuiltinCountLikeMethod) {
      if (isVectorMetadataSetterMethod) {
        error = priorError;
        return DirectCallStatementEmitResult::NotMatched;
      }
      return DirectCallStatementEmitResult::Error;
    }
    if (callee) {
      const bool isInternalMetadataSetter =
          directStmt.args.size() == 2 &&
          (isSimpleCallName(directStmt, "set_field_count") ||
           isSimpleCallName(directStmt, "set_field_capacity")) &&
          (isInternalSoaMetadataSetterCallee(*callee) ||
           isInternalVectorMetadataSetterCallee(*callee));
      if (isInternalMetadataSetter) {
        const Expr &receiver = directStmt.args.front();
        const Expr &value = directStmt.args[1];
        const int32_t countSlotOffset = metadataCountSlotOffset(*callee);
        const int32_t capacitySlotOffset = metadataCapacitySlotOffset(*callee);
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 0});
        instructions.push_back({IrOpcode::CmpLtI32, 0});
        emitBoundsTrapIfStackTrue();

        if (isSimpleCallName(directStmt, "set_field_count")) {
          if (!emitExpr(value, localsIn) ||
              !emitVectorHeaderFieldLoad(receiver, capacitySlotOffset)) {
            return DirectCallStatementEmitResult::Error;
          }
          instructions.push_back({IrOpcode::CmpGtI32, 0});
          emitBoundsTrapIfStackTrue();
          if (!emitVectorHeaderFieldAddress(receiver, countSlotOffset) ||
              !emitExpr(value, localsIn)) {
            return DirectCallStatementEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreIndirect, 0});
          instructions.push_back({IrOpcode::Pop, 0});
          error = priorError;
          return DirectCallStatementEmitResult::Emitted;
        }

        if (!emitVectorHeaderFieldLoad(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 1073741823});
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, capacitySlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        instructions.push_back({IrOpcode::Pop, 0});
        error = priorError;
        return DirectCallStatementEmitResult::Emitted;
      }
      if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error.clear();
  }

  const std::string priorError = error;
  const Definition *callee = resolveDirectStatementDefinition(directStmt);
  if (!callee) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
    error = "native backend does not support block arguments on calls";
    return DirectCallStatementEmitResult::Error;
  }

  ReturnInfo info;
  if (!getReturnInfo(callee->fullPath, info)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (isNonEntryKeyValueConstructorCallee(*callee)) {
    for (const Expr &arg : stmt.args) {
      if (!emitExpr(arg, localsIn)) {
        return DirectCallStatementEmitResult::Error;
      }
      instructions.push_back({IrOpcode::Pop, 0});
    }
    error = priorError;
    return DirectCallStatementEmitResult::Emitted;
  }
  if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!info.returnsVoid && !isStructDefinitionCallee(*callee)) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  error = priorError;
  return DirectCallStatementEmitResult::Emitted;
}

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
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  return tryEmitDirectCallStatement(stmt,
                                    localsIn,
                                    isArrayCountCall,
                                    isStringCountCall,
                                    isVectorCapacityCall,
                                    emitExpr,
                                    resolveMethodCallDefinition,
                                    resolveDefinitionCall,
                                    getReturnInfo,
                                    emitInlineDefinitionCall,
                                    []() {},
                                    instructions,
                                    error,
                                    semanticProgram,
                                    semanticIndex);
}

AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    std::vector<IrInstruction> &instructions) {
  if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
    if (!emitExpr(stmt, localsIn)) {
      return AssignOrExprStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::Pop, 0});
    return AssignOrExprStatementEmitResult::Emitted;
  }
  if (!emitExpr(stmt, localsIn)) {
    return AssignOrExprStatementEmitResult::Error;
  }
  if (!isFreeMemoryIntrinsicCall(stmt)) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return AssignOrExprStatementEmitResult::Emitted;
}

} // namespace primec::ir_lowerer
