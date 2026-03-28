TEST_CASE("allows comments around version attribute") {
  auto baseDir = importResolverPath("include_version_comment_base");
  auto includeRoot = importResolverPath("include_version_comment_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_COMMENT_VERSION\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"/lib.prime\" /* note */, version = \"1.2\" >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_COMMENT_VERSION") != std::string::npos);
}

TEST_CASE("allows comments containing > in import list") {
  auto baseDir = importResolverPath("include_comment_bracket_base");
  auto includeRoot = importResolverPath("include_comment_bracket_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_COMMENT_BRACKET\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"/lib.prime\" /* ignore > here */, version=\"1.2\" >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_COMMENT_BRACKET") != std::string::npos);
}

TEST_CASE("resolves versioned absolute import from import path") {
  auto baseDir = importResolverPath("include_version_abs_base");
  auto includeRoot = importResolverPath("include_version_abs_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_VERSION_ABS_120\n");
  writeFile(includeRoot / "1.2.9" / "lib.prime", "// INCLUDE_VERSION_ABS_129\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_ABS_129") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_ABS_120") == std::string::npos);
}

TEST_CASE("selects newest import version across roots") {
  auto baseDir = importResolverPath("include_version_multi_base");
  auto includeRootA = importResolverPath("include_version_multi_a");
  auto includeRootB = importResolverPath("include_version_multi_b");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRootA);
  std::filesystem::remove_all(includeRootB);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRootA);
  std::filesystem::create_directories(includeRootB);

  writeFile(includeRootA / "1.2.0" / "lib.prime", "// INCLUDE_VERSION_MULTI_120\n");
  writeFile(includeRootB / "1.2.9" / "lib.prime", "// INCLUDE_VERSION_MULTI_129\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRootA.string(), includeRootB.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_MULTI_129") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_MULTI_120") == std::string::npos);
}

TEST_CASE("rejects import version mismatch across paths") {
  auto baseDir = importResolverPath("include_version_mismatch_base");
  auto includeRootA = importResolverPath("include_version_mismatch_a");
  auto includeRootB = importResolverPath("include_version_mismatch_b");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRootA);
  std::filesystem::remove_all(includeRootB);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRootA);
  std::filesystem::create_directories(includeRootB);

  writeFile(includeRootA / "1.2.0" / "lib_a.prime", "// V120_A\n");
  writeFile(includeRootB / "1.2.9" / "lib_b.prime", "// V129_B\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib_a.prime\", \"/lib_b.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath,
                                      source,
                                      error,
                                      {includeRootA.string(), includeRootB.string()}));
  CHECK(error.find("import version mismatch") != std::string::npos);
}

TEST_CASE("resolves versioned relative import from import path") {
  auto baseDir = importResolverPath("include_version_rel_base");
  auto includeRoot = importResolverPath("include_version_rel_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "2.0.0" / "lib.prime", "// INCLUDE_VERSION_REL_200\n");
  writeFile(includeRoot / "2.1.0" / "lib.prime", "// INCLUDE_VERSION_REL_210\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"lib.prime\", version=\"2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_REL_210") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_REL_200") == std::string::npos);
}

TEST_CASE("resolves versioned import from archive root") {
  if (!hasZipTools()) {
    return;
  }
  auto baseDir = importResolverPath("include_zip_base");
  auto includeRoot = importResolverPath("include_zip_root");
  auto archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(archiveSource);

  writeFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime", "// ZIP_MARKER\n");
  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/std/io\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("ZIP_MARKER") != std::string::npos);
}

TEST_CASE("resolves versioned import from archives in stable order") {
  if (!hasZipTools()) {
    return;
  }
  auto baseDir = importResolverPath("include_zip_stable_base");
  auto includeRoot = importResolverPath("include_zip_stable_root");
  auto archiveSourceA = includeRoot / "archive_src_a";
  auto archiveSourceB = includeRoot / "archive_src_b";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(archiveSourceA);
  std::filesystem::create_directories(archiveSourceB);

  writeFile(archiveSourceA / "1.2.0" / "std" / "io" / "lib.prime", "// ZIP_STABLE_A\n");
  writeFile(archiveSourceB / "1.2.0" / "std" / "io" / "lib.prime", "// ZIP_STABLE_B\n");
  const std::filesystem::path archivePathZ = includeRoot / "zzz_std_io.zip";
  const std::filesystem::path archivePathA = includeRoot / "aaa_std_io.zip";
  CHECK(createZip(archivePathZ, archiveSourceB));
  CHECK(createZip(archivePathA, archiveSourceA));

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/std/io\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("ZIP_STABLE_A") != std::string::npos);
  CHECK(source.find("ZIP_STABLE_B") == std::string::npos);
}

