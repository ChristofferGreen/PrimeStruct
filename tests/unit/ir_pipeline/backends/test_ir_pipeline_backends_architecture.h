TEST_CASE("main routes glsl and spirv through ir backends without legacy fallback branches") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "bin" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "bin" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("resolveIrBackendEmitKind(options.emitKind)") != std::string::npos);
  CHECK(source.find("findIrBackend(options.emitKind)") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"glsl\")") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"spirv\")") == std::string::npos);
  CHECK(source.find("if (irBackend == nullptr && options.emitKind == \"glsl\")") == std::string::npos);
  CHECK(source.find("if (irBackend == nullptr && options.emitKind == \"spirv\")") == std::string::npos);
  CHECK(source.find("if (irFailure.stage != IrBackendRunFailureStage::Emit)") == std::string::npos);
}

TEST_CASE("stdlib surface registry stays source locked") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path headerPath = cwd / "include" / "primec" / "support" / "StdlibSurfaceRegistry.h";
  std::filesystem::path sourcePath = cwd / "src" / "support" / "StdlibSurfaceRegistry.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
  }
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "include" / "primec" / "support" / "StdlibSurfaceRegistry.h";
  }
  if (!std::filesystem::exists(sourcePath)) {
    sourcePath = cwd.parent_path() / "src" / "support" / "StdlibSurfaceRegistry.cpp";
  }

  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);

  CHECK(cmake.find("src/support/StdlibSurfaceRegistry.cpp") != std::string::npos);

  CHECK(header.find("enum class StdlibSurfaceDomain") != std::string::npos);
  CHECK(header.find("enum class StdlibSurfaceShape") != std::string::npos);
  CHECK(header.find("enum class StdlibSurfaceId") != std::string::npos);
  CHECK(header.find("struct StdlibSurfaceMemberAlias") != std::string::npos);
  CHECK(header.find("std::span<const std::string_view> memberNames;") != std::string::npos);
  CHECK(header.find("std::span<const StdlibSurfaceMemberAlias> memberAliases;") !=
        std::string::npos);
  CHECK(header.find("std::span<const std::string_view> statementMemberNames;") !=
        std::string::npos);
  CHECK(header.find("std::span<const std::string_view> importAliasSpellings;") != std::string::npos);
  CHECK(header.find("std::span<const std::string_view> compatibilitySpellings;") != std::string::npos);
  CHECK(header.find("std::span<const std::string_view> loweringSpellings;") != std::string::npos);
  CHECK(header.find("findStdlibSurfaceMetadataBySpelling") != std::string::npos);
  CHECK(header.find("findStdlibSurfaceMetadataByResolvedPath") != std::string::npos);
  CHECK(header.find("resolveStdlibSurfaceMemberName") != std::string::npos);
  CHECK(header.find("stdlibSurfaceCanonicalHelperPath") != std::string::npos);
  CHECK(header.find("stdlibSurfacePreferredSpellingForMember") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::FileHelpers") != std::string::npos);
  CHECK(source.find("\"file.file_helpers\"") != std::string::npos);
  CHECK(source.find("\"/std/file/File\"") != std::string::npos);
  CHECK(source.find("\"/File\"") != std::string::npos);
  CHECK(source.find("\"write_line\"") != std::string::npos);
  CHECK(source.find("\"/file/write\"") != std::string::npos);
  CHECK(source.find("\"/file/write_line\"") != std::string::npos);
  CHECK(source.find("\"/file/write_bytes\"") != std::string::npos);
  CHECK(source.find("\"/file/close\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::FileErrorHelpers") != std::string::npos);
  CHECK(source.find("\"file.file_error\"") != std::string::npos);
  CHECK(source.find("\"/std/file/FileError\"") != std::string::npos);
  CHECK(source.find("\"/FileError\"") != std::string::npos);
  CHECK(source.find("\"is_eof\"") != std::string::npos);
  CHECK(source.find("\"/std/file/fileErrorResult\"") != std::string::npos);
  CHECK(source.find("\"/file_error/why\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsManifestSurface0") != std::string::npos);
  CHECK(source.find("loadCollectionsManifestSurfaces()") == std::string::npos);
  CHECK(source.find("surfaces.psmeta") == std::string::npos);
  CHECK(source.find("resolveMetadataMemberName(") != std::string::npos);
  CHECK(source.find("deriveCollectionsSurfaces(") != std::string::npos);
  CHECK(source.find("scanStdlibPublicFunctions(") != std::string::npos);
  CHECK(source.find("deriveAndVerifyCollectionsSurfaces(") == std::string::npos);
  CHECK(source.find("\"/std/collections/vector\"") != std::string::npos);
  CHECK(source.find("\"/vector/count\"") == std::string::npos);
  CHECK(source.find("\"/vector/remove_swap\"") == std::string::npos);
  CHECK(source.find("\"remove_swap\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_vector/vectorRemoveSwap\"") ==
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsManifestSurface1") == std::string::npos);
  CHECK(source.find(".id = collectionSurfaceId(1)") != std::string::npos);
  CHECK(source.find("\"/std/collections/vector/vector\"") != std::string::npos);
  CHECK(source.find("\"vectorSingle\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_vector/vectorPair\"") ==
        std::string::npos);
  CHECK(source.find("\"collections.vector_helpers\"") != std::string::npos);
  CHECK(source.find("\"collections.vector_constructors\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsMapHelpers") == std::string::npos);
  CHECK(source.find(".id = collectionSurfaceId(2)") != std::string::npos);
  CHECK(source.find("\"collections.map_helpers\"") != std::string::npos);
  CHECK(source.find("\"/map/count\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/mapInsert\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapCount\"") ==
        std::string::npos);
  CHECK(source.find("stripResolvedPathSpecializationSuffix(") != std::string::npos);
  CHECK(source.find("resolveCollectionsVectorMemberName(") == std::string::npos);
  CHECK(source.find("resolveCollectionsMapHelperMemberName(") == std::string::npos);
  CHECK(source.find("CollectionsMapHelperMembers") == std::string::npos);
  CHECK(source.find("CollectionsMapConstructorMembers") == std::string::npos);
  CHECK(source.find("resolveStdlibSurfaceMemberName(const StdlibSurfaceMetadata &metadata,") !=
        std::string::npos);
  CHECK(source.find("stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId id,") !=
        std::string::npos);
  CHECK(source.find("stdlibSurfacePreferredSpellingForMember(StdlibSurfaceId id,") !=
        std::string::npos);
  CHECK(source.find("matchesResolvedRootedMemberPath(") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsMapConstructors") == std::string::npos);
  CHECK(source.find(".id = collectionSurfaceId(3)") != std::string::npos);
  CHECK(source.find("\"collections.map_constructors\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/mapNew\"") == std::string::npos);
  CHECK(source.find("\"mapOct\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapOct\"") ==
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsColumnarHelpers") == std::string::npos);
  CHECK(source.find(".id = collectionSurfaceId(4)") != std::string::npos);
  CHECK(source.find("\"collections.soa_helpers\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/soa\"") != std::string::npos);
  CHECK(source.find("/std/collections/experimental_soa") == std::string::npos);
  CHECK(source.find("\"field_view\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/count\"") == std::string::npos);
  CHECK(source.find("\"/soa/to_aos\"") == std::string::npos);
  CHECK(source.find("\"soaVectorGetRef\"") == std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa/soaVectorPush\"") ==
        std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa_conversions/soaVectorToAos\"") ==
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsColumnarConstructors") !=
        std::string::npos);
  CHECK(source.find("\"collections.soa_constructors\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/soa/soa\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa/soaVectorNew\"") ==
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsContainerErrorHelpers") != std::string::npos);
  CHECK(source.find("\"collections.container_error\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/ContainerError\"") != std::string::npos);
  CHECK(source.find("\"/ContainerError\"") != std::string::npos);
  CHECK(source.find("\"capacity_exceeded\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/containerErrorResult\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::GfxBufferHelpers") != std::string::npos);
  CHECK(source.find("\"gfx.buffer_helpers\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/Buffer\"") != std::string::npos);
  CHECK(source.find("\"/Buffer\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/experimental/Buffer\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/experimental/Buffer/count\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/experimental/Buffer/upload\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/Buffer/is_valid\"") != std::string::npos);
  CHECK(source.find("\"/std/gpu/buffer_load\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::GfxErrorHelpers") != std::string::npos);
  CHECK(source.find("\"gfx.gfx_error\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/GfxError\"") != std::string::npos);
  CHECK(source.find("\"/GfxError\"") != std::string::npos);
  CHECK(source.find("\"/std/gfx/experimental/GfxError\"") != std::string::npos);
  CHECK(source.find("\"frame_present_failed\"") != std::string::npos);
}

TEST_CASE("map insert surface registry rejects legacy compatibility spellings") {
  const primec::StdlibSurfaceMetadata *metadata =
      primec::findStdlibSurfaceMetadata(primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  REQUIRE(metadata != nullptr);

  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "insert") == "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/map/insert") ==
        "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/map/insert").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/mapInsert").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *metadata, "/std/collections/experimental_map/mapInsert").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "insert_ref") == "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/map/insert_ref") ==
        "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/map/insert_ref").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/mapInsertRef").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *metadata, "/std/collections/experimental_map/mapInsertRef").empty());

  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id, "insert") ==
        "/std/collections/map/insert");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
            "/std/collections/map/insert_ref") == "/std/collections/map/insert_ref");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
            "/std/collections/mapInsertRef") == "");
}

