#include "IrLowererStatementCallHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"

#include <optional>
#include <utility>

namespace primec::ir_lowerer {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool isFreeMemoryIntrinsicCall(const Expr &expr) {
  std::string builtinName;
  return expr.kind == Expr::Kind::Call && getBuiltinMemoryName(expr, builtinName) && builtinName == "free";
}

} // namespace

static bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(vectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  return false;
}

static bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

static bool isExplicitVectorMutatorHelperCall(const Expr &expr) {
  std::string aliasName;
  if (!resolveVectorHelperAliasName(expr, aliasName)) {
    return false;
  }
  const bool isExplicitHelperPath =
      expr.name.rfind("/vector/", 0) == 0 || expr.name.rfind("/std/collections/vector/", 0) == 0;
  if (!isExplicitHelperPath) {
    return false;
  }
  return aliasName == "push" || aliasName == "pop" || aliasName == "reserve" || aliasName == "clear" ||
         aliasName == "remove_at" || aliasName == "remove_swap";
}

static size_t explicitVectorHelperReceiverIndex(const Expr &expr) {
  for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
    if (expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
      return i;
    }
  }
  return 0;
}

static bool isSoaVectorTargetExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(expr, collection) && collection == "soa_vector";
  }
  return false;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKind(
    const Expr &bufferExpr,
    const LocalMap &localsIn) {
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

static DirectCallStatementEmitResult tryEmitVectorHelperCallFormStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  const bool isVectorMutatorCall =
      isVectorBuiltinName(stmt, "push") || isVectorBuiltinName(stmt, "pop") || isVectorBuiltinName(stmt, "reserve") ||
      isVectorBuiltinName(stmt, "clear") || isVectorBuiltinName(stmt, "remove_at") ||
      isVectorBuiltinName(stmt, "remove_swap");
  if (!isVectorMutatorCall || stmt.args.empty()) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  std::string explicitStdlibHelperName;
  const bool isExplicitStdlibVectorHelper =
      resolveVectorHelperAliasName(stmt, explicitStdlibHelperName) &&
      stmt.name.rfind("/std/collections/vector/", 0) == 0;

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= stmt.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };

  const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
  if (hasNamedArgs) {
    bool hasValuesNamedReceiver = false;
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        appendReceiverIndex(i);
        hasValuesNamedReceiver = true;
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }

  const bool probePositionalReorderedReceiver =
      !hasNamedArgs && stmt.args.size() > 1 &&
      (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
       stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
       stmt.args.front().kind == Expr::Kind::Name);
  if (probePositionalReorderedReceiver) {
    for (size_t i = 1; i < stmt.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }

  for (size_t receiverIndex : receiverIndices) {
    Expr methodStmt = stmt;
    methodStmt.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveVectorHelperAliasName(methodStmt, normalizedHelperName)) {
      methodStmt.name = normalizedHelperName;
    }
    if (receiverIndex != 0) {
      std::swap(methodStmt.args[0], methodStmt.args[receiverIndex]);
      if (methodStmt.argNames.size() < methodStmt.args.size()) {
        methodStmt.argNames.resize(methodStmt.args.size());
      }
      std::swap(methodStmt.argNames[0], methodStmt.argNames[receiverIndex]);
    }
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn);
    if (!callee) {
      error = priorError;
      continue;
    }
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

  if (isExplicitStdlibVectorHelper &&
      (explicitStdlibHelperName == "clear" || explicitStdlibHelperName == "remove_at" ||
       explicitStdlibHelperName == "remove_swap")) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  return DirectCallStatementEmitResult::NotMatched;
}

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (const auto resolvedKind = resolveBufferTargetElementKind(stmt.args[0], localsIn); resolvedKind.has_value()) {
    elemKind = *resolvedKind;
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
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
    std::string &error) {
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
    const LocalInfo::ValueKind dimKind = inferExprKind(stmt.args[i], localsIn);
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
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  bool explicitVectorMutatorHelperCall = isExplicitVectorMutatorHelperCall(stmt);
  auto explicitVectorHelperUsesBuiltinVectorReceiver = [&](const Expr &callExpr) {
    if (!explicitVectorMutatorHelperCall || callExpr.args.empty()) {
      return false;
    }
    const size_t receiverIndex = explicitVectorHelperReceiverIndex(callExpr);
    const auto targetInfo = resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex], localsIn);
    return targetInfo.isVectorTarget &&
           (targetInfo.structTypeName.empty() || targetInfo.structTypeName == "/vector");
  };
  auto rewriteBareVectorMethodMutatorToDirectCall = [&](const Expr &callExpr, Expr &rewrittenExpr) {
    if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty() ||
        !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
      return false;
    }
    const std::string helperName = callExpr.name;
    if (helperName != "push" && helperName != "pop" && helperName != "reserve" &&
        helperName != "clear" && helperName != "remove_at" && helperName != "remove_swap") {
      return false;
    }
    const size_t expectedArgCount =
        (helperName == "pop" || helperName == "clear") ? 1u : 2u;
    if (callExpr.args.size() != expectedArgCount) {
      return false;
    }
    const auto targetInfo = resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn);
    if (!targetInfo.isVectorTarget) {
      return false;
    }
    rewrittenExpr = callExpr;
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.name = helperName;
    return true;
  };
  Expr directStmt = stmt;
  bool rewrittenExplicitVectorMutatorToBuiltinCall = false;
  Expr rewrittenBareVectorMethodStmt;
  if (!explicitVectorMutatorHelperCall &&
      rewriteBareVectorMethodMutatorToDirectCall(stmt, rewrittenBareVectorMethodStmt)) {
    directStmt = rewrittenBareVectorMethodStmt;
  }
  if (explicitVectorMutatorHelperCall && explicitVectorHelperUsesBuiltinVectorReceiver(directStmt)) {
    std::string helperName;
    if (resolveVectorHelperAliasName(directStmt, helperName)) {
      const size_t receiverIndex = explicitVectorHelperReceiverIndex(directStmt);
      directStmt.name = helperName;
      directStmt.namespacePrefix.clear();
      if (receiverIndex < directStmt.args.size() && receiverIndex != 0) {
        std::swap(directStmt.args[0], directStmt.args[receiverIndex]);
        if (directStmt.argNames.size() < directStmt.args.size()) {
          directStmt.argNames.resize(directStmt.args.size());
        }
        std::swap(directStmt.argNames[0], directStmt.argNames[receiverIndex]);
      }
      if (hasNamedArguments(directStmt.argNames)) {
        directStmt.argNames.clear();
      }
      explicitVectorMutatorHelperCall = false;
      rewrittenExplicitVectorMutatorToBuiltinCall = true;
    }
  }
  if (rewrittenExplicitVectorMutatorToBuiltinCall) {
    if (!emitExpr(directStmt, localsIn)) {
      return DirectCallStatementEmitResult::Error;
    }
    return DirectCallStatementEmitResult::Emitted;
  }

  if (!explicitVectorMutatorHelperCall &&
      !directStmt.isMethodCall &&
      directStmt.args.size() == 1 &&
      isSoaVectorTargetExpr(directStmt.args.front(), localsIn)) {
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
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(directStmt, localsIn);
    if (!callee && !isBuiltinCountLikeMethod) {
      return DirectCallStatementEmitResult::Error;
    }
    if (callee) {
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

  if (!explicitVectorMutatorHelperCall) {
    const auto vectorHelperCallFormResult = tryEmitVectorHelperCallFormStatement(
        directStmt, localsIn, resolveMethodCallDefinition, emitInlineDefinitionCall, error);
    if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Error) {
      return DirectCallStatementEmitResult::Error;
    }
    if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Emitted) {
      return DirectCallStatementEmitResult::Emitted;
    }
  }

  const std::string priorError = error;
  const Definition *callee = resolveDefinitionCall(directStmt);
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
  if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!info.returnsVoid) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  error = priorError;
  return DirectCallStatementEmitResult::Emitted;
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

