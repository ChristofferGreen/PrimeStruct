TEST_CASE("main routes glsl and spirv through ir backends without legacy fallback branches") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
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

TEST_CASE("design doc records backend boundary policy") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(designPath)) {
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(designPath));

  const std::string design = readTextFile(designPath);
  CHECK(design.find("all codegen modes consume canonical IR via") != std::string::npos);
  CHECK(design.find("`IrBackend`") != std::string::npos);
  CHECK(design.find("including production aliases (`cpp`, `exe`, `glsl`, `spirv`)") != std::string::npos);
}

TEST_CASE("design doc records semantic ownership boundary policy") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(designPath)) {
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(designPath));

  const std::string design = readTextFile(designPath);
  CHECK(design.find("semantic product follow an explicit ownership split") != std::string::npos);
  CHECK(design.find("benchmark-only") != std::string::npos);
  CHECK(design.find("shadow comparisons must stay isolated from production") != std::string::npos);
  CHECK(design.find("lowering/publication paths.") != std::string::npos);
}

TEST_CASE("design doc records vector map bridge contract") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(designPath)) {
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(designPath));

  const std::string design = readTextFile(designPath);
  CHECK(design.find("### Vector/Map Bridge Contract") != std::string::npos);
  CHECK(design.find("scope reference for the vector/map ownership-cutover lane") !=
        std::string::npos);
  CHECK(design.find("Bridge-owned public contract:") != std::string::npos);
  CHECK(design.find("exact and wildcard `/std/collections`") != std::string::npos);
  CHECK(design.find("Migration-only seams:") != std::string::npos);
  CHECK(design.find("rooted `/vector/*` and `/map/*` spellings") != std::string::npos);
  CHECK(design.find("Compatibility adapter inventory:") != std::string::npos);
  CHECK(design.find("map insert helper compatibility is the\n"
                    "  first migrated family") == std::string::npos);
  CHECK(design.find("map insert helper compatibility was the\n"
                    "  first migrated family") != std::string::npos);
  CHECK(design.find("Template monomorphization now asks the same registry") !=
        std::string::npos);
  CHECK(design.find("SoA helper compatibility is routed\n"
                    "  through `StdlibSurfaceRegistry::CollectionsSoaVectorHelpers`") !=
        std::string::npos);
  CHECK(design.find("Gfx Buffer helper compatibility is routed through\n"
                    "  `StdlibSurfaceRegistry::GfxBufferHelpers`") !=
        std::string::npos);
  CHECK(design.find("Out of scope for this bridge lane:** `array<T>` core ownership,") !=
        std::string::npos);
}

TEST_CASE("design doc records stdlib de-experimentalization policy") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(designPath)) {
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(designPath));

  const std::string design = readTextFile(designPath);
  CHECK(design.find("### Stdlib De-Experimentalization Policy") != std::string::npos);
  CHECK(design.find("Canonical public API:") != std::string::npos);
  CHECK(design.find("Temporary compatibility namespace:") != std::string::npos);
  CHECK(design.find("Internal substrate/helper namespace:") != std::string::npos);
  CHECK(design.find("sole public namespaced vector contract") != std::string::npos);
  CHECK(design.find("sole public namespaced map contract") != std::string::npos);
  CHECK(design.find("/std/collections/experimental_vector/*") != std::string::npos);
  CHECK(design.find("Internal implementation module behind the canonical `/std/collections/vector/*` public contract") !=
        std::string::npos);
  CHECK(design.find("Internal implementation module behind the canonical `/std/collections/map/*` public contract") !=
        std::string::npos);
  CHECK(design.find("/std/gfx/experimental/*") != std::string::npos);
  CHECK(design.find("Legacy compatibility shim over canonical `/std/gfx/*`") != std::string::npos);
  CHECK(design.find("no longer part of the public gfx contract") != std::string::npos);
  CHECK(design.find("/std/collections/internal_buffer_checked/*") != std::string::npos);
  CHECK(design.find("/std/collections/internal_buffer_unchecked/*") != std::string::npos);
  CHECK(design.find("/std/collections/internal_soa_storage/*") != std::string::npos);
}

