#include "IrLowererFlowHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

namespace primec::ir_lowerer {

namespace {
bool checkedAddI64(int64_t lhs, int64_t rhs, int64_t &out) {
  if ((rhs > 0 && lhs > std::numeric_limits<int64_t>::max() - rhs) ||
      (rhs < 0 && lhs < std::numeric_limits<int64_t>::min() - rhs)) {
    return false;
  }
  out = lhs + rhs;
  return true;
}

bool checkedSubI64(int64_t lhs, int64_t rhs, int64_t &out) {
  if ((rhs > 0 && lhs < std::numeric_limits<int64_t>::min() + rhs) ||
      (rhs < 0 && lhs > std::numeric_limits<int64_t>::max() + rhs)) {
    return false;
  }
  out = lhs - rhs;
  return true;
}

bool checkedNegI64(int64_t value, int64_t &out) {
  if (value == std::numeric_limits<int64_t>::min()) {
    return false;
  }
  out = -value;
  return true;
}

bool checkedAddU64(uint64_t lhs, uint64_t rhs, uint64_t &out) {
  if (lhs > std::numeric_limits<uint64_t>::max() - rhs) {
    return false;
  }
  out = lhs + rhs;
  return true;
}

bool checkedSubU64(uint64_t lhs, uint64_t rhs, uint64_t &out) {
  if (lhs < rhs) {
    return false;
  }
  out = lhs - rhs;
  return true;
}

enum class SignedLiteralIntegerEvalResult { NotFoldable, Value, Overflow };
enum class UnsignedLiteralIntegerEvalResult { NotFoldable, Value, Overflow };

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool resolveStdCollectionsVectorWrapperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "vectorPush") {
    helperNameOut = "push";
    return true;
  }
  if (helperName == "vectorPop") {
    helperNameOut = "pop";
    return true;
  }
  if (helperName == "vectorReserve") {
    helperNameOut = "reserve";
    return true;
  }
  if (helperName == "vectorClear") {
    helperNameOut = "clear";
    return true;
  }
  if (helperName == "vectorRemoveAt") {
    helperNameOut = "remove_at";
    return true;
  }
  if (helperName == "vectorRemoveSwap") {
    helperNameOut = "remove_swap";
    return true;
  }
  return false;
}

bool resolveExperimentalVectorMutatorAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "vectorPush") {
    helperNameOut = "push";
    return true;
  }
  if (helperName == "vectorPop") {
    helperNameOut = "pop";
    return true;
  }
  if (helperName == "vectorReserve") {
    helperNameOut = "reserve";
    return true;
  }
  if (helperName == "vectorClear") {
    helperNameOut = "clear";
    return true;
  }
  if (helperName == "vectorRemoveAt") {
    helperNameOut = "remove_at";
    return true;
  }
  if (helperName == "vectorRemoveSwap") {
    helperNameOut = "remove_swap";
    return true;
  }
  return false;
}

