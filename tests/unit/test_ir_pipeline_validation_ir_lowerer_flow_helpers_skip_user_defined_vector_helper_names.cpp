#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer flow helpers skip user-defined vector helper names") {
  using EmitResult = primec::ir_lowerer::VectorStatementHelperEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "v";

  primec::Expr value;
  value.kind = primec::Expr::Kind::Literal;
  value.intWidth = 32;
  value.literalValue = 9;

  primec::Expr pushCall;
  pushCall.kind = primec::Expr::Kind::Call;
  pushCall.name = "push";
  pushCall.args = {target, value};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = Kind::Vector;
  vectorInfo.valueKind = ValueKind::Int32;
  vectorInfo.isMutable = true;
  vectorInfo.index = 2;
  locals.emplace("v", vectorInfo);

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 10;
  std::string error;

  int vectorMethodProbeCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            pushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 1);

  primec::Expr aliasPushCall = pushCall;
  aliasPushCall.name = "/vector/push";
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            aliasPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 0);

  primec::Expr stdlibAliasPushCall = pushCall;
  stdlibAliasPushCall.name = "/std/collections/vector/push";
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            stdlibAliasPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());
  CHECK(vectorMethodProbeCalls == 0);

  primec::Expr namespacedStdlibPushCall = pushCall;
  namespacedStdlibPushCall.namespacePrefix = "/std/collections/vector";
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            namespacedStdlibPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());
  CHECK(vectorMethodProbeCalls == 0);

  primec::Expr positionalReorderedPushCall = pushCall;
  positionalReorderedPushCall.args = {value, target};
  positionalReorderedPushCall.argNames.clear();
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            positionalReorderedPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 1);

  primec::Expr valueName;
  valueName.kind = primec::Expr::Kind::Name;
  valueName.name = "valueName";
  primec::Expr positionalNameReorderedPushCall = pushCall;
  positionalNameReorderedPushCall.args = {valueName, target};
  positionalNameReorderedPushCall.argNames.clear();
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            positionalNameReorderedPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 1);

  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            positionalNameReorderedPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");
  CHECK(instructions.empty());

  primec::Expr namedPushCall = pushCall;
  namedPushCall.args = {value, target};
  namedPushCall.argNames = {std::string("value"), std::string("values")};
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            namedPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK_FALSE(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 1);

  primec::Expr labeledFallbackPushCall = pushCall;
  labeledFallbackPushCall.args = {target, valueName};
  labeledFallbackPushCall.argNames = {std::string("value"), std::string("values")};
  int namedProbeCalls = 0;
  int namedMatchedCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            labeledFallbackPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              ++namedProbeCalls;
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "v") {
                return false;
              }
              ++namedMatchedCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());
  CHECK(namedProbeCalls == 2);
  CHECK(namedMatchedCalls == 0);

  primec::Expr tempReceiver;
  tempReceiver.kind = primec::Expr::Kind::Call;
  tempReceiver.name = "wrapVector";
  primec::Expr tempPushCall = pushCall;
  tempPushCall.args = {tempReceiver, value};
  vectorMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            tempPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Call ||
                  candidate.args.front().name != "wrapVector") {
                return false;
              }
              ++vectorMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
  CHECK(vectorMethodProbeCalls == 1);

  primec::Expr reorderedTempPushCall = pushCall;
  reorderedTempPushCall.args = {tempReceiver, target};
  reorderedTempPushCall.argNames.clear();
  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            reorderedTempPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              return candidate.isMethodCall && !candidate.args.empty() &&
                     candidate.args.front().kind == primec::Expr::Kind::Name &&
                     candidate.args.front().name == "v";
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");
  CHECK(instructions.empty());

  primec::Expr soaTarget;
  soaTarget.kind = primec::Expr::Kind::Name;
  soaTarget.name = "soa";

  primec::Expr soaPushCall = pushCall;
  soaPushCall.args = {soaTarget, value};

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = Kind::Value;
  soaInfo.valueKind = ValueKind::Unknown;
  soaInfo.isMutable = true;
  soaInfo.isSoaVector = true;
  soaInfo.index = 9;
  locals.emplace("soa", soaInfo);

  int soaMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            soaPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "soa") {
                return false;
              }
              ++soaMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK_FALSE(error.empty());
  CHECK(instructions.empty());
  CHECK(soaMethodProbeCalls == 1);

  primec::Expr namedSoaPushCall = soaPushCall;
  namedSoaPushCall.args = {value, soaTarget};
  namedSoaPushCall.argNames = {std::string("value"), std::string("values")};
  soaMethodProbeCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            namedSoaPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &candidate) {
              if (!candidate.isMethodCall || candidate.name != "push" || candidate.args.empty() ||
                  candidate.args.front().kind != primec::Expr::Kind::Name ||
                  candidate.args.front().name != "soa") {
                return false;
              }
              ++soaMethodProbeCalls;
              return true;
            },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK_FALSE(error.empty());
  CHECK(instructions.empty());
  CHECK(soaMethodProbeCalls == 1);
}

TEST_CASE("ir lowerer flow helpers resolve buffer init info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Call;
  bufferExpr.name = "buffer";
  bufferExpr.templateArgs = {"i64"};

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 3;
  bufferExpr.args.push_back(countExpr);

  primec::ir_lowerer::BufferInitInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::resolveBufferInitInfo(
      bufferExpr,
      [](const std::string &typeName) {
        if (typeName == "i64") {
          return ValueKind::Int64;
        }
        if (typeName == "f32") {
          return ValueKind::Float32;
        }
        if (typeName == "string") {
          return ValueKind::String;
        }
        return ValueKind::Unknown;
      },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.count == 3);
  CHECK(info.elemKind == ValueKind::Int64);
  CHECK(info.zeroOpcode == primec::IrOpcode::PushI64);

  primec::Expr wrongTemplateExpr = bufferExpr;
  wrongTemplateExpr.templateArgs.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      wrongTemplateExpr,
      [](const std::string &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer requires exactly one template argument");

  primec::Expr wrongCountExpr = bufferExpr;
  wrongCountExpr.args.front().literalValue = 2147483648ull;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      wrongCountExpr,
      [](const std::string &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer size out of range");

  primec::Expr unsupportedElemExpr = bufferExpr;
  unsupportedElemExpr.templateArgs = {"string"};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      unsupportedElemExpr,
      [](const std::string &typeName) {
        if (typeName == "string") {
          return ValueKind::String;
        }
        return ValueKind::Unknown;
      },
      info,
      error));
  CHECK(error == "buffer requires numeric/bool element type");
}


TEST_SUITE_END();
