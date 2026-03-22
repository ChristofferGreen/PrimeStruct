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

TEST_CASE("duplicate definitions fail") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(2)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate definition") != std::string::npos);
}

TEST_CASE("return transform requires template argument") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return")}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform requires a type") != std::string::npos);
}

TEST_CASE("return transform rejects multiple template arguments") {
  primec::Program program;
  primec::Transform transform;
  transform.name = "return";
  transform.templateArgs = {"int", "i32"};
  program.definitions.push_back(
      makeDefinition("/main", {transform}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform requires a type") != std::string::npos);
}

TEST_CASE("import math root is rejected") {
  primec::Program program;
  program.imports.push_back("/std/math");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("import /std/math is not supported") != std::string::npos);
}

TEST_CASE("return transform rejects arguments in manual AST") {
  primec::Program program;
  primec::Transform transform = makeTransform("return", std::string("int"));
  transform.arguments = {"bad"};
  program.definitions.push_back(makeDefinition("/main", {transform}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform does not accept arguments") != std::string::npos);
}

TEST_CASE("monomorphizes template definition bindings") {
  primec::Program program;
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("T"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeBinding("value", {makeTransform("T")}, {})});
  identity.templateArgs = {"T"};
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  call.templateArgs = {"i32"};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("template argument count mismatch fails") {
  primec::Program program;
  primec::Definition wrap = makeDefinition("/wrap",
                                           {makeTransform("return", std::string("int"))},
                                           {makeCall("/return", {makeName("value")})},
                                           {makeParameter("value", "int")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);

  primec::Expr call = makeCall("/wrap", {makeLiteral(1)});
  call.templateArgs = {"i32", "i64"};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("template argument count mismatch") != std::string::npos);
}

TEST_CASE("template arguments required for templated calls") {
  primec::Program program;
  primec::Definition wrap = makeDefinition("/wrap",
                                           {makeTransform("return", std::string("int"))},
                                           {makeCall("/return", {makeName("value")})},
                                           {makeParameter("value", "int")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);

  primec::Expr call = makeCall("/wrap", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("template arguments required") != std::string::npos);
}

TEST_CASE("implicit auto template inference for calls") {
  primec::Program program;
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeParameter("value", "auto")});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference uses defaults") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {makeTransform("auto")}, {makeLiteral(3)});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference honors named arguments") {
  primec::Program program;
  primec::Definition pick =
      makeDefinition("/pick", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("first")})},
                     {makeParameter("first", "auto"), makeParameter("second", "auto")});
  program.definitions.push_back(pick);

  std::vector<primec::Expr> args = {makeLiteral(2), makeLiteral(1)};
  std::vector<std::optional<std::string>> argNames = {std::string("second"), std::string("first")};
  primec::Expr call = makeCall("/pick", args, argNames);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference supports named arguments with defaults") {
  primec::Program program;
  primec::Expr first = makeBinding("first", {makeTransform("auto")}, {makeLiteral(3)});
  primec::Expr second = makeBinding("second", {makeTransform("auto")}, {makeLiteral(4)});
  primec::Definition pick =
      makeDefinition("/pick", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("second")})},
                     {first, second});
  program.definitions.push_back(pick);

  std::vector<primec::Expr> args = {makeLiteral(9)};
  std::vector<std::optional<std::string>> argNames = {std::string("second")};
  primec::Expr call = makeCall("/pick", args, argNames);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference for omitted parameters") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference uses defaults for omitted parameters") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {makeLiteral(3)});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto parameters reject templated definitions when omitted") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  identity.templateArgs = {"T"};
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("implicit auto parameters are only supported on non-templated definitions") != std::string::npos);
}

TEST_CASE("match call behaves like if") {
  primec::Program program;
  primec::Expr thenCall = makeCall("then", {}, {}, {makeLiteral(1)});
  thenCall.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(2)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeBool(true), thenCall, elseCall});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {matchCall})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("match cases validate") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr caseOne = makeCall("case", {makeLiteral(1)}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr caseTwo = makeCall("case", {makeLiteral(2)}, {}, {makeLiteral(20)});
  caseTwo.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(30)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne, caseTwo, elseCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_MESSAGE(validateProgram(program, "/main", error), error);
}