bool resolveVectorMutatorAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string collectionsVectorWrapperPrefix = "std/collections/vector";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(vectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(collectionsVectorWrapperPrefix, 0) == 0 &&
      normalized.rfind(stdVectorPrefix, 0) != 0) {
    return resolveStdCollectionsVectorWrapperAliasName(
        normalized.substr(collectionsVectorWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    return resolveExperimentalVectorMutatorAliasName(
        normalized.substr(experimentalVectorPrefix.size()), helperNameOut);
  }
  return false;
}

bool resolveVectorMutatorName(const Expr &expr, std::string &helperNameOut) {
  if (isSimpleCallName(expr, "push")) {
    helperNameOut = "push";
    return true;
  }
  if (isSimpleCallName(expr, "pop")) {
    helperNameOut = "pop";
    return true;
  }
  if (isSimpleCallName(expr, "reserve")) {
    helperNameOut = "reserve";
    return true;
  }
  if (isSimpleCallName(expr, "clear")) {
    helperNameOut = "clear";
    return true;
  }
  if (isSimpleCallName(expr, "remove_at")) {
    helperNameOut = "remove_at";
    return true;
  }
  if (isSimpleCallName(expr, "remove_swap")) {
    helperNameOut = "remove_swap";
    return true;
  }

  std::string aliasName;
  if (!resolveVectorMutatorAliasName(expr, aliasName)) {
    return false;
  }
  if (aliasName == "push" || aliasName == "pop" || aliasName == "reserve" ||
      aliasName == "clear" || aliasName == "remove_at" || aliasName == "remove_swap") {
    helperNameOut = aliasName;
    return true;
  }
  return false;
}

SignedLiteralIntegerEvalResult tryEvaluateSignedLiteralIntegerExpr(const Expr &expr, int64_t &out) {
  if (expr.kind == Expr::Kind::Literal && !expr.isUnsigned) {
    out = expr.intWidth == 32 ? static_cast<int32_t>(expr.literalValue) : static_cast<int64_t>(expr.literalValue);
    return SignedLiteralIntegerEvalResult::Value;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || !expr.templateArgs.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return SignedLiteralIntegerEvalResult::NotFoldable;
  }
  if (isSimpleCallName(expr, "negate") && expr.args.size() == 1) {
    int64_t operand = 0;
    const SignedLiteralIntegerEvalResult operandResult = tryEvaluateSignedLiteralIntegerExpr(expr.args.front(), operand);
    if (operandResult != SignedLiteralIntegerEvalResult::Value) {
      return operandResult;
    }
    if (!checkedNegI64(operand, out)) {
      return SignedLiteralIntegerEvalResult::Overflow;
    }
    return SignedLiteralIntegerEvalResult::Value;
  }
  if (expr.args.size() != 2) {
    return SignedLiteralIntegerEvalResult::NotFoldable;
  }
  const bool isPlus = isSimpleCallName(expr, "plus");
  const bool isMinus = isSimpleCallName(expr, "minus");
  if (!isPlus && !isMinus) {
    return SignedLiteralIntegerEvalResult::NotFoldable;
  }
  int64_t lhs = 0;
  int64_t rhs = 0;
  const SignedLiteralIntegerEvalResult lhsResult = tryEvaluateSignedLiteralIntegerExpr(expr.args[0], lhs);
  const SignedLiteralIntegerEvalResult rhsResult = tryEvaluateSignedLiteralIntegerExpr(expr.args[1], rhs);
  if (lhsResult == SignedLiteralIntegerEvalResult::Overflow ||
      rhsResult == SignedLiteralIntegerEvalResult::Overflow) {
    return SignedLiteralIntegerEvalResult::Overflow;
  }
  if (lhsResult != SignedLiteralIntegerEvalResult::Value ||
      rhsResult != SignedLiteralIntegerEvalResult::Value) {
    return SignedLiteralIntegerEvalResult::NotFoldable;
  }
  if (isPlus) {
    if (!checkedAddI64(lhs, rhs, out)) {
      return SignedLiteralIntegerEvalResult::Overflow;
    }
  } else {
    if (!checkedSubI64(lhs, rhs, out)) {
      return SignedLiteralIntegerEvalResult::Overflow;
    }
  }
  return SignedLiteralIntegerEvalResult::Value;
}

UnsignedLiteralIntegerEvalResult tryEvaluateUnsignedLiteralIntegerExpr(const Expr &expr, uint64_t &out) {
  if (expr.kind == Expr::Kind::Literal && expr.isUnsigned) {
    out = expr.literalValue;
    return UnsignedLiteralIntegerEvalResult::Value;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding || !expr.templateArgs.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() || expr.args.size() != 2) {
    return UnsignedLiteralIntegerEvalResult::NotFoldable;
  }
  const bool isPlus = isSimpleCallName(expr, "plus");
  const bool isMinus = isSimpleCallName(expr, "minus");
  if (!isPlus && !isMinus) {
    return UnsignedLiteralIntegerEvalResult::NotFoldable;
  }
  uint64_t lhs = 0;
  uint64_t rhs = 0;
  const UnsignedLiteralIntegerEvalResult lhsResult = tryEvaluateUnsignedLiteralIntegerExpr(expr.args[0], lhs);
  const UnsignedLiteralIntegerEvalResult rhsResult = tryEvaluateUnsignedLiteralIntegerExpr(expr.args[1], rhs);
  if (lhsResult == UnsignedLiteralIntegerEvalResult::Overflow ||
      rhsResult == UnsignedLiteralIntegerEvalResult::Overflow) {
    return UnsignedLiteralIntegerEvalResult::Overflow;
  }
  if (lhsResult != UnsignedLiteralIntegerEvalResult::Value ||
      rhsResult != UnsignedLiteralIntegerEvalResult::Value) {
    return UnsignedLiteralIntegerEvalResult::NotFoldable;
  }
  if (isPlus) {
    if (!checkedAddU64(lhs, rhs, out)) {
      return UnsignedLiteralIntegerEvalResult::Overflow;
    }
  } else {
    if (!checkedSubU64(lhs, rhs, out)) {
      return UnsignedLiteralIntegerEvalResult::Overflow;
    }
  }
  return UnsignedLiteralIntegerEvalResult::Value;
}
} // namespace

