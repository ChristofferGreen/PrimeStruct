#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer count access helpers build bundled entry count setup") {
  primec::Definition entryDef;
  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK_FALSE(setup.hasEntryArgs);
  CHECK(setup.entryArgsName.empty());

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  primec::ir_lowerer::LocalMap locals;
  CHECK_FALSE(setup.classifiers.isEntryArgsName(entryName, locals));

  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters = {entryParam};

  error.clear();
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK(setup.hasEntryArgs);
  CHECK(setup.entryArgsName == "argv");
  CHECK(setup.classifiers.isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);
  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soaValues";
  countEntry.args = {soaName};
  countEntry.name = "count";
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/std/collections/vector/count";
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/array/count";
  CHECK_FALSE(setup.classifiers.isArrayCountCall(countEntry, locals));

  primec::Expr extraParam = entryParam;
  extraParam.name = "extra";
  entryDef.parameters = {entryParam, extraParam};
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE("ir lowerer count access helpers prefer semantic product entry args facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr staleParam;
  staleParam.name = "stale";
  primec::Transform staleTransform;
  staleTransform.name = "array";
  staleTransform.templateArgs = {"i64"};
  staleParam.transforms.push_back(staleTransform);
  entryDef.parameters = {staleParam};

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "argv",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 2,
      .sourceColumn = 3,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/argv"),
  });

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(setup.hasEntryArgs);
  CHECK(setup.entryArgsName == "argv");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer count access helpers reject removed /array/capacity alias") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  capacityCall.name = "/array/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
}

TEST_CASE("ir lowerer count access helpers classify capacity and string count") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);

  primec::ir_lowerer::LocalInfo refVecInfo;
  refVecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refVecInfo.referenceToVector = true;
  locals.emplace("refVec", refVecInfo);

  primec::ir_lowerer::LocalInfo ptrVecInfo;
  ptrVecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  ptrVecInfo.pointerToVector = true;
  locals.emplace("ptrVec", ptrVecInfo);

  primec::ir_lowerer::LocalInfo vectorArgsInfo;
  vectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  vectorArgsInfo.isArgsPack = true;
  vectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("vectorArgs", vectorArgsInfo);

  primec::ir_lowerer::LocalInfo refVectorArgsInfo;
  refVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  refVectorArgsInfo.isArgsPack = true;
  refVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refVectorArgsInfo.referenceToVector = true;
  locals.emplace("refVectorArgs", refVectorArgsInfo);

  primec::ir_lowerer::LocalInfo ptrVectorArgsInfo;
  ptrVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  ptrVectorArgsInfo.isArgsPack = true;
  ptrVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  ptrVectorArgsInfo.pointerToVector = true;
  locals.emplace("ptrVectorArgs", ptrVectorArgsInfo);

  primec::ir_lowerer::LocalInfo soaVectorArgsInfo;
  soaVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  soaVectorArgsInfo.isArgsPack = true;
  soaVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  soaVectorArgsInfo.pointerToVector = true;
  soaVectorArgsInfo.isSoaVector = true;
  locals.emplace("soaVectorArgs", soaVectorArgsInfo);

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr refVecName;
  refVecName.kind = primec::Expr::Kind::Name;
  refVecName.name = "refVec";
  capacityCall.name = "capacity";
  capacityCall.args = {refVecName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr ptrVecName;
  ptrVecName.kind = primec::Expr::Kind::Name;
  ptrVecName.name = "ptrVec";
  capacityCall.args = {ptrVecName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr vectorArgsName;
  vectorArgsName.kind = primec::Expr::Kind::Name;
  vectorArgsName.name = "vectorArgs";
  primec::Expr zeroIndex;
  zeroIndex.kind = primec::Expr::Kind::Literal;
  zeroIndex.literalValue = 0;
  primec::Expr indexedVector;
  indexedVector.kind = primec::Expr::Kind::Call;
  indexedVector.name = "at";
  indexedVector.args = {vectorArgsName, zeroIndex};
  capacityCall.args = {indexedVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr refVectorArgsName;
  refVectorArgsName.kind = primec::Expr::Kind::Name;
  refVectorArgsName.name = "refVectorArgs";
  primec::Expr indexedRefVector;
  indexedRefVector.kind = primec::Expr::Kind::Call;
  indexedRefVector.name = "at";
  indexedRefVector.args = {refVectorArgsName, zeroIndex};
  primec::Expr derefRefVector;
  derefRefVector.kind = primec::Expr::Kind::Call;
  derefRefVector.name = "dereference";
  derefRefVector.args = {indexedRefVector};
  capacityCall.args = {derefRefVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr ptrVectorArgsName;
  ptrVectorArgsName.kind = primec::Expr::Kind::Name;
  ptrVectorArgsName.name = "ptrVectorArgs";
  primec::Expr indexedPtrVector;
  indexedPtrVector.kind = primec::Expr::Kind::Call;
  indexedPtrVector.name = "at";
  indexedPtrVector.args = {ptrVectorArgsName, zeroIndex};
  primec::Expr derefPtrVector;
  derefPtrVector.kind = primec::Expr::Kind::Call;
  derefPtrVector.name = "dereference";
  derefPtrVector.args = {indexedPtrVector};
  capacityCall.args = {derefPtrVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr soaValuesName;
  soaValuesName.kind = primec::Expr::Kind::Name;
  soaValuesName.name = "soaValues";
  primec::Expr toAosCall;
  toAosCall.kind = primec::Expr::Kind::Call;
  toAosCall.name = "/std/collections/soa_vector/to_aos";
  toAosCall.args = {soaValuesName};
  capacityCall.args = {toAosCall};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr soaVectorArgsName;
  soaVectorArgsName.kind = primec::Expr::Kind::Name;
  soaVectorArgsName.name = "soaVectorArgs";
  primec::Expr indexedSoaVector;
  indexedSoaVector.kind = primec::Expr::Kind::Call;
  indexedSoaVector.name = "at";
  indexedSoaVector.args = {soaVectorArgsName, zeroIndex};
  primec::Expr derefSoaVector;
  derefSoaVector.kind = primec::Expr::Kind::Call;
  derefSoaVector.name = "dereference";
  derefSoaVector.args = {indexedSoaVector};
  capacityCall.args = {derefSoaVector};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr stringCount;
  stringCount.kind = primec::Expr::Kind::Call;
  stringCount.name = "count";
  primec::Expr literal;
  literal.kind = primec::Expr::Kind::StringLiteral;
  literal.stringValue = "\"ok\"utf8";
  stringCount.args = {literal};
  CHECK(primec::ir_lowerer::isStringCountCall(stringCount, locals));

  primec::ir_lowerer::LocalInfo stringInfo;
  stringInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  stringInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  locals.emplace("text", stringInfo);
  primec::Expr textName;
  textName.kind = primec::Expr::Kind::Name;
  textName.name = "text";
  stringCount.args = {textName};
  CHECK(primec::ir_lowerer::isStringCountCall(stringCount, locals));
  stringCount.name = "/vector/count";
  CHECK_FALSE(primec::ir_lowerer::isStringCountCall(stringCount, locals));

  capacityCall.name = "count";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
}

TEST_CASE("ir lowerer count access helpers emit string count calls") {
  using Result = primec::ir_lowerer::StringCountCallEmitResult;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "text";
  countCall.args = {target};

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t emittedLength = -1;

  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::NotHandled);

  countCall.args.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "count requires exactly one argument");

  countCall.args = {target};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "native backend only supports count() on string literals or string bindings");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 7;
              length = static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
              return true;
            },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "native backend string too large for count()");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 3;
              length = 42;
              return true;
            },
            [&](int32_t length) { emittedLength = length; },
            error) == Result::Emitted);
  CHECK(emittedLength == 42);
}

