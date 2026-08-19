#include "third_party/doctest.h"

#include "test_ir_pipeline_validation_ir_lowerer_statement_call_helper_buffer_store_direct_calls_helper_lowerer_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper emits direct calls: core method and direct call paths") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.methodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &f.methodDef;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              ++inlineCalls;
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.defStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &f.def; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = false;
              return true;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              ++inlineCalls;
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  // TODO-4754 fix: emitInlineDefinitionCall(..., requireValue=false) is
  // responsible for balancing its own discarded return value (it already
  // does so for the real-call path), so the statement-call helper must not
  // add a second Pop on top of it - doing so caused a VM stack underflow
  // for any statement-position call to a real (non-inlined) function whose
  // result was discarded.
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &f.mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.namespacedMapInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &f.mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &f.mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.find("missing semantic-product method-call target: insert") != std::string::npos);
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &f.mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &f.mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessAliasInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapInsert") {
                return &f.mapInsertAliasDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalAliasPathInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/MapInsert__generated") {
                return &f.mapInsertGeneratedPascalAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalRefAliasPathInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/MapInsertRef__generated") {
                return &f.mapInsertGeneratedPascalRefAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "insert__generated") {
                return &f.mapInsertGeneratedBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapInsert__generated") {
                return &f.mapInsertGeneratedAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "MapInsert__generated") {
                return &f.mapInsertGeneratedPascalAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalRefAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "MapInsertRef__generated") {
                return &f.mapInsertGeneratedPascalRefAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertMethodDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

}

TEST_CASE("ir lowerer statement call helper emits direct calls: core method and direct call paths (continued)") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/insert__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedPascalAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/MapInsert__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedAliasMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapInsert__generated" &&
                  callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "mapInsert__generated");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/mapInsert__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalRefAliasMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "MapInsertRef__generated" &&
                  callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedPascalRefAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "MapInsertRef__generated");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/MapInsertRef__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedAliasSlashMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall &&
                  callExpr.name == "/std/collections/mapInsert__generated" &&
                  callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/mapInsert__generated");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/mapInsert__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertFieldAccessGeneratedPascalRefAliasSlashMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall &&
                  callExpr.name == "/std/collections/MapInsertRef__generated" &&
                  callExpr.args.size() == 3) {
                return &f.mapInsertGeneratedPascalRefAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/MapInsertRef__generated");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/MapInsertRef__generated");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertLocationLocalMapInferredStmt,
            f.localMapLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("valuesLocal") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertLocationLocalMapMethodStmt,
            f.localMapLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              CHECK(localsIn.find("valuesLocal") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackWrappedInferredStmt,
            f.mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackWrappedMethodStmt,
            f.mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackReceiverLocationInferredStmt,
            f.mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackReceiverLocationMethodStmt,
            f.mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());
}

TEST_SUITE_END();
