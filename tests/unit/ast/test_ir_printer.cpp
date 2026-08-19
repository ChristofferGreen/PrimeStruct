#include "third_party/doctest.h"

#include <string>

#include "primec/ast/Ast.h"
#include "primec/ir/IrPrinter.h"

TEST_SUITE_BEGIN("primestruct.ir_printer");

TEST_CASE("ir printer prints empty program") {
  primec::Program program;
  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("module") != std::string::npos);
}

TEST_CASE("ir printer prints single definition") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/main";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/main") != std::string::npos);
}

TEST_CASE("ir printer prints definition with parameters") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/add";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});

  primec::Expr paramA;
  paramA.kind = primec::Expr::Kind::Name;
  paramA.name = "a";
  paramA.isBinding = true;
  paramA.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.parameters.push_back(paramA);

  primec::Expr paramB;
  paramB.kind = primec::Expr::Kind::Name;
  paramB.name = "b";
  paramB.isBinding = true;
  paramB.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.parameters.push_back(paramB);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/add") != std::string::npos);
}

TEST_CASE("ir printer prints execution envelope") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/print_line";
  exec.transforms.push_back(primec::Transform{"effects", {"io_err"}, {}});

  primec::Expr arg;
  arg.kind = primec::Expr::Kind::StringLiteral;
  arg.stringValue = "hello";
  exec.arguments.push_back(arg);

  program.executions.push_back(exec);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/print_line") != std::string::npos);
}

TEST_CASE("ir printer prints nested expression in return statement") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/compute";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});

  primec::Expr litA;
  litA.kind = primec::Expr::Kind::Literal;
  litA.intWidth = 32;
  litA.literalValue = 2;

  primec::Expr litB;
  litB.kind = primec::Expr::Kind::Literal;
  litB.intWidth = 32;
  litB.literalValue = 3;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "plus";
  callExpr.args.push_back(litA);
  callExpr.args.push_back(litB);

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args.push_back(callExpr);
  def.statements.push_back(returnStmt);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("return plus(2, 3)") != std::string::npos);
}

TEST_CASE("ir printer prints struct definition with field binding") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/data";
  def.transforms.push_back(primec::Transform{"struct", {}, {}});

  primec::Expr field;
  field.kind = primec::Expr::Kind::Name;
  field.name = "value";
  field.isBinding = true;
  field.transforms.push_back(primec::Transform{"i32", {}, {}});

  primec::Expr literal;
  literal.kind = primec::Expr::Kind::Literal;
  literal.intWidth = 32;
  literal.literalValue = 1;
  field.args.push_back(literal);

  def.statements.push_back(field);
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /data(): void {\n"
      "    let value: i32 = 1\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(output == expected);
}

TEST_CASE("ir printer prints definition with template args") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/identity";
  def.transforms.push_back(primec::Transform{"return", {"T"}, {}});
  def.templateArgs.push_back("T");

  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = "value";
  param.isBinding = true;
  param.transforms.push_back(primec::Transform{"return", {"T"}, {}});
  def.parameters.push_back(param);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/identity") != std::string::npos);
}

TEST_SUITE_END();