TEST_CASE("design doc records soa maturity track") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(designPath)) {
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(designPath));

  const std::string design = readTextFile(designPath);
  CHECK(design.find("### SoA Maturity Track") != std::string::npos);
  CHECK(design.find("`soa_vector<T>` remains an incubating public extension") !=
        std::string::npos);
  CHECK(design.find("/std/collections/soa_vector/*") != std::string::npos);
  CHECK(design.find("/std/collections/experimental_soa_vector/*") != std::string::npos);
  CHECK(design.find("/std/collections/internal_soa_storage/*") != std::string::npos);
}

TEST_CASE("stdlib surface registry stays source locked") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path headerPath = cwd / "include" / "primec" / "StdlibSurfaceRegistry.h";
  std::filesystem::path sourcePath = cwd / "src" / "StdlibSurfaceRegistry.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
  }
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "include" / "primec" / "StdlibSurfaceRegistry.h";
  }
  if (!std::filesystem::exists(sourcePath)) {
    sourcePath = cwd.parent_path() / "src" / "StdlibSurfaceRegistry.cpp";
  }

  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);

  CHECK(cmake.find("src/StdlibSurfaceRegistry.cpp") != std::string::npos);

  CHECK(header.find("enum class StdlibSurfaceDomain") != std::string::npos);
  CHECK(header.find("enum class StdlibSurfaceShape") != std::string::npos);
  CHECK(header.find("enum class StdlibSurfaceId") != std::string::npos);
  CHECK(header.find("std::span<const std::string_view> memberNames;") != std::string::npos);
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

  CHECK(source.find("StdlibSurfaceId::CollectionsVectorHelpers") != std::string::npos);
  CHECK(source.find("\"collections.vector_helpers\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/vector\"") != std::string::npos);
  CHECK(source.find("\"/vector/count\"") != std::string::npos);
  CHECK(source.find("\"/vector/remove_swap\"") != std::string::npos);
  CHECK(source.find("\"remove_swap\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_vector/vectorRemoveSwap\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsVectorConstructors") != std::string::npos);
  CHECK(source.find("\"collections.vector_constructors\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/vector/vector\"") != std::string::npos);
  CHECK(source.find("\"vectorSingle\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_vector/vectorPair\"") !=
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsMapHelpers") != std::string::npos);
  CHECK(source.find("\"collections.map_helpers\"") != std::string::npos);
  CHECK(source.find("\"/map/count\"") != std::string::npos);
  CHECK(source.find("\"count_ref\"") != std::string::npos);
  CHECK(source.find("\"insert_ref\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/mapInsert\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapCount\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapCountRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapContains\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapContainsRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapTryAt\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapTryAtRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapAt\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapAtRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapAtUnsafe\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapAtUnsafeRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapInsert\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapInsertRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/mapInsertRef\"") != std::string::npos);
  CHECK(source.find("stripResolvedPathSpecializationSuffix(") != std::string::npos);
  CHECK(source.find("resolveCollectionsVectorMemberName(") != std::string::npos);
  CHECK(source.find("resolveCollectionsMapHelperMemberName(") != std::string::npos);
  CHECK(source.find("resolveStdlibSurfaceMemberName(const StdlibSurfaceMetadata &metadata,") !=
        std::string::npos);
  CHECK(source.find("stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId id,") !=
        std::string::npos);
  CHECK(source.find("stdlibSurfacePreferredSpellingForMember(StdlibSurfaceId id,") !=
        std::string::npos);
  CHECK(source.find("matchesResolvedRootedMemberPath(") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsMapConstructors") != std::string::npos);
  CHECK(source.find("\"collections.map_constructors\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/mapNew\"") != std::string::npos);
  CHECK(source.find("\"mapOct\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_map/mapOct\"") != std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsSoaVectorHelpers") != std::string::npos);
  CHECK(source.find("\"collections.soa_vector_helpers\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/soa_vector\"") != std::string::npos);
  CHECK(source.find("\"field_view\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/count\"") != std::string::npos);
  CHECK(source.find("\"/soa_vector/to_aos\"") != std::string::npos);
  CHECK(source.find("\"soaVectorGetRef\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa_vector/soaVectorPush\"") !=
        std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa_vector_conversions/soaVectorToAos\"") !=
        std::string::npos);

  CHECK(source.find("StdlibSurfaceId::CollectionsSoaVectorConstructors") !=
        std::string::npos);
  CHECK(source.find("\"collections.soa_vector_constructors\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/soa_vector/soa_vector\"") != std::string::npos);
  CHECK(source.find("\"/std/collections/experimental_soa_vector/soaVectorNew\"") !=
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

TEST_CASE("map insert surface registry resolves legacy compatibility spellings") {
  const primec::StdlibSurfaceMetadata *metadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsMapHelpers);
  REQUIRE(metadata != nullptr);

  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "insert") == "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/map/insert") == "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/mapInsert") ==
        "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *metadata, "/std/collections/experimental_map/mapInsert") == "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "insert_ref") == "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/map/insert_ref") ==
        "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*metadata, "/std/collections/mapInsertRef") ==
        "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *metadata, "/std/collections/experimental_map/mapInsertRef") == "insert_ref");

  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::CollectionsMapHelpers, "insert") ==
        "/std/collections/map/insert");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::CollectionsMapHelpers, "/std/collections/mapInsertRef") ==
        "/std/collections/map/insert_ref");
}

