#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer on_error helpers wire definition handlers") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  mainDef.transforms.push_back(onError);
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().kind == primec::Expr::Kind::Literal);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 1);

  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
}

TEST_CASE("ir lowerer on_error helpers wire definition handlers from call adapters") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  mainDef.transforms.push_back(onError);
  program.definitions.push_back(mainDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/main", &program.definitions[1]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};
  const auto callResolutionAdapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinitionFromCallResolutionAdapters(
      program, callResolutionAdapters, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().kind == primec::Expr::Kind::Literal);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 1);

  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
}

TEST_CASE("ir lowerer on_error helpers reject missing resolution adapters") {
  std::vector<primec::Transform> transforms;
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  transforms.push_back(onError);

  std::optional<primec::ir_lowerer::OnErrorHandler> handler;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::parseOnErrorTransform(
      transforms, "", "/main", {}, {}, handler, error));
  CHECK(error == "internal error: missing on_error resolution adapters on /main");
  CHECK_FALSE(handler.has_value());
}

TEST_CASE("ir lowerer on_error helpers prefer semantic-product metadata") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  handlerDef.semanticNodeId = 21;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  mainDef.semanticNodeId = 22;
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 21,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 22,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/semantic/main",
      .returnKind = "void",
      .errorType = "StaleError",
      .boundArgCount = 1,
      .boundArgTexts = {"99i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 22,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {primec::semanticProgramInternCallTargetString(semanticProgram, "2i32")},
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->errorType == "FileError");
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 2);
}

TEST_CASE("ir lowerer on_error helpers skip semantic-product sum definitions") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  handlerDef.semanticNodeId = 21;
  program.definitions.push_back(handlerDef);

  primec::Definition choiceDef;
  choiceDef.fullPath = "/Choice";
  choiceDef.namespacePrefix = "";
  choiceDef.semanticNodeId = 22;
  primec::Transform sumTransform;
  sumTransform.name = "sum";
  choiceDef.transforms.push_back(sumTransform);
  program.definitions.push_back(choiceDef);

  primec::Definition generatedResultDef;
  generatedResultDef.fullPath = "/std/result/Result__arity2__tabc";
  generatedResultDef.namespacePrefix = "";
  generatedResultDef.semanticNodeId = 24;
  program.definitions.push_back(generatedResultDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  mainDef.semanticNodeId = 23;
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) {
    return path == "/handler" || path == "/Choice" || path == "/main";
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 21,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 23,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = generatedResultDef.fullPath,
      .category = "sum",
      .semanticNodeId = generatedResultDef.semanticNodeId,
  });
  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = generatedResultDef.fullPath,
      .variantCount = 2,
      .semanticNodeId = generatedResultDef.semanticNodeId,
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());
  CHECK(onErrorByDef.count("/Choice") == 0);
  CHECK(onErrorByDef.count(generatedResultDef.fullPath) == 0);
  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
  REQUIRE(onErrorByDef.count("/main") == 1);
  CHECK_FALSE(onErrorByDef.at("/main").has_value());
}

TEST_CASE("ir lowerer on_error helpers reject missing semantic bound arg ids") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  handlerDef.semanticNodeId = 21;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  mainDef.semanticNodeId = 22;
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 21,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 22,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/semantic/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"2i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 22,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {primec::InvalidSymbolId},
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error == "missing semantic-product on_error bound arg text id: /main");
}

TEST_CASE("ir lowerer on_error helpers require definition semantic ids for semantic-product metadata") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 21,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 22,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"2i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error == "missing semantic-product on_error fact: /main");
}

TEST_CASE("ir lowerer on_error helpers reject definition-path fallback facts") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 21,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 22,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/legacy/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"2i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error == "missing semantic-product on_error fact: /main");
}

TEST_CASE("ir lowerer on_error helpers use semantic-id facts without definition-path fallback") {
  primec::Program program;

  primec::Definition semanticHandlerDef;
  semanticHandlerDef.fullPath = "/handler_semantic";
  semanticHandlerDef.namespacePrefix = "";
  program.definitions.push_back(semanticHandlerDef);

  primec::Definition fallbackHandlerDef;
  fallbackHandlerDef.fullPath = "/handler_fallback";
  fallbackHandlerDef.namespacePrefix = "";
  program.definitions.push_back(fallbackHandlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  mainDef.semanticNodeId = 222;
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) {
    return path == "/handler_semantic" || path == "/handler_fallback" || path == "/main";
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 221,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler_semantic"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 223,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler_fallback"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler_semantic",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 222,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/semantic/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"2i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 222,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler_semantic"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/legacy/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"3i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler_fallback"),
  });

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, &semanticProgram, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());
  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler_semantic");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 2);
}

