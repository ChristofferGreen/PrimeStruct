TEST_CASE("semantics validate source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path semanticsValidatePath = repoRoot / "src" / "semantics" / "SemanticsValidate.cpp";
  const std::filesystem::path semanticsValidateConvertConstructorsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateConvertConstructors.cpp";
  const std::filesystem::path semanticsValidateConvertConstructorsHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateConvertConstructors.h";
  const std::filesystem::path semanticsValidateExperimentalGfxConstructorsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateExperimentalGfxConstructors.cpp";
  const std::filesystem::path semanticsValidateExperimentalGfxConstructorsHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateExperimentalGfxConstructors.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpers.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpers.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersComparePath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersCompare.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersCompareHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersCompare.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersCloneDebugPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersCloneDebug.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersCloneDebug.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersSerializationPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersSerialization.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersSerializationHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersSerialization.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersStatePath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersState.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersStateHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersState.h";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersValidatePath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersValidate.cpp";
  const std::filesystem::path semanticsValidateReflectionGeneratedHelpersValidateHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionGeneratedHelpersValidate.h";
  const std::filesystem::path semanticsValidateReflectionMetadataPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionMetadata.cpp";
  const std::filesystem::path semanticsValidateReflectionMetadataHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateReflectionMetadata.h";
  const std::filesystem::path semanticsValidateTransformsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateTransforms.cpp";
  const std::filesystem::path semanticsValidateTransformsHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateTransforms.h";
  const std::filesystem::path semanticsValidateTransformsEnumsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidateTransformsEnums.cpp";
  REQUIRE(std::filesystem::exists(semanticsValidatePath));
  REQUIRE(std::filesystem::exists(semanticsValidateConvertConstructorsPath));
  REQUIRE(std::filesystem::exists(semanticsValidateConvertConstructorsHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateExperimentalGfxConstructorsPath));
  REQUIRE(std::filesystem::exists(semanticsValidateExperimentalGfxConstructorsHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersCloneDebugPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersComparePath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersCompareHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersSerializationPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersSerializationHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersStatePath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersStateHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersValidatePath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionGeneratedHelpersValidateHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionMetadataPath));
  REQUIRE(std::filesystem::exists(semanticsValidateReflectionMetadataHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateTransformsPath));
  REQUIRE(std::filesystem::exists(semanticsValidateTransformsHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidateTransformsEnumsPath));
  const std::string semanticsValidateSource = readText(semanticsValidatePath);
  const std::string semanticsValidateConvertConstructorsSource = readText(semanticsValidateConvertConstructorsPath);
  const std::string semanticsValidateConvertConstructorsHeaderSource =
      readText(semanticsValidateConvertConstructorsHeaderPath);
  const std::string semanticsValidateExperimentalGfxConstructorsSource =
      readText(semanticsValidateExperimentalGfxConstructorsPath);
  const std::string semanticsValidateExperimentalGfxConstructorsHeaderSource =
      readText(semanticsValidateExperimentalGfxConstructorsHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersSource =
      readText(semanticsValidateReflectionGeneratedHelpersPath);
  const std::string semanticsValidateReflectionGeneratedHelpersHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersCloneDebugSource =
      readText(semanticsValidateReflectionGeneratedHelpersCloneDebugPath);
  const std::string semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersCompareSource =
      readText(semanticsValidateReflectionGeneratedHelpersComparePath);
  const std::string semanticsValidateReflectionGeneratedHelpersCompareHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersCompareHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersSerializationSource =
      readText(semanticsValidateReflectionGeneratedHelpersSerializationPath);
  const std::string semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersSerializationHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersStateSource =
      readText(semanticsValidateReflectionGeneratedHelpersStatePath);
  const std::string semanticsValidateReflectionGeneratedHelpersStateHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersStateHeaderPath);
  const std::string semanticsValidateReflectionGeneratedHelpersValidateSource =
      readText(semanticsValidateReflectionGeneratedHelpersValidatePath);
  const std::string semanticsValidateReflectionGeneratedHelpersValidateHeaderSource =
      readText(semanticsValidateReflectionGeneratedHelpersValidateHeaderPath);
  const std::string semanticsValidateReflectionMetadataSource = readText(semanticsValidateReflectionMetadataPath);
  const std::string semanticsValidateReflectionMetadataHeaderSource =
      readText(semanticsValidateReflectionMetadataHeaderPath);
  const std::string semanticsValidateTransformsSource = readText(semanticsValidateTransformsPath);
  const std::string semanticsValidateTransformsHeaderSource = readText(semanticsValidateTransformsHeaderPath);
  const std::string semanticsValidateTransformsEnumsSource = readText(semanticsValidateTransformsEnumsPath);

  CHECK(semanticsValidateSource.find("#include \"SemanticsValidateConvertConstructors.h\"") != std::string::npos);
  CHECK(semanticsValidateSource.find("#include \"SemanticsValidateExperimentalGfxConstructors.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("#include \"SemanticsValidateReflectionGeneratedHelpers.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("#include \"SemanticsValidateReflectionMetadata.h\"") != std::string::npos);
  CHECK(semanticsValidateSource.find("#include \"SemanticsValidateTransforms.h\"") != std::string::npos);
  CHECK(semanticsValidateSource.find("semantics::applySemanticTransforms(program, semanticTransforms, error)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("semantics::rewriteConvertConstructors(program, error)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("semantics::rewriteExperimentalGfxConstructors(program, error)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("semantics::rewriteReflectionGeneratedHelpers(program, error)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("semantics::rewriteReflectionMetadataQueries(program, error)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find("bool applySemanticTransforms(Program &program,") == std::string::npos);
  CHECK(semanticsValidateSource.find("bool rewriteConvertConstructors(Program &program, std::string &error)") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find("bool rewriteExperimentalGfxConstructors(Program &program, std::string &error)") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find("bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error)") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find("bool rewriteReflectionMetadataQueries(Program &program, std::string &error)") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find("bool validateExprTransforms(const Expr &expr,") == std::string::npos);
  CHECK(semanticsValidateSource.find("bool rewriteEnumDefinitions(Program &program, std::string &error)") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "auto isCanonicalSoaToAosDefinitionPath = [&](std::string_view path)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalPath, \"to_aos\")") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "isCanonicalSoaToAosDefinitionPath(def.fullPath)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "const std::string canonicalToAosImportTarget =") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "localImportPathCoversTarget(importPath, canonicalToAosImportTarget)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "semantics::isExperimentalSoaVectorConversionFamilyPath(def.fullPath)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "semantics::isExperimentalSoaGetLikeHelperPath(receiver.name)") !=
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "receiver.name.rfind(getPrefix, 0) != 0") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "def.fullPath == \"/std/collections/soa_vector/to_aos\"") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "def.fullPath.rfind(\"/std/collections/soa_vector/to_aos__\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "def.fullPath.rfind(\"/std/collections/experimental_soa_vector_conversions/\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsValidateSource.find(
            "localImportPathCoversTarget(\n"
            "            importPath, \"/std/collections/soa_vector/to_aos\")") ==
        std::string::npos);
  CHECK(semanticsValidateConvertConstructorsHeaderSource.find("bool rewriteConvertConstructors(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateConvertConstructorsSource.find("bool rewriteConvertConstructors(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateConvertConstructorsSource.find("std::ostringstream message;") != std::string::npos);
  CHECK(semanticsValidateConvertConstructorsSource.find("expr.name = helpers.front();") != std::string::npos);
  CHECK(semanticsValidateExperimentalGfxConstructorsHeaderSource.find("bool rewriteExperimentalGfxConstructors(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateExperimentalGfxConstructorsSource.find("bool rewriteExperimentalGfxConstructors(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateExperimentalGfxConstructorsSource.find("expr.name = \"create_pipeline_VertexColored\";") !=
        std::string::npos);
  CHECK(semanticsValidateExperimentalGfxConstructorsSource.find("rememberBinding(arg, namespacePrefix, bodyLocals);") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersHeaderSource.find("bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("generated reflection helper already exists: ") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("#include \"SemanticsValidateReflectionGeneratedHelpersCloneDebug.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("#include \"SemanticsValidateReflectionGeneratedHelpersCompare.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("#include \"SemanticsValidateReflectionGeneratedHelpersSerialization.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("#include \"SemanticsValidateReflectionGeneratedHelpersState.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("#include \"SemanticsValidateReflectionGeneratedHelpersValidate.h\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionComparisonHelper(compareContext, \"Equal\", \"equal\", \"and\", true)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionComparisonHelper(compareContext, \"NotEqual\", \"not_equal\", \"or\", false)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionCompareHelper(compareContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionHash64Helper(compareContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionClearHelper(stateContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionCopyFromHelper(stateContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionDefaultHelper(stateContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionIsDefaultHelper(stateContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionCloneHelper(cloneDebugContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionDebugPrintHelper(cloneDebugContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionSerializeHelper(serializationContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionDeserializeHelper(serializationContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionValidateHelper(validationContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("transformHasArgument(transform, \"SoaSchema\")") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("emitReflectionSoaSchemaHelpers(validationContext)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find(
            "emitReflectionSoaSchemaStorageHelpers(validationContext)") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find(
            "const std::unordered_map<std::string, std::string> &fieldVisibilityNames;") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find(
            "const std::unordered_map<std::string, uint32_t> &fieldOffsetBytes;") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find(
            "uint32_t elementStrideBytes = 0;") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateHeaderSource.find(
            "bool emitReflectionSoaSchemaHelpers(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateHeaderSource.find(
            "bool emitReflectionSoaSchemaStorageHelpers(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition countHelper = makeHelper(\"SoaSchemaFieldCount\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition chunkCountHelper = makeHelper(\"SoaSchemaChunkCount\"") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("storageStruct.name = \"SoaSchemaStorage\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition storageNewHelper = makeHelper(\"SoaSchemaStorageNew\"") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition storageCountHelper = makeHelper(\"SoaSchemaStorageCount\"") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition storageCapacityHelper = makeHelper(\"SoaSchemaStorageCapacity\"") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition storageReserveHelper = makeHelper(\"SoaSchemaStorageReserve\"") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition storageClearHelper = makeHelper(\"SoaSchemaStorageClear\"") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "const std::string storageDestroyHelperPath = storageStructPath + \"/Destroy\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "storageDestroyHelper.name = \"Destroy\";") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "clearCall.name = \"SoaSchemaStorageClear\";") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedStringHelper(\"SoaSchemaFieldName\", nameHelperPath, context.fieldNames)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedStringHelper(\"SoaSchemaFieldType\", typeHelperPath, fieldTypes)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedStringHelper(\"SoaSchemaFieldVisibility\", visibilityHelperPath, fieldVisibilities)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedI32Helper(\"SoaSchemaFieldOffset\", offsetHelperPath, fieldOffsets)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Definition strideHelper = makeHelper(\"SoaSchemaElementStride\", strideHelperPath, \"i32\", false);") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedI32Helper(\"SoaSchemaChunkFieldStart\", chunkStartHelperPath, chunkStarts)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "appendIndexedI32Helper(\"SoaSchemaChunkFieldCount\", chunkFieldCountHelperPath, chunkFieldCounts)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "Expr chunkBinding =\n        makeTypeBinding(\"chunk\" + std::to_string(chunkIndex),") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "makeChunkHelperBasePath(\"Reserve\", chunkTemplateArgs[chunkIndex].size())") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find(
            "makeChunkHelperBasePath(\"Clear\", chunkTemplateArgs[chunkIndex].size())") != std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitComparisonHelper = [&](const std::string &helperName,") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitCompareHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitHash64Helper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitClearHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitCopyFromHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitDefaultHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitIsDefaultHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitCloneHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitDebugPrintHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitSerializeHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitDeserializeHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSource.find("auto emitValidateHelper = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderSource.find("bool emitReflectionCloneHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugHeaderSource.find("bool emitReflectionDebugPrintHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugSource.find("bool emitReflectionCloneHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugSource.find("bool emitReflectionDebugPrintHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugSource.find("helper.name = \"Clone\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugSource.find("helper.name = \"DebugPrint\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCloneDebugSource.find("helper.statements.push_back(makeCallExpr(\"print_line\", makeStringLiteralExpr(context.def.fullPath + \" {}\")))") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareHeaderSource.find("bool emitReflectionComparisonHelper(") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareHeaderSource.find("const std::string &helperName") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareHeaderSource.find("bool emitReflectionCompareHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareHeaderSource.find("bool emitReflectionHash64Helper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("bool emitReflectionComparisonHelper(") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("const std::string &comparisonName") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("bool emitReflectionCompareHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("bool emitReflectionHash64Helper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("helper.name = \"Compare\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("helper.name = \"Hash64\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersCompareSource.find("helper.parameters.push_back(makeTypeBinding(\"left\", context.def.fullPath, helper.namespacePrefix));") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateHeaderSource.find("bool emitReflectionClearHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateHeaderSource.find("bool emitReflectionCopyFromHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateHeaderSource.find("bool emitReflectionDefaultHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateHeaderSource.find("bool emitReflectionIsDefaultHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("bool emitReflectionClearHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("bool emitReflectionCopyFromHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("bool emitReflectionDefaultHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("bool emitReflectionIsDefaultHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("helper.name = \"Clear\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("helper.name = \"CopyFrom\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("helper.name = \"Default\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersStateSource.find("helper.name = \"IsDefault\";") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find("struct ReflectionGeneratedHelperContext") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find("bool emitReflectionSerializeHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationHeaderSource.find("bool emitReflectionDeserializeHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationSource.find("bool emitReflectionSerializeHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationSource.find("bool emitReflectionDeserializeHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationSource.find("DeserializePayloadSizeErrorCode") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationSource.find("SerializationFormatVersionTag") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersSerializationSource.find("helper.parameters.push_back(makeTypeBinding(\"payload\", \"array<u64>\", helper.namespacePrefix));") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateHeaderSource.find("bool emitReflectionValidateHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("bool emitReflectionValidateHelper(ReflectionGeneratedHelperContext &context)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("hook.name = \"ValidateField_\" + context.fieldNames[fieldIndex];") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("returnTransform.templateArgs.push_back(\"Result<FileError>\");") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionGeneratedHelpersValidateSource.find("helper.returnExpr = makeOkResultExpr();") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionMetadataHeaderSource.find("bool rewriteReflectionMetadataQueries(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionMetadataSource.find("bool rewriteReflectionMetadataQueries(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateReflectionMetadataSource.find("ParsedStringLiteral parsed;") != std::string::npos);
  CHECK(semanticsValidateReflectionMetadataSource.find("queryName != \"type_name\"") != std::string::npos);
  CHECK(semanticsValidateReflectionMetadataSource.find("meta.has_trait requires one or two template arguments") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsHeaderSource.find("bool applySemanticTransforms(Program &program,") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsSource.find("bool applySemanticTransforms(Program &program,") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsSource.find("bool validateExprTransforms(const Expr &expr,") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsEnumsSource.find("bool rewriteEnumDefinitions(Program &program, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsSource.find("bool rewriteSharedScopeStatements(std::vector<Expr> &statements, std::string &error)") !=
        std::string::npos);
  CHECK(semanticsValidateTransformsSource.find("bool applySingleTypeToReturn(std::vector<Transform> &transforms,") !=
        std::string::npos);
}
