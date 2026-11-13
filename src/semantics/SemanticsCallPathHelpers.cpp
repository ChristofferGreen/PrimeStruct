#include "primec/support/CompileContext.h"
#include "primec/support/CallNames.h"
#include "primec/support/CompileArena.h"
#include "SemanticsHelpers.h"
#include "StdlibCollectionSurfaceHelpers.h"

#include "primec/support/StdlibSurfaceRegistry.h"

#include <string_view>
#include <utility>

namespace primec::semantics {

namespace {

bool resolveCanonicalVectorHelperMemberName(std::string_view path,
                                            std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      vectorHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view normalizedPath = trimLeadingSlash(path);
  const std::string_view canonicalRoot = trimLeadingSlash(metadata->canonicalPath);
  if (normalizedPath.size() <= canonicalRoot.size() ||
      normalizedPath.compare(0, canonicalRoot.size(), canonicalRoot) != 0 ||
      normalizedPath[canonicalRoot.size()] != '/') {
    return false;
  }
  std::string memberPath(normalizedPath.substr(canonicalRoot.size() + 1));
  const size_t slash = memberPath.find('/');
  if (slash != std::string::npos) {
    return false;
  }
  const size_t generatedSuffix = memberPath.find("__t");
  if (generatedSuffix != std::string::npos) {
    memberPath.erase(generatedSuffix);
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, memberPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool resolveKeyValueHelperMemberName(std::string_view path,
                                     std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (memberName.empty() || memberName == "entry") {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

} // namespace

bool isAssignCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name;
  if (!readSingleSegmentName(expr.name, name)) {
    return false;
  }
  return name == "assign";
}

// Semantics policy row: collection helper member spellings decide their own match.
static bool
decideSemanticsCollectionMember(const std::string &name, std::string_view target, bool &matches) {
  std::string member;
  if (resolveCanonicalVectorHelperMemberName(name, member) ||
      resolveKeyValueHelperMemberName(name, member)) {
    matches = member == target;
    return true;
  }
  return false;
}

static bool isSimpleCallNameUncached(const Expr &expr, const char *nameToMatch) {
  static const SimpleCallPolicy policy{false, true, true, decideSemanticsCollectionMember};
  return matchSimpleCallName(expr.name,
                             {},
                             nameToMatch == nullptr ? std::string_view()
                                                    : std::string_view(nameToMatch),
                             policy);
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  // A pure function of (name, target): memoized per compilation because the front end
  // asks it for every expression against dozens of builtin names.
  std::string key;
  key.reserve(expr.name.size() + 24);
  key.push_back('s');
  key.append(expr.name).push_back('\0');
  key.append(nameToMatch == nullptr ? "" : nameToMatch);
  auto &cache = CompileContext::current().nameClassifiers.simpleCallNames;
  if (const auto it = cache.find(key); it != cache.end()) {
    return it->second;
  }
  const bool result = isSimpleCallNameUncached(expr, nameToMatch);
  {
    primec::SystemHeapScope systemHeapGuard;
    cache.emplace(key, result);
  }
  return result;
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::If.data());
}

bool isMatchCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::Match.data());
}

bool isPickCall(const Expr &expr) {
  return !expr.isMethodCall && !expr.isFieldAccess &&
         isSimpleCallName(expr, call_names::Pick.data()) &&
         (expr.hasBodyArguments || !expr.bodyArguments.empty());
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::Return.data());
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::Loop.data());
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::While.data());
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::For.data());
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::Repeat.data());
}

bool isBlockCall(const Expr &expr) {
  return isSimpleCallName(expr, call_names::Block.data());
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