TEST_CASE("ir lowerer on_error helpers build bundled entry call and on_error setup") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/main", &program.definitions[2]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::ir_lowerer::EntryCallOnErrorSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCallOnErrorSetup(
      program, program.definitions[2], true, defMap, importAliases, setup, error));
  CHECK(error.empty());
  CHECK(setup.hasTailExecution);
  CHECK(setup.callResolutionAdapters.resolveExprPath(tailCall) == "/callee");
  CHECK(setup.callResolutionAdapters.definitionExists("/callee"));

  REQUIRE(setup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.onErrorByDefinition.at("/main")->handlerPath == "/handler");
  REQUIRE(setup.onErrorByDefinition.at("/main")->boundArgs.size() == 1);
  CHECK(setup.onErrorByDefinition.at("/main")->boundArgs.front().literalValue == 1);

  primec::Program badProgram = program;
  badProgram.definitions[2].transforms.front().templateArgs[1] = "missing";
  const std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {"/handler", &badProgram.definitions[0]},
      {"/callee", &badProgram.definitions[1]},
      {"/main", &badProgram.definitions[2]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryCallOnErrorSetup(
      badProgram, badProgram.definitions[2], true, badDefMap, importAliases, setup, error));
  CHECK(error == "unknown on_error handler: /missing");
}

TEST_CASE("ir lowerer on_error entry setup validates semantic bound arg counts") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  handlerDef.semanticNodeId = 31;
  program.definitions.push_back(handlerDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  entryDef.semanticNodeId = 32;
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/main", &program.definitions[1]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 31,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 2,
      .semanticNodeId = 32,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/semantic/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 2,
      .boundArgTexts = {"1i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 32,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });

  primec::ir_lowerer::EntryCallOnErrorSetup setup;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildEntryCallOnErrorSetup(
      program, program.definitions[1], true, defMap, importAliases, &semanticProgram, setup, error));
  CHECK(error == "semantic-product on_error bound arg mismatch on /main");
}

TEST_CASE("ir lowerer on_error helpers build bundled entry count and call/on_error setup") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/main", &program.definitions[2]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::ir_lowerer::EntryCountCallOnErrorSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountCallOnErrorSetup(
      program, program.definitions[2], true, defMap, importAliases, setup, error));
  CHECK(setup.countAccessSetup.hasEntryArgs);
  CHECK(setup.countAccessSetup.entryArgsName == "argv");
  CHECK(setup.callOnErrorSetup.hasTailExecution);
  CHECK(setup.callOnErrorSetup.callResolutionAdapters.resolveExprPath(tailCall) == "/callee");
  REQUIRE(setup.callOnErrorSetup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.callOnErrorSetup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.callOnErrorSetup.onErrorByDefinition.at("/main")->handlerPath == "/handler");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[2].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/main", &badEntryProgram.definitions[2]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountCallOnErrorSetup(
      badEntryProgram, badEntryProgram.definitions[2], true, badEntryDefMap, importAliases, setup, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled entry and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  std::string error;
  primec::ir_lowerer::EntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      program,
      program.definitions[4],
      true,
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());
  CHECK(setup.entryCountCallOnErrorSetup.countAccessSetup.hasEntryArgs);
  CHECK(setup.entryCountCallOnErrorSetup.countAccessSetup.entryArgsName == "argv");
  CHECK(setup.entryCountCallOnErrorSetup.callOnErrorSetup.hasTailExecution);
  REQUIRE(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.at("/main")->handlerPath ==
        "/handler");

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setup.setupMathTypeStructAndUninitializedResolutionSetup.setupMathAndBindingAdapters
            .setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");
  CHECK(setup.setupMathTypeStructAndUninitializedResolutionSetup.setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.setupMathTypeStructAndUninitializedResolutionSetup
              .setupTypeStructAndUninitializedResolutionSetup
              .structAndUninitializedResolutionSetup
              .uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
                  "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[4].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/pkg/Container", &badEntryProgram.definitions[2]},
      {"/pkg/MyStruct", &badEntryProgram.definitions[3]},
      {"/main", &badEntryProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      badEntryProgram,
      badEntryProgram.definitions[4],
      true,
      badEntryDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled runtime entry and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;
  primec::ir_lowerer::RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      true,
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());

  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .countAccessSetup.hasEntryArgs);
  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .callOnErrorSetup.hasTailExecution);
  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  CHECK(setup.runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString("hello") == 0);
  setup.runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "array index out of bounds");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[4].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/pkg/Container", &badEntryProgram.definitions[2]},
      {"/pkg/MyStruct", &badEntryProgram.definitions[3]},
      {"/main", &badEntryProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badEntryProgram,
      badEntryProgram.definitions[4],
      true,
      badEntryDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled entry return, runtime entry, and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);

  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Result<i64, FileError>"};
  entryDef.transforms.push_back(returnTransform);

  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);

  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      "/main",
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());
  CHECK(setup.entryReturnConfig.hasReturnTransform);
  CHECK_FALSE(setup.entryReturnConfig.returnsVoid);
  CHECK(setup.entryReturnConfig.hasResultInfo);
  CHECK(setup.entryReturnConfig.resultInfo.isResult);

  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .countAccessSetup.hasEntryArgs);
  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString("hello") == 0);
  setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      .runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);

  primec::Program badProgram = program;
  badProgram.definitions[4].transforms.clear();
  primec::Transform badReturn;
  badReturn.name = "return";
  badReturn.templateArgs = {"array<string>"};
  badProgram.definitions[4].transforms.push_back(badReturn);
  const std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {"/handler", &badProgram.definitions[0]},
      {"/callee", &badProgram.definitions[1]},
      {"/pkg/Container", &badProgram.definitions[2]},
      {"/pkg/MyStruct", &badProgram.definitions[3]},
      {"/main", &badProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badProgram,
      badProgram.definitions[4],
      "/main",
      badDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_SUITE_END();
