#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer uninitialized type helpers build storage resolver from definition field index") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveUninitializedStorage =
      primec::ir_lowerer::makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
          fieldIndex, defMap, resolveTypeInfo, resolveSlot, error);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(resolveUninitializedStorage(fieldExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled uninitialized resolution adapters") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };
  auto adapters = primec::ir_lowerer::makeUninitializedResolutionAdapters(
      resolveStruct, resolveExprPath, fieldIndex, defMap, resolveSlot, error);

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(adapters.resolveUninitializedTypeInfo("MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  CHECK_FALSE(adapters.resolveUninitializedTypeInfo("Thing<i32>", "/pkg", typeInfo));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");

  error.clear();
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(adapters.resolveUninitializedStorage(fieldExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");

  primec::Expr selfExpr;
  selfExpr.kind = primec::Expr::Kind::Name;
  selfExpr.name = "self";
  CHECK(adapters.inferStructExprPath(selfExpr, locals) == "/pkg/Container");

  primec::Expr missingExpr;
  missingExpr.kind = primec::Expr::Kind::Name;
  missingExpr.name = "missing";
  CHECK(adapters.inferStructExprPath(missingExpr, locals).empty());

  primec::ir_lowerer::LocalInfo filePack;
  filePack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  filePack.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  filePack.isArgsPack = true;
  filePack.isFileHandle = true;
  filePack.structTypeName = "/std/file/File<Read>";
  filePack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  locals.emplace("files", filePack);

  primec::Expr filesExpr;
  filesExpr.kind = primec::Expr::Kind::Name;
  filesExpr.name = "files";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {filesExpr, indexExpr};
  CHECK(adapters.inferStructExprPath(accessExpr, locals).empty());
}

TEST_CASE("ir lowerer binding transform helpers classify qualifiers and mutability") {
  CHECK(primec::ir_lowerer::isBindingQualifierName("public"));
  CHECK(primec::ir_lowerer::isBindingQualifierName("mut"));
  CHECK_FALSE(primec::ir_lowerer::isBindingQualifierName("array"));

  primec::Expr bindingExpr;
  primec::Transform mutTransform;
  mutTransform.name = "mut";
  bindingExpr.transforms.push_back(mutTransform);
  CHECK(primec::ir_lowerer::isBindingMutable(bindingExpr));

  bindingExpr.transforms.clear();
  CHECK_FALSE(primec::ir_lowerer::isBindingMutable(bindingExpr));
}

TEST_CASE("ir lowerer binding transform helpers detect explicit binding types") {
  primec::Expr expr;

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};
  expr.transforms.push_back(effects);

  primec::Transform qualifier;
  qualifier.name = "public";
  expr.transforms.push_back(qualifier);

  CHECK_FALSE(primec::ir_lowerer::hasExplicitBindingTypeTransform(expr));

  primec::Transform typed;
  typed.name = "array";
  typed.templateArgs = {"i64"};
  expr.transforms.push_back(typed);
  CHECK(primec::ir_lowerer::hasExplicitBindingTypeTransform(expr));
}

TEST_CASE("ir lowerer binding transform helpers extract first binding type transform") {
  primec::Expr expr;

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};
  expr.transforms.push_back(effects);

  primec::Transform qualifier;
  qualifier.name = "mut";
  expr.transforms.push_back(qualifier);

  primec::Transform withArgs;
  withArgs.name = "align_bytes";
  withArgs.arguments = {"16"};
  expr.transforms.push_back(withArgs);

  primec::Transform typed;
  typed.name = "Result";
  typed.templateArgs = {"i32", "FileError"};
  expr.transforms.push_back(typed);

  std::string typeName;
  std::vector<std::string> templateArgs;
  REQUIRE(primec::ir_lowerer::extractFirstBindingTypeTransform(expr, typeName, templateArgs));
  CHECK(typeName == "Result");
  REQUIRE(templateArgs.size() == 2);
  CHECK(templateArgs[0] == "i32");
  CHECK(templateArgs[1] == "FileError");

  expr.transforms.clear();
  expr.transforms.push_back(effects);
  expr.transforms.push_back(qualifier);
  CHECK_FALSE(primec::ir_lowerer::extractFirstBindingTypeTransform(expr, typeName, templateArgs));
  CHECK(typeName.empty());
  CHECK(templateArgs.empty());
}

TEST_CASE("ir lowerer binding transform helpers extract uninitialized template args") {
  primec::Expr expr;
  primec::Transform uninitialized;
  uninitialized.name = "uninitialized";
  uninitialized.templateArgs = {"i64"};
  expr.transforms.push_back(uninitialized);

  std::string typeText;
  CHECK(primec::ir_lowerer::extractUninitializedTemplateArg(expr, typeText));
  CHECK(typeText == "i64");

  expr.transforms.clear();
  uninitialized.templateArgs = {"i64", "i32"};
  expr.transforms.push_back(uninitialized);
  CHECK_FALSE(primec::ir_lowerer::extractUninitializedTemplateArg(expr, typeText));
}

TEST_CASE("ir lowerer binding transform helpers detect entry args params") {
  primec::Expr param;
  primec::Transform mutTransform;
  mutTransform.name = "mut";
  param.transforms.push_back(mutTransform);

  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  param.transforms.push_back(arrayTransform);
  CHECK(primec::ir_lowerer::isEntryArgsParam(param));

  arrayTransform.templateArgs = {"i64"};
  param.transforms.back() = arrayTransform;
  CHECK_FALSE(primec::ir_lowerer::isEntryArgsParam(param));
}

TEST_CASE("ir lowerer binding type helpers classify binding kind and string/fileerror types") {
  primec::Expr vectorExpr;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  vectorExpr.transforms.push_back(vectorTransform);
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(vectorExpr) == primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr soaVectorExpr;
  primec::Transform soaVectorTransform;
  soaVectorTransform.name = "SoaVector";
  soaVectorTransform.templateArgs = {"Particle"};
  soaVectorExpr.transforms.push_back(soaVectorTransform);
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(soaVectorExpr) ==
        primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr canonicalMapExpr;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "i64"};
  canonicalMapExpr.transforms.push_back(canonicalMapTransform);
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(canonicalMapExpr) == primec::ir_lowerer::LocalInfo::Kind::Map);

  primec::Expr defaultExpr;
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(defaultExpr) == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "std/collections/experimental_soa_vector/SoaVector__Particle") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "/std/collections/experimental_soa_vector/SoaVector__Particle") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("SoaVector__Particle") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "std/collections/experimental_soa_vector/SoaVector") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "/std/collections/experimental_soa_vector/SoaVector") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("SoaVector") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("/SoaVector") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "std/collections/experimental_soa_vector/SoaVector<Particle>") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName(
            "/std/collections/experimental_soa_vector/SoaVector<Particle>") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("SoaVector<Particle>") ==
        "soa_vector");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("std/file/File") ==
        "File");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("/std/file/File") ==
        "File");
  CHECK(primec::ir_lowerer::normalizeCollectionBindingTypeName("/File") ==
        "File");

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
  const std::filesystem::path bindingTypeHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp";
  REQUIRE(std::filesystem::exists(bindingTypeHelpersPath));
  const std::string bindingTypeHelpersSource = readText(bindingTypeHelpersPath);
  CHECK(bindingTypeHelpersSource.find(
            "semantics::isExperimentalSoaVectorTypePath(name)") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find(
            "name == \"std/collections/experimental_soa_vector/SoaVector\"") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find(
            "name == \"/std/collections/experimental_soa_vector/SoaVector\"") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("name == \"SoaVector\"") != std::string::npos);
  CHECK(bindingTypeHelpersSource.find("name == \"/SoaVector\"") != std::string::npos);

  primec::Expr stringExpr;
  primec::Transform qualifier;
  qualifier.name = "mut";
  stringExpr.transforms.push_back(qualifier);
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(primec::ir_lowerer::isStringBindingType(stringExpr));

  primec::Expr fileErrorExpr;
  fileErrorExpr.transforms.push_back(qualifier);
  primec::Transform fileErrorTransform;
  fileErrorTransform.name = "FileError";
  fileErrorExpr.transforms.push_back(fileErrorTransform);
  CHECK(primec::ir_lowerer::isFileErrorBindingType(fileErrorExpr));

  primec::Expr referenceFileErrorExpr;
  primec::Transform referenceFileErrorTransform;
  referenceFileErrorTransform.name = "Reference";
  referenceFileErrorTransform.templateArgs = {"FileError"};
  referenceFileErrorExpr.transforms.push_back(referenceFileErrorTransform);
  CHECK(primec::ir_lowerer::isFileErrorBindingType(referenceFileErrorExpr));

  primec::Expr pointerFileErrorExpr;
  primec::Transform pointerFileErrorTransform;
  pointerFileErrorTransform.name = "Pointer";
  pointerFileErrorTransform.templateArgs = {"FileError"};
  pointerFileErrorExpr.transforms.push_back(pointerFileErrorTransform);
  CHECK(primec::ir_lowerer::isFileErrorBindingType(pointerFileErrorExpr));

  CHECK_FALSE(primec::ir_lowerer::isFileErrorBindingType(stringExpr));
  CHECK(primec::ir_lowerer::isStringTypeName("string"));
  CHECK_FALSE(primec::ir_lowerer::isStringTypeName("i64"));
}