TEST_CASE("match requires else block") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr caseOne = makeCall("case", {makeLiteral(1)}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  const bool hasMatchError = (error.find("match requires value, cases, else") != std::string::npos) ||
                             (error.find("match requires final else block") != std::string::npos);
  CHECK(hasMatchError);
}

TEST_CASE("match rejects incompatible case patterns") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr stringLiteral;
  stringLiteral.kind = primec::Expr::Kind::StringLiteral;
  stringLiteral.stringValue = "\"nope\"utf8";
  primec::Expr caseOne = makeCall("case", {stringLiteral}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(30)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne, elseCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("comparisons do not support mixed string/numeric operands") != std::string::npos);
}

TEST_CASE("uninitialized binding accepts constructor") {
  primec::Program program;
  primec::Expr init = makeCall("uninitialized");
  init.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized parameters are rejected") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {makeTransform("uninitialized", std::string("i32"))}, {});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")},
                                               {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed on parameters") != std::string::npos);
}

TEST_CASE("uninitialized initializer type mismatch fails") {
  primec::Program program;
  primec::Expr init = makeCall("uninitialized");
  init.templateArgs = {"i64"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized initializer type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized helpers are statement-only") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr badBinding = makeBinding("value", {makeTransform("i32")}, {initCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, badBinding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init is only supported as a statement") != std::string::npos);
}

TEST_CASE("uninitialized init/drop allowed as statements") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, dropCall, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized take infers return kind") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr takeCall = makeCall("take", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {takeCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized borrow infers return kind") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr borrowCall = makeCall("borrow", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {borrowCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized init rejects non-storage binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr initCall = makeCall("init", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init requires uninitialized<T> storage") != std::string::npos);
}

TEST_CASE("uninitialized init rejects value type mismatch") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeBool(true)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates array element types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"array<i32>"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("array<i32>"))}, {initStorage});
  primec::Expr arrayValue = makeCall("array", {makeBool(true)});
  arrayValue.templateArgs = {"bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), arrayValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates vector element types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"vector<i32>"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("vector<i32>"))}, {initStorage});
  primec::Expr vectorValue = makeCall("vector");
  vectorValue.templateArgs = {"bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), vectorValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates map key/value types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"map<i32, string>"};
  primec::Expr binding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("map<i32, string>"))}, {initStorage});
  primec::Expr mapValue = makeCall("map");
  mapValue.templateArgs = {"i32", "bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), mapValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init requires uninitialized storage") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr initAgain = makeCall("init", {makeName("storage"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, initAgain, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init requires uninitialized storage") != std::string::npos);
}

TEST_CASE("uninitialized drop requires initialized storage") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, dropCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized storage must be dropped before end of scope") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage must be dropped before end of scope") != std::string::npos);
}

