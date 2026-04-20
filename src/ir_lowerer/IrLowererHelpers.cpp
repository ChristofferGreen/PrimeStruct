#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isScopedBuiltinControlAlias(const std::string &name) {
  return name == "return" || name == "then" || name == "else" ||
         name == "if" || name == "block" || name == "loop" ||
         name == "while" || name == "for" || name == "repeat" ||
         name == "do";
}

std::string normalizeInternalSoaStorageBuiltinAlias(std::string name) {
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  const std::string prefix = "std/collections/internal_soa_storage/";
  if (name.rfind(prefix, 0) == 0) {
    std::string alias = name.substr(prefix.size());
    const size_t slash = alias.find_last_of('/');
    if (slash != std::string::npos) {
      alias = alias.substr(slash + 1);
    }
    const size_t generatedSuffix = alias.find("__");
    if (generatedSuffix != std::string::npos) {
      alias.erase(generatedSuffix);
    }
    return alias;
  }
  std::string alias = name;
  const size_t slash = alias.find_last_of('/');
  if (slash != std::string::npos) {
    alias = alias.substr(slash + 1);
  }
  if (isScopedBuiltinControlAlias(alias)) {
    return alias;
  }
  return name;
}

} // namespace

std::string vectorLocalCapacityLimitExceededMessage() {
  return "vector local capacity limit exceeded (" + std::to_string(kVectorLocalDynamicCapacityLimit) + ")";
}

std::string vectorReserveExceedsLocalCapacityLimitMessage() {
  return "vector reserve exceeds local capacity limit (" + std::to_string(kVectorLocalDynamicCapacityLimit) + ")";
}

std::string vectorLiteralExceedsLocalCapacityLimitMessage() {
  return "vector literal exceeds local capacity limit (" + std::to_string(kVectorLocalDynamicCapacityLimit) + ")";
}

std::string vectorPushAllocationFailedMessage() {
  return "vector push allocation failed (out of memory)";
}

