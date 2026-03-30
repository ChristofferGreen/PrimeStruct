#pragma once

inline void expectMapConformanceProgramRuns(const std::string &source,
                                            const std::string &nameStem,
                                            const std::string &emitMode,
                                            int expectedExitCode) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == expectedExitCode);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectMapConformanceProgramRunsWithOutput(const std::string &source,
                                                      const std::string &nameStem,
                                                      const std::string &emitMode,
                                                      int expectedExitCode,
                                                      const std::string &expectedOutput) {
  CAPTURE(nameStem);
  CAPTURE(emitMode);
  CAPTURE(expectedExitCode);
  CAPTURE(expectedOutput);
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == expectedOutput);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedOutput);
}

inline void expectMapVmProgramRunsWithOutput(const std::string &source,
                                             const std::string &nameStem,
                                             int expectedExitCode,
                                             const std::string &expectedOutput) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_vm_out.txt")).string();
  const std::string runCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedOutput);
}

inline void expectMapConformanceCompileReject(const std::string &source,
                                              const std::string &nameStem,
                                              const std::string &emitMode,
                                              const std::string &expectedError) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expectedError) != std::string::npos);
    return;
  }

  const std::string artifactPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_artifact")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(artifactPath) + " --entry /main > " + quoteShellArg(outPath) +
                                 " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find(expectedError) != std::string::npos);
}

inline void expectMapConformanceFailure(const std::string &source,
                                        const std::string &nameStem,
                                        const std::string &emitMode,
                                        int expectedExitCode,
                                        const std::string &expectedError,
                                        bool duringCompile) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == expectedExitCode);
    if (!expectedError.empty()) {
      CHECK(readFile(outPath).find(expectedError) != std::string::npos);
    }
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  if (duringCompile) {
    CHECK(runCommand(compileCmd) == expectedExitCode);
    if (!expectedError.empty()) {
      CHECK(readFile(outPath).find(expectedError) != std::string::npos);
    }
    return;
  }

  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(runCmd) == expectedExitCode);
  if (!expectedError.empty()) {
    CHECK(readFile(outPath).find(expectedError) != std::string::npos);
  }
}

inline void expectVmStdlibMapHelperSurfaceConformance() {
  expectMapVmProgramRunsWithOutput(makeMapHelperSurfaceConformanceSource("/std/collections/*"),
                                   "map_helper_surface_stdlib",
                                   16,
                                   "2\n4\n7\n1\n2\n");
}

inline void expectVmStdlibMapExtendedConstructorConformance() {
  expectMapVmProgramRunsWithOutput(makeMapExtendedConstructorConformanceSource("/std/collections/*"),
                                   "map_extended_ctor_stdlib",
                                   77,
                                   "3\n10\n30\n1\n2\n8\n3\n8\n4\n8\n");
}

inline void expectMapOverwriteConformance(const std::string &emitMode,
                                          const std::string &importPath) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  expectMapConformanceProgramRuns(
      makeMapOverwriteConformanceSource(importPath),
      "map_overwrite_" + slug + "_" + emitMode,
      emitMode,
      19);
}

inline void expectMapTryAtConformance(const std::string &emitMode,
                                      const std::string &importPath,
                                      bool boolValues) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  const std::string valueSlug = boolValues ? "bool" : "i32";
  const int expectedExitCode = boolValues ? 22 : 28;
  const std::string source = makeMapTryAtConformanceImportSource(importPath, boolValues);
  const std::string srcPath = writeTemp("map_try_at_" + slug + "_" + valueSlug + "_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / ("primec_map_try_at_" + slug + "_" + valueSlug + "_" + emitMode +
                                                 "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_map_try_at_" + slug + "_" + valueSlug + "_" + emitMode +
                                                 "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectExperimentalMapMethodConformance(const std::string &emitMode) {
  const int expectedExitCode = 20;
  const std::string expectedError = "container missing key";
  expectMapConformanceFailure(makeExperimentalMapMethodConformanceSource(),
                              "experimental_map_methods",
                              emitMode,
                              expectedExitCode,
                              expectedError,
                              false);
}

inline void expectExperimentalMapReferenceHelperConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapReferenceHelperConformanceSource();
  const std::string srcPath = writeTemp("experimental_map_reference_helpers_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_experimental_map_reference_helpers_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 20);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") /
       ("primec_experimental_map_reference_helpers_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 20);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectExperimentalMapReferenceMethodConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeExperimentalMapReferenceMethodConformanceSource(),
                                            "experimental_map_reference_methods",
                                            emitMode,
                                            33,
                                            "container missing key\n");
}

inline void expectExperimentalMapVariadicConstructorConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(makeExperimentalMapVariadicConstructorConformanceSource(),
                                  "experimental_map_variadic_ctor_" + emitMode,
                                  emitMode,
                                  17);
}

inline void expectExperimentalMapVariadicConstructorMismatchReject(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorMismatchSource(),
                                    "experimental_map_variadic_ctor_mismatch",
                                    emitMode,
                                    "argument type mismatch");
}

inline void expectExperimentalMapInsertConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeExperimentalMapInsertConformanceSource(),
                                            "experimental_map_insert",
                                            emitMode,
                                            36,
                                            "3\n9\n13\n11\n");
}

inline void expectExperimentalMapOwnershipConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeExperimentalMapOwnershipConformanceSource(),
                                            "experimental_map_ownership",
                                            emitMode,
                                            13,
                                            "");
}

inline void expectCanonicalMapNamespaceExperimentalInsertConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeCanonicalMapNamespaceExperimentalInsertConformanceSource(),
                                            "map_namespace_canonical_experimental_insert",
                                            emitMode,
                                            18,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertPendingDiagnostic(const std::string &emitMode) {
  expectMapConformanceFailure(makeBuiltinCanonicalMapInsertTemporaryFailureSource(),
                              "map_builtin_canonical_insert_pending_" + emitMode,
                              emitMode,
                              3,
                              "builtin canonical map insert pending",
                              false);
}

inline void expectBuiltinCanonicalMapInsertFirstGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertFirstGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_first_growth_" + emitMode,
                                            emitMode,
                                            8,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertPairGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertPairGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_pair_growth_" + emitMode,
                                            emitMode,
                                            18,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertTripleGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertTripleGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_triple_growth_" + emitMode,
                                            emitMode,
                                            30,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertQuadGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertQuadGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_quad_growth_" + emitMode,
                                            emitMode,
                                            44,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertQuintGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertQuintGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_quint_growth_" + emitMode,
                                            emitMode,
                                            60,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertSextGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertSextGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_sext_growth_" + emitMode,
                                            emitMode,
                                            78,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertSeptGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertSeptGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_sept_growth_" + emitMode,
                                            emitMode,
                                            98,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertOctGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertOctGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_oct_growth_" + emitMode,
                                            emitMode,
                                            120,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertNinthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertNinthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_ninth_growth_" + emitMode,
                                            emitMode,
                                            144,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertTenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertTenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_tenth_growth_" + emitMode,
                                            emitMode,
                                            170,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertOverwriteConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertOverwriteConformanceSource(),
                                            "map_builtin_canonical_insert_overwrite_" + emitMode,
                                            emitMode,
                                            22,
                                            "");
}

inline void expectExperimentalMapIndexConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapIndexConformanceSource(),
                                    "experimental_map_index",
                                    emitMode,
                                    "unknown call target: /std/collections/map/at");
}

inline void expectCanonicalMapNamespaceVmConformance() {
  expectMapVmProgramRunsWithOutput(makeCanonicalMapNamespaceConformanceSource(),
                                   "map_namespace_canonical_vm",
                                   20,
                                   "4\ncontainer missing key\n2\n4\n7\n1\n2\n");
}

inline void expectCanonicalMapNamespaceOwnershipReject(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeCanonicalMapNamespaceOwnershipRejectSource(),
      "map_namespace_canonical_ownership_reject",
      emitMode,
      "map literal requires relocation-trivial map value type until container move/reallocation semantics are "
      "implemented: Owned");
}

inline void expectCanonicalMapNamespaceExperimentalReferenceConformance(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceExperimentalReferenceConformanceSource();
  const std::string srcPath =
      writeTemp("map_namespace_canonical_experimental_reference_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_map_namespace_canonical_experimental_reference_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}
