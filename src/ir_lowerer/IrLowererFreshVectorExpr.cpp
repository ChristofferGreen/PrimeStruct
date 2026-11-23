#include "primec/ir_lowerer/IrLowererFlowHelpers.h"

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

#include <algorithm>
#include <string>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

// Which expressions make a fresh builtin vector nobody else owns, as opposed to reading an element or a
// field a vector keeps owning (docs/spec/value-lifecycle.md, Ownership).
namespace primec::ir_lowerer {

bool isUserShadowedCollectionCall(
    const Expr &expr, const std::function<const Definition *(const Expr &)> &resolveCallee) {
  const Definition *callee = resolveCallee ? resolveCallee(expr) : nullptr;
  return callee != nullptr && callee->fullPath.rfind("/std/", 0) != 0;
}

bool borrowedVectorElementAccessName(const std::string &path, std::string &accessName) {
  std::string base = path;
  base.resize(std::min(base.find("__t"), base.size()));
  if (base == collection_helpers::kCanonicalVectorAtRef) {
    accessName = "at";
    return true;
  }
  if (base == collection_helpers::kCanonicalVectorAtUnsafeRef) {
    accessName = "at_unsafe";
    return true;
  }
  return false;
}

bool isStdlibVectorValueReaderPath(const std::string &path, size_t argCount) {
  std::string base = path;
  base.resize(std::min(base.find("__t"), base.size()));
  if (argCount == 2) {
    return base == collection_helpers::kCanonicalVectorAt ||
           base == collection_helpers::kCanonicalVectorAtUnsafe;
  }
  return argCount == 1 && (base == collection_helpers::kCanonicalVectorCount ||
                           base == collection_helpers::kCanonicalVectorCapacity);
}

bool isFreshBuiltinVectorExpr(
    const Expr &expr, const std::function<const Definition *(const Expr &)> &resolveCallee) {
  if (expr.kind != Expr::Kind::Call || expr.isFieldAccess) {
    return false;
  }
  if (!expr.isMethodCall) {
    std::string collectionName;
    if (getBuiltinCollectionName(expr, collectionName) &&
        !isUserShadowedCollectionCall(expr, resolveCallee)) {
      return collectionName == "vector";
    }
    // A `pick` expression that yields a vector builds a record of its own (`tryEmitPickExpr`).
    if (isSimpleCallName(expr, "pick") && expr.args.size() == 1 && !expr.bodyArguments.empty()) {
      return true;
    }
  }
  const Definition *callee = resolveCallee ? resolveCallee(expr) : nullptr;
  if (callee == nullptr) {
    return false;
  }
  if (callee->fullPath.rfind("/std/", 0) == 0) {
    std::string leaf = callee->fullPath.substr(callee->fullPath.find_last_of('/') + 1);
    leaf.erase(std::min(leaf.find("__"), leaf.size()));
    if (callee->fullPath.rfind(vectorBackingMemberRoot(), 0) == 0 && leaf == "vector") {
      return true;
    }
  }
  // `rows[i]` of a `Reference<vector<vector<T>>>` is an element the vector keeps owning, not a fresh vector.
  if (std::string accessName; borrowedVectorElementAccessName(callee->fullPath, accessName)) {
    return false;
  }
  // A definition returning `vector<T>` hands over a vector of its own, whether the user's or the stdlib's.
  for (const auto &transform : callee->transforms) {
    std::string base;
    std::string argText;
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        splitTemplateTypeName(
            trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
      return normalizeCollectionBindingTypeName(trimTemplateTypeText(base)) == "vector";
    }
  }
  return false;
}

} // namespace primec::ir_lowerer
