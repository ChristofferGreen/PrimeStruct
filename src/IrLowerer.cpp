#include "primec/IrLowerer.h"

#include <functional>
#include <limits>
#include <unordered_map>

namespace primec {
namespace {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isThenCall(const Expr &expr) {
  return isSimpleCallName(expr, "then");
}

bool isElseCall(const Expr &expr) {
  return isSimpleCallName(expr, "else");
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinClampName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinConvertName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "convert";
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "dereference" || name == "location") {
    out = name;
    return true;
  }
  return false;
}

} // namespace

bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "native backend requires entry definition " + entryPath;
    return false;
  }
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name == "return" && transform.templateArg && *transform.templateArg == "void") {
      returnsVoid = true;
      break;
    }
  }

  IrFunction function;
  function.name = entryPath;
  bool sawReturn = false;
  struct LocalInfo {
    int32_t index = 0;
    bool isMutable = false;
    enum class Kind { Value, Pointer, Reference } kind = Kind::Value;
    enum class ValueKind { Unknown, Int32, Int64, UInt64, Bool } valueKind = ValueKind::Unknown;
  };
  using LocalMap = std::unordered_map<std::string, LocalInfo>;
  LocalMap locals;
  int32_t nextLocal = 0;

  auto isBindingMutable = [](const Expr &expr) -> bool {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  };

  auto isBindingQualifierName = [](const std::string &name) -> bool {
    return name == "public" || name == "private" || name == "package" || name == "static" || name == "mut" ||
           name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes";
  };

  auto valueKindFromTypeName = [](const std::string &name) -> LocalInfo::ValueKind {
    if (name == "int" || name == "i32") {
      return LocalInfo::ValueKind::Int32;
    }
    if (name == "i64") {
      return LocalInfo::ValueKind::Int64;
    }
    if (name == "u64") {
      return LocalInfo::ValueKind::UInt64;
    }
    if (name == "bool") {
      return LocalInfo::ValueKind::Bool;
    }
    return LocalInfo::ValueKind::Unknown;
  };

  auto bindingKind = [](const Expr &expr) -> LocalInfo::Kind {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "Reference") {
        return LocalInfo::Kind::Reference;
      }
      if (transform.name == "Pointer") {
        return LocalInfo::Kind::Pointer;
      }
    }
    return LocalInfo::Kind::Value;
  };

  auto bindingValueKind = [&](const Expr &expr, LocalInfo::Kind kind) -> LocalInfo::ValueKind {
    for (const auto &transform : expr.transforms) {
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.name == "Pointer" || transform.name == "Reference") {
        if (transform.templateArg) {
          return valueKindFromTypeName(*transform.templateArg);
        }
        return LocalInfo::ValueKind::Unknown;
      }
      LocalInfo::ValueKind kindValue = valueKindFromTypeName(transform.name);
      if (kindValue != LocalInfo::ValueKind::Unknown) {
        return kindValue;
      }
    }
    if (kind != LocalInfo::Kind::Value) {
      return LocalInfo::ValueKind::Unknown;
    }
    return LocalInfo::ValueKind::Unknown;
  };

  auto combineNumericKinds = [&](LocalInfo::ValueKind left,
                                 LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
      return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
                 ? LocalInfo::ValueKind::UInt64
                 : LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
      if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
          (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
        return LocalInfo::ValueKind::Int64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
      return LocalInfo::ValueKind::Int32;
    }
    return LocalInfo::ValueKind::Unknown;
  };

  auto comparisonKind = [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) -> LocalInfo::ValueKind {
    if (left == LocalInfo::ValueKind::Bool) {
      left = LocalInfo::ValueKind::Int32;
    }
    if (right == LocalInfo::ValueKind::Bool) {
      right = LocalInfo::ValueKind::Int32;
    }
    return combineNumericKinds(left, right);
  };

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
  inferPointerTargetKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (expr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return LocalInfo::ValueKind::Unknown;
      }
      if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
        return it->second.valueKind;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto it = localsIn.find(target.name);
          if (it != localsIn.end()) {
            return it->second.valueKind;
          }
        }
        return LocalInfo::ValueKind::Unknown;
      }
      std::string builtin;
      if (getBuiltinOperatorName(expr, builtin) && (builtin == "plus" || builtin == "minus") &&
          expr.args.size() == 2) {
        std::function<bool(const Expr &)> isPointerExpr;
        isPointerExpr = [&](const Expr &candidate) -> bool {
          if (candidate.kind == Expr::Kind::Name) {
            auto it = localsIn.find(candidate.name);
            return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer;
          }
          if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
            return true;
          }
          if (candidate.kind == Expr::Kind::Call) {
            std::string innerName;
            if (getBuiltinOperatorName(candidate, innerName) && (innerName == "plus" || innerName == "minus") &&
                candidate.args.size() == 2) {
              return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
            }
          }
          return false;
        };
        if (isPointerExpr(expr.args[0]) && !isPointerExpr(expr.args[1])) {
          return inferPointerTargetKind(expr.args[0], localsIn);
        }
      }
    }
    return LocalInfo::ValueKind::Unknown;
  };

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  inferExprKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return LocalInfo::ValueKind::UInt64;
        }
        if (expr.intWidth == 64) {
          return LocalInfo::ValueKind::Int64;
        }
        return LocalInfo::ValueKind::Int32;
      case Expr::Kind::BoolLiteral:
        return LocalInfo::ValueKind::Bool;
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          return LocalInfo::ValueKind::Unknown;
        }
        if (it->second.kind == LocalInfo::Kind::Value || it->second.kind == LocalInfo::Kind::Reference) {
          return it->second.valueKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      case Expr::Kind::Call: {
        std::string builtin;
        if (getBuiltinComparisonName(expr, builtin)) {
          return LocalInfo::ValueKind::Bool;
        }
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferExprKind(expr.args.front(), localsIn);
          }
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto left = inferExprKind(expr.args[0], localsIn);
          auto right = inferExprKind(expr.args[1], localsIn);
          return combineNumericKinds(left, right);
        }
        if (getBuiltinClampName(expr)) {
          if (expr.args.size() != 3) {
            return LocalInfo::ValueKind::Unknown;
          }
          auto first = inferExprKind(expr.args[0], localsIn);
          auto second = inferExprKind(expr.args[1], localsIn);
          auto third = inferExprKind(expr.args[2], localsIn);
          return combineNumericKinds(combineNumericKinds(first, second), third);
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const std::string &typeName = expr.templateArgs.front();
          return valueKindFromTypeName(typeName);
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end()) {
              return it->second.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1) {
            return inferPointerTargetKind(target.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (builtin == "dereference") {
            if (expr.args.size() != 1) {
              return LocalInfo::ValueKind::Unknown;
            }
            return inferPointerTargetKind(expr.args.front(), localsIn);
          }
          return LocalInfo::ValueKind::Unknown;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };

  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  auto allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };

  auto emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects then/else blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on then/else blocks";
      return false;
    }
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    return true;
  };

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PushI64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
      return true;
    }
    error = "boolean conversion requires numeric operand";
    return false;
  };

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        if (expr.intWidth == 64 || expr.isUnsigned) {
          IrInstruction inst;
          inst.op = IrOpcode::PushI64;
          inst.imm = expr.literalValue;
          function.instructions.push_back(inst);
          return true;
        }
        int64_t value = static_cast<int64_t>(expr.literalValue);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
          error = "i32 literal out of range for native backend";
          return false;
        }
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(value);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::BoolLiteral: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = expr.boolValue ? 1 : 0;
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it == localsIn.end()) {
          error = "native backend does not know identifier: " + expr.name;
          return false;
        }
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        if (it->second.kind == LocalInfo::Kind::Reference) {
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
        }
        return true;
      }
      case Expr::Kind::Call: {
        std::string builtin;
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              error = "negate requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (kind == LocalInfo::ValueKind::Bool || kind == LocalInfo::ValueKind::Unknown) {
              error = "negate requires numeric operand";
              return false;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              error = "negate does not support unsigned operands";
              return false;
            }
            function.instructions.push_back({kind == LocalInfo::ValueKind::Int64 ? IrOpcode::NegI64 : IrOpcode::NegI32,
                                             0});
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          std::function<bool(const Expr &, const LocalMap &)> isPointerOperand;
          isPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
            if (candidate.kind == Expr::Kind::Name) {
              auto it = localsRef.find(candidate.name);
              return it != localsRef.end() && it->second.kind == LocalInfo::Kind::Pointer;
            }
            if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
              return true;
            }
            if (candidate.kind == Expr::Kind::Call) {
              std::string opName;
              if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
                  candidate.args.size() == 2) {
                return isPointerOperand(candidate.args[0], localsRef) && !isPointerOperand(candidate.args[1], localsRef);
              }
            }
            return false;
          };
          bool leftPointer = false;
          bool rightPointer = false;
          if (builtin == "plus" || builtin == "minus") {
            leftPointer = isPointerOperand(expr.args[0], localsIn);
            rightPointer = isPointerOperand(expr.args[1], localsIn);
            if (leftPointer && rightPointer) {
              error = "pointer arithmetic does not support pointer + pointer";
              return false;
            }
            if (rightPointer) {
              error = "pointer arithmetic requires pointer on the left";
              return false;
            }
            if (leftPointer || rightPointer) {
              const Expr &offsetExpr = leftPointer ? expr.args[1] : expr.args[0];
              LocalInfo::ValueKind offsetKind = inferExprKind(offsetExpr, localsIn);
              if (offsetKind != LocalInfo::ValueKind::Int32 && offsetKind != LocalInfo::ValueKind::Int64 &&
                  offsetKind != LocalInfo::ValueKind::UInt64) {
                error = "pointer arithmetic requires an integer offset";
                return false;
              }
            }
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (numericKind == LocalInfo::ValueKind::Unknown && !(leftPointer || rightPointer)) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::AddI32;
          if (builtin == "plus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::AddI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::AddI64;
            } else {
              op = IrOpcode::AddI32;
            }
          } else if (builtin == "minus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::SubI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::SubI64;
            } else {
              op = IrOpcode::SubI32;
            }
          } else if (builtin == "multiply") {
            op = (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64)
                     ? IrOpcode::MulI64
                     : IrOpcode::MulI32;
          } else if (builtin == "divide") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::DivU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::DivI64;
            } else {
              op = IrOpcode::DivI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
        if (getBuiltinComparisonName(expr, builtin)) {
          if (builtin == "not") {
            if (expr.args.size() != 1) {
              error = "not requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            return emitCompareToZero(kind, true);
          }
          if (builtin == "and") {
            if (expr.args.size() != 2) {
              error = "and requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpFalse = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t falseIndex = function.instructions.size();
            function.instructions[jumpFalse].imm = static_cast<int32_t>(falseIndex);
            function.instructions.push_back({IrOpcode::PushI32, 0});
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (builtin == "or") {
            if (expr.args.size() != 2) {
              error = "or requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpEval = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t evalIndex = function.instructions.size();
            function.instructions[jumpEval].imm = static_cast<int32_t>(evalIndex);
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind =
              comparisonKind(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (numericKind == LocalInfo::ValueKind::Unknown) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::CmpEqI32;
          if (builtin == "equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpEqI64
                     : IrOpcode::CmpEqI32;
          } else if (builtin == "not_equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpNeI64
                     : IrOpcode::CmpNeI32;
          } else if (builtin == "less_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLtI64;
            } else {
              op = IrOpcode::CmpLtI32;
            }
          } else if (builtin == "less_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLeI64;
            } else {
              op = IrOpcode::CmpLeI32;
            }
          } else if (builtin == "greater_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGtI64;
            } else {
              op = IrOpcode::CmpGtI32;
            }
          } else if (builtin == "greater_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGeI64;
            } else {
              op = IrOpcode::CmpGeI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
        if (getBuiltinClampName(expr)) {
          if (expr.args.size() != 3) {
            error = "clamp requires exactly three arguments";
            return false;
          }
          bool sawUnsigned = false;
          bool sawSigned = false;
          for (const auto &arg : expr.args) {
            LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
            if (arg.kind == Expr::Kind::Literal && arg.isUnsigned) {
              sawUnsigned = true;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              sawUnsigned = true;
            } else if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64) {
              sawSigned = true;
            }
          }
          if (sawUnsigned && sawSigned) {
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind clampKind =
              combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                      inferExprKind(expr.args[1], localsIn)),
                                  inferExprKind(expr.args[2], localsIn));
          if (clampKind == LocalInfo::ValueKind::Unknown) {
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          IrOpcode cmpLt = IrOpcode::CmpLtI32;
          IrOpcode cmpGt = IrOpcode::CmpGtI32;
          if (clampKind == LocalInfo::ValueKind::UInt64) {
            cmpLt = IrOpcode::CmpLtU64;
            cmpGt = IrOpcode::CmpGtU64;
          } else if (clampKind == LocalInfo::ValueKind::Int64) {
            cmpLt = IrOpcode::CmpLtI64;
            cmpGt = IrOpcode::CmpGtI64;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempMin = allocTempLocal();
          int32_t tempMax = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMin)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMax)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({cmpLt, 0});
          size_t skipMin = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMax = function.instructions.size();
          function.instructions[skipMin].imm = static_cast<int32_t>(checkMax);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({cmpGt, 0});
          size_t skipMax = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t useValue = function.instructions.size();
          function.instructions[skipMax].imm = static_cast<int32_t>(useValue);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "convert requires exactly one argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs.front();
          if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" && typeName != "bool") {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, or "
                    "convert<bool>";
            return false;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (typeName == "bool") {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (expr.args.size() != 1) {
            error = builtin + " requires exactly one argument";
            return false;
          }
          if (builtin == "location") {
            const Expr &target = expr.args.front();
            if (target.kind != Expr::Kind::Name) {
              error = "location requires a local name";
              return false;
            }
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "location requires a known local: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
            }
            return true;
          }
          const Expr &pointerExpr = expr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it == localsIn.end()) {
              error = "native backend does not know identifier: " + pointerExpr.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Value) {
              error = "dereference requires a pointer or reference";
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          } else {
            if (!emitExpr(pointerExpr, localsIn)) {
              return false;
            }
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
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
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreIndirect, 0});
              return true;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::Dup, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
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
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreIndirect, 0});
            return true;
          }
          error = "native backend only supports assign to local names or dereference";
          return false;
        }
        if (isIfCall(expr) || isThenCall(expr) || isElseCall(expr)) {
          error = "native backend does not support if/then/else in expression context";
          return false;
        }
        error = "native backend only supports arithmetic/comparison/clamp/convert/pointer/assign calls in expressions";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (localsIn.count(stmt.name) > 0) {
        error = "binding redefines existing name: " + stmt.name;
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo info;
      info.index = nextLocal++;
      info.isMutable = isBindingMutable(stmt);
      info.kind = bindingKind(stmt);
      info.valueKind = bindingValueKind(stmt, info.kind);
      if (info.kind == LocalInfo::Kind::Reference) {
        const Expr &init = stmt.args.front();
        if (init.kind != Expr::Kind::Call || !isSimpleCallName(init, "location") || init.args.size() != 1) {
          error = "reference binding requires location(...) initializer";
          return false;
        }
      }
      localsIn.emplace(stmt.name, info);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
      return true;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.empty()) {
        if (!returnsVoid) {
          error = "return requires exactly one argument";
          return false;
        }
        function.instructions.push_back({IrOpcode::ReturnVoid, 0});
        sawReturn = true;
        return true;
      }
      if (stmt.args.size() != 1) {
        error = "return requires exactly one argument";
        return false;
      }
      if (returnsVoid) {
        error = "return value not allowed for void definition";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
      if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64) {
        function.instructions.push_back({IrOpcode::ReturnI64, 0});
      } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
        function.instructions.push_back({IrOpcode::ReturnI32, 0});
      } else {
        error = "native backend only supports returning numeric or bool values";
        return false;
      }
      sawReturn = true;
      return true;
    }
    if (isIfCall(stmt)) {
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (!emitExpr(stmt.args[0], localsIn)) {
        return false;
      }
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      if (!isThenCall(thenBlock) || !isElseCall(elseBlock)) {
        error = "if requires then/else blocks";
        return false;
      }
      size_t jumpIfZeroIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});
      LocalMap thenLocals = localsIn;
      if (!emitBlock(thenBlock, thenLocals)) {
        return false;
      }
      size_t jumpIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::Jump, 0});
      size_t elseIndex = function.instructions.size();
      function.instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
      LocalMap elseLocals = localsIn;
      if (!emitBlock(elseBlock, elseLocals)) {
        return false;
      }
      size_t endIndex = function.instructions.size();
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
      if (!emitExpr(stmt, localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::Pop, 0});
      return true;
    }
    if (!emitExpr(stmt, localsIn)) {
      return false;
    }
    function.instructions.push_back({IrOpcode::Pop, 0});
    return true;
  };

  for (const auto &stmt : entryDef->statements) {
    if (!emitStatement(stmt, locals)) {
      return false;
    }
  }

  if (!sawReturn) {
    if (returnsVoid) {
      function.instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return false;
    }
  }

  out.functions.push_back(std::move(function));
  out.entryIndex = 0;
  return true;
}

} // namespace primec
