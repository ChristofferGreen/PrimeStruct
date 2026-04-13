#include "primec/Ast.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"

#include "third_party/doctest.h"

namespace {
primec::Expr makeLiteral(int value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Literal;
  expr.literalValue = value;
  return expr;
}

primec::Expr makeBool(bool value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::BoolLiteral;
  expr.boolValue = value;
  return expr;
}

primec::Expr makeName(const std::string &name) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Name;
  expr.name = name;
  return expr;
}

primec::Transform makeTransform(const std::string &name,
                                std::optional<std::string> templateArg = std::nullopt) {
  primec::Transform transform;
  transform.name = name;
  if (templateArg.has_value()) {
    transform.templateArgs = {*templateArg};
  }
  return transform;
}

primec::Expr makeCall(const std::string &name,
                      std::vector<primec::Expr> args = {},
                      std::vector<std::optional<std::string>> argNames = {},
                      std::vector<primec::Expr> bodyArguments = {}) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = name;
  expr.args = std::move(args);
  expr.argNames = std::move(argNames);
  expr.bodyArguments = std::move(bodyArguments);
  return expr;
}

primec::Expr makeBinding(const std::string &name,
                         std::vector<primec::Transform> transforms,
                         std::vector<primec::Expr> args) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isBinding = true;
  expr.name = name;
  expr.transforms = std::move(transforms);
  expr.args = std::move(args);
  return expr;
}

primec::Expr makeParameter(const std::string &name, const std::string &type = "i32") {
  return makeBinding(name, {makeTransform(type)}, {});
}

primec::Definition makeDefinition(const std::string &fullPath,
                                  std::vector<primec::Transform> transforms,
                                  std::vector<primec::Expr> statements,
                                  std::vector<primec::Expr> parameters = {}) {
  primec::Definition def;
  def.fullPath = fullPath;
  def.name = fullPath;
  def.transforms = std::move(transforms);
  def.statements = std::move(statements);
  def.parameters = std::move(parameters);
  return def;
}

bool validateProgram(primec::Program &program, const std::string &entry, std::string &error) {
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, entry, error, defaults, defaults);
}

bool validateSourceProgram(const std::string &source, const std::string &entry, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, entry, error, defaults, defaults);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.semantics.manual");

#include "test_semantics_manual_core.h"
#include "test_semantics_manual_templates.h"
#include "test_semantics_manual_uninitialized.h"
#include "test_semantics_manual_borrows.h"
#include "test_semantics_manual_types.h"
#include "test_semantics_manual_calls.h"

TEST_SUITE_END();