TEST_CASE("collection helper surface registry resolves preferred compatibility spellings") {
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsManifestSurface0,
            "/std/collections/vector/count",
            "/std/collections/experimental_vector/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsManifestSurface0,
            "/vector/remove_swap",
            "/std/collections/experimental_vector/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
            "/std/collections/map/contains_ref",
            "/std/collections/experimental_map/") == "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
            "/std/collections/mapAtUnsafe",
            "/std/collections/experimental_map/") == "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
            "/not_map/count",
            "/std/collections/experimental_map/") == "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/std/collections/soa/count",
            "/std/collections/experimental_soa/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/std/collections/soa/count",
            "/std/collections/experimental_soa/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/std/collections/count",
            "/std/collections/experimental_soa/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/soa/push",
            "/std/collections/experimental_soa/") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/std/collections/soa/to_aos",
            "/std/collections/experimental_soa_conversions/") ==
        "");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/std/collections/experimental_soa/soaVectorGetRef") ==
        "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsColumnarHelpers,
            "/not_soa/count",
            "/std/collections/experimental_soa/") == "");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::GfxBufferHelpers,
            "/std/gfx/experimental/Buffer/count") ==
        "/std/gfx/Buffer/count");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::GfxBufferHelpers,
            "/std/gfx/experimental/Buffer/is_valid") ==
        "/std/gfx/Buffer/is_valid");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::GfxBufferHelpers,
            "/Buffer/upload") ==
        "/std/gfx/Buffer/upload");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::GfxBufferHelpers,
            "/not_gfx/upload") == "");
}