TEST_CASE("ir lowerer binding type helpers resolve value kinds from transforms") {
  primec::Expr pointerExpr;
  primec::Transform pointerTransform;
  pointerTransform.name = "Pointer";
  pointerTransform.templateArgs = {"i64"};
  pointerExpr.transforms.push_back(pointerTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(pointerExpr, primec::ir_lowerer::LocalInfo::Kind::Pointer) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr canonicalMapExpr;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "f64"};
  canonicalMapExpr.transforms.push_back(canonicalMapTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(canonicalMapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr soaVectorValueExpr;
  primec::Transform soaVectorValueTransform;
  soaVectorValueTransform.name = "SoaVector";
  soaVectorValueTransform.templateArgs = {"Particle"};
  soaVectorValueExpr.transforms.push_back(soaVectorValueTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(
            soaVectorValueExpr, primec::ir_lowerer::LocalInfo::Kind::Vector) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr resultExpr;
  primec::Transform resultTransform;
  resultTransform.name = "Result";
  resultTransform.templateArgs = {"i64", "FileError"};
  resultExpr.transforms.push_back(resultTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(resultExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr namespacedFileExpr;
  primec::Transform namespacedFileTransform;
  namespacedFileTransform.name = "/std/file/File";
  namespacedFileTransform.templateArgs = {"Read"};
  namespacedFileExpr.transforms.push_back(namespacedFileTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(
            namespacedFileExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(primec::ir_lowerer::bindingValueKindFromTypeText(
            "/std/file/File<Read>", primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr structExpr;
  primec::Transform structTransform;
  structTransform.name = "Holder";
  structExpr.transforms.push_back(structTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(structExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(primec::ir_lowerer::bindingValueKindFromTypeText("Holder", primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr defaultExpr;
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(defaultExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(defaultExpr, primec::ir_lowerer::LocalInfo::Kind::Array) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer binding type helpers mark reference-to-array metadata") {
  primec::Expr referenceArrayExpr;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  referenceArrayExpr.transforms.push_back(referenceArrayTransform);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceArrayExpr, info);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr referenceScalarExpr;
  primec::Transform referenceScalarTransform;
  referenceScalarTransform.name = "Reference";
  referenceScalarTransform.templateArgs = {"i64"};
  referenceScalarExpr.transforms.push_back(referenceScalarTransform);

  primec::ir_lowerer::LocalInfo scalarInfo;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  scalarInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceScalarExpr, scalarInfo);
  CHECK_FALSE(scalarInfo.referenceToArray);
  CHECK(scalarInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::ir_lowerer::LocalInfo presetInfo;
  presetInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  presetInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceArrayExpr, presetInfo);
  CHECK(presetInfo.referenceToArray);
  CHECK(presetInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer binding type helpers build bundled setup adapters") {
  auto adapters = primec::ir_lowerer::makeBindingTypeAdapters();

  primec::Expr vectorExpr;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  vectorExpr.transforms.push_back(vectorTransform);
  CHECK(adapters.bindingKind(vectorExpr) == primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr stringExpr;
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(adapters.hasExplicitBindingTypeTransform(stringExpr));
  CHECK(adapters.isStringBinding(stringExpr));
  CHECK_FALSE(adapters.isFileErrorBinding(stringExpr));

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(adapters.bindingValueKind(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::Expr referenceArrayExpr;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  referenceArrayExpr.transforms.push_back(referenceArrayTransform);
  adapters.setReferenceArrayInfo(referenceArrayExpr, info);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer binding type helpers prefer semantic product temporary facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "temporary",
      .name = "makeVec",
      .bindingTypeText = "vector<i64>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 12,
      .sourceColumn = 7,
      .semanticNodeId = 17,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/makeVec"),
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "temporary",
      .name = "loadArrayRef",
      .bindingTypeText = "Reference<array<i64>>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 13,
      .sourceColumn = 9,
      .semanticNodeId = 23,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/loadArrayRef"),
  });

  auto adapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);

  primec::Expr tempVectorCall;
  tempVectorCall.kind = primec::Expr::Kind::Call;
  tempVectorCall.name = "makeVec";
  tempVectorCall.semanticNodeId = 17;
  tempVectorCall.sourceLine = 12;
  tempVectorCall.sourceColumn = 7;
  CHECK(adapters.hasExplicitBindingTypeTransform(tempVectorCall));
  CHECK(adapters.bindingKind(tempVectorCall) == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(adapters.bindingValueKind(tempVectorCall, primec::ir_lowerer::LocalInfo::Kind::Vector) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr tempRefCall;
  tempRefCall.kind = primec::Expr::Kind::Call;
  tempRefCall.name = "loadArrayRef";
  tempRefCall.semanticNodeId = 23;
  tempRefCall.sourceLine = 13;
  tempRefCall.sourceColumn = 9;
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  adapters.setReferenceArrayInfo(tempRefCall, info);
  CHECK(adapters.bindingKind(tempRefCall) == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer binding type helpers stop using transform fallback for semantic expr ids") {
  primec::SemanticProgram semanticProgram;
  auto adapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);

  primec::Expr tempVectorCall;
  tempVectorCall.kind = primec::Expr::Kind::Call;
  tempVectorCall.name = "makeVec";
  tempVectorCall.semanticNodeId = 91;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  tempVectorCall.transforms.push_back(vectorTransform);
  CHECK_FALSE(adapters.hasExplicitBindingTypeTransform(tempVectorCall));
  CHECK(adapters.bindingKind(tempVectorCall) == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(adapters.bindingValueKind(tempVectorCall, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr tempRefCall;
  tempRefCall.kind = primec::Expr::Kind::Call;
  tempRefCall.name = "loadArrayRef";
  tempRefCall.semanticNodeId = 92;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  tempRefCall.transforms.push_back(referenceArrayTransform);
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  adapters.setReferenceArrayInfo(tempRefCall, info);
  CHECK_FALSE(adapters.hasExplicitBindingTypeTransform(tempRefCall));
  CHECK_FALSE(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer binding type helpers treat semantic local-auto facts as non-explicit bindings") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 11,
      .sourceColumn = 5,
      .semanticNodeId = 114,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "i32",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "return",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 11,
      .sourceColumn = 5,
      .semanticNodeId = 114,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  auto adapters = primec::ir_lowerer::makeBindingTypeAdapters(&semanticProgram);

  primec::Expr localAutoBinding;
  localAutoBinding.kind = primec::Expr::Kind::Call;
  localAutoBinding.name = "selected";
  localAutoBinding.isBinding = true;
  localAutoBinding.semanticNodeId = 114;
  primec::Transform explicitTransform;
  explicitTransform.name = "i32";
  localAutoBinding.transforms.push_back(explicitTransform);
  CHECK_FALSE(adapters.hasExplicitBindingTypeTransform(localAutoBinding));
}

TEST_CASE("ir lowerer count access helpers classify entry args and count calls") {
  primec::Definition entryDef;
  bool hasEntryArgs = true;
  std::string entryArgsName = "stale";
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK_FALSE(hasEntryArgs);
  CHECK(entryArgsName.empty());
  CHECK(error.empty());

  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters = {entryParam};
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(hasEntryArgs);
  CHECK(entryArgsName == "argv");
  CHECK(error.empty());

  primec::Expr extraParam = entryParam;
  extraParam.name = "extra";
  entryDef.parameters = {entryParam, extraParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");

  error.clear();
  primec::Expr badTypeParam;
  badTypeParam.name = "argv";
  primec::Transform badTypeTransform;
  badTypeTransform.name = "array";
  badTypeTransform.templateArgs = {"i64"};
  badTypeParam.transforms.push_back(badTypeTransform);
  entryDef.parameters = {badTypeParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend entry parameter must be array<string>");

  error.clear();
  entryParam.args.push_back(primec::Expr{});
  entryDef.parameters = {entryParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend does not allow entry parameter defaults");

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "semanticArgv",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 3,
      .sourceColumn = 1,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/semanticArgv"),
  });
  entryDef.fullPath = "/main";
  entryDef.parameters.clear();
  error.clear();
  hasEntryArgs = false;
  entryArgsName.clear();
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(
      entryDef, &semanticProgram, hasEntryArgs, entryArgsName, error));
  CHECK(hasEntryArgs);
  CHECK(entryArgsName == "semanticArgv");
  CHECK(error.empty());

  primec::Expr staleParam;
  staleParam.name = "stale";
  primec::Transform staleTransform;
  staleTransform.name = "array";
  staleTransform.templateArgs = {"i64"};
  staleParam.transforms.push_back(staleTransform);
  entryDef.parameters = {staleParam};
  error.clear();
  hasEntryArgs = false;
  entryArgsName.clear();
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(
      entryDef, &semanticProgram, hasEntryArgs, entryArgsName, error));
  CHECK(hasEntryArgs);
  CHECK(entryArgsName == "semanticArgv");
  CHECK(error.empty());

  primec::SemanticProgram incompleteSemanticProgram;
  error.clear();
  hasEntryArgs = false;
  entryArgsName.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(
      entryDef, &incompleteSemanticProgram, hasEntryArgs, entryArgsName, error));
  CHECK(error == "missing semantic-product entry parameter fact: /main");

  primec::ir_lowerer::LocalMap locals;
  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(primec::ir_lowerer::isEntryArgsName(entryName, locals, true, "argv"));

  primec::ir_lowerer::LocalInfo shadowed;
  shadowed.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("argv", shadowed);
  CHECK_FALSE(primec::ir_lowerer::isEntryArgsName(entryName, locals, true, "argv"));
  locals.erase("argv");

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, true, "argv"));
  countEntry.name = "/vector/count";
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, true, "argv"));

  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("arr", arrayInfo);
  primec::Expr arrayName;
  arrayName.kind = primec::Expr::Kind::Name;
  arrayName.name = "arr";
  countEntry.args = {arrayName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo refInfo;
  refInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refInfo.referenceToArray = true;
  locals.emplace("arrRef", refInfo);
  primec::Expr refName;
  refName.kind = primec::Expr::Kind::Name;
  refName.name = "arrRef";
  countEntry.args = {refName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::Expr vectorCall;
  vectorCall.kind = primec::Expr::Kind::Call;
  vectorCall.name = "vector";
  vectorCall.templateArgs = {"i64"};
  countEntry.args = {vectorCall};
  countEntry.name = "/std/collections/vector/count";
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo mapRefArgs;
  mapRefArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  mapRefArgs.isArgsPack = true;
  mapRefArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  mapRefArgs.referenceToMap = true;
  mapRefArgs.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapRefArgs.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapRefs", mapRefArgs);

  primec::Expr mapRefsName;
  mapRefsName.kind = primec::Expr::Kind::Name;
  mapRefsName.name = "mapRefs";
  primec::Expr zeroIndex;
  zeroIndex.kind = primec::Expr::Kind::Literal;
  zeroIndex.literalValue = 0;
  primec::Expr mapRefAccess;
  mapRefAccess.kind = primec::Expr::Kind::Call;
  mapRefAccess.name = "at";
  mapRefAccess.args = {mapRefsName, zeroIndex};
  primec::Expr mapRefDeref;
  mapRefDeref.kind = primec::Expr::Kind::Call;
  mapRefDeref.name = "dereference";
  mapRefDeref.args = {mapRefAccess};
  countEntry.name = "count";
  countEntry.args = {mapRefDeref};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo mapPtrArgs;
  mapPtrArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  mapPtrArgs.isArgsPack = true;
  mapPtrArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  mapPtrArgs.pointerToMap = true;
  mapPtrArgs.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapPtrArgs.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapPtrs", mapPtrArgs);

  primec::Expr mapPtrsName;
  mapPtrsName.kind = primec::Expr::Kind::Name;
  mapPtrsName.name = "mapPtrs";
  primec::Expr mapPtrAccess;
  mapPtrAccess.kind = primec::Expr::Kind::Call;
  mapPtrAccess.name = "at";
  mapPtrAccess.args = {mapPtrsName, zeroIndex};
  primec::Expr mapPtrDeref;
  mapPtrDeref.kind = primec::Expr::Kind::Call;
  mapPtrDeref.name = "dereference";
  mapPtrDeref.args = {mapPtrAccess};
  countEntry.args = {mapPtrDeref};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo bufferRefArgs;
  bufferRefArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  bufferRefArgs.isArgsPack = true;
  bufferRefArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  bufferRefArgs.referenceToBuffer = true;
  bufferRefArgs.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("bufferRefs", bufferRefArgs);

  primec::Expr bufferRefsName;
  bufferRefsName.kind = primec::Expr::Kind::Name;
  bufferRefsName.name = "bufferRefs";
  primec::Expr bufferRefAccess;
  bufferRefAccess.kind = primec::Expr::Kind::Call;
  bufferRefAccess.name = "at";
  bufferRefAccess.args = {bufferRefsName, zeroIndex};
  primec::Expr bufferRefDeref;
  bufferRefDeref.kind = primec::Expr::Kind::Call;
  bufferRefDeref.name = "dereference";
  bufferRefDeref.args = {bufferRefAccess};
  countEntry.args = {bufferRefDeref};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo bufferPtrArgs;
  bufferPtrArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  bufferPtrArgs.isArgsPack = true;
  bufferPtrArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  bufferPtrArgs.pointerToBuffer = true;
  bufferPtrArgs.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("bufferPtrs", bufferPtrArgs);

  primec::Expr bufferPtrsName;
  bufferPtrsName.kind = primec::Expr::Kind::Name;
  bufferPtrsName.name = "bufferPtrs";
  primec::Expr bufferPtrAccess;
  bufferPtrAccess.kind = primec::Expr::Kind::Call;
  bufferPtrAccess.name = "at";
  bufferPtrAccess.args = {bufferPtrsName, zeroIndex};
  primec::Expr bufferPtrDeref;
  bufferPtrDeref.kind = primec::Expr::Kind::Call;
  bufferPtrDeref.name = "dereference";
  bufferPtrDeref.args = {bufferPtrAccess};
  countEntry.args = {bufferPtrDeref};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));
}

TEST_SUITE_END();
