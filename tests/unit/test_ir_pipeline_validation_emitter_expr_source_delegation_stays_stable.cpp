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

TEST_CASE("semantics validator expr source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path semanticsExprPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
  const std::filesystem::path semanticsExprCollectionCountCapacityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionCountCapacity.cpp";
  const std::filesystem::path semanticsExprCollectionDispatchSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
  const std::filesystem::path semanticsExprCountCapacityMapBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCountCapacityMapBuiltins.cpp";
  const std::filesystem::path semanticsExprDirectCollectionFallbacksPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
  const std::filesystem::path semanticsExprLateCallCompatibilityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateCallCompatibility.cpp";
  const std::filesystem::path semanticsExprMethodResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
  const std::filesystem::path semanticsExprMethodTargetResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodTargetResolution.cpp";
  const std::filesystem::path semanticsExprVectorHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprVectorHelpers.cpp";
  REQUIRE(std::filesystem::exists(semanticsExprPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionCountCapacityPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprCountCapacityMapBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprDirectCollectionFallbacksPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateCallCompatibilityPath));
  REQUIRE(std::filesystem::exists(semanticsExprMethodResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprMethodTargetResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprVectorHelpersPath));

  const std::string semanticsExprSource = readText(semanticsExprPath);
  const std::string semanticsExprCollectionCountCapacitySource =
      readText(semanticsExprCollectionCountCapacityPath);
  const std::string semanticsExprCollectionDispatchSetupSource =
      readText(semanticsExprCollectionDispatchSetupPath);
  const std::string semanticsExprCountCapacityMapBuiltinsSource =
      readText(semanticsExprCountCapacityMapBuiltinsPath);
  const std::string semanticsExprDirectCollectionFallbacksSource =
      readText(semanticsExprDirectCollectionFallbacksPath);
  const std::string semanticsExprLateCallCompatibilitySource =
      readText(semanticsExprLateCallCompatibilityPath);
  const std::string semanticsExprMethodResolutionSource =
      readText(semanticsExprMethodResolutionPath);
  const std::string semanticsExprMethodTargetResolutionSource =
      readText(semanticsExprMethodTargetResolutionPath);
  const std::string semanticsExprVectorHelpersSource =
      readText(semanticsExprVectorHelpersPath);

  CHECK(semanticsExprSource.find("validateExprCountCapacityMapBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprLateCallCompatibility(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprDirectCollectionFallbacks(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("bool SemanticsValidator::resolveExprCollectionCountCapacityTarget(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("bool SemanticsValidator::resolveVectorHelperMethodTarget(") ==
        std::string::npos);

  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "bool SemanticsValidator::resolveExprCollectionCountCapacityTarget(") !=
        std::string::npos);
  CHECK(semanticsExprCollectionDispatchSetupSource.find(
            "bool SemanticsValidator::prepareExprCollectionDispatchSetup(") !=
        std::string::npos);
  CHECK(semanticsExprCountCapacityMapBuiltinsSource.find(
            "bool SemanticsValidator::validateExprCountCapacityMapBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "bool SemanticsValidator::validateExprDirectCollectionFallbacks(") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "bool SemanticsValidator::validateExprLateCallCompatibility(") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool SemanticsValidator::validateExprMethodCallTarget(") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "bool SemanticsValidator::resolveMethodTarget(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool SemanticsValidator::resolveVectorHelperMethodTarget(") !=
        std::string::npos);
}
TEST_CASE("template monomorph source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path templateMonomorphPath = repoRoot / "src" / "semantics" / "TemplateMonomorph.cpp";
  const std::filesystem::path templateMonomorphFallbackPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphFallbackTypeInference.h";
  const std::filesystem::path templateMonomorphBindingCallPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphBindingCallInference.h";
  const std::filesystem::path templateMonomorphBindingBlockPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphBindingBlockInference.h";
  const std::filesystem::path templateMonomorphMethodTargetsPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphMethodTargets.h";
  const std::filesystem::path templateMonomorphTypeResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphTypeResolution.h";
  const std::filesystem::path templateMonomorphAssignmentTargetResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphAssignmentTargetResolution.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionArgumentRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionArgumentRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionConstructorRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionConstructorRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionTargetValueRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionValueRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionValueRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReceiverResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReceiverResolution.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionConstructorPathsPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionConstructorPaths.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionTypeHelpersPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionTypeHelpers.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReturnRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReturnRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReturnSetupPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReturnSetup.h";
  const std::filesystem::path templateMonomorphDefinitionBindingSetupPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionBindingSetup.h";
  const std::filesystem::path templateMonomorphDefinitionReturnOrchestrationPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionReturnOrchestration.h";
  const std::filesystem::path templateMonomorphDefinitionExperimentalCollectionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h";
  const std::filesystem::path templateMonomorphExecutionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExecutionRewrites.h";
  const std::filesystem::path templateMonomorphDefinitionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionRewrites.h";
  const std::filesystem::path templateMonomorphTemplateSpecializationPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphTemplateSpecialization.h";
  const std::filesystem::path templateMonomorphImplicitTemplateInferencePath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphImplicitTemplateInference.h";
  const std::filesystem::path templateMonomorphExpressionRewritePath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExpressionRewrite.h";
  const std::filesystem::path templateMonomorphCollectionCompatibilityPathsPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphCollectionCompatibilityPaths.h";
  const std::filesystem::path mapConstructorHelpersPath =
      repoRoot / "src" / "semantics" / "MapConstructorHelpers.h";
  const std::filesystem::path collectionTypeHelpersPath =
      repoRoot / "src" / "emitter" / "EmitterExprCollectionTypeHelpers.h";
  REQUIRE(std::filesystem::exists(templateMonomorphPath));
  REQUIRE(std::filesystem::exists(templateMonomorphFallbackPath));
  REQUIRE(std::filesystem::exists(templateMonomorphBindingCallPath));
  REQUIRE(std::filesystem::exists(templateMonomorphBindingBlockPath));
  REQUIRE(std::filesystem::exists(templateMonomorphMethodTargetsPath));
  REQUIRE(std::filesystem::exists(templateMonomorphTypeResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphAssignmentTargetResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionArgumentRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionConstructorRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionTargetValueRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionValueRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReceiverResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionConstructorPathsPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionTypeHelpersPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReturnRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReturnSetupPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionBindingSetupPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionReturnOrchestrationPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionExperimentalCollectionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExecutionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphTemplateSpecializationPath));
  REQUIRE(std::filesystem::exists(templateMonomorphImplicitTemplateInferencePath));
  REQUIRE(std::filesystem::exists(templateMonomorphExpressionRewritePath));
  REQUIRE(std::filesystem::exists(templateMonomorphCollectionCompatibilityPathsPath));
  REQUIRE(std::filesystem::exists(mapConstructorHelpersPath));
  REQUIRE(std::filesystem::exists(collectionTypeHelpersPath));
  const std::string templateMonomorphSource = readText(templateMonomorphPath);
  const std::string templateMonomorphFallbackSource = readText(templateMonomorphFallbackPath);
  const std::string templateMonomorphBindingCallSource = readText(templateMonomorphBindingCallPath);
  const std::string templateMonomorphBindingBlockSource = readText(templateMonomorphBindingBlockPath);
  const std::string templateMonomorphMethodTargetsSource = readText(templateMonomorphMethodTargetsPath);
  const std::string templateMonomorphTypeResolutionSource = readText(templateMonomorphTypeResolutionPath);
  const std::string templateMonomorphAssignmentTargetResolutionSource =
      readText(templateMonomorphAssignmentTargetResolutionPath);
  const std::string templateMonomorphExperimentalCollectionArgumentRewritesSource =
      readText(templateMonomorphExperimentalCollectionArgumentRewritesPath);
  const std::string templateMonomorphExperimentalCollectionConstructorRewritesSource =
      readText(templateMonomorphExperimentalCollectionConstructorRewritesPath);
  const std::string templateMonomorphExperimentalCollectionTargetValueRewritesSource =
      readText(templateMonomorphExperimentalCollectionTargetValueRewritesPath);
  const std::string templateMonomorphExperimentalCollectionValueRewritesSource =
      readText(templateMonomorphExperimentalCollectionValueRewritesPath);
  const std::string templateMonomorphExperimentalCollectionReceiverResolutionSource =
      readText(templateMonomorphExperimentalCollectionReceiverResolutionPath);
  const std::string templateMonomorphExperimentalCollectionConstructorPathsSource =
      readText(templateMonomorphExperimentalCollectionConstructorPathsPath);
  const std::string templateMonomorphExperimentalCollectionTypeHelpersSource =
      readText(templateMonomorphExperimentalCollectionTypeHelpersPath);
  const std::string templateMonomorphExperimentalCollectionReturnRewritesSource =
      readText(templateMonomorphExperimentalCollectionReturnRewritesPath);
  const std::string templateMonomorphExperimentalCollectionReturnSetupSource =
      readText(templateMonomorphExperimentalCollectionReturnSetupPath);
  const std::string templateMonomorphDefinitionBindingSetupSource =
      readText(templateMonomorphDefinitionBindingSetupPath);
  const std::string templateMonomorphDefinitionReturnOrchestrationSource =
      readText(templateMonomorphDefinitionReturnOrchestrationPath);
  const std::string templateMonomorphDefinitionExperimentalCollectionRewritesSource =
      readText(templateMonomorphDefinitionExperimentalCollectionRewritesPath);
  const std::string templateMonomorphExecutionRewritesSource =
      readText(templateMonomorphExecutionRewritesPath);
  const std::string templateMonomorphDefinitionRewritesSource =
      readText(templateMonomorphDefinitionRewritesPath);
  const std::string templateMonomorphTemplateSpecializationSource =
      readText(templateMonomorphTemplateSpecializationPath);
  const std::string templateMonomorphImplicitTemplateInferenceSource =
      readText(templateMonomorphImplicitTemplateInferencePath);
  const std::string templateMonomorphExpressionRewriteSource =
      readText(templateMonomorphExpressionRewritePath);
  const std::string templateMonomorphCollectionCompatibilityPathsSource =
      readText(templateMonomorphCollectionCompatibilityPathsPath);
  const std::string mapConstructorHelpersSource =
      readText(mapConstructorHelpersPath);
  const std::string collectionTypeHelpersSource =
      readText(collectionTypeHelpersPath);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphFallbackTypeInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphBindingCallInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphBindingBlockInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphMethodTargets.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphTypeResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphAssignmentTargetResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionArgumentRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionConstructorRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionTargetValueRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionValueRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReceiverResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionConstructorPaths.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReturnRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReturnSetup.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionBindingSetup.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionReturnOrchestration.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionExperimentalCollectionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExecutionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphTemplateSpecialization.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string inferExprTypeTextForTemplatedVectorFallback(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool shouldPreferTemplatedVectorFallbackForTypeMismatch(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string preferVectorStdlibImplicitTemplatePath(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string resolveNameToPath(const std::string &name,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferCallBindingTypeForMonomorph(const Expr &initializer,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "ResolvedType resolveTypeStringImpl(std::string input,\n"
            "                                   const SubstMap &mapping,\n"
            "                                   const std::unordered_set<std::string> &allowedParams,\n"
            "                                   const std::string &namespacePrefix,\n"
            "                                   Context &ctx,\n"
            "                                   std::string &error,\n"
            "                                   std::unordered_set<std::string> &substitutionStack) {") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteTransforms(std::vector<Transform> &transforms,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx) {") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferStdlibCollectionHelperTemplateArgs(const Definition &def,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferCallTargetBinding(const Expr &bindingExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveDereferenceBindingTarget(const Expr &target,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveAssignmentTargetBinding(const Expr &target,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorArgsForTarget(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isExperimentalMapEntryArgument(const Expr &argExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferExperimentalCollectionConstructorTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteCanonicalExperimentalMapConstructorExpr(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteCanonicalExperimentalVectorConstructorExpr(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("auto rewriteCanonicalExperimentalVectorReturnConstructors = [&](auto &self,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("auto rewriteCanonicalExperimentalMapReturnConstructors = [&](auto &self,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("struct ExperimentalCollectionReturnRewritePlan") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool tryAppendDefinitionParameterBinding(Expr &param,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionParameters(std::vector<Expr> &parameters,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void recordDefinitionStatementBindingLocal(Expr &stmt,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isDefinitionReturnPathStatement(const Expr &stmt,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionReturnConstructors(Expr &expr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalMapConstructorValue(Expr &valueExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalMapReturnConstructors(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExecutionEntry(Execution &exec, Context &ctx, std::string &error)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinition(Definition &def,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::vector<Definition> collectTemplateSpecializationFamily(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool canReplaceGeneratedTemplateShell(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string parentPathForDefinition(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool specializeTemplateDefinitionFamily(const std::string &basePath,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalConstructorTargetTypeText(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteExperimentalAssignTargetValue(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteExperimentalInitTargetValue(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isBuiltinResultOkPayloadCall(const Expr &candidate)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorValueTree(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalMapResultOkPayloadTree(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorBinding(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalMapValueReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalMapBorrowedReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalVectorValueReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalMapValueReceiverTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalVectorValueReceiverTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool hasVisibleStdCollectionsImportForPath(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalMapHelperPathForCanonicalHelper(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalVectorHelperPathForCanonicalHelper(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalMapConstructorHelperPath(size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto normalizeFileMethodName = [](std::string_view methodName)") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (methodName == \"readByte\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string normalizedHelperName = normalizeFileMethodName(helperName);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto normalizeFileErrorMethodName = [](std::string_view methodName)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (inferCallBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut,") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "std::optional<std::string> builtinSoaPendingExprDiagnosticForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "builtinSoaPendingExprDiagnosticForMonomorph(*argExpr, params, locals, ctx)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "splitSoaFieldViewHelperPath(resolved, &fieldNameOut)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "splitSoaFieldViewHelperPath(resolvedPath, &fieldName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "std::optional<std::string> soaPendingUnavailableMethodDiagnosticForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "bool hasVisibleDefinitionPathForMonomorph(const Context &ctx,") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "bool hasVisibleSoaHelperTargetForMonomorph(const Context &ctx,") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasVisibleSoaRefHelper =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasVisibleSoaRefRefHelper =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const auto canonicalizeLegacySoaResolvedPath =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string resolvedSoaCanonical =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string normalizedNameSoaCanonical =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string normalizedNameSoaPath =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedNameUsesCanonicalSoaNamespace =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedNameUsesLegacySoaNamespace =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string normalizedPrefixedSoaPath =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string normalizedMethodSoaPath =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedMethodNameMatchesSoaRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedMethodNameMatchesSoaRefRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefNonMethodCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isExplicitSoaRefCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isExplicitSoaRefRefCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isNonMethodCall = !isMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const char *missingSoaRefHelperPath =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedPrefixedUsesLegacySoaNamespace =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedPrefixedNameMatchesSoaRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedPrefixedNameMatchesSoaRefRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedCanonicalNameMatchesSoaRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedCanonicalNameMatchesSoaRefRef =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool resolvedCanonicalNameMatchesSoaRef =\n"
            "        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool resolvedCanonicalNameMatchesSoaRefRef =\n"
            "        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedNameMatchesSoaRef =\n"
            "        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool normalizedNameMatchesSoaRefRef =\n"
            "        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyNormalizedNameSoaRefMatch =\n"
            "        normalizedNameMatchesSoaRef || normalizedNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyNormalizedCanonicalNameSoaRefMatch =\n"
            "        normalizedCanonicalNameMatchesSoaRef ||\n"
            "        normalizedCanonicalNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaCanonical, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedNameSoaCanonical, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedNameSoaPath, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(normalizedPrefixedSoaPath, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedPrefixedSoaPath, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isMethodCall && hasAnyBuiltinSoaRefMethodNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isMethodCall &&\n"
            "          (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "           normalizedCanonicalNameMatchesSoaRef ||\n"
            "           normalizedCanonicalNameMatchesSoaRefRef);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isMethodCall &&\n"
            "           (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "            normalizedCanonicalNameMatchesSoaRef ||\n"
            "            normalizedCanonicalNameMatchesSoaRefRef)) ||") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isMethodCall &&\n"
            "          (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "           hasAnyNormalizedCanonicalNameSoaRefMatch);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isMethodCall &&\n"
            "          (normalizedMethodNameMatchesSoaRefRef ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedName == \"soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedName == \"soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedPrefix == \"soa_vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall && normalizedName == \"ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall && normalizedName == \"ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(normalizedNameUsesCanonicalSoaNamespace &&\n"
            "         isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaCanonical, \"ref\")) ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(normalizedNameUsesCanonicalSoaNamespace &&\n"
             "         isLegacyOrCanonicalSoaHelperPath(\n"
             "             normalizedNameSoaCanonical, \"ref_ref\")) ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool canonicalNamespaceNameMatchesSoaRef =\n"
            "        normalizedNameUsesCanonicalSoaNamespace &&\n"
            "        normalizedCanonicalNameMatchesSoaRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool canonicalNamespaceNameMatchesSoaRefRef =\n"
            "        normalizedNameUsesCanonicalSoaNamespace &&\n"
            "        normalizedCanonicalNameMatchesSoaRefRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefRefCall =\n"
            "        canonicalNamespaceNameMatchesSoaRefRef ||\n"
            "        resolvedCanonicalNameMatchesSoaRefRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefCall =\n"
            "        (normalizedNameUsesCanonicalSoaNamespace &&\n"
            "         normalizedCanonicalNameMatchesSoaRef) ||\n"
            "        resolvedCanonicalNameMatchesSoaRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefRefCall =\n"
            "        (normalizedNameUsesCanonicalSoaNamespace &&\n"
            "         normalizedCanonicalNameMatchesSoaRefRef) ||\n"
            "        resolvedCanonicalNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefCall =\n"
            "        (normalizedNameUsesCanonicalSoaNamespace &&\n"
            "         normalizedCanonicalNameMatchesSoaRef) ||\n"
            "        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref\");") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isCanonicalBuiltinSoaRefRefCall =\n"
            "        (normalizedNameUsesCanonicalSoaNamespace &&\n"
            "         normalizedCanonicalNameMatchesSoaRefRef) ||\n"
            "        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref_ref\");") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isOldSurfaceBuiltinSoaRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        normalizedNameMatchesSoaRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isOldSurfaceBuiltinSoaRefRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        normalizedNameMatchesSoaRefRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isOldSurfaceBuiltinSoaRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref\");") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isOldSurfaceBuiltinSoaRefRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref_ref\");") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isSimpleCallName(candidate, \"ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isSimpleCallName(candidate, \"ref_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "return soaUnavailableMethodDiagnostic(missingSoaRefHelperPath);") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "return soaUnavailableMethodDiagnostic(\n"
            "            isRefRefCall ? \"/std/collections/soa_vector/ref_ref\"\n"
            "                         : \"/std/collections/soa_vector/ref\");") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isAnyOldSurfaceBuiltinSoaRefCall;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isOldSurfaceBuiltinSoaRefRefCall;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyCanonicalNamespaceNameSoaRefMatch =\n"
            "        canonicalNamespaceNameMatchesSoaRef ||\n"
            "        canonicalNamespaceNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyResolvedCanonicalNameSoaRefMatch =\n"
            "        resolvedCanonicalNameMatchesSoaRef ||\n"
            "        resolvedCanonicalNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalBuiltinSoaRefCall =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalBuiltinSoaRefCall =\n"
            "        canonicalNamespaceNameMatchesSoaRef ||\n"
            "        canonicalNamespaceNameMatchesSoaRefRef ||\n"
            "        resolvedCanonicalNameMatchesSoaRef ||\n"
            "        resolvedCanonicalNameMatchesSoaRefRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalBuiltinSoaRefCall =\n"
            "        canonicalNamespaceNameMatchesSoaRef ||\n"
            "        canonicalNamespaceNameMatchesSoaRefRef ||\n"
            "        hasAnyResolvedCanonicalNameSoaRefMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalBuiltinSoaRefCall =\n"
            "        isCanonicalBuiltinSoaRefCall || isCanonicalBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyOldSurfaceBuiltinSoaRefCall =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyOldSurfaceBuiltinSoaRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        (normalizedNameMatchesSoaRef || normalizedNameMatchesSoaRefRef);") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyOldSurfaceBuiltinSoaRefCall =\n"
            "        normalizedNameUsesLegacySoaNamespace &&\n"
            "        hasAnyNormalizedNameSoaRefMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyOldSurfaceBuiltinSoaRefCall =\n"
            "        isOldSurfaceBuiltinSoaRefCall || isOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitSoaRefCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefNonMethodCall =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalOrOldSurfaceBuiltinSoaRefRefCall =\n"
            "        isCanonicalBuiltinSoaRefRefCall || isOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefRefNameMatch =\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          isAnyCanonicalOrOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefRefNameMatch =\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          isCanonicalBuiltinSoaRefRefCall || isOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefRefCall =\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          isCanonicalBuiltinSoaRefRefCall ||\n"
            "          isOldSurfaceBuiltinSoaRefRefCall;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefRefCall =\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          isAnyCanonicalOrOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefRefCall =\n"
            "          hasAnyBuiltinSoaRefRefNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyNormalizedMethodNameSoaRefCall =\n"
            "        normalizedMethodNameMatchesSoaRef || normalizedMethodNameMatchesSoaRefRef;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyCanonicalOrOldSurfaceBuiltinSoaRefCall =\n"
            "        isAnyCanonicalBuiltinSoaRefCall || isAnyOldSurfaceBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefNameMatch =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefCall =\n"
            "        isAnyNormalizedMethodNameSoaRefCall ||\n"
            "        isAnyCanonicalBuiltinSoaRefCall ||\n"
            "        isAnyOldSurfaceBuiltinSoaRefCall;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefCall =\n"
            "        isAnyNormalizedMethodNameSoaRefCall ||\n"
            "        isAnyCanonicalOrOldSurfaceBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefNameMatch =\n"
            "        isAnyNormalizedMethodNameSoaRefCall || isAnyCanonicalBuiltinSoaRefCall ||\n"
            "        isAnyOldSurfaceBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefCall = hasAnyBuiltinSoaRefNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isAnyBuiltinSoaRefRefCall ? \"/std/collections/soa_vector/ref_ref\"\n"
            "                                    : \"/std/collections/soa_vector/ref\";") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasVisibleMissingSoaRefHelper =\n"
            "          isAnyBuiltinSoaRefRefCall ? hasVisibleSoaRefRefHelper\n"
            "                                    : hasVisibleSoaRefHelper;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (hasVisibleMissingSoaRefHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isAnyBuiltinSoaRefCall)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasLeadingCallArg =\n"
            "          !candidate.args.empty() &&\n"
            "          candidate.args.front().kind == Expr::Kind::Call;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasCanonicalBuiltinSoaRefCallArg =\n"
            "          isAnyCanonicalBuiltinSoaRefCall && hasLeadingCallArg;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasCanonicalBuiltinSoaRefCallArg =\n"
            "          isAnyCanonicalBuiltinSoaRefCall && !candidate.args.empty() &&\n"
            "          candidate.args.front().kind == Expr::Kind::Call;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isAnyCanonicalBuiltinSoaRefCall && hasLeadingCallArg)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (hasCanonicalBuiltinSoaRefCallArg)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodOrNonMethodCall =\n"
            "          isAnyBuiltinSoaRefMethodCall || isAnyBuiltinSoaRefNonMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          isAnyExplicitSoaRefCall || isAnyBuiltinSoaRefMethodOrNonMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          ((!candidate.isMethodCall &&\n"
            "            normalizedPrefixedUsesLegacySoaNamespace) &&\n"
            "           (normalizedPrefixedNameMatchesSoaRef ||\n"
            "            normalizedPrefixedNameMatchesSoaRefRef)) ||\n"
            "          isAnyOldSurfaceBuiltinSoaRefCall ||\n"
            "          (candidate.isMethodCall &&\n"
            "           (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "            normalizedCanonicalNameMatchesSoaRef ||\n"
            "            normalizedCanonicalNameMatchesSoaRefRef)) ||\n"
            "          (!candidate.isMethodCall && isAnyNormalizedMethodNameSoaRefCall);") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          (isExplicitLegacySoaNonMethodCall &&\n"
            "           (normalizedPrefixedNameMatchesSoaRef ||\n"
            "            normalizedPrefixedNameMatchesSoaRefRef)) ||\n"
            "          isAnyOldSurfaceBuiltinSoaRefCall ||\n"
            "          (isMethodCall &&\n"
            "           (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "            normalizedCanonicalNameMatchesSoaRef ||\n"
            "            normalizedCanonicalNameMatchesSoaRefRef)) ||\n"
            "          (isNonMethodCall && isAnyNormalizedMethodNameSoaRefCall);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          isAnyExplicitLegacySoaNonMethodRefCall ||\n"
            "          isAnyOldSurfaceBuiltinSoaRefCall ||\n"
            "          (isMethodCall &&\n"
            "           (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "            normalizedCanonicalNameMatchesSoaRef ||\n"
            "            normalizedCanonicalNameMatchesSoaRefRef)) ||\n"
            "          (isNonMethodCall && isAnyNormalizedMethodNameSoaRefCall);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          isAnyExplicitSoaRefCall || isAnyBuiltinSoaRefMethodCall ||\n"
            "          isAnyBuiltinSoaRefNonMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitOrBuiltinSoaRefCall =\n"
            "          isAnyExplicitLegacySoaNonMethodRefCall ||\n"
            "          isAnyOldSurfaceBuiltinSoaRefCall ||\n"
            "          isAnyBuiltinSoaRefMethodCall ||\n"
            "          isAnyBuiltinSoaRefNonMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasCanonicalResolvedSoaRefHelper =\n"
            "          isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyUnavailableSoaRefHelperCall =\n"
            "          hasCanonicalResolvedSoaRefHelper || isAnyExplicitOrBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (hasAnyUnavailableSoaRefHelperCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isMethodCall = candidate.isMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isExplicitLegacySoaNonMethodCall =\n"
            "          isNonMethodCall && normalizedPrefixedUsesLegacySoaNamespace;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyNormalizedPrefixedNameSoaRefMatch =\n"
            "          normalizedPrefixedNameMatchesSoaRef ||\n"
            "          normalizedPrefixedNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitLegacySoaNonMethodRefCall =\n"
            "          isExplicitLegacySoaNonMethodCall &&\n"
            "          (normalizedPrefixedNameMatchesSoaRef ||\n"
            "           normalizedPrefixedNameMatchesSoaRefRef);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "((!candidate.isMethodCall &&\n"
            "            normalizedPrefixedUsesLegacySoaNamespace) &&\n"
            "           (normalizedPrefixedNameMatchesSoaRef ||\n"
            "            normalizedPrefixedNameMatchesSoaRefRef)) ||") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitLegacySoaNonMethodRefCall =\n"
            "          isExplicitLegacySoaNonMethodCall &&\n"
            "          hasAnyNormalizedPrefixedNameSoaRefMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitSoaRefCall =\n"
            "          isAnyExplicitLegacySoaNonMethodRefCall ||\n"
            "          isAnyOldSurfaceBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isNonMethodCall = !candidate.isMethodCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(isExplicitLegacySoaNonMethodCall &&\n"
            "           normalizedPrefixedNameMatchesSoaRef) ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(isExplicitLegacySoaNonMethodCall &&\n"
            "           normalizedPrefixedNameMatchesSoaRefRef) ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyExplicitSoaRefCall =\n"
            "          isExplicitSoaRefCall || isExplicitSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefNonMethodCall =\n"
            "          isNonMethodCall && isAnyNormalizedMethodNameSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefCanonicalNameMatch =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyNormalizedCanonicalNameSoaRefMatch =\n"
            "        normalizedCanonicalNameMatchesSoaRef ||\n"
            "        normalizedCanonicalNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefMethodNameMatch =\n"
            "          isAnyNormalizedMethodNameSoaRefCall ||\n"
            "          hasAnyNormalizedCanonicalNameSoaRefMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefMethodNameMatch =\n"
            "          isAnyNormalizedMethodNameSoaRefCall ||\n"
            "          hasAnyBuiltinSoaRefCanonicalNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodCall =\n"
            "          isMethodCall &&\n"
            "          (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "           normalizedCanonicalNameMatchesSoaRef ||\n"
            "           normalizedCanonicalNameMatchesSoaRefRef);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodCall =\n"
            "          isMethodCall &&\n"
            "          (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "           hasAnyNormalizedCanonicalNameSoaRefMatch);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodCall =\n"
            "          isMethodCall && hasAnyBuiltinSoaRefMethodNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefMethodNameMatch =\n"
            "          normalizedMethodNameMatchesSoaRef ||\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          hasAnyBuiltinSoaRefCanonicalNameMatch;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasAnyBuiltinSoaRefMethodNameMatch =\n"
            "          normalizedMethodNameMatchesSoaRef ||\n"
            "          normalizedCanonicalNameMatchesSoaRef ||\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          normalizedCanonicalNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall &&\n"
            "          (normalizedMethodNameMatchesSoaRef ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall &&\n"
            "          (normalizedMethodNameMatchesSoaRefRef ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "!candidate.isMethodCall && normalizedMethodNameMatchesSoaRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "!candidate.isMethodCall && normalizedMethodNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isNonMethodCall && normalizedMethodNameMatchesSoaRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isNonMethodCall && normalizedMethodNameMatchesSoaRefRef;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(!candidate.isMethodCall &&\n"
            "           normalizedPrefixedUsesLegacySoaNamespace &&\n"
            "           isLegacyOrCanonicalSoaHelperPath(normalizedPrefixedSoaPath, \"ref\"))") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(!candidate.isMethodCall &&\n"
            "           normalizedPrefixedUsesLegacySoaNamespace &&\n"
            "           isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedPrefixedSoaPath, \"ref_ref\"))") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isNonMethodCall &&\n"
            "           normalizedPrefixedUsesLegacySoaNamespace &&\n"
            "           normalizedPrefixedNameMatchesSoaRef)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isNonMethodCall &&\n"
            "           normalizedPrefixedUsesLegacySoaNamespace &&\n"
            "           normalizedPrefixedNameMatchesSoaRefRef)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(normalizedNameUsesLegacySoaNamespace &&\n"
            "           isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, \"ref\"));") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(normalizedNameUsesLegacySoaNamespace &&\n"
            "           isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedNameSoaPath, \"ref_ref\"));") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall &&\n"
            "          (isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedMethodSoaPath, \"ref\") ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "candidate.isMethodCall &&\n"
            "          (isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedMethodSoaPath, \"ref_ref\") ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedMethodNameMatchesSoaRef ||\n"
            "           isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedNameSoaCanonical, \"ref\"));") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedMethodNameMatchesSoaRefRef ||\n"
            "           isLegacyOrCanonicalSoaHelperPath(\n"
            "               normalizedNameSoaCanonical, \"ref_ref\"));") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isCanonicalBuiltinSoaRefCall || isCanonicalBuiltinSoaRefRefCall ||\n"
            "        isOldSurfaceBuiltinSoaRefCall || isOldSurfaceBuiltinSoaRefRefCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "(isCanonicalBuiltinSoaRefCall || isCanonicalBuiltinSoaRefRefCall) &&") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical) ||\n"
            "          isExplicitSoaRefCall ||\n"
            "          isExplicitSoaRefRefCall ||\n"
            "          isBuiltinSoaRefMethod ||\n"
            "          isBuiltinSoaRefRefMethod ||\n"
            "          isBuiltinSoaRefNonMethod ||\n"
            "          isBuiltinSoaRefRefNonMethod)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (normalizedName == \"ref\" || normalizedName == \"ref_ref\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizedName == \"ref_ref\" || isCanonicalBuiltinSoaRefRefCall ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isRefRefCall =\n"
            "          normalizedMethodNameMatchesSoaRefRef || isCanonicalBuiltinSoaRefRefCall ||\n"
            "          isOldSurfaceBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isRefRefCall = isAnyBuiltinSoaRefRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (normalizedMethodNameMatchesSoaRef ||\n"
            "        normalizedMethodNameMatchesSoaRefRef ||\n"
            "        isAnyCanonicalBuiltinSoaRefCall ||\n"
            "        isAnyOldSurfaceBuiltinSoaRefCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isAnyNormalizedMethodNameSoaRefCall ||\n"
            "        isAnyCanonicalBuiltinSoaRefCall ||\n"
            "        isAnyOldSurfaceBuiltinSoaRefCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefCall =\n"
            "        isAnyNormalizedMethodNameSoaRefCall || isAnyCanonicalBuiltinSoaRefCall ||\n"
            "        isAnyOldSurfaceBuiltinSoaRefCall;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isAnyCanonicalBuiltinSoaRefCall &&\n"
            "          !candidate.args.empty() &&\n"
            "          candidate.args.front().kind == Expr::Kind::Call)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isAnyBuiltinSoaRefRefCall ? hasVisibleSoaRefRefHelper\n"
            "                                    : hasVisibleSoaRefHelper)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefRefCall =\n"
            "          normalizedMethodNameMatchesSoaRefRef ||\n"
            "          isCanonicalBuiltinSoaRefRefCall ||\n"
            "          isOldSurfaceBuiltinSoaRefRefCall;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical) ||\n"
            "          isAnyExplicitOrBuiltinSoaRefCall)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (hasCanonicalResolvedSoaRefHelper ||\n"
            "          isAnyExplicitOrBuiltinSoaRefCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isAnyBuiltinSoaRefMethodCall =\n"
            "          isMethodCall &&\n"
            "          (normalizedMethodNameMatchesSoaRef ||\n"
            "           normalizedCanonicalNameMatchesSoaRef ||\n"
            "           normalizedMethodNameMatchesSoaRefRef ||\n"
            "           normalizedCanonicalNameMatchesSoaRefRef);") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical) ||\n"
            "          isAnyExplicitSoaRefCall ||\n"
            "          isAnyBuiltinSoaRefMethodCall ||\n"
            "          isAnyBuiltinSoaRefNonMethodCall)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaRefNonMethod || isBuiltinSoaRefRefNonMethod;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaRefMethod || isBuiltinSoaRefRefMethod;") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool resolvedMatchesSoaRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool resolvedMatchesSoaRefRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (resolvedPath == \"/soa_vector/ref\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (resolvedSoaCanonical == \"/std/collections/soa_vector/ref\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "hasVisibleDefinitionPathForMonomorph(ctx, helperPath)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "auto hasVisibleSoaHelperTarget =") ==
        std::string::npos);
  CHECK((templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(\"/soa_vector/ref\") > 0 ||") !=
         std::string::npos ||
         templateMonomorphImplicitTemplateInferenceSource.find(
             "hasVisibleSoaBorrowedHelper(\"ref\")") !=
             std::string::npos));
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(ownedPath) > 0 ||") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "hasVisibleSoaHelperTarget(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool isKnownBuiltinSoaHelperName =\n"
            "        normalizedName == \"count\" || normalizedName == \"get\" ||\n"
            "        normalizedName == \"to_soa\" || normalizedName == \"to_aos\" ||\n"
            "        normalizedName == \"to_aos_ref\" ||\n"
            "        normalizedName == \"contains\";") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (isKnownBuiltinSoaHelperName)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (normalizedName == \"count\" || normalizedName == \"get\" ||\n"
            "        normalizedName == \"to_soa\" || normalizedName == \"to_aos\" ||\n"
            "        normalizedName == \"to_aos_ref\" ||\n"
            "        normalizedName == \"contains\")") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(ownedPath) > 0 ||\n"
            "        ctx.helperOverloads.count(ownedPath) > 0") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnosticForMonomorph(\n"
            "                  ctx, resolvedPath)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectFieldViewPendingDiagnostic(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(\n"
            "        soaFieldViewHelperPath(normalizedName))") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaUnavailableMethodDiagnostic(\n"
            "        soaFieldViewHelperPath(normalizedName))") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "\"/std/collections/soa_vector/ref_ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "\"/std/collections/soa_vector/ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaUnavailableMethodDiagnostic(\"/soa_vector/ref\", false)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaFieldViewPendingDiagnostic(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("\"/soa_vector/field_view/\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("hasVisibleSoaFieldHelperForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaFieldViewExprForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaRefExprForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("if (handledCallInference) {") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "std::string paramBaseType = paramInfo.typeName;") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "splitTemplateTypeName(paramInfo.typeTemplateArg, innerBase, innerArgs)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizeCollectionReceiverTypeName(paramBaseType)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "normalizeCollectionReceiverTypeName(paramInfo.typeName))") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "return inferBlockBodyBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut);") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string resolveStructLikeExprPathForTemplatedVectorFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string inferExprTypeTextForTemplatedVectorFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool shouldPreferTemplatedVectorFallbackForTypeMismatch(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string preferVectorStdlibImplicitTemplatePath(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "path == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "isLegacyOrCanonicalSoaHelperPath(pathCanonical, \"count_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "isLegacyOrCanonicalSoaHelperPath(pathCanonical, \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "if (isLegacyOrCanonicalSoaHelperPath(pathCanonical, \"get_ref\") ||\n"
            "      isCanonicalSoaRefLikeHelperPath(pathCanonical))") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "auto isLegacyOrCanonicalSoaHelperPath = [](const std::string &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "constexpr const char kLegacyPrefix[] = \"/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "constexpr const char kCanonicalPrefix[] = \"/std/collections/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "isCanonicalSoaRefLikeHelperPath(pathCanonical)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "auto isCanonicalSoaRefLikePath = [](const std::string &candidate)") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "const auto canonicalizeLegacySoaRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "const std::string pathCanonical =") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "canonicalizeLegacySoaGetHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "pathCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "pathCanonical == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "constexpr const char *kLegacyPrefix = \"/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "constexpr const char *kCanonicalPrefix = \"/std/collections/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaCarryNonRefHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaGetHelperPath(candidate)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, \"get\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto canonicalizeLegacySoaRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaRefHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalPath == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalPath == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string resolvedPathCanonical =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string methodPathCanonical =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaHelperTemplateCarryPath(methodPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedReceiverFamily == \"vector\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPathCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/get\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/count\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/count_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/reserve\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/push\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto isCanonicalSoaHelperPath = [](const std::string &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaHelperPath(canonicalSoaCountPath, \"count\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(candidate, \"count_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaHelperPath(canonicalSoaCountPath, \"count_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaHelperPath(canonicalSoaCountPath, \"reserve\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaHelperPath(canonicalSoaCountPath, \"push\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string canonicalSoaGetPath =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, \"get\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string canonicalSoaToAosPath =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaToAosHelperPath(samePathToAosHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathToAosHelper = \"/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, \"to_aos\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto isCanonicalSoaBorrowedWrapperHelper = [&](const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "!isCanonicalSoaBorrowedWrapperHelper(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "!isCanonicalSoaBorrowedWrapperHelper(methodPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosHelper, \"to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosHelper, \"to_aos_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (hasVisibleRootBuiltinSoaConversionHelper(samePathToAosHelper) &&\n"
            "          resolvesBuiltinSoaToAosShadowReceiver(receiverExpr)) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool matchesSoaToAosHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool matchesBorrowedSoaToAosHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaToAosHelperPath(path)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaToAosHelperPath(canonicalSoaToAosHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/to_aos\" || path == \"/std/collections/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/ref\" || path == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaRefLikeHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"to_aos\" ? \"/to_aos\" : \"/to_aos_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"to_aos\" ? std::string(\"/to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get\" || helperName == \"get_ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string receiverFamily = inferCollectionReceiverFamily(receiverExpr);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto resolvesBorrowedExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverExpr->kind == Expr::Kind::Call && !receiverExpr->isBinding) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "std::vector<std::string> receiverCandidatePaths;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "std::string resolvedReceiverPath;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "inferDefinitionReturnBindingForTemplatedFallback(\n"
            "                    defIt->second, allowMathBare, ctx, inferredReturn)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool receiverResolvesBorrowedExperimentalSoaVector =\n"
            "        resolvesBorrowedExperimentalSoaVectorReceiver(receiverExpr);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverResolvesBorrowedExperimentalSoaVector) {\n"
            "      if (helperName == \"count\") {\n"
            "        helperName = \"count_ref\";\n"
            "      } else if (helperName == \"get\") {\n"
            "        helperName = \"get_ref\";\n"
            "      } else if (helperName == \"ref\") {\n"
            "        helperName = \"ref_ref\";\n"
            "      } else if (helperName == \"to_aos\") {\n"
            "        helperName = \"to_aos_ref\";\n"
            "      }\n"
            "    }") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto preferredBorrowedSoaWrapperPath = [&](const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/std/collections/soa_vector/count_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/std/collections/soa_vector/get_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/std/collections/soa_vector/ref_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/std/collections/soa_vector/to_aos_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string preferredBorrowedSoaPath =\n"
            "        preferredBorrowedSoaWrapperPath(resolvedPath);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string latePreferredBorrowedSoaPath =\n"
            "        preferredBorrowedSoaWrapperPath(resolvedPath);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (!preferredBorrowedSoaPath.empty() &&\n"
            "        resolvesBorrowedExperimentalSoaVectorReceiver(") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (expr.templateArgs.empty() &&\n"
            "        !latePreferredBorrowedSoaPath.empty() &&\n"
            "        resolvesBorrowedExperimentalSoaVectorReceiver(resolvedReceiverExpr) &&") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&\n"
            "        resolvesBorrowedExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathGetHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverFamily == \"soa_vector\" ||\n"
            "        receiverResolvesBorrowedExperimentalSoaVector) {\n"
            "      const std::string preferred = \"/std/collections/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "(resolvesVectorFamilyPath ||\n"
            "           hasVisibleStdCollectionsImportForPath(ctx, preferred))") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverFamily == \"soa_vector\" && resolvesVectorFamilyPath)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverFamily == \"soa_vector\" && !resolvesVectorFamilyPath)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"count\" || helperName == \"count_ref\" ||\n"
            "        helperName == \"push\" || helperName == \"reserve\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathSoaNonRefHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const auto vectorReceiverHasVisibleCanonicalHelper =\n"
            "        [&](std::string_view candidateHelperName) {\n"
            "          const std::string preferred =\n"
            "              \"/std/collections/vector/\" + std::string(candidateHelperName);\n"
            "          return receiverFamily == \"vector\" &&\n"
            "                 hasVisibleStdCollectionsImportForPath(ctx, preferred) &&\n"
            "                 ctx.sourceDefs.count(preferred) > 0;\n"
            "        };") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool receiverEligibleForSamePathSoaHelper =\n"
            "          receiverFamily == \"soa_vector\" ||\n"
            "          receiverResolvesBorrowedExperimentalSoaVector ||\n"
            "          receiverResolvesExperimentalSoaVector ||\n"
            "          ((helperName == \"count\" || helperName == \"count_ref\") &&\n"
            "           receiverFamily == \"vector\" &&\n"
            "           !vectorReceiverHasVisibleCanonicalHelper(helperName));") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(samePathSoaNonRefHelper,") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (receiverEligibleForSamePathSoaHelper &&\n"
            "          hasDefinitionFamilyPath(samePathSoaNonRefHelper)) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathSoaNonRefHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/push\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/reserve\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/push\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/reserve\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaGetHelperPath(samePathGetHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (hasDefinitionFamilyPath(samePathGetHelper) &&\n"
            "          (receiverFamily == \"soa_vector\" ||\n"
            "           receiverResolvesBorrowedExperimentalSoaVector ||\n"
            "           receiverResolvesExperimentalSoaVector ||\n"
            "           receiverFamily == \"vector\")) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalGetHelper, helperName)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathGetHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get_ref\" ? \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get_ref\" ? std::string(\"/soa_vector/get_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathRefHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (hasDefinitionFamilyPath(samePathRefHelper) &&\n"
            "          (receiverFamily == \"soa_vector\" ||\n"
            "           receiverResolvesBorrowedExperimentalSoaVector ||\n"
            "           receiverResolvesExperimentalSoaVector ||\n"
            "           receiverFamily == \"vector\")) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "inferCollectionReceiverFamily(receiverExpr) == \"soa_vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "inferCollectionReceiverFamily(receiverExpr) == \"vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaRefHelperPath(samePathRefHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalRefHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathRefHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(helperName == \"ref_ref\" ? \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return helperName == \"ref_ref\" ? std::string(\"/soa_vector/ref_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedPath == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPath == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("bool inferCallBindingTypeForMonomorph(const Expr &initializer,") !=
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("handledOut = false;") !=
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphBindingBlockSource.find("bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,") !=
        std::string::npos);
  CHECK(templateMonomorphBindingBlockSource.find("LocalTypeMap blockLocals = locals;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("bool resolveMethodCallTemplateTarget(const Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("std::string resolveNameToPath(const std::string &name,") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "canonicalizeLegacySoaToAosHelperPath(samePath)") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string helperNameString(helperName);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto soaCanonicalMethodPath = [&](const std::string &helperNameString) {\n"
            "    return \"/std/collections/soa_vector/\" + helperNameString;\n"
            "  };") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto soaCanonicalMethodPath = [](const std::string &helperNameString) {\n"
            "    return \"/std/collections/soa_vector/\" + helperNameString;\n"
            "  };") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSamePathSoaMethodTarget =\n"
            "      [&](std::string_view helperName, std::string_view samePathPrefix) {") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSamePathSoaToAosMethodTarget = [&](std::string_view helperName) {\n"
            "    return preferredSamePathSoaMethodTarget(helperName, \"/\");\n"
            "  };") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSamePathSoaPushReserveMethodTarget = [&](std::string_view helperName) {\n"
            "    return preferredSamePathSoaMethodTarget(helperName, \"/soa_vector/\");\n"
            "  };") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSamePathSoaGetMethodTarget = [&](std::string_view helperName) {\n"
            "    return preferredSamePathSoaMethodTarget(helperName, \"/soa_vector/\");\n"
            "  };") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSamePathSoaRefMethodTarget = [&](std::string_view helperName) {\n"
            "    return preferredSamePathSoaMethodTarget(helperName, \"/soa_vector/\");\n"
            "  };") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto isBorrowedSoaReceiverType = [&](std::string typeText) {") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto borrowedSoaWrapperMethodName = [](std::string_view helperName) {") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (helperName == \"count\") {\n"
            "      return std::string(\"count_ref\");\n"
            "    }\n"
            "    if (helperName == \"get\") {\n"
            "      return std::string(\"get_ref\");\n"
            "    }\n"
            "    if (helperName == \"ref\") {\n"
            "      return std::string(\"ref_ref\");\n"
            "    }\n"
            "    if (helperName == \"to_aos\") {\n"
            "      return std::string(\"to_aos_ref\");") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonicalPath =\n"
            "        \"/std/collections/soa_vector/\" + std::string(helperName);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonicalPath =\n"
            "        \"/std/collections/soa_vector/\" + helperNameString;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonicalPath = soaCanonicalMethodPath(helperNameString);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = \"/\" + std::string(helperName);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = \"/\" + helperNameString;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = std::string(samePathPrefix) + helperNameString;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePathPrefixString(samePathPrefix);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = samePathPrefixString + helperNameString;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string pathString(path);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "ctx.sourceDefs.count(std::string(path)) > 0 || ctx.helperOverloads.count(std::string(path)) > 0") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "ctx.sourceDefs.count(pathString) > 0 || ctx.helperOverloads.count(pathString) > 0") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (defPath == path || defPath.rfind(templatedPrefix, 0) == 0 ||") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (defPath.rfind(templatedPrefix, 0) == 0 ||\n"
            "          defPath.rfind(specializedPrefix, 0) == 0 ||\n"
            "          defPath.rfind(overloadPrefix, 0) == 0) {") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalPath, \"to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalPath, \"to_aos_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "(matchesSoaToAosHelperPath || matchesBorrowedSoaToAosHelperPath) &&") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (hasDefinitionFamilyPath(samePath)) {\n"
            "      return samePath;\n"
            "    }\n"
            "    return canonicalPath;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (hasDefinitionFamilyPath(samePath)) {\n"
            "      return samePath;\n"
            "    }\n"
            "    return soaCanonicalMethodPath(helperNameString);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = \"/soa_vector/\" + std::string(helperName);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = \"/soa_vector/\" + helperNameString;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonical = \"/std/collections/soa_vector/\" + std::string(helperName);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "return \"/std/collections/soa_vector/\" + std::string(helperName);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (hasDefinitionFamilyPath(samePath)) {\n"
            "      return samePath;\n"
            "    }\n"
            "    return \"/std/collections/soa_vector/\" + helperNameString;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "return preferredSamePathSoaMethodTarget(helperName, \"/\");") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "return preferredSamePathSoaMethodTarget(helperName, \"/soa_vector/\");") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSoaToAosMethodTarget =") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "auto preferredSoaMethodTarget =") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaMethodTarget(normalizedMethodName, \"/\"), ctx);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaMethodTarget(normalizedMethodName, \"/soa_vector/\"), ctx);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaToAosMethodTarget(normalizedMethodName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaGetMethodTarget(normalizedMethodName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaPushReserveMethodTarget(normalizedMethodName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaRefMethodTarget(normalizedMethodName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string helperName =\n"
            "        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)\n"
            "                              : normalizedMethodName;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaGetMethodTarget(helperName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaCountMethodTarget(helperName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaRefMethodTarget(helperName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaToAosMethodTarget(helperName), ctx);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "std::string_view samePathPrefix;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "samePathPrefix = \"/\";") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "samePathPrefix = \"/soa_vector/\";") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSamePathSoaMethodTarget(normalizedMethodName, samePathPrefix), ctx);") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (hasDefinitionFamilyPath(canonical)) {\n"
            "      return canonical;\n"
            "    }") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "helperName == \"to_aos\" ? std::string(\"/to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"count\" || normalizedMethodName == \"count_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"to_aos\" || normalizedMethodName == \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" && normalizedMethodName == \"to_aos\"") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"get\" || normalizedMethodName == \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"push\" || normalizedMethodName == \"reserve\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"ref\" || normalizedMethodName == \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const bool isConcreteExperimentalSoaReceiver =\n"
            "      normalizedTypeName == \"soa_vector\" &&\n"
            "      resolvedType.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) == 0;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (isConcreteExperimentalSoaReceiver &&\n"
            "      (normalizedMethodName == \"count\" || normalizedMethodName == \"count_ref\")) {\n"
            "    pathOut = selectHelperOverloadPath(\n"
            "        expr, preferredSamePathSoaCountMethodTarget(normalizedMethodName), ctx);\n"
            "    return true;\n"
            "  }") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (isConcreteExperimentalSoaReceiver &&\n"
            "      (normalizedMethodName == \"push\" || normalizedMethodName == \"reserve\")) {\n"
            "    pathOut = selectHelperOverloadPath(\n"
            "        expr, preferredSamePathSoaPushReserveMethodTarget(normalizedMethodName), ctx);\n"
            "    return true;\n"
            "  }") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (normalizedTypeName == \"soa_vector\") {\n"
            "    std::string_view samePathPrefix;") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isCanonicalSoaToAosHelperPath(canonicalPath)") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonical = \"/std/collections/soa_vector/to_aos\"") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSoaToAosMethodTarget()") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find("ResolvedType resolveTypeStringImpl(std::string input,") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find("bool rewriteTransforms(std::vector<Transform> &transforms,") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx)") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "#include \"MapConstructorHelpers.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "metadataBackedCanonicalMapConstructorRewritePath(") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "auto vectorConstructorHelperPath = [&]() -> std::string {") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "auto mapConstructorHelperPath = [&]() -> std::string {") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "helperPath = vectorConstructorHelperPath();") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "helperPath = mapConstructorHelperPath();") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "resolvedPath == \"/std/collections/vector/vector\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "if (resolvedPath == \"/std/collections/vector\") {") ==
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool inferCallTargetBinding(const Expr &bindingExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool resolveAssignmentTargetBinding(const Expr &target,") !=
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool resolveDereferenceBindingTarget(const Expr &target,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionArgumentRewritesSource.find(
            "std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(const Definition &targetDef,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionArgumentRewritesSource.find(
            "bool rewriteExperimentalConstructorArgsForTarget(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool isExperimentalMapEntryArgument(const Expr &argExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool inferExperimentalCollectionConstructorTemplateArgs(const std::string &originalPath,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool rewriteCanonicalExperimentalMapConstructorExpr(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool rewriteCanonicalExperimentalVectorConstructorExpr(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "bool resolveExperimentalConstructorTargetTypeText(const Expr &targetExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "void rewriteExperimentalAssignTargetValue(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "void rewriteExperimentalInitTargetValue(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool isBuiltinResultOkPayloadCall(const Expr &candidate)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalConstructorValueTree(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalMapResultOkPayloadTree(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalConstructorBinding(Expr &bindingExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolvesExperimentalMapValueReceiver(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolveExperimentalMapValueReceiverTemplateArgs(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolveExperimentalVectorValueReceiverTemplateArgs(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "std::string experimentalMapHelperPathForCanonicalHelper(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "std::string experimentalSoaVectorHelperPathForCanonicalHelper(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "primec::stdlibSurfacePreferredSpellingForMember(") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "primec::StdlibSurfaceId::CollectionsSoaVectorHelpers") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "if (path == \"/std/collections/map/count\")") == std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "if (path == \"/std/collections/vector/count\" || path == \"/vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool isExperimentalSoaVectorPublicHelperPath(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "isExperimentalSoaVectorHelperFamilyPath(path);") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "isExperimentalSoaVectorConversionFamilyPath(path);") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "path.rfind(\"/std/collections/experimental_soa_vector/\", 0) == 0 ||") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "path.rfind(\"/std/collections/experimental_soa_vector_conversions/\", 0) == 0;") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/count_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, \"count_ref\") ||\n"
            "      isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, \"get_ref\") ||\n"
            "      isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, \"ref_ref\")) {\n"
            "    return {};\n"
            "  }") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "stdlibSurfaceCanonicalHelperPath(\n"
            "        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "return \"/std/collections/experimental_soa_vector/soaVectorCount\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, \"count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "normalizeCollectionReceiverTypeName(normalizedBase) == \"soa_vector\" ||\n"
            "                 isExperimentalSoaVectorSpecializedTypePath(normalizedBase)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (normalizedBase != \"soa_vector\") {") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, \"count_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "return \"/std/collections/experimental_soa_vector/soaVectorCountRef\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/get\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/get_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "return \"/std/collections/experimental_soa_vector/soaVectorGetRef\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/ref_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/reserve\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/push\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "auto canonicalizeSoaHelperPath = [](std::string canonicalPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "const std::string canonicalSoaGetPath =") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "canonicalizeLegacySoaGetHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "normalizedResolvedPath.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isExperimentalSoaVectorPublicHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isExperimentalSoaVectorPublicHelperPath(methodPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isExperimentalSoaVectorSpecializedTypePath(normalizedBase)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "normalizedBase.rfind(\"SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "normalizedResolvedPath.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "normalizedBase.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedPath.rfind(\"/std/collections/experimental_soa_vector_conversions/\", 0)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool shouldRewriteCanonicalSoaHelperToExperimental =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasVisibleStdCollectionsImportForPath(ctx, resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool canRewriteNamedExperimentalVectorTemporary =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "!hasNamedArguments(experimentalVectorReceiverExpr->argNames)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvesConcreteExperimentalSoaVectorReceiver(") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const bool shouldRewriteCanonicalSoaMethodToExperimental =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasVisibleStdCollectionsImportForPath(ctx, methodPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto preferredConcreteSamePathSoaHelperPath = [&](const std::string &path) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (!resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "preferredConcreteSamePathSoaHelperPath(resolvedPath);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (expr.templateArgs.empty() &&\n"
            "        isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&\n"
            "        resolvesBorrowedExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {\n"
            "        expr.templateArgs = std::move(receiverTemplateArgs);\n"
            "        allConcrete = true;\n"
            "      }\n"
            "    }") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (expr.templateArgs.empty() &&\n"
            "          isCanonicalSoaBorrowedWrapperHelper(methodPath) &&\n"
            "          resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&\n"
            "        resolvedPath.find(\"__t\") != std::string::npos) {\n"
            "      expr.templateArgs.clear();\n"
            "    }") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "if (isCanonicalSoaBorrowedWrapperHelper(methodPath) &&\n"
            "          methodPath.find(\"__t\") != std::string::npos) {\n"
            "        expr.templateArgs.clear();\n"
            "      }") !=
        std::string::npos);
  CHECK(collectionTypeHelpersSource.find(
            "normalizedPath.rfind(\"/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(collectionTypeHelpersSource.find(
            "\"/std/collections/soa_vector/\" +") !=
        std::string::npos);
  CHECK(collectionTypeHelpersSource.find(
            "normalizedPath.rfind(\"/std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(collectionTypeHelpersSource.find(
            "\"/soa_vector/\" +") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "isExperimentalSoaVectorTypePath(value)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "if (preferred.rfind(\"/soa_vector/\", 0) == 0 && defs.count(preferred) == 0)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "rawMethodName.rfind(\"soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "rawMethodName.rfind(\"std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "receiverTypeName == \"soa_vector\"") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "isRemovedBorrowedSoaCompatibilityHelper(helperName)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "std::string_view mapCompatibilityHelperBase(std::string_view helperName)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "isRemovedMapCompatibilityHelper(mapCompatibilityHelperBase(suffix))") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "const std::string compatibilityPath = \"/map/\" + std::string(helperBase);") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "\"/std/collections/soa_vector/\" + suffix") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "if (path.rfind(\"/soa_vector/\", 0) == 0)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "if (path.rfind(\"/std/collections/soa_vector/\", 0) == 0)") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "\"/soa_vector/\" + suffix") !=
        std::string::npos);
  CHECK(templateMonomorphCollectionCompatibilityPathsSource.find(
            "value == \"std/collections/experimental_soa_vector/SoaVector\" || value == \"SoaVector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPath.rfind(\"/std/collections/experimental_soa_vector_conversions/\", 0)") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "const std::string canonicalSoaRefPath =") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "canonicalizeLegacySoaRefHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "auto preferredExperimentalSoaHelper = [](const std::string &candidatePath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,\n"
            "        candidatePath,\n"
            "        \"/std/collections/experimental_soa_vector/\");") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "preferredExperimentalSoaHelper(canonicalSoaCountPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "preferredExperimentalSoaHelper(canonicalSoaGetPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "preferredExperimentalSoaHelper(canonicalSoaRefPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "mapsToBorrowedSoaHelper(canonicalSoaCountPath) ||\n"
            "      mapsToBorrowedSoaHelper(canonicalSoaGetPath) ||\n"
            "      mapsToBorrowedSoaHelper(canonicalSoaRefPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/to_aos_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "soaVectorToAosRef") ==
        std::string::npos);
  CHECK(mapConstructorHelpersSource.find(
            "std::string experimentalMapConstructorHelperPath(size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "#include \"MapConstructorHelpers.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "metadataBackedExperimentalMapConstructorRewritePath(") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "metadataBackedExperimentalVectorConstructorCompatibilityPath(") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "resolvedPath == \"/vector\" || resolvedPath == \"/std/collections/vector/vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "resolvedPath == \"/std/collections/vector/vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "if (resolvedPath == \"/std/collections/vectorSingle\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTypeHelpersSource.find(
            "metadataBackedExperimentalMapConstructorRewritePath(resolvedPath, 0)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTypeHelpersSource.find(
            "normalizedPath == \"/std/collections/mapNew\"") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTypeHelpersSource.find(
            "return canonicalMapConstructorToExperimental(normalizedPath);") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnRewritesSource.find(
            "void rewriteExperimentalConstructorReturnTree(Expr &candidate, RewriteCurrentFn &&rewriteCurrent)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "struct ExperimentalCollectionReturnRewritePlan") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(const Definition &def)") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "bool tryAppendDefinitionParameterBinding(Expr &param,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "bool rewriteDefinitionParameters(std::vector<Expr> &parameters,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "void recordDefinitionStatementBindingLocal(Expr &stmt,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionReturnOrchestrationSource.find(
            "bool isDefinitionReturnPathStatement(const Expr &stmt,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionReturnOrchestrationSource.find(
            "bool rewriteDefinitionReturnConstructors(Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalMapConstructorValue(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalMapReturnConstructors(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphExecutionRewritesSource.find(
            "bool rewriteExecutionEntry(Execution &exec, Context &ctx, std::string &error)") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionRewritesSource.find(
            "bool rewriteDefinition(Definition &def,") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "std::vector<Definition> collectTemplateSpecializationFamily(const std::string &basePath,") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "bool canReplaceGeneratedTemplateShell(const Definition &existingDef,") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "std::string parentPathForDefinition(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "bool specializeTemplateDefinitionFamily(const std::string &basePath,") !=
        std::string::npos);
}

TEST_CASE("emitter collection helper metadata delegation stays source locked") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                            : std::filesystem::path("..");

  const std::filesystem::path metadataHeaderPath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinMethodResolutionTypeInferenceInternal.h";
  const std::filesystem::path metadataHelpersPath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinMethodResolutionMetadataHelpers.cpp";
  const std::filesystem::path callPathHelpersPath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinCallPathHelpers.cpp";
  const std::filesystem::path methodResolutionHelpersPath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinMethodResolutionHelpers.cpp";
  const std::filesystem::path exprControlCallPathStepPath =
      repoRoot / "src" / "emitter" / "EmitterExprControlCallPathStep.cpp";

  REQUIRE(std::filesystem::exists(metadataHeaderPath));
  REQUIRE(std::filesystem::exists(metadataHelpersPath));
  REQUIRE(std::filesystem::exists(callPathHelpersPath));
  REQUIRE(std::filesystem::exists(methodResolutionHelpersPath));
  REQUIRE(std::filesystem::exists(exprControlCallPathStepPath));

  const std::string metadataHeaderSource = readText(metadataHeaderPath);
  const std::string metadataHelpersSource = readText(metadataHelpersPath);
  const std::string callPathHelpersSource = readText(callPathHelpersPath);
  const std::string methodResolutionHelpersSource = readText(methodResolutionHelpersPath);
  const std::string exprControlCallPathStepSource = readText(exprControlCallPathStepPath);

  CHECK(metadataHeaderSource.find("bool resolvePublishedCollectionSurfaceMemberToken(") !=
        std::string::npos);
  CHECK(metadataHeaderSource.find("bool resolvePublishedCollectionSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(metadataHeaderSource.find("bool isRemovedCollectionMethodAliasPath(") !=
        std::string::npos);
  CHECK(metadataHeaderSource.find("bool removedCollectionAliasNeedsDefinitionPath(") !=
        std::string::npos);

  CHECK(metadataHelpersSource.find("findPublishedCollectionSurfaceMetadata(") !=
        std::string::npos);
  CHECK(metadataHelpersSource.find("rebuildScopedCollectionHelperPath(") !=
        std::string::npos);
  CHECK(metadataHelpersSource.find("resolveStdlibSurfaceMemberName(*metadata, normalizedToken)") !=
        std::string::npos);
  CHECK(metadataHelpersSource.find("bool isRemovedCollectionMethodAliasPath(") !=
        std::string::npos);
  CHECK(metadataHelpersSource.find("bool removedCollectionAliasNeedsDefinitionPath(") !=
        std::string::npos);

  CHECK(callPathHelpersSource.find("resolvePublishedCollectionSurfaceMemberToken(") !=
        std::string::npos);
  CHECK(callPathHelpersSource.find("resolvePublishedCollectionSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(callPathHelpersSource.find("bool allowsVectorStdlibCompatibilitySuffix(") ==
        std::string::npos);

  CHECK(methodResolutionHelpersSource.find("isRemovedCollectionMethodAliasPath(") !=
        std::string::npos);
  CHECK(methodResolutionHelpersSource.find("removedCollectionAliasNeedsDefinitionPath(") !=
        std::string::npos);
  CHECK(methodResolutionHelpersSource.find("bool isRemovedCollectionMethodAlias(") ==
        std::string::npos);
  CHECK(methodResolutionHelpersSource.find("bool removedCollectionAliasNeedsDefinition(") ==
        std::string::npos);

  CHECK(exprControlCallPathStepSource.find("normalizeMapImportAliasPath(importIt->second)") !=
        std::string::npos);
  CHECK(exprControlCallPathStepSource.find("std::string normalizeMapImportAliasPath(") ==
        std::string::npos);
}

TEST_CASE("emitter collection fallback helpers stay scoped path aware") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                            : std::filesystem::path("..");
  const std::filesystem::path fallbackHelpersPath =
      repoRoot / "src" / "emitter" / "EmitterExprCollectionFallbackHelpers.h";

  REQUIRE(std::filesystem::exists(fallbackHelpersPath));

  const std::string fallbackHelpersSource = readText(fallbackHelpersPath);

  CHECK(fallbackHelpersSource.find("std::string normalized = resolveExprPath(candidate);") !=
        std::string::npos);
  CHECK(fallbackHelpersSource.find("const std::string resolvedPath = resolveExprPath(candidate);") !=
        std::string::npos);
  CHECK(fallbackHelpersSource.find("std::string normalized = candidate.name;") ==
        std::string::npos);
}

TEST_SUITE_END();
