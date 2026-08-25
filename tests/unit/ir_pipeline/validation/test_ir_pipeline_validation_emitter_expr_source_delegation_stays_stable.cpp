#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr contract covers control routing without source locks") {
  auto nameExpr = [](std::string name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto callExpr = [](std::string name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    return expr;
  };

  SUBCASE("expression control steps publish emitted expression behavior") {
    primec::Emitter::BindingInfo localInfo;
    localInfo.typeName = "f64";
    const std::unordered_map<std::string, primec::Emitter::BindingInfo> localTypes = {
        {"e", localInfo},
    };
    const auto localResolved = primec::emitter::runEmitterExprControlNameStep(
        nameExpr("e"),
        localTypes,
        true);
    REQUIRE(localResolved.has_value());
    CHECK(*localResolved == "e");

    const auto mathConstant = primec::emitter::runEmitterExprControlNameStep(
        nameExpr("pi"),
        {},
        true);
    REQUIRE(mathConstant.has_value());
    CHECK(*mathConstant == "ps_const_pi");

    primec::Expr intExpr;
    intExpr.kind = primec::Expr::Kind::Literal;
    intExpr.intWidth = 32;
    intExpr.literalValue = 42;
    const auto intValue = primec::emitter::runEmitterExprControlIntegerLiteralStep(intExpr);
    REQUIRE(intValue.has_value());
    CHECK(*intValue == "42");

    primec::Expr boolExpr;
    boolExpr.kind = primec::Expr::Kind::BoolLiteral;
    boolExpr.boolValue = true;
    const auto boolValue = primec::emitter::runEmitterExprControlBoolLiteralStep(boolExpr);
    REQUIRE(boolValue.has_value());
    CHECK(*boolValue == "true");

    primec::Expr stringExpr;
    stringExpr.kind = primec::Expr::Kind::StringLiteral;
    stringExpr.stringValue = "\"hello\"utf8";
    const auto stringValue = primec::emitter::runEmitterExprControlStringLiteralStep(stringExpr);
    REQUIRE(stringValue.has_value());
    CHECK(*stringValue == "std::string_view(\"hello\")");

    primec::Expr fieldAccess = callExpr("count");
    fieldAccess.isFieldAccess = true;
    fieldAccess.args = {nameExpr("buffer")};
    const auto fieldValue = primec::emitter::runEmitterExprControlFieldAccessStep(
        fieldAccess,
        [](const primec::Expr &receiver) {
          CHECK(receiver.name == "buffer");
          return std::string("buffer");
        },
        {});
    REQUIRE(fieldValue.has_value());
    CHECK(*fieldValue == "buffer.count");

    primec::Expr constructorCall = callExpr("Vec3");
    const auto aliasPath = primec::emitter::runEmitterExprControlCallPathStep(
        constructorCall,
        "Vec3",
        {},
        {{"Vec3", "/pkg/Vec3"}});
    REQUIRE(aliasPath.has_value());
    CHECK(*aliasPath == "/pkg/Vec3");

    primec::Expr methodCall = callExpr("count");
    methodCall.isMethodCall = true;
    bool resolverCalled = false;
    const auto methodPath = primec::emitter::runEmitterExprControlMethodPathStep(
        methodCall,
        {},
        {},
        {},
        [&](std::string &pathOut) {
          resolverCalled = true;
          pathOut = "/std/collections/vector/count";
          return true;
        });
    REQUIRE(methodPath.has_value());
    CHECK(*methodPath == "/std/collections/vector/count");
    CHECK(resolverCalled);
  }

  SUBCASE("block and if control helpers publish wrapper and statement behavior") {
    primec::Expr value = nameExpr("value");

    primec::Expr block = callExpr("block");
    block.hasBodyArguments = true;
    block.bodyArguments = {value};
    const auto blockPrelude = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
        block,
        {},
        [](const primec::Expr &expr, const std::unordered_map<std::string, std::string> &) {
          return expr.name == "block";
        },
        [](const std::vector<std::optional<std::string>> &argNames) {
          return !argNames.empty();
        });
    CHECK(blockPrelude.matched);
    CHECK_FALSE(blockPrelude.earlyReturnExpr.has_value());

    primec::Expr invalidBlock = block;
    invalidBlock.args = {value};
    const auto invalidBlockPrelude = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
        invalidBlock,
        {},
        [](const primec::Expr &expr, const std::unordered_map<std::string, std::string> &) {
          return expr.name == "block";
        },
        {});
    CHECK(invalidBlockPrelude.matched);
    REQUIRE(invalidBlockPrelude.earlyReturnExpr.has_value());
    CHECK(*invalidBlockPrelude.earlyReturnExpr == "0");

    primec::Expr returnCall = callExpr("return");
    returnCall.args = {value};
    const auto earlyReturn = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
        returnCall,
        false,
        [](const primec::Expr &expr) { return expr.name == "return"; },
        [](const primec::Expr &expr) { return expr.name; });
    CHECK(earlyReturn.handled);
    CHECK(earlyReturn.emittedStatement == "return value; ");

    const auto finalValue = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
        value,
        true,
        [](const primec::Expr &) { return false; },
        [](const primec::Expr &expr) { return expr.name; });
    CHECK(finalValue.handled);
    CHECK(finalValue.emittedStatement == "return value; ");

    const auto statement = primec::emitter::runEmitterExprControlBuiltinBlockStatementStep(
        value,
        [](const primec::Expr &expr) { return expr.name; });
    CHECK(statement.handled);
    CHECK(statement.emittedStatement == "(void)value; ");

    primec::Expr callWithBody = callExpr("compute");
    callWithBody.hasBodyArguments = true;
    callWithBody.bodyArguments = {value};
    const auto bodyWrapper = primec::emitter::runEmitterExprControlBodyWrapperStep(
        callWithBody,
        {},
        [](const primec::Expr &, const std::unordered_map<std::string, std::string> &) {
          return false;
        },
        [](const primec::Expr &expr) { return expr.name; });
    REQUIRE(bodyWrapper.has_value());
    CHECK(*bodyWrapper ==
          "([&]() { auto ps_call_value = compute; (void)block; return ps_call_value; }())");

    CHECK(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(block));
    const auto branchPrelude = primec::emitter::runEmitterExprControlIfBranchPreludeStep(
        value,
        [](const primec::Expr &) { return false; },
        [](const primec::Expr &expr) { return expr.name; });
    CHECK(branchPrelude.handled);
    CHECK(branchPrelude.emittedExpr == "value");

    const auto branchValue = primec::emitter::runEmitterExprControlIfBranchValueStep(
        block,
        [](const primec::Expr &expr) {
          return primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(expr);
        },
        [](const primec::Expr &expr) { return expr.name; },
        [](const primec::Expr &stmt, bool isLast) {
          primec::emitter::EmitterExprControlIfBranchBodyEmitResult result;
          result.handled = true;
          result.emittedStatement =
              isLast ? "return " + stmt.name + "; " : "(void)" + stmt.name + "; ";
          return result;
        });
    CHECK(branchValue.handled);
    CHECK(branchValue.emittedExpr == "([&]() { return value; }())");
  }

  SUBCASE("builtin helper routing uses normalized expression paths") {
    primec::Expr vectorAt = callExpr("at");
    vectorAt.namespacePrefix = "/std/collections/vector";
    std::string builtinAccessName;
    CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(vectorAt, builtinAccessName));
    CHECK(builtinAccessName == "at");
    CHECK(primec::emitter::resolveExprPath(vectorAt) == "/std/collections/vector/at");

    primec::Expr vectorAtUnsafe = callExpr("at_unsafe");
    vectorAtUnsafe.namespacePrefix = "/std/collections/vector";
    builtinAccessName.clear();
    CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(vectorAtUnsafe, builtinAccessName));
    CHECK(builtinAccessName == "at_unsafe");

    primec::Expr localAt = callExpr("at");
    builtinAccessName.clear();
    CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(localAt, builtinAccessName));
    CHECK(builtinAccessName == "at");

    for (const char *memoryName : {
             "alloc", "free", "realloc", "at", "at_unsafe", "reinterpret"}) {
      primec::Expr memoryCall = callExpr(memoryName);
      memoryCall.namespacePrefix = "/std/intrinsics/memory";
      std::string resolvedMemoryName;
      CHECK(primec::emitter::getBuiltinMemoryName(memoryCall, resolvedMemoryName));
      CHECK(resolvedMemoryName == memoryName);
    }

    primec::Expr unrelatedMemory = callExpr("alloc");
    std::string resolvedMemoryName;
    CHECK_FALSE(primec::emitter::getBuiltinMemoryName(unrelatedMemory, resolvedMemoryName));
  }
}

TEST_CASE("source C++ emitter emits block-argument expression wrappers without source locks") {
  const std::string source = R"(
[return<int>]
choose([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(choose(block() {
    [i32] local{1i32}
    plus(local, 2i32)
  }))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");

  CHECK(cpp.find("return ps_choose(([&]() { const int local = 1; "
                 "return (local + 2); }()));") != std::string::npos);
  CHECK(cpp.find("(void)(local + 2);") == std::string::npos);
  CHECK(cpp.find("switch (pc)") == std::string::npos);
}

TEST_SUITE_END();
