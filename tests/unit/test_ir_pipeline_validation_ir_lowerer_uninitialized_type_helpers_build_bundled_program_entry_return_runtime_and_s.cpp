#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled program entry return runtime and setup resolution") {
  primec::Program program;
  program.imports.push_back("/std/math/*");

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
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setupWithMath;
  REQUIRE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      "/main",
      defMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupWithMath,
      error));
  CHECK(error.empty());
  CHECK(setupWithMath.entryReturnConfig.returnsVoid);

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setupWithMath.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Program noMathProgram = program;
  noMathProgram.imports.clear();
  const std::unordered_map<std::string, const primec::Definition *> noMathDefMap = {
      {"/handler", &noMathProgram.definitions[0]},
      {"/callee", &noMathProgram.definitions[1]},
      {"/pkg/Container", &noMathProgram.definitions[2]},
      {"/pkg/MyStruct", &noMathProgram.definitions[3]},
      {"/main", &noMathProgram.definitions[4]},
  };
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setupNoMath;
  REQUIRE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      noMathProgram,
      noMathProgram.definitions[4],
      "/main",
      noMathDefMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupNoMath,
      error));
  CHECK_FALSE(setupNoMath.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
                  .entrySetupMathTypeStructAndUninitializedResolutionSetup
                  .setupMathTypeStructAndUninitializedResolutionSetup
                  .setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));

  primec::Program badProgram = program;
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
  CHECK_FALSE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badProgram,
      badProgram.definitions[4],
      "/main",
      badDefMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupWithMath,
      error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_CASE("ir lowerer setup locals helper unpacks orchestration adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;
  program.imports.push_back("/std/math/*");

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
  REQUIRE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      "/main",
      defMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());

  primec::ir_lowerer::SetupLocalsOrchestration unpacked;
  primec::ir_lowerer::populateSetupLocalsOrchestration(setup, unpacked);
  CHECK(unpacked.entryReturnConfig.returnsVoid);
  CHECK(unpacked.entryCountAccessSetup.hasEntryArgs);
  CHECK(unpacked.entryCountAccessSetup.entryArgsName == "argv");
  CHECK(unpacked.entryCallOnErrorSetup.onErrorByDefinition.count("/main") == 1u);
  CHECK_FALSE(unpacked.entryCallOnErrorSetup.hasTailExecution);

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(unpacked.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  CHECK(unpacked.setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);
  primec::ir_lowerer::StructSlotLayoutInfo slotLayout;
  CHECK(unpacked.structSlotResolutionAdapters.resolveStructSlotLayout("/pkg/MyStruct", slotLayout));
  CHECK(slotLayout.fields.size() == 1u);

  primec::ir_lowerer::StructArrayTypeInfo arrayInfo;
  CHECK(unpacked.structArrayInfoAdapters.resolveStructArrayTypeInfoFromPath("/pkg/MyStruct", arrayInfo));
  CHECK(arrayInfo.elementKind == ValueKind::Int32);

  const size_t instructionBefore = function.instructions.size();
  unpacked.runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  CHECK(function.instructions.size() > instructionBefore);
}

TEST_CASE("ir lowerer on_error helpers reject unknown handler") {
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "missing"};
  const std::vector<primec::Transform> transforms = {onError};

  std::optional<primec::ir_lowerer::OnErrorHandler> out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::parseOnErrorTransform(
      transforms,
      "",
      "/main",
      [](const primec::Expr &expr) { return std::string("/") + expr.name; },
      [](const std::string &) { return false; },
      out,
      error));
  CHECK(error == "unknown on_error handler: /missing");
}

TEST_CASE("ir lowerer setup type helper maps primitive aliases") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("int") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("i32") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("float") == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("f64") == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("bool") == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("string") == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup type helper maps file and packed error types") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("FileError") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("/std/file/FileError") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("ImageError") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("/std/image/ImageError") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("ContainerError") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("/std/collections/ContainerError") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("GfxError") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("/std/gfx/GfxError") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Read>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("/std/file/File<Read>") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Write>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper returns unknown for unsupported names") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Vec3") == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Result<FileError>") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper maps value kinds to type names") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Int32) == "i32");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Int64) == "i64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::UInt64) == "u64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Float32) == "f32");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Float64) == "f64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Bool) == "bool");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::String) == "string");
}