bool buildCallableDefinitionCallContext(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    int32_t &nextLocal,
    LocalMap &definitionLocals,
    Expr &callExpr,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)> &inferParameterLocalInfo,
    std::string &error) {
  definitionLocals.clear();
  callExpr = Expr{};
  callExpr.kind = Expr::Kind::Call;
  callExpr.name = def.fullPath;
  callExpr.namespacePrefix = def.namespacePrefix;

  bool isComputeDefinition = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      isComputeDefinition = true;
      break;
    }
  }
  if (isComputeDefinition) {
    auto addGpuLocal = [&](const char *name) {
      LocalInfo gpuInfo;
      gpuInfo.index = nextLocal++;
      gpuInfo.kind = LocalInfo::Kind::Value;
      gpuInfo.valueKind = LocalInfo::ValueKind::Int32;
      definitionLocals.emplace(name, gpuInfo);
    };
    addGpuLocal(kGpuGlobalIdXName);
    addGpuLocal(kGpuGlobalIdYName);
    addGpuLocal(kGpuGlobalIdZName);
  }

  std::string helperParent;
  if (isStructHelperDefinition(def, structNames, helperParent) &&
      !definitionHasTransform(def, "static")) {
    Expr thisParam = makeStructHelperThisParam(
        helperParent,
        definitionHasTransform(def, "mut"));
    LocalInfo thisInfo;
    thisInfo.index = nextLocal++;
    if (!inferParameterLocalInfo(thisParam, definitionLocals, thisInfo, error)) {
      return false;
    }
    if (!thisInfo.structTypeName.empty() && thisInfo.structSlotCount <= 0) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(thisInfo.structTypeName, layout)) {
        error = "internal error: missing struct slot layout for " + thisInfo.structTypeName;
        return false;
      }
      thisInfo.structSlotCount = layout.totalSlots;
    }
    definitionLocals.emplace(thisParam.name, thisInfo);

    Expr argExpr;
    argExpr.kind = Expr::Kind::Name;
    argExpr.name = thisParam.name;
    callExpr.args.push_back(std::move(argExpr));
    callExpr.argNames.push_back(std::nullopt);
  }

  for (const auto &param : def.parameters) {
    LocalInfo info;
    info.index = nextLocal++;
    if (!inferParameterLocalInfo(param, definitionLocals, info, error)) {
      return false;
    }
    if (!info.structTypeName.empty() && info.structSlotCount <= 0) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(info.structTypeName, layout)) {
        error = "internal error: missing struct slot layout for " + info.structTypeName;
        return false;
      }
      info.structSlotCount = layout.totalSlots;
    }
    if (info.isArgsPack && info.argsPackElementCount < 0) {
      // Synthetic callable lowering has no concrete caller, but mixed forwarding still
      // needs a non-negative pack size to lower deterministically.
      info.argsPackElementCount = 0;
    }
    if (!info.isArgsPack && info.valueKind == LocalInfo::ValueKind::Unknown && info.structTypeName.empty()) {
      error = "native backend requires typed parameters on " + def.fullPath;
      return false;
    }
    definitionLocals.emplace(param.name, info);

    Expr argExpr;
    argExpr.kind = Expr::Kind::Name;
    argExpr.name = param.name;
    callExpr.args.push_back(std::move(argExpr));
    callExpr.argNames.push_back(std::nullopt);
  }
  return true;
}

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
    std::string &error) {
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryDef.fullPath || isStructDefinition(def) ||
        loweredCallTargets.find(def.fullPath) == loweredCallTargets.end()) {
      continue;
    }
    bool hasArgsPackParam = false;
    for (const auto &param : def.parameters) {
      if (isArgsPackBinding(param)) {
        hasArgsPackParam = true;
        break;
      }
    }
    if (hasArgsPackParam) {
      continue;
    }

    ReturnInfo returnInfo;
    if (!getReturnInfo(def.fullPath, returnInfo)) {
      return CallableDefinitionOrchestrationResult::Error;
    }

    function = IrFunction{};
    function.name = def.fullPath;
    if (!resolveEffectMask(
            def.transforms, false, defaultEffects, entryDefaultEffects, function.metadata.effectMask, error)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    if (!resolveCapabilityMask(def.transforms,
                               resolveActiveEffects(def.transforms, false, defaultEffects, entryDefaultEffects),
                               def.fullPath,
                               function.metadata.capabilityMask,
                               error)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    function.metadata.schedulingScope = IrSchedulingScope::Default;
    function.metadata.instrumentationFlags = 0;
    if (hasTailExecutionCandidate(def.statements, returnInfo.returnsVoid, isTailCallCandidate)) {
      function.metadata.instrumentationFlags |= InstrumentationTailExecution;
    }

    resetDefinitionLoweringState();
    nextLocal = 0;

    LocalMap definitionLocals;
    Expr callExpr;
    if (!buildDefinitionCallContext(def, nextLocal, definitionLocals, callExpr, error)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    if (!emitInlineDefinitionCall(callExpr, def, definitionLocals, !returnInfo.returnsVoid)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    if (!appendReturnForDefinition(def.fullPath, returnInfo)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    outFunctions.push_back(std::move(function));
  }

  return CallableDefinitionOrchestrationResult::Emitted;
}

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
    std::string &error) {
  OnErrorScope entryOnErrorScope(currentOnError, entryOnError);
  ResultReturnScope entryResultScope(currentReturnResult, entryResult);
  pushFileScope();
  for (const auto &stmt : entryDef.statements) {
    if (!emitStatement(stmt)) {
      return EntryCallableExecutionResult::Error;
    }
  }
  if (!sawReturn && entryDef.returnExpr.has_value()) {
    Expr returnStmt;
    returnStmt.kind = Expr::Kind::Call;
    returnStmt.name = "return";
    returnStmt.namespacePrefix = entryDef.namespacePrefix;
    returnStmt.sourceLine = entryDef.returnExpr->sourceLine;
    returnStmt.sourceColumn = entryDef.returnExpr->sourceColumn;
    returnStmt.args.push_back(*entryDef.returnExpr);
    returnStmt.argNames.push_back(std::nullopt);
    if (!emitStatement(returnStmt)) {
      return EntryCallableExecutionResult::Error;
    }
  }
  emitCurrentFileScopeCleanup();
  popFileScope();

  if (!sawReturn) {
    if (definitionReturnsVoid) {
      instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return EntryCallableExecutionResult::Error;
    }
  }
  return EntryCallableExecutionResult::Emitted;
}

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
    std::string &error) {
  outFunctions.push_back(std::move(entryFunction));
  entryIndex = 0;

  IrFunction callableFunction;
  const auto callableLoweringResult = lowerCallableDefinitionOrchestration(
      program,
      entryDef,
      loweredCallTargets,
      isStructDefinition,
      getReturnInfo,
      defaultEffects,
      entryDefaultEffects,
      isTailCallCandidate,
      resetDefinitionLoweringState,
      buildDefinitionCallContext,
      emitInlineDefinitionCall,
      [&](const std::string &definitionPath, const ReturnInfo &returnInfo) {
        return emitReturnForDefinition(callableFunction.instructions, definitionPath, returnInfo, error);
      },
      callableFunction,
      nextLocal,
      outFunctions,
      error);
  if (callableLoweringResult == CallableDefinitionOrchestrationResult::Error) {
    return FunctionTableFinalizationResult::Error;
  }
  return FunctionTableFinalizationResult::Emitted;
}

} // namespace primec::ir_lowerer