TEST_CASE("uninitialized return requires drop") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized non-void return requires drop") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {makeLiteral(7)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized drop before non-void return passes") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, dropCall, makeCall("/return", {makeLiteral(7)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized if expression requires drop on all branches") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("take", {makeName("storage")})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeLiteral(7)});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifExpr = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {ifExpr})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized if expression drops on both branches") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("take", {makeName("storage")})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("take", {makeName("storage")})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifExpr = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {ifExpr})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized block expression propagates take") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr blockExpr = makeCall("block", {}, {}, {makeCall("take", {makeName("storage")})});
  blockExpr.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {blockExpr})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized for condition take with reinit step passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr stepExpr = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {makeCall("/noop"), condExpr, stepExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, forCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized for condition take requires reinit before next check") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {makeCall("/noop"), condExpr, makeCall("/noop"), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, forCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized while condition take with reinit body passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock =
      makeCall("do", {}, {}, {makeCall("init", {makeName("storage"), makeLiteral(1)}), makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr whileCall = makeCall("while", {condExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, whileCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized while condition take requires reinit before next check") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr whileCall = makeCall("while", {condExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, whileCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized loop body take with reinit passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall(
      "do", {}, {}, {makeCall("take", {makeName("storage")}), makeCall("init", {makeName("storage"), makeLiteral(1)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(2), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("drop", {makeName("storage")}),
                                                makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop body take requires reinit before next iteration") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")}), makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(2), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized repeat body take with reinit passes") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall(
      "repeat", {makeLiteral(2)}, {}, {makeCall("take", {makeName("storage")}),
                                       makeCall("init", {makeName("storage"), makeLiteral(1)})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("drop", {makeName("storage")}),
                                                makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat body take requires reinit before next iteration") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeLiteral(2)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized repeat count expression effects are tracked") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeCall("take", {makeName("storage")})}, {}, {makeCall("/noop")});
  repeatCall.hasBodyArguments = true;
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized loop literal one executes body exactly once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(1), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop literal zero preserves initialized state") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(0), bodyBlock});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat literal one executes body exactly once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeLiteral(1)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat false executes zero iterations") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeBool(false)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat true executes body once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeBool(true)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop literal one leaves storage uninitialized") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(1), bodyBlock});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

TEST_CASE("borrow checker rejects overlapping mutable and immutable references with future use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker rejects overlapping mutable references with future use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker allows overlapping immutable references") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign to borrowed binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, assign, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker rejects move from borrowed binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr moveValue = makeCall("move", {makeName("value")});
  primec::Expr movedBinding = makeBinding("out", {makeTransform("i32")}, {moveValue});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, movedBinding, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after borrow scope ends") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr innerRef =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr block = makeCall("block", {}, {}, {innerRef, makeCall("/noop")});
  block.hasBodyArguments = true;
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/noop",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")}));
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, block, assign, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker tracks references through location(ref)") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after last use in same scope") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, assign, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows mutable borrow after immutable last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr writeThroughRef = makeCall("assign", {makeName("refB"), makeLiteral(3)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, observed, refB, writeThroughRef, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, assign, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker rejects mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate = makeCall("increment", {makeName("value")});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows mutation after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate = makeCall("increment", {makeName("value")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer dereference assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {makeName("ptr")}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer dereference assign after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {makeName("ptr")}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer dereference mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {makeName("ptr")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer dereference mutation after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {makeName("ptr")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows pointer arithmetic dereference assign without borrow") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ptr, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer arithmetic assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer arithmetic assign after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer arithmetic mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer arithmetic mutation after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows assign through location of mutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("ref")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows pointer arithmetic write through location of mutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("ref")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign through location of immutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("ref")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, write, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding: ref") != std::string::npos);
}

TEST_CASE("borrow checker rejects mutation through location of immutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("ref")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, mutate, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("increment target must be a mutable binding: ref") != std::string::npos);
}

TEST_CASE("borrow checker reports immutable location root for assign") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, write, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding: value") != std::string::npos);
}

TEST_CASE("borrow checker reports immutable location arithmetic root for mutation") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("value")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, mutate, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("increment target must be a mutable binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign through location of mutable binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects location(value) assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows location(value) assign after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects location(value) mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate =
      makeCall("increment", {makeCall("dereference", {makeCall("location", {makeName("value")})})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows location(value) mutation after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate =
      makeCall("increment", {makeCall("dereference", {makeCall("location", {makeName("value")})})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized not allowed in array element types") {
  primec::Program program;
  primec::Expr init = makeCall("array");
  init.templateArgs = {"uninitialized<i32>"};
  primec::Expr binding = makeBinding("values", {makeTransform("array", std::string("uninitialized<i32>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in array element types") != std::string::npos);
}

TEST_CASE("uninitialized not allowed as user template arg") {
  primec::Program program;
  primec::Expr init = makeCall("Widget");
  init.templateArgs = {"uninitialized<i32>"};
  primec::Expr binding = makeBinding("value", {makeTransform("Widget", std::string("uninitialized<i32>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed as template argument to user-defined types") !=
        std::string::npos);
}

TEST_CASE("uninitialized canonical map template arg keeps map key value diagnostics") {
  primec::Program program;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "uninitialized<i32>"};

  primec::Expr init = makeCall("map");
  init.templateArgs = {"i32", "uninitialized<i32>"};
  primec::Expr binding = makeBinding("value", {canonicalMapTransform}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in map key/value types") != std::string::npos);
}

TEST_CASE("canonical map template arg without uninitialized validates") {
  primec::Program program;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "i32"};

  primec::Expr init = makeCall("map");
  init.templateArgs = {"i32", "i32"};
  primec::Expr binding = makeBinding("value", {canonicalMapTransform}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized struct fields are allowed") {
  primec::Program program;
  primec::Expr fieldInit = makeCall("uninitialized");
  fieldInit.templateArgs = {"i32"};
  primec::Expr field = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {fieldInit});
  program.definitions.push_back(makeDefinition("/Box", {makeTransform("struct")}, {field}));
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("nested uninitialized remains disallowed in pointer targets") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr init = makeCall("location", {makeName("value")});
  primec::Expr binding =
      makeBinding("ptr", {makeTransform("Pointer", std::string("uninitialized<array<uninitialized<i32>>>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in pointer targets") != std::string::npos);
}

TEST_CASE("nested uninitialized remains disallowed in reference targets") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr init = makeCall("location", {makeName("value")});
  primec::Expr binding =
      makeBinding("ref", {makeTransform("Reference", std::string("uninitialized<array<uninitialized<i32>>>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in reference targets") != std::string::npos);
}

TEST_CASE("uninitialized return types are rejected") {
  primec::Program program;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("uninitialized<i32>"))},
                                               {makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed as return type") != std::string::npos);
}

TEST_CASE("implicit auto parameters reject templated definitions") {
  primec::Program program;
  primec::Definition wrap =
      makeDefinition("/wrap", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeParameter("value", "auto")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {makeLiteral(1)})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("implicit auto parameters are only supported on non-templated definitions") != std::string::npos);
}

TEST_CASE("struct definition cannot return a value") {
  primec::Program program;
  primec::Definition def = makeDefinition("/main", {makeTransform("struct")}, {});
  def.returnExpr = makeLiteral(1);
  program.definitions.push_back(def);
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("struct definitions cannot return a value") != std::string::npos);
}

TEST_CASE("conflicting return types fail") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int")), makeTransform("return", std::string("float"))},
      {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("conflicting return types") != std::string::npos);
}

TEST_CASE("duplicate return transforms fail") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int")), makeTransform("return", std::string("i32"))},
      {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate return transform") != std::string::npos);
}

TEST_CASE("binding transform template arguments fail") {
  primec::Program program;
  primec::Expr binding =
      makeBinding("value", {makeTransform("i32", std::string("bad"))}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding transforms do not take template arguments") != std::string::npos);
}

TEST_CASE("binding requires exactly one type") {
  primec::Program program;
  primec::Expr binding = makeBinding(
      "value", {makeTransform("i32"), makeTransform("f32")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding requires exactly one type") != std::string::npos);
}

TEST_CASE("binding defaults to int when omitted") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("mut")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return blocks are rejected") {
  primec::Program program;
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)}, {}, {makeLiteral(2)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return does not accept block arguments") != std::string::npos);
}

TEST_CASE("if trailing blocks are rejected") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock}, {}, {makeLiteral(3)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("if does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("if block wrappers do not require special names") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("wrong", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  primec::Expr elseBlock = makeCall("also_wrong", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("int"))},
                                               {ifCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block arguments accept bindings on statement calls") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr callWithBlock = makeCall("foo", {}, {}, {binding});
  program.definitions.push_back(
      makeDefinition("/foo", {makeTransform("return", std::string("void"))}, {}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {callWithBlock, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding blocks are rejected") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  binding.hasBodyArguments = true;
  binding.bodyArguments = {makeLiteral(2)};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding does not accept block arguments") != std::string::npos);
}

TEST_CASE("literal statements are allowed") {
  primec::Program program;
  primec::Expr literal = makeLiteral(1);
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {literal, returnCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign target must be a mutable binding") {
  primec::Program program;
  primec::Expr assignCall = makeCall("assign", {makeCall("foo"), makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {assignCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("execution argument name count mismatch fails") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("value")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1)};
  exec.argumentNames = {std::string("value"), std::string("extra")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument name count mismatch") != std::string::npos);
}

TEST_CASE("execution return transform rejects") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1)};
  exec.transforms = {makeTransform("return", std::string("int"))};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform not allowed on executions") != std::string::npos);
}

TEST_CASE("duplicate binding names fail") {
  primec::Program program;
  primec::Expr bindingA = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr bindingB = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {bindingA, bindingB, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate binding name") != std::string::npos);
}

TEST_CASE("binding cannot shadow parameter") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {binding, makeCall("/return", {makeLiteral(1)})}, {makeParameter("value")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate binding name") != std::string::npos);
}

TEST_CASE("unknown call target fails") {
  primec::Program program;
  primec::Expr unknownCall = makeCall("mystery", {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {unknownCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("return not allowed in expression context") {
  primec::Program program;
  primec::Expr innerReturn = makeCall("return", {makeLiteral(1)});
  primec::Expr plusCall = makeCall("plus", {innerReturn, makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {plusCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return not allowed in expression context") != std::string::npos);
}

TEST_CASE("expression call accepts empty block arguments") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(3)})}));
  primec::Expr blockCall = makeCall("task");
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("expression call accepts block arguments") {
  primec::Program program;
  primec::Expr returnExpr = makeCall("/return", {makeName("input")});
  program.definitions.push_back(makeDefinition("/task",
                                               {makeTransform("return", std::string("int"))},
                                               {returnExpr},
                                               {makeParameter("input")}));
  primec::Expr binding = makeBinding("temp", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr blockCall = makeCall("task", {makeLiteral(2)}, {}, {binding, makeName("temp")});
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression yields last expression") {
  primec::Program program;
  primec::Expr binding = makeBinding("x", {makeTransform("i32")}, {makeLiteral(4)});
  primec::Expr plusCall = makeCall("plus", {makeName("x"), makeLiteral(1)});
  primec::Expr blockCall = makeCall("block", {}, {}, {binding, plusCall});
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression requires a value") {
  primec::Program program;
  primec::Expr blockCall = makeCall("block");
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("block statement can contain return") {
  primec::Program program;
  primec::Expr innerReturn = makeCall("/return", {makeLiteral(1)});
  primec::Expr blockStmt = makeCall("block", {}, {}, {innerReturn});
  blockStmt.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {blockStmt}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return rejects named arguments") {
  primec::Program program;
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)}, {std::string("value")});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("if rejects named arguments in manual AST") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock},
                                 {std::string("cond"), std::string("yes"), std::string("no")});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("if branches returning satisfy missing return checks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if missing else return fails missing return checks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr fallbackBinding = makeBinding("fallback", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr elseBlock = makeCall("else", {}, {}, {fallbackBinding});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("for condition binding bool passes") {
  primec::Program program;
  primec::Expr initBinding = makeBinding("i", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(0)});
  primec::Expr condBinding = makeBinding("keepGoing", {makeTransform("bool")}, {makeBool(true)});
  primec::Expr stepCall = makeCall("increment", {makeName("i")});
  primec::Expr bodyBlock = makeCall("body", {}, {}, {makeCall("/return", {makeLiteral(7)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {initBinding, condBinding, stepCall, bodyBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {forCall, makeCall("/return", {makeLiteral(0)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("for condition binding requires bool in manual AST") {
  primec::Program program;
  primec::Expr initBinding = makeBinding("i", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(0)});
  primec::Expr condBinding = makeBinding("keepGoing", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr stepCall = makeCall("increment", {makeName("i")});
  primec::Expr bodyBlock = makeCall("body", {}, {}, {makeCall("/return", {makeLiteral(7)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {initBinding, condBinding, stepCall, bodyBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {forCall, makeCall("/return", {makeLiteral(0)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("for condition requires bool") != std::string::npos);
}

TEST_CASE("missing return statement fails") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {binding}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("method calls require a receiver") {
  primec::Program program;
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "count";
  primec::Expr returnCall = makeCall("/return", {methodCall});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("method call missing receiver") != std::string::npos);
}

TEST_CASE("recursive return inference requires annotation") {
  const std::string source = R"(
main() {
  return(main())
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("/main") != std::string::npos);
}

TEST_CASE("named arguments not allowed on builtin calls in manual AST") {
  primec::Program program;
  primec::Expr plusCall = makeCall("plus", {makeLiteral(1), makeLiteral(2)},
                                  {std::string("left"), std::string("right")});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {plusCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution unknown named argument fails in manual AST") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("c"), std::string("b")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("unknown named argument") != std::string::npos);
}

TEST_CASE("duplicate parameter name fails") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("value"), makeParameter("value")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate parameter") != std::string::npos);
}

TEST_CASE("parameters must use binding syntax") {
  primec::Program program;
  primec::Expr param = makeName("value");
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}, {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("parameters must use binding syntax") != std::string::npos);
}

TEST_CASE("parameters reject block arguments") {
  primec::Program program;
  primec::Expr param = makeParameter("value");
  param.hasBodyArguments = true;
  param.bodyArguments = {makeLiteral(1)};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}, {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("parameter does not accept block arguments") != std::string::npos);
}

TEST_CASE("call argument name count mismatch fails") {
  primec::Program program;
  primec::Expr call = makeCall("callee", {makeLiteral(1)});
  call.argNames = {std::string("a"), std::string("b")};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument name count mismatch") != std::string::npos);
}

TEST_CASE("positional arguments may follow named arguments") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/callee", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
                     {makeParameter("a"), makeParameter("b")}));
  primec::Expr call = makeCall("/callee", {makeLiteral(1), makeLiteral(2)});
  call.argNames = {std::string("a"), std::nullopt};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign requires exactly two arguments") {
  primec::Program program;
  primec::Expr assignCall = makeCall("assign", {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {assignCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign requires exactly two arguments") != std::string::npos);
}

TEST_CASE("convert requires exactly one argument") {
  primec::Program program;
  primec::Expr convertCall = makeCall("convert");
  convertCall.templateArgs = {"int"};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {convertCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("array literal requires exactly one template argument") {
  primec::Program program;
  primec::Expr arrayCall = makeCall("array", {makeLiteral(1)});
  arrayCall.templateArgs = {"i32", "i32"};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {arrayCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("array<T, N> is unsupported; use array<T> (runtime-count array)") !=
        std::string::npos);
}

TEST_CASE("if expression blocks require a value expression") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("block");
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("block", {}, {}, {makeLiteral(3)});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {ifCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("then block must produce a value") != std::string::npos);
}

TEST_CASE("if statement allows empty branch blocks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then");
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else");
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int"))},
      {ifCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution positional argument after named is allowed") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("a"), std::nullopt};
  program.executions.push_back(exec);

  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution named arguments reorder") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(2), makeLiteral(1)};
  exec.argumentNames = {std::string("b"), std::string("a")};
  program.executions.push_back(exec);

  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution duplicate named arguments fail") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("a"), std::string("a")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate named argument") != std::string::npos);
}

TEST_CASE("execution arguments accept collection literals") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("items"), makeParameter("pairs")}));

  primec::Expr arrayCall = makeCall("array", {makeLiteral(1), makeLiteral(2)});
  arrayCall.templateArgs = {"i32"};
  primec::Expr mapCall = makeCall("map", {makeLiteral(1), makeLiteral(10), makeLiteral(2), makeLiteral(20)});
  mapCall.templateArgs = {"i32", "i32"};

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {arrayCall, mapCall};
  exec.argumentNames = {std::string("items"), std::string("pairs")};
  program.executions.push_back(exec);

  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