TEST_CASE("ir lowerer setup type helper returns empty name for unknown kind") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Unknown).empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver local targets") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  std::string typeName;
  std::string resolvedTypePath;

  LocalInfo arrayLocal;
  arrayLocal.kind = LocalInfo::Kind::Array;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(arrayLocal, typeName, resolvedTypePath));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());

  LocalInfo structArrayLocal;
  structArrayLocal.kind = LocalInfo::Kind::Array;
  structArrayLocal.structTypeName = "/pkg/Vec3";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(structArrayLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath == "/pkg/Vec3");

  LocalInfo vectorLocal;
  vectorLocal.kind = LocalInfo::Kind::Vector;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(vectorLocal, typeName, resolvedTypePath));
  CHECK(typeName == "vector");
  CHECK(resolvedTypePath.empty());

  LocalInfo mapLocal;
  mapLocal.kind = LocalInfo::Kind::Map;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(mapLocal, typeName, resolvedTypePath));
  CHECK(typeName == "map");
  CHECK(resolvedTypePath.empty());

  LocalInfo bufferLocal;
  bufferLocal.kind = LocalInfo::Kind::Buffer;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(bufferLocal, typeName, resolvedTypePath));
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());

  LocalInfo soaVectorLocal;
  soaVectorLocal.kind = LocalInfo::Kind::Value;
  soaVectorLocal.valueKind = LocalInfo::ValueKind::Unknown;
  soaVectorLocal.isSoaVector = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(soaVectorLocal, typeName, resolvedTypePath));
  CHECK(typeName == "soa_vector");
  CHECK(resolvedTypePath.empty());

  LocalInfo referenceArrayLocal;
  referenceArrayLocal.kind = LocalInfo::Kind::Reference;
  referenceArrayLocal.referenceToArray = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(referenceArrayLocal, typeName, resolvedTypePath));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());

  LocalInfo referenceBufferLocal;
  referenceBufferLocal.kind = LocalInfo::Kind::Reference;
  referenceBufferLocal.referenceToBuffer = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(referenceBufferLocal, typeName, resolvedTypePath));
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());

  LocalInfo pointerBufferLocal;
  pointerBufferLocal.kind = LocalInfo::Kind::Pointer;
  pointerBufferLocal.pointerToBuffer = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(pointerBufferLocal, typeName, resolvedTypePath));
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());

  LocalInfo valueLocal;
  valueLocal.kind = LocalInfo::Kind::Value;
  valueLocal.valueKind = LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(valueLocal, typeName, resolvedTypePath));
  CHECK(typeName == "i64");
  CHECK(resolvedTypePath.empty());

  LocalInfo fileLocal;
  fileLocal.kind = LocalInfo::Kind::Value;
  fileLocal.valueKind = LocalInfo::ValueKind::Int64;
  fileLocal.isFileHandle = true;
  fileLocal.structTypeName = "/std/file/File<Read>";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(fileLocal, typeName, resolvedTypePath));
  CHECK(typeName == "File");
  CHECK(resolvedTypePath.empty());

  LocalInfo unknownValueLocal;
  unknownValueLocal.kind = LocalInfo::Kind::Value;
  unknownValueLocal.valueKind = LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      unknownValueLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());
}

