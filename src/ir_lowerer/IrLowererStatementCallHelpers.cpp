#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerEffects.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace primec::ir_lowerer {

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

  auto resolveStructLayoutForLocal = [&](LocalInfo &info) {
    if (info.isFileHandle) {
      info.structTypeName.clear();
      return true;
    }
    if (info.structTypeName.empty() || info.structSlotCount > 0) {
      return true;
    }
    // TODO-5380: a borrowed `Reference<vector<T>>` / `Pointer<vector<T>>` binding
    // carries the builtin vector spelling; use the canonical Vector record.
    if ((info.referenceToVector || info.pointerToVector) && !info.isSoaVector &&
        normalizeCollectionBindingTypeName(info.structTypeName) == "vector") {
      const std::string elementType = typeNameForValueKind(info.valueKind);
      if (!elementType.empty()) {
        info.structTypeName = specializedCollectionVectorRecordPathForElementType(elementType);
      }
    }
    StructSlotLayoutInfo layout;
    if (!resolveStructSlotLayout(info.structTypeName, layout)) {
      bool resolvedViaEnclosingNamespace = false;
      if (!info.structTypeName.empty() && info.structTypeName.front() != '/') {
        // A bare struct type name declared inside a nested namespace (e.g. a
        // helper namespace like GraphicsSubstrate reopened inside std::gfx)
        // is not itself namespace-qualified; the actual struct is typically a
        // sibling declared in an ancestor namespace, not the definition's own
        // namespace. Walk up the enclosing namespace chain looking for it
        // before falling back to a bare root-level lookup.
        std::string prefix = def.namespacePrefix;
        while (!prefix.empty()) {
          const std::string candidate = prefix + "/" + info.structTypeName;
          if (resolveStructSlotLayout(candidate, layout)) {
            info.structTypeName = candidate;
            resolvedViaEnclosingNamespace = true;
            break;
          }
          const size_t lastSlash = prefix.find_last_of('/');
          if (lastSlash == std::string::npos) {
            break;
          }
          prefix = prefix.substr(0, lastSlash);
        }
      }
      if (!resolvedViaEnclosingNamespace) {
        if (info.structTypeName.empty() || info.structTypeName.front() == '/') {
          error = "internal error: missing struct slot layout for " + info.structTypeName;
          return false;
        }
        // The stdlib owned text is declared `[String]` after `import /std/text/*`.
        const std::string rootedStructTypeName =
            info.structTypeName == "String" ? std::string("/std/text/String") : "/" + info.structTypeName;
        if (!resolveStructSlotLayout(rootedStructTypeName, layout)) {
          error = "internal error: missing struct slot layout for " + info.structTypeName;
          return false;
        }
        info.structTypeName = rootedStructTypeName;
      }
    }
    // A prior speculative resolveStructSlotLayout attempt (e.g. an
    // enclosing-namespace climb candidate that didn't exist) may have left a
    // stale failure message in the shared error string even though this
    // local ultimately resolved successfully. Clear it so an unrelated later
    // failure doesn't surface this leftover, misleading text.
    error.clear();
    info.structSlotCount = layout.totalSlots;
    return true;
  };

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
    if (!resolveStructLayoutForLocal(thisInfo)) {
      return false;
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
    if (!resolveStructLayoutForLocal(info)) {
      return false;
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
  // A body whose last statement is a `return` already ran the scope cleanup on that path; the cleanup at the end is
  // only reachable when the body can fall off its end or a jump lands there (an unreachable second copy
  // of every destructor doubled large bodies).
  const bool endsWithReturn = [&]() {
    if (entryDef.statements.empty()) {
      return false;
    }
    const Expr &last = entryDef.statements.back();
    if (last.kind != Expr::Kind::Call || last.isBinding || last.name != "return" ||
        instructions.empty()) {
      return false;
    }
    const IrOpcode lastOp = instructions.back().op;
    if (lastOp != IrOpcode::ReturnVoid && lastOp != IrOpcode::ReturnI32 &&
        lastOp != IrOpcode::ReturnI64 && lastOp != IrOpcode::ReturnF32 &&
        lastOp != IrOpcode::ReturnF64) {
      return false;
    }
    return std::none_of(instructions.begin(), instructions.end(), [&](const IrInstruction &inst) {
      return (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) &&
             inst.imm == instructions.size();
    });
  }();
  if (!endsWithReturn) {
    emitCurrentFileScopeCleanup();
  }
  popFileScope();

  // A void body can also end by falling through after a return nested in a branch, or through a
  // branch whose jump lands past the last statement; either needs a trailing return.
  if (definitionReturnsVoid) {
    const auto isReturnOpcode = [](IrOpcode op) {
      return op == IrOpcode::ReturnVoid || op == IrOpcode::ReturnI32 || op == IrOpcode::ReturnI64 ||
             op == IrOpcode::ReturnF32 || op == IrOpcode::ReturnF64;
    };
    const bool jumpReachesEnd =
        std::any_of(instructions.begin(), instructions.end(), [&](const IrInstruction &inst) {
          return (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) &&
                 inst.imm == instructions.size();
        });
    if (instructions.empty() || !isReturnOpcode(instructions.back().op) || jumpReachesEnd) {
      instructions.push_back({IrOpcode::ReturnVoid, 0});
    }
  } else if (!sawReturn) {
    error = "native backend requires an explicit return statement";
    return EntryCallableExecutionResult::Error;
  }
  return EntryCallableExecutionResult::Emitted;
}

void finalizeEntryFunctionTable(IrFunction &entryFunction,
                                std::vector<IrFunction> &outFunctions,
                                int32_t &entryIndex) {
  // The entry is function 0. Definitions inlined at their call sites get no standalone body: no Call reaches
  // one. Bodies of real calls are appended after it by the calls stage (docs/spec/vm-design.md, Real calls).
  outFunctions.push_back(std::move(entryFunction));
  entryIndex = 0;
}

} // namespace primec::ir_lowerer
