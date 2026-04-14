#include "IrLowererFlowVectorResolutionInternal.h"

#include "IrLowererHelpers.h"

#include <limits>
#include <utility>

namespace primec::ir_lowerer {

namespace {
bool isVectorStructPath(const std::string &structPath) {
  return structPath == "/vector" ||
         structPath == "/std/collections/experimental_vector/Vector" ||
         structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

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

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

std::string normalizeQualifiedHelperName(const Expr &expr) {
  if (expr.name.empty()) {
    return "";
  }
  std::string normalizedName = expr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (!expr.namespacePrefix.empty() && normalizedName.find('/') == std::string::npos) {
    std::string normalizedPrefix = expr.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    if (!normalizedPrefix.empty()) {
      normalizedName = normalizedPrefix + "/" + normalizedName;
    }
  }
  return normalizedName;
}

bool resolveExperimentalVectorMutatorAliasName(std::string helperName,
                                               std::string &helperNameOut) {
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
  std::string normalized = normalizeQualifiedHelperName(expr);
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string soaVectorPrefix = "soa_vector/";
  const std::string stdSoaVectorPrefix = "std/collections/soa_vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(soaVectorPrefix, 0) == 0) {
    const std::string helperName = stripGeneratedHelperSuffix(normalized.substr(soaVectorPrefix.size()));
    if (helperName == "push" || helperName == "reserve") {
      helperNameOut = helperName;
      return true;
    }
    return false;
  }
  if (normalized.rfind(stdSoaVectorPrefix, 0) == 0) {
    const std::string helperName = stripGeneratedHelperSuffix(normalized.substr(stdSoaVectorPrefix.size()));
    if (helperName == "push" || helperName == "reserve") {
      helperNameOut = helperName;
      return true;
    }
    return false;
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

bool classifyMutableVectorLocal(const LocalInfo &localInfo, bool fromArgsPack) {
  const LocalInfo::Kind kind = fromArgsPack ? localInfo.argsPackElementKind : localInfo.kind;
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
}

bool isMutableVectorTargetExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return false;
    }
    return classifyMutableVectorLocal(it->second, false);
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack) {
        return classifyMutableVectorLocal(it->second, true);
      }
    }
    if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
      const Expr &derefTarget = expr.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        if (it != localsIn.end()) {
          return classifyMutableVectorLocal(it->second, false);
        }
      }
      if (getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.args.front().name);
        if (it != localsIn.end() && it->second.isArgsPack) {
          return classifyMutableVectorLocal(it->second, true);
        }
      }
    }
  }
  return expr.kind == Expr::Kind::Call && expr.isFieldAccess &&
         isVectorStructPath(inferStructExprPath(expr, localsIn));
}

bool validateVectorStatementHelperTarget(
    const Expr &target,
    const std::string &vectorHelper,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    std::string &error) {
  if (!isMutableVectorTargetExpr(target, localsIn, inferStructExprPath)) {
    error = vectorHelper + " requires mutable vector binding";
    return false;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      error = vectorHelper + " requires mutable vector binding";
      return false;
    }
    if (it->second.kind != LocalInfo::Kind::Vector || !it->second.isMutable) {
      error = vectorHelper + " requires mutable vector binding";
      return false;
    }
    return true;
  }
  if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") &&
      target.args.size() == 1) {
    const Expr &derefTarget = target.args.front();
    if (derefTarget.kind == Expr::Kind::Name) {
      auto it = localsIn.find(derefTarget.name);
      if (it == localsIn.end()) {
        error = vectorHelper + " requires mutable vector binding";
        return false;
      }
      if (!((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
            (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector))) {
        error = vectorHelper + " requires mutable vector binding";
        return false;
      }
      return true;
    }

    std::string derefAccessName;
    if (!(getBuiltinArrayAccessName(derefTarget, derefAccessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name)) {
      error = vectorHelper + " requires mutable vector binding";
      return false;
    }
    auto it = localsIn.find(derefTarget.args.front().name);
    if (it == localsIn.end() || !it->second.isArgsPack) {
      error = vectorHelper + " requires mutable vector binding";
      return false;
    }
    if (!((it->second.argsPackElementKind == LocalInfo::Kind::Reference &&
           it->second.referenceToVector) ||
          (it->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
           it->second.pointerToVector))) {
      error = vectorHelper + " requires mutable vector binding";
      return false;
    }
    return true;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.args.front().name);
      if (it == localsIn.end() || !it->second.isArgsPack) {
        error = vectorHelper + " requires mutable vector binding";
        return false;
      }
      if (it->second.isSoaVector) {
        error = vectorHelper + " requires mutable vector binding";
        return false;
      }
      if (it->second.argsPackElementKind != LocalInfo::Kind::Vector) {
        error = vectorHelper + " requires mutable vector binding";
        return false;
      }
      return true;
    }
    if (target.isFieldAccess && isVectorStructPath(inferStructExprPath(target, localsIn))) {
      return true;
    }
    error = vectorHelper + " requires mutable vector binding";
    return false;
  }
  if (target.kind == Expr::Kind::Call && target.isFieldAccess &&
      isVectorStructPath(inferStructExprPath(target, localsIn))) {
    return true;
  }
  error = vectorHelper + " requires mutable vector binding";
  return false;
}

bool isImportedBuiltinSoaMethodMutatorCall(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath) {
  if (!stmt.isMethodCall || stmt.args.empty()) {
    return false;
  }
  if (!(isSimpleCallName(stmt, "push") || isSimpleCallName(stmt, "reserve"))) {
    return false;
  }
  const Expr &target = stmt.args.front();
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (target.kind == Expr::Kind::Call && target.isFieldAccess &&
      inferStructExprPath(target, localsIn) == "/soa_vector") {
    return true;
  }
  return false;
}
} // namespace