TEST_CASE("resolves versioned import from zip root directly") {
  if (!hasZipTools()) {
    return;
  }
  auto baseDir = importResolverPath("include_zip_direct_base");
  auto archiveRoot = importResolverPath("include_zip_direct_root");
  auto archiveSource = archiveRoot / "archive_src";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(archiveRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(archiveSource);

  writeFile(archiveSource / "1.2.0" / "lib.prime", "// ZIP_DIRECT_MARKER\n");
  const std::filesystem::path archivePath = archiveRoot / "lib.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {archivePath.string()}));
  CHECK(error.empty());
  CHECK(source.find("ZIP_DIRECT_MARKER") != std::string::npos);
}

TEST_CASE("resolves versioned absolute import using base directory") {
  auto baseDir = importResolverPath("include_version_base_dir");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "1.2.0" / "lib.prime", "// BASE_DIR_MARKER\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("BASE_DIR_MARKER") != std::string::npos);
}

TEST_CASE("resolves exact import version") {
  auto baseDir = importResolverPath("include_versions_exact");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "1.2.0" / "lib.prime", "// V120\n");
  writeFile(baseDir / "1.2.1" / "lib.prime", "// V121\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2.0\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("V120") != std::string::npos);
  CHECK(source.find("V121") == std::string::npos);
}

TEST_CASE("selects newest matching import version") {
  auto baseDir = importResolverPath("include_versions_latest");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "1.2.0" / "lib.prime", "// V120\n");
  writeFile(baseDir / "1.2.5" / "lib.prime", "// V125\n");
  writeFile(baseDir / "1.3.0" / "lib.prime", "// V130\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("V125") != std::string::npos);
  CHECK(source.find("V130") == std::string::npos);
}

TEST_CASE("expands directory import contents") {
  auto baseDir = importResolverPath("include_dir");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "b.prime", "// DIR_B\n");
  writeFile(baseDir / "a.prime", "// DIR_A\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"" + baseDir.string() + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find("DIR_A");
  const auto second = source.find("DIR_B");
  CHECK(first != std::string::npos);
  CHECK(second != std::string::npos);
  CHECK(first < second);
}

TEST_CASE("skips private subdirectories in directory import") {
  auto baseDir = importResolverPath("include_dir_private");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "_private" / "secret.prime", "// SECRET\n");
  writeFile(baseDir / "public.prime", "// PUBLIC\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"" + baseDir.string() + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("PUBLIC") != std::string::npos);
  CHECK(source.find("SECRET") == std::string::npos);
}

TEST_CASE("rejects private import root directory") {
  auto baseDir = importResolverPath("include_private_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir / "_private");

  writeFile(baseDir / "_private" / "secret.prime", "// SECRET\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"" + (baseDir / "_private").string() + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_CASE("import resolver uses injected process runner for archive extraction errors") {
  auto baseDir = importResolverPath("include_injected_runner_fail");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  const std::filesystem::path archivePath = baseDir / "fake_archive.zip";
  writeFile(archivePath, "not-a-real-zip");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.0\">\n");

  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 1; });
  primec::ImportResolver resolver(&runner);
  std::string source;
  std::string error;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {archivePath.string()}));
  CHECK(error.find("failed to extract archive:") == 0);
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front().size() == 6);
  CHECK(runner.commands.front()[0] == "unzip");
  CHECK(runner.commands.front()[1] == "-q");
  CHECK(runner.commands.front()[2] == "-o");
  CHECK(runner.commands.front()[4] == "-d");
}

TEST_CASE("import resolver supports injected process runner success path") {
  if (!hasZipTools()) {
    return;
  }
  auto baseDir = importResolverPath("include_injected_runner_ok");
  auto archiveRoot = baseDir / "archive_root";
  auto archiveSource = archiveRoot / "archive_src";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(archiveSource);

  writeFile(archiveSource / "1.2.0" / "lib.prime", "// INJECTED_RUNNER_OK\n");
  const std::filesystem::path archivePath = archiveRoot / "lib.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");
  RecordingProcessRunner runner([](const std::vector<std::string> &args) {
    return primec::systemProcessRunner().run(args);
  });
  primec::ImportResolver resolver(&runner);

  std::string source;
  std::string error;
  CHECK(resolver.expandImports(srcPath, source, error, {archivePath.string()}));
  CHECK(error.empty());
  CHECK(source.find("INJECTED_RUNNER_OK") != std::string::npos);
  REQUIRE_FALSE(runner.commands.empty());
  CHECK(runner.commands.front().size() == 6);
  CHECK(runner.commands.front()[0] == "unzip");
  CHECK(runner.commands.front()[1] == "-q");
  CHECK(runner.commands.front()[2] == "-o");
  CHECK(runner.commands.front()[4] == "-d");
}

TEST_SUITE_END();
