#include "IrLowererStringParameterUse.h"

#include <string_view>

namespace primec::ir_lowerer {

bool isFileOpenWithPath(const Expr &call) {
  if (call.kind != Expr::Kind::Call || call.isMethodCall || call.args.size() != 1) {
    return false;
  }
  if (call.templateArgs.size() == 1 && (call.name == "File" || call.name == "/std/file/File")) {
    return true;
  }
  static constexpr std::string_view OpenHelpers[] = {"/File/openRead",
                                                     "/File/openWrite",
                                                     "/File/openAppend",
                                                     "/File/open_read",
                                                     "/File/open_write",
                                                     "/File/open_append"};
  for (const std::string_view helper : OpenHelpers) {
    if (call.name == helper) {
      return true;
    }
  }
  return false;
}

namespace {

// Whether every use of the `string` parameter `name` in `expr` reads its runtime index: see
// stringParameterUsedOnlyAsIndexReads.
bool stringParameterUseIsIndexRead(const Expr &expr, const std::string &name, const Expr *parent) {
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    if (parent == nullptr || parent->kind != Expr::Kind::Call || parent->args.empty() ||
        &parent->args.front() != &expr) {
      return false;
    }
    if (isFileOpenWithPath(*parent)) {
      return true;
    }
    if (parent->args.size() != 1) {
      return false;
    }
    const size_t slash = parent->name.rfind('/');
    const std::string_view leaf = slash == std::string::npos
                                      ? std::string_view(parent->name)
                                      : std::string_view(parent->name).substr(slash + 1);
    return leaf == "count" || leaf == "__pstext_literal";
  }
  for (const Expr &arg : expr.args) {
    if (!stringParameterUseIsIndexRead(arg, name, &expr)) {
      return false;
    }
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    if (!stringParameterUseIsIndexRead(bodyArg, name, &expr)) {
      return false;
    }
  }
  return true;
}

} // namespace

bool stringParameterUsedOnlyAsIndexReads(const Definition &def, const std::string &name) {
  for (const Expr &stmt : def.statements) {
    if (!stringParameterUseIsIndexRead(stmt, name, nullptr)) {
      return false;
    }
  }
  return !def.returnExpr.has_value() ||
         stringParameterUseIsIndexRead(*def.returnExpr, name, nullptr);
}

} // namespace primec::ir_lowerer
