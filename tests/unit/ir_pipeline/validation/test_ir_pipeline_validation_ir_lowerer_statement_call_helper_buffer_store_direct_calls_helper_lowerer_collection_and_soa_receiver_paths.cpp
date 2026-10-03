#include "third_party/doctest.h"

#include "test_ir_pipeline_validation_ir_lowerer_statement_call_helper_buffer_store_direct_calls_helper_lowerer_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper emits direct calls: collection and soa receiver paths") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at") {
                return &f.mapAtArgsPackDef;
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
            f.mapInsertArgsPackNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/map/at") {
                return &f.mapAtArgsPackDef;
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
            f.mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe") {
                return &f.mapAtArgsPackUnsafeDef;
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
            f.mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/map/at_unsafe") {
                return &f.mapAtArgsPackUnsafeDef;
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
            f.mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_ref") {
                return &f.mapAtArgsPackRefDef;
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
            f.mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/map/at_ref") {
                return &f.mapAtArgsPackRefDef;
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
            f.mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe_ref") {
                return &f.mapAtArgsPackUnsafeRefDef;
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
            f.mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/map/at_unsafe_ref") {
                return &f.mapAtArgsPackUnsafeRefDef;
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
            f.mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafeRef__generated") {
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
            f.mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/std/collections/mapAtUnsafeRef__generated") {
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
            f.mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "at_ref") {
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
            f.mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "at_ref") {
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
            f.mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
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
}

TEST_CASE("ir lowerer statement call helper emits direct calls: collection and soa receiver paths (continued)") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  const DirectCallStatementFixtures f = loadDirectCallStatementFixtures();
  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;

  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            f.mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "mapAtUnsafeRef__generated") {
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
            f.mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
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

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
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

  expectDirectCallEmpty(inlineCalls, instructions, error, EmitResult::NotMatched, 0,
            f.mapInsertArgsPackAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAt") {
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
            f.mapInsertArgsPackAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/std/collections/mapAt") {
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
            f.mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafe") {
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
            f.mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.name == "/std/collections/mapAtUnsafe") {
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
            f.mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at" && callExpr.args.size() == 2) {
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
            f.mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "at" && callExpr.args.size() == 2) {
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
            f.mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe" && callExpr.args.size() == 2) {
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
            f.mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe" && callExpr.args.size() == 2) {
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
            f.mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt,
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "At" && callExpr.args.size() == 2) {
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
            f.mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt,
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
              if (callExpr.isMethodCall && callExpr.name == "At" && callExpr.args.size() == 2) {
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
}

TEST_SUITE_END();
