#include "third_party/doctest.h"

#include "test_ir_pipeline_validation_ir_lowerer_statement_call_helper_buffer_store_direct_calls_helper_lowerer_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper emits direct calls: maps pack alias and wrapped receiver paths") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
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
            f.mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "AtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            f.mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            f.mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
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
            f.mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert__generated" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
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
              CHECK(callExpr.name == "insert__generated");
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
            f.mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" &&
                  callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            f.mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert__generated" && callExpr.args.size() == 3) {
                return &f.mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" &&
                  callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
              CHECK(callExpr.name == "insert__generated");
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
            f.mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "mapAt" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
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
            f.mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
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
            f.mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_ref" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "at_ref" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            f.mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe_ref" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe_ref" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            f.mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "mapAtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
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
            f.mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
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
            f.mapInsertLocationHelperInferredStmt,
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
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
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
            f.mapInsertLocationHelperMethodStmt,
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
}

TEST_SUITE_END();