TEST_CASE("collection helper surface registry resolves preferred compatibility spellings") {
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsVectorHelpers,
            "/std/collections/vector/count",
            "/std/collections/experimental_vector/") ==
        "/std/collections/experimental_vector/vectorCount");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsVectorHelpers,
            "/vector/remove_swap",
            "/std/collections/experimental_vector/") ==
        "/std/collections/experimental_vector/vectorRemoveSwap");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsMapHelpers,
            "/std/collections/map/contains_ref",
            "/std/collections/experimental_map/") ==
        "/std/collections/experimental_map/mapContainsRef");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsMapHelpers,
            "/std/collections/mapAtUnsafe",
            "/std/collections/experimental_map/") ==
        "/std/collections/experimental_map/mapAtUnsafe");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsMapHelpers,
            "/not_map/count",
            "/std/collections/experimental_map/") == "");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/std/collections/soa_vector/count",
            "/std/collections/experimental_soa_vector/") ==
        "/std/collections/experimental_soa_vector/soaVectorCount");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/std/collections/count",
            "/std/collections/experimental_soa_vector/") ==
        "/std/collections/experimental_soa_vector/soaVectorCount");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/soa_vector/push",
            "/std/collections/experimental_soa_vector/") ==
        "/std/collections/experimental_soa_vector/soaVectorPush");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/std/collections/soa_vector/to_aos",
            "/std/collections/experimental_soa_vector_conversions/") ==
        "/std/collections/experimental_soa_vector_conversions/soaVectorToAos");
  CHECK(primec::stdlibSurfaceCanonicalHelperPath(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/std/collections/experimental_soa_vector/soaVectorGetRef") ==
        "/std/collections/soa_vector/get_ref");
  CHECK(primec::stdlibSurfacePreferredSpellingForMember(
            primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
            "/not_soa/count",
            "/std/collections/experimental_soa_vector/") == "");
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
  CHECK(source.find("#include \"primec/StdlibSurfaceRegistry.h\"") != std::string::npos);
  CHECK(source.find("resolveBuiltinMapInsertSurfaceMemberName(") != std::string::npos);
  CHECK(source.find("findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsMapHelpers)") !=
        std::string::npos);
  CHECK(source.find("resolveStdlibSurfaceMemberName(*metadata, name)") != std::string::npos);
  CHECK(source.find("stdlibSurfaceCanonicalHelperPath(\n"
                    "      StdlibSurfaceId::CollectionsMapHelpers,") != std::string::npos);
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
  CHECK(source.find("#include \"primec/StdlibSurfaceRegistry.h\"") != std::string::npos);
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

TEST_CASE("cmake splits primec library into subsystem targets") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
  }
  REQUIRE(std::filesystem::exists(cmakePath));

  const std::string cmake = readTextFile(cmakePath);
  CHECK(cmake.find("set(PRIMESTRUCT_SUPPORT_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_FRONTEND_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_IR_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_CODEGEN_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_RUNTIME_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_BACKEND_REGISTRY_SOURCES") != std::string::npos);
  CHECK(cmake.find("src/IrBackendProfiles.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateConvertConstructors.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateExperimentalGfxConstructors.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersCloneDebug.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersCompare.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersSerialization.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersState.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersValidate.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionMetadata.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateTransforms.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidationBenchmarkOrchestration.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidationPublicationOrchestration.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprBodyArguments.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprBlock.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprDispatchBootstrap.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprPreDispatchDirectCalls.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMethodCompatibilitySetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccess.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionDispatchSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccessSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprDirectCollectionFallbacks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprPostAccessPrechecks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprBuiltinContextSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionCountCapacity.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateCollectionAccessFallbacks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateFallbackBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionPredicates.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionLiterals.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprFieldResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprGpuBuffer.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateCallCompatibility.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateMapSoaBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateUnknownTargetFallbacks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLambda.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMethodResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMutationBorrows.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprNamedArgumentBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprNumeric.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprPointerLike.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprResolvedCallSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprResolvedCallArguments.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprReferenceEscapes.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprResultFile.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprStructConstructors.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprTry.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprVectorHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionCountCapacity.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionDirectCountCapacity.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionDispatchSetup.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferLateFallbackBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferPreDispatchCalls.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollections.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferDefinition.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferResolvedCalls.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferScalarBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorPassesEffects.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorPassesDiagnostics.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementBindings.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementVectorHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererAccessTargetResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererAccessLoadHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererIndexedAccessEmit.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererCallResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererNativeTailDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererGpuEffects.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererNativeEffects.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererVmEffects.cpp") != std::string::npos);
  CHECK(cmake.find("src/VmHeapHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("add_library(primec_support_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_frontend_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_ir_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_emitters_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_codegen_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("add_library(primec_runtime_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_registry_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("add_library(primec_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_frontend_lib PUBLIC primec_support_lib)") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_runtime_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_emitters_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_codegen_lib INTERFACE primec_backend_emitters_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_ir_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PRIVATE primec_backend_emitters_lib primec_runtime_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PUBLIC primec_backend_emitters_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PRIVATE primec_codegen_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PUBLIC primec_codegen_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_lib INTERFACE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_lib INTERFACE primec_backend_lib primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_backend_lib)") == std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_lib)") == std::string::npos);
  CHECK(cmake.find("target_link_libraries(primevm PRIVATE primec_ir_lib primec_runtime_lib)") != std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_misc_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_misc_tests PRIVATE primec_ir_lib)") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructMiscTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_misc_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_backend_ir_tests") != std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_backend_runtime_tests") != std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_compile_run_tests") != std::string::npos);
  CHECK(cmake.find("foreach(backendTestTarget IN ITEMS") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(${backendTestTarget} PRIVATE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_backend_ir_tests PRIVATE primec_backend_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_backend_runtime_tests PRIVATE primec_backend_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_compile_run_tests PRIVATE primec_backend_lib)") ==
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_backend_tests") == std::string::npos);
  CHECK(cmake.find("set(PrimeStructBackendIrTestSuites") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructBackendRuntimeTestSuites") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructBackendTestSuites") != std::string::npos);
  CHECK(cmake.find("primeStructSelectDoctestTarget(PrimeStructSuite_TARGET \"${suite}\")") !=
        std::string::npos);
  CHECK(cmake.find("primeStructSelectDoctestTarget(suiteTarget \"${suite}\")") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_semantics_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_semantics_tests PRIVATE primec_ir_lib)") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructSemanticsTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_text_filter_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_text_filter_tests PRIVATE primec_frontend_lib)") !=
        std::string::npos);
  CHECK(cmake.find("set(PrimeStructTextFilterTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_text_filter_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_parser_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_parser_tests PRIVATE primec_frontend_lib)") !=
        std::string::npos);
  CHECK(cmake.find("set(PrimeStructParserTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_parser_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("find_package(Python3 REQUIRED COMPONENTS Interpreter)") != std::string::npos);
  CHECK(cmake.find("NAME PrimeStruct_include_layers") != std::string::npos);
  CHECK(cmake.find("scripts/check_include_layers.py") != std::string::npos);
  CHECK(cmake.find("scripts/include_layer_allowlist.txt") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_misc_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_backend_ir_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_backend_runtime_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_compile_run_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_backend_suite_registration") == std::string::npos);
  CHECK(cmake.find("PrimeStruct_parser_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_text_filter_suite_registration") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_tests PRIVATE primec_lib)") == std::string::npos);
}

TEST_CASE("managed backend suite sharding keeps lowering and runtime suites on focused binaries") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path managedSuitesPath = cwd / "cmake" / "PrimeStructManagedUnitBackendSuites.cmake";
  if (!std::filesystem::exists(managedSuitesPath)) {
    managedSuitesPath = cwd.parent_path() / "cmake" / "PrimeStructManagedUnitBackendSuites.cmake";
  }
  REQUIRE(std::filesystem::exists(managedSuitesPath));

  const std::string managedSuites = readTextFile(managedSuitesPath);
  CHECK(managedSuites.find("TARGET PrimeStruct_backend_ir_tests") != std::string::npos);
  CHECK(managedSuites.find("TARGET PrimeStruct_backend_runtime_tests") != std::string::npos);
  CHECK(managedSuites.find("TARGET PrimeStruct_backend_tests") == std::string::npos);
  CHECK(managedSuites.find(
            "addPrimeStructManagedDoctestSuite(\"primestruct.ir.pipeline.validation\"\n"
            "  TARGET PrimeStruct_backend_ir_tests") != std::string::npos);
  CHECK(managedSuites.find(
            "addPrimeStructManagedDoctestSuite(\"primestruct.ir.pipeline.serialization\"\n"
            "  TARGET PrimeStruct_backend_ir_tests\n"
            "  LABEL \"parallel-safe\"\n"
            "  TIMEOUT 300\n"
            "  RANGE_FIRST 53\n"
            "  RANGE_LAST 64\n"
            "  CASES_PER_SHARD 2") != std::string::npos);
  CHECK(managedSuites.find(
            "addPrimeStructManagedDoctestSuite(\"primestruct.ir.pipeline.to_glsl\"\n"
            "  TARGET PrimeStruct_backend_runtime_tests") != std::string::npos);
  CHECK(managedSuites.find(
            "addPrimeStructManagedDoctestSuite(\"primestruct.vm.debug.session\"\n"
            "  TARGET PrimeStruct_backend_runtime_tests") != std::string::npos);
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
      cwd / "include" / "primec" / "SoaPathHelpers.h";
  std::filesystem::path parserTestApiPath = cwd / "include" / "primec" / "testing" / "ParserHelpers.h";
  std::filesystem::path semanticsControlFlowApiPath =
      cwd / "include" / "primec" / "testing" / "SemanticsControlFlowProbes.h";
  std::filesystem::path semanticsGraphTestApiPath = cwd / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
  std::filesystem::path semanticsTestApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path textFilterTestApiPath = cwd / "include" / "primec" / "testing" / "TextFilterHelpers.h";
  std::filesystem::path parserHelperTestPath = cwd / "tests" / "unit" / "test_parser_basic_parser_helpers.cpp";
  std::filesystem::path textFilterHelperTestPath = cwd / "tests" / "unit" / "test_text_filter_helpers.cpp";
  std::filesystem::path compileRunTestPath = cwd / "tests" / "unit" / "test_compile_run_vm_bounds.cpp";
  std::filesystem::path irPipelineTestPath = cwd / "tests" / "unit" / "test_ir_pipeline.cpp";
  std::filesystem::path validationHelpersTestPath =
      cwd / "tests" / "unit" / "test_ir_pipeline_validation_helpers.h";
  std::filesystem::path countAccessValidationTestPath =
      cwd / "tests" / "unit" /
      "test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = cwd.parent_path() / "scripts" / "check_include_layers.py";
    allowlistPath = cwd.parent_path() / "scripts" / "include_layer_allowlist.txt";
    emitterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "EmitterHelpers.h";
    irLowererTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererHelpers.h";
    irLowererCountAccessContractsApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererCountAccessContracts.h";
    irLowererStageContractsApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererStageContracts.h";
    soaPathHelpersApiPath = cwd.parent_path() / "include" / "primec" / "SoaPathHelpers.h";
    parserTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "ParserHelpers.h";
    semanticsControlFlowApiPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsControlFlowProbes.h";
    semanticsGraphTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
    semanticsTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    textFilterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "TextFilterHelpers.h";
    parserHelperTestPath = cwd.parent_path() / "tests" / "unit" / "test_parser_basic_parser_helpers.cpp";
    textFilterHelperTestPath = cwd.parent_path() / "tests" / "unit" / "test_text_filter_helpers.cpp";
    compileRunTestPath = cwd.parent_path() / "tests" / "unit" / "test_compile_run_vm_bounds.cpp";
    irPipelineTestPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline.cpp";
    validationHelpersTestPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_validation_helpers.h";
    countAccessValidationTestPath =
        cwd.parent_path() / "tests" / "unit" /
        "test_ir_pipeline_validation_ir_lowerer_count_access_helpers_build_bundled_entry_count_setup.cpp";
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
  CHECK(soaPathHelpers.find("isExperimentalSoaVectorSpecializedTypePath") !=
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