SignedLiteralIntegerEvalResult tryEvaluateSignedLiteralIntegerExpr(const Expr &expr, int64_t &out) {
  if (expr.kind == Expr::Kind::Literal && !expr.isUnsigned) {
    out = expr.intWidth == 32 ? static_cast<int32_t>(expr.literalValue)
                              : static_cast<int64_t>(expr.literalValue);
    return SignedLiteralIntegerEvalResult::Value;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      !expr.templateArgs.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return SignedLiteralIntegerEvalResult::NotFoldable;
  }
  if (isSimpleCallName(expr, "negate") && expr.args.size() == 1) {
    int64_t operand = 0;
    const SignedLiteralIntegerEvalResult operandResult =
        tryEvaluateSignedLiteralIntegerExpr(expr.args.front(), operand);
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
  const SignedLiteralIntegerEvalResult lhsResult =
      tryEvaluateSignedLiteralIntegerExpr(expr.args[0], lhs);
  const SignedLiteralIntegerEvalResult rhsResult =
      tryEvaluateSignedLiteralIntegerExpr(expr.args[1], rhs);
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

UnsignedLiteralIntegerEvalResult tryEvaluateUnsignedLiteralIntegerExpr(const Expr &expr,
                                                                      uint64_t &out) {
  if (expr.kind == Expr::Kind::Literal && expr.isUnsigned) {
    out = expr.literalValue;
    return UnsignedLiteralIntegerEvalResult::Value;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      !expr.templateArgs.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || expr.args.size() != 2) {
    return UnsignedLiteralIntegerEvalResult::NotFoldable;
  }
  const bool isPlus = isSimpleCallName(expr, "plus");
  const bool isMinus = isSimpleCallName(expr, "minus");
  if (!isPlus && !isMinus) {
    return UnsignedLiteralIntegerEvalResult::NotFoldable;
  }
  uint64_t lhs = 0;
  uint64_t rhs = 0;
  const UnsignedLiteralIntegerEvalResult lhsResult =
      tryEvaluateUnsignedLiteralIntegerExpr(expr.args[0], lhs);
  const UnsignedLiteralIntegerEvalResult rhsResult =
      tryEvaluateUnsignedLiteralIntegerExpr(expr.args[1], rhs);
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

VectorStatementHelperPrepareResult prepareVectorStatementHelperCall(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    PreparedVectorStatementHelperCall &out,
    std::string &error) {
  std::string vectorHelper;
  if (!resolveVectorMutatorName(stmt, vectorHelper)) {
    return VectorStatementHelperPrepareResult::NotMatched;
  }
  const bool explicitVectorHelperPath =
      !stmt.isMethodCall &&
      (stmt.name.rfind("/std/collections/vector/", 0) == 0);
  Expr normalizedStmt = stmt;
  const Expr *activeStmt = &stmt;
  bool useBuiltinCompatibilityStmt = false;

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
        (stmt.args.front().kind == Expr::Kind::Literal ||
         stmt.args.front().kind == Expr::Kind::BoolLiteral ||
         stmt.args.front().kind == Expr::Kind::FloatLiteral ||
         stmt.args.front().kind == Expr::Kind::StringLiteral ||
         (stmt.args.front().kind == Expr::Kind::Name &&
          !isMutableVectorTargetExpr(stmt.args.front(), localsIn, inferStructExprPath)));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      if (receiverIndex >= stmt.args.size()) {
        continue;
      }
      if (explicitVectorHelperPath &&
          isMutableVectorTargetExpr(stmt.args[receiverIndex], localsIn, inferStructExprPath)) {
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
        return VectorStatementHelperPrepareResult::NotMatched;
      }
    }
  }

  const Expr &callStmt = *activeStmt;
  if (!useBuiltinCompatibilityStmt && isUserDefinedVectorHelperCall(stmt)) {
    return VectorStatementHelperPrepareResult::NotMatched;
  }
  if (!callStmt.templateArgs.empty()) {
    error = vectorHelper + " does not accept template arguments";
    return VectorStatementHelperPrepareResult::Error;
  }
  if (callStmt.hasBodyArguments || !callStmt.bodyArguments.empty()) {
    error = vectorHelper + " does not accept block arguments";
    return VectorStatementHelperPrepareResult::Error;
  }

  const size_t expectedArgs = (vectorHelper == "pop" || vectorHelper == "clear") ? 1 : 2;
  if (callStmt.args.size() != expectedArgs) {
    error = expectedArgs == 1 ? vectorHelper + " requires exactly one argument"
                              : vectorHelper + " requires exactly two arguments";
    return VectorStatementHelperPrepareResult::Error;
  }

  if (isImportedBuiltinSoaMethodMutatorCall(callStmt, localsIn, inferStructExprPath)) {
    error = "struct parameter type mismatch for /std/collections/soa_vector/" + vectorHelper +
            " parameter values: expected /std/collections/experimental_soa_vector/SoaVector__ specialization";
    return VectorStatementHelperPrepareResult::Error;
  }

  if (!validateVectorStatementHelperTarget(callStmt.args.front(),
                                           vectorHelper,
                                           localsIn,
                                           inferStructExprPath,
                                           error)) {
    return VectorStatementHelperPrepareResult::Error;
  }

  out.vectorHelper = std::move(vectorHelper);
  out.callStmt = callStmt;
  return VectorStatementHelperPrepareResult::Ready;
}

} // namespace primec::ir_lowerer
