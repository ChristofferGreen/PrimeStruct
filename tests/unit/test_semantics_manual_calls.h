#pragma once

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

TEST_CASE("bare zero-arg expression rewrites to call") {
  const std::string source = R"(
[return<int>]
answer() {
  return(7i32)
}

[return<int>]
main() {
  return(answer)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  CHECK(validateProgram(program, "/main", error));
  REQUIRE(error.empty());
  auto mainIt = std::find_if(program.definitions.begin(),
                             program.definitions.end(),
                             [](const primec::Definition &def) {
                               return def.fullPath == "/main";
                             });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE(mainIt->returnExpr.has_value());
  CHECK(mainIt->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(mainIt->returnExpr->name == "answer");
  CHECK(mainIt->returnExpr->args.empty());
}

TEST_CASE("bare zero-arg statement rewrites to call") {
  const std::string source = R"(
[return<void>]
tick() {
}

[return<int>]
main() {
  tick
  return(4i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  CHECK(validateProgram(program, "/main", error));
  REQUIRE(error.empty());
  auto mainIt = std::find_if(program.definitions.begin(),
                             program.definitions.end(),
                             [](const primec::Definition &def) {
                               return def.fullPath == "/main";
                             });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE(mainIt->statements.size() >= 2);
  CHECK(mainIt->statements.front().kind == primec::Expr::Kind::Call);
  CHECK(mainIt->statements.front().name == "tick");
  CHECK(mainIt->statements.front().args.empty());
}

TEST_CASE("local binding conflicts with bare zero-arg expression") {
  const std::string source = R"(
[return<int>]
answer() {
  return(7i32)
}

[return<int>]
main() {
  answer{3i32}
  return(answer)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("ambiguous bare name: answer") != std::string::npos);
  CHECK(error.find("local value") != std::string::npos);
  CHECK(error.find("/answer") != std::string::npos);
}

TEST_CASE("unique local binding still reads as value") {
  const std::string source = R"(
[return<int>]
main() {
  answer{3i32}
  return(answer)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  CHECK(validateProgram(program, "/main", error));
  REQUIRE(error.empty());
  auto mainIt = std::find_if(program.definitions.begin(),
                             program.definitions.end(),
                             [](const primec::Definition &def) {
                               return def.fullPath == "/main";
                             });
  REQUIRE(mainIt != program.definitions.end());
  REQUIRE(mainIt->returnExpr.has_value());
  CHECK(mainIt->returnExpr->kind == primec::Expr::Kind::Name);
  CHECK(mainIt->returnExpr->name == "answer");
}

TEST_CASE("bare zero-arg import alias ambiguity rejects deterministically") {
  const std::string source = R"(
import /alpha/*
import /beta/*
namespace beta {
  [public return<int>]
  answer() {
    return(2i32)
  }
}
namespace alpha {
  [public return<int>]
  answer() {
    return(1i32)
  }
}
[return<int>]
main() {
  return(answer)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("ambiguous bare name: answer") != std::string::npos);
  const std::size_t alphaPos = error.find("/alpha/answer");
  const std::size_t betaPos = error.find("/beta/answer");
  CHECK(alphaPos != std::string::npos);
  CHECK(betaPos != std::string::npos);
  CHECK(alphaPos < betaPos);
}

TEST_CASE("bare zero-arg statement conflicts with local binding") {
  const std::string source = R"(
[return<void>]
tick() {
}

[return<int>]
main() {
  tick{0i32}
  tick
  return(4i32)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("ambiguous bare name: tick") != std::string::npos);
  CHECK(error.find("local value") != std::string::npos);
  CHECK(error.find("/tick") != std::string::npos);
}

TEST_CASE("non-zero-arg callable bare name stays diagnostic") {
  const std::string source = R"(
[return<int>]
needs_value([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(needs_value)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("unknown identifier: needs_value") != std::string::npos);
}

TEST_CASE("compile-time type local validates and is erased") {
  const std::string source = R"(
[return<int>]
main() {
  [type] ValueT { i32 }
  return(5i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::SemanticProgram semanticProgram;
  CHECK(semantics.validate(program,
                           "/main",
                           error,
                           defaults,
                           defaults,
                           {},
                           nullptr,
                           false,
                           &semanticProgram));
  REQUIRE(error.empty());
  auto mainIt = std::find_if(program.definitions.begin(),
                             program.definitions.end(),
                             [](const primec::Definition &def) {
                               return def.fullPath == "/main";
                             });
  REQUIRE(mainIt != program.definitions.end());
  CHECK(std::none_of(mainIt->statements.begin(),
                     mainIt->statements.end(),
                     [](const primec::Expr &stmt) {
                       return stmt.isBinding && stmt.name == "ValueT";
                     }));
  CHECK(std::none_of(semanticProgram.bindingFacts.begin(),
                     semanticProgram.bindingFacts.end(),
                     [](const primec::SemanticProgramBindingFact &fact) {
                       return fact.name == "ValueT";
                     }));
}

TEST_CASE("typeof symbol type local validates and is erased") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  [type] ValueT { typeof<value> }
  return(value)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::SemanticProgram semanticProgram;
  CHECK(semantics.validate(program,
                           "/main",
                           error,
                           defaults,
                           defaults,
                           {},
                           nullptr,
                           false,
                           &semanticProgram));
  REQUIRE(error.empty());
  auto mainIt = std::find_if(program.definitions.begin(),
                             program.definitions.end(),
                             [](const primec::Definition &def) {
                               return def.fullPath == "/main";
                             });
  REQUIRE(mainIt != program.definitions.end());
  CHECK(std::none_of(mainIt->statements.begin(),
                     mainIt->statements.end(),
                     [](const primec::Expr &stmt) {
                       return stmt.isBinding && stmt.name == "ValueT";
                     }));
  CHECK(std::none_of(semanticProgram.bindingFacts.begin(),
                     semanticProgram.bindingFacts.end(),
                     [](const primec::SemanticProgramBindingFact &fact) {
                       return fact.name == "ValueT";
                     }));
}

TEST_CASE("typeof symbol accepts parameter and earlier type local") {
  const std::string source = R"(
[return<int>]
id([i64] value) {
  [type] ValueT { typeof<value> }
  [type] AgainT { typeof<ValueT> }
  return(1i32)
}

[return<int>]
main() {
  return(id(9i64))
}
)";
  std::string error;
  CHECK(validateSourceProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("type locals annotate later local and struct field envelopes") {
  const std::string source = R"(
[struct]
Holder() {
  [i32] seed{1i32}
  [type] ValueT { typeof<seed> }
  [ValueT] copy{2i32}
}

[return<int>]
main() {
  [i32] value{5i32}
  [type] ValueT { typeof<value> }
  [ValueT] copy{value}
  [Holder] holder{Holder{}}
  return(plus(copy, holder.copy))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK(semantics.validate(program,
                           "/main",
                           error,
                           defaults,
                           defaults,
                           {},
                           nullptr,
                           false,
                           &semanticProgram));
  INFO(error);
  CHECK(error.empty());
  bool sawCopyField = false;
  bool sawTypeLocalField = false;
  for (const auto &entry : semanticProgram.structFieldMetadata) {
    if (entry.structPath != "/Holder") {
      continue;
    }
    if (entry.fieldName == "copy") {
      sawCopyField = true;
      CHECK(entry.bindingTypeText == "i32");
    }
    if (entry.fieldName == "ValueT") {
      sawTypeLocalField = true;
    }
  }
  CHECK(sawCopyField);
  CHECK_FALSE(sawTypeLocalField);
  CHECK(std::none_of(semanticProgram.bindingFacts.begin(),
                     semanticProgram.bindingFacts.end(),
                     [](const primec::SemanticProgramBindingFact &fact) {
                       return fact.name == "ValueT";
                     }));
}

TEST_CASE("local generated structs consume enclosing type locals") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] left{3i32}
  [i32] right{4i32}
  [type] LeftT { typeof<left> }
  [type] RightT { typeof<right> }
  [struct] PairT {
    [LeftT] first{0i32}
    [RightT] second{0i32}
  }
  [PairT] pair{PairT{left, right}}
  return(plus(pair.first, pair.second))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  REQUIRE(parser.parse(program, error));
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK(semantics.validate(program,
                           "/main",
                           error,
                           defaults,
                           defaults,
                           {},
                           nullptr,
                           false,
                           &semanticProgram));
  INFO(error);
  CHECK(error.empty());
  auto generatedStructIt =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [](const primec::Definition &def) {
                     return def.fullPath == "/main/PairT";
                   });
  REQUIRE(generatedStructIt != program.definitions.end());
  CHECK(generatedStructIt->isNested);
  CHECK(generatedStructIt->namespacePrefix == "/main");

  bool sawGeneratedStructMetadata = false;
  for (const auto &entry : semanticProgram.typeMetadata) {
    if (entry.fullPath == "/main/PairT") {
      sawGeneratedStructMetadata = true;
      CHECK(entry.category == "struct");
      CHECK(entry.fieldCount == 2);
    }
  }
  CHECK(sawGeneratedStructMetadata);

  bool sawFirstField = false;
  bool sawSecondField = false;
  for (const auto &entry : semanticProgram.structFieldMetadata) {
    if (entry.structPath != "/main/PairT") {
      continue;
    }
    if (entry.fieldName == "first") {
      sawFirstField = true;
      CHECK(entry.fieldIndex == 0);
      CHECK(entry.bindingTypeText == "i32");
    }
    if (entry.fieldName == "second") {
      sawSecondField = true;
      CHECK(entry.fieldIndex == 1);
      CHECK(entry.bindingTypeText == "i32");
    }
  }
  CHECK(sawFirstField);
  CHECK(sawSecondField);
}

TEST_CASE("local generated structs reject escapes and shadowing") {
  const std::string explicitReturn = R"(
[return<PairT>]
make_pair() {
  [i32] value{1i32}
  [type] ValueT { typeof<value> }
  [struct] PairT {
    [ValueT] first{0i32}
  }
  [PairT] pair{PairT{value}}
  return(pair)
}
)";
  std::string explicitError;
  CHECK_FALSE(validateSourceProgram(explicitReturn, "/make_pair", explicitError));
  INFO(explicitError);
  CHECK(explicitError.find("local generated struct cannot escape return type: /make_pair/PairT") !=
        std::string::npos);
  CHECK(explicitError.find("local type defined at") != std::string::npos);
  CHECK(explicitError.find("type facts: ValueT at") != std::string::npos);

  const std::string autoReturn = R"(
[return<auto>]
make_pair() {
  [i32] value{1i32}
  [type] ValueT { typeof<value> }
  [struct] PairT {
    [ValueT] first{0i32}
  }
  [PairT] pair{PairT{value}}
  return(pair)
}
)";
  std::string autoError;
  CHECK_FALSE(validateSourceProgram(autoReturn, "/make_pair", autoError));
  INFO(autoError);
  CHECK(autoError.find("local generated struct cannot escape return type: /make_pair/PairT") !=
        std::string::npos);
  CHECK(autoError.find("local type defined at") != std::string::npos);
  CHECK(autoError.find("type facts: ValueT at") != std::string::npos);

  const std::string shadowLocal = R"(
[return<int>]
main() {
  [struct] PairT {
    [i32] first{0i32}
  }
  [i32] PairT{1i32}
  return(PairT)
}
)";
  std::string shadowError;
  CHECK_FALSE(validateSourceProgram(shadowLocal, "/main", shadowError));
  INFO(shadowError);
  CHECK(shadowError.find("local generated struct name shadows local binding: PairT") !=
        std::string::npos);
}

TEST_CASE("local generated structs reject forward facts and recursive storage") {
  const std::string forwardFact = R"(
[return<int>]
main() {
  [struct] PairT {
    [ValueT] first{0i32}
  }
  [type] ValueT { i32 }
  [PairT] pair{PairT{1i32}}
  return(pair.first)
}
)";
  std::string forwardError;
  CHECK_FALSE(validateSourceProgram(forwardFact, "/main", forwardError));
  INFO(forwardError);
  CHECK(forwardError.find("unsupported binding type: ValueT") !=
        std::string::npos);

  const std::string recursiveStorage = R"(
[return<int>]
main() {
  [struct] NodeT {
    [NodeT] next{NodeT{}}
  }
  return(0i32)
}
)";
  std::string recursiveError;
  CHECK_FALSE(validateSourceProgram(recursiveStorage, "/main", recursiveError));
  INFO(recursiveError);
  CHECK(recursiveError.find("recursive struct layout not supported: /main/NodeT") !=
        std::string::npos);
}

TEST_CASE("type local envelopes reject forward and runtime value use") {
  const std::string forward = R"(
[return<int>]
main() {
  [ValueT] copy{1i32}
  [type] ValueT { i32 }
  return(copy)
}
)";
  std::string forwardError;
  CHECK_FALSE(validateSourceProgram(forward, "/main", forwardError));
  CHECK(forwardError.find("unsupported binding type: ValueT") !=
        std::string::npos);

  const std::string runtimeUse = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [type] ValueT { typeof<value> }
  return(ValueT)
}
)";
  std::string runtimeError;
  CHECK_FALSE(validateSourceProgram(runtimeUse, "/main", runtimeError));
  CHECK(runtimeError.find("type local is not a runtime value: ValueT") !=
        std::string::npos);
}

TEST_CASE("typeof symbol rejects missing and runtime argument forms") {
  const std::string missing = R"(
[return<int>]
main() {
  [type] ValueT { typeof<missing> }
  return(1i32)
}
)";
  std::string missingError;
  CHECK_FALSE(validateSourceProgram(missing, "/main", missingError));
  CHECK(missingError.find("unknown typeof symbol: missing") !=
        std::string::npos);

  const std::string runtime = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [type] ValueT { typeof(value) }
  return(value)
}
)";
  std::string runtimeError;
  CHECK_FALSE(validateSourceProgram(runtime, "/main", runtimeError));
  CHECK(runtimeError.find(
            "typeof requires compile-time symbol syntax: typeof<symbol>") !=
        std::string::npos);
}

TEST_CASE("typeof symbol rejects unsupported callable and ambiguity") {
  const std::string callable = R"(
[return<int>]
answer() {
  return(1i32)
}

[return<int>]
main() {
  [type] ValueT { typeof<answer> }
  return(1i32)
}
)";
  std::string callableError;
  CHECK_FALSE(validateSourceProgram(callable, "/main", callableError));
  CHECK(callableError.find("typeof symbol is not a value: answer") !=
        std::string::npos);

  const std::string ambiguous = R"(
[return<int>]
answer() {
  return(1i32)
}

[return<int>]
main([i32] answer) {
  [type] ValueT { typeof<answer> }
  return(1i32)
}
)";
  std::string ambiguousError;
  CHECK_FALSE(validateSourceProgram(ambiguous, "/main", ambiguousError));
  INFO(ambiguousError);
  CHECK(ambiguousError.find("ambiguous typeof symbol: answer") !=
        std::string::npos);
  CHECK(ambiguousError.find("local value") != std::string::npos);
  CHECK(ambiguousError.find("/answer") != std::string::npos);
}

TEST_CASE("type local is not a runtime value") {
  const std::string source = R"(
[return<int>]
main() {
  [type] ValueT { i32 }
  return(ValueT)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("type local is not a runtime value: ValueT") != std::string::npos);
}

TEST_CASE("type local rejects runtime initializer") {
  const std::string source = R"(
[return<int>]
main() {
  value{1i32}
  [type] ValueT { value }
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("type binding initializer requires a concrete type") != std::string::npos);
}

TEST_CASE("type local rejects binding qualifiers") {
  const std::string source = R"(
[return<int>]
main() {
  [type mut] ValueT { i32 }
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("type binding does not accept binding qualifier: mut") != std::string::npos);
}

TEST_CASE("type local shares duplicate binding namespace") {
  const std::string source = R"(
[return<int>]
main() {
  [type] ValueT { i32 }
  [i32] ValueT{1i32}
  return(ValueT)
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("duplicate binding name: ValueT") != std::string::npos);
}

TEST_CASE("top-level type local rejects as binding syntax") {
  const std::string source = R"(
[type] ValueT { i32 }
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("bindings are only allowed inside definition bodies or parameter lists") !=
        std::string::npos);
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
