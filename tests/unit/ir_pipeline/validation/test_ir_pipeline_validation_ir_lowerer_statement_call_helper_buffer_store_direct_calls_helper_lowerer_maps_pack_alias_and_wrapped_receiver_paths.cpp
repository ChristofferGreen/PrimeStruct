#include "third_party/doctest.h"

#include "test_ir_pipeline_validation_ir_lowerer_statement_call_helper_buffer_store_direct_calls_helper_lowerer_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper emits direct calls: maps pack alias and wrapped receiver paths") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert__generated", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert__generated", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt" && callExpr.args.size() == 2) {
                return &f.mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

}

TEST_CASE("ir lowerer statement call helper emits direct calls: maps pack alias and wrapped receiver paths (continued)") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafe" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_ref" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe_ref" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtRef" && callExpr.args.size() == 2) {
                return &f.mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafeRef" && callExpr.args.size() == 2) {
                return &f.mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertLocationHelperInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &f.mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &f.mapInsertBuiltinDef;
              }
              return nullptr;
            },
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "/std/collections/map/insert_builtin", false,
                                   "/std/collections/map/insert_builtin", {"i32", "i32"}));

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
            insertBuiltinReturnInfo,
            makeInlineCallChecker(inlineCalls, "insert", true, "/std/collections/mapInsert", {}),
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());
}

TEST_SUITE_END();
