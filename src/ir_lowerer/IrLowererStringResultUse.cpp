#include "IrLowererStringResultUse.h"

#include "IrLowererRecursionAnalysis.h"
#include "IrLowererStringParameterUse.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

#include <string_view>

namespace primec::ir_lowerer {

namespace {

std::string_view callLeaf(std::string_view name) {
  const size_t separator = name.find_last_of("/.");
  return separator == std::string_view::npos ? name : name.substr(separator + 1);
}

bool isPrintCall(const Expr &call) {
  if (call.isMethodCall) {
    return false;
  }
  const std::string_view leaf = callLeaf(call.name);
  return leaf == "print" || leaf == "print_line" || leaf == "print_error" ||
         leaf == "print_line_error";
}

// Whether the argument at `index` of `call` is a `string` parameter that the real call of its callee reads only as a
// runtime index.
bool callTakesStringIndex(
    const std::unordered_map<std::string, const Definition *> &definitionsByPath,
    const Expr &call,
    size_t index) {
  if (call.isMethodCall || call.resolvedCallPath.empty()) {
    return false;
  }
  const auto calleeIt = definitionsByPath.find(call.resolvedCallPath);
  if (calleeIt == definitionsByPath.end() || calleeIt->second == nullptr ||
      index >= calleeIt->second->parameters.size()) {
    return false;
  }
  const Expr &param = calleeIt->second->parameters[index];
  return !paramHasMutTransform(param) &&
         stringParameterUsedOnlyAsIndexReads(*calleeIt->second, param.name);
}

bool isIndexRead(const std::unordered_map<std::string, const Definition *> &definitionsByPath,
                 const Expr &call,
                 const Expr *parent) {
  if (parent == nullptr || parent->kind != Expr::Kind::Call) {
    return false;
  }
  if (isPrintCall(*parent) || isFileOpenWithPath(*parent)) {
    return true;
  }
  if (parent->args.size() == 1 && callLeaf(parent->name) == "count") {
    return true;
  }
  for (size_t index = 0; index < parent->args.size(); ++index) {
    if (&parent->args[index] == &call) {
      return callTakesStringIndex(definitionsByPath, *parent, index);
    }
  }
  return false;
}

struct StringResultSites {
  const std::unordered_map<std::string, const Definition *> &definitionsByPath;
  const std::string &path;
  std::string_view leaf;
};

bool isCallSite(const StringResultSites &sites, const Expr &expr) {
  return expr.kind == Expr::Kind::Call &&
         (expr.resolvedCallPath == sites.path || callLeaf(expr.name) == sites.leaf);
}

bool sitesAreIndexReads(const StringResultSites &sites, const Expr &expr, const Expr *parent) {
  if (isCallSite(sites, expr) && !isIndexRead(sites.definitionsByPath, expr, parent)) {
    return false;
  }
  for (const Expr &arg : expr.args) {
    if (!sitesAreIndexReads(sites, arg, &expr)) {
      return false;
    }
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    if (!sitesAreIndexReads(sites, bodyArg, &expr)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool hasStringResult(const Definition &def) {
  for (const Transform &transform : def.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      return trimTemplateTypeText(transform.templateArgs.front()) == "string";
    }
  }
  return false;
}

bool stringResultUsedOnlyAsIndexReads(
    const std::unordered_map<std::string, const Definition *> &definitionsByPath,
    const std::unordered_set<std::string> &reachable,
    const Definition &def) {
  const StringResultSites sites{definitionsByPath, def.fullPath, callLeaf(def.fullPath)};
  for (const std::string &path : reachable) {
    const auto callerIt = definitionsByPath.find(path);
    if (callerIt == definitionsByPath.end() || callerIt->second == nullptr) {
      continue;
    }
    const Definition &caller = *callerIt->second;
    for (const Expr &stmt : caller.statements) {
      if (!sitesAreIndexReads(sites, stmt, nullptr)) {
        return false;
      }
    }
    if (caller.returnExpr.has_value() && !sitesAreIndexReads(sites, *caller.returnExpr, nullptr)) {
      return false;
    }
  }
  return true;
}

} // namespace primec::ir_lowerer