std::string vectorReserveAllocationFailedMessage() {
  return "vector reserve allocation failed (out of memory)";
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::string targetName = nameToMatch == nullptr ? std::string() : std::string(nameToMatch);
  auto isInternalSoaStorageBareBuiltin = [](const std::string &name) {
    return name == "assign" || name == "if" || name == "while" || name == "take" ||
           name == "borrow" || name == "init" || name == "drop" || name == "increment" ||
           name == "decrement" || name == "return" || name == "then" || name == "else" ||
           name == "do" || name == "block" || name == "loop" || name == "for" ||
           name == "repeat" || name == "location" || name == "dereference";
  };
  std::string name = expr.name;
  if (!expr.namespacePrefix.empty() && name.find('/') == std::string::npos) {
    std::string scopedPrefix = expr.namespacePrefix;
    if (!scopedPrefix.empty() && scopedPrefix.front() != '/') {
      scopedPrefix.insert(scopedPrefix.begin(), '/');
    }
    if (!scopedPrefix.empty()) {
      name = scopedPrefix + "/" + name;
    }
  }
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "count" || alias == "capacity" || alias == "at" || alias == "at_unsafe" || alias == "push" ||
         alias == "pop" || alias == "reserve" || alias == "clear" || alias == "remove_at" ||
         alias == "remove_swap")) {
      return alias == targetName;
    }
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/map/").size());
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  const std::string internalSoaAlias =
      normalizeInternalSoaStorageBuiltinAlias(name);
  if (internalSoaAlias.find('/') == std::string::npos &&
      isInternalSoaStorageBareBuiltin(internalSoaAlias)) {
    return internalSoaAlias == targetName;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == targetName;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isMatchCall(const Expr &expr) {
  return isSimpleCallName(expr, "match");
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool isBlockCall(const Expr &expr) {
  return isSimpleCallName(expr, "block");
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error) {
  if (!isMatchCall(expr)) {
    out = expr;
    return true;
  }
  if (hasNamedArguments(expr.argNames)) {
    error = "named arguments not supported for builtin calls";
    return false;
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    error = "match does not accept trailing block arguments";
    return false;
  }
  if (expr.args.size() < 3) {
    error = "match requires value, cases, else";
    return false;
  }
  auto isBlockEnvelope = [&](const Expr &candidate, size_t expectedArgs) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (candidate.args.size() != expectedArgs) {
      return false;
    }
    if (!candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto wrapInBlockEnvelope = [&](const Expr &value) -> Expr {
    Expr block;
    block.kind = Expr::Kind::Call;
    block.name = "else";
    block.namespacePrefix = expr.namespacePrefix;
    block.hasBodyArguments = true;
    block.bodyArguments = {value};
    return block;
  };
  if (expr.args.size() == 3 && isBlockEnvelope(expr.args[1], 0) && isBlockEnvelope(expr.args[2], 0)) {
    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.namespacePrefix = expr.namespacePrefix;
    ifCall.transforms = expr.transforms;
    ifCall.args = {expr.args[0], expr.args[1], expr.args[2]};
    ifCall.argNames = {std::nullopt, std::nullopt, std::nullopt};
    out = std::move(ifCall);
    return true;
  }

  const Expr &elseArg = expr.args.back();
  if (!isBlockEnvelope(elseArg, 0)) {
    error = "match requires final else block";
    return false;
  }
  for (size_t i = 1; i + 1 < expr.args.size(); ++i) {
    if (!isBlockEnvelope(expr.args[i], 1)) {
      error = "match cases require pattern and block";
      return false;
    }
  }

  Expr currentElse = elseArg;
  for (size_t i = expr.args.size() - 1; i-- > 1;) {
    const Expr &caseExpr = expr.args[i];
    Expr branch = caseExpr;
    branch.args.clear();
    branch.argNames.clear();
    Expr cond;
    cond.kind = Expr::Kind::Call;
    cond.name = "equal";
    cond.namespacePrefix = expr.namespacePrefix;
    cond.args = {expr.args[0], caseExpr.args.front()};
    cond.argNames = {std::nullopt, std::nullopt};

    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.namespacePrefix = expr.namespacePrefix;
    Expr elseBranch = currentElse;
    if (!isBlockEnvelope(elseBranch, 0)) {
      elseBranch = wrapInBlockEnvelope(currentElse);
    }
    ifCall.args = {std::move(cond), std::move(branch), std::move(elseBranch)};
    ifCall.argNames = {std::nullopt, std::nullopt, std::nullopt};
    currentElse = std::move(ifCall);
  }
  currentElse.transforms = expr.transforms;
  currentElse.namespacePrefix = expr.namespacePrefix;
  out = std::move(currentElse);
  return true;
}

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  base.clear();
  arg.clear();
  const size_t open = text.find('<');
  if (open == std::string::npos || open == 0 || text.back() != '>') {
    return false;
  }
  base = text.substr(0, open);
  int depth = 0;
  size_t start = open + 1;
  for (size_t i = start; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      depth++;
      continue;
    }
    if (c == '>') {
      if (depth == 0) {
        if (i + 1 != text.size()) {
          return false;
        }
        arg = text.substr(start, i - start);
        return true;
      }
      depth--;
    }
  }
  return false;
}

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out) {
  if (isSimpleCallName(expr, "print")) {
    out.target = PrintTarget::Out;
    out.newline = false;
    out.name = "print";
    return true;
  }
  if (isSimpleCallName(expr, "print_line")) {
    out.target = PrintTarget::Out;
    out.newline = true;
    out.name = "print_line";
    return true;
  }
  if (isSimpleCallName(expr, "print_error")) {
    out.target = PrintTarget::Err;
    out.newline = false;
    out.name = "print_error";
    return true;
  }
  if (isSimpleCallName(expr, "print_line_error")) {
    out.target = PrintTarget::Err;
    out.newline = true;
    out.name = "print_line_error";
    return true;
  }
  return false;
}

bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out) {
  if (isSimpleCallName(expr, "notify")) {
    out.name = "notify";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "insert")) {
    out.name = "insert";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "take")) {
    out.name = "take";
    out.argumentCount = 1;
    return true;
  }
  if (isSimpleCallName(expr, "bind")) {
    out.name = "bind";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "unbind")) {
    out.name = "unbind";
    out.argumentCount = 1;
    return true;
  }
  if (isSimpleCallName(expr, "schedule")) {
    out.name = "schedule";
    out.argumentCount = 2;
    return true;
  }
  return false;
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

} // namespace primec::ir_lowerer
