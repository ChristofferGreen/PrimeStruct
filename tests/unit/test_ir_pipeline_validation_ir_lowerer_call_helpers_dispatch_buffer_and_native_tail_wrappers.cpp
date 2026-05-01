#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers dispatch buffer and native tail wrappers") {
  using BufferResult = primec::ir_lowerer::BufferBuiltinDispatchResult;
  using NativeResult = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo arrayInfo{};
  arrayInfo.kind = LocalInfo::Kind::Array;
  arrayInfo.index = 9;
  arrayInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("arr", arrayInfo);

  LocalInfo vectorInfo{};
  vectorInfo.kind = LocalInfo::Kind::Vector;
  vectorInfo.index = 11;
  vectorInfo.valueKind = LocalInfo::ValueKind::Unknown;
  locals.emplace("vec", vectorInfo);

  LocalInfo soaInfo;
  soaInfo.kind = LocalInfo::Kind::Value;
  soaInfo.index = 12;
  soaInfo.valueKind = LocalInfo::ValueKind::Unknown;
  soaInfo.isSoaVector = true;
  locals.emplace("soa", soaInfo);

  primec::Expr arrName;
  arrName.kind = primec::Expr::Kind::Name;
  arrName.name = "arr";

  primec::Expr vecName;
  vecName.kind = primec::Expr::Kind::Name;
  vecName.name = "vec";

  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soa";

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };

  int nextLocal = 20;
  int nextTempLocal = 100;
  std::string error;

  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Call;
  bufferExpr.name = "buffer";
  bufferExpr.templateArgs = {"i32"};
  primec::Expr twoLiteral;
  twoLiteral.kind = primec::Expr::Kind::Literal;
  twoLiteral.literalValue = 2;
  bufferExpr.args = {twoLiteral};

  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinDispatchWithLocals(
            bufferExpr,
            locals,
            [](const std::string &typeName) {
              return typeName == "i32" ? LocalInfo::ValueKind::Int32 : LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&](int32_t localCount) {
              const int32_t base = nextLocal;
              nextLocal += localCount;
              return base;
            },
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            emitInstruction,
            error) == BufferResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  instructions.clear();
  primec::Expr plainExpr;
  plainExpr.kind = primec::Expr::Kind::Call;
  plainExpr.name = "plain";
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinDispatchWithLocals(
            plainExpr,
            locals,
            [](const std::string &) { return LocalInfo::ValueKind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Unknown;
            },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            emitInstruction,
            error) == BufferResult::NotHandled);
  CHECK(instructions.empty());

  primec::Expr badBufferExpr = bufferExpr;
  badBufferExpr.templateArgs = {"string"};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinDispatchWithLocals(
            badBufferExpr,
            locals,
            [](const std::string &) { return LocalInfo::ValueKind::String; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            emitInstruction,
            error) == BufferResult::Error);
  CHECK(error == "buffer requires numeric/bool element type");

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Call;
  countExpr.name = "count";
  countExpr.args = {arrName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            countExpr,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &) {
              return callExpr.name == "count";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextTempLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == NativeResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr toSoaExpr;
  toSoaExpr.kind = primec::Expr::Kind::Call;
  toSoaExpr.name = "to_soa";
  toSoaExpr.args = {vecName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            toSoaExpr,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextTempLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == NativeResult::NotHandled);
  CHECK(error.empty());

  primec::Expr toAosExpr;
  toAosExpr.kind = primec::Expr::Kind::Call;
  toAosExpr.name = "to_aos";
  toAosExpr.args = {soaName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            toAosExpr,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextTempLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == NativeResult::NotHandled);
  CHECK(error.empty());

  primec::Expr tempReceiverCall;
  tempReceiverCall.kind = primec::Expr::Kind::Call;
  tempReceiverCall.name = "wrapValues";
  tempReceiverCall.args = {vecName};

  primec::Expr tempReceiverAccessExpr;
  tempReceiverAccessExpr.kind = primec::Expr::Kind::Call;
  tempReceiverAccessExpr.name = "at_unsafe";
  tempReceiverAccessExpr.isMethodCall = true;
  tempReceiverAccessExpr.args = {tempReceiverCall, twoLiteral};

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            tempReceiverAccessExpr,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextTempLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == NativeResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers resolve and validate map access targets") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("items", mapInfo);

  primec::Expr mapName;
  mapName.kind = primec::Expr::Kind::Name;
  mapName.name = "items";
  auto resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapName, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateMapAccessTargetInfo(resolved, "at", error));
  CHECK(error.empty());

  primec::Expr mapCtor;
  mapCtor.kind = primec::Expr::Kind::Call;
  mapCtor.name = "map";
  mapCtor.templateArgs = {"bool", "i32"};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapCtor, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::ir_lowerer::LocalInfo experimentalMapInfo;
  experimentalMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  experimentalMapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  experimentalMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  experimentalMapInfo.structTypeName = "/std/collections/experimental_map/Map";
  locals.emplace("experimentalValues", experimentalMapInfo);

  primec::Expr experimentalMapName;
  experimentalMapName.kind = primec::Expr::Kind::Name;
  experimentalMapName.name = "experimentalValues";
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(experimentalMapName, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::ir_lowerer::LocalInfo mapArgsInfo;
  mapArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  mapArgsInfo.isArgsPack = true;
  mapArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapArgsInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapArgsInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  mapArgsInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("mapArgs", mapArgsInfo);

  primec::Expr mapArgsName;
  mapArgsName.kind = primec::Expr::Kind::Name;
  mapArgsName.name = "mapArgs";
  primec::Expr mapArgsIndex;
  mapArgsIndex.kind = primec::Expr::Kind::Literal;
  mapArgsIndex.literalValue = 0;
  primec::Expr mapArgsAccess;
  mapArgsAccess.kind = primec::Expr::Kind::Call;
  mapArgsAccess.name = "at";
  mapArgsAccess.args = {mapArgsName, mapArgsIndex};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapArgsAccess, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::Expr canonicalMapArgsAccess;
  canonicalMapArgsAccess.kind = primec::Expr::Kind::Call;
  canonicalMapArgsAccess.name = "at";
  canonicalMapArgsAccess.namespacePrefix = "/std/collections/map";
  canonicalMapArgsAccess.args = {mapArgsName, mapArgsIndex};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(canonicalMapArgsAccess, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::ir_lowerer::LocalInfo mapPointerInfo;
  mapPointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  mapPointerInfo.pointerToMap = true;
  mapPointerInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  mapPointerInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapPointerInfo.structTypeName = "/std/collections/experimental_map/Map";
  locals.emplace("mapPtr", mapPointerInfo);

  primec::Expr mapPtrName;
  mapPtrName.kind = primec::Expr::Kind::Name;
  mapPtrName.name = "mapPtr";
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapPtrName, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.isWrappedMapTarget);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::Expr mapPtrDeref;
  mapPtrDeref.kind = primec::Expr::Kind::Call;
  mapPtrDeref.name = "dereference";
  mapPtrDeref.args = {mapPtrName};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapPtrDeref, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::ir_lowerer::LocalInfo mapPtrArgsInfo;
  mapPtrArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  mapPtrArgsInfo.isArgsPack = true;
  mapPtrArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  mapPtrArgsInfo.pointerToMap = true;
  mapPtrArgsInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapPtrArgsInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  mapPtrArgsInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  mapPtrArgsInfo.structTypeName = "/std/collections/experimental_map/Map";
  locals.emplace("mapPtrArgs", mapPtrArgsInfo);

  primec::Expr mapPtrArgsName;
  mapPtrArgsName.kind = primec::Expr::Kind::Name;
  mapPtrArgsName.name = "mapPtrArgs";
  primec::Expr mapPtrArgsIndex;
  mapPtrArgsIndex.kind = primec::Expr::Kind::Literal;
  mapPtrArgsIndex.literalValue = 0;
  primec::Expr mapPtrArgsAccess;
  mapPtrArgsAccess.kind = primec::Expr::Kind::Call;
  mapPtrArgsAccess.name = "at";
  mapPtrArgsAccess.args = {mapPtrArgsName, mapPtrArgsIndex};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapPtrArgsAccess, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(resolved.isWrappedMapTarget);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::Expr mapPtrArgsDeref;
  mapPtrArgsDeref.kind = primec::Expr::Kind::Call;
  mapPtrArgsDeref.name = "dereference";
  mapPtrArgsDeref.args = {mapPtrArgsAccess};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapPtrArgsDeref, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK_FALSE(resolved.isWrappedMapTarget);
  CHECK(resolved.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0);

  primec::Expr plain;
  plain.kind = primec::Expr::Kind::Name;
  plain.name = "other";
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(plain, locals);
  CHECK_FALSE(resolved.isMapTarget);
  primec::Expr helperMapName;
  helperMapName.kind = primec::Expr::Kind::Name;
  helperMapName.name = "/wrapMap__ti32_i64";
  bool helperMapCallbackInvoked = false;
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(
      helperMapName,
      locals,
      [&](const primec::Expr &targetExpr, primec::ir_lowerer::MapAccessTargetInfo &targetInfoOut) {
        helperMapCallbackInvoked = true;
        if (targetExpr.kind != primec::Expr::Kind::Name ||
            targetExpr.name != "/wrapMap__ti32_i64") {
          return false;
        }
        targetInfoOut.isMapTarget = true;
        targetInfoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        targetInfoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        return true;
      });
  CHECK(helperMapCallbackInvoked);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  error = "stale";
  CHECK(primec::ir_lowerer::validateMapAccessTargetInfo(resolved, "get", error));
  CHECK(error == "stale");

  primec::ir_lowerer::MapAccessTargetInfo untyped;
  untyped.isMapTarget = true;
  untyped.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  untyped.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapAccessTargetInfo(untyped, "at", error));
  CHECK(error == "native backend requires typed map bindings for at");

  primec::ir_lowerer::MapAccessTargetInfo stringValue;
  stringValue.isMapTarget = true;
  stringValue.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  stringValue.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  error = "stale";
  CHECK(primec::ir_lowerer::validateMapAccessTargetInfo(stringValue, "at", error));
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer call helpers resolve and validate array vector access targets") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  const std::string soaVectorStructTypePrefix =
      "/std/collections/experimental_soa_vector/SoaVector__";

  primec::ir_lowerer::LocalMap locals;
  LocalInfo arrayInfo;
  arrayInfo.kind = LocalInfo::Kind::Array;
  arrayInfo.valueKind = Kind::Int32;
  locals.emplace("arr", arrayInfo);

  LocalInfo vectorInfo;
  vectorInfo.kind = LocalInfo::Kind::Vector;
  vectorInfo.valueKind = Kind::Float64;
  locals.emplace("vec", vectorInfo);

  LocalInfo experimentalVectorInfo;
  experimentalVectorInfo.kind = LocalInfo::Kind::Value;
  experimentalVectorInfo.valueKind = Kind::Int32;
  experimentalVectorInfo.structTypeName =
      "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
  locals.emplace("experimentalVec", experimentalVectorInfo);

  LocalInfo refArrayInfo{};
  refArrayInfo.kind = LocalInfo::Kind::Reference;
  refArrayInfo.referenceToArray = true;
  refArrayInfo.valueKind = Kind::Bool;
  locals.emplace("refArr", refArrayInfo);

  LocalInfo bufferInfo{};
  bufferInfo.kind = LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = Kind::Int32;
  locals.emplace("buffer", bufferInfo);

  LocalInfo refBufferInfo{};
  refBufferInfo.kind = LocalInfo::Kind::Reference;
  refBufferInfo.referenceToBuffer = true;
  refBufferInfo.valueKind = Kind::Int32;
  locals.emplace("refBuffer", refBufferInfo);

  LocalInfo soaValuesInfo{};
  soaValuesInfo.kind = LocalInfo::Kind::Value;
  soaValuesInfo.isSoaVector = true;
  soaValuesInfo.structTypeName =
      soaVectorStructTypePrefix + "tdd6edf08e597bb3d";
  locals.emplace("soaValues", soaValuesInfo);

  LocalInfo structArgsInfo{};
  structArgsInfo.kind = LocalInfo::Kind::Array;
  structArgsInfo.isArgsPack = true;
  structArgsInfo.argsPackElementKind = LocalInfo::Kind::Value;
  structArgsInfo.structTypeName = "/pkg/Pair";
  structArgsInfo.structSlotCount = 2;
  locals.emplace("structArgs", structArgsInfo);

  LocalInfo vectorArgsInfo{};
  vectorArgsInfo.kind = LocalInfo::Kind::Array;
  vectorArgsInfo.isArgsPack = true;
  vectorArgsInfo.argsPackElementKind = LocalInfo::Kind::Vector;
  locals.emplace("vectorArgs", vectorArgsInfo);

  LocalInfo borrowedArrayArgsInfo{};
  borrowedArrayArgsInfo.kind = LocalInfo::Kind::Array;
  borrowedArrayArgsInfo.isArgsPack = true;
  borrowedArrayArgsInfo.argsPackElementKind = LocalInfo::Kind::Reference;
  borrowedArrayArgsInfo.referenceToArray = true;
  borrowedArrayArgsInfo.valueKind = Kind::Int32;
  locals.emplace("borrowedArrayArgs", borrowedArrayArgsInfo);

  LocalInfo pointerArrayArgsInfo{};
  pointerArrayArgsInfo.kind = LocalInfo::Kind::Array;
  pointerArrayArgsInfo.isArgsPack = true;
  pointerArrayArgsInfo.argsPackElementKind = LocalInfo::Kind::Pointer;
  pointerArrayArgsInfo.pointerToArray = true;
  pointerArrayArgsInfo.valueKind = Kind::Int32;
  locals.emplace("pointerArrayArgs", pointerArrayArgsInfo);

  LocalInfo borrowedVectorArgsInfo{};
  borrowedVectorArgsInfo.kind = LocalInfo::Kind::Array;
  borrowedVectorArgsInfo.isArgsPack = true;
  borrowedVectorArgsInfo.argsPackElementKind = LocalInfo::Kind::Reference;
  borrowedVectorArgsInfo.referenceToVector = true;
  borrowedVectorArgsInfo.valueKind = Kind::Int32;
  locals.emplace("borrowedVectorArgs", borrowedVectorArgsInfo);

  LocalInfo pointerVectorArgsInfo{};
  pointerVectorArgsInfo.kind = LocalInfo::Kind::Array;
  pointerVectorArgsInfo.isArgsPack = true;
  pointerVectorArgsInfo.argsPackElementKind = LocalInfo::Kind::Pointer;
  pointerVectorArgsInfo.pointerToVector = true;
  pointerVectorArgsInfo.valueKind = Kind::Int32;
  locals.emplace("pointerVectorArgs", pointerVectorArgsInfo);

  LocalInfo borrowedBufferArgsInfo{};
  borrowedBufferArgsInfo.kind = LocalInfo::Kind::Array;
  borrowedBufferArgsInfo.isArgsPack = true;
  borrowedBufferArgsInfo.argsPackElementKind = LocalInfo::Kind::Reference;
  borrowedBufferArgsInfo.referenceToBuffer = true;
  borrowedBufferArgsInfo.valueKind = Kind::Int32;
  locals.emplace("borrowedBufferArgs", borrowedBufferArgsInfo);

  LocalInfo pointerBufferArgsInfo{};
  pointerBufferArgsInfo.kind = LocalInfo::Kind::Array;
  pointerBufferArgsInfo.isArgsPack = true;
  pointerBufferArgsInfo.argsPackElementKind = LocalInfo::Kind::Pointer;
  pointerBufferArgsInfo.pointerToBuffer = true;
  pointerBufferArgsInfo.valueKind = Kind::Int32;
  locals.emplace("pointerBufferArgs", pointerBufferArgsInfo);

  LocalInfo scalarRefArgsInfo{};
  scalarRefArgsInfo.kind = LocalInfo::Kind::Array;
  scalarRefArgsInfo.isArgsPack = true;
  scalarRefArgsInfo.argsPackElementKind = LocalInfo::Kind::Reference;
  scalarRefArgsInfo.valueKind = Kind::Int32;
  locals.emplace("scalarRefArgs", scalarRefArgsInfo);

  primec::Expr arrName;
  arrName.kind = primec::Expr::Kind::Name;
  arrName.name = "arr";
  auto resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(arrName, locals);
  std::string error;
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr vecName;
  vecName.kind = primec::Expr::Kind::Name;
  vecName.name = "vec";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(vecName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Float64);
  CHECK(resolved.isVectorTarget);

  primec::Expr experimentalVecName;
  experimentalVecName.kind = primec::Expr::Kind::Name;
  experimentalVecName.name = "experimentalVec";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      experimentalVecName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK(resolved.isVectorTarget);
  CHECK(resolved.structTypeName ==
        "/std/collections/experimental_vector/Vector__t25a78a513414c3bf");

  primec::Expr refArrName;
  refArrName.kind = primec::Expr::Kind::Name;
  refArrName.name = "refArr";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(refArrName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Bool);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "buffer";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(bufferName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr refBufferName;
  refBufferName.kind = primec::Expr::Kind::Name;
  refBufferName.name = "refBuffer";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(refBufferName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr soaValuesName;
  soaValuesName.kind = primec::Expr::Kind::Name;
  soaValuesName.name = "soaValues";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(soaValuesName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Unknown);
  CHECK_FALSE(resolved.isVectorTarget);
  CHECK(resolved.isSoaVector);
  CHECK(resolved.structTypeName.rfind(soaVectorStructTypePrefix, 0) == 0);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  primec::Expr vectorCtor;
  vectorCtor.kind = primec::Expr::Kind::Call;
  vectorCtor.name = "vector";
  vectorCtor.templateArgs = {"u64"};
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(vectorCtor, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::UInt64);
  CHECK(resolved.isVectorTarget);

  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  primec::Expr soaVectorCtor;
  soaVectorCtor.kind = primec::Expr::Kind::Call;
  soaVectorCtor.name = "soa_vector";
  soaVectorCtor.templateArgs = {"Particle"};
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(soaVectorCtor, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Unknown);
  CHECK_FALSE(resolved.isVectorTarget);
  CHECK(resolved.isSoaVector);
  CHECK(resolved.structTypeName.rfind(soaVectorStructTypePrefix, 0) == 0);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  primec::Expr structArgsName;
  structArgsName.kind = primec::Expr::Kind::Name;
  structArgsName.name = "structArgs";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(structArgsName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Unknown);
  CHECK_FALSE(resolved.isVectorTarget);
  CHECK(resolved.isArgsPackTarget);
  CHECK(resolved.argsPackElementKind == LocalInfo::Kind::Value);
  CHECK(resolved.structTypeName == "/pkg/Pair");
  CHECK(resolved.elemSlotCount == 0);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  primec::Expr vectorArgsName;
  vectorArgsName.kind = primec::Expr::Kind::Name;
  vectorArgsName.name = "vectorArgs";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(vectorArgsName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Unknown);
  CHECK_FALSE(resolved.isVectorTarget);
  CHECK(resolved.isArgsPackTarget);
  CHECK(resolved.argsPackElementKind == LocalInfo::Kind::Value);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  auto makeDereferencedArgsPackAccess = [](const char *name) {
    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = name;

    primec::Expr indexExpr;
    indexExpr.kind = primec::Expr::Kind::Literal;
    indexExpr.literalValue = 0;

    primec::Expr accessExpr;
    accessExpr.kind = primec::Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.args = {receiverName, indexExpr};

    primec::Expr derefExpr;
    derefExpr.kind = primec::Expr::Kind::Call;
    derefExpr.name = "dereference";
    derefExpr.args = {accessExpr};
    return derefExpr;
  };

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("borrowedArrayArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("pointerArrayArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("borrowedVectorArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK(resolved.isVectorTarget);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("pointerVectorArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK(resolved.isVectorTarget);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("borrowedBufferArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("pointerBufferArgs"), locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      makeDereferencedArgsPackAccess("scalarRefArgs"), locals);
  CHECK_FALSE(resolved.isArrayOrVectorTarget);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error ==
        "native backend only supports at() on numeric/bool/string arrays or vectors, plus args<Struct>/args<map<K, V>>/args<Pointer<Struct>>/args<Reference<Struct>>/args<Pointer<map<K, V>>>/args<Reference<map<K, V>>>/args<vector<T>>/args<Pointer<vector<T>>>/args<Reference<vector<T>>>/args<Pointer<soa_vector<T>>>/args<Reference<soa_vector<T>>> packs");

  primec::Expr plain;
  plain.kind = primec::Expr::Kind::Name;
  plain.name = "other";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(plain, locals);
  CHECK_FALSE(resolved.isArrayOrVectorTarget);
  primec::Expr helperVectorName;
  helperVectorName.kind = primec::Expr::Kind::Name;
  helperVectorName.name = "/wrapVector__ti32";
  bool helperVectorCallbackInvoked = false;
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      helperVectorName,
      locals,
      [&](const primec::Expr &targetExpr,
          primec::ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
        helperVectorCallbackInvoked = true;
        if (targetExpr.kind != primec::Expr::Kind::Name ||
            targetExpr.name != "/wrapVector__ti32") {
          return false;
        }
        targetInfoOut.isArrayOrVectorTarget = true;
        targetInfoOut.isVectorTarget = true;
        targetInfoOut.elemKind = Kind::Int32;
        return true;
      });
  CHECK(helperVectorCallbackInvoked);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.isVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(plain, locals);
  CHECK_FALSE(resolved.isArrayOrVectorTarget);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error ==
        "native backend only supports at() on numeric/bool/string arrays or vectors, plus args<Struct>/args<map<K, V>>/args<Pointer<Struct>>/args<Reference<Struct>>/args<Pointer<map<K, V>>>/args<Reference<map<K, V>>>/args<vector<T>>/args<Pointer<vector<T>>>/args<Reference<vector<T>>>/args<Pointer<soa_vector<T>>>/args<Reference<soa_vector<T>>> packs");

  primec::ir_lowerer::ArrayVectorAccessTargetInfo stringElem;
  stringElem.isArrayOrVectorTarget = true;
  stringElem.elemKind = Kind::String;
  error.clear();
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(stringElem, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer temporary vector receiver reject guards stdlib wrapper constructors") {
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
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  CHECK(source.find("resolvePublishedLateCollectionMemberName(") !=
        std::string::npos);
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsVectorConstructors") !=
        std::string::npos);
  CHECK(source.find("ir_lowerer::resolvePublishedStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(source.find("constructorName") != std::string::npos);
  CHECK(source.find("matchesVectorConstructorPath(\"/std/collections/vectorPair\")") ==
        std::string::npos);
  CHECK(source.find("auto rejectCanonicalVectorTemporaryReceiverExpr = [&](const Expr &callExpr) -> bool {") !=
        std::string::npos);
  CHECK(source.find("if (rejectCanonicalVectorTemporaryReceiverExpr(expr)) {") !=
        std::string::npos);
  CHECK(source.find("auto resolveCollectionExprDirectDefinition =") !=
        std::string::npos);
  CHECK(source.find("auto findDirectHelperDefinition = [&](const std::string &path)") !=
        std::string::npos);
  CHECK(source.find("matchesGeneratedLeafDefinition(candidatePath, \"__t\", 3)") !=
        std::string::npos);
  CHECK(source.find("matchesGeneratedLeafDefinition(candidatePath, \"__ov\", 4)") !=
        std::string::npos);
  CHECK(source.find("const Definition *receiverDef =") !=
        std::string::npos);
  CHECK(source.find("resolveCollectionExprDirectDefinition(*receiverExpr)") !=
        std::string::npos);
  CHECK(source.find("auto tryPopulateFromSemanticQueryFact = [&]() {") ==
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFactBySemanticId(*semanticIndex, targetCallExpr)") ==
        std::string::npos);
  CHECK(source.find("bindingType.rfind(\"/std/collections/experimental_vector/Vector__\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front())") !=
        std::string::npos);
}

TEST_SUITE_END();