TEST_CASE("ir lowerer count access helpers emit dynamic string count calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Call;
  targetExpr.name = "at";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.args = {targetExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::String;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 3});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
}

TEST_CASE("ir lowerer count access helpers emit canonical runtime string count calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo textInfo;
  textInfo.index = 11;
  textInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  textInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  textInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  locals.emplace("text", textInfo);

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "text";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/count";
  callExpr.args = {targetExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK_FALSE(primec::ir_lowerer::isStringCountCall(callExpr, locals));
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &countExpr, const primec::ir_lowerer::LocalMap &countLocals) {
              return primec::ir_lowerer::isStringCountCall(countExpr, countLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &valueLocals) {
              if (valueExpr.kind == primec::Expr::Kind::Name) {
                auto it = valueLocals.find(valueExpr.name);
                if (it != valueLocals.end()) {
                  return it->second.valueKind;
                }
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::LoadLocal, 11});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 11);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
}

TEST_CASE("ir lowerer call helpers lower soa_vector count calls") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo soaInfo;
  soaInfo.kind = LocalInfo::Kind::Value;
  soaInfo.index = 5;
  soaInfo.valueKind = LocalInfo::ValueKind::Unknown;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);

  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soaValues";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args = {soaName};

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };
  int nextLocal = 32;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            countCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(callExpr, callLocals);
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isStringCountCall(callExpr, callLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &emitLocals) {
              if (expr.kind != primec::Expr::Kind::Name) {
                return false;
              }
              auto it = emitLocals.find(expr.name);
              if (it == emitLocals.end()) {
                return false;
              }
              emitInstruction(primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return LocalInfo::ValueKind::Int32; },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() >= 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
}

TEST_SUITE_END();
