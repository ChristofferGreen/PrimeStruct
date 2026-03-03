#include "IrLowererCountAccessHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!hasEntryArgs || expr.kind != Expr::Kind::Name || expr.name != entryArgsName) {
    return false;
  }
  return localsIn.count(entryArgsName) == 0;
}

bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!isSimpleCallName(expr, "count") || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isEntryArgsName(target, localsIn, hasEntryArgs, entryArgsName)) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray;
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Map;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    if (collection == "array" || collection == "vector") {
      return target.templateArgs.size() == 1;
    }
    if (collection == "map") {
      return target.templateArgs.size() == 2;
    }
  }
  return false;
}

bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn) {
  if (!isSimpleCallName(expr, "capacity") || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Vector;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    return collection == "vector" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isStringCountCall(const Expr &expr, const LocalMap &localsIn) {
  if (!isSimpleCallName(expr, "count") || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String;
  }
  return false;
}

} // namespace primec::ir_lowerer