OnErrorScope::OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

OnErrorScope::~OnErrorScope() {
  target = std::move(previous);
}

ResultReturnScope::ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

ResultReturnScope::~ResultReturnScope() {
  target = std::move(previous);
}

bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error) {
  if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::PushI64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::PushF64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::PushF32, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqF32 : IrOpcode::CmpNeF32, 0});
    return true;
  }
  error = "boolean conversion requires numeric operand";
  return false;
}

bool emitFloatLiteral(std::vector<IrInstruction> &instructions, const Expr &expr, std::string &error) {
  char *end = nullptr;
  const char *text = expr.floatValue.c_str();
  double value = std::strtod(text, &end);
  if (end == text || (end && *end != '\0')) {
    error = "invalid float literal";
    return false;
  }
  if (expr.floatWidth == 64) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    instructions.push_back({IrOpcode::PushF64, bits});
    return true;
  }
  float f32 = static_cast<float>(value);
  uint32_t bits = 0;
  std::memcpy(&bits, &f32, sizeof(bits));
  instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
  return true;
}

bool emitReturnForDefinition(std::vector<IrInstruction> &instructions,
                             const std::string &defPath,
                             const ReturnInfo &returnInfo,
                             std::string &error) {
  if (returnInfo.returnsVoid) {
    instructions.push_back({IrOpcode::ReturnVoid, 0});
    return true;
  }
  if (returnInfo.returnsArray || returnInfo.kind == LocalInfo::ValueKind::Int64 ||
      returnInfo.kind == LocalInfo::ValueKind::UInt64 || returnInfo.kind == LocalInfo::ValueKind::String) {
    instructions.push_back({IrOpcode::ReturnI64, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Int32 || returnInfo.kind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::ReturnI32, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::ReturnF32, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::ReturnF64, 0});
    return true;
  }
  error = "native backend does not support return type on " + defPath;
  return false;
}

UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(const Expr &expr,
                                                       const char *callName,
                                                       const std::function<bool(const Expr &)> &emitExpr,
                                                       std::string &error) {
  if (!isSimpleCallName(expr, callName)) {
    return UnaryPassthroughCallResult::NotMatched;
  }
  if (expr.args.size() != 1) {
    error = std::string(callName) + " requires exactly one argument";
    return UnaryPassthroughCallResult::Error;
  }
  if (!emitExpr(expr.args.front())) {
    return UnaryPassthroughCallResult::Error;
  }
  return UnaryPassthroughCallResult::Emitted;
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  std::string vectorHelper;
  if (!resolveVectorMutatorName(stmt, vectorHelper)) {
    return VectorStatementHelperEmitResult::NotMatched;
  }
  const bool explicitVectorHelperPath =
      !stmt.isMethodCall &&
      (stmt.name.rfind("/std/collections/vector/", 0) == 0 || stmt.name.rfind("/vector/", 0) == 0);
  Expr normalizedStmt = stmt;
  const Expr *activeStmt = &stmt;
  bool useBuiltinCompatibilityStmt = false;

  auto isMutableVectorTargetExpr = [&](const Expr &expr) {
    auto classifyVectorLocal = [&](const LocalInfo &localInfo, bool fromArgsPack) {
      const LocalInfo::Kind kind = fromArgsPack ? localInfo.argsPackElementKind : localInfo.kind;
      if (localInfo.isSoaVector &&
          (kind == LocalInfo::Kind::Value || kind == LocalInfo::Kind::Vector ||
           kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer)) {
        return true;
      }
      if (!fromArgsPack && kind == LocalInfo::Kind::Vector && localInfo.isMutable) {
        return true;
      }
      if (fromArgsPack && kind == LocalInfo::Kind::Vector) {
        return true;
      }
      if (kind == LocalInfo::Kind::Reference && localInfo.referenceToVector) {
        return true;
      }
      if (kind == LocalInfo::Kind::Pointer && localInfo.pointerToVector) {
        return true;
      }
      return false;
    };

    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return false;
      }
      return classifyVectorLocal(it->second, false);
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 &&
          expr.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(expr.args.front().name);
        if (it != localsIn.end() && it->second.isArgsPack) {
          return classifyVectorLocal(it->second, true);
        }
      }
      if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
        const Expr &derefTarget = expr.args.front();
        if (derefTarget.kind == Expr::Kind::Name) {
          auto it = localsIn.find(derefTarget.name);
          if (it != localsIn.end()) {
            return classifyVectorLocal(it->second, false);
          }
        }
        if (getBuiltinArrayAccessName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
            derefTarget.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(derefTarget.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack) {
            return classifyVectorLocal(it->second, true);
          }
        }
      }
    }
    return expr.kind == Expr::Kind::Call && expr.isFieldAccess && inferStructExprPath(expr, localsIn) == "/vector";
  };

  if (!stmt.isMethodCall && !stmt.args.empty()) {
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
    bool hasNamedArgs = false;
    for (const auto &argName : stmt.argNames) {
      if (argName.has_value()) {
        hasNamedArgs = true;
        break;
      }
    }
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
            *stmt.argNames[i] == "values") {
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
         (stmt.args.front().kind == Expr::Kind::Name &&
          !isMutableVectorTargetExpr(stmt.args.front())));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      if (receiverIndex >= stmt.args.size()) {
        continue;
      }
      if (explicitVectorHelperPath && isMutableVectorTargetExpr(stmt.args[receiverIndex])) {
        normalizedStmt = stmt;
        if (receiverIndex != 0) {
          std::swap(normalizedStmt.args[0], normalizedStmt.args[receiverIndex]);
          if (normalizedStmt.argNames.size() < normalizedStmt.args.size()) {
            normalizedStmt.argNames.resize(normalizedStmt.args.size());
          }
          std::swap(normalizedStmt.argNames[0], normalizedStmt.argNames[receiverIndex]);
        }
        if (hasNamedArguments(normalizedStmt.argNames)) {
          normalizedStmt.argNames.clear();
        }
        activeStmt = &normalizedStmt;
        useBuiltinCompatibilityStmt = true;
        break;
      }
      Expr methodStmt = stmt;
      methodStmt.isMethodCall = true;
      methodStmt.name = vectorHelper;
      if (receiverIndex != 0) {
        std::swap(methodStmt.args[0], methodStmt.args[receiverIndex]);
        if (methodStmt.argNames.size() < methodStmt.args.size()) {
          methodStmt.argNames.resize(methodStmt.args.size());
        }
        std::swap(methodStmt.argNames[0], methodStmt.argNames[receiverIndex]);
      }
      if (isUserDefinedVectorHelperCall(methodStmt)) {
        return VectorStatementHelperEmitResult::NotMatched;
      }
    }
  }

  const Expr &callStmt = *activeStmt;
  if (!useBuiltinCompatibilityStmt && isUserDefinedVectorHelperCall(stmt)) {
    return VectorStatementHelperEmitResult::NotMatched;
  }
  if (!callStmt.templateArgs.empty()) {
    error = vectorHelper + " does not accept template arguments";
    return VectorStatementHelperEmitResult::Error;
  }
  if (callStmt.hasBodyArguments || !callStmt.bodyArguments.empty()) {
    error = vectorHelper + " does not accept block arguments";
    return VectorStatementHelperEmitResult::Error;
  }

  const size_t expectedArgs = (vectorHelper == "pop" || vectorHelper == "clear") ? 1 : 2;
  if (callStmt.args.size() != expectedArgs) {
    if (expectedArgs == 1) {
      error = vectorHelper + " requires exactly one argument";
    } else {
      error = vectorHelper + " requires exactly two arguments";
    }
    return VectorStatementHelperEmitResult::Error;
  }

  const Expr &target = callStmt.args.front();
  if (!isMutableVectorTargetExpr(target)) {
    error = vectorHelper + " requires mutable vector binding";
    return VectorStatementHelperEmitResult::Error;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      error = vectorHelper + " requires mutable vector binding";
      return VectorStatementHelperEmitResult::Error;
    }
    if (it->second.isSoaVector) {
      error = "native backend does not support soa_vector helper: " + vectorHelper;
      return VectorStatementHelperEmitResult::Error;
    }
    if (it->second.kind != LocalInfo::Kind::Vector || !it->second.isMutable) {
      error = vectorHelper + " requires mutable vector binding";
      return VectorStatementHelperEmitResult::Error;
    }
  } else if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") &&
             target.args.size() == 1) {
    auto emitSoaTargetError = [&]() {
      error = "native backend does not support soa_vector helper: " + vectorHelper;
      return VectorStatementHelperEmitResult::Error;
    };
    const Expr &derefTarget = target.args.front();
    if (derefTarget.kind == Expr::Kind::Name) {
      auto it = localsIn.find(derefTarget.name);
      if (it == localsIn.end()) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
      if (it->second.isSoaVector) {
        return emitSoaTargetError();
      }
      if (!((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
            (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector))) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
    } else {
      std::string derefAccessName;
      if (!(getBuiltinArrayAccessName(derefTarget, derefAccessName) && derefTarget.args.size() == 2 &&
            derefTarget.args.front().kind == Expr::Kind::Name)) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
      auto it = localsIn.find(derefTarget.args.front().name);
      if (it == localsIn.end() || !it->second.isArgsPack) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
      if (it->second.isSoaVector) {
        return emitSoaTargetError();
      }
      if (!((it->second.argsPackElementKind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
            (it->second.argsPackElementKind == LocalInfo::Kind::Pointer && it->second.pointerToVector))) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
    }
  } else if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.args.front().name);
      if (it == localsIn.end() || !it->second.isArgsPack) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
      if (it->second.isSoaVector) {
        error = "native backend does not support soa_vector helper: " + vectorHelper;
        return VectorStatementHelperEmitResult::Error;
      }
      if (it->second.argsPackElementKind != LocalInfo::Kind::Vector) {
        error = vectorHelper + " requires mutable vector binding";
        return VectorStatementHelperEmitResult::Error;
      }
    } else if (!(target.isFieldAccess && inferStructExprPath(target, localsIn) == "/vector")) {
      error = vectorHelper + " requires mutable vector binding";
      return VectorStatementHelperEmitResult::Error;
    }
  } else if (!(target.kind == Expr::Kind::Call && target.isFieldAccess &&
               inferStructExprPath(target, localsIn) == "/vector")) {
    error = vectorHelper + " requires mutable vector binding";
    return VectorStatementHelperEmitResult::Error;
  }

  auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
    if (kind == LocalInfo::ValueKind::Int32) {
      instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(value)});
    } else {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
    }
  };

  const int32_t ptrLocal = allocTempLocal();
  constexpr uint64_t kVectorDataPtrOffsetBytes = 2 * IrSlotBytes;
  if (!emitExpr(target, localsIn)) {
    return VectorStatementHelperEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  auto emitLoadVectorDataPtr = [&](int32_t dataPtrLocal) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, kVectorDataPtrOffsetBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataPtrLocal)});
  };

  if (vectorHelper == "clear") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t countLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadIndirect, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  int32_t capacityLocal = -1;
  if (vectorHelper == "push" || vectorHelper == "reserve") {
    capacityLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
  }

  auto emitReallocateVectorData = [&](int32_t desiredLocal, const std::function<void()> &emitAllocationFailure) {
    const int32_t oldDataPtrLocal = allocTempLocal();
    emitLoadVectorDataPtr(oldDataPtrLocal);

    const int32_t newDataPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::HeapAlloc, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(newDataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, 0});
    instructions.push_back({IrOpcode::CmpEqI64, 0});
    const size_t jumpAllocationSucceeded = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitAllocationFailure();
    const size_t allocationSucceededIndex = instructions.size();
    instructions[jumpAllocationSucceeded].imm = static_cast<int32_t>(allocationSucceededIndex);

    const int32_t copyIndexLocal = allocTempLocal();
    const int32_t srcPtrLocal = allocTempLocal();
    const int32_t destPtrLocal = allocTempLocal();
    const int32_t copyValueLocal = allocTempLocal();
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal)});

    const size_t copyLoopStart = instructions.size();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpCopyDone = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(oldDataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyValueLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyValueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal)});
    instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(copyLoopStart)});

    const size_t copyDoneIndex = instructions.size();
    instructions[jumpCopyDone].imm = static_cast<int32_t>(copyDoneIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, kVectorDataPtrOffsetBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
  };

  if (vectorHelper == "reserve") {
    LocalInfo::ValueKind capacityKind = normalizeIndexKind(inferExprKind(callStmt.args[1], localsIn));
    if (!isSupportedIndexKind(capacityKind)) {
      error = "reserve requires integer capacity";
      return VectorStatementHelperEmitResult::Error;
    }
    const Expr &desiredExpr = callStmt.args[1];
    int64_t signedDesired = 0;
    uint64_t unsignedDesired = 0;
    const SignedLiteralIntegerEvalResult signedEvalResult = tryEvaluateSignedLiteralIntegerExpr(desiredExpr, signedDesired);
    if (signedEvalResult == SignedLiteralIntegerEvalResult::Overflow) {
      error = "vector reserve literal expression overflow";
      return VectorStatementHelperEmitResult::Error;
    }
    if (signedEvalResult == SignedLiteralIntegerEvalResult::Value) {
      if (signedDesired < 0) {
        error = "vector reserve expects non-negative capacity";
        return VectorStatementHelperEmitResult::Error;
      }
      if (signedDesired > static_cast<int64_t>(kVectorLocalDynamicCapacityLimit)) {
        error = vectorReserveExceedsLocalCapacityLimitMessage();
        return VectorStatementHelperEmitResult::Error;
      }
    } else {
      const UnsignedLiteralIntegerEvalResult unsignedEvalResult =
          tryEvaluateUnsignedLiteralIntegerExpr(desiredExpr, unsignedDesired);
      if (unsignedEvalResult == UnsignedLiteralIntegerEvalResult::Overflow) {
        error = "vector reserve literal expression overflow";
        return VectorStatementHelperEmitResult::Error;
      }
      if (unsignedEvalResult == UnsignedLiteralIntegerEvalResult::Value &&
          unsignedDesired > static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)) {
        error = vectorReserveExceedsLocalCapacityLimitMessage();
        return VectorStatementHelperEmitResult::Error;
      }
    }

    const int32_t desiredLocal = allocTempLocal();
    if (!emitExpr(callStmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

    if (capacityKind != LocalInfo::ValueKind::UInt64) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
      instructions.push_back({pushZeroForIndex(capacityKind), 0});
      instructions.push_back({cmpLtForIndex(capacityKind), 0});
      size_t jumpNonNegative = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      emitVectorReserveNegative();
      size_t nonNegativeIndex = instructions.size();
      instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
    }

    const IrOpcode cmpLeOp = (capacityKind == LocalInfo::ValueKind::Int32)
                                 ? IrOpcode::CmpLeI32
                                 : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLeI64 : IrOpcode::CmpLeU64);
    const IrOpcode cmpGtOp = (capacityKind == LocalInfo::ValueKind::Int32)
                                 ? IrOpcode::CmpGtI32
                                 : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpGtI64 : IrOpcode::CmpGtU64);
    const IrOpcode pushLimitOp =
        (capacityKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({cmpLeOp, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    const size_t jumpReserveEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({pushLimitOp, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({cmpGtOp, 0});
    const size_t jumpWithinLimit = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorReserveExceeded();
    const size_t withinLimitIndex = instructions.size();
    instructions[jumpWithinLimit].imm = static_cast<int32_t>(withinLimitIndex);

    emitReallocateVectorData(desiredLocal, emitVectorReserveExceeded);

    const size_t reserveEndIndex = instructions.size();
    instructions[jumpReserveEnd].imm = static_cast<int32_t>(reserveEndIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "push") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    const size_t pushBodyIndex = instructions.size();
    const int32_t valueLocal = allocTempLocal();
    if (!emitExpr(callStmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

    const int32_t dataPtrLocal = allocTempLocal();
    emitLoadVectorDataPtr(dataPtrLocal);
    const int32_t destPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    const size_t jumpEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpOom = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    const int32_t desiredLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

    emitReallocateVectorData(desiredLocal, emitVectorCapacityExceeded);

    instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(pushBodyIndex)});

    const size_t errorIndex = instructions.size();
    instructions[jumpOom].imm = static_cast<int32_t>(errorIndex);
    emitVectorCapacityExceeded();
    const size_t endIndex = instructions.size();
    instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "pop") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpEqI32, 0});
    size_t jumpNonEmpty = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorPopOnEmpty();
    size_t nonEmptyIndex = instructions.size();
    instructions[jumpNonEmpty].imm = static_cast<int32_t>(nonEmptyIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(callStmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = vectorHelper + " requires integer index";
    return VectorStatementHelperEmitResult::Error;
  }

  IrOpcode cmpLtOp =
      (indexKind == LocalInfo::ValueKind::Int32)
          ? IrOpcode::CmpLtI32
          : (indexKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtU64);
  IrOpcode addOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32 : IrOpcode::AddI64;
  IrOpcode subOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::SubI32 : IrOpcode::SubI64;
  IrOpcode mulOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::MulI32 : IrOpcode::MulI64;

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(callStmt.args[1], localsIn)) {
    return VectorStatementHelperEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

  if (indexKind != LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    instructions.push_back({pushZeroForIndex(indexKind), 0});
    instructions.push_back({cmpLtForIndex(indexKind), 0});
    size_t jumpNonNegative = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorIndexOutOfBounds();
    size_t nonNegativeIndex = instructions.size();
    instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({cmpGeForIndex(indexKind), 0});
  size_t jumpInRange = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  emitVectorIndexOutOfBounds();
  size_t inRangeIndex = instructions.size();
  instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);

  const int32_t lastIndexLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({subOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(lastIndexLocal)});

  if (vectorHelper == "remove_swap") {
    const int32_t dataPtrLocal = allocTempLocal();
    emitLoadVectorDataPtr(dataPtrLocal);
    const int32_t destPtrLocal = allocTempLocal();
    const int32_t srcPtrLocal = allocTempLocal();
    const int32_t tempValueLocal = allocTempLocal();

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    pushIndexConst(indexKind, IrSlotBytesI32);
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
    pushIndexConst(indexKind, IrSlotBytesI32);
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t destPtrLocal = allocTempLocal();
  const int32_t srcPtrLocal = allocTempLocal();
  const int32_t tempValueLocal = allocTempLocal();
  const int32_t dataPtrLocal = allocTempLocal();
  emitLoadVectorDataPtr(dataPtrLocal);

  const size_t loopStart = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
  instructions.push_back({cmpLtOp, 0});
  size_t jumpLoopEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, IrSlotBytesI32);
  instructions.push_back({mulOp, 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({addOp, 0});
  pushIndexConst(indexKind, IrSlotBytesI32);
  instructions.push_back({mulOp, 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
  instructions.push_back({IrOpcode::LoadIndirect, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({addOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

  size_t loopEndIndex = instructions.size();
  instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::SubI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return VectorStatementHelperEmitResult::Emitted;
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      allocTempLocal,
      inferExprKind,
      [](const Expr &, const LocalMap &) { return std::string(); },
      emitExpr,
      isUserDefinedVectorHelperCall,
      emitVectorCapacityExceeded,
      emitVectorPopOnEmpty,
      emitVectorIndexOutOfBounds,
      emitVectorReserveNegative,
      emitVectorReserveExceeded,
      error);
}

} // namespace primec::ir_lowerer
