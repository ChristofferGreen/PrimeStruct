TEST_CASE("compiles and runs archive import expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_archive_compile_run";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "import</std/io, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_archive_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs exact versioned archive import expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_archive_compile_exact";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  std::filesystem::create_directories(archiveSource / "1.2.3" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.3" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(libFile.good());
  }

  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "import</std/io, version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include_exact.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_archive_inc_exact_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs newest archive import expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_archive_compile_latest";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  std::filesystem::create_directories(archiveSource / "1.2.8" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.8" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(libFile.good());
  }

  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "import</std/io, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include_latest.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_archive_inc_latest_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("conformance: versioned import selects latest for wildcard import exposure") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_conformance_versioned_wildcard";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "lib");
    std::ofstream oldLib(includeRoot / "1.2.0" / "lib" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "namespace lib {\n"
              "  [public return<int>]\n"
              "  value(){ return(3i32) }\n"
              "}\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.7" / "lib");
    std::ofstream newLib(includeRoot / "1.2.7" / "lib" / "lib.prime");
    CHECK(newLib.good());
    newLib << "namespace lib {\n"
              "  [public return<int>]\n"
              "  value(){ return(8i32) }\n"
              "}\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import</lib, version=\"1.2\">\n"
      "import /lib\n"
      "[return<int>]\n"
      "main(){ return(value()) }\n";
  const std::string srcPath = writeTemp("compile_conformance_versioned_wildcard.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_conformance_versioned_wildcard_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("conformance: duplicate versioned imports are deduplicated before import aliasing") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_conformance_dedup";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "util");
    std::ofstream lib(includeRoot / "1.2.0" / "util" / "lib.prime");
    CHECK(lib.good());
    lib << "namespace util {\n"
           "  [public return<int>]\n"
           "  helper(){ return(9i32) }\n"
           "}\n";
    CHECK(lib.good());
  }

  const std::string source =
      "import</util, version=\"1.2.0\">\n"
      "import</util, version=\"1.2.0\">\n"
      "import /util\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_conformance_duplicate_versioned_include.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_conformance_duplicate_versioned_include_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("conformance: versioned import rejects underscore-private paths") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_conformance_private_underscore";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "_hidden");
    std::ofstream hiddenLib(includeRoot / "1.2.0" / "_hidden" / "lib.prime");
    CHECK(hiddenLib.good());
    hiddenLib << "[return<int>]\nhelper(){ return(1i32) }\n";
    CHECK(hiddenLib.good());
  }

  const std::string source =
      "import</_hidden, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_conformance_private_underscore.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_conformance_private_underscore_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --import-path " +
                                 includeRoot.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("import path refers to private folder") != std::string::npos);
}

TEST_CASE("conformance: wildcard import does not expose private members from imported source") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_conformance_wildcard_visibility";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "pack");
    std::ofstream lib(includeRoot / "1.2.0" / "pack" / "lib.prime");
    CHECK(lib.good());
    lib << "namespace pack {\n"
           "  [public return<int>]\n"
           "  visible(){ return(2i32) }\n"
           "  [private return<int>]\n"
           "  hidden(){ return(7i32) }\n"
           "}\n";
    CHECK(lib.good());
  }

  const std::string source =
      "import</pack, version=\"1.2.0\">\n"
      "import /pack\n"
      "[return<int>]\n"
      "main(){ return(hidden()) }\n";
  const std::string srcPath = writeTemp("compile_conformance_wildcard_visibility.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_conformance_wildcard_visibility_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --import-path " +
                                 includeRoot.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: hidden") != std::string::npos);
}

TEST_CASE("conformance: versioned import directory expansion order is deterministic") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_conformance_ordering";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "pkg");
    std::ofstream bFile(includeRoot / "1.2.0" / "pkg" / "b.prime");
    CHECK(bFile.good());
    bFile << "// ORDER_CONFORMANCE_B\n[return<int>]\n/b(){ return(2i32) }\n";
    CHECK(bFile.good());
    std::ofstream aFile(includeRoot / "1.2.0" / "pkg" / "a.prime");
    CHECK(aFile.good());
    aFile << "// ORDER_CONFORMANCE_A\n[return<int>]\n/a(){ return(1i32) }\n";
    CHECK(aFile.good());
  }

  const std::string source =
      "import</pkg, version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_conformance_ordering.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_conformance_ordering_pre_ast.txt").string();

  const std::string dumpCmd = "./primec --dump-stage=pre_ast " + srcPath + " --entry /main --import-path " +
                              includeRoot.string() + " > " + outPath;
  CHECK(runCommand(dumpCmd) == 0);
  const std::string dumped = readFile(outPath);
  const auto aPos = dumped.find("ORDER_CONFORMANCE_A");
  const auto bPos = dumped.find("ORDER_CONFORMANCE_B");
  CHECK(aPos != std::string::npos);
  CHECK(bPos != std::string::npos);
  CHECK(aPos < bPos);
}
