#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/TextFilterPipeline.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}

std::string parseError(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  return error;
}

primec::Program parseSurfaceProgram(const std::string &source,
                                    std::string &filtered) {
  primec::TextFilterPipeline pipeline;
  primec::TextFilterOptions options;
  options.enabledFilters = {"operators", "implicit-i32"};
  std::string error;
  CHECK(pipeline.apply(source, filtered, error, options));
  CAPTURE(error);
  CHECK(error.empty());
  CAPTURE(filtered);
  return parseProgram(filtered);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.parser.templates");

TEST_CASE("parses template list on definition") {
  const std::string source = R"(
[return<int>]
identity<T>([T] x) {
  return(x)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs.size() == 1);
  CHECK(program.definitions[0].templateArgs[0] == "T");
  CHECK(program.definitions[0].templateArgIsPack.size() == 1);
  CHECK_FALSE(program.definitions[0].templateArgIsPack[0]);
}

TEST_CASE("parses heterogeneous type pack parameter on definition") {
  const std::string source = R"(
[return<T>]
pack_head<T, Ts...>([T] head) {
  return(head)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs ==
        std::vector<std::string>{"T", "Ts"});
  CHECK(program.definitions[0].templateArgIsPack ==
        std::vector<bool>{false, true});
}

TEST_CASE("parses heterogeneous type pack parameter on struct") {
  const std::string source = R"(
[struct]
Tuple<T, Ts...> {
  [T] head
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs ==
        std::vector<std::string>{"T", "Ts"});
  CHECK(program.definitions[0].templateArgIsPack ==
        std::vector<bool>{false, true});
}

TEST_CASE("parses heterogeneous type pack field expansion") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &field = program.definitions[0].statements.front();
  REQUIRE(field.isBinding);
  CHECK(field.name == "values");
  REQUIRE(field.transforms.size() == 1);
  CHECK(field.transforms.front().name == "Ts");
  CHECK(field.transforms.front().isPackExpansion);
}

TEST_CASE("parses heterogeneous type pack expansion in type envelopes") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Tuple<Ts...>>]
identity<Ts...>([Tuple<Ts...>] value) {
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &helper = program.definitions[1];
  REQUIRE(helper.transforms.size() == 1);
  REQUIRE(helper.transforms.front().templateArgs.size() == 1);
  CHECK(helper.transforms.front().templateArgs.front() == "Tuple<Ts...>");
  REQUIRE(helper.parameters.size() == 1);
  REQUIRE(helper.parameters.front().transforms.size() == 1);
  REQUIRE(helper.parameters.front().transforms.front().templateArgs.size() == 1);
  CHECK(helper.parameters.front().transforms.front().templateArgs.front() == "Ts...");
}