TEST_CASE("map insert semantic rewrite uses stdlib surface adapter") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidate.cpp";
  if (!std::filesystem::exists(sourcePath)) {
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidate.cpp";
  }
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string source = readTextFile(sourcePath);
  CHECK(source.find("#include \"primec/support/StdlibSurfaceRegistry.h\"") != std::string::npos);
  CHECK(source.find("resolveBuiltinKeyValueInsertSurfaceMemberName(") != std::string::npos);
  CHECK(source.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(source.find("resolveStdlibSurfaceMemberName(*metadata, name)") != std::string::npos);
  CHECK(source.find("stdlibSurfaceCanonicalHelperPath(\n"
                    "      metadata->id,") != std::string::npos);
  CHECK(source.find("kBuiltinMapInsertAliasPath") == std::string::npos);
  CHECK(source.find("kBuiltinExperimentalMapInsertPath") == std::string::npos);
  CHECK(source.find("kBuiltinMapInsertRefWrapperPath") == std::string::npos);
}

TEST_CASE("gfx buffer semantic rewrite uses stdlib surface adapter") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path sourcePath =
      cwd / "src" / "semantics" / "SemanticsValidateExperimentalGfxConstructors.cpp";
  if (!std::filesystem::exists(sourcePath)) {
    sourcePath = cwd.parent_path() / "src" / "semantics" /
                 "SemanticsValidateExperimentalGfxConstructors.cpp";
  }
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string source = readTextFile(sourcePath);
  CHECK(source.find("#include \"primec/support/StdlibSurfaceRegistry.h\"") != std::string::npos);
  CHECK(source.find("canonicalGfxBufferHelperPath") != std::string::npos);
  CHECK(source.find("findStdlibSurfaceMetadataByResolvedPath(path)") !=
        std::string::npos);
  CHECK(source.find("StdlibSurfaceId::GfxBufferHelpers") != std::string::npos);
  CHECK(source.find("expr.name == \"/std/gfx/Buffer/allocate\" ||") ==
        std::string::npos);
  CHECK(source.find("expr.name == \"/std/gfx/Buffer/load\" ||") ==
        std::string::npos);
  CHECK(source.find("directReceiverPath == \"/std/gfx/Buffer\"") ==
        std::string::npos);
  CHECK(source.find("receiverPath == \"/std/gfx/experimental/Buffer\"") ==
        std::string::npos);
}

