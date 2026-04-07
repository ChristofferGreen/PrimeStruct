#include "SemanticsHelpers.h"

#include <string_view>
#include <utility>

namespace primec::semantics {

bool isAssignCall(const Expr &expr) {
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
  return name == "assign";
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  const std::string targetName = nameToMatch == nullptr ? std::string() : std::string(nameToMatch);
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("vector/").size()));
    if (alias.find('/') == std::string::npos && alias == "vector") {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias =
        stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/vector/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "count" || alias == "capacity" || alias == "at" || alias == "at_unsafe" || alias == "push" ||
         alias == "pop" || alias == "reserve" || alias == "clear" || alias == "remove_at" ||
         alias == "remove_swap")) {
      return alias == targetName;
    }
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("map/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "count_ref" ||
         alias == "contains" || alias == "contains_ref" || alias == "tryAt" ||
         alias == "tryAt_ref" || alias == "at" || alias == "at_ref" ||
         alias == "at_unsafe" || alias == "at_unsafe_ref" ||
         alias == "insert" || alias == "insert_ref")) {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/map/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "count_ref" ||
         alias == "contains" || alias == "contains_ref" || alias == "tryAt" ||
         alias == "tryAt_ref" || alias == "at" || alias == "at_ref" ||
         alias == "at_unsafe" || alias == "at_unsafe_ref" ||
         alias == "insert" || alias == "insert_ref")) {
      return alias == targetName;
    }
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  name = stripTemplateSpecializationSuffix(name);
  return name == targetName;
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isMatchCall(const Expr &expr) {
  return isSimpleCallName(expr, "match");
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
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
  if (!expr.isMethodCall && expr.namespacePrefix.empty() && isSimpleCallName(expr, "notify")) {
    out.target = PathSpaceTarget::Notify;
    out.name = "notify";
    out.requiredEffect = "pathspace_notify";
    out.argumentCount = 2;
    return true;
  }
  if (!expr.isMethodCall && expr.namespacePrefix.empty() && isSimpleCallName(expr, "insert")) {
    out.target = PathSpaceTarget::Insert;
    out.name = "insert";
    out.requiredEffect = "pathspace_insert";
    out.argumentCount = 2;
    return true;
  }
  if (!expr.isMethodCall && expr.namespacePrefix.empty() && isSimpleCallName(expr, "take")) {
    out.target = PathSpaceTarget::Take;
    out.name = "take";
    out.requiredEffect = "pathspace_take";
    out.argumentCount = 1;
    return true;
  }
  if (!expr.isMethodCall && expr.namespacePrefix.empty() && isSimpleCallName(expr, "bind")) {
    out.target = PathSpaceTarget::Bind;
    out.name = "bind";
    out.requiredEffect = "pathspace_bind";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "unbind")) {
    out.target = PathSpaceTarget::Unbind;
    out.name = "unbind";
    out.requiredEffect = "pathspace_bind";
    out.argumentCount = 1;
    return true;
  }
  if (isSimpleCallName(expr, "schedule")) {
    out.target = PathSpaceTarget::Schedule;
    out.name = "schedule";
    out.requiredEffect = "pathspace_schedule";
    out.argumentCount = 2;
    return true;
  }
  return false;
}

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == name) {
      return namespacePrefix;
    }
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

} // namespace primec::semantics
