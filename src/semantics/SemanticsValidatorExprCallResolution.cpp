#include "SemanticsValidator.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace primec::semantics {

bool SemanticsValidator::validateExprEarlyPointerBuiltin(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprDispatchBootstrap &dispatchBootstrap,
    bool &handledOut) {
  handledOut = false;

  auto isAcceptedLocationTarget = [&](const Expr &target, const auto &self) -> bool {
    if (target.kind == Expr::Kind::Name) {
      return !isEntryArgsName(target.name) &&
             (locals.count(target.name) != 0 || isParam(params, target.name));
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (dispatchBootstrap.isDeclaredPointerLikeCall != nullptr &&
        dispatchBootstrap.isDeclaredPointerLikeCall(target)) {
      return true;
    }
    if (target.isFieldAccess && target.args.size() == 1) {
      return self(target.args.front(), self);
    }
    std::string indexedElemType;
    const auto &dispatchResolvers = dispatchBootstrap.dispatchResolvers;
    const bool resolvesWrappedIndexedArgsPackElement =
        ((dispatchResolvers.resolveIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveIndexedArgsPackElementType(target, indexedElemType)) ||
         (dispatchResolvers.resolveWrappedIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveWrappedIndexedArgsPackElementType(target, indexedElemType)) ||
         (dispatchResolvers.resolveDereferencedIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveDereferencedIndexedArgsPackElementType(target, indexedElemType)));
    if (!resolvesWrappedIndexedArgsPackElement) {
      return false;
    }
    return unwrapReferencePointerTypeText(indexedElemType) != indexedElemType;
  };

  std::string earlyPointerBuiltin;
  if (!getBuiltinPointerName(expr, earlyPointerBuiltin)) {
    return true;
  }

  handledOut = true;
  auto failExprCallResolution = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (hasNamedArguments(expr.argNames)) {
    return failExprCallResolution("named arguments not supported for builtin calls");
  }
  if (!expr.templateArgs.empty()) {
    return failExprCallResolution("pointer helpers do not accept template arguments");
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failExprCallResolution("pointer helpers do not accept block arguments");
  }
  if (expr.args.size() != 1) {
    return failExprCallResolution("argument count mismatch for builtin " + earlyPointerBuiltin);
  }
  if (earlyPointerBuiltin == "location") {
    const Expr &target = expr.args.front();
    if (!isAcceptedLocationTarget(target, isAcceptedLocationTarget)) {
      return failExprCallResolution("location requires a local binding");
    }
  }
  if (earlyPointerBuiltin == "dereference" &&
      !isPointerLikeExpr(expr.args.front(), params, locals) &&
      !dispatchBootstrap.isDeclaredPointerLikeCall(expr.args.front())) {
    return failExprCallResolution("dereference requires a pointer or reference");
  }
  return validateExpr(params, locals, expr.args.front());
}

std::string SemanticsValidator::resolveExprConcreteCallPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &candidatePath) const {
  auto pathExists = [&](const std::string &path) {
    return defMap_.count(path) > 0 || paramsByDef_.count(path) > 0;
  };
  auto hasOverloadFamily = [&](const std::string &path) {
    const std::string overloadPrefix = path + "__ov";
    for (const auto &[candidate, defPtr] : defMap_) {
      (void)defPtr;
      if (candidate.rfind(overloadPrefix, 0) == 0) {
        return true;
      }
    }
    for (const auto &[candidate, candidateParams] : paramsByDef_) {
      (void)candidateParams;
      if (candidate.rfind(overloadPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  auto appendIfMissing = [](std::vector<std::string> &paths, const std::string &path) {
    if (path.empty()) {
      return;
    }
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
      paths.push_back(path);
    }
  };
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : text) {
      hash ^= static_cast<uint64_t>(c);
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  auto stripWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (!std::isspace(static_cast<unsigned char>(c))) {
        out.push_back(c);
      }
    }
    return out;
  };
  auto specializedPathForBase = [&](const std::string &basePath) {
    if (expr.templateArgs.empty()) {
      return std::string{};
    }
    std::ostringstream out;
    out << basePath
        << "__t"
        << std::hex
        << fnv1a64(stripWhitespace(joinTemplateArgs(expr.templateArgs)));
    const std::string directCandidate = out.str();
    if (pathExists(directCandidate)) {
      return directCandidate;
    }
    const std::string specializationPrefix = basePath + "__t";
    std::string singleCandidate;
    auto considerCandidate = [&](const std::string &candidate) {
      if (candidate.rfind(specializationPrefix, 0) != 0) {
        return;
      }
      if (!singleCandidate.empty() && singleCandidate != candidate) {
        singleCandidate.clear();
        singleCandidate = "__ambiguous__";
        return;
      }
      singleCandidate = candidate;
    };
    for (const auto &[candidate, defPtr] : defMap_) {
      (void)defPtr;
      considerCandidate(candidate);
      if (singleCandidate == "__ambiguous__") {
        return std::string{};
      }
    }
    for (const auto &[candidate, candidateParams] : paramsByDef_) {
      (void)candidateParams;
      considerCandidate(candidate);
      if (singleCandidate == "__ambiguous__") {
        return std::string{};
      }
    }
    return singleCandidate == "__ambiguous__" ? std::string{} : singleCandidate;
  };

  std::vector<std::string> baseCandidates;
  const bool hasTemplateOverloads = hasOverloadFamily(candidatePath);
  const bool isTypeNamespaceCall = isTypeNamespaceMethodCall(params, locals, expr, candidatePath);
  const bool hasExplicitReceiverBinding =
      expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name &&
      (findParamBinding(params, expr.args.front().name) != nullptr ||
       locals.count(expr.args.front().name) > 0);
  if (hasTemplateOverloads) {
    if (isTypeNamespaceCall && !expr.args.empty()) {
      appendIfMissing(baseCandidates,
                      candidatePath + "__ov" + std::to_string(expr.args.size() - 1));
    } else if (expr.isMethodCall) {
      const size_t parameterCount =
          hasExplicitReceiverBinding ? expr.args.size() : expr.args.size() + 1;
      appendIfMissing(baseCandidates,
                      candidatePath + "__ov" + std::to_string(parameterCount));
    } else {
      appendIfMissing(baseCandidates,
                      candidatePath + "__ov" + std::to_string(expr.args.size()));
    }
  }
  appendIfMissing(baseCandidates, candidatePath);

  for (const auto &basePath : baseCandidates) {
    if (const std::string specializedPath = specializedPathForBase(basePath);
        !specializedPath.empty()) {
      return specializedPath;
    }
    if (pathExists(basePath)) {
      return basePath;
    }
  }
  return baseCandidates.empty() ? candidatePath : baseCandidates.front();
}

} // namespace primec::semantics