TEST_CASE("include layer guardrail baseline tracks existing private test headers") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path scriptPath = cwd / "scripts" / "check_include_layers.py";
  std::filesystem::path allowlistPath = cwd / "scripts" / "include_layer_allowlist.txt";
  std::filesystem::path emitterTestApiPath = cwd / "include" / "primec" / "testing" / "EmitterHelpers.h";
  std::filesystem::path irLowererTestApiPath = cwd / "include" / "primec" / "testing" / "IrLowererHelpers.h";
  std::filesystem::path irLowererCountAccessContractsApiPath =
      cwd / "include" / "primec" / "testing" / "IrLowererCountAccessContracts.h";
  std::filesystem::path irLowererStageContractsApiPath =
      cwd / "include" / "primec" / "testing" / "IrLowererStageContracts.h";
  std::filesystem::path soaPathHelpersApiPath =
      cwd / "include" / "primec" / "ir" / "SoaPathHelpers.h";
  std::filesystem::path parserTestApiPath = cwd / "include" / "primec" / "testing" / "ParserHelpers.h";
  std::filesystem::path semanticsControlFlowApiPath =
      cwd / "include" / "primec" / "testing" / "SemanticsControlFlowProbes.h";
  std::filesystem::path semanticsGraphTestApiPath = cwd / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
  std::filesystem::path semanticsTestApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path textFilterTestApiPath = cwd / "include" / "primec" / "testing" / "TextFilterHelpers.h";
  std::filesystem::path parserHelperTestPath =
      cwd / "tests" / "unit" / "parser" / "test_parser_basic_parser_helpers.cpp";
  std::filesystem::path textFilterHelperTestPath =
      cwd / "tests" / "unit" / "text_filter" / "test_text_filter_helpers.cpp";
  std::filesystem::path compileRunTestPath =
      cwd / "tests" / "unit" / "compile_run" / "vm" / "test_compile_run_vm_bounds.cpp";
  std::filesystem::path irPipelineTestPath = cwd / "tests" / "unit" / "ir_pipeline" / "test_ir_pipeline.cpp";
  std::filesystem::path validationHelpersTestPath =
      cwd / "tests" / "unit" / "ir_pipeline" / "validation" / "test_ir_pipeline_validation_helpers.h";
  std::filesystem::path countAccessValidationTestPath =
      cwd / "tests" / "unit" / "ir_pipeline" / "validation" / "test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = cwd.parent_path() / "scripts" / "check_include_layers.py";
    allowlistPath = cwd.parent_path() / "scripts" / "include_layer_allowlist.txt";
    emitterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "EmitterHelpers.h";
    irLowererTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererHelpers.h";
    irLowererCountAccessContractsApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererCountAccessContracts.h";
    irLowererStageContractsApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererStageContracts.h";
    soaPathHelpersApiPath = cwd.parent_path() / "include" / "primec" / "ir" / "SoaPathHelpers.h";
    parserTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "ParserHelpers.h";
    semanticsControlFlowApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsControlFlowProbes.h";
    semanticsGraphTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
    semanticsTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    textFilterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "TextFilterHelpers.h";
    parserHelperTestPath =
        cwd.parent_path() / "tests" / "unit" / "parser" / "test_parser_basic_parser_helpers.cpp";
    textFilterHelperTestPath =
        cwd.parent_path() / "tests" / "unit" / "text_filter" / "test_text_filter_helpers.cpp";
    compileRunTestPath =
        cwd.parent_path() / "tests" / "unit" / "compile_run" / "vm" / "test_compile_run_vm_bounds.cpp";
    irPipelineTestPath = cwd.parent_path() / "tests" / "unit" / "ir_pipeline" / "test_ir_pipeline.cpp";
    validationHelpersTestPath =
        cwd.parent_path() / "tests" / "unit" / "ir_pipeline" / "validation" / "test_ir_pipeline_validation_helpers.h";
    countAccessValidationTestPath =
        cwd.parent_path() / "tests" / "unit" / "ir_pipeline" / "validation" / "test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp";
  }
  REQUIRE(std::filesystem::exists(scriptPath));
  REQUIRE(std::filesystem::exists(allowlistPath));
  REQUIRE(std::filesystem::exists(emitterTestApiPath));
  REQUIRE(std::filesystem::exists(irLowererTestApiPath));
  REQUIRE(std::filesystem::exists(irLowererCountAccessContractsApiPath));
  REQUIRE(std::filesystem::exists(irLowererStageContractsApiPath));
  REQUIRE(std::filesystem::exists(soaPathHelpersApiPath));
  REQUIRE(std::filesystem::exists(parserTestApiPath));
  REQUIRE(std::filesystem::exists(semanticsControlFlowApiPath));
  REQUIRE(std::filesystem::exists(semanticsGraphTestApiPath));
  REQUIRE(std::filesystem::exists(semanticsTestApiPath));
  REQUIRE(std::filesystem::exists(textFilterTestApiPath));
  REQUIRE(std::filesystem::exists(parserHelperTestPath));
  REQUIRE(std::filesystem::exists(textFilterHelperTestPath));
  REQUIRE(std::filesystem::exists(compileRunTestPath));
  REQUIRE(std::filesystem::exists(irPipelineTestPath));
  REQUIRE(std::filesystem::exists(validationHelpersTestPath));
  REQUIRE(std::filesystem::exists(countAccessValidationTestPath));

  const std::string script = readTextFile(scriptPath);
  CHECK(script.find("public headers must not include private src headers") != std::string::npos);
  CHECK(script.find("production sources must not include test headers") != std::string::npos);
  CHECK(script.find("direct tests -> src include is not allowlisted") != std::string::npos);
  CHECK(script.find("lowerer sources must not include private semantics headers") !=
        std::string::npos);
  CHECK(script.find("resolve_repo_include_path") != std::string::npos);
  CHECK(script.find("allowlisted private include-layer dependencies remain") !=
        std::string::npos);
  CHECK(script.find("stale allowlist entry should be removed") != std::string::npos);

  const std::string allowlist = readTextFile(allowlistPath);
  const std::string soaPathHelpers = readTextFile(soaPathHelpersApiPath);
  CHECK(soaPathHelpers.find("namespace primec::soa_paths") != std::string::npos);
  CHECK(soaPathHelpers.find("isExperimentalColumnarVectorSpecializedTypePath") !=
        std::string::npos);
  CHECK(soaPathHelpers.find("canonicalizeLegacySoaRefHelperPath") !=
        std::string::npos);
  CHECK(allowlist.find("# No current lowerer -> private semantics helper dependencies.") !=
        std::string::npos);
  CHECK(allowlist.find("src/ir_lowerer/IrLowererBindingTypeHelpers.cpp -> src/semantics/SemanticsHelpers.h") ==
        std::string::npos);
  CHECK(allowlist.find("-> src/semantics/SemanticsHelpers.h") == std::string::npos);
  CHECK(allowlist.find("src/ir_lowerer/ -> src/semantics/") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/emitter/") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/ir_lowerer/") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/semantics/SemanticsValidatorExprCaptureSplitStep.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/semantics/SemanticsValidatorStatementLoopCountStep.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_compile_run.cpp -> src/emitter/EmitterHelpers.h") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_parser_basic_parser_helpers.h -> src/parser/ParserHelpers.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_text_filter_helpers.cpp -> src/text_filter/TextFilterHelpers.h") ==
        std::string::npos);

  const std::string emitterTestApi = readTextFile(emitterTestApiPath);
  CHECK(emitterTestApi.find("namespace primec::emitter") != std::string::npos);
  CHECK(emitterTestApi.find("bool runEmitterEmitSetupMathImport(const Program &program);") != std::string::npos);
  CHECK(emitterTestApi.find("std::optional<EmitterLifecycleHelperMatch> runEmitterEmitSetupLifecycleHelperMatchStep") !=
        std::string::npos);
  CHECK(emitterTestApi.find("EmitterExprControlIfBranchBodyEmitResult") != std::string::npos);
  CHECK(emitterTestApi.find("EmitterExprControlIfTernaryFallbackStepResult") != std::string::npos);

  const std::string irLowererTestApi = readTextFile(irLowererTestApiPath);
  const std::string irLowererCountAccessContractsApi =
      readTextFile(irLowererCountAccessContractsApiPath);
  const std::string irLowererStageContractsApi = readTextFile(irLowererStageContractsApiPath);
  CHECK(irLowererTestApi.find("namespace primec::ir_lowerer") != std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererSharedTypes.h\"") !=
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererFlowHelpers.h\"") !=
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererStringCallHelpers.h\"") !=
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererUninitializedTypeHelpers.h\"") !=
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerInferenceSetup.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerSetupStage.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerReturnEmitStage.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsCallsStage.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsCallsStep.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsEntryExecutionStep.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsEntryStatementStep.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsFunctionTableStep.h\"") ==
        std::string::npos);
  CHECK(irLowererTestApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsSourceMapStep.h\"") ==
        std::string::npos);

  CHECK(irLowererCountAccessContractsApi.find("namespace primec::ir_lowerer") !=
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("IrLowererCountAccessHelpers.h") !=
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("IrLowererCallDispatchHelpers.h") !=
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("IrLowererSharedTypes.h") !=
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("primec/testing/IrLowererHelpers.h") ==
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("IrLowererFlowHelpers.h") ==
        std::string::npos);
  CHECK(irLowererCountAccessContractsApi.find("IrLowererUninitializedTypeHelpers.h") ==
        std::string::npos);

  CHECK(irLowererStageContractsApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerInferenceSetup.h\"") !=
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerSetupStage.h\"") !=
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerReturnEmitStage.h\"") !=
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("#include \"primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsCallsStage.h\"") !=
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("IrLowererLowerStatementsCallsStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("IrLowererLowerStatementsEntryExecutionStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("IrLowererLowerStatementsEntryStatementStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("IrLowererLowerStatementsFunctionTableStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsApi.find("IrLowererLowerStatementsSourceMapStep.h") ==
        std::string::npos);

  const std::string parserTestApi = readTextFile(parserTestApiPath);
  const std::string semanticsControlFlowApi = readTextFile(semanticsControlFlowApiPath);
  const std::string semanticsGraphTestApi = readTextFile(semanticsGraphTestApiPath);
  CHECK(parserTestApi.find("namespace primec::parser") != std::string::npos);
  CHECK(parserTestApi.find("bool isBuiltinName(const std::string &name, bool allowMathBare);") !=
        std::string::npos);

  CHECK(semanticsControlFlowApi.find("namespace primec::semantics") != std::string::npos);
  CHECK(semanticsControlFlowApi.find("struct ExprCaptureSplitProbeSnapshotForTesting") !=
        std::string::npos);
  CHECK(semanticsControlFlowApi.find("struct LoopCountProbeSnapshotForTesting") != std::string::npos);
  CHECK(semanticsControlFlowApi.find("probeExprCaptureSplitForTesting") != std::string::npos);
  CHECK(semanticsControlFlowApi.find("probeLoopCountForTesting") != std::string::npos);

  CHECK(semanticsGraphTestApi.find("namespace primec::semantics") != std::string::npos);
  CHECK(semanticsGraphTestApi.find("struct TypeResolutionGraphSnapshotNode") != std::string::npos);
  CHECK(semanticsGraphTestApi.find("computeStronglyConnectedComponentsForTesting") != std::string::npos);
  CHECK(semanticsGraphTestApi.find("struct TemplatedFallbackQueryStateEnvelopeSnapshotForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("struct ExplicitTemplateArgResolutionFactForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("struct ImplicitTemplateArgResolutionFactForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("collectExplicitTemplateArgResolutionFactsForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("collectImplicitTemplateArgResolutionFactsForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("collectExplicitTemplateArgFactConsumptionMetricsForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("collectImplicitTemplateArgFactConsumptionMetricsForTesting") !=
        std::string::npos);
  CHECK(semanticsGraphTestApi.find("classifyTemplatedFallbackQueryTypeTextForTesting") !=
        std::string::npos);

  const std::string textFilterTestApi = readTextFile(textFilterTestApiPath);
  CHECK(textFilterTestApi.find("namespace primec::text_filter") != std::string::npos);
  CHECK(textFilterTestApi.find("bool isSeparator(char c);") != std::string::npos);
  CHECK(textFilterTestApi.find("std::string maybeAppendUtf8(const std::string &token);") != std::string::npos);

  const std::string semanticsTestApi = readTextFile(semanticsTestApiPath);
  CHECK(semanticsTestApi.find("namespace primec::semantics") != std::string::npos);
  CHECK(semanticsTestApi.find("std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep") ==
        std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementKnownIterationCountStep") == std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementCanIterateMoreThanOnceStep") ==
        std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementIsNegativeIntegerLiteralStep") ==
        std::string::npos);
  CHECK(semanticsTestApi.find("TemplatedFallbackQueryStateEnvelopeSnapshotForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("ExplicitTemplateArgResolutionFactForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("ImplicitTemplateArgResolutionFactForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("collectExplicitTemplateArgResolutionFactsForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("collectImplicitTemplateArgResolutionFactsForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("collectExplicitTemplateArgFactConsumptionMetricsForTesting") ==
        std::string::npos);
  CHECK(semanticsTestApi.find("collectImplicitTemplateArgFactConsumptionMetricsForTesting") ==
        std::string::npos);
  CHECK(semanticsTestApi.find("classifyTemplatedFallbackQueryTypeTextForTesting") == std::string::npos);
  CHECK(semanticsTestApi.find("buildTypeResolutionGraphForTesting") == std::string::npos);

  const std::string parserHelperTest = readTextFile(parserHelperTestPath);
  CHECK(parserHelperTest.find("#include \"primec/testing/ParserHelpers.h\"") != std::string::npos);
  CHECK(parserHelperTest.find("#include \"src/parser/ParserHelpers.h\"") == std::string::npos);

  const std::string textFilterHelperTest = readTextFile(textFilterHelperTestPath);
  CHECK(textFilterHelperTest.find("#include \"primec/testing/TextFilterHelpers.h\"") != std::string::npos);
  CHECK(textFilterHelperTest.find("#include \"src/text_filter/TextFilterHelpers.h\"") == std::string::npos);

  const std::string compileRunTest = readTextFile(compileRunTestPath);
  CHECK(compileRunTest.find("#include \"src/emitter/EmitterHelpers.h\"") == std::string::npos);

  const std::string irPipelineTest = readTextFile(irPipelineTestPath);
  const std::string validationHelpersTest = readTextFile(validationHelpersTestPath);
  const std::string countAccessValidationTest = readTextFile(countAccessValidationTestPath);
  CHECK(irPipelineTest.find("#include \"primec/testing/EmitterHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"primec/testing/IrLowererHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"primec/testing/SemanticsValidationHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/emitter/") == std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/ir_lowerer/") == std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/semantics/SemanticsValidatorExprCaptureSplitStep.h\"") ==
        std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/semantics/SemanticsValidatorStatementLoopCountStep.h\"") ==
        std::string::npos);
  CHECK(validationHelpersTest.find("#include \"primec/testing/IrLowererHelpers.h\"") != std::string::npos);
  CHECK(validationHelpersTest.find("#include \"primec/testing/IrLowererStageContracts.h\"") !=
        std::string::npos);
  CHECK(validationHelpersTest.find("#include \"primec/testing/SemanticsControlFlowProbes.h\"") !=
        std::string::npos);
  CHECK(countAccessValidationTest.find("#include \"primec/testing/IrLowererCountAccessContracts.h\"") !=
        std::string::npos);
  CHECK(countAccessValidationTest.find("#include \"test_ir_pipeline_validation_helpers.h\"") ==
        std::string::npos);
  CHECK(countAccessValidationTest.find("#include \"primec/testing/IrLowererHelpers.h\"") ==
        std::string::npos);
}

TEST_CASE("glsl and spirv ir backends use glsl ir validation target") {
  primec::Options options;

  const primec::IrBackend *glslBackend = primec::findIrBackend("glsl-ir");
  REQUIRE(glslBackend != nullptr);
  CHECK(glslBackend->validationTarget(options) == primec::IrValidationTarget::Glsl);

  const primec::IrBackend *spirvBackend = primec::findIrBackend("spirv-ir");
  REQUIRE(spirvBackend != nullptr);
  CHECK(spirvBackend->validationTarget(options) == primec::IrValidationTarget::Glsl);
}