TEST_CASE("parses template list on call") {
  const std::string source = R"(
[return<int>]
identity<T>([T] x) {
  return(x)
}

[return<int>]
main() {
  return(identity<int>(3i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &mainDef = program.definitions[1];
  REQUIRE(mainDef.returnExpr);
  CHECK(mainDef.returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(mainDef.returnExpr->templateArgs.size() == 1);
  CHECK(mainDef.returnExpr->templateArgs[0] == "int");
}

TEST_CASE("parses integer template argument metadata on call and method call") {
  const std::string source = R"(
[return<i32>]
main() {
  [auto] value{wrap<i32>(1i32)}
  [auto] picked{select<0>(value)}
  return(value.get<1>())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 3);
  REQUIRE(mainDef.statements[0].args.size() == 1);
  const auto &wrapCall = mainDef.statements[0].args[0];
  REQUIRE(wrapCall.templateArgs.size() == 1);
  CHECK(wrapCall.templateArgs[0] == "i32");
  REQUIRE(wrapCall.templateArgDetails.size() == 1);
  CHECK(wrapCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Type);
  REQUIRE(mainDef.statements[1].args.size() == 1);
  const auto &selectCall = mainDef.statements[1].args[0];
  REQUIRE(selectCall.templateArgs.size() == 1);
  CHECK(selectCall.templateArgs[0] == "0");
  REQUIRE(selectCall.templateArgDetails.size() == 1);
  CHECK(selectCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Integer);
  CHECK(selectCall.templateArgDetails[0].integerValue == 0);

  REQUIRE(mainDef.returnExpr);
  const auto &methodCall = *mainDef.returnExpr;
  CHECK(methodCall.isMethodCall);
  CHECK(methodCall.name == "get");
  REQUIRE(methodCall.templateArgs.size() == 1);
  CHECK(methodCall.templateArgs[0] == "1");
  REQUIRE(methodCall.templateArgDetails.size() == 1);
  CHECK(methodCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Integer);
  CHECK(methodCall.templateArgDetails[0].integerValue == 1);
}

TEST_CASE("compile-time argument metadata reserves symbol and unsupported kinds") {
  const auto typeArg = primec::TemplateArgument::type("i32");
  CHECK(typeArg.kind == primec::TemplateArgumentKind::Type);
  CHECK(typeArg.text == "i32");

  const auto integerArg = primec::TemplateArgument::integer("7", 7);
  CHECK(integerArg.kind == primec::TemplateArgumentKind::Integer);
  CHECK(integerArg.text == "7");
  CHECK(integerArg.integerValue == 7);

  const auto symbolArg = primec::TemplateArgument::symbol("value");
  CHECK(symbolArg.kind == primec::TemplateArgumentKind::Symbol);
  CHECK(symbolArg.text == "value");

  const auto unsupportedArg = primec::TemplateArgument::unsupported("future");
  CHECK(unsupportedArg.kind == primec::TemplateArgumentKind::Unsupported);
  CHECK(unsupportedArg.text == "future");

  const std::vector<std::string> legacyTexts = {"T"};
  const std::vector<primec::TemplateArgument> noDetails;
  CHECK(primec::templateArgumentAt(legacyTexts, noDetails, 0).kind ==
        primec::TemplateArgumentKind::Type);
}

TEST_CASE("parses bare typeof symbol as compile-time intrinsic") {
  const std::string source = R"(
[return<i32>]
main() {
  [i32] value{1i32}
  [type] ValueT { typeof<value> }
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 3);
  const auto &typeBinding = mainDef.statements[1];
  REQUIRE(typeBinding.args.size() == 1);
  const auto &typeOfExpr = typeBinding.args.front();
  CHECK(typeOfExpr.kind == primec::Expr::Kind::Call);
  CHECK(typeOfExpr.name == "typeof");
  CHECK(typeOfExpr.args.empty());
  REQUIRE(typeOfExpr.templateArgs.size() == 1);
  CHECK(typeOfExpr.templateArgs[0] == "value");
  REQUIRE(typeOfExpr.templateArgDetails.size() == 1);
  CHECK(typeOfExpr.templateArgDetails[0].kind ==
        primec::TemplateArgumentKind::Symbol);
}

TEST_CASE("parses empty template argument lists") {
  const std::string source = R"(
[return<tuple<>>]
empty_tuple() {
  return(tuple<>{})
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &def = program.definitions[0];
  REQUIRE(def.transforms.size() == 1);
  REQUIRE(def.transforms.front().templateArgs.size() == 1);
  CHECK(def.transforms.front().templateArgs.front() == "tuple<>");
  REQUIRE(def.returnExpr);
  CHECK(def.returnExpr->name == "tuple");
  CHECK(def.returnExpr->isBraceConstructor);
  CHECK(def.returnExpr->templateArgs.empty());
}

TEST_CASE("parses nested template arguments on call") {
  const std::string source = R"(
[return<int>]
main() {
  return(use(map<i32, array<i32>>(1i32, array<i32>(2i32))))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.returnExpr);
  CHECK(mainDef.returnExpr->kind == primec::Expr::Kind::Call);
  REQUIRE(mainDef.returnExpr->args.size() == 1);
  const auto &mapCall = mainDef.returnExpr->args[0];
  CHECK(mapCall.kind == primec::Expr::Kind::Call);
  CHECK(mapCall.templateArgs.size() == 2);
  CHECK(mapCall.templateArgs[0] == "i32");
  CHECK(mapCall.templateArgs[1] == "array<i32>");
}

TEST_CASE("rejects invalid non-type template arguments") {
  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<1.5>(0i32))
}
)").find("integer compile-time arguments must be unsigned integer literals") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<-1>(0i32))
}
)").find("negative integer compile-time arguments are not supported") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<"field">(0i32))
}
)").find("compile-time arguments do not accept string literals") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<true>(0i32))
}
)").find("compile-time arguments do not accept bool literals") !=
        std::string::npos);
}

TEST_CASE("parses nested template argument on return transform") {
  const std::string source = R"(
[return<array<i32>>]
main() {
  return(array<i32>(1i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &def = program.definitions[0];
  REQUIRE(def.transforms.size() == 1);
  REQUIRE(def.transforms[0].templateArgs.size() == 1);
  CHECK(def.transforms[0].templateArgs[0] == "array<i32>");
}

TEST_CASE("rejects invalid heterogeneous type pack declarations") {
  CHECK(parseError(R"(
[return<i32>]
bad<T, T>() {
  return(0i32)
}
)").find("duplicate template parameter: T") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<Ts..., Us...>() {
  return(0i32)
}
)").find("only one type pack parameter is supported") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<Ts..., T>() {
  return(0i32)
}
)").find("type pack parameter must be last") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<args<T>>() {
  return(0i32)
}
)").find("args<T> is variadic value-pack syntax") != std::string::npos);
}

