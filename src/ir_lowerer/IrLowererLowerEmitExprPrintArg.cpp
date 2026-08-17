#include "IrLowererLowerEmitExprPrintArg.h"

#include "IrLowererCountAccessHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererStringLiteralHelpers.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

bool emitPrintArgImpl(
    LowerSetupStageState &setupStageIn,
    LowerReturnEmitStageState &stateOutIn,
    const CallResolutionAdapters &callResolutionAdaptersIn,
    const Expr &arg,
    const LocalMap &localsIn,
    const PrintBuiltin &builtin,
    std::string &error) {
  IrFunction &function = setupStageIn.function;
  const SemanticProgram *const &semanticProgram = callResolutionAdaptersIn.semanticProgram;
  const ResolveExprPathFn &resolveExprPath = callResolutionAdaptersIn.resolveExprPath;
  const auto &inferExprKind = setupStageIn.inferenceSetupBootstrap.inferExprKind;
  const auto &allocTempLocal = stateOutIn.allocTempLocal;
  const auto &emitExpr = stateOutIn.emitExpr;
  const auto &internString =
      setupStageIn.setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString;
  const auto &emitArrayIndexOutOfBounds =
      setupStageIn.setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters
          .emitArrayIndexOutOfBounds;
  const bool &hasEntryArgs = setupStageIn.setupLocalsOrchestration.entryCountAccessSetup.hasEntryArgs;
  const std::string &entryArgsName = setupStageIn.setupLocalsOrchestration.entryCountAccessSetup.entryArgsName;
  const auto &isEntryArgsName =
      setupStageIn.setupLocalsOrchestration.entryCountAccessSetup.classifiers.isEntryArgsName;

  uint64_t flags = encodePrintFlags(builtin.newline, builtin.target == PrintTarget::Err);
  auto isEntryArgsPrintTarget = [&](const Expr &target) {
    if (isEntryArgsName(target, localsIn)) {
      return true;
    }
    if (!hasEntryArgs || semanticProgram == nullptr || target.kind != Expr::Kind::Name ||
        target.name != entryArgsName) {
      return false;
    }
    const auto *bindingFact = findSemanticProductBindingFact(
        callResolutionAdaptersIn.semanticProductTargets.semanticIndex, target);
    if (bindingFact == nullptr) {
      return false;
    }
    return bindingFact->name == entryArgsName &&
           bindingFact->bindingTypeText == "array<string>" &&
           (bindingFact->siteKind == "parameter" ||
            bindingFact->siteKind == "parameter-reference");
  };
  if (arg.kind == Expr::Kind::Call) {
    std::string accessName;
    const bool isBuiltinAccess =
        getBuiltinArrayAccessName(arg, accessName) ||
        ([&]() {
          const std::string resolvedAccessPath = resolveExprPath(arg);
          if (resolvedAccessPath == "/at") {
            accessName = "at";
            return true;
          }
          if (resolvedAccessPath == "/at_unsafe") {
            accessName = "at_unsafe";
            return true;
          }
          return false;
        })();
    if (isBuiltinAccess) {
      if (arg.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (isEntryArgsPrintTarget(arg.args.front())) {
        LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
        if (!resolveValidatedAccessIndexKind(
                arg.args[1],
                localsIn,
                accessName,
                inferExprKind,
                indexKind,
                error,
                semanticProgram,
                &callResolutionAdaptersIn.semanticProductTargets.semanticIndex)) {
          return false;
        }

        const int32_t indexLocal = allocTempLocal();
        if (!emitExpr(arg.args[1], localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

        if (accessName == "at") {
          if (indexKind != LocalInfo::ValueKind::UInt64) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({pushZeroForIndex(indexKind), 0});
            function.instructions.push_back({cmpLtForIndex(indexKind), 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({IrOpcode::PushArgc, 0});
          function.instructions.push_back({cmpGeForIndex(indexKind), 0});
          size_t jumpInRange = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          size_t inRangeIndex = function.instructions.size();
          function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
        }

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        IrOpcode printOp = (accessName == "at_unsafe") ? IrOpcode::PrintArgvUnsafe : IrOpcode::PrintArgv;
        function.instructions.push_back({printOp, flags});
        return true;
      }
    }
  }
  if (arg.kind == Expr::Kind::StringLiteral) {
    std::string decoded;
    if (!ir_lowerer::parseLowererStringLiteral(arg.stringValue, decoded, error)) {
      return false;
    }
    int32_t index = internString(decoded);
    function.instructions.push_back(
        {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(index), flags)});
    return true;
  }
  if (arg.kind == Expr::Kind::Name) {
    auto it = localsIn.find(arg.name);
    if (it == localsIn.end()) {
      error = "native backend does not know identifier: " + arg.name;
      return false;
    }
    const LocalInfo::ValueKind printNameKind = inferExprKind(arg, localsIn);
    if ((printNameKind == LocalInfo::ValueKind::Unknown ||
         printNameKind == LocalInfo::ValueKind::String) &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
        if (it->second.stringIndex < 0) {
          error = "native backend missing string data for: " + arg.name;
          return false;
        }
        function.instructions.push_back(
            {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(it->second.stringIndex), flags)});
        return true;
      }
      if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        IrOpcode printOp = it->second.argvChecked ? IrOpcode::PrintArgv : IrOpcode::PrintArgvUnsafe;
        function.instructions.push_back({printOp, flags});
        return true;
      }
      if (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
        return true;
      }
    }
  }
  if (!emitExpr(arg, localsIn)) {
    return false;
  }
  LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
  if (kind == LocalInfo::ValueKind::String) {
    function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int64) {
    function.instructions.push_back({IrOpcode::PrintI64, flags});
    return true;
  }
  if (kind == LocalInfo::ValueKind::UInt64) {
    function.instructions.push_back({IrOpcode::PrintU64, flags});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
    function.instructions.push_back({IrOpcode::PrintI32, flags});
    return true;
  }
  if (arg.kind == Expr::Kind::Call) {
    const size_t slashPos = arg.name.find_last_of('/');
    const std::string whyLeaf =
        slashPos == std::string::npos ? arg.name : arg.name.substr(slashPos + 1);
    if (whyLeaf == "why") {
      function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
      return true;
    }
  }
  error = builtin.name + " requires an integer/bool or string literal/binding argument";
  return false;
}

} // namespace primec::ir_lowerer