TEST_CASE("ir lowerer setup type helper rejects pointer and non-array reference receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  std::string typeName = "stale";
  std::string resolvedTypePath = "stale";

  LocalInfo pointerLocal;
  pointerLocal.kind = LocalInfo::Kind::Pointer;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      pointerLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());

  typeName = "stale";
  resolvedTypePath = "stale";
  LocalInfo referenceLocal;
  referenceLocal.kind = LocalInfo::Kind::Reference;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      referenceLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver call targets") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr arrayCall;
  arrayCall.kind = primec::Expr::Kind::Call;
  arrayCall.name = "array";
  arrayCall.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(arrayCall, ValueKind::Unknown) == "array");

  primec::Expr vectorCall;
  vectorCall.kind = primec::Expr::Kind::Call;
  vectorCall.name = "vector";
  vectorCall.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(vectorCall, ValueKind::Unknown) ==
        "vector");

  primec::Expr mapCall;
  mapCall.kind = primec::Expr::Kind::Call;
  mapCall.name = "map";
  mapCall.templateArgs = {"i32", "i64"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(mapCall, ValueKind::Unknown) == "map");

  primec::Expr bufferCall;
  bufferCall.kind = primec::Expr::Kind::Call;
  bufferCall.name = "Buffer";
  bufferCall.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(bufferCall, ValueKind::Unknown) == "Buffer");

  primec::Expr scopedBufferCall = bufferCall;
  scopedBufferCall.namespacePrefix = "/std/gfx";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(
            scopedBufferCall, ValueKind::Unknown) == "Buffer");

  primec::Expr soaVectorCall;
  soaVectorCall.kind = primec::Expr::Kind::Call;
  soaVectorCall.name = "soa_vector";
  soaVectorCall.templateArgs = {"Particle"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(soaVectorCall, ValueKind::Unknown) ==
        "soa_vector");
}

TEST_CASE("ir lowerer setup type helper falls back for method receiver call targets") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr invalidArrayCall;
  invalidArrayCall.kind = primec::Expr::Kind::Call;
  invalidArrayCall.name = "array";
  invalidArrayCall.templateArgs = {};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(invalidArrayCall, ValueKind::Int64) ==
        "i64");

  primec::Expr userCall;
  userCall.kind = primec::Expr::Kind::Call;
  userCall.name = "/pkg/make";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(userCall, ValueKind::Float32) == "f32");
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(userCall, ValueKind::Unknown).empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver struct paths from call expressions") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  const std::unordered_set<std::string> structNames = {
      "/pkg/Ctor",
      "/imports/Ctor",
      "/std/collections/map/at",
      "/map/at",
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"Ctor", "/imports/Ctor"},
      {"MapCanonicalAlias", "std/collections/map/at"},
      {"MapCompatAlias", "map/at"},
  };

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames) == "/pkg/Ctor");

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/Ctor", importAliases, structNames) == "/imports/Ctor");

  receiverCall.name = "MapCanonicalAlias";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCanonicalAlias", importAliases, structNames) == "/std/collections/map/at");

  receiverCall.name = "MapCompatAlias";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCompatAlias", importAliases, structNames) == "/map/at");

  receiverCall.name = "Ctor";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "std/collections/map/at", importAliases, structNames) == "/std/collections/map/at");
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "map/at", importAliases, structNames) == "/map/at");

  const std::unordered_set<std::string> canonicalOnlyStructNames = {
      "/std/collections/map/at",
  };
  receiverCall.name = "MapCompatAlias";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCompatAlias", importAliases, canonicalOnlyStructNames)
        .empty());

  const std::unordered_set<std::string> compatOnlyStructNames = {
      "/map/at",
  };
  receiverCall.name = "MapCanonicalAlias";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCanonicalAlias", importAliases, compatOnlyStructNames)
        .empty());
}

TEST_CASE("ir lowerer setup type helper keeps direct map access unsafe receiver paths separated") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "MapCompatAlias";

  const std::unordered_map<std::string, std::string> importAliases = {
      {"MapCanonicalAlias", "std/collections/map/at_unsafe"},
      {"MapCompatAlias", "map/at_unsafe"},
  };

  const std::unordered_set<std::string> canonicalOnlyStructNames = {
      "/std/collections/map/at_unsafe",
  };
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCompatAlias", importAliases, canonicalOnlyStructNames)
        .empty());

  const std::unordered_set<std::string> compatOnlyStructNames = {
      "/map/at_unsafe",
  };
  receiverCall.name = "MapCanonicalAlias";
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/MapCanonicalAlias", importAliases, compatOnlyStructNames)
        .empty());
}

TEST_CASE("ir lowerer setup type helper rejects non-struct method receiver call paths") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  const std::unordered_set<std::string> structNames = {"/pkg/Ctor"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Ctor", "/imports/Ctor"}};

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/Ctor", importAliases, structNames)
        .empty());

  receiverCall.isBinding = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames)
        .empty());

  receiverCall.isBinding = false;
  receiverCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames)
        .empty());
}

TEST_SUITE_END();