TEST_CASE("parses type pack expansion in call template arguments") {
  const std::string source = R"(
[return<i32>]
forward<Ts...>() {
  return(use<Ts...>())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions.front().returnExpr.has_value());
  const auto &useCall = *program.definitions.front().returnExpr;
  REQUIRE(useCall.kind == primec::Expr::Kind::Call);
  REQUIRE(useCall.templateArgs.size() == 1);
  CHECK(useCall.templateArgs.front() == "Ts...");
}

TEST_CASE("parses canonical generic requirement predicate transforms") {
  const std::string source = R"(
[return<LeftT> require(type_equals<typeof<left>, typeof<right>>(), has_trait<typeof<left>>(Additive), value_greater<N, 0>())]
choose<LeftT, RightT, N>([LeftT] left, [RightT] right) {
  return(left)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &def = program.definitions.front();
  REQUIRE(def.transforms.size() == 2);
  const auto &requireTransform = def.transforms[1];
  CHECK(requireTransform.name == "require");
  REQUIRE(requireTransform.arguments.size() == 3);
  CHECK(requireTransform.arguments[0] ==
        "type_equals<typeof<left>, typeof<right>>()");
  CHECK(requireTransform.arguments[1] == "has_trait<typeof<left>>(Additive)");
  CHECK(requireTransform.arguments[2] == "value_greater<N, 0>()");
  CHECK(def.templateArgs == std::vector<std::string>{"LeftT", "RightT", "N"});
}

TEST_CASE("parses public requirement syntax after text-filter normalization") {
  const std::string source = R"(
[require(typeof<left> == typeof<right>, meta.has_trait<typeof<left>>(Additive), N > 0) return<LeftT>]
choose<LeftT, RightT, N>([LeftT] left, [RightT] right) {
  return(left)
}
)";
  std::string filtered;
  const auto program = parseSurfaceProgram(source, filtered);
  REQUIRE(program.definitions.size() == 1);
  const auto &requireTransform = program.definitions.front().transforms.front();
  CHECK(requireTransform.name == "require");
  REQUIRE(requireTransform.arguments.size() == 3);
  CHECK(requireTransform.arguments[0] ==
        "equal(typeof<left>(), typeof<right>())");
  CHECK(requireTransform.arguments[1] ==
        "meta.has_trait<typeof<left>>(Additive)");
  CHECK(requireTransform.arguments[2] == "greater_than(N, 0i32)");
}

TEST_CASE("parses public ct_if statement and expression branch syntax") {
  const std::string source = R"(
[return<LeftT>]
choose<LeftT, RightT>([LeftT] left, [RightT] right) {
  ct_if(type_equals<typeof<left>, typeof<right>>()) {
    [LeftT] selected{left}
  } else {
    [LeftT] selected{left}
  }
  return(ct_if(type_equals<typeof<left>, typeof<right>>()) {
    left
  } else {
    left
  })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &def = program.definitions.front();
  REQUIRE(def.statements.size() == 2);
  const auto &statementBranch = def.statements.front();
  CHECK(statementBranch.name == "ct_if");
  REQUIRE(statementBranch.args.size() == 3);
  CHECK(statementBranch.args[0].name == "type_equals");
  CHECK(statementBranch.args[1].name == "then");
  REQUIRE(statementBranch.args[1].bodyArguments.size() == 1);
  CHECK(statementBranch.args[1].bodyArguments.front().isBinding);
  CHECK(statementBranch.args[2].name == "else");
  REQUIRE(statementBranch.args[2].bodyArguments.size() == 1);
  CHECK(statementBranch.args[2].bodyArguments.front().isBinding);
  CHECK(def.statements[1].name == "return");

  REQUIRE(def.returnExpr.has_value());
  const auto &valueBranch = *def.returnExpr;
  CHECK(valueBranch.name == "ct_if");
  REQUIRE(valueBranch.args.size() == 3);
  CHECK(valueBranch.args[0].name == "type_equals");
  CHECK(valueBranch.args[1].name == "then");
  REQUIRE(valueBranch.args[1].bodyArguments.size() == 1);
  CHECK(valueBranch.args[1].bodyArguments.front().name == "left");
  CHECK(valueBranch.args[2].name == "else");
  REQUIRE(valueBranch.args[2].bodyArguments.size() == 1);
  CHECK(valueBranch.args[2].bodyArguments.front().name == "left");
}

TEST_SUITE_END();
