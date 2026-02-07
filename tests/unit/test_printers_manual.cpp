#include "primec/Ast.h"
#include "primec/AstPrinter.h"
#include "primec/IrPrinter.h"

#include "third_party/doctest.h"

namespace {
primec::Expr makeLiteral(int value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Literal;
  expr.literalValue = value;
  return expr;
}

primec::Expr makeFloat(const std::string &value, int width) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::FloatLiteral;
  expr.floatValue = value;
  expr.floatWidth = width;
  return expr;
}

primec::Expr makeString(const std::string &value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::StringLiteral;
  expr.stringValue = value;
  return expr;
}

primec::Expr makeName(const std::string &name) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Name;
  expr.name = name;
  return expr;
}

primec::Transform makeTransform(const std::string &name,
                                std::vector<std::string> templateArgs = {},
                                std::vector<std::string> arguments = {}) {
  primec::Transform transform;
  transform.name = name;
  transform.templateArgs = std::move(templateArgs);
  transform.arguments = std::move(arguments);
  return transform;
}

primec::Expr makeCall(const std::string &name,
                      std::vector<primec::Expr> args,
                      std::vector<std::optional<std::string>> argNames = {},
                      std::vector<primec::Expr> bodyArguments = {},
                      std::vector<primec::Transform> transforms = {},
                      std::vector<std::string> templateArgs = {}) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = name;
  expr.args = std::move(args);
  expr.argNames = std::move(argNames);
  expr.bodyArguments = std::move(bodyArguments);
  expr.transforms = std::move(transforms);
  expr.templateArgs = std::move(templateArgs);
  return expr;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.printers.manual");

TEST_CASE("ast printer includes call transforms and bodies") {
  primec::Definition def;
  def.fullPath = "/main";
  def.transforms = {makeTransform("return", {"int"})};

  primec::Expr returnCall = makeCall("/return", {makeLiteral(7)});

  primec::Expr complexCall = makeCall(
      "/ns/return",
      {makeName("first"), makeString("\"hi\"utf8"), makeLiteral(2)},
      {std::string("lhs"), std::nullopt, std::string("rhs")},
      {makeLiteral(1), makeCall("inner", {makeFloat("3.5", 64)})},
      {makeTransform("trace", {"i32"}, {"fast"}), makeTransform("tag")},
      {"f64", "i32"});

  def.statements = {returnCall, complexCall};

  primec::Program program;
  program.definitions.push_back(def);

  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 7\n"
      "    [trace<i32>(fast), tag] /ns/return<f64, i32>(lhs = first, \"hi\"utf8, rhs = 2) { 1, inner(3.5f64) }\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir printer covers bindings assigns and exec bodies") {
  primec::Definition def;
  def.fullPath = "/calc";
  def.transforms = {makeTransform("return", {"f64"})};

  primec::Expr binding;
  binding.kind = primec::Expr::Kind::Call;
  binding.isBinding = true;
  binding.name = "value";
  binding.transforms = {makeTransform("mut"), makeTransform("f64")};
  binding.args = {makeFloat("1.5", 64)};

  primec::Expr assignCall = makeCall("/assign", {makeName("value"), makeFloat("2.0", 64)});
  primec::Expr bodyCall = makeCall("blend", {makeName("value")}, {},
                                  {makeCall("inner", {makeLiteral(1)})});
  primec::Expr returnCall = makeCall("/return", {makeName("value")});

  def.statements = {binding, assignCall, bodyCall, returnCall};

  primec::Execution exec;
  exec.fullPath = "/run";
  exec.transforms = {makeTransform("effects", {"io"})};
  exec.arguments = {makeLiteral(1)};
  exec.bodyArguments = {makeCall("step", {makeLiteral(2)})};

  primec::Program program;
  program.definitions.push_back(def);
  program.executions.push_back(exec);

  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /calc(): f64 {\n"
      "    let value: f64 = 1.5f64\n"
      "    assign value, 2.0f64\n"
      "    call blend(value) { inner(1) }\n"
      "    return value\n"
      "  }\n"
      "  exec [effects<io>] /run(1) { step(2) }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast printer covers executions with templates and bodies") {
  primec::Program program;

  primec::Execution execWithBody;
  execWithBody.fullPath = "/run";
  execWithBody.transforms = {makeTransform("effects", {"io"})};
  execWithBody.templateArgs = {"i32"};
  execWithBody.arguments = {makeLiteral(1), makeLiteral(2)};
  execWithBody.argumentNames = {std::string("count"), std::nullopt};
  execWithBody.bodyArguments = {makeCall("step", {makeLiteral(3)}, {}, {}, {}, {"f32"})};

  primec::Execution execNoBody;
  execNoBody.fullPath = "/ping";
  execNoBody.arguments = {makeString("\"ok\"utf8")};

  program.executions.push_back(execWithBody);
  program.executions.push_back(execNoBody);

  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [effects<io>] /run<i32>(count = 1, 2) { step<f32>(3) }\n"
      "  /ping(\"ok\"utf8)\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_SUITE_END();
